/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Deepak KARDA/ deepak.karda@stericsson.com for ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include "ste_audio_io_dev.h"

#define STR_DEBUG_ON		"debug on"
#define AUDIOIO_DEVNAME		"ab8500-codec"

static int ste_audio_io_open(struct inode *inode, struct file *filp);
static int ste_audio_io_release(struct inode *inode, struct file *filp);
static long ste_audio_io_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg);
static int ste_audio_io_cmd_parser(unsigned int cmd, unsigned long arg);
static ssize_t ste_audio_io_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *f_pos);


/**
 * @brief Check IOCTL type, command no and access direction
 * @ inode  value corresponding to the file descriptor
 * @file	 value corresponding to the file descriptor
 * @cmd	IOCTL command code
 * @arg	Command argument
 * @return 0 on success otherwise negative error code
 */
static long ste_audio_io_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	int retval = 0;
	int err = 0;

	/* Check type and command number */
	if (_IOC_TYPE(cmd) != AUDIOIO_IOC_MAGIC)
		return -ENOTTY;

	/* IOC_DIR is from the user perspective, while access_ok is
	 * from the kernel perspective; so they look reversed.
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg,
							_IOC_SIZE(cmd));
	if (err == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg,
							_IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	retval = ste_audio_io_cmd_parser(cmd, arg);

	return retval;
}
/**
 * @brief IOCTL call to read the value from AB8500 device
 * @cmd IOCTL command code
 * @arg  Command argument
 * @return 0 on success otherwise negative error code
 */

static int process_read_register_cmd(unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	union audioio_cmd_data_t cmd_data;
	struct audioio_data_t *audio_dev_data;

	audio_dev_data = (struct audioio_data_t *)&cmd_data;

	if (copy_from_user(audio_dev_data, (void __user *)arg,
						sizeof(struct audioio_data_t)))
		return -EFAULT;

	retval =  ste_audio_io_core_api_access_read(audio_dev_data);
	if (0 != retval)
		return retval;

	if (copy_to_user((void __user *)arg, audio_dev_data,
						sizeof(struct audioio_data_t)))
		return -EFAULT;
	return 0;
}
/**
 * @brief IOCTL call to write the given value to the AB8500 device
 * @cmd IOCTL command code
 * @arg  Command argument
 * @return 0 on success otherwise negative error code
 */

static int process_write_register_cmd(unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	union audioio_cmd_data_t cmd_data;
	struct audioio_data_t	*audio_dev_data;

	audio_dev_data = (struct audioio_data_t *)&cmd_data;

	if (copy_from_user(audio_dev_data, (void __user *)arg,
						sizeof(struct audioio_data_t)))
		return  -EFAULT;

	retval = ste_audio_io_core_api_access_write(audio_dev_data);

	return retval;
}
/**
 * @brief IOCTL call to control the power on/off of hardware components
 * @cmd IOCTL command code
 * @arg  Command argument
 * @return 0 on success otherwise negative error code
 */

static int process_pwr_ctrl_cmd(unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	union audioio_cmd_data_t cmd_data;
	struct audioio_pwr_ctrl_t *audio_pwr_ctrl;

	audio_pwr_ctrl = (struct audioio_pwr_ctrl_t *)&cmd_data;

	if (copy_from_user(audio_pwr_ctrl, (void __user *)arg,
					sizeof(struct audioio_pwr_ctrl_t)))
		return -EFAULT;

	retval = ste_audio_io_core_api_power_control_transducer(audio_pwr_ctrl);

	return retval;
}

static int process_pwr_sts_cmd(unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	union audioio_cmd_data_t cmd_data;
	struct audioio_pwr_ctrl_t *audio_pwr_sts;

	audio_pwr_sts = (struct audioio_pwr_ctrl_t *)&cmd_data;

	if (copy_from_user(audio_pwr_sts, (void __user *)arg,
					sizeof(struct audioio_pwr_ctrl_t)))
		return  -EFAULT;

	retval = ste_audio_io_core_api_power_status_transducer(audio_pwr_sts);
	if (0 != retval)
		return retval;

	if (copy_to_user((void __user *)arg, audio_pwr_sts,
					sizeof(struct audioio_pwr_ctrl_t)))
		return -EFAULT;

	return 0;
}

static int process_lp_ctrl_cmd(unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	union audioio_cmd_data_t cmd_data;
	struct audioio_loop_ctrl_t *audio_lp_ctrl;

	audio_lp_ctrl = (struct audioio_loop_ctrl_t *)&cmd_data;

	if (copy_from_user(audio_lp_ctrl, (void __user *)arg,
					sizeof(struct audioio_loop_ctrl_t)))
		return -EFAULT;

	retval =  ste_audio_io_core_api_loop_control(audio_lp_ctrl);

	return retval;
}

