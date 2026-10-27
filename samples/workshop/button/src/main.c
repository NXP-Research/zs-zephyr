/*
 * Workshop: Button + LED (starter)
 *
 * Goal: toggle the red LED (led0) each time button SW2 (sw0) is pressed,
 * using a GPIO interrupt instead of polling.
 *
 * Start from what you learned in the blinky sample.
 *
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

/*
 * Step 1: Get a struct gpio_dt_spec for the LED and for the button, the same
 *         way blinky does it for the LED. The button alias is "sw0":
 *
 *           GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios)
 *           GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios)
 *
 *         You also need a callback structure:
 *
 *           static struct gpio_callback button_cb_data;
 *
 *         The GPIO driver keeps a pointer to it, so it must stay in memory.
 */

/*
 * Step 2: Write the callback function. The GPIO driver calls it when the
 *         button interrupt fires. It must have this signature:
 *
 *           static void button_pressed(const struct device *dev,
 *                                      struct gpio_callback *cb, uint32_t pins)
 *
 *         Toggle the LED (gpio_pin_toggle_dt()) and print a press counter.
 *
 *         This function runs in interrupt (ISR) context: keep it short and
 *         never block (no k_msleep() here!).
 */

int main(void)
{
	/*
	 * Step 3: Check that both GPIO devices are ready (gpio_is_ready_dt()).
	 *
	 * Step 4: Configure the LED as an output (GPIO_OUTPUT_INACTIVE) and the
	 *         button as an input (GPIO_INPUT) with gpio_pin_configure_dt().
	 *
	 * Step 5: Make the button pin raise an interrupt when it is pressed:
	 *         gpio_pin_interrupt_configure_dt() with GPIO_INT_EDGE_TO_ACTIVE.
	 *
	 * Step 6: Bind your callback to the button pin and register it:
	 *         gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	 *         gpio_add_callback_dt(&button, &button_cb_data);
	 *
	 * Most of these functions return a negative error code on failure.
	 * Check it and print an error message.
	 */

	printk("Press SW2 to toggle the LED\n");

	/*
	 * Nothing else to do in main(): the work happens in the interrupt.
	 * Returning from main() ends only the main thread, not the system.
	 */
	return 0;
}
