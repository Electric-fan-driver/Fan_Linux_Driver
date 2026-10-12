// SPDX-License-Identifier: GPL-2.0
/* Jetson Orin Nano / L4T R36.5.2 / Linux 5.15.
 * Header 7=KEY falling, 11=S2 both edges, 36=S1 both edges.
 * IRQ: validate quadrature transitions, count detents, wake readers.
 * Actions run in process context; diagnostic printk is deferred to workqueue.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/bug.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include "fan_input_event.h"
#include "fan_input_logic.h"
#include "fan_event_queue.h"
#ifndef FAN_ENCODER_ID
#define FAN_ENCODER_ID ENC_FAN
#define FAN_BUTTON_ID BTN_ENC_FAN
#endif

#ifndef FAN_DEVICE_NAME
#define FAN_DEVICE_NAME "fandev"
#endif
#ifndef FAN_DEVICE_CLASS
#define FAN_DEVICE_CLASS "fan_input"
#endif
#ifndef FAN_DEVICE_MAJOR
#define FAN_DEVICE_MAJOR 120
#endif
#ifndef FAN_GPIO_KEY
#define FAN_GPIO_KEY 492
#endif
#ifndef FAN_GPIO_S2
#define FAN_GPIO_S2 460
#endif
#ifndef FAN_GPIO_S1
#define FAN_GPIO_S1 461
#endif
#ifndef FAN_KEY_HEADER_PIN
#define FAN_KEY_HEADER_PIN 7
#endif
#ifndef FAN_S2_HEADER_PIN
#define FAN_S2_HEADER_PIN 11
#endif
#ifndef FAN_S1_HEADER_PIN
#define FAN_S1_HEADER_PIN 36
#endif
#ifndef FAN_MODULE_DESCRIPTION
#define FAN_MODULE_DESCRIPTION "Detent-count KEY falling / S1 S2 both-edge IRQ driver"
#endif
#ifndef FAN_PARAM_GPIO_KEY
#define FAN_PARAM_GPIO_KEY gpio_key
#define FAN_PARAM_GPIO_S2 gpio_s2
#define FAN_PARAM_GPIO_S1 gpio_s1
#define FAN_PARAM_ENABLE_GPIO enable_gpio
#define FAN_PARAM_IRQ_MASK irq_mask
#define FAN_PARAM_INVERT_DIRECTION invert_direction
#define FAN_PARAM_KEY_DEBOUNCE_MS key_debounce_ms
#define FAN_PARAM_DEBUG_WIND_LEVEL debug_wind_level
#endif
#ifndef FAN_PARAM_STEPS_PER_DETENT
#define FAN_PARAM_STEPS_PER_DETENT steps_per_detent
#endif
#ifndef FAN_DEBUG_WIND_LEVEL_DEFAULT
#define FAN_DEBUG_WIND_LEVEL_DEFAULT true
#endif
#define DEVICE_NAME FAN_DEVICE_NAME
#define DEVICE_CLASS FAN_DEVICE_CLASS
#define DEVICE_MAJOR FAN_DEVICE_MAJOR
#define NUM_LINES 3
#define S2_LINE 0
#define S1_LINE 1
#define KEY_LINE 2
#define ALL_IRQ_MASK 0x07U
/* Expand per-instance parameter names before MODULE_PARM_DESC stringifies. */
#define FAN_PARM_DESC(name, description) MODULE_PARM_DESC(name, description)

