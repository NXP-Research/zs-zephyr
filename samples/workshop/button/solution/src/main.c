/*
 * Workshop: Button + LED (solution)
 *
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

/* Holds the callback registration with the GPIO driver. Must stay in memory. */
static struct gpio_callback button_cb_data;

/*
 * Called by the GPIO driver when the button interrupt fires.
 * This runs in interrupt (ISR) context: keep it short and never block.
 *
 * 'pins' is a bit mask of the pins that triggered the interrupt.
 */
static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	/* 'static' keeps the value between calls. It starts at 0. */
	static uint32_t press_count;

	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	gpio_pin_toggle_dt(&led);
	press_count++;

	/* printk() is fine for learning, but slow inside an ISR. */
	printk("Button pressed %u times\n", press_count);
}

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

	/* Bind our function to the button pin and register it with the driver. */
	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	ret = gpio_add_callback_dt(&button, &button_cb_data);
	if (ret < 0) {
		printk("Error %d: failed to add button callback\n", ret);
		return 0;
	}

	printk("Press SW2 to toggle the LED\n");

	/*
	 * Nothing left to do in main(): the work happens in the interrupt.
	 * Returning from main() ends only the main thread, not the system.
	 */
	return 0;
}
