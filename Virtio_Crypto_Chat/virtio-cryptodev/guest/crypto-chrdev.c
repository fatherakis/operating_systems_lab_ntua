/*
 * crypto-chrdev.c
 *
 * Implementation of character devices
 * for virtio-cryptodev device 
 *
 * Vangelis Koukis <vkoukis@cslab.ece.ntua.gr>
 * Dimitris Siakavaras <jimsiak@cslab.ece.ntua.gr>
 * Stefanos Gerangelos <sgerag@cslab.ece.ntua.gr>
 *
 */
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>

#include "crypto.h"
#include "crypto-chrdev.h"
#include "debug.h"

#include "cryptodev.h"

/*
 * Global data
 */
struct cdev crypto_chrdev_cdev;

/**
 * Given the minor number of the inode return the crypto device 
 * that owns that number.
 **/
static struct crypto_device *get_crypto_dev_by_minor(unsigned int minor)
{
	struct crypto_device *crdev;
	unsigned long flags;

	debug("Entering");

	spin_lock_irqsave(&crdrvdata.lock, flags);
	list_for_each_entry(crdev, &crdrvdata.devs, list) {
		if (crdev->minor == minor)
			goto out;
	}
	crdev = NULL;

out:
	spin_unlock_irqrestore(&crdrvdata.lock, flags);

	debug("Leaving");
	return crdev;
}

/*************************************
 * Implementation of file operations
 * for the Crypto character device
 *************************************/

static int crypto_chrdev_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	int err;
	unsigned int len;	
	struct crypto_open_file *crof;
	struct crypto_device *crdev;
	unsigned int *syscall_type;
	//int *host_fd;
	/* OUR INITS: */
	unsigned int num_out=0, num_in=0;
	unsigned long flags;
	struct scatterlist syscall_type_sg, host_fd_sg, *sgs[2];

	debug("Entering");

	syscall_type = kzalloc(sizeof(*syscall_type), GFP_KERNEL);
	*syscall_type = VIRTIO_CRYPTODEV_SYSCALL_OPEN;
	//host_fd = kzalloc(sizeof(*host_fd), GFP_KERNEL);
	//*host_fd = -1;

	ret = -ENODEV;
	if ((ret = nonseekable_open(inode, filp)) < 0)
		goto fail;

	/* Associate this open file with the relevant crypto device. */
	crdev = get_crypto_dev_by_minor(iminor(inode));
	if (!crdev) {
		debug("Could not find crypto device with %u minor", iminor(inode));
		ret = -ENODEV;
		goto fail;
	}

	crof = kzalloc(sizeof(*crof), GFP_KERNEL);
	if (!crof) {
		ret = -ENOMEM;
		goto fail;
	}
	crof->crdev = crdev;
	crof->host_fd = -1;
	filp->private_data = crof;

	/**
	 * We need two sg lists, one for syscall_type and one to get the 
	 * file descriptor from the host.
	 **/		
	/* ?? */
	sg_init_one(&syscall_type_sg, &syscall_type, sizeof(syscall_type));
	sgs[num_out++] = &syscall_type_sg;
	sg_init_one(&host_fd_sg, &crof->host_fd, sizeof(int));
	sgs[num_out + num_in++] = &host_fd_sg;
	
	/**
	 * Wait for the host to process our data.
	 **/
	/* ?? */
	//https://elixir.bootlin.com/linux/latest/source/drivers/virtio/virtio_ring.c#L1822
	/**
	 * virtqueue_add_sgs - expose buffers to other end
	 * @_vq: the struct virtqueue we're talking about.
	 * @sgs: array of terminated scatterlists.
	 * @out_sgs: the number of scatterlists readable by other side
	 * @in_sgs: the number of scatterlists which are writable (after readable ones)
	 * @data: the token identifying the buffer.
	 * @gfp: how to do memory allocations (if necessary).
	 *
	 * Caller must ensure we don't call this with other virtqueue operations
	 * at the same time (except where noted).
	 *
	 * Returns zero or a negative error (ie. ENOSPC, ENOMEM, EIO).
 	*/
	spin_lock_irqsave(&crdev->lock,flags);
	err = virtqueue_add_sgs(crdev->vq, sgs, num_out, num_in, &syscall_type_sg, GFP_ATOMIC);
	virtqueue_kick(crdev->vq);
	while (virtqueue_get_buf(crdev->vq, &len) == NULL) /* do nothing */;
	spin_unlock_irqrestore(&crdev->lock,flags);

	/* If host failed to open() return -ENODEV. */
	/* ?? */
	if(crof->host_fd < 0){
		debug("Host failed to open");
		ret = -ENODEV;
	}
	

fail:
	debug("Leaving");
	return ret;
}

