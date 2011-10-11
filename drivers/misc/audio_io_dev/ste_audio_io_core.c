/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Deepak KARDA/ deepak.karda@stericsson.com for ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2.
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <mach/ste_audio_io_vibrator.h>
#include <mach/ste_audio.h>

#include "ste_audio_io_core.h"
#include "ste_audio_io_hwctrl_common.h"
#include "ste_audio_io_ab8500_reg_defs.h"

static struct audiocodec_context_t *ptr_audio_codec_cnxt;

static struct clk *clk_ptr_msp1;
static struct clk *clk_ptr_msp3;
static struct clk *clk_ptr_sysclk;

static struct regulator *regulator_vdmic;
static struct regulator *regulator_vaudio;
static struct regulator *regulator_vamic1;
static struct regulator *regulator_vamic2;
struct regulator *regulator_avsource;
static void ste_audio_io_init_transducer_cnxt(void);

bool ste_audio_io_core_is_ready_for_suspend()
{
	bool err = false;
	mutex_lock(&(ptr_audio_codec_cnxt->audio_io_mutex));
	if ((!ptr_audio_codec_cnxt->power_client) &&
			(!ptr_audio_codec_cnxt->audio_codec_powerup))
		err = true;
	mutex_unlock(&(ptr_audio_codec_cnxt->audio_io_mutex));
	return err;
}
int ste_audio_io_core_api_init_data(struct platform_device *pdev)
{
	struct ab8500 *ab8500 = dev_get_drvdata(pdev->dev.parent);
	struct ab8500_platform_data *pdata = dev_get_platdata(ab8500->dev);
	int status = 0;
	ptr_audio_codec_cnxt = kmalloc(sizeof(struct audiocodec_context_t),
						GFP_KERNEL);
	if (!ptr_audio_codec_cnxt)
		return -ENOMEM;

	memset(ptr_audio_codec_cnxt, 0, sizeof(*ptr_audio_codec_cnxt));
	ptr_audio_codec_cnxt->dev = &pdev->dev;
	mutex_init(&(ptr_audio_codec_cnxt->audio_io_mutex));

	if (pdata) {
		if (pdata->audio) {
			ptr_audio_codec_cnxt->gpio_altf_init =
				pdata->audio->ste_gpio_altf_init;
			ptr_audio_codec_cnxt->gpio_altf_exit =
				pdata->audio->ste_gpio_altf_exit;
		}
	}

	regulator_vdmic = regulator_get(NULL, "v-dmic");
	if (IS_ERR(regulator_vdmic)) {
		status = PTR_ERR(regulator_vdmic);
		dev_err(ptr_audio_codec_cnxt->dev,
				"Register error for v-dmic=%d", status);
		goto free_audio_codec_cnxt;
	}
	regulator_vamic1 = regulator_get(NULL, "v-amic1");
	if (IS_ERR(regulator_vamic1)) {
		status = PTR_ERR(regulator_vamic1);
		dev_err(ptr_audio_codec_cnxt->dev,
				"Register error for v-amic1=%d", status);
		goto free_regulator_vdmic;
	}
	regulator_vamic2 = regulator_get(NULL, "v-amic2");
	if (IS_ERR(regulator_vamic2)) {
		status = PTR_ERR(regulator_vamic2);
		dev_err(ptr_audio_codec_cnxt->dev,
				"Register error for v-amic2=%d", status);
		goto free_regulator_vdmic_vamic1;
	}
	regulator_vaudio = regulator_get(NULL, "v-audio");
	if (IS_ERR(regulator_vaudio)) {
		status = PTR_ERR(regulator_vaudio);
		dev_err(ptr_audio_codec_cnxt->dev,
				"Register error for v-audio=%d", status);
		goto free_regulator_vdmic_vamic1_vamic2;
	}
	regulator_avsource = regulator_get(ptr_audio_codec_cnxt->dev,
								"vcc-avswitch");
	if (IS_ERR(regulator_avsource)) {
		status = PTR_ERR(regulator_avsource);
		dev_err(ptr_audio_codec_cnxt->dev,
				"Register error for vcc-avswitch=%d", status);
		goto free_regulator_vdmic_vamic1_vamic2_vaudio;
	}

	ste_audio_io_init_transducer_cnxt();
	return 0;

free_regulator_vdmic_vamic1_vamic2_vaudio:
	regulator_put(regulator_vaudio);
free_regulator_vdmic_vamic1_vamic2:
	regulator_put(regulator_vamic2);
free_regulator_vdmic_vamic1:
	regulator_put(regulator_vamic1);
free_regulator_vdmic:
	regulator_put(regulator_vdmic);
free_audio_codec_cnxt:
	kfree(ptr_audio_codec_cnxt);
	return status;
}

static struct transducer_context_t transducer_headset = {
	.pwr_up_func = ste_audio_io_power_up_headset,
	.pwr_down_func = ste_audio_io_power_down_headset,
	.set_gain_func = ste_audio_io_set_headset_gain,
	.get_gain_func = ste_audio_io_get_headset_gain,
	.mute_func = ste_audio_io_mute_headset,
	.unmute_func = ste_audio_io_unmute_headset,
	.enable_fade_func = ste_audio_io_enable_fade_headset,
	.disable_fade_func = ste_audio_io_disable_fade_headset,
	.switch_to_burst_func = ste_audio_io_switch_to_burst_mode_headset,
	.switch_to_normal_func = ste_audio_io_switch_to_normal_mode_headset
};

static struct transducer_context_t transducer_earpiece = {
	.pwr_up_func = ste_audio_io_power_up_earpiece,
	.pwr_down_func = ste_audio_io_power_down_earpiece,
	.set_gain_func = ste_audio_io_set_earpiece_gain,
	.get_gain_func = ste_audio_io_get_earpiece_gain,
	.mute_func = ste_audio_io_mute_earpiece,
	.unmute_func = ste_audio_io_unmute_earpiece,
	.enable_fade_func = ste_audio_io_enable_fade_earpiece,
	.disable_fade_func = ste_audio_io_disable_fade_earpiece
};

static struct transducer_context_t transducer_ihf = {
	.pwr_up_func = ste_audio_io_power_up_ihf,
	.pwr_down_func = ste_audio_io_power_down_ihf,
	.set_gain_func = ste_audio_io_set_ihf_gain,
	.get_gain_func = ste_audio_io_get_ihf_gain,
	.mute_func = ste_audio_io_mute_ihf,
	.unmute_func = ste_audio_io_unmute_ihf,
	.enable_fade_func = ste_audio_io_enable_fade_ihf,
	.disable_fade_func = ste_audio_io_disable_fade_ihf

};

