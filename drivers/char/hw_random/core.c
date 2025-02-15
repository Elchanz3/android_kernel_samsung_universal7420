/*
        Added support for the AMD Geode LX RNG
	(c) Copyright 2004-2005 Advanced Micro Devices, Inc.

	derived from

 	Hardware driver for the Intel/AMD/VIA Random Number Generators (RNG)
	(c) Copyright 2003 Red Hat Inc <jgarzik@redhat.com>

 	derived from

        Hardware driver for the AMD 768 Random Number Generator (RNG)
        (c) Copyright 2001 Red Hat Inc <alan@redhat.com>

 	derived from

	Hardware driver for Intel i810 Random Number Generator (RNG)
	Copyright 2000,2001 Jeff Garzik <jgarzik@pobox.com>
	Copyright 2000,2001 Philipp Rumpf <prumpf@mandrakesoft.com>

	Added generic RNG API
	Copyright 2006 Michael Buesch <m@bues.ch>
	Copyright 2005 (c) MontaVista Software, Inc.

	Please read Documentation/hw_random.txt for details on use.

	----------------------------------------------------------
	This software may be used and distributed according to the terms
        of the GNU General Public License, incorporated herein by reference.

 */


#include <linux/device.h>
#include <linux/hw_random.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/err.h>
#include <linux/freezer.h>
#include <asm/uaccess.h>


#define RNG_MODULE_NAME		"hw_random"
#define PFX			RNG_MODULE_NAME ": "
#define RNG_MISCDEV_MINOR	183 /* official */


static struct hwrng *current_rng;
static struct task_struct *hwrng_fill;
static LIST_HEAD(rng_list);
/* Protects rng_list and current_rng */
static DEFINE_MUTEX(rng_mutex);
/* Protects rng read functions, data_avail, rng_buffer and rng_fillbuf */
static DEFINE_MUTEX(reading_mutex);
static int data_avail;
static u8 *rng_buffer, *rng_fillbuf;
static unsigned short current_quality;
static unsigned short default_quality; /* = 0; default to "off" */

module_param(current_quality, ushort, 0644);
MODULE_PARM_DESC(current_quality,
		 "current hwrng entropy estimation per mill");
module_param(default_quality, ushort, 0644);
MODULE_PARM_DESC(default_quality,
		 "default entropy content of hwrng per mill");

static void start_khwrngd(void);

static inline int rng_get_data(struct hwrng *rng, u8 *buffer, size_t size,
			       int wait);

static size_t rng_buffer_size(void)
{
	return SMP_CACHE_BYTES < 32 ? 32 : SMP_CACHE_BYTES;
}

static void add_early_randomness(struct hwrng *rng)
{
	unsigned char bytes[16];
	int bytes_read;

	mutex_lock(&reading_mutex);
	bytes_read = rng_get_data(rng, bytes, sizeof(bytes), 1);
	mutex_unlock(&reading_mutex);
	if (bytes_read > 0)
		add_device_randomness(bytes, bytes_read);
}

static inline void cleanup_rng(struct kref *kref)
{
	struct hwrng *rng = container_of(kref, struct hwrng, ref);

	if (rng->cleanup)
		rng->cleanup(rng);
}

static void set_current_rng(struct hwrng *rng)
{
	BUG_ON(!mutex_is_locked(&rng_mutex));
	kref_get(&rng->ref);
	current_rng = rng;
}

static void drop_current_rng(void)
{
	BUG_ON(!mutex_is_locked(&rng_mutex));
	if (!current_rng)
		return;

	/* decrease last reference for triggering the cleanup */
	kref_put(&current_rng->ref, cleanup_rng);
	current_rng = NULL;
}

/* Returns ERR_PTR(), NULL or refcounted hwrng */
static struct hwrng *get_current_rng(void)
{
	struct hwrng *rng;

	if (mutex_lock_interruptible(&rng_mutex))
		return ERR_PTR(-ERESTARTSYS);

	rng = current_rng;
	if (rng)
		kref_get(&rng->ref);

	mutex_unlock(&rng_mutex);
	return rng;
}

static void put_rng(struct hwrng *rng)
{
	/*
	 * Hold rng_mutex here so we serialize in case they set_current_rng
	 * on rng again immediately.
	 */
	mutex_lock(&rng_mutex);
	if (rng)
		kref_put(&rng->ref, cleanup_rng);
	mutex_unlock(&rng_mutex);
}

static inline int hwrng_init(struct hwrng *rng)
{
	if (rng->init) {
		int ret;

		ret =  rng->init(rng);
		if (ret)
			return ret;
	}
	add_early_randomness(rng);

	current_quality = rng->quality ? : default_quality;
	current_quality &= 1023;

	if (current_quality == 0 && hwrng_fill)
		kthread_stop(hwrng_fill);
	if (current_quality > 0 && !hwrng_fill)
		start_khwrngd();

	return 0;
}