static int process_lp_sts_cmd(unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	union audioio_cmd_data_t cmd_data;
	struct audioio_loop_ctrl_t *audio_lp_sts;

	audio_lp_sts = (struct audioio_loop_ctrl_t *)&cmd_data;


	if (copy_from_user(audio_lp_sts, (void __user *)arg,
					sizeof(struct audioio_loop_ctrl_t)))
		return  -EFAULT;

	retval = ste_audio_io_core_api_loop_status(audio_lp_sts);
	if (0 != retval)
		return retval;

	if (copy_to_user((void __user *)arg, audio_lp_sts,
					sizeof(struct audioio_loop_ctrl_t)))
		return -EFAULT;
	return 0;
}

static int process_get_trnsdr_gain_capability_cmd(unsigned int cmd,
							unsigned long arg)
{
	int retval = 0;
	union audioio_cmd_data_t cmd_data;
	struct audioio_get_gain_t *audio_trnsdr_gain;

	audio_trnsdr_gain = (struct audioio_get_gain_t *)&cmd_data;

	if (copy_from_user(audio_trnsdr_gain, (void __user *)arg,
					sizeof(struct audioio_get_gain_t)))
		return -EFAULT;

	retval = ste_audio_io_core_api_get_transducer_gain_capability(
							audio_trnsdr_gain);
	if (0 != retval)
		return retval;

	if (copy_to_user((void __user *)arg, audio_trnsdr_gain,
					sizeof(struct audioio_get_gain_t)))
		return -EFAULT;
	return 0;
}

static int process_gain_cap_loop_cmd(unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	union audioio_cmd_data_t cmd_data;
	struct audioio_gain_loop_t *audio_gain_loop;

	audio_gain_loop = (struct audioio_gain_loop_t *)&cmd_data;

	if (copy_from_user(audio_gain_loop, (void __user *)arg,
					sizeof(struct audioio_gain_loop_t)))
		return  -EFAULT;

	retval = ste_audio_io_core_api_gain_capabilities_loop(audio_gain_loop);
	if (0 != retval)
		return retval;

	if (copy_to_user((void __user *)arg, audio_gain_loop,
					sizeof(struct audioio_gain_loop_t)))
		return -EFAULT;
	return 0;
}


static int process_support_loop_cmd(unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	union audioio_cmd_data_t cmd_data;
	struct audioio_support_loop_t *audio_spprt_loop;

	audio_spprt_loop = (struct audioio_support_loop_t *)&cmd_data;

	if (copy_from_user(audio_spprt_loop, (void __user *)arg,
					sizeof(struct audioio_support_loop_t)))
		return  -EFAULT;

	retval = ste_audio_io_core_api_supported_loops(audio_spprt_loop);
	if (0 != retval)
		return retval;

	if (copy_to_user((void __user *)arg, audio_spprt_loop,
					sizeof(struct audioio_support_loop_t)))
		return -EFAULT;
	return 0;
}


static int process_gain_desc_trnsdr_cmd(unsigned int cmd, unsigned long arg)

{
	int retval = 0;
	union audioio_cmd_data_t cmd_data;
	struct audioio_gain_desc_trnsdr_t *audio_gain_desc;

	audio_gain_desc = (struct audioio_gain_desc_trnsdr_t *)&cmd_data;

	if (copy_from_user(audio_gain_desc, (void __user *)arg,
				sizeof(struct audioio_gain_desc_trnsdr_t)))
		return  -EFAULT;

	retval = ste_audio_io_core_api_gain_descriptor_transducer(
					audio_gain_desc);
	if (0 != retval)
		return retval;

	if (copy_to_user((void __user *)arg, audio_gain_desc,
				sizeof(struct audioio_gain_desc_trnsdr_t)))
		return -EFAULT;
	return 0;
}


static int process_gain_ctrl_trnsdr_cmd(unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	union audioio_cmd_data_t cmd_data;
	struct audioio_gain_ctrl_trnsdr_t *audio_gain_ctrl;

	audio_gain_ctrl = (struct audioio_gain_ctrl_trnsdr_t *)&cmd_data;

	if (copy_from_user(audio_gain_ctrl, (void __user *)arg,
				sizeof(struct audioio_gain_ctrl_trnsdr_t)))
		return -EFAULT;

	retval = ste_audio_io_core_api_gain_control_transducer(
					audio_gain_ctrl);

	return retval;
}

