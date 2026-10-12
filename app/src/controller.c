#include "shared_state.h"
#include "fan_api.h"

void apply_to_hardware(const SystemContext *copy) {
    static int last_power = -1;     // 타이머 스레드에서만 호출됨

    if (copy->power != last_power) {    // 전원이 바뀐 순간에만 백라이트를 바꿈
        lcd_set_backlight(copy->power ? 1 : 0);
        last_power = copy->power;
    }

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