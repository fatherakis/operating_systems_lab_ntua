/*
 * lunix-chrdev.c
 *
 * Implementation of character devices
 * for Lunix:TNG
 *
 * < Xristos Nikolopoulos >
 * < Alexandros Paterakis >
 *
 */

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mmzone.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>

#include "lunix.h"
#include "lunix-chrdev.h"
#include "lunix-lookup.h"

/*
 * Global data
 */
struct cdev lunix_chrdev_cdev;

/*
 * Just a quick [unlocked] check to see if the cached
 * chrdev state needs to be updated from sensor measurements.
 */
static int lunix_chrdev_state_needs_refresh(struct lunix_chrdev_state_struct *state){
	struct lunix_sensor_struct *sensor;

	WARN_ON ( !(sensor = state->sensor));
	if(sensor->msr_data[state->type]->last_update > state->buf_timestamp) {
    //debug("@ NEEDS_REFRESH: returning 1, wake up!\n");
    return 1; // => wake up
  }
	/* The following return is bogus, just for the stub to compile */
	debug("@ N_R: return 0\n");
	return 0;// => no new data, keep sleeping
}

/*
 * Updates the cached state of a character device
 * based on sensor data. Must be called with the
 * character device state lock held.
 */

static int lunix_chrdev_state_update(struct lunix_chrdev_state_struct *state){
	struct lunix_sensor_struct *sensor;
	uint16_t values;
	long looked;
  unsigned long flags;
	int akeraio_meros, dekadiko_meros;
	long* lookup[N_LUNIX_MSR];

	lookup[BATT]= lookup_voltage;
	lookup[TEMP]= lookup_temperature;
	lookup[LIGHT]= lookup_light;

	debug("chrdev_state_update:Entering\n");

	if(!lunix_chrdev_state_needs_refresh(state)) { debug("@ lunix-chrdev-state_update: ABOUT TO RETURN -EGAIN\n"); return -EAGAIN;}
	/*
	 * Grab the raw data quickly, hold the
	 * spinlock for as little as possible.
	 */
	/* ? */
	sensor = state->sensor;
	/* Why use spinlocks? See LDD3, p. 119 */
	spin_lock_irqsave(&sensor->lock,flags); //bh? irqsave? irq?
	values = sensor->msr_data[state->type]->values[0];
	state->buf_timestamp = sensor->msr_data[state->type]->last_update;
	spin_unlock_irqrestore(&sensor->lock,flags);

	/*
	 * Now we can take our time to format them,
	 * holding only the private state semaphore
	 */

  //state locks are handled by read
	looked = lookup[state->type][values];
	akeraio_meros = looked / 1000;
	dekadiko_meros = looked % 1000;
	sprintf(state->buf_data, "%d.%d\n", akeraio_meros, abs(dekadiko_meros));
	state->buf_lim = strlen(state->buf_data);
	//up(&state->lock);
	debug("chrdev_state_update:leaving with data %d.%d\n",akeraio_meros,abs(dekadiko_meros));
	return 0;
}

/*************************************
 * Implementation of file operations
 * for the Lunix character device
 *************************************/

static int lunix_chrdev_open(struct inode *inode, struct file *filp){
	/* Declarations */
	int ret, minor, sensor_no, type;
	struct lunix_chrdev_state_struct *pd;
	debug("Open:entering\n");
	ret = -ENODEV;
	if ((ret = nonseekable_open(inode, filp)) < 0)
		goto out;

	/*
	 * Associate this open file with the relevant sensor based on
	 * the minor number of the device node [/dev/sensor<NO>-<TYPE>]
	 */

	minor = MINOR(inode->i_rdev);
	sensor_no = minor >> 3;
	type = minor & 0b111;//batt, light, temp

	//check if type is over N_LUNIX_MSR
	if(type >= N_LUNIX_MSR){
    debug("type is wrong\n");
    ret = -ENODEV;
    goto out;
  }

	/* Allocate a new Lunix character device private state structure */
  //pd = (struct lunix_chrdev_state_struct*) filp->private_data; ???
	pd = kmalloc(sizeof(struct lunix_chrdev_state_struct), GFP_KERNEL);
	filp->private_data = pd;
	if (!(pd)){debug("fail at malloc\n"); ret = -ENOMEM; goto out;}
  //EFAULT is for bad address and ENOMEM is for no mem left

	pd->type = type;
	pd->sensor = &lunix_sensors[sensor_no];
  //lunix_sensors is a table with structs for all the sensors and used as lunix_sensors[s_no]
	pd->buf_lim = 0;
  //buf_data it can stay unallocated until a bug shows up
	pd->buf_timestamp = 0;
	sema_init(&pd->lock, 1);
out:
	debug("Open:leaving, with ret = %d\n", ret);
	return ret;
}

