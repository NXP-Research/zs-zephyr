/*
 * Workshop: Threads (solution)
 *
 * Button -> ISR -> message queue -> worker thread -> LED
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

/* Edges closer together than this are treated as contact bounce. */
#define DEBOUNCE_MS 50

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec heartbeat_led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

static struct gpio_callback button_cb_data;

/* One message in the queue: what happened and when. */
struct button_event {
	uint32_t timestamp_ms;
	/* true: the button was pressed, false: it was released */
	bool pressed;
};

/* A queue of up to 8 button_event messages, 4-byte aligned. */
K_MSGQ_DEFINE(button_msgq, sizeof(struct button_event), 8, 4);

/*
 * Incremented by the ISR, read by the worker thread. atomic_t and the
 * atomic_*() functions make each access a single, uninterruptible operation,
 * so the two contexts can safely share it.
 */
static atomic_t dropped_events;

/*
 * ISR: do the minimum. Record the event and hand it over to a thread.
 * It runs on every edge: press and release (GPIO_INT_EDGE_BOTH in main()).
 */
static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	struct button_event evt = {
		.timestamp_ms = k_uptime_get_32(),
		/* gpio_pin_get_dt() is ISR-safe: 1 = active (pressed) */
		.pressed = (gpio_pin_get_dt(&button) == 1),
	};

	/* In an ISR we must never wait: K_NO_WAIT fails if the queue is full. */
	if (k_msgq_put(&button_msgq, &evt, K_NO_WAIT) != 0) {
		atomic_inc(&dropped_events);
	}
}

/*
 * Worker thread: blocks on the queue, so it uses no CPU until an event arrives.
 * Here we are in thread context and may sleep, print, take mutexes, etc.
 */
static void worker_thread(void *p1, void *p2, void *p3)
{
	struct button_event evt;
	uint32_t press_count = 0;
	uint32_t last_edge_ms = 0;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		/* Blocking call: the thread sleeps until a message arrives. */
		k_msgq_get(&button_msgq, &evt, K_FOREVER);

		/*
		 * Debounce: accept a press only if the button was quiet (no
		 * edge at all) for DEBOUNCE_MS before it. Every edge restarts
		 * the quiet time, also the ones we ignore.
		 */
		bool quiet = (evt.timestamp_ms - last_edge_ms) >= DEBOUNCE_MS;

		last_edge_ms = evt.timestamp_ms;

		if (!evt.pressed || !quiet) {
			continue;
		}

		press_count++;

		gpio_pin_toggle_dt(&led);
		printk("[%6u ms] press %u (dropped: %ld)\n", evt.timestamp_ms, press_count,
		       atomic_get(&dropped_events));
	}
}

/*
 * Heartbeat thread: shows that other work keeps running independently.
 */
static void heartbeat_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	if (!gpio_is_ready_dt(&heartbeat_led)) {
		printk("Error: heartbeat LED not ready\n");
		return;
	}

	if (gpio_pin_configure_dt(&heartbeat_led, GPIO_OUTPUT_INACTIVE) < 0) {
		printk("Error: failed to configure heartbeat LED\n");
		return;
	}

	while (1) {
		gpio_pin_toggle_dt(&heartbeat_led);
		k_msleep(1000);
	}
}

/* Statically defined threads: they start automatically after boot. */
K_THREAD_DEFINE(worker_tid, STACK_SIZE, worker_thread, NULL, NULL, NULL,
		WORKER_PRIORITY, 0, 0);
K_THREAD_DEFINE(heartbeat_tid, STACK_SIZE, heartbeat_thread, NULL, NULL, NULL,
		HEARTBEAT_PRIORITY, 0, 0);

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

	/* Interrupt on both edges, press and release, to see the bounce. */
	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
	if (ret < 0) {
		printk("Error %d: failed to configure button interrupt\n", ret);
		return 0;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	ret = gpio_add_callback_dt(&button, &button_cb_data);
	if (ret < 0) {
		printk("Error %d: failed to add button callback\n", ret);
		return 0;
	}

	printk("Press SW2: the worker thread toggles the red LED\n");

	return 0;
}
