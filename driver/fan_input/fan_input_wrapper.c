/* SPDX-License-Identifier: GPL-2.0 */
/* One module, ordered /dev/fan_input (8-byte events) plus legacy count devices:
 * /dev/fandev and /dev/timerdev keep their 32-byte ABI for old applications.
 */
#include <linux/module.h>

#define FAN_EMBEDDED
#include "fan_input.c"

int __init timer_input_init_child(void);
void timer_input_exit_child(void);
int __init button_input_init_child(void);
void button_input_exit_child(void);
static unsigned int event_abi = FAN_IOCTL_VERSION;
module_param(event_abi, uint, 0444);
MODULE_PARM_DESC(event_abi, "Ordered /dev/fan_input event ABI (must be 1)");

static int __init fan_input_bundle_init(void)
{
    int ret;

    if (event_abi != FAN_IOCTL_VERSION)
        return -EINVAL;
    ret = fan_event_device_init();
    if (ret)
        return ret;
    ret = fan_input_init();
    if (ret)
        goto err_events;
    ret = timer_input_init_child();
    if (ret) {
        fan_input_exit();
        goto err_events;
    }
    ret = button_input_init_child();
    if (ret) {
        timer_input_exit_child();
        fan_input_exit();
        goto err_events;
    }
    fan_event_device_start();
    return 0;
err_events:
    fan_event_device_exit();
    return ret;
}

static void __exit fan_input_bundle_exit(void)
{
    button_input_exit_child();
    timer_input_exit_child();
    fan_input_exit();
    fan_event_device_exit();
}

module_init(fan_input_bundle_init);
module_exit(fan_input_bundle_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("fan-project");
MODULE_DESCRIPTION("Fan and timer ordered 8-byte event driver with legacy count devices");
