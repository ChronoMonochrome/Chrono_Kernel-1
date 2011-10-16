/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Martin Hovang <martin.xm.hovang@stericsson.com>
 * Author: Joakim Bech <joakim.xx.bech@stericsson.com>
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/tee.h>
#include <linux/slab.h>

#define TEED_NAME "tee"

#define TEED_STATE_OPEN_DEV 0
#define TEED_STATE_OPEN_SESSION 1

static struct mutex sync;

static int tee_open(struct inode *inode, struct file *file);
static int tee_release(struct inode *inode, struct file *file);
static int tee_read(struct file *filp, char __user *buffer,
		    size_t length, loff_t *offset);
static int tee_write(struct file *filp, const char __user *buffer,
		     size_t length, loff_t *offset);

static inline void set_emsg(struct tee_session *ts, u32 msg)
{
	ts->err = msg;
	ts->origin = TEED_ORIGIN_DRIVER;
}

static void reset_session(struct tee_session *ts)
{
	ts->state = TEED_STATE_OPEN_DEV;
	ts->err = TEED_SUCCESS;
	ts->origin = TEED_ORIGIN_DRIVER;
	ts->id = 0;
	ts->ta = NULL;
	ts->uuid = NULL;
	ts->cmd = 0;
	ts->driver_cmd = TEED_OPEN_SESSION;
	ts->ta_size = 0;
	ts->op = NULL;
}

static int copy_ta(struct tee_session *ts,
		   struct tee_session *ku_buffer)
{
	ts->ta = kmalloc(ku_buffer->ta_size, GFP_KERNEL);
	if (ts->ta == NULL) {
		pr_err("[%s] error, out of memory (ta)\n",
		       __func__);
		set_emsg(ts, TEED_ERROR_OUT_OF_MEMORY);
		return -ENOMEM;
	}

	ts->ta_size = ku_buffer->ta_size;

	memcpy(ts->ta, ku_buffer->ta, ku_buffer->ta_size);
	return 0;
}

static int copy_uuid(struct tee_session *ts,
		     struct tee_session *ku_buffer)
{
	ts->uuid = kmalloc(sizeof(struct tee_uuid), GFP_KERNEL);

	if (ts->uuid == NULL) {
		pr_err("[%s] error, out of memory (uuid)\n",
		       __func__);
		set_emsg(ts, TEED_ERROR_OUT_OF_MEMORY);
		return -ENOMEM;
	}

	memcpy(ts->uuid, ku_buffer->uuid, sizeof(struct tee_uuid));

	return 0;
}

static inline void free_operation(struct tee_session *ts)
{
	int i;

	for (i = 0; i < 4; ++i) {
		kfree(ts->op->shm[i].buffer);
		ts->op->shm[i].buffer = NULL;
	}

	kfree(ts->op);
	ts->op = NULL;
}

static inline void memrefs_phys_to_virt(struct tee_session *ts)
{
	int i;

	for (i = 0; i < 4; ++i) {
		if (ts->op->flags & (1 << i)) {
			ts->op->shm[i].buffer =
				phys_to_virt((unsigned long)
					     ts->op->shm[i].buffer);
		}
	}
}

static int copy_memref_to_user(struct tee_operation *op,
			       struct tee_operation *ubuf_op,
			       int memref)
{
	unsigned long bytes_left;

	bytes_left = copy_to_user(ubuf_op->shm[memref].buffer,
				  op->shm[memref].buffer,
				  op->shm[memref].size);

	if (bytes_left != 0) {
		pr_err("[%s] Failed to copy result to user space (%lu "
		       "bytes left of buffer).\n", __func__, bytes_left);
		return bytes_left;
	}

	bytes_left = put_user(op->shm[memref].size, &ubuf_op->shm[memref].size);

	if (bytes_left != 0) {
		pr_err("[%s] Failed to copy result to user space (%lu "
		       "bytes left of size).\n", __func__, bytes_left);
		return -EINVAL;
	}

	bytes_left = put_user(op->shm[memref].flags,
			      &ubuf_op->shm[memref].flags);
	if (bytes_left != 0) {
		pr_err("[%s] Failed to copy result to user space (%lu "
		       "bytes left of flags).\n", __func__, bytes_left);
		return -EINVAL;
	}

	return 0;
}