static int process_gain_query_trnsdr_cmd(unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	union audioio_cmd_data_t cmd_data;
	struct audioio_gain_ctrl_trnsdr_t *audio_gain_query;

	audio_gain_query = (struct audioio_gain_ctrl_trnsdr_t *)&cmd_data;

	if (copy_from_user(audio_gain_query, (void __user *)arg,
				sizeof(struct audioio_gain_ctrl_trnsdr_t)))
		return -EFAULT;

	retval = ste_audio_io_core_api_gain_query_transducer(audio_gain_query);
	if (0 != retval)
		return retval;

	if (copy_to_user((void __user *)arg, audio_gain_query,
				sizeof(struct audioio_gain_ctrl_trnsdr_t)))
		return -EFAULT;
	return 0;
}

static int process_mute_ctrl_cmd(unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	union audioio_cmd_data_t cmd_data;
	struct audioio_mute_trnsdr_t *audio_mute_ctrl;

	audio_mute_ctrl = (struct audioio_mute_trnsdr_t *)&cmd_data;
	if (copy_from_user(audio_mute_ctrl , (void __user *)arg,
					sizeof(struct audioio_mute_trnsdr_t)))
		return -EFAULT;

	retval = ste_audio_io_core_api_mute_control_transducer(
						audio_mute_ctrl);

	return retval;
}

static int process_mute_sts_cmd(unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	union audioio_cmd_data_t cmd_data;
	struct audioio_mute_trnsdr_t *audio_mute_sts;

	audio_mute_sts = (struct audioio_mute_trnsdr_t *)&cmd_data;

	if (copy_from_user(audio_mute_sts, (void __user *)arg,
				sizeof(struct audioio_mute_trnsdr_t)))
		return -EFAULT;

	retval = ste_audio_io_core_api_mute_status_transducer(audio_mute_sts);
	if (0 != retval)
		return retval;

	if (copy_to_user((void __user *)arg, audio_mute_sts,
				sizeof(struct audioio_mute_trnsdr_t)))
		return -EFAULT;
	return 0;
}

static int process_fade_cmd(unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	union audioio_cmd_data_t cmd_data;
	struct audioio_fade_ctrl_t *audio_fade;
	audio_fade = (struct audioio_fade_ctrl_t *)&cmd_data;

	if (copy_from_user(audio_fade , (void __user *)arg,
					sizeof(struct audioio_fade_ctrl_t)))
		return -EFAULT;

	retval =  ste_audio_io_core_api_fading_control(audio_fade);

	return retval;
}

static int process_burst_ctrl_cmd(unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	union audioio_cmd_data_t cmd_data;
	struct audioio_burst_ctrl_t *audio_burst;

	audio_burst = (struct audioio_burst_ctrl_t  *)&cmd_data;
	if (copy_from_user(audio_burst , (void __user *)arg,
					sizeof(struct audioio_burst_ctrl_t)))
		return -EFAULT;

	retval = ste_audio_io_core_api_burstmode_control(audio_burst);

	return retval;

	return 0;
}

static int process_fsbitclk_ctrl_cmd(unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	union audioio_cmd_data_t cmd_data;
	struct audioio_fsbitclk_ctrl_t *audio_fsbitclk;

	audio_fsbitclk = (struct audioio_fsbitclk_ctrl_t  *)&cmd_data;

	if (copy_from_user(audio_fsbitclk , (void __user *)arg,
				sizeof(struct audioio_fsbitclk_ctrl_t)))
		return -EFAULT;

	retval = ste_audio_io_core_api_fsbitclk_control(audio_fsbitclk);

	return retval;

	return 0;

}

static int process_pseudoburst_ctrl_cmd(unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	union audioio_cmd_data_t cmd_data;
	struct audioio_pseudoburst_ctrl_t *audio_pseudoburst;

	audio_pseudoburst = (struct audioio_pseudoburst_ctrl_t  *)&cmd_data;

	if (copy_from_user(audio_pseudoburst , (void __user *)arg,
				sizeof(struct audioio_pseudoburst_ctrl_t)))
		return -EFAULT;

	retval = ste_audio_io_core_api_pseudoburst_control(audio_pseudoburst);

	return retval;

	return 0;

}
static int process_audiocodec_pwr_ctrl_cmd(unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	union audioio_cmd_data_t cmd_data;
	struct audioio_acodec_pwr_ctrl_t *audio_acodec_pwr_ctrl;
	audio_acodec_pwr_ctrl = (struct audioio_acodec_pwr_ctrl_t *)&cmd_data;
	if (copy_from_user(audio_acodec_pwr_ctrl, (void __user *)arg,
				sizeof(struct audioio_acodec_pwr_ctrl_t)))
		return -EFAULT;
	retval = ste_audio_io_core_api_acodec_power_control(
							audio_acodec_pwr_ctrl);
	return retval;
}

