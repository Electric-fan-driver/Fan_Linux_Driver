// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/bug.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/kfifo.h>
#include <linux/ktime.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include "fan_event_queue.h"

/* One ordered stream shared by both encoders, 512 fixed-size events. */
static DECLARE_KFIFO(events, struct fan_input_event, 512);
static DEFINE_SPINLOCK(event_lock);
static DEFINE_MUTEX(event_read_lock);
static DECLARE_WAIT_QUEUE_HEAD(event_wait);
static atomic_t reader_open = ATOMIC_INIT(0);
static bool ready;
static bool overflow_pending;
static u64 dropped_events;

bool fan_event_enqueue(__u8 type, __u8 id, __s8 value)
{
    struct fan_input_event event = { .type = type, .id = id, .value = value };
    unsigned long flags;
    bool wake;

    spin_lock_irqsave(&event_lock, flags);
    wake = kfifo_is_empty(&events) && !overflow_pending;
    event.ts_ms = (__u32)(ktime_get_ns() / 1000000ULL);
    if (!kfifo_put(&events, event)) {
        wake = wake || !overflow_pending;
        overflow_pending = true;
        ++dropped_events;
    }
    spin_unlock_irqrestore(&event_lock, flags);
    return wake;
}

void fan_event_wake(void)
{
    wake_up_interruptible(&event_wait);
}

static bool event_available(void)
{
    unsigned long flags;
    bool available;
    spin_lock_irqsave(&event_lock, flags);
    available = !kfifo_is_empty(&events) || overflow_pending || !ready;
    spin_unlock_irqrestore(&event_lock, flags);
    return available;
}

static int event_open(struct inode *inode, struct file *file)
{
    int ret;
    if (!READ_ONCE(ready))
        return -ENODEV;
    if (atomic_cmpxchg(&reader_open, 0, 1))
        return -EBUSY;
    ret = nonseekable_open(inode, file);
    if (ret)
        atomic_set(&reader_open, 0);
    return ret;
}

static int event_release(struct inode *inode, struct file *file)
{
    (void)inode;
    (void)file;
    atomic_set(&reader_open, 0);
    return 0;
}

static ssize_t event_read(struct file *file, char __user *buf, size_t size, loff_t *offset)
{
    struct fan_input_event event;
    unsigned long flags;
    int ret;
    bool has_event;
    (void)offset;
    if (size < sizeof(event))
        return -EINVAL;
    if (file->f_flags & O_NONBLOCK) {
        if (!mutex_trylock(&event_read_lock))
            return -EAGAIN;
    } else {
        ret = mutex_lock_interruptible(&event_read_lock);
        if (ret)
            return ret;
    }
    for (;;) {
        spin_lock_irqsave(&event_lock, flags);
        if (overflow_pending) {
            overflow_pending = false;
            spin_unlock_irqrestore(&event_lock, flags);
            ret = -EOVERFLOW;
            goto out;
        }
        if (!ready) {
            spin_unlock_irqrestore(&event_lock, flags);
            ret = -ENODEV;
            goto out;
        }
        has_event = kfifo_out_peek(&events, &event, 1) == 1;
        spin_unlock_irqrestore(&event_lock, flags);
        if (has_event)
            break;
        if (file->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            goto out;
        }
        ret = wait_event_interruptible(event_wait, event_available());
        if (ret)
            goto out;
    }
    /* Do not consume on EFAULT. Only this reader can remove the FIFO head. */
    if (copy_to_user(buf, &event, sizeof(event))) {
        ret = -EFAULT;
        goto out;
    }
    spin_lock_irqsave(&event_lock, flags);
    has_event = kfifo_get(&events, &event);
    spin_unlock_irqrestore(&event_lock, flags);
    ret = has_event ? (int)sizeof(event) : -EIO;
out:
    mutex_unlock(&event_read_lock);
    return ret;
}

static __poll_t event_poll(struct file *file, poll_table *wait)
{
    unsigned long flags;
    __poll_t mask = 0;
    poll_wait(file, &event_wait, wait);
    spin_lock_irqsave(&event_lock, flags);
    if (!kfifo_is_empty(&events))
        mask |= EPOLLIN | EPOLLRDNORM;
    if (overflow_pending)
        mask |= EPOLLERR;
    if (!ready)
        mask |= EPOLLHUP;
    spin_unlock_irqrestore(&event_lock, flags);
    return mask;
}

static const struct file_operations event_fops = {
    .owner = THIS_MODULE,
    .open = event_open,
    .release = event_release,
    .read = event_read,
    .poll = event_poll,
    .llseek = no_llseek,
};
static struct miscdevice event_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "fan_input",
    .fops = &event_fops,
};

int fan_event_device_init(void)
{
    BUILD_BUG_ON(sizeof(struct fan_input_event) != 8);
    ready = false;
    overflow_pending = false;
    dropped_events = 0;
    INIT_KFIFO(events);
    return misc_register(&event_device);
}
void fan_event_device_start(void)
{
    unsigned long flags;
    spin_lock_irqsave(&event_lock, flags);
    ready = true;
    spin_unlock_irqrestore(&event_lock, flags);
    fan_event_wake();
    pr_info("fan_input: /dev/fan_input event ABI=%u packet=8 bytes\n", FAN_IOCTL_VERSION);
}
void fan_event_device_exit(void)
{
    unsigned long flags;
    spin_lock_irqsave(&event_lock, flags);
    ready = false;
    spin_unlock_irqrestore(&event_lock, flags);
    fan_event_wake();
    misc_deregister(&event_device);
    pr_info("fan_input: event FIFO dropped=%llu\n", (unsigned long long)dropped_events);
}
