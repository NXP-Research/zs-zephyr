/*
 * Workshop: Threads (starter)
 *
 * Goal: move the application logic out of the interrupt and into a thread:
 *
 *   Button -> ISR -> message queue -> worker thread -> LED
 *
 * The LED, the button and the button interrupt are already set up for you
 * in main(). You write the message queue, the ISR body and the threads.
 *
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

/* Lower number = higher priority. Both are preemptible (>= 0). */
#define WORKER_PRIORITY    5
#define HEARTBEAT_PRIORITY 7
#define STACK_SIZE         1024

/* Presses closer together than this are treated as contact bounce. */
#define DEBOUNCE_MS 50

/* Red LED (led0) and button SW2 (sw0), from the board Devicetree. */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

/* Holds the callback registration with the GPIO driver. Must stay in memory. */
static struct gpio_callback button_cb_data;

/*
 * Step 1: Define a message type and a message queue for it:
 *
 *           struct button_event {
 *                   uint32_t timestamp_ms;
 *           };
 *
 *           K_MSGQ_DEFINE(button_msgq, sizeof(struct button_event), 8, 4);
 *
 *         (name, size of one message, max number of messages, alignment)
 */

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
	 * Step 2: Only record the event: fill a struct button_event with
	 *         k_uptime_get_32() and send it with
	 *         k_msgq_put(&button_msgq, &evt, K_NO_WAIT).
	 *
	 *         Why K_NO_WAIT? What happens if the queue is full?
	 */
}

/*
 * Step 3: Write a worker thread. It waits for events with
 *         k_msgq_get(&button_msgq, &evt, K_FOREVER), then toggles the red
 *         LED (led) and prints the timestamp. A thread entry function has
 *         this signature:
 *
 *           static void worker_thread(void *p1, void *p2, void *p3)
 *
 *         Bonus: ignore events that arrive less than DEBOUNCE_MS after the
 *         previous accepted one (contact bounce).
 */

/*
 * Step 4: Write a heartbeat thread that toggles the green LED (led1) once
 *         per second, to show that other work keeps running independently.
 *         It needs its own gpio_dt_spec:
 *
 *           GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios)
 */

/*
 * Step 5: Define both threads. They start automatically after boot:
 *
 *   K_THREAD_DEFINE(name, stack size, entry function, p1, p2, p3,
 *                   priority, options, start delay);
 */

int main(void)
{
	int ret;

	if (!gpio_is_ready_dt(&led) || !gpio_is_ready_dt(&button)) {
		printk("Error: GPIO device not ready\n");
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		printk("Error %d: failed to configure LED\n", ret);
		return 0;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret < 0) {
		printk("Error %d: failed to configure button\n", ret);
		return 0;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret < 0) {
		printk("Error %d: failed to configure button interrupt\n", ret);
		return 0;
	}

	/* Bind button_pressed() to the button pin and register it with the driver. */
	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	ret = gpio_add_callback_dt(&button, &button_cb_data);
	if (ret < 0) {
		printk("Error %d: failed to add button callback\n", ret);
		return 0;
	}

	printk("Press SW2: the worker thread toggles the red LED\n");

	/* Nothing left to do in main(): the threads do the rest. */
	return 0;
}