/* Legacy Linux GPIO numbers, NOT physical header pin numbers. */
static int gpio_key = FAN_GPIO_KEY;
static int gpio_s2 = FAN_GPIO_S2;
static int gpio_s1 = FAN_GPIO_S1;
module_param_named(FAN_PARAM_GPIO_KEY, gpio_key, int, 0444);
module_param_named(FAN_PARAM_GPIO_S2, gpio_s2, int, 0444);
module_param_named(FAN_PARAM_GPIO_S1, gpio_s1, int, 0444);
FAN_PARM_DESC(FAN_PARAM_GPIO_KEY, "KEY GPIO (falling edge)");
FAN_PARM_DESC(FAN_PARAM_GPIO_S2, "S2 GPIO (rising and falling edges)");
FAN_PARM_DESC(FAN_PARAM_GPIO_S1, "S1 GPIO (rising and falling edges)");
/* Safe startup retained: this board previously hung during IRQ tests. */
static bool enable_gpio;
module_param_named(FAN_PARAM_ENABLE_GPIO, enable_gpio, bool, 0444);
FAN_PARM_DESC(FAN_PARAM_ENABLE_GPIO, "Request GPIO/IRQ lines (default: false)");
static unsigned int irq_mask = ALL_IRQ_MASK;
module_param_named(FAN_PARAM_IRQ_MASK, irq_mask, uint, 0444);
FAN_PARM_DESC(FAN_PARAM_IRQ_MASK, "0x04=KEY, 0x03=S2/S1, 0x07=all three pins");
static bool invert_direction;
module_param_named(FAN_PARAM_INVERT_DIRECTION, invert_direction, bool, 0444);
FAN_PARM_DESC(FAN_PARAM_INVERT_DIRECTION, "Swap CW/CCW to match physical wiring");
static unsigned int steps_per_detent = 4;
module_param_named(FAN_PARAM_STEPS_PER_DETENT, steps_per_detent, uint, 0444);
FAN_PARM_DESC(FAN_PARAM_STEPS_PER_DETENT, "Valid phase transitions per click: 4 (default) or 2");
static unsigned int key_debounce_ms = 200;
module_param_named(FAN_PARAM_KEY_DEBOUNCE_MS, key_debounce_ms, uint, 0444);
FAN_PARM_DESC(FAN_PARAM_KEY_DEBOUNCE_MS, "KEY falling-edge debounce (default: 200 ms)");
/* Bench-test level only. The application owner implements MAN/AUTO policy.
 * Disable this diagnostic on integration; it never controls any output. */
static bool debug_wind_level = FAN_DEBUG_WIND_LEVEL_DEFAULT;
module_param_named(FAN_PARAM_DEBUG_WIND_LEVEL, debug_wind_level, bool, 0444);
FAN_PARM_DESC(FAN_PARAM_DEBUG_WIND_LEVEL, "Log encoder diagnostic wind level (0..4)");

struct irq_line {
    int gpio;
    int irq;
    unsigned int index;
    bool gpio_requested;
    bool irq_requested;
};
static struct irq_line lines[NUM_LINES];
static dev_t devt;
static struct cdev fan_cdev;
static struct class *fan_class;
static bool gpio_initialized;
static atomic_t reader_open = ATOMIC_INIT(0);

/* Every access to pending/total/key debounce/decoder state takes count_lock.
 * Separate direction counts preserve CW then CCW without cancellation.
 */
static struct fan_input_counts pending;
static struct fan_input_counts total;
static DEFINE_SPINLOCK(count_lock);
static DEFINE_MUTEX(read_lock);
static DECLARE_WAIT_QUEUE_HEAD(count_wait);
static u64 last_key_ns;
static bool key_seen;
static unsigned int debug_level;
static u64 debug_level_sequence;
static u64 debug_level_logged_sequence;
static struct fan_encoder_decoder encoder;

static void fan_wind_log_work(struct work_struct *work)
{
    unsigned long flags;
    unsigned int level;
    bool changed;

    (void)work;
    spin_lock_irqsave(&count_lock, flags);
    changed = debug_level_sequence != debug_level_logged_sequence;
    level = debug_level;
    debug_level_logged_sequence = debug_level_sequence;
    spin_unlock_irqrestore(&count_lock, flags);
    /* Coalesce rapid changes, but never run printk in hard IRQ context.
     * The actual level was updated in input order; only logs may coalesce.
     */
    if (changed)
        pr_info("%s: wind_level=%u (debug, range=0..4)\n", DEVICE_NAME, level);
}
static DECLARE_WORK(wind_log_work, fan_wind_log_work);

static bool fan_has_count(void)
{
    unsigned long flags;
    bool available;

    spin_lock_irqsave(&count_lock, flags);
    available = pending.cw_count || pending.ccw_count || pending.key_count;
    spin_unlock_irqrestore(&count_lock, flags);
    return available;
}

