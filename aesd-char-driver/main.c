/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include "aesdchar.h"
int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("Zehariah Oginsky");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                  loff_t *f_pos)
{
    ssize_t retval = 0;
    struct aesd_dev *dev = filp->private_data;
    uint8_t pos = dev->buff.out_offs;
    size_t total_length = 0;
    size_t bytes_to_write;
    char *temp_buf = NULL;

    if (mutex_lock_interruptible(&dev->mutex))
    {
        return -ERESTARTSYS;
    }

    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);

    if (!filp || !buf)
    {
        mutex_unlock(&dev->mutex);
        return -EINVAL;
    }

    do
    {
        total_length += dev->buff.entry[pos].size;
        if (!temp_buf)
        {
            temp_buf = kzalloc(total_length, GFP_KERNEL);
            if (!temp_buf)
            {
                mutex_unlock(&dev->mutex);
                return -ENOMEM;
            }
        }
        else
        {
            temp_buf = krealloc(temp_buf, total_length, GFP_KERNEL);
            if (!temp_buf)
            {
                mutex_unlock(&dev->mutex);
                return -ENOMEM;
            }
            memset(temp_buf + (total_length - dev->buff.entry[pos].size), 0, dev->buff.entry[pos].size);
        }

        memcpy(temp_buf + (total_length - dev->buff.entry[pos].size), dev->buff.entry[pos].buffptr, dev->buff.entry[pos].size);

        if (pos == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - 1)
        {
            pos = 0;
        }
        else
        {
            pos++;
        }
    } while (pos != dev->buff.in_offs);

    temp_buf = krealloc(temp_buf, total_length + dev->wb_len, GFP_KERNEL);
    if (!temp_buf)
    {
        mutex_unlock(&dev->mutex);
        return -ENOMEM;
    }
    memset(temp_buf + total_length, 0, dev->wb_len);
    memcpy(temp_buf + total_length, dev->working_buffer, dev->wb_len);

    bytes_to_write = total_length - *f_pos;
    if (bytes_to_write > count)
    {
        bytes_to_write = count;
    }
    if (copy_to_user(buf, temp_buf + (*f_pos), bytes_to_write))
    {
        mutex_unlock(&dev->mutex);
        return -EFAULT;
    }
    *f_pos += bytes_to_write;

    kfree(temp_buf);

    mutex_unlock(&dev->mutex);

    retval = bytes_to_write;
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    size_t entry_len, offset;
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry new_entry;
    char *newline;

    if (mutex_lock_interruptible(&dev->mutex))
    {
        return -ERESTARTSYS;
    }

    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

    if (!filp || !buf)
    {
        mutex_unlock(&dev->mutex);
        return -EINVAL;
    }

    if (!dev->working_buffer)
    {
        dev->working_buffer = kmalloc(count, GFP_KERNEL);
        if (!dev->working_buffer)
        {
            mutex_unlock(&dev->mutex);
            return -ENOMEM;
        }
        dev->wb_len = count;
        dev->wb_alloc_len = count;
        offset = 0;
    }
    else
    {
        if (dev->wb_alloc_len < (dev->wb_len + count))
        {
            dev->working_buffer = krealloc(dev->working_buffer, dev->wb_len + count, GFP_KERNEL);
            if (!dev->working_buffer)
            {
                mutex_unlock(&dev->mutex);
                return -ENOMEM;
            }
            dev->wb_alloc_len += count;
        }
        memset(dev->working_buffer + dev->wb_len, 0, count);
        offset = dev->wb_len;
        dev->wb_len += count;
    }

    if (copy_from_user(dev->working_buffer + offset, buf, count))
    {
        mutex_unlock(&dev->mutex);
        return -EFAULT;
    }

    do
    {
        newline = memchr(dev->working_buffer, '\n', dev->wb_len);
        if (newline)
        {
            entry_len = 1 + (newline - dev->working_buffer);
            new_entry.buffptr = kzalloc(entry_len, GFP_KERNEL);
            if (!dev->working_buffer)
            {
                mutex_unlock(&dev->mutex);
                return -ENOMEM;
            }
            memcpy(new_entry.buffptr, dev->working_buffer, entry_len);
            new_entry.size = entry_len;
            if (dev->buff.full)
            {
                kfree(dev->buff.entry[dev->buff.in_offs].buffptr);
                dev->buff.entry[dev->buff.in_offs].size = 0;
            }
            aesd_circular_buffer_add_entry(&dev->buff, &new_entry);
            memset(dev->working_buffer, 0, entry_len);
            memcpy(dev->working_buffer, dev->working_buffer + entry_len, dev->wb_len - entry_len);
            memset(dev->working_buffer + (dev->wb_len - entry_len), 0, dev->wb_len - entry_len);
            dev->wb_len -= entry_len;
            offset = dev->wb_len - 1;
            filp->f_pos += entry_len;
        }
    } while (newline);

    mutex_unlock(&dev->mutex);

    retval = count;
    return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t f_pos, int origin)
{
    loff_t retval, total_length;
    struct aesd_dev *dev = filp->private_data;
    uint8_t pos = dev->buff.out_offs;

    PDEBUG("llseek postion: %lld, mode: %i", f_pos, origin);

    switch (origin)
    {
    case SEEK_SET:
        retval = f_pos;
        break;
    case SEEK_CUR:
        retval = filp->f_pos + f_pos;
        break;
    case SEEK_END:
        do
        {
            total_length += dev->buff.entry[pos].size;

            if (pos == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - 1)
            {
                pos = 0;
            }
            else
            {
                pos++;
            }
        } while (pos != dev->buff.in_offs);
        retval = total_length + f_pos;
        break;
    default:
        return -EINVAL;
    }

    if (retval < 0)
    {
        return -EINVAL;
    }
    filp->f_pos = retval;
    return retval;
}

struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
    .write = aesd_write,
    .open = aesd_open,
    .release = aesd_release,
    .llseek = aesd_llseek,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
    {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
                                 "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0)
    {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device, 0, sizeof(struct aesd_dev));

    mutex_init(&aesd_device.mutex);

    aesd_circular_buffer_init(&aesd_device.buff);

    aesd_device.working_buffer = NULL;
    aesd_device.wb_len = 0;

    result = aesd_setup_cdev(&aesd_device);

    if (result)
    {
        unregister_chrdev_region(dev, 1);
    }
    return result;
}

void aesd_cleanup_module(void)
{
    int idx = 0;
    struct aesd_buffer_entry *entry = NULL;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    if (aesd_device.working_buffer)
    {
        kfree(aesd_device.working_buffer);
    }

    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buff, idx)
    {
        if (entry->buffptr)
        {
            kfree(entry->buffptr);
        }
    }

    mutex_destroy(&aesd_device.mutex);

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
