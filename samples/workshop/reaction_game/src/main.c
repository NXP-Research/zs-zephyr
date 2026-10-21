/*
 * Workshop: Reaction Game (starter)
 *
 * Press SW2 to start a round. After a random delay the red LED turns on;
 * press SW2 as fast as you can. The reaction time is printed on the console.
 *
 * The LED, the button, the button interrupt and the message queue are
 * already set up for you. You write the ISR body and the game.
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

/* Red LED (led0) and button SW2 (sw0), from the board Devicetree. */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

/* Holds the callback registration with the GPIO driver. Must stay in memory. */
static struct gpio_callback button_cb_data;

/* One message in the queue: one button press. */
struct button_event {
	/* Hardware cycle counter at the moment of the press. */
	uint32_t cycles;
};

/* A queue of up to 4 button_event messages, 4-byte aligned. */
K_MSGQ_DEFINE(button_msgq, sizeof(struct button_event), 4, 4);

enum game_state {
	STATE_IDLE,    /* waiting for the player to start a round */
	STATE_WAITING, /* LED off, random delay running */
	STATE_REACT,   /* LED on, measuring the reaction time */
};

/*
 * Small xorshift pseudo-random generator. The board has no entropy driver
 * enabled, so seed it with rng_mix() using the time of the player's button
 * presses, which is unpredictable enough for a game.
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
 * Called by the GPIO driver when the button interrupt fires.
 * This runs in interrupt (ISR) context: keep it short and never block.
 */
static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	/*
	 * Step 1: Store the hardware cycle counter (k_cycle_get_32()) in a
	 *         struct button_event and send it with
	 *         k_msgq_put(&button_msgq, &evt, K_NO_WAIT).
	 *
	 *         Debounce first: remember the last accepted press in a
	 *         'static' variable and drop presses closer than DEBOUNCE_MS
	 *         (k_cyc_to_ms_floor32() converts cycles to ms).
	 */
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

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret < 0) {
		return ret;
	}

	/* Bind button_pressed() to the button pin and register it with the driver. */
	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	return gpio_add_callback_dt(&button, &button_cb_data);
}

int main(void)
{
	int ret;

	ret = setup_gpio();
	if (ret < 0) {
		printk("Error %d: GPIO setup failed\n", ret);
		return 0;
	}

	printk("\n=== Reaction Game ===\n");

	/*
	 * Step 2: Run the game as a state machine in a loop:
	 *
	 *   enum game_state state = STATE_IDLE;
	 *
	 *   while (1) {
	 *           switch (state) {
	 *           case STATE_IDLE:    ...
	 *           case STATE_WAITING: ...
	 *           case STATE_REACT:   ...
	 *           }
	 *   }
	 *
	 *   STATE_IDLE:    wait for a press (K_FOREVER), seed the RNG with
	 *                  rng_mix(), go to STATE_WAITING.
	 *   STATE_WAITING: pick a random delay between MIN_DELAY_MS and
	 *                  MAX_DELAY_MS with rng_next(). Wait for a press with
	 *                  that delay as timeout:
	 *                    k_msgq_get(&button_msgq, &evt, K_MSEC(delay_ms))
	 *                  0       -> pressed too early, back to STATE_IDLE.
	 *                  -EAGAIN -> turn the LED on, remember the cycle
	 *                             counter, go to STATE_REACT.
	 *   STATE_REACT:   wait for a press with REACT_TIMEOUT_MS as timeout.
	 *                  On a press: LED off, print the reaction time
	 *                  (k_cyc_to_us_floor32() of the cycle difference).
	 *                  On timeout: LED off, print "Too slow!".
	 *                  Back to STATE_IDLE.
	 *
	 * Bonus: keep statistics (rounds, best and average reaction time).
	 */

	return 0;
}