static irqreturn_t fan_gpio_irq_handler(int irq, void *dev_id)
{
    struct irq_line *line = dev_id;
    unsigned long flags;
    u64 now = 0;
    int direction = 0;
    unsigned int phase;
    bool changed = false;
    bool level_changed = false;
    bool was_empty;
    bool event_wake = false;
    unsigned int level = 0;

    (void)irq;
    if (line->index == KEY_LINE) {
        now = ktime_get_ns();
    }
    spin_lock_irqsave(&count_lock, flags);
    if (line->index != KEY_LINE) {
        /* Serialize sampling and decoder updates across both phase IRQs.
         * Init rejects GPIO controllers that can sleep. Repeated snapshots
         * are harmless; missed two-bit transitions invalidate the click.
         */
        phase = (gpio_get_value(gpio_s1) ? 2U : 0U) |
                (gpio_get_value(gpio_s2) ? 1U : 0U);
        direction = fan_encoder_decode(&encoder, phase, invert_direction);
        if (!direction) {
            spin_unlock_irqrestore(&count_lock, flags);
            return IRQ_HANDLED;
        }
    }
    was_empty = !pending.cw_count && !pending.ccw_count && !pending.key_count;
    if (line->index == KEY_LINE) {
        if (!key_seen || now - last_key_ns >= (u64)key_debounce_ms * 1000000ULL) {
            key_seen = true;
            last_key_ns = now;
            ++pending.key_count;
            ++total.key_count;
            changed = true;
        }
    } else if (direction > 0) {
        ++pending.cw_count;
        ++total.cw_count;
        changed = true;
    } else {
        ++pending.ccw_count;
        ++total.ccw_count;
        changed = true;
    }
    if (direction && debug_wind_level) {
        level = fan_debug_level_step(debug_level, direction);
        level_changed = level != debug_level;
        debug_level = level;
        if (level_changed)
            ++debug_level_sequence;
    }
    if (changed)
        event_wake = fan_event_enqueue(line->index == KEY_LINE ? FAN_EV_BTN : FAN_EV_ROT,
                                      line->index == KEY_LINE ? FAN_BUTTON_ID : FAN_ENCODER_ID,
                                      line->index == KEY_LINE ? 1 : direction);
    spin_unlock_irqrestore(&count_lock, flags);
    if (event_wake)
        fan_event_wake();
    if (level_changed)
        schedule_work(&wind_log_work);
    /* Once a batch is pending, poll/read already see readiness. Avoid
     * repeatedly waking the reader for every click in the same batch.
     */
    if (changed && was_empty)
        wake_up_interruptible(&count_wait);
    return IRQ_HANDLED;
}

static int fan_input_open(struct inode *inode, struct file *file)
{
    int ret;

    /* One consumer; the application can safely poll then read. */
    if (atomic_cmpxchg(&reader_open, 0, 1))
        return -EBUSY;
    ret = nonseekable_open(inode, file);
    if (ret)
        atomic_set(&reader_open, 0);
    return ret;
}

static int fan_input_release(struct inode *inode, struct file *file)
{
    (void)inode;
    (void)file;
    atomic_set(&reader_open, 0);
    return 0;
}