static struct transducer_context_t transducer_vibl = {
	.pwr_up_func = ste_audio_io_power_up_vibl,
	.pwr_down_func = ste_audio_io_power_down_vibl,
	.set_gain_func = ste_audio_io_set_vibl_gain,
	.get_gain_func = ste_audio_io_get_vibl_gain,
	.mute_func = ste_audio_io_mute_vibl,
	.unmute_func = ste_audio_io_unmute_vibl,
	.enable_fade_func = ste_audio_io_enable_fade_vibl,
	.disable_fade_func = ste_audio_io_disable_fade_vibl
};

static struct transducer_context_t transducer_vibr = {
	.pwr_up_func = ste_audio_io_power_up_vibr,
	.pwr_down_func = ste_audio_io_power_down_vibr,
	.set_gain_func = ste_audio_io_set_vibr_gain,
	.get_gain_func = ste_audio_io_get_vibr_gain,
	.mute_func = ste_audio_io_mute_vibr,
	.unmute_func = ste_audio_io_unmute_vibr,
	.enable_fade_func = ste_audio_io_enable_fade_vibr,
	.disable_fade_func = ste_audio_io_disable_fade_vibr
};

static struct transducer_context_t transducer_mic1a = {
	.pwr_up_func = ste_audio_io_power_up_mic1a,
	.pwr_down_func = ste_audio_io_power_down_mic1a,
	.set_gain_func = ste_audio_io_set_mic1a_gain,
	.get_gain_func = ste_audio_io_get_mic1a_gain,
	.mute_func = ste_audio_io_mute_mic1a,
	.unmute_func = ste_audio_io_unmute_mic1a,
	.enable_fade_func = ste_audio_io_enable_fade_mic1a,
	.disable_fade_func = ste_audio_io_disable_fade_mic1a
};

static struct transducer_context_t transducer_mic1b = {
	.pwr_up_func = ste_audio_io_power_up_mic1b,
	.pwr_down_func = ste_audio_io_power_down_mic1b,
	.set_gain_func = ste_audio_io_set_mic1a_gain,
	.get_gain_func = ste_audio_io_get_mic1a_gain,
	.mute_func = ste_audio_io_mute_mic1a,
	.unmute_func = ste_audio_io_unmute_mic1a,
	.enable_fade_func = ste_audio_io_enable_fade_mic1a,
	.disable_fade_func = ste_audio_io_disable_fade_mic1a,
	.enable_loop = ste_audio_io_enable_loop_mic1b,
	.disable_loop = ste_audio_io_disable_loop_mic1b
};

static struct transducer_context_t transducer_mic2 = {
	.pwr_up_func = ste_audio_io_power_up_mic2,
	.pwr_down_func = ste_audio_io_power_down_mic2,
	.set_gain_func = ste_audio_io_set_mic2_gain,
	.get_gain_func = ste_audio_io_get_mic2_gain,
	.mute_func = ste_audio_io_mute_mic2,
	.unmute_func = ste_audio_io_unmute_mic2,
	.enable_fade_func = ste_audio_io_enable_fade_mic2,
	.disable_fade_func = ste_audio_io_disable_fade_mic2
};

static struct transducer_context_t transducer_lin = {
	.pwr_up_func = ste_audio_io_power_up_lin,
	.pwr_down_func = ste_audio_io_power_down_lin,
	.set_gain_func = ste_audio_io_set_lin_gain,
	.get_gain_func = ste_audio_io_get_lin_gain,
	.mute_func = ste_audio_io_mute_lin,
	.unmute_func = ste_audio_io_unmute_lin,
	.enable_fade_func = ste_audio_io_enable_fade_lin,
	.disable_fade_func = ste_audio_io_disable_fade_lin
};

static struct transducer_context_t transducer_dmic12 = {
	.pwr_up_func = ste_audio_io_power_up_dmic12,
	.pwr_down_func = ste_audio_io_power_down_dmic12,
	.set_gain_func = ste_audio_io_set_dmic12_gain,
	.get_gain_func = ste_audio_io_get_dmic12_gain,
	.mute_func = ste_audio_io_mute_dmic12,
	.unmute_func = ste_audio_io_unmute_dmic12,
	.enable_fade_func = ste_audio_io_enable_fade_dmic12,
	.disable_fade_func = ste_audio_io_disable_fade_dmic12,
	.enable_loop = ste_audio_io_enable_loop_dmic12,
	.disable_loop = ste_audio_io_disable_loop_dmic12
};

static struct transducer_context_t transducer_dmic34 = {
	.pwr_up_func = ste_audio_io_power_up_dmic34,
	.pwr_down_func = ste_audio_io_power_down_dmic34,
	.set_gain_func = ste_audio_io_set_dmic34_gain,
	.get_gain_func = ste_audio_io_get_dmic34_gain,
	.mute_func = ste_audio_io_mute_dmic34,
	.unmute_func = ste_audio_io_unmute_dmic34,
	.enable_fade_func = ste_audio_io_enable_fade_dmic34,
	.disable_fade_func = ste_audio_io_disable_fade_dmic34
};

static struct transducer_context_t transducer_dmic56 = {
	.pwr_up_func = ste_audio_io_power_up_dmic56,
	.pwr_down_func = ste_audio_io_power_down_dmic56,
	.set_gain_func = ste_audio_io_set_dmic56_gain,
	.get_gain_func = ste_audio_io_get_dmic56_gain,
	.mute_func = ste_audio_io_mute_dmic56,
	.unmute_func = ste_audio_io_unmute_dmic56,
	.enable_fade_func = ste_audio_io_enable_fade_dmic56,
	.disable_fade_func = ste_audio_io_disable_fade_dmic56,
};

static struct transducer_context_t transducer_fmrx = {
	.pwr_up_func = ste_audio_io_power_up_fmrx,
	.pwr_down_func = ste_audio_io_power_down_fmrx,
};

static struct transducer_context_t transducer_fmtx = {
	.pwr_up_func = ste_audio_io_power_up_fmtx,
	.pwr_down_func = ste_audio_io_power_down_fmtx,
};

static struct transducer_context_t transducer_bluetooth = {
	.pwr_up_func = ste_audio_io_power_up_bluetooth,
	.pwr_down_func = ste_audio_io_power_down_bluetooth,
};

static void ste_audio_io_init_transducer_cnxt(void)
{
	ptr_audio_codec_cnxt->transducer[HS_CH] = &transducer_headset;
	ptr_audio_codec_cnxt->transducer[EAR_CH] = &transducer_earpiece;
	ptr_audio_codec_cnxt->transducer[IHF_CH] = &transducer_ihf;
	ptr_audio_codec_cnxt->transducer[VIBL_CH] = &transducer_vibl;
	ptr_audio_codec_cnxt->transducer[VIBR_CH] = &transducer_vibr;
	ptr_audio_codec_cnxt->transducer[MIC1A_CH] = &transducer_mic1a;
	ptr_audio_codec_cnxt->transducer[MIC1B_CH] = &transducer_mic1b;
	ptr_audio_codec_cnxt->transducer[MIC2_CH] = &transducer_mic2;
	ptr_audio_codec_cnxt->transducer[LIN_CH] = &transducer_lin;
	ptr_audio_codec_cnxt->transducer[DMIC12_CH] = &transducer_dmic12;
	ptr_audio_codec_cnxt->transducer[DMIC34_CH] = &transducer_dmic34;
	ptr_audio_codec_cnxt->transducer[DMIC56_CH] = &transducer_dmic56;
	ptr_audio_codec_cnxt->transducer[FMRX_CH] = &transducer_fmrx;
	ptr_audio_codec_cnxt->transducer[FMTX_CH] = &transducer_fmtx;
	ptr_audio_codec_cnxt->transducer[BLUETOOTH_CH] = &transducer_bluetooth;
}

