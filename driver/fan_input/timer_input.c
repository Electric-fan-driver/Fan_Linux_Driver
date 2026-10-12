/* SPDX-License-Identifier: GPL-2.0 */
/* Second, independent instance of the proven count-only input driver.
 * User wiring requested here: header 12=KEY, 35=S2, 38=S1.
 */
#define FAN_DEVICE_NAME "timerdev"
#define FAN_DEVICE_CLASS "fan_timer_input"
#define FAN_DEVICE_MAJOR 121
#define FAN_GPIO_KEY 398 /* header 12: PH.07 */
#define FAN_GPIO_S2 401  /* header 35: PI.02 */
#define FAN_GPIO_S1 400  /* header 38: PI.01 */
#define FAN_KEY_HEADER_PIN 12
#define FAN_S2_HEADER_PIN 35
#define FAN_S1_HEADER_PIN 38
#define FAN_MODULE_DESCRIPTION "Timer encoder count-only IRQ driver (12/35/38)"
#define FAN_PARAM_GPIO_KEY timer_gpio_key
#define FAN_PARAM_GPIO_S2 timer_gpio_s2
#define FAN_PARAM_GPIO_S1 timer_gpio_s1
#define FAN_PARAM_ENABLE_GPIO timer_enable_gpio
#define FAN_PARAM_IRQ_MASK timer_irq_mask
#define FAN_PARAM_INVERT_DIRECTION timer_invert_direction
#define FAN_PARAM_KEY_DEBOUNCE_MS timer_key_debounce_ms
#define FAN_PARAM_DEBUG_WIND_LEVEL timer_debug_wind_level
#define FAN_PARAM_STEPS_PER_DETENT timer_steps_per_detent
#define FAN_DEBUG_WIND_LEVEL_DEFAULT false
#define FAN_EMBEDDED
#define FAN_ENCODER_ID ENC_TIMER
#define FAN_BUTTON_ID BTN_ENC_TIMER
#include "fan_input.c"

int __init timer_input_init_child(void)
{
    return fan_input_init();
}

void timer_input_exit_child(void)
{
    fan_input_exit();
}