static int rng_dev_open(struct inode *inode, struct file *filp)
{
	/* enforce read-only access to this chrdev */
	if ((filp->f_mode & FMODE_READ) == 0)
		return -EINVAL;
	if (filp->f_mode & FMODE_WRITE)
		return -EINVAL;
	return 0;
}

static inline int rng_get_data(struct hwrng *rng, u8 *buffer, size_t size,
			int wait) {
	int present;

	BUG_ON(!mutex_is_locked(&reading_mutex));
	if (rng->read)
		return rng->read(rng, (void *)buffer, size, wait);

	if (rng->data_present)
		present = rng->data_present(rng, wait);
	else
		present = 1;

	if (present)
		return rng->data_read(rng, (u32 *)buffer);

	return 0;
}

static ssize_t rng_dev_read(struct file *filp, char __user *buf,
			    size_t size, loff_t *offp)
{
	ssize_t ret = 0;
	int err = 0;
	int bytes_read, len;
	struct hwrng *rng;

	while (size) {
		rng = get_current_rng();
		if (IS_ERR(rng)) {
			err = PTR_ERR(rng);
			goto out;
		}
		if (!rng) {
			err = -ENODEV;
			goto out;
		}

		mutex_lock(&reading_mutex);
		if (!data_avail) {
			bytes_read = rng_get_data(rng, rng_buffer,
				rng_buffer_size(),
				!(filp->f_flags & O_NONBLOCK));
			if (bytes_read < 0) {
				err = bytes_read;
				goto out_unlock_reading;
			}
			data_avail = bytes_read;
		}

		if (!data_avail) {
			if (filp->f_flags & O_NONBLOCK) {
				err = -EAGAIN;
				goto out_unlock_reading;
			}
		} else {
			len = data_avail;
			if (len > size)
				len = size;

			data_avail -= len;

			if (copy_to_user(buf + ret, rng_buffer + data_avail,
								len)) {
				err = -EFAULT;
				goto out_unlock_reading;
			}

			size -= len;
			ret += len;
		}

		mutex_unlock(&reading_mutex);
		put_rng(rng);

		if (need_resched())
			schedule_timeout_interruptible(1);

		if (signal_pending(current)) {
			err = -ERESTARTSYS;
			goto out;
		}
	}
out:
	return ret ? : err;

out_unlock_reading:
	mutex_unlock(&reading_mutex);
	put_rng(rng);
	goto out;
}


static const struct file_operations rng_chrdev_ops = {
	.owner		= THIS_MODULE,
	.open		= rng_dev_open,
	.read		= rng_dev_read,
	.llseek		= noop_llseek,
};

static struct miscdevice rng_miscdev = {
	.minor		= RNG_MISCDEV_MINOR,
	.name		= RNG_MODULE_NAME,
	.nodename	= "hwrng",
	.fops		= &rng_chrdev_ops,
};


static ssize_t hwrng_attr_current_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
	int err;
	struct hwrng *rng;

	err = mutex_lock_interruptible(&rng_mutex);
	if (err)
		return -ERESTARTSYS;
	err = -ENODEV;
	list_for_each_entry(rng, &rng_list, list) {
		if (strcmp(rng->name, buf) == 0) {
			if (rng == current_rng) {
				err = 0;
				break;
			}
			err = hwrng_init(rng);
			if (err)
				break;
			drop_current_rng();
			set_current_rng(rng);
			err = 0;
			break;
		}
	}
	mutex_unlock(&rng_mutex);

	return err ? : len;
}

static ssize_t hwrng_attr_current_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	ssize_t ret;
	struct hwrng *rng;

	rng = get_current_rng();
	if (IS_ERR(rng))
		return PTR_ERR(rng);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", rng ? rng->name : "none");
	put_rng(rng);

	return ret;
}

static ssize_t hwrng_attr_available_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int err;
	struct hwrng *rng;

	err = mutex_lock_interruptible(&rng_mutex);
	if (err)
		return -ERESTARTSYS;
	buf[0] = '\0';
	list_for_each_entry(rng, &rng_list, list) {
		strlcat(buf, rng->name, PAGE_SIZE);
		strlcat(buf, " ", PAGE_SIZE);
	}
	strlcat(buf, "\n", PAGE_SIZE);
	mutex_unlock(&rng_mutex);

	return strlen(buf);
}

static DEVICE_ATTR(rng_current, S_IRUGO | S_IWUSR,
		   hwrng_attr_current_show,
		   hwrng_attr_current_store);
static DEVICE_ATTR(rng_available, S_IRUGO,
		   hwrng_attr_available_show,
		   NULL);


static void unregister_miscdev(void)
{
	device_remove_file(rng_miscdev.this_device, &dev_attr_rng_available);
	device_remove_file(rng_miscdev.this_device, &dev_attr_rng_current);
	misc_deregister(&rng_miscdev);
}