void ste_audio_io_core_api_free_data(void)
{
	regulator_put(regulator_vdmic);
	regulator_put(regulator_vamic1);
	regulator_put(regulator_vamic2);
	regulator_put(regulator_vaudio);
	regulator_put(regulator_avsource);
	kfree(ptr_audio_codec_cnxt);
}

static int ste_audio_io_core_api_enable_regulators(int channel_type)
{
	int error = 0;

	switch (channel_type) {
	case EAR_CH:
	case HS_CH:
	case IHF_CH:
	case VIBL_CH:
	case VIBR_CH:
	case LIN_CH:
	case FMRX_CH:
	case FMTX_CH:
	case BLUETOOTH_CH:
		/* vaduio already enabled
		   no additional regualtor required */
	break;

	case MIC1A_CH:
	case MIC1B_CH:
		error = regulator_enable(regulator_vamic1);
		if (error)
			dev_err(ptr_audio_codec_cnxt->dev,
			"unable to enable regulator vamic1 error = %d", error);
	break;

	case MIC2_CH:
		error = regulator_enable(regulator_vamic2);
		if (error)
			dev_err(ptr_audio_codec_cnxt->dev,
			"unable to enable regulator vamic2 error = %d", error);
	break;

	case DMIC12_CH:
	case DMIC34_CH:
	case DMIC56_CH:
	case MULTI_MIC_CH:
		error = regulator_enable(regulator_vdmic);
		if (error)
			dev_err(ptr_audio_codec_cnxt->dev,
			"unable to enable regulator vdmic error = %d", error);
	}
	return error;
}

static int ste_audio_io_core_api_disable_regulators(int channel_type)
{
	int error = 0;

	switch (channel_type) {
	case EAR_CH:
	case HS_CH:
	case IHF_CH:
	case VIBL_CH:
	case VIBR_CH:
	case LIN_CH:
	case FMRX_CH:
	case FMTX_CH:
	case BLUETOOTH_CH:
		/* no need to disable separately*/
	break;

	case MIC1A_CH:
	case MIC1B_CH:
		error = regulator_disable(regulator_vamic1);
		if (error)
			dev_err(ptr_audio_codec_cnxt->dev,
			"unable to disable regulator vamic1 error = %d", error);
	break;

	case MIC2_CH:
		error = regulator_disable(regulator_vamic2);
		if (error)
			dev_err(ptr_audio_codec_cnxt->dev,
			"unable to disable regulator vamic2 error = %d", error);
	break;

	case DMIC12_CH:
	case DMIC34_CH:
	case DMIC56_CH:
	case MULTI_MIC_CH:
		error = regulator_disable(regulator_vdmic);
		if (error)
			dev_err(ptr_audio_codec_cnxt->dev,
			"unable to disable regulator vdmic error = %d", error);
	}
	return error;
}

int ste_audio_io_core_api_powerup_audiocodec(int power_client)
{
	int error = 0;
	int acodec_device_id;

	/* aquire mutex */
	mutex_lock(&(ptr_audio_codec_cnxt->audio_io_mutex));

	acodec_device_id = abx500_get_chip_id(&ste_audio_io_device->dev);

	/*
	 * If there is no power client registered, power up
	 * common audio blocks for audio and vibrator
	 */
	if (!ptr_audio_codec_cnxt->power_client) {
		__u8 data, old_data;

		old_data = HW_REG_READ(AB8500_CTRL3_REG);

		/* Enable 32 Khz clock signal on Clk32KOut2 ball */
		data = (~CLK_32K_OUT2_DISABLE) & old_data;
		error = HW_REG_WRITE(AB8500_CTRL3_REG, data);
		if (error) {
			dev_err(ptr_audio_codec_cnxt->dev,
				"enabling 32KHz clock error = %d", error);
			goto err_cleanup;
		}
		data = INACTIVE_RESET_AUDIO | old_data;
		error = HW_REG_WRITE(AB8500_CTRL3_REG, data);
		if (error) {
			dev_err(ptr_audio_codec_cnxt->dev,
			"deactivate audio codec reset error = %d", error);
			goto err_cleanup;
		}
		old_data = HW_REG_READ(AB8500_SYSULPCLK_CTRL1_REG);
		data = ENABLE_AUDIO_CLK_TO_AUDIO_BLK | old_data;

		error = HW_REG_WRITE(AB8500_SYSULPCLK_CTRL1_REG, data);
		if (error) {
			dev_err(ptr_audio_codec_cnxt->dev,
			"enabling clock to audio block error = %d", error);
			goto err_cleanup;
		}
		regulator_enable(regulator_vaudio);

		old_data = HW_REG_READ(AB8500_GPIO_DIR4_REG);
		data = (GPIO27_DIR_OUTPUT | GPIO29_DIR_OUTPUT |
			GPIO31_DIR_OUTPUT) | old_data;
		error = HW_REG_WRITE(AB8500_GPIO_DIR4_REG, data);
		if (error) {
			dev_err(ptr_audio_codec_cnxt->dev,
				"setting gpio dir4 error = %d", error);
			goto err_cleanup;
		}
		error = HW_REG_WRITE(SOFTWARE_RESET_REG, SW_RESET);
		if (error != 0) {
			dev_err(ptr_audio_codec_cnxt->dev,
				"Software reset error=%d", error);
			goto err_cleanup;
		}

		error = HW_ACODEC_MODIFY_WRITE(POWER_UP_CONTROL_REG,
				(DEVICE_POWER_UP|ANALOG_PARTS_POWER_UP), 0);
		if (error != 0) {
			dev_err(ptr_audio_codec_cnxt->dev,
				"Device Power Up, error=%d", error);
			goto err_cleanup;
		}
	}
	/* Save information that given client already powered up audio block */
	ptr_audio_codec_cnxt->power_client |= power_client;

	/* If audio block requested power up, turn on additional audio blocks */
	if (power_client == STE_AUDIOIO_POWER_AUDIO) {
		if (!ptr_audio_codec_cnxt->audio_codec_powerup) {
			clk_ptr_sysclk =
				clk_get(ptr_audio_codec_cnxt->dev, "sysclk");
			if (!IS_ERR(clk_ptr_sysclk)) {
				error = clk_enable(clk_ptr_sysclk);
				if (error)
					goto err_cleanup;
			} else {
				error = -EFAULT;
				goto err_cleanup;
			}

			clk_ptr_msp1 = clk_get_sys("msp1", NULL);
			if (!IS_ERR(clk_ptr_msp1)) {
				error = clk_enable(clk_ptr_msp1);
				if (error)
					goto err_cleanup;
			} else {
				error = -EFAULT;
				goto err_cleanup;
			}

			if (AB8500_REV_20 == acodec_device_id) {
				clk_ptr_msp3 = clk_get_sys("msp3", NULL);
				if (!IS_ERR(clk_ptr_msp3)) {
					error = clk_enable(clk_ptr_msp3);
					if (error)
						goto err_cleanup;
				} else {
					error = -EFAULT;
					goto err_cleanup;
				}
			}

			if (ptr_audio_codec_cnxt->gpio_altf_init) {
				error = ptr_audio_codec_cnxt->gpio_altf_init();
				if (error)
					goto err_cleanup;
			}

			error = HW_ACODEC_MODIFY_WRITE(IF0_IF1_MASTER_CONF_REG,
							EN_MASTGEN, 0);
			if (error != 0) {
				dev_err(ptr_audio_codec_cnxt->dev,
				"Enable Master Generator, error=%d", error);
				goto err_cleanup;
			}

			error = HW_ACODEC_MODIFY_WRITE(TDM_IF_BYPASS_B_FIFO_REG,
							IF0_MASTER, 0);
			if (error != 0) {
				dev_err(ptr_audio_codec_cnxt->dev,
					"IF0: Master Mode, error=%d", error);
				goto err_cleanup;
			}

			/* Configuring IF0 */

			error = HW_ACODEC_MODIFY_WRITE(IF0_IF1_MASTER_CONF_REG,
							BITCLK_OSR_N_256, 0);
			if (error != 0) {
				dev_err(ptr_audio_codec_cnxt->dev,
				"IF0: Enable FsBitClk & FSync error=%d", error);
				goto err_cleanup;
			}

			error = HW_REG_WRITE(IF0_CONF_REG, IF_DELAYED
						| TDM_FORMAT | WORD_LENGTH_20);
			if (error != 0) {
				dev_err(ptr_audio_codec_cnxt->dev,
				"IF0: TDM Format 16 Bits word length, error=%d",
					error);
				goto err_cleanup;
			}
		}
		ptr_audio_codec_cnxt->audio_codec_powerup++;
	}
err_cleanup:
	/* release mutex */
	mutex_unlock(&(ptr_audio_codec_cnxt->audio_io_mutex));
	return error;
}