static int crypto_chrdev_release(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct crypto_open_file *crof = filp->private_data;
	struct crypto_device *crdev = crof->crdev;
	unsigned int *syscall_type;
	/* OUR INITS: */
    	int err;
	unsigned int num_out=0, num_in=0;
	unsigned int len;
    	unsigned long flags;
	struct scatterlist syscall_type_sg, host_fd_sg, *sgs[2];
	
	debug("Entering");

	syscall_type = kzalloc(sizeof(*syscall_type), GFP_KERNEL);
	*syscall_type = VIRTIO_CRYPTODEV_SYSCALL_CLOSE;

	/**
	 * Send data to the host.
	 **/
	/* ?? */
	/*It's offered to use scatterlist structures to transfer plaintext to block cipher function. 
	 * Scatterlist handle to the plaintext by storing location of plaintext on the memmory page.*/
	sg_init_one(&syscall_type_sg, syscall_type, sizeof(*syscall_type));
	sgs[num_out++] = &syscall_type_sg;
	sg_init_one(&host_fd_sg, &crof->host_fd, sizeof(int));
	sgs[num_out++] = &host_fd_sg;

	/**
	 * Wait for the host to process our data.
	 **/
	/* ?? */
	/* same as before */
	spin_lock_irqsave(&crdev->lock,flags);
	err = virtqueue_add_sgs(crdev->vq, sgs, num_out, num_in, &syscall_type_sg, GFP_ATOMIC);
	virtqueue_kick(crdev->vq);
	while (virtqueue_get_buf(crdev->vq, &len) == NULL) /* do nothing */;
	spin_unlock_irqrestore(&crdev->lock,flags);


	kfree(crof);
	debug("Leaving");
	return ret;
}