static ssize_t fan_input_read(struct file *file, char __user *buf,
                              size_t size, loff_t *ppos)
{
    struct fan_input_counts batch = { .abi_version = FAN_INPUT_ABI_VERSION };
    struct fan_input_counts totals;
    unsigned long flags;
    int ret;

    (void)ppos;
    if (size < sizeof(batch))
        return -EINVAL;
    if (file->f_flags & O_NONBLOCK) {
        if (!mutex_trylock(&read_lock))
            return -EAGAIN;
    } else {
        ret = mutex_lock_interruptible(&read_lock);
        if (ret)
            return ret;
    }
    for (;;) {
        spin_lock_irqsave(&count_lock, flags);
        batch.cw_count = pending.cw_count;
        batch.ccw_count = pending.ccw_count;
        batch.key_count = pending.key_count;
        spin_unlock_irqrestore(&count_lock, flags);
        if (batch.cw_count || batch.ccw_count || batch.key_count)
            break;
        if (file->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            goto out;
        }
        ret = wait_event_interruptible(count_wait, fan_has_count());
        if (ret)
            goto out;
    }
    /* copy_to_user can sleep: no spinlock here. Keep pending on EFAULT.
     * read_lock serializes reads; subtract only the copied batch so IRQs
     * arriving during the copy remain pending for the next read.
     */
    if (copy_to_user(buf, &batch, sizeof(batch))) {
        ret = -EFAULT;
        goto out;
    }
    spin_lock_irqsave(&count_lock, flags);
    pending.cw_count -= batch.cw_count;
    pending.ccw_count -= batch.ccw_count;
    pending.key_count -= batch.key_count;
    totals = total;
    spin_unlock_irqrestore(&count_lock, flags);
    pr_info_ratelimited("%s: CW=%llu CCW=%llu KEY=%llu (total detents/presses)\n",
                        DEVICE_NAME,
                        (unsigned long long)totals.cw_count,
                        (unsigned long long)totals.ccw_count,
                        (unsigned long long)totals.key_count);
    ret = sizeof(batch);
out:
    mutex_unlock(&read_lock);
    return ret;
}

static __poll_t fan_input_poll(struct file *file, poll_table *wait)
{
    poll_wait(file, &count_wait, wait);
    return fan_has_count() ? EPOLLIN | EPOLLRDNORM : 0;
}

static const struct file_operations fan_fops = {
    .owner = THIS_MODULE,
    .open = fan_input_open,
    .release = fan_input_release,
    .read = fan_input_read,
    .poll = fan_input_poll,
    .llseek = no_llseek,
};

static void fan_gpio_cleanup(void)
{
    int i;

    /* Quiesce all handlers before releasing any GPIO they might sample. */
    for (i = 0; i < NUM_LINES; ++i) {
        if (lines[i].irq_requested) {
            free_irq(lines[i].irq, &lines[i]);
            lines[i].irq_requested = false;
        }
    }
    /* IRQs can no longer enqueue work. Finish the last diagnostic before
     * module code is unloaded, including partially initialized error paths.
     */
    flush_work(&wind_log_work);
    for (i = 0; i < NUM_LINES; ++i) {
        if (lines[i].gpio_requested) {
            gpio_free(lines[i].gpio);
            lines[i].gpio_requested = false;
        }
    }
}

static int fan_gpio_init(void)
{
    int gpios[NUM_LINES] = { gpio_s2, gpio_s1, gpio_key };
    struct gpio_desc *desc;
    unsigned long trigger;
    int i, j, ret;

    if (!irq_mask || (irq_mask & ~ALL_IRQ_MASK) ||
        ((irq_mask & 0x03U) && (irq_mask & 0x03U) != 0x03U))
        return -EINVAL;
    for (i = 0; i < NUM_LINES; ++i) {
        if (!(irq_mask & BIT(i)))
            continue;
        if (!gpio_is_valid(gpios[i]))
            return -EINVAL;
        for (j = 0; j < i; ++j)
            if ((irq_mask & BIT(j)) && gpios[i] == gpios[j])
                return -EINVAL;
    }
    /* Configure both phases before registering any IRQ that samples them. */
    for (i = 0; i < NUM_LINES; ++i) {
        lines[i].gpio = gpios[i];
        lines[i].index = i;
        if (!(irq_mask & BIT(i)))
            continue;
        ret = gpio_request(gpios[i], DEVICE_NAME);
        if (ret)
            goto error;
        lines[i].gpio_requested = true;
        ret = gpio_direction_input(gpios[i]);
        if (ret)
            goto error;
        desc = gpio_to_desc(gpios[i]);
        if (!desc || gpiod_cansleep(desc)) {
            ret = -EINVAL;
            goto error;
        }
    }
    /* Initialize from the resting phase before either rotation IRQ is live.
     * Keep the knob stationary on a click while loading the module.
     */
    if (irq_mask & 0x03U)
        fan_encoder_init(&encoder,
                         (gpio_get_value(gpio_s1) ? 2U : 0U) |
                         (gpio_get_value(gpio_s2) ? 1U : 0U),
                         steps_per_detent);
    for (i = 0; i < NUM_LINES; ++i) {
        if (!(irq_mask & BIT(i)))
            continue;
        ret = gpio_to_irq(lines[i].gpio);
        if (ret <= 0) {
            if (!ret)
                ret = -EINVAL;
            goto error;
        }
        lines[i].irq = ret;
        trigger = i == KEY_LINE ? IRQF_TRIGGER_FALLING :
                                 (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING);
        ret = request_irq(lines[i].irq, fan_gpio_irq_handler, trigger,
                          DEVICE_NAME, &lines[i]);
        if (ret)
            goto error;
        lines[i].irq_requested = true;
        pr_info("fan_input: GPIO %d -> IRQ %d %s registered\n",
                lines[i].gpio, lines[i].irq,
                i == KEY_LINE ? "KEY falling" : (i == S1_LINE ? "S1 both edges" : "S2 both edges"));
    }
    return 0;
error:
    pr_err("fan_input: GPIO/IRQ setup failed: %d\n", ret);
    fan_gpio_cleanup();
    return ret;
}