static int process_fir_coeffs_ctrl_cmd(unsigned int cmd, unsigned long arg)
{
	int retval;
	struct audioio_fir_coefficients_t *cmd_data;
	cmd_data = kmalloc(sizeof(struct audioio_fir_coefficients_t),
								GFP_KERNEL);
	if (!cmd_data)
		return -ENOMEM;
	if (copy_from_user(cmd_data, (void __user *)arg,
				sizeof(struct audioio_fir_coefficients_t))) {
		kfree(cmd_data);
		return -EFAULT;
	}
	retval = ste_audio_io_core_api_fir_coeffs_control(cmd_data);
	kfree(cmd_data);
	return retval;
}

static int process_clk_select_cmd(unsigned int cmd, unsigned long arg)
{
	int retval;
	struct audioio_clk_select_t *cmd_data;
	cmd_data = kmalloc(sizeof(struct audioio_clk_select_t),
								GFP_KERNEL);
	if (!cmd_data)
		return -ENOMEM;
	if (copy_from_user(cmd_data, (void __user *)arg,
				sizeof(struct audioio_clk_select_t))) {
		kfree(cmd_data);
		return -EFAULT;
	}
	retval = ste_audio_io_core_clk_select_control(cmd_data);
	kfree(cmd_data);
	return retval;
}

static int ste_audio_io_cmd_parser(unsigned int cmd, unsigned long arg)
{
	int retval = 0;

	switch (cmd) {
	case AUDIOIO_READ_REGISTER:
		retval = process_read_register_cmd(cmd, arg);
	break;

	case AUDIOIO_WRITE_REGISTER:
		retval = process_write_register_cmd(cmd, arg);
	break;

	case AUDIOIO_PWR_CTRL_TRNSDR:
		retval = process_pwr_ctrl_cmd(cmd, arg);
	break;

	case AUDIOIO_PWR_STS_TRNSDR:
		retval = process_pwr_sts_cmd(cmd, arg);
	break;

	case AUDIOIO_LOOP_CTRL:
		 retval = process_lp_ctrl_cmd(cmd, arg);
	break;

	case AUDIOIO_LOOP_STS:
		 retval = process_lp_sts_cmd(cmd, arg);
	break;

	case AUDIOIO_GET_TRNSDR_GAIN_CAPABILITY:
		retval = process_get_trnsdr_gain_capability_cmd(cmd, arg);
	break;

	case AUDIOIO_GAIN_CAP_LOOP:
		retval = process_gain_cap_loop_cmd(cmd, arg);
	break;

	case AUDIOIO_SUPPORT_LOOP:
		retval = process_support_loop_cmd(cmd, arg);
	break;

	case AUDIOIO_GAIN_DESC_TRNSDR:
		retval = process_gain_desc_trnsdr_cmd(cmd, arg);
	break;

	case AUDIOIO_GAIN_CTRL_TRNSDR:
		retval = process_gain_ctrl_trnsdr_cmd(cmd, arg);
	break;

	case AUDIOIO_GAIN_QUERY_TRNSDR:
		retval = process_gain_query_trnsdr_cmd(cmd, arg);
	break;

	case AUDIOIO_MUTE_CTRL_TRNSDR:
		retval = process_mute_ctrl_cmd(cmd, arg);
	break;

	case AUDIOIO_MUTE_STS_TRNSDR:
		retval = process_mute_sts_cmd(cmd, arg);
	break;

	case AUDIOIO_FADE_CTRL:
		retval = process_fade_cmd(cmd, arg);
	break;

	case AUDIOIO_BURST_CTRL:
		retval = process_burst_ctrl_cmd(cmd, arg);
	break;

	case AUDIOIO_FSBITCLK_CTRL:
		retval = process_fsbitclk_ctrl_cmd(cmd, arg);
	break;

	case AUDIOIO_PSEUDOBURST_CTRL:
		retval = process_pseudoburst_ctrl_cmd(cmd, arg);
	break;

	case AUDIOIO_AUDIOCODEC_PWR_CTRL:
		retval = process_audiocodec_pwr_ctrl_cmd(cmd, arg);
	break;

	case AUDIOIO_FIR_COEFFS_CTRL:
		retval = process_fir_coeffs_ctrl_cmd(cmd, arg);
		break;
	case AUDIOIO_CLK_SELECT_CTRL:
		retval = process_clk_select_cmd(cmd, arg);
		break;
	}
	return retval;
}