static int copy_memref_to_kernel(struct tee_operation *op,
				 struct tee_operation *kbuf_op,
				 int memref)
{
	/* Buffer freed in invoke_command if this function fails */
	op->shm[memref].buffer = kmalloc(kbuf_op->shm[memref].size, GFP_KERNEL);

	if (!op->shm[memref].buffer) {
		pr_err("[%s] out of memory\n", __func__);
		return -ENOMEM;
	}

	/*
	 * Copy shared memory operations to a local kernel
	 * buffer if they are of type input.
	 */
	if (kbuf_op->shm[memref].flags & TEEC_MEM_INPUT) {
		memcpy(op->shm[memref].buffer,
		       kbuf_op->shm[memref].buffer,
		       kbuf_op->shm[memref].size);
	}

	op->shm[memref].size = kbuf_op->shm[memref].size;
	op->shm[memref].flags = kbuf_op->shm[memref].flags;

	/* Secure world expects physical addresses. */
	op->shm[memref].buffer = (void *)virt_to_phys(op->shm[memref].buffer);

	return 0;
}

static int open_tee_device(struct tee_session *ts,
			   struct tee_session *ku_buffer)
{
	int ret;

	if (ku_buffer->driver_cmd != TEED_OPEN_SESSION) {
		set_emsg(ts, TEED_ERROR_BAD_STATE);
		return -EINVAL;
	}

	if (ku_buffer->ta) {
		ret = copy_ta(ts, ku_buffer);
	} else if (ku_buffer->uuid) {
		ret = copy_uuid(ts, ku_buffer);
	} else {
		set_emsg(ts, TEED_ERROR_COMMUNICATION);
		return -EINVAL;
	}

	ts->id = 0;
	ts->state = TEED_STATE_OPEN_SESSION;
	return ret;
}

static int invoke_command(struct tee_session *ts,
			  struct tee_session *ku_buffer,
			  struct tee_session __user *u_buffer)
{
	int i;
	int ret = 0;
	struct tee_operation *kbuf_op =
		(struct tee_operation *)ku_buffer->op;

	ts->op = kmalloc(sizeof(struct tee_operation), GFP_KERNEL);

	if (!ts->op) {
		if (ts->op == NULL) {
			pr_err("[%s] error, out of memory "
			       "(op)\n", __func__);
			set_emsg(ts, TEED_ERROR_OUT_OF_MEMORY);
			ret = -ENOMEM;
			goto err;
		}
	}

	/* Copy memrefs to kernel space. */
	ts->op->flags = kbuf_op->flags;
	ts->cmd = ku_buffer->cmd;

	for (i = 0; i < 4; ++i) {
		/* We only want to copy memrefs in use. */
		if (kbuf_op->flags & (1 << i)) {
			ret = copy_memref_to_kernel(ts->op, kbuf_op, i);

			if (ret)
				goto err;
		} else {
			ts->op->shm[i].buffer = NULL;
			ts->op->shm[i].size = 0;
			ts->op->shm[i].flags = 0;
		}
	}

	/* To call secure world */
	if (call_sec_world(ts, TEED_INVOKE)) {
		ret = -EINVAL;
		goto err;
	}

	/*
	 * Convert physical addresses back to virtual address so the
	 * kernel can free the buffers when closing the session.
	 */
	memrefs_phys_to_virt(ts);

	for (i = 0; i < 4; ++i) {
		if ((kbuf_op->flags & (1 << i)) &&
		    (kbuf_op->shm[i].flags & TEEC_MEM_OUTPUT)) {
			struct tee_operation *ubuf_op =
				(struct tee_operation *)u_buffer->op;

			ret = copy_memref_to_user(ts->op, ubuf_op, i);
		}
	}
err:
	free_operation(ts);

	return ret;
}

static int tee_open(struct inode *inode, struct file *filp)
{
	struct tee_session *ts;

	filp->private_data = kmalloc(sizeof(struct tee_session),
				     GFP_KERNEL);

	if (filp->private_data == NULL)
		return -ENOMEM;

	ts = (struct tee_session *) (filp->private_data);

	reset_session(ts);

	return 0;
}