int ste_audio_io_core_api_powerdown_audiocodec(int power_client)
{
	int error = 0;
	/* aquire mutex */
	mutex_lock(&(ptr_audio_codec_cnxt->audio_io_mutex));

	/* Update power client status */
	if (power_client == STE_AUDIOIO_POWER_AUDIO) {
		ptr_audio_codec_cnxt->audio_codec_powerup--;
		if (!ptr_audio_codec_cnxt->audio_codec_powerup) {
			ptr_audio_codec_cnxt->power_client &= ~power_client;
			clk_disable(clk_ptr_sysclk);
			clk_put(clk_ptr_sysclk);
			clk_disable(clk_ptr_msp1);
			clk_put(clk_ptr_msp1);
			if (AB8500_REV_20 ==
				abx500_get_chip_id(&ste_audio_io_device->dev)) {
				clk_disable(clk_ptr_msp3);
				clk_put(clk_ptr_msp3);
			}

			if (ptr_audio_codec_cnxt->gpio_altf_exit) {
				error = ptr_audio_codec_cnxt->gpio_altf_exit();
				if (error)
					goto err_cleanup;
			}
		}
	} else
		ptr_audio_codec_cnxt->power_client &= ~power_client;

	/* If no power client registered, power down audio block */
	if (!ptr_audio_codec_cnxt->power_client) {
		regulator_disable(regulator_vaudio);
		if (error != 0) {
			dev_err(ptr_audio_codec_cnxt->dev,
		"Device Power Down and Analog Parts Power Down error = %d ",
							error);
			goto err_cleanup;
		}
	}

err_cleanup:
	/* release mutex */
	mutex_unlock(&(ptr_audio_codec_cnxt->audio_io_mutex));
	return error;
}
/**
 * @brief Read from AB8500 device
 * @dev_data Pointer to the structure __audioio_data
 * @return 0
 */

int ste_audio_io_core_api_access_read(struct audioio_data_t *dev_data)
{
	int reg;
	if (NULL == dev_data)
		return -EFAULT;
	reg = (dev_data->block<<8)|(dev_data->addr&0xff);
	dev_data->data = HW_REG_READ(reg);
	return 0;
}
/**
 * @brief Write on AB8500 device
 * @dev_data Pointer to the structure __audioio_data
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_core_api_access_write(struct audioio_data_t *dev_data)
{
	int retval, reg;
	if (NULL == dev_data)
		return -EFAULT;

	reg = (dev_data->block<<8)|(dev_data->addr&0xff);
	retval = HW_REG_WRITE(reg, dev_data->data);

	return retval;
}
/**
 * @brief Store the power and mute status of transducer
 * @channel_index Channel-index of transducer
 * @ptr Array storing the status
 * @value status being stored
 * @return 0 on success otherwise negative error code
 */

void ste_audio_io_core_api_store_data(enum AUDIOIO_CH_INDEX channel_index,
							int *ptr, int value)
{
	if (channel_index & e_CHANNEL_1)
		ptr[0] = value;

	if (channel_index & e_CHANNEL_2)
		ptr[1] = value;

	if (channel_index & e_CHANNEL_3)
		ptr[2] = value;

	if (channel_index & e_CHANNEL_4)
		ptr[3] = value;
}
/**
 * @brief Get power or mute status on a specific channel
 * @channel_index Channel-index of the transducer
 * @ptr Pointer to is_power_up array or is_muted array
 * @return status of control switch
 */
enum AUDIOIO_COMMON_SWITCH ste_audio_io_core_api_get_status(
				enum AUDIOIO_CH_INDEX channel_index, int *ptr)
{
	if (channel_index & e_CHANNEL_1) {
		if (AUDIOIO_TRUE == ptr[0])
			return AUDIOIO_COMMON_ON;
		else
			return AUDIOIO_COMMON_OFF;
	}

	if (channel_index & e_CHANNEL_2) {
		if (AUDIOIO_TRUE == ptr[1])
			return AUDIOIO_COMMON_ON;
		else
			return AUDIOIO_COMMON_OFF;
	}

	if (channel_index & e_CHANNEL_3) {
		if (AUDIOIO_TRUE == ptr[2])
			return AUDIOIO_COMMON_ON;
		else
			return AUDIOIO_COMMON_OFF;
	}