static long crypto_chrdev_ioctl(struct file *filp, unsigned int cmd, 
                                unsigned long arg)
{
	long ret = 0;
	int err;
	struct crypto_open_file *crof = filp->private_data;
	struct crypto_device *crdev = crof->crdev;
	struct virtqueue *vq = crdev->vq;
	//struct scatterlist syscall_type_sg, output_msg_sg, input_msg_sg, *sgs[3];
	unsigned int num_out, num_in, len;
#define MSG_LEN 100
	//unsigned char *output_msg, *input_msg;
	unsigned int *syscall_type;
	/* OUR INITS: */
        struct scatterlist syscall_type_sg, host_fd_sg, ioctl_cmd_sg,
			sess_key_sg, sess_op_sg, sess_id_sg, crypt_op_sg,
			crypt_src_sg, crypt_iv_sg, crypt_dst_sg, host_ret_val_sg, *sgs[11];
    	unsigned int *ioctl_cmd;
	int data_length = 0;
    	int *host_fd, *host_ret_val;
    	struct session_op *sess;
    	struct crypt_op *crypt;
	struct crypt_op *returning_struct;
    	unsigned char *sess_key=NULL, *crypt_src=NULL, *crypt_iv=NULL, *crypt_dst=NULL;
    	__u32 *session_id=NULL; //Check Cryptodev Header file. u32 is 32 bit unsigned int (used for session id)
    	unsigned long flags;



	debug("Entering");

	/**
	 * Allocate all data that will be sent to the host.
	 **/
	//output_msg = kzalloc(MSG_LEN, GFP_KERNEL);
	//input_msg = kzalloc(MSG_LEN, GFP_KERNEL);
	syscall_type = kzalloc(sizeof(*syscall_type), GFP_KERNEL);
	*syscall_type = VIRTIO_CRYPTODEV_SYSCALL_IOCTL;	
	/* OUR ALLOCS: */
 	host_ret_val = kzalloc(sizeof(*host_ret_val), GFP_KERNEL);
	host_fd = kzalloc(sizeof(*host_fd),GFP_KERNEL);	*host_fd = crof->host_fd;
	ioctl_cmd = kzalloc(sizeof(*ioctl_cmd), GFP_KERNEL);
	sess = kzalloc(sizeof(struct session_op), GFP_KERNEL);
	crypt = kzalloc(sizeof(struct crypt_op), GFP_KERNEL);

	num_out = 0;
	num_in = 0;

	/**
	 *  These are common to all ioctl commands.
	 **/
	sg_init_one(&syscall_type_sg, syscall_type, sizeof(*syscall_type));
	sgs[num_out++] = &syscall_type_sg;
	/* ?? */
	sg_init_one(&host_fd_sg, host_fd, sizeof(*host_fd));
	sgs[num_out++] = &host_fd_sg;
	*ioctl_cmd = cmd;
	sg_init_one(&ioctl_cmd_sg, ioctl_cmd, sizeof(*ioctl_cmd));
        sgs[num_out++] = &ioctl_cmd_sg;
	debug("cmd is '%u'",cmd);

	/**
	 *  Add all the cmd specific sg lists.
	 **/
	switch (cmd) {
	case CIOCGSESSION:
		debug("CIOCGSESSION");
		
		/*
		 * Calling ioctl to start the session happens as ioctl(cfd,CIOCGSESSION,&sess) (we changed it from v2 since we need to access the whole struct;
		 * therefore filp is our cfd and cmd is our command but we need to get the session from arg
	 	 * since the arg comes from userspace and gives the struct we need to use copy_from_user to get the key and the rest of the data
		 */
		
		if (copy_from_user(sess, (struct session_op*) arg, sizeof(struct session_op)))			{return -EFAULT;} 	 
		
		sess_key = kzalloc(sess->keylen, GFP_KERNEL);
		//sess_key = sess->key;
		// StackOverlow suggest copying the struct data from userspace with arg or sess with copy_from_user so we trust them.
		if (copy_from_user(sess_key, sess->key, sess->keylen))						{return -EFAULT;}
		
		sg_init_one(&sess_key_sg, sess_key, sess->keylen);
		sgs[num_out++] = &sess_key_sg;
		sg_init_one(&sess_op_sg, sess, sizeof(struct session_op));
		sgs[num_out + num_in++] = &sess_op_sg;
		
		sg_init_one(&host_ret_val_sg, host_ret_val, sizeof(*host_ret_val));
		sgs[num_out + num_in++] = &host_ret_val_sg;

		/* THIS IS GIVEN CODE JUST TO BE ABLE TO COMPILE
		memcpy(output_msg, "Hello HOST from ioctl CIOCGSESSION.", 36);
		input_msg[0] = '\0';
		sg_init_one(&output_msg_sg, output_msg, MSG_LEN);
		sgs[num_out++] = &output_msg_sg;
		sg_init_one(&input_msg_sg, input_msg, MSG_LEN);
		sgs[num_out + num_in++] = &input_msg_sg;
		*/
		break;

	case CIOCFSESSION:
		debug("CIOCFSESSION");
		
		/*
		 * In this case arg is from userspace and sends just the session id that we will need to close
		 * so in the same way as before we copy from user 
		 */	
		session_id = kzalloc(sizeof(*session_id), GFP_KERNEL);
		if (copy_from_user(session_id, (__u32*) arg, sizeof(*session_id)))		{return -EFAULT;}
		sg_init_one(&sess_id_sg, session_id, sizeof(*session_id));
		sgs[num_out++] = &sess_id_sg;
		
		sg_init_one(&host_ret_val_sg, host_ret_val, sizeof(*host_ret_val));
		sgs[num_out + num_in++] = &host_ret_val_sg;


		/* THIS IS GIVEN CODE JUST TO BE ABLE TO COMPILE
		memcpy(output_msg, "Hello HOST from ioctl CIOCGSESSION.", 36);
		memcpy(output_msg, "Hello HOST from ioctl CIOCFSESSION.", 36);
		input_msg[0] = '\0';
		sg_init_one(&output_msg_sg, output_msg, MSG_LEN);
		sgs[num_out++] = &output_msg_sg;
		sg_init_one(&input_msg_sg, input_msg, MSG_LEN);
		sgs[num_out + num_in++] = &input_msg_sg;
		*/
		break;

	case CIOCCRYPT:
		debug("CIOCCRYPT");
		
		/*
		 * In this case we call ioctl(cfd,CIOCRYPT,crypt) where crypt is the crypt operation struct
		 * with that in mind we work the same way as with sess struct
		 */

		if (copy_from_user(crypt, (struct crypt_op*) arg, sizeof(struct crypt_op)))		{return -EFAULT;}	
		data_length = crypt->len;
		crypt_dst = kzalloc(data_length, GFP_KERNEL);
		crypt_iv = kzalloc(VIRTIO_CRYPTODEV_BLOCK_SIZE, GFP_KERNEL);
		crypt_src = kzalloc(data_length, GFP_KERNEL);
		sg_init_one(&crypt_op_sg, crypt, sizeof(struct crypt_op));
		sgs[num_out++] = &crypt_op_sg;
		//crypt_src = crypt->src;
		// StackOverlow suggest copying the struct data from userspace wth arg or crypt with copy_from_user so we trust them.
		if(copy_from_user(crypt_src, crypt->src, data_length))					{return -EFAULT;}
		sg_init_one(&crypt_src_sg, crypt_src, data_length);
		sgs[num_out++] = &crypt_src_sg;
		debug("This is src %02x",*crypt_src);
		if(copy_from_user(crypt_iv, crypt->iv, VIRTIO_CRYPTODEV_BLOCK_SIZE))					{return -EFAULT;}
		sg_init_one(&crypt_iv_sg, crypt_iv, VIRTIO_CRYPTODEV_BLOCK_SIZE);
		sgs[num_out++] = &crypt_iv_sg;
		debug("This is iv %02x",*crypt_dst);
		sg_init_one(&crypt_dst_sg, crypt_dst, data_length);
		sgs[num_out + num_in++] = &crypt_dst_sg;
		sg_init_one(&host_ret_val_sg, host_ret_val,sizeof(*host_ret_val));
		sgs[num_out + num_in++] = &host_ret_val_sg;

		/* THIS IS GIVEN CODE JUST TO BE ABLE TO COMPILE
		memcpy(output_msg, "Hello HOST from ioctl CIOCCRYPT.", 33);
		input_msg[0] = '\0';
		sg_init_one(&output_msg_sg, output_msg, MSG_LEN);
		sgs[num_out++] = &output_msg_sg;
		sg_init_one(&input_msg_sg, input_msg, MSG_LEN);
		sgs[num_out + num_in++] = &input_msg_sg;
		*/
		break;

	default:
		debug("Unsupported ioctl command");

		break;
	}


	/**
	 * Wait for the host to process our data.
	 **/
	/* ?? */
	/* ?? Lock ?? */
	spin_lock_irqsave(&crdev->lock,flags);	//<---
	err = virtqueue_add_sgs(vq, sgs, num_out, num_in,
	                        &syscall_type_sg, GFP_ATOMIC);
	virtqueue_kick(vq);
	while (virtqueue_get_buf(vq, &len) == NULL)
		/* do nothing */;
	spin_unlock_irqrestore(&crdev->lock,flags);	//<---

	//debug("We said: '%s'", output_msg);
	//debug("Host answered: '%s'", input_msg);
	
	switch(cmd){
		case CIOCGSESSION:
			//lets send back the session struct
			if(copy_to_user((struct session_op*)arg, sess, sizeof(struct session_op))) 		{return -EFAULT;}	
			break;
		case CIOCFSESSION:
			//release the session
			kfree(session_id);
			break;
		case CIOCCRYPT:
			// we reassign arg as a crypt struct so we can send back our data to crypt->dst  then free our internal crypt structs
			returning_struct = (struct crypt_op*) arg;
			if(copy_to_user(returning_struct->dst, crypt_dst,data_length))				{return -EFAULT;}	
			kfree(crypt_dst);
			kfree(crypt_iv);
			kfree(crypt_src);
			break;
		default:
			debug("not supported cmd");
			break;
	}

	//kfree(output_msg);
	//kfree(input_msg);
	kfree(syscall_type);
	/* OUR KFREE: */
        kfree(host_fd);
        kfree(host_ret_val);
        kfree(ioctl_cmd);

	debug("Leaving");

	return ret;
}