static int __init fan_input_init(void)
{
    struct device *device;
    int ret;

    BUILD_BUG_ON(sizeof(struct fan_input_counts) != FAN_INPUT_PACKET_SIZE);
    if (!fan_encoder_steps_valid(steps_per_detent)) {
        pr_err("%s: steps_per_detent must be 2 or 4\n", DEVICE_NAME);
        return -EINVAL;
    }
    devt = MKDEV(DEVICE_MAJOR, 0);
    ret = register_chrdev_region(devt, 1, DEVICE_NAME);
    if (ret)
        return ret;
    cdev_init(&fan_cdev, &fan_fops);
    fan_cdev.owner = THIS_MODULE;
    ret = cdev_add(&fan_cdev, devt, 1);
    if (ret)
        goto err_chrdev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    fan_class = class_create(DEVICE_CLASS);
#else
    fan_class = class_create(THIS_MODULE, DEVICE_CLASS);
#endif
    if (IS_ERR(fan_class)) {
        ret = PTR_ERR(fan_class);
        goto err_cdev;
    }
    device = device_create(fan_class, NULL, devt, NULL, DEVICE_NAME);
    if (IS_ERR(device)) {
        ret = PTR_ERR(device);
        goto err_class;
    }
    if (enable_gpio) {
        ret = fan_gpio_init();
        if (ret)
            goto err_device;
        gpio_initialized = true;
        pr_info("%s: IRQ enabled, mask=0x%02x steps_per_detent=%u\n",
                DEVICE_NAME, irq_mask, steps_per_detent);
    } else {
        pr_info("fan_input: safe mode: no GPIO/IRQ requested\n");
    }
    pr_info("fan_input: /dev/%s major=%d ABI=%u packet=%u bytes\n",
            DEVICE_NAME, MAJOR(devt), FAN_INPUT_ABI_VERSION, FAN_INPUT_PACKET_SIZE);
    return 0;
err_device:
    device_destroy(fan_class, devt);
err_class:
    class_destroy(fan_class);
err_cdev:
    cdev_del(&fan_cdev);
err_chrdev:
    unregister_chrdev_region(devt, 1);
    return ret;
}

/* Also used by bundle init rollback, so do not put this in .exit.text. */
static void fan_input_exit(void)
{
    if (gpio_initialized)
        fan_gpio_cleanup();
    /* IRQs have stopped: exact final totals. */
    pr_info("%s: final CW=%llu CCW=%llu KEY=%llu\n", DEVICE_NAME,
            (unsigned long long)total.cw_count,
            (unsigned long long)total.ccw_count,
            (unsigned long long)total.key_count);
    device_destroy(fan_class, devt);
    class_destroy(fan_class);
    cdev_del(&fan_cdev);
    unregister_chrdev_region(devt, 1);
}
#ifndef FAN_EMBEDDED
module_init(fan_input_init);
module_exit(fan_input_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("fan-project");
MODULE_DESCRIPTION(FAN_MODULE_DESCRIPTION);
#endif