	if (channel_index & e_CHANNEL_4) {
		if (AUDIOIO_TRUE == ptr[3])
			return AUDIOIO_COMMON_ON;
		else
			return AUDIOIO_COMMON_OFF;
	}
	return 0;
}

int ste_audio_io_core_api_acodec_power_control(struct audioio_acodec_pwr_ctrl_t
							*audio_acodec_pwr_ctrl)
{
	int error = 0;
	if (audio_acodec_pwr_ctrl->ctrl_switch == AUDIOIO_COMMON_ON)
		error = ste_audio_io_core_api_powerup_audiocodec(
				STE_AUDIOIO_POWER_AUDIO);
	else
		error = ste_audio_io_core_api_powerdown_audiocodec(
				STE_AUDIOIO_POWER_AUDIO);

	return error;
}
/**
 * @brief Control for powering on/off HW components on a specific channel
 * @pwr_ctrl Pointer to the structure __audioio_pwr_ctrl
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_core_api_power_control_transducer(
					struct audioio_pwr_ctrl_t *pwr_ctrl)
{
	int error = 0;
	struct transducer_context_t *ptr = NULL;
	enum AUDIOIO_CH_INDEX channel_index;

	channel_index = pwr_ctrl->channel_index;

	if ((pwr_ctrl->channel_type < FIRST_CH)
			|| (pwr_ctrl->channel_type > LAST_CH))
		return -EINVAL;

	ptr = ptr_audio_codec_cnxt->transducer[pwr_ctrl->channel_type];

	/* aquire mutex */
	mutex_lock(&(ptr_audio_codec_cnxt->audio_io_mutex));

	if (AUDIOIO_COMMON_ON == pwr_ctrl->ctrl_switch) {
		if (ptr->pwr_up_func) {
			error = ste_audio_io_core_api_enable_regulators(
							pwr_ctrl->channel_type);
			if (error)
				goto free_mutex;

			error = ptr->pwr_up_func(pwr_ctrl->channel_index,
						ptr_audio_codec_cnxt->dev);
			if (0 == error) {
				ste_audio_io_core_api_store_data(channel_index,
						ptr->is_power_up, AUDIOIO_TRUE);
			}
		}
	} else {
		if (ptr->pwr_down_func) {
			error = ptr->pwr_down_func(pwr_ctrl->channel_index,
						ptr_audio_codec_cnxt->dev);
			if (0 == error) {
				ste_audio_io_core_api_store_data(channel_index,
					ptr->is_power_up, AUDIOIO_FALSE);
			}
			error = ste_audio_io_core_api_disable_regulators(
							pwr_ctrl->channel_type);
		}
	}

free_mutex:
	/* release mutex */
	mutex_unlock(&(ptr_audio_codec_cnxt->audio_io_mutex));

	return error;
}
/**
 * @brief Query power state of HW path on specified channel
 * @pwr_ctrl Pointer to the structure __audioio_pwr_ctrl
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_core_api_power_status_transducer(
					struct audioio_pwr_ctrl_t *pwr_ctrl)
{

	struct transducer_context_t *ptr = NULL;
	enum AUDIOIO_CH_INDEX channel_index;

	channel_index = pwr_ctrl->channel_index;

	if ((pwr_ctrl->channel_type < FIRST_CH)
			|| (pwr_ctrl->channel_type > LAST_CH))
		return -EINVAL;

	ptr = ptr_audio_codec_cnxt->transducer[pwr_ctrl->channel_type];


	/* aquire mutex */
	mutex_lock(&(ptr_audio_codec_cnxt->audio_io_mutex));


	pwr_ctrl->ctrl_switch = ste_audio_io_core_api_get_status(channel_index,
							ptr->is_power_up);

	/* release mutex */
	mutex_unlock(&(ptr_audio_codec_cnxt->audio_io_mutex));

	return 0;

}

int ste_audio_io_core_api_loop_control(struct audioio_loop_ctrl_t *loop_ctrl)
{
	int error = 0;
	struct transducer_context_t *ptr = NULL;

	if ((loop_ctrl->channel_type < FIRST_CH)
			|| (loop_ctrl->channel_type > LAST_CH))
		return -EINVAL;

	ptr = ptr_audio_codec_cnxt->transducer[loop_ctrl->channel_type];

	mutex_lock(&(ptr_audio_codec_cnxt->audio_io_mutex));

	if (AUDIOIO_COMMON_ON == loop_ctrl->ctrl_switch) {
		if (ptr->enable_loop) {
			error = ptr->enable_loop(loop_ctrl->channel_index,
					loop_ctrl->hw_loop,
					loop_ctrl->loop_gain,
					ptr_audio_codec_cnxt->dev,
					ptr_audio_codec_cnxt->transducer);
			if (error)
				dev_err(ptr_audio_codec_cnxt->dev,
			"Loop enable failed for hw loop = %d,  error = %d ",
					(int)loop_ctrl->hw_loop, error);
		} else {
			error = -EFAULT;
			dev_err(ptr_audio_codec_cnxt->dev,
		"Hw Loop enable does not exist for channel= %d,  error = %d ",
					(int)loop_ctrl->channel_type, error);
		}
	} else {
		if (ptr->disable_loop) {
			error = ptr->disable_loop(loop_ctrl->channel_index,
					loop_ctrl->hw_loop,
					ptr_audio_codec_cnxt->dev,
					ptr_audio_codec_cnxt->transducer);
			if (error)
				dev_err(ptr_audio_codec_cnxt->dev,
			"Loop disable failed for hw loop = %d,  error = %d ",
					(int)loop_ctrl->hw_loop, error);
		} else {
			error = -EFAULT;
				dev_err(ptr_audio_codec_cnxt->dev,
		"Hw Loop disable does not exist for channel= %d,  error = %d ",
					(int)loop_ctrl->channel_type, error);
		}
	}

	mutex_unlock(&(ptr_audio_codec_cnxt->audio_io_mutex));

	return error;
}

int ste_audio_io_core_api_loop_status(struct audioio_loop_ctrl_t *loop_ctrl)
{
	return 0;
}

int ste_audio_io_core_api_get_transducer_gain_capability(
					struct audioio_get_gain_t *get_gain)
{
	return 0;
}

int ste_audio_io_core_api_gain_capabilities_loop(
					struct audioio_gain_loop_t *gain_loop)
{
	if ((gain_loop->channel_type < FIRST_CH)
			|| (gain_loop->channel_type > LAST_CH))
		return -EINVAL;

	mutex_lock(&(ptr_audio_codec_cnxt->audio_io_mutex));

	gain_loop->num_loop =
		transducer_max_no_Of_supported_loops[gain_loop->channel_type];
	gain_loop->max_gains = max_no_of_loop_gains[gain_loop->channel_type];
	mutex_unlock(&(ptr_audio_codec_cnxt->audio_io_mutex));
	return 0;
}

int ste_audio_io_core_api_supported_loops(
				struct audioio_support_loop_t *support_loop)
{
	if ((support_loop->channel_type < FIRST_CH)
			|| (support_loop->channel_type > LAST_CH))
		return -EINVAL;