static int lunix_chrdev_release(struct inode *inode, struct file *filp){
	if (filp->private_data) kfree(filp->private_data);
	//MOD_DEC_USE_COUNT;
	return 0;
}

static long lunix_chrdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
	/* Why? */
	return -EINVAL;
}

static ssize_t lunix_chrdev_read(struct file *filp, char __user *usrbuf, size_t cnt, loff_t *f_pos){
	ssize_t ret;

	struct lunix_sensor_struct *sensor;
	struct lunix_chrdev_state_struct *state;

	state = filp->private_data;
	WARN_ON(!state);

	sensor = state->sensor;
	WARN_ON(!sensor);

	/* Lock? */
	debug("@ lunix-chrdev-read: trying to lock\n");
	if (down_interruptible(&state->lock)) return -ERESTARTSYS;
	/*
	 * If the cached character device state needs to be
	 * updated by actual sensor data (i.e. we need to report
	 * on a "fresh" measurement, do so
	 */

	if (*f_pos == 0) {
		debug("@ lunix-chrdev-read: inside f_pos==0, entering while state_update\n");
		while (lunix_chrdev_state_update(state) == -EAGAIN) {
			up(&state->lock); //don't keep the semaphore, you might go to sleep
			/* The process needs to sleep */
			/* See LDD3, page 153 for a hint */
			ret = wait_event_interruptible(sensor->wq, lunix_chrdev_state_needs_refresh(state)); //sleeps here
			if (ret == -ERESTARTSYS) goto out;
			// sleep
			ret = down_interruptible(&state->lock);//goodmorning here is a semaphore.
			if (ret == -ERESTARTSYS) goto out;
		}
	}
	debug("@ lunix-chrdev-read: outta if-while\n");

	/* End of file */

	/* Determine the number of cached bytes to copy to userspace */

	//cnt is the num of bytes the user requests to read which should read up to the reamining info (thx pdf pg 82)
  if (*f_pos + cnt > state->buf_lim) cnt = state->buf_lim - *f_pos;

	//copy_to_user copies a block of data from kernelspace to userspace for n bytes copy_to_user(dst,src,n)
	if(copy_to_user(usrbuf, state->buf_data + *f_pos, cnt)) {
		ret = -EFAULT;
		goto out;
	}
	debug("lunix-chrdev-read: read is going good");
	*f_pos += cnt;
	ret = cnt;

	/* Auto-rewind on EOF mode? */
        if (*f_pos >= state->buf_lim){*f_pos = 0; goto out;}
out:
	/* Unlock? */
	//https://0xax.gitbooks.io/linux-insides/content/SyncPrim/linux-sync-3.html
	debug("@ lunix-chrdev-read: unlocking\n");
	up(&state->lock); //Release sem
	return ret;
}

static int lunix_chrdev_mmap(struct file *filp, struct vm_area_struct *vma){
	return -EINVAL;
}

static struct file_operations lunix_chrdev_fops ={
        .owner          = THIS_MODULE,
	.open           = lunix_chrdev_open,
	.release        = lunix_chrdev_release,
	.read           = lunix_chrdev_read,
	.unlocked_ioctl = lunix_chrdev_ioctl,
	.mmap           = lunix_chrdev_mmap
};

int lunix_chrdev_init(void){
	/*
	 * Register the character device with the kernel, asking for
	 * a range of minor numbers (number of sensors * 8 measurements / sensor)
	 * beginning with LINUX_CHRDEV_MAJOR:0
	 */
	int ret;
	dev_t dev_no;
	unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;

	debug("initializing character device\n");
	cdev_init(&lunix_chrdev_cdev, &lunix_chrdev_fops);
	lunix_chrdev_cdev.owner = THIS_MODULE;

	dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
	/* register_chrdev_region? */
  //resister a range of device numbers register_chrdev_region(first,count,name);
	ret = register_chrdev_region(dev_no, lunix_minor_cnt, "lunix");
	if (ret < 0) {
		debug("failed to register region, ret = %d\n", ret);
		goto out;
	}
	/* cdev_add? */
  //add a character device to the system cdev_add(cdev_stuct,device_no,#ofminor)
	ret = cdev_add(&lunix_chrdev_cdev, dev_no, lunix_minor_cnt);
	if (ret < 0) {
		debug("failed to add character device\n");
		goto out_with_chrdev_region;
	}
	debug("completed successfully\n");
	return 0;

out_with_chrdev_region:
	unregister_chrdev_region(dev_no, lunix_minor_cnt);
out:
	return ret;
}

void lunix_chrdev_destroy(void){
	dev_t dev_no;
	unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;

	debug("Destroy:entering\n");
	dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
	cdev_del(&lunix_chrdev_cdev);
	unregister_chrdev_region(dev_no, lunix_minor_cnt);
	debug("Destroy:leaving\n");
}