static int register_miscdev(void)
{
	int err;

	err = misc_register(&rng_miscdev);
	if (err)
		goto out;
	err = device_create_file(rng_miscdev.this_device,
				 &dev_attr_rng_current);
	if (err)
		goto err_misc_dereg;
	err = device_create_file(rng_miscdev.this_device,
				 &dev_attr_rng_available);
	if (err)
		goto err_remove_current;
out:
	return err;

err_remove_current:
	device_remove_file(rng_miscdev.this_device, &dev_attr_rng_current);
err_misc_dereg:
	misc_deregister(&rng_miscdev);
	goto out;
}

static int hwrng_fillfn(void *unused)
{
	long rc;

	set_freezable();

	while (!kthread_should_stop()) {
		struct hwrng *rng;

		rng = get_current_rng();
		if (IS_ERR(rng) || !rng)
			break;
		mutex_lock(&reading_mutex);
		rc = rng_get_data(rng, rng_fillbuf,
				  rng_buffer_size(), 1);
		mutex_unlock(&reading_mutex);
		put_rng(rng);
		if (rc <= 0) {
			pr_warn("hwrng: no data available\n");
			msleep_interruptible(10000);
			continue;
		}
		/* Outside lock, sure, but y'know: randomness. */
		add_hwgenerator_randomness((void *)rng_fillbuf, rc,
					   rc * current_quality * 8 >> 10);
	}
	hwrng_fill = 0;
	return 0;
}

static void start_khwrngd(void)
{
	hwrng_fill = kthread_run(hwrng_fillfn, NULL, "hwrng");
	if (IS_ERR(hwrng_fill)) {
		pr_err("hwrng_fill thread creation failed");
		hwrng_fill = NULL;
	}
}

int hwrng_register(struct hwrng *rng)
{
	int err = -EINVAL;
	struct hwrng *old_rng, *tmp;

	if (rng->name == NULL ||
	    (rng->data_read == NULL && rng->read == NULL))
		goto out;

	mutex_lock(&rng_mutex);

	/* kmalloc makes this safe for virt_to_page() in virtio_rng.c */
	err = -ENOMEM;
	if (!rng_buffer) {
		rng_buffer = kmalloc(rng_buffer_size(), GFP_KERNEL);
		if (!rng_buffer)
			goto out_unlock;
	}
	if (!rng_fillbuf) {
		rng_fillbuf = kmalloc(rng_buffer_size(), GFP_KERNEL);
		if (!rng_fillbuf) {
			kfree(rng_buffer);
			goto out_unlock;
		}
	}

	/* Must not register two RNGs with the same name. */
	err = -EEXIST;
	list_for_each_entry(tmp, &rng_list, list) {
		if (strcmp(tmp->name, rng->name) == 0)
			goto out_unlock;
	}

	old_rng = current_rng;
	if (!old_rng) {
		err = hwrng_init(rng);
		if (err)
			goto out_unlock;
		set_current_rng(rng);
	}
	err = 0;
	if (!old_rng) {
		err = register_miscdev();
		if (err) {
			drop_current_rng();
			goto out_unlock;
		}
	}
	INIT_LIST_HEAD(&rng->list);
	list_add_tail(&rng->list, &rng_list);

	if (old_rng && !rng->init) {
		/*
		 * Use a new device's input to add some randomness to
		 * the system.  If this rng device isn't going to be
		 * used right away, its init function hasn't been
		 * called yet; so only use the randomness from devices
		 * that don't need an init callback.
		 */
		add_early_randomness(rng);
	}

out_unlock:
	mutex_unlock(&rng_mutex);
out:
	return err;
}
EXPORT_SYMBOL_GPL(hwrng_register);

void hwrng_unregister(struct hwrng *rng)
{
	mutex_lock(&rng_mutex);

	list_del(&rng->list);
	if (current_rng == rng) {
		drop_current_rng();
		if (!list_empty(&rng_list)) {
			struct hwrng *tail;

			tail = list_entry(rng_list.prev, struct hwrng, list);

			if (hwrng_init(tail) == 0)
				set_current_rng(tail);
		}
	}

	if (list_empty(&rng_list)) {
		unregister_miscdev();
		if (hwrng_fill)
			kthread_stop(hwrng_fill);
	}

	mutex_unlock(&rng_mutex);
}
EXPORT_SYMBOL_GPL(hwrng_unregister);

static void __exit hwrng_exit(void)
{
	mutex_lock(&rng_mutex);
	BUG_ON(current_rng);
	kfree(rng_buffer);
	kfree(rng_fillbuf);
	mutex_unlock(&rng_mutex);
}

module_exit(hwrng_exit);

MODULE_DESCRIPTION("H/W Random Number Generator (RNG) driver");
MODULE_LICENSE("GPL");