	mutex_lock(&(ptr_audio_codec_cnxt->audio_io_mutex));
	support_loop->spprtd_loop_index =
	transducer_no_Of_supported_loop_indexes[support_loop->channel_type];
	mutex_unlock(&(ptr_audio_codec_cnxt->audio_io_mutex));
	return 0;
}

int ste_audio_io_core_api_gain_descriptor_transducer(
				struct audioio_gain_desc_trnsdr_t *gdesc_trnsdr)
{
	return 0;
}
/**
 * @brief Control for muting a specific channel in HW
 * @mute_trnsdr Pointer to the structure __audioio_mute_trnsdr
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_core_api_mute_control_transducer(
				struct audioio_mute_trnsdr_t *mute_trnsdr)
{
	int error = 0;
	struct transducer_context_t *ptr = NULL;
	enum AUDIOIO_CH_INDEX channel_index;

	channel_index = mute_trnsdr->channel_index;

	if ((mute_trnsdr->channel_type < FIRST_CH)
			|| (mute_trnsdr->channel_type > LAST_CH))
		return -EINVAL;

	ptr = ptr_audio_codec_cnxt->transducer[mute_trnsdr->channel_type];

	/* aquire mutex */
	mutex_lock(&(ptr_audio_codec_cnxt->audio_io_mutex));

	if (AUDIOIO_COMMON_ON == mute_trnsdr->ctrl_switch) {
		if (ptr->mute_func) {
			error = ptr->mute_func(mute_trnsdr->channel_index,
						ptr_audio_codec_cnxt->dev);
		    if (0 == error) {
				ste_audio_io_core_api_store_data(channel_index ,
						ptr->is_muted, AUDIOIO_TRUE);
			}
		}
	} else {
		if (ptr->unmute_func) {
			if (0 == ptr->unmute_func(channel_index, ptr->gain,
						ptr_audio_codec_cnxt->dev)) {
				ste_audio_io_core_api_store_data(channel_index,
						ptr->is_muted, AUDIOIO_FALSE);
			}
		}
	}

	/* release mutex */
	mutex_unlock(&(ptr_audio_codec_cnxt->audio_io_mutex));

	return error;
}
/**
 * @brief Query state of mute on specified channel
 * @mute_trnsdr Pointer to the structure __audioio_mute_trnsdr
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_core_api_mute_status_transducer(
				struct audioio_mute_trnsdr_t *mute_trnsdr)
{
	struct transducer_context_t *ptr = NULL;
	enum AUDIOIO_CH_INDEX channel_index;

	channel_index = mute_trnsdr->channel_index;

	if ((mute_trnsdr->channel_type < FIRST_CH)
			|| (mute_trnsdr->channel_type > LAST_CH))
		return -EINVAL;

	ptr = ptr_audio_codec_cnxt->transducer[mute_trnsdr->channel_type];

	/* aquire mutex */
	mutex_lock(&(ptr_audio_codec_cnxt->audio_io_mutex));

	mute_trnsdr->ctrl_switch = ste_audio_io_core_api_get_status(
				channel_index, ptr->is_muted);
	/* release mutex */
	mutex_unlock(&(ptr_audio_codec_cnxt->audio_io_mutex));

	return 0;
}
/**
 * @brief control the fading on the transducer called on.
 * @fade_ctrl Pointer to the structure __audioio_fade_ctrl
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_core_api_fading_control(struct audioio_fade_ctrl_t *fade_ctrl)
{
	int error = 0;
	struct transducer_context_t *ptr = NULL;

	if ((fade_ctrl->channel_type < FIRST_CH)
			|| (fade_ctrl->channel_type > LAST_CH))
		return -EINVAL;
	ptr = ptr_audio_codec_cnxt->transducer[fade_ctrl->channel_type];

	/* aquire mutex */
	mutex_lock(&(ptr_audio_codec_cnxt->audio_io_mutex));

	if (AUDIOIO_COMMON_ON == fade_ctrl->ctrl_switch)
		error = ptr->enable_fade_func(ptr_audio_codec_cnxt->dev);

	else
		error = ptr->disable_fade_func(ptr_audio_codec_cnxt->dev);


	/* release mutex */
	mutex_unlock(&(ptr_audio_codec_cnxt->audio_io_mutex));

	return error;
}
/**
 * @brief control the low power mode of headset.
 * @burst_ctrl Pointer to the structure __audioio_burst_ctrl
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_core_api_burstmode_control(
					struct audioio_burst_ctrl_t *burst_ctrl)
{
	int error = 0;
	struct transducer_context_t *ptr = NULL;
	int burst_fifo_switch_frame;

	burst_fifo_switch_frame = burst_ctrl->burst_fifo_switch_frame;

	if ((burst_ctrl->channel_type < FIRST_CH)
			|| (burst_ctrl->channel_type > LAST_CH))
		return -EINVAL;
	ptr = ptr_audio_codec_cnxt->transducer[burst_ctrl->channel_type];

	/* aquire mutex */
	mutex_lock(&(ptr_audio_codec_cnxt->audio_io_mutex));

	if (AUDIOIO_COMMON_ON == burst_ctrl->ctrl_switch) {
		if (ptr->switch_to_burst_func)
			error = ptr->switch_to_burst_func(
					burst_fifo_switch_frame,
						ptr_audio_codec_cnxt->dev);
	} else
			if (ptr->switch_to_normal_func)
				error = ptr->switch_to_normal_func(
						ptr_audio_codec_cnxt->dev);
	/* release mutex */
	mutex_unlock(&(ptr_audio_codec_cnxt->audio_io_mutex));

	return error;
}
/**
 * @brief Convert channel index to array index
 * @channel_index Channel Index of transducer
 * @return Array index corresponding to the specified channel index
 */

int convert_channel_index_to_array_index(enum AUDIOIO_CH_INDEX channel_index)
{
	if (channel_index & e_CHANNEL_1)
		return 0;
	else if (channel_index & e_CHANNEL_2)
		return 1;
	else if (channel_index & e_CHANNEL_3)
		return 2;
	else
		return 3;
}

