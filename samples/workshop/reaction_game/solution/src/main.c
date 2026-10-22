/*
 * Workshop: Reaction Game (solution)
 *
 * Press SW2 to start a round. After a random delay the red LED turns on;
 * press SW2 as fast as you can. The reaction time is printed on the console.
 *
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define MIN_DELAY_MS     1000
#define MAX_DELAY_MS     4000
#define REACT_TIMEOUT_MS 2000
#define DEBOUNCE_MS      50

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

static struct gpio_callback button_cb_data;

struct button_event {
	/* Hardware cycle counter at the moment of the press. */
	uint32_t cycles;
};

K_MSGQ_DEFINE(button_msgq, sizeof(struct button_event), 4, 4);

enum game_state {
	STATE_IDLE,    /* waiting for the player to start a round */
	STATE_WAITING, /* LED off, random delay running */
	STATE_REACT,   /* LED on, measuring the reaction time */
};

/*
 * Small xorshift pseudo-random generator. The board has no entropy driver
 * enabled, so we seed it with the time of the player's button presses,
 * which is unpredictable enough for a game.
 */
static uint32_t rng_state = 1;

static inline void rng_mix(uint32_t seed)
{
	rng_state ^= seed;
	if (rng_state == 0) {
		rng_state = 1;
	}
}

static inline uint32_t rng_next(void)
{
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 17;
	rng_state ^= rng_state << 5;
	return rng_state;
}

/*
 * Runs on every edge, press and release (GPIO_INT_EDGE_BOTH in setup_gpio()).
 */
static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	static uint32_t last_edge_cycles;
	uint32_t now = k_cycle_get_32();
	bool quiet;

	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	/*
	 * Debounce right here: a subtraction and a pin read, cheap enough for
	 * an ISR. Accept a press only if the button was quiet (no edge at all)
	 * for DEBOUNCE_MS before it, and the pin now reads "pressed". Every
	 * edge restarts the quiet time, also the ones we ignore. This drops
	 * both the press bounce and the release bounce.
	 */
	quiet = k_cyc_to_ms_floor32(now - last_edge_cycles) >= DEBOUNCE_MS;
	last_edge_cycles = now;

	if (!quiet || gpio_pin_get_dt(&button) != 1) {
		return;
	}

	struct button_event evt = {
		.cycles = now,
	};

	(void)k_msgq_put(&button_msgq, &evt, K_NO_WAIT);
}

static int setup_gpio(void)
{
	int ret;

	if (!gpio_is_ready_dt(&led) || !gpio_is_ready_dt(&button)) {
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return ret;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret < 0) {
		return ret;
	}

	/* Interrupt on both edges, so the ISR also sees the release bounce. */
	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
	if (ret < 0) {
		return ret;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	return gpio_add_callback_dt(&button, &button_cb_data);
}

int main(void)
{
	enum game_state state = STATE_IDLE;
	struct button_event evt;
	uint32_t led_on_cycles = 0;
	uint32_t round_no = 1;
	uint32_t best_us = UINT32_MAX;
	uint64_t total_us = 0;
	int ret;

	ret = setup_gpio();
	if (ret < 0) {
		printk("Error %d: GPIO setup failed\n", ret);
		return 0;
	}

	printk("\n=== Reaction Game ===\n");

	while (1) {
		switch (state) {
		case STATE_IDLE:
			printk("\nPress SW2 to start round %u\n", round_no);
			/* Forget presses made while the last round was printing. */
			k_msgq_purge(&button_msgq);
			k_msgq_get(&button_msgq, &evt, K_FOREVER);
			rng_mix(evt.cycles);
			state = STATE_WAITING;
			break;

		case STATE_WAITING: {
			uint32_t delay_ms = MIN_DELAY_MS + rng_next() % (MAX_DELAY_MS - MIN_DELAY_MS);

			printk("Get ready... press when the LED turns on!\n");
			gpio_pin_set_dt(&led, 0);

			/* Only presses from now on may count as "too early". */
			k_msgq_purge(&button_msgq);

			/*
			 * The random delay is the timeout of k_msgq_get(): if a
			 * message arrives before it expires, the player pressed
			 * too early.
			 */
			if (k_msgq_get(&button_msgq, &evt, K_MSEC(delay_ms)) == 0) {
				printk("  Too early! Wait for the LED.\n");
				state = STATE_IDLE;
				break;
			}

			gpio_pin_set_dt(&led, 1);
			led_on_cycles = k_cycle_get_32();
			state = STATE_REACT;
			break;
		}

		case STATE_REACT:
			ret = k_msgq_get(&button_msgq, &evt, K_MSEC(REACT_TIMEOUT_MS));
			gpio_pin_set_dt(&led, 0);

			if (ret == 0) {
				/* Unsigned subtraction handles counter wrap-around. */
				uint32_t reaction_us =
					k_cyc_to_us_floor32(evt.cycles - led_on_cycles);

				printk("  Reaction time: %u.%03u ms\n", reaction_us / 1000,
				       reaction_us % 1000);

				total_us += reaction_us;
				if (reaction_us < best_us) {
					best_us = reaction_us;
					printk("  New best!\n");
				}
				printk("  Rounds: %u, best: %u ms, average: %u ms\n", round_no,
				       best_us / 1000, (uint32_t)(total_us / round_no / 1000));

				round_no++;
				rng_mix(evt.cycles);
			} else {
				printk("  Too slow! (more than %d ms)\n", REACT_TIMEOUT_MS);
			}
			state = STATE_IDLE;
			break;
		}
	}

	return 0;
}
