#include "shared_state.h"


void apply_to_hardware(const SystemContext *copy) {
    if (!copy->power_on) {
        motor_set_level(0);
        led_set_level(0);
        servo_swing(0);
        return;
    }

    motor_set_level(copy->fan_level);
    led_set_level(copy->fan_level);
    servo_set_swing(copy->swing_on);
    lcd_set_backlight(copy->lcd_set_backlight);
}