/**
 * @brief Set individual gain along the HW path of a specified channel
 * @gctrl_trnsdr Pointer to the structure __audioio_gain_ctrl_trnsdr
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_core_api_gain_control_transducer(
				struct audioio_gain_ctrl_trnsdr_t *gctrl_trnsdr)
{
	struct transducer_context_t *ptr = NULL;
	enum AUDIOIO_CH_INDEX channel_index;
	int ch_array_index;
	u16 gain_index;
	int gain_value;
	u32 linear;
	int channel_type;
	int error;
	int min_gain, max_gain, gain;

	if ((gctrl_trnsdr->channel_type < FIRST_CH)
			|| (gctrl_trnsdr->channel_type > LAST_CH))
		return -EINVAL;

	if (gctrl_trnsdr->gain_index >= MAX_NO_GAINS)
		return -EINVAL;

	ptr = ptr_audio_codec_cnxt->transducer[gctrl_trnsdr->channel_type];
	channel_index = gctrl_trnsdr->channel_index;
	gain_index = gctrl_trnsdr->gain_index;
	gain_value = gctrl_trnsdr->gain_value;
	linear = gctrl_trnsdr->linear;
	channel_type = gctrl_trnsdr->channel_type;

	ch_array_index = convert_channel_index_to_array_index(channel_index);
	if (linear) {	/* Gain is in the range 0 to 100 */
		min_gain = gain_descriptor[channel_type]\
				[ch_array_index][gain_index].min_gain;
		max_gain = gain_descriptor[channel_type]\
				[ch_array_index][gain_index].max_gain;

		gain = ((gain_value * (max_gain - min_gain))/100) + min_gain;
	} else
		/* Convert to db */
		gain = gain_value/100;

	gain_value = gain;

#if 1
	if (gain_index >= transducer_no_of_gains[channel_type])
		return -EINVAL;

	if (gain_value < gain_descriptor[channel_type]\
				[ch_array_index][gain_index].min_gain)
		return -EINVAL;

	if (gain_value > gain_descriptor[channel_type]\
				[ch_array_index][gain_index].max_gain)
		return -EINVAL;

#endif

	/* aquire mutex */
	mutex_lock(&(ptr_audio_codec_cnxt->audio_io_mutex));

	error = ptr->set_gain_func(channel_index,
					gain_index, gain_value, linear,
					ptr_audio_codec_cnxt->dev);
	if (0 == error)
		ste_audio_io_core_api_store_data(channel_index ,
							ptr->gain, gain_value);


	/* release mutex */
	mutex_unlock(&(ptr_audio_codec_cnxt->audio_io_mutex));

	return error;
}
/**
 * @brief Get individual gain along the HW path of a specified channel
 * @gctrl_trnsdr Pointer to the structure __audioio_gain_ctrl_trnsdr
 * @return 0 on success otherwise negative error code
 */


int ste_audio_io_core_api_gain_query_transducer(
				struct audioio_gain_ctrl_trnsdr_t *gctrl_trnsdr)
{
	struct transducer_context_t *ptr = NULL;
	enum AUDIOIO_CH_INDEX channel_index;
	u16 gain_index;
	u32 linear;
	int left_volume, right_volume;
	int max_gain, min_gain;
	int ch_array_index;

	if ((gctrl_trnsdr->channel_type < FIRST_CH)
			|| (gctrl_trnsdr->channel_type > LAST_CH))
		return -EINVAL;

	if (gctrl_trnsdr->gain_index >= MAX_NO_GAINS)
		return -EINVAL;

	ptr = ptr_audio_codec_cnxt->transducer[gctrl_trnsdr->channel_type];

	channel_index = gctrl_trnsdr->channel_index;
	gain_index = gctrl_trnsdr->gain_index;
	linear = gctrl_trnsdr->linear;

	ptr->get_gain_func(&left_volume, &right_volume, gain_index,
						ptr_audio_codec_cnxt->dev);

	ch_array_index = convert_channel_index_to_array_index(channel_index);
	max_gain = gain_descriptor[gctrl_trnsdr->channel_type]\
				[ch_array_index][gain_index].max_gain;
	min_gain = gain_descriptor[gctrl_trnsdr->channel_type]\
				[ch_array_index][gain_index].min_gain;

	switch (channel_index) {
	case e_CHANNEL_1:
		gctrl_trnsdr->gain_value = linear ? \
		min_gain+left_volume*(max_gain-min_gain)/100 : left_volume;
		break;
	case e_CHANNEL_2:
		gctrl_trnsdr->gain_value = linear ? \
		min_gain+right_volume*(max_gain-min_gain)/100 : right_volume;
		break;
	case e_CHANNEL_3:
		break;
	case e_CHANNEL_4:
		break;
	case e_CHANNEL_ALL:
		if (left_volume == right_volume) {
			if (linear)
				gctrl_trnsdr->gain_value =
				min_gain+right_volume*(max_gain-min_gain)/100;
			else
				gctrl_trnsdr->gain_value  = right_volume;
			}
	}

	return 0;
}


int ste_audio_io_core_api_fsbitclk_control(
			struct audioio_fsbitclk_ctrl_t *fsbitclk_ctrl)
{
	int error = 0;

	if (AUDIOIO_COMMON_ON == fsbitclk_ctrl->ctrl_switch)
		error = HW_ACODEC_MODIFY_WRITE(IF0_IF1_MASTER_CONF_REG,
							EN_FSYNC_BITCLK, 0);
	else
		error = HW_ACODEC_MODIFY_WRITE(IF0_IF1_MASTER_CONF_REG, 0,
							EN_FSYNC_BITCLK);

	return error;
}
int ste_audio_io_core_api_pseudoburst_control(
			struct audioio_pseudoburst_ctrl_t *pseudoburst_ctrl)
{
	int error = 0;

	return error;
}
int ste_audio_io_core_debug(int x)
{
	debug_audioio(x);

return 0;
}

/**
 * ste_audioio_vibrator_alloc()
 * @client:		Client id which allocates vibrator
 * @mask:		Mask against which vibrator usage is checked
 *
 * This function allocates vibrator.
 * Mask is added here as audioio driver controls left and right vibrator
 * separately (can work independently). In case when audioio has allocated
 * one of its channels (left or right) it should be still able to allocate
 * the other channel.
 *
 * Returns:
 *	0 - Success
 *	-EBUSY - other client already registered
 **/
int ste_audioio_vibrator_alloc(int client, int mask)
{
	int error = 0;

	/* Check if other client is already using vibrator */
	if (ptr_audio_codec_cnxt->vibra_client & ~mask)
		error = -EBUSY;
	else
		ptr_audio_codec_cnxt->vibra_client |= client;

	return error;
}

/**
 * ste_audioio_vibrator_release()
 * @client:	     Client id which releases vibrator
 *
 * This function releases vibrator
 **/
void ste_audioio_vibrator_release(int client)
{
	ptr_audio_codec_cnxt->vibra_client &= ~client;
}

/**
 * ste_audioio_vibrator_pwm_control()
 * @client:		Client id which will use vibrator
 * @left_speed:		Left vibrator speed
 * @right_speed:	Right vibrator speed
 *
 * This function controls vibrator using PWM source
 *
 * Returns:
 *      0 - success
 *	-EBUSY - Vibrator already used
 **/
int ste_audioio_vibrator_pwm_control(
				int client,
				struct ste_vibra_speed left_speed,
				struct ste_vibra_speed right_speed)
{
	int error = 0;

	mutex_lock(&ptr_audio_codec_cnxt->audio_io_mutex);

	/* Try to allocate vibrator for given client */
	error = ste_audioio_vibrator_alloc(client, client);

