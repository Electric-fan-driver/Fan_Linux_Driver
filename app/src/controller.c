#include "shared_state.h"


void apply_to_hardware(const SystemContext *copy) {
    if (!copy->power) {
        motor_set_level(0);
        led_set_level(0);
        servo_set_swing(0);
        lcd_clear(); 
        return;
    }

    motor_set_level(copy->fan_level);
    led_set_level(copy->fan_level);
    servo_set_swing(copy->swing);

    lcd_show_mode(copy->mode);                     
    lcd_show_temp(copy->temp, 1);           
    lcd_show_rot(copy->swing);
    lcd_show_timer(copy->timer_sec, copy->display_visible);
}