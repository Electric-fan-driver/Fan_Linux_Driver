// SPDX-License-Identifier: GPL-2.0
// 전원 버튼, 회전 버튼 (헤더 29번, 31번)
// 디바이스 트리 fan_input 노드의 power-gpios, swing-gpios 를 읽어서,
// 버튼을 누를 때마다 /dev/fan_input 에 이벤트(FAN_EV_BTN, BTN_POWER 또는 BTN_SWING, 값 1)를 넣는다.
// 엔코더 드라이버(fan_input.c)와 같은 이벤트 줄(fan_event_queue.c)을 쓴다.
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include "fan_event_queue.h"

#define NUM_BUTTONS	2

struct button {
	struct gpio_desc *gpio;
	struct delayed_work work;
	u8 id;
};

static struct button buttons[NUM_BUTTONS];
static int button_irqs[NUM_BUTTONS];
static unsigned int debounce_ms = 20;	//DT 의 debounce-ms
static const char *const button_names[NUM_BUTTONS] = { "power", "swing" };
static const u8 button_ids[NUM_BUTTONS] = { BTN_POWER, BTN_SWING };

//떨림이 멎고 debounce-ms 가 지난 뒤 실행됨. 눌린 상태이면 이벤트를 넣음
static void button_work(struct work_struct *work)
{
	struct button *b = container_of(to_delayed_work(work), struct button, work);

	if (gpiod_get_value_cansleep(b->gpio) && fan_event_enqueue(FAN_EV_BTN, b->id, 1))
		fan_event_wake();
}

//신호가 바뀔 때마다 불림. 바로 읽지 않고 debounce-ms 뒤로 미룸
static irqreturn_t button_irq(int irq, void *data)
{
	struct button *b = data;

	mod_delayed_work(system_wq, &b->work, msecs_to_jiffies(debounce_ms));
	return IRQ_HANDLED;
}

static int button_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int i, ret;

	device_property_read_u32(dev, "debounce-ms", &debounce_ms);
	for (i = 0; i < NUM_BUTTONS; i++) {
		buttons[i].gpio = devm_gpiod_get_optional(dev, button_names[i], GPIOD_IN);
		if (IS_ERR(buttons[i].gpio))
			return PTR_ERR(buttons[i].gpio);
		if (!buttons[i].gpio)
			continue;
		buttons[i].id = button_ids[i];
		INIT_DELAYED_WORK(&buttons[i].work, button_work);
		button_irqs[i] = gpiod_to_irq(buttons[i].gpio);
		if (button_irqs[i] < 0)
			return button_irqs[i];
		ret = devm_request_irq(dev, button_irqs[i], button_irq,
				       IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				       button_names[i], &buttons[i]);
		if (ret)
			return ret;
	}
	return 0;
}

static int button_remove(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < NUM_BUTTONS; i++) {
		if (!buttons[i].gpio)
			continue;
		disable_irq(button_irqs[i]);
		cancel_delayed_work_sync(&buttons[i].work);
	}
	return 0;
}

static const struct of_device_id button_of_match[] = {
	{ .compatible = "kcci,fan-input" },
	{ }
};

static struct platform_driver button_driver = {
	.probe	= button_probe,
	.remove	= button_remove,
	.driver	= {
		.name = "fan_button_input",
		.of_match_table	= button_of_match,
	},
};

int __init button_input_init_child(void)
{
	return platform_driver_register(&button_driver);
}

void button_input_exit_child(void)
{
	platform_driver_unregister(&button_driver);
}