	mutex_unlock(&ptr_audio_codec_cnxt->audio_io_mutex);

	if (error)
		return error;

	/* Duty cycle supported by vibrator's PWM is 0-100 */
	if (left_speed.positive > STE_AUDIOIO_VIBRATOR_MAX_SPEED)
		left_speed.positive = STE_AUDIOIO_VIBRATOR_MAX_SPEED;

	if (right_speed.positive > STE_AUDIOIO_VIBRATOR_MAX_SPEED)
		right_speed.positive = STE_AUDIOIO_VIBRATOR_MAX_SPEED;

	if (left_speed.negative > STE_AUDIOIO_VIBRATOR_MAX_SPEED)
		left_speed.negative = STE_AUDIOIO_VIBRATOR_MAX_SPEED;

	if (right_speed.negative > STE_AUDIOIO_VIBRATOR_MAX_SPEED)
		right_speed.negative = STE_AUDIOIO_VIBRATOR_MAX_SPEED;

	if (left_speed.negative || right_speed.negative ||
		left_speed.positive || right_speed.positive) {
		/* Power up audio block for vibrator */
		error = ste_audio_io_core_api_powerup_audiocodec(
						STE_AUDIOIO_POWER_VIBRA);
		if (error) {
			dev_err(ptr_audio_codec_cnxt->dev,
				"Audio power up failed %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(ANALOG_OUTPUT_ENABLE_REG,
					(EN_VIBL_MASK|EN_VIBR_MASK), 0);
		if (error) {
			dev_err(ptr_audio_codec_cnxt->dev,
				"Powerup Vibrator Class-D driver %d",
				error);
			return error;
		}

		error = HW_REG_WRITE(VIB_DRIVER_CONF_REG, 0xff);
		if (error) {
			dev_err(ptr_audio_codec_cnxt->dev,
				"Enable Vibrator PWM generator %d",
				error);
			return error;
		}
	}

	error = HW_REG_WRITE(PWM_VIBNL_CONF_REG, left_speed.negative);
	if (error) {
		dev_err(ptr_audio_codec_cnxt->dev,
			"Write Left Vibrator negative PWM %d", error);
		goto err_cleanup;
	}

	error = HW_REG_WRITE(PWM_VIBPL_CONF_REG, left_speed.positive);
	if (error) {
		dev_err(ptr_audio_codec_cnxt->dev,
			"Write Left Vibrator positive PWM %d", error);
		goto err_cleanup;
	}

	error = HW_REG_WRITE(PWM_VIBNR_CONF_REG, right_speed.negative);
	if (error) {
		dev_err(ptr_audio_codec_cnxt->dev,
			"Write Right Vibrator negative PWM %d", error);
		goto err_cleanup;
	}

	error = HW_REG_WRITE(PWM_VIBPR_CONF_REG, right_speed.positive);
	if (error) {
		dev_err(ptr_audio_codec_cnxt->dev,
			"Write Right Vibrator positive PWM %d", error);
		goto err_cleanup;
	}

	if (!left_speed.negative && !right_speed.negative &&
		!left_speed.positive && !right_speed.positive) {
		error = HW_REG_WRITE(VIB_DRIVER_CONF_REG, 0);
		if (error) {
			dev_err(ptr_audio_codec_cnxt->dev,
				"Disable PWM Vibrator generator %d",
				error);
			goto err_cleanup;
		}

		error = HW_ACODEC_MODIFY_WRITE(ANALOG_OUTPUT_ENABLE_REG,
					0, (EN_VIBL_MASK|EN_VIBR_MASK));
		if (error) {
			dev_err(ptr_audio_codec_cnxt->dev,
				"Power down Vibrator Class-D driver %d",
				error);
			goto err_cleanup;
		}

		/* Power down audio block */
		error = ste_audio_io_core_api_powerdown_audiocodec(
				STE_AUDIOIO_POWER_VIBRA);
		if (error) {
			dev_err(ptr_audio_codec_cnxt->dev,
				"Audio power down failed %d", error);
			goto err_cleanup;
		}
	}

err_cleanup:
	/* Release client */
	if (!left_speed.negative && !right_speed.negative &&
		!left_speed.positive && !right_speed.positive) {
		mutex_lock(&ptr_audio_codec_cnxt->audio_io_mutex);
		ste_audioio_vibrator_release(client);
		mutex_unlock(&ptr_audio_codec_cnxt->audio_io_mutex);
	}
	return error;
}
EXPORT_SYMBOL(ste_audioio_vibrator_pwm_control);

/**
 * @brief This function sets FIR coefficients
 * @fir_coeffs: pointer to structure audioio_fir_coefficients_t
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_core_api_fir_coeffs_control(struct audioio_fir_coefficients_t
						*fir_coeffs)
{
	unsigned char coefficient;
	int i, error;

	if (fir_coeffs->start_addr >= STE_AUDIOIO_MAX_COEFFICIENTS)
		return -EINVAL;

	mutex_lock(&(ptr_audio_codec_cnxt->audio_io_mutex));

	error = HW_REG_WRITE(SIDETONE_FIR_ADDR_REG, fir_coeffs->start_addr);
	if (error) {
		dev_err(ptr_audio_codec_cnxt->dev,
			"FIR start address write failed %d", error);
			goto err_cleanup;
	}

	for (i = fir_coeffs->start_addr;
				i < STE_AUDIOIO_MAX_COEFFICIENTS; i++) {

		coefficient = (fir_coeffs->coefficients[i]>>8) & 0xff;
		error = HW_REG_WRITE(SIDETONE_FIR_COEFF_MSB_REG, coefficient);
		if (error) {
			dev_err(ptr_audio_codec_cnxt->dev,
			"FIR coefficient [%d] msb write failed %d", i, error);
			goto err_cleanup;
		}

		coefficient = fir_coeffs->coefficients[i] & 0xff;
		error = HW_REG_WRITE(SIDETONE_FIR_COEFF_LSB_REG, coefficient);
		if (error) {
			dev_err(ptr_audio_codec_cnxt->dev,
			"FIR coefficient [%d] lsb write failed %d", i, error);
			goto err_cleanup;
		}
	}

	error = HW_ACODEC_MODIFY_WRITE(SIDETONE_FIR_ADDR_REG,
						APPLY_FIR_COEFFS_MASK, 0);
	if (error) {
		dev_err(ptr_audio_codec_cnxt->dev,
			"FIR coefficients activation failed %d", error);
			goto err_cleanup;
	}

	error = HW_ACODEC_MODIFY_WRITE(FILTERS_CONTROL_REG,
						FIR_FILTERCONTROL, 0);
	if (error) {
		dev_err(ptr_audio_codec_cnxt->dev,
			"ST FIR Filters enable failed %d", error);
			goto err_cleanup;
	}

err_cleanup:
	mutex_unlock(&(ptr_audio_codec_cnxt->audio_io_mutex));
	return error;
}