static ssize_t crypto_chrdev_read(struct file *filp, char __user *usrbuf, 
                                  size_t cnt, loff_t *f_pos)
{
	debug("Entering");
	debug("Leaving");
	return -EINVAL;
}

static struct file_operations crypto_chrdev_fops = 
{
	.owner          = THIS_MODULE,
	.open           = crypto_chrdev_open,
	.release        = crypto_chrdev_release,
	.read           = crypto_chrdev_read,
	.unlocked_ioctl = crypto_chrdev_ioctl,
};

int crypto_chrdev_init(void)
{
	int ret;
	dev_t dev_no;
	unsigned int crypto_minor_cnt = CRYPTO_NR_DEVICES;
	
	debug("Initializing character device...");
	cdev_init(&crypto_chrdev_cdev, &crypto_chrdev_fops);
	crypto_chrdev_cdev.owner = THIS_MODULE;
	
	dev_no = MKDEV(CRYPTO_CHRDEV_MAJOR, 0);
	ret = register_chrdev_region(dev_no, crypto_minor_cnt, "crypto_devs");
	if (ret < 0) {
		debug("failed to register region, ret = %d", ret);
		goto out;
	}
	ret = cdev_add(&crypto_chrdev_cdev, dev_no, crypto_minor_cnt);
	if (ret < 0) {
		debug("failed to add character device");
		goto out_with_chrdev_region;
	}

	debug("Completed successfully");
	return 0;

out_with_chrdev_region:
	unregister_chrdev_region(dev_no, crypto_minor_cnt);
out:
	return ret;
}

void crypto_chrdev_destroy(void)
{
	dev_t dev_no;
	unsigned int crypto_minor_cnt = CRYPTO_NR_DEVICES;

	debug("entering");
	dev_no = MKDEV(CRYPTO_CHRDEV_MAJOR, 0);
	cdev_del(&crypto_chrdev_cdev);
	unregister_chrdev_region(dev_no, crypto_minor_cnt);
	debug("leaving");
}