static int ste_audio_io_open(struct inode *inode, struct file *filp)
{
	if (!try_module_get(THIS_MODULE))
		return -ENODEV;
	return 0;
}

static int ste_audio_io_release(struct inode *inode, struct file *filp)
{
	module_put(THIS_MODULE);
	return 0;
}

static ssize_t ste_audio_io_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *f_pos)
{
	char *x = kmalloc(count, GFP_KERNEL);
	int debug_flag = 0;

	if (copy_from_user(x, buf, count))
		return  -EFAULT;

	if (count >= strlen(STR_DEBUG_ON)) {

		if (!strncmp(STR_DEBUG_ON, x, strlen(STR_DEBUG_ON)))
			debug_flag = 1;
		else
			debug_flag = 0;
	}

	ste_audio_io_core_debug(debug_flag);

	kfree(x);

	return count;
}

static const struct file_operations ste_audio_io_fops = {
	.owner   = THIS_MODULE,
	.unlocked_ioctl   = ste_audio_io_ioctl,
	.open    = ste_audio_io_open,
	.release = ste_audio_io_release,
	.write   = ste_audio_io_write,
};

/**
 * audio_io_misc_dev - Misc device config for audio_io
 */
static struct miscdevice audio_io_misc_dev = {
	MISC_DYNAMIC_MINOR,
	"audioio",
	&ste_audio_io_fops
};

/**
 * ste_audio_io_probe() - probe the device
 * @pdev:	pointer to the platform device structure
 *
 * This funtion is called after the driver is registered to platform
 * device framework. It does allocate the memory for the internal
 * data structure and intialized core APIs.
 */
static int ste_audio_io_drv_probe(struct platform_device *pdev)
{
	int error;

	ste_audio_io_device = pdev;

	dev_dbg(&ste_audio_io_device->dev, "ste_audio_io device probe\n");

	error = misc_register(&audio_io_misc_dev);
	if (error) {
		printk(KERN_WARNING "%s: registering misc device failed\n",
		       __func__);
		return error;
	}

	error = ste_audio_io_core_api_init_data(ste_audio_io_device);
	if (error < 0) {
		dev_err(&ste_audio_io_device->dev,
			"ste_audioio_core_api_init_data failed err = %d",
			error);
		goto ste_audio_io_misc_deregister;
	}
	return 0;

ste_audio_io_misc_deregister:
	misc_deregister(&audio_io_misc_dev);
	return error;
}

/**
 * ste_audio_io_remove() - Removes the device
 * @pdev:	pointer to the platform_device structure
 *
 * This function is called when this mnodule is removed using rmmod
 */
static int ste_audio_io_drv_remove(struct platform_device *pdev)
{
	ste_audio_io_core_api_free_data();
	misc_deregister(&audio_io_misc_dev);
	return 0;
}

/**
 * ste_audio_io_drv_suspend - suspend audio_io
 * @pdev:       platform data
 * @state:	power down level
 */
static int ste_audio_io_drv_suspend(struct platform_device *pdev,
				    pm_message_t state)
{
	if (ste_audio_io_core_is_ready_for_suspend())
		return 0;
	else
		return -EINVAL;
}

/**
 * ste_audio_io_drv_resume - put back audio_io in the normal state
 * @pdev:       platform data
 */
static int ste_audio_io_drv_resume(struct platform_device *pdev)
{
	return 0;
}

/**
 * struct audio_io_driver: audio_io platform structure
 * @probe:	The probe funtion to be called
 * @remove:	The remove funtion to be called
 * @resume:	The resume function to be called
 * @suspend:	The suspend function to be called
 * @driver:	The driver data
 */
static struct platform_driver ste_audio_io_driver = {
	.probe = ste_audio_io_drv_probe,
	.remove = ste_audio_io_drv_remove,
	.driver = {
		.name = AUDIOIO_DEVNAME,
		.owner = THIS_MODULE,
	},
	.suspend = ste_audio_io_drv_suspend,
	.resume = ste_audio_io_drv_resume,
};

/** Pointer to platform device needed to access abx500 core functions */
struct platform_device *ste_audio_io_device;

static int __init ste_audio_io_init(void)
{
	return  platform_driver_register(&ste_audio_io_driver);
}
module_init(ste_audio_io_init);

static void __exit ste_audio_io_exit(void)
{
	platform_driver_unregister(&ste_audio_io_driver);
}
module_exit(ste_audio_io_exit);

MODULE_AUTHOR("Deepak KARDA <deepak.karda@stericsson.com>");
MODULE_DESCRIPTION("STE_AUDIO_IO");
MODULE_LICENSE("GPL v2");