static int tee_release(struct inode *inode, struct file *filp)
{
	struct tee_session *ts;
	int i;

	ts = (struct tee_session *) (filp->private_data);

	if (ts == NULL)
			goto no_ts;

	if (ts->op) {
		for (i = 0; i < 4; ++i) {
			kfree(ts->op->shm[i].buffer);
			ts->op->shm[i].buffer = NULL;
		}
	}

	kfree(ts->op);
	ts->op = NULL;

	kfree(ts->ta);
	ts->ta = NULL;

no_ts:
	kfree(filp->private_data);
	filp->private_data = NULL;

	return 0;
}

/*
 * Called when a process, which already opened the dev file, attempts
 * to read from it. This function gets the current status of the session.
 */
static int tee_read(struct file *filp, char __user *buffer,
		    size_t length, loff_t *offset)
{
	struct tee_read buf;
	struct tee_session *ts;

	if (length != sizeof(struct tee_read)) {
		pr_err("[%s] error, incorrect input length\n",
		       __func__);
		return -EINVAL;
	}

	ts = (struct tee_session *) (filp->private_data);

	if (ts == NULL) {
		pr_err("[%s] error, private_data not "
		       "initialized\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&sync);

	buf.err = ts->err;
	buf.origin = ts->origin;

	mutex_unlock(&sync);

	if (copy_to_user(buffer, &buf, length)) {
		pr_err("[%s] error, copy_to_user failed!\n",
		       __func__);
		return -EINVAL;
	}

	return length;
}

/*
 * Called when a process writes to a dev file
 */
static int tee_write(struct file *filp, const char __user *buffer,
		     size_t length, loff_t *offset)
{
	struct tee_session ku_buffer;
	struct tee_session *ts;
	int ret = length;

	if (length != sizeof(struct tee_session)) {
		pr_err("[%s] error, incorrect input length\n",
		       __func__);
		return -EINVAL;
	}

	if (copy_from_user(&ku_buffer, buffer, length)) {
		pr_err("[%s] error, tee_session "
		       "copy_from_user failed\n", __func__);
		return -EINVAL;
	}

	ts = (struct tee_session *) (filp->private_data);

	if (ts == NULL) {
		pr_err("[%s] error, private_data not "
		       "initialized\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&sync);

	switch (ts->state) {
	case TEED_STATE_OPEN_DEV:
		ret = open_tee_device(ts, &ku_buffer);
		break;

	case TEED_STATE_OPEN_SESSION:
		switch (ku_buffer.driver_cmd) {
		case TEED_INVOKE:
			ret = invoke_command(ts, &ku_buffer,
					     (struct tee_session *)buffer);
			break;

		case TEED_CLOSE_SESSION:
			/* no caching implemented yet... */
			if (call_sec_world(ts, TEED_CLOSE_SESSION))
				ret = -EINVAL;

			kfree(ts->ta);
			ts->ta = NULL;

			reset_session(ts);
			break;

		default:
			set_emsg(ts, TEED_ERROR_BAD_PARAMETERS);
			ret = -EINVAL;
		}
		break;
	default:
		pr_err("[%s] unknown state\n", __func__);
		set_emsg(ts, TEED_ERROR_BAD_STATE);
		ret = -EINVAL;
	}

	/*
	 * We expect that ret has value zero when reaching the end here.
	 * If it has any other value some error must have occured.
	 */
	if (!ret)
		ret = length;
	else
		ret = -EINVAL;

	mutex_unlock(&sync);

	return ret;
}

int teec_initialize_context(const char *name, struct tee_context *context)
{
	return TEED_SUCCESS;
}
EXPORT_SYMBOL(teec_initialize_context);

int teec_finalize_context(struct tee_context *context)
{
	return TEED_SUCCESS;
}
EXPORT_SYMBOL(teec_finalize_context);

int teec_open_session(struct tee_context *context,
		      struct tee_session *session,
		      const struct tee_uuid *destination,
		      unsigned int connection_method,
		      void *connection_data, struct tee_operation *operation,
		      unsigned int *error_origin)
{
	int res = TEED_SUCCESS;

	if (session == NULL || destination == NULL) {
		pr_err("[%s] session or destination == NULL\n", __func__);
		if (error_origin != NULL)
			*error_origin = TEED_ORIGIN_DRIVER;
		res = TEED_ERROR_BAD_PARAMETERS;
		goto exit;
	}

	reset_session(session);

	/*
	 * Open a session towards an application already loaded inside
	 * the TEE
	 */
	session->uuid = kmalloc(sizeof(struct tee_uuid), GFP_KERNEL);

	if (session->uuid == NULL) {
		pr_err("[%s] error, out of memory (uuid)\n",
		       __func__);
		if (error_origin != NULL)
			*error_origin = TEED_ORIGIN_DRIVER;
		res = TEED_ERROR_OUT_OF_MEMORY;
		goto exit;
	}

	memcpy(session->uuid, destination, sizeof(struct tee_uuid));

	session->ta = NULL;
	session->id = 0;

exit:
	return res;
}
EXPORT_SYMBOL(teec_open_session);

int teec_close_session(struct tee_session *session)
{
	int res = TEED_SUCCESS;

	mutex_lock(&sync);

	if (session == NULL) {
		pr_err("[%s] error, session == NULL\n", __func__);
		res = TEED_ERROR_BAD_PARAMETERS;
		goto exit;
	}

	if (call_sec_world(session, TEED_CLOSE_SESSION)) {
		pr_err("[%s] error, call_sec_world failed\n", __func__);
		res = TEED_ERROR_GENERIC;
		goto exit;
	}

exit:
	if (session != NULL) {
		kfree(session->uuid);
		session->uuid = NULL;
	}

	mutex_unlock(&sync);
	return res;
}
EXPORT_SYMBOL(teec_close_session);

int teec_invoke_command(
	struct tee_session *session, unsigned int command_id,
	struct tee_operation *operation,
	unsigned int *error_origin)
{
	int res = TEED_SUCCESS;
	int i;

	mutex_lock(&sync);

	if (session == NULL || operation == NULL || error_origin == NULL) {
		pr_err("[%s] error, input parameters == NULL\n", __func__);
		if (error_origin != NULL)
			*error_origin = TEED_ORIGIN_DRIVER;
		res = TEED_ERROR_BAD_PARAMETERS;
		goto exit;
	}

	for (i = 0; i < 4; ++i) {
		/* We only want to translate memrefs in use. */
		if (operation->flags & (1 << i)) {
			operation->shm[i].buffer =
				(void *)virt_to_phys(
					operation->shm[i].buffer);
		}
	}
	session->op = operation;
	session->cmd = command_id;

	/*
	 * Call secure world
	 */
	if (call_sec_world(session, TEED_INVOKE)) {
		pr_err("[%s] error, call_sec_world failed\n", __func__);
		if (error_origin != NULL)
			*error_origin = TEED_ORIGIN_DRIVER;
		res = TEED_ERROR_GENERIC;
	}
	if (session->err != TEED_SUCCESS) {
		pr_err("[%s] error, call_sec_world failed\n", __func__);
		if (error_origin != NULL)
			*error_origin = session->origin;
		res = session->err;
	}

	memrefs_phys_to_virt(session);
	session->op = NULL;

exit:
	mutex_unlock(&sync);
	return res;
}
EXPORT_SYMBOL(teec_invoke_command);

int teec_allocate_shared_memory(struct tee_context *context,
				struct tee_sharedmemory *shared_memory)
{
	int res = TEED_SUCCESS;

	if (shared_memory == NULL) {
		res = TEED_ERROR_BAD_PARAMETERS;
		goto exit;
	}

	shared_memory->buffer = kmalloc(shared_memory->size,
					GFP_KERNEL);

	if (shared_memory->buffer == NULL) {
		res = TEED_ERROR_OUT_OF_MEMORY;
		goto exit;
	}

exit:
	return res;
}
EXPORT_SYMBOL(teec_allocate_shared_memory);

void teec_release_shared_memory(struct tee_sharedmemory *shared_memory)
{
	kfree(shared_memory->buffer);
}
EXPORT_SYMBOL(teec_release_shared_memory);

static const struct file_operations tee_fops = {
	.owner = THIS_MODULE,
	.read = tee_read,
	.write = tee_write,
	.open = tee_open,
	.release = tee_release,
};

static struct miscdevice tee_dev = {
	MISC_DYNAMIC_MINOR,
	TEED_NAME,
	&tee_fops
};

static int __init tee_init(void)
{
	int err = 0;

	err = misc_register(&tee_dev);

	if (err) {
		pr_err("[%s] error %d adding character device "
		       "TEE\n", __func__, err);
	}

	mutex_init(&sync);

	return err;
}

static void __exit tee_exit(void)
{
	misc_deregister(&tee_dev);
}

subsys_initcall(tee_init);
module_exit(tee_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Trusted Execution Enviroment driver");
