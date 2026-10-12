#include <linux/delay.h>
#include "fan_lcd.h"

/* PCF8574: P0=RS, P1=R/W, P2=E, P3=백라이트, P4..P7=DB4..DB7 */
#define LCD_RS              BIT(0)
#define LCD_ENABLE          BIT(2)
#define LCD_BACKLIGHT_MASK  BIT(3)
#define LCD_DATA_MASK       0xf0

#define LCD_CLEAR_CMD       0x01
#define LCD_HOME_CMD        0x02
#define LCD_ROW1_ADDR       0x80
#define LCD_ROW2_ADDR       0xc0


/* 아래 전송·초기화 함수는 호출자가 lock을 잡은 상태에서 사용한다. */
static int fan_lcd_output(struct fan_lcd_data *data, u8 pins)
{
    return i2c_smbus_write_byte(data->client, pins);
}

/* 상위 4비트를 DB4..DB7에 출력. R/W는 항상 0(쓰기). */
static int fan_lcd_write_nibble(struct fan_lcd_data *data, u8 value, bool rs)
{
    u8 pins = (value & LCD_DATA_MASK) | (rs ? LCD_RS : 0) |
              (data->backlight ? LCD_BACKLIGHT_MASK : 0);
    int ret;

    ret = fan_lcd_output(data, pins);
    if (ret)
        return ret;

    ret = fan_lcd_output(data, pins | LCD_ENABLE);
    if (ret)
        return ret;

    udelay(1);

    ret = fan_lcd_output(data, pins);
    if (ret)
        return ret;

    usleep_range(50, 100);
    return 0;
}

static int fan_lcd_write_byte(struct fan_lcd_data *data, u8 value, bool rs)
{
    int ret;

    ret = fan_lcd_write_nibble(data, value, rs);
    if (ret)
        return ret;

    return fan_lcd_write_nibble(data, value << 4, rs);
}

static int fan_lcd_command(struct fan_lcd_data *data, u8 command)
{
    int ret = fan_lcd_write_byte(data, command, false);

    if (!ret && (command == LCD_CLEAR_CMD || command == LCD_HOME_CMD))
        msleep(2);

    return ret;
}

static int fan_lcd_write_char(struct fan_lcd_data *data, u8 character)
{
    return fan_lcd_write_byte(data, character, true);
}

int fan_lcd_write_line(struct fan_lcd_data *data, u8 address,
                              const char *text)
{
    int ret;
    int i;

    ret = fan_lcd_command(data, address);
    if (ret)
        return ret;

    for (i = 0; i < LCD_COLUMNS && text[i]; i++) {
        ret = fan_lcd_write_char(data, text[i]);
        if (ret)
            return ret;
    }

    return 0;
}

int fan_lcd_initialize(struct fan_lcd_data *data)
{
    static const u8 commands[] = {
        0x28,          /* 4비트 모드, 2행, 5x8 폰트 */
        0x08,          /* 초기화 중 화면 끄기 */
        LCD_CLEAR_CMD,
        0x06,          /* 문자 출력 후 주소 증가, 화면 이동 없음 */
        0x0c,          /* 화면 켜기, 커서·깜빡임 끄기 */
    };
    int ret;
    size_t i;

    /* E와 R/W를 낮춘 뒤 전원 안정화 시간을 확보한다. */
    ret = fan_lcd_output(data, data->backlight ? LCD_BACKLIGHT_MASK : 0);
    if (ret)
        return ret;
    msleep(50);

    ret = fan_lcd_write_nibble(data, 0x30, false);
    if (ret)
        return ret;
    msleep(5);

    ret = fan_lcd_write_nibble(data, 0x30, false);
    if (ret)
        return ret;
    usleep_range(200, 300);

    ret = fan_lcd_write_nibble(data, 0x30, false);
    if (ret)
        return ret;

    ret = fan_lcd_write_nibble(data, 0x20, false);
    if (ret)
        return ret;

    for (i = 0; i < ARRAY_SIZE(commands); i++) {
        ret = fan_lcd_command(data, commands[i]);
        if (ret)
            return ret;
    }

    return 0;
}


int fan_lcd_set_cursor(struct fan_lcd_data *data, unsigned int offset)
{
    if (offset >= FAN_LCD_SIZE)
        return -EINVAL;
    return fan_lcd_command(data, (offset < LCD_COLUMNS ? LCD_ROW1_ADDR :
                           LCD_ROW2_ADDR) + offset % LCD_COLUMNS);
}

int fan_lcd_put_char(struct fan_lcd_data *data, u8 character)
{
    return fan_lcd_write_char(data, character);
}

int fan_lcd_clear(struct fan_lcd_data *data)
{
    return fan_lcd_command(data, LCD_CLEAR_CMD);
}

int fan_lcd_set_backlight(struct fan_lcd_data *data, bool enable)
{
    int ret = fan_lcd_output(data, enable ? LCD_BACKLIGHT_MASK : 0);

    if (!ret)
        data->backlight = enable;
    return ret;
}
