/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Deepak KARDA/ deepak.karda@stericsson.com for ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2.
 */

#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <mach/ste_audio_io_vibrator.h>
#include <mach/ste_audio.h>
#include "ste_audio_io_func.h"
#include "ste_audio_io_core.h"
#include "ste_audio_io_ab8500_reg_defs.h"
#include "ste_audio_io_hwctrl_common.h"

static struct clk *clk_ptr_msp0;
static int bluetooth_power_up_count;
static int  acodec_reg_dump;

#define NCP_TIMEOUT		200 /* 200 ms */
/*
 * TODO: Use proper register defines instead of home-made generic ones.
 */
#define SHIFT_QUARTET0  0
#define SHIFT_QUARTET1  4
#define MASK_QUARTET    (0xFUL)
#define MASK_QUARTET1   (MASK_QUARTET << SHIFT_QUARTET1)
#define MASK_QUARTET0   (MASK_QUARTET << SHIFT_QUARTET0)

/**
 * @brief Modify the specified register
 * @reg Register
 * @mask_set Bit to be set
 * @mask_clear Bit to be cleared
 * @return 0 on success otherwise negative error code
 */

unsigned int ab8500_acodec_modify_write(unsigned int reg, u8 mask_set,
								u8 mask_clear)
{
	u8 value8, retval = 0;
	value8 = HW_REG_READ(reg);
	/* clear the specified bit */
	value8 &= ~mask_clear;
	/* set the asked bit */
	value8 |= mask_set;
	retval = HW_REG_WRITE(reg, value8);
	return retval;
}

/**
 * @brief Power up headset on a specific channel
 * @channel_index Channel-index of headset
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_power_up_headset(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal_DA = 0;
	unsigned long end_time;

	/* Check if HS PowerUp request is mono or Stereo channel */
	if (!(channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		dev_err(dev, "HS should have mono or stereo channels");
		return -EINVAL;
	}

	ste_audio_io_mute_headset(channel_index, dev);

	error = HW_ACODEC_MODIFY_WRITE(NCP_ENABLE_HS_AUTOSTART_REG,
								HS_AUTO_EN, 0);
	if (0 != error) {
		dev_err(dev, "NCP fully controlled with EnCpHs bit %d", error);
		return error;
	}
	error = HW_ACODEC_MODIFY_WRITE(NCP_ENABLE_HS_AUTOSTART_REG,
					(EN_NEG_CP|HS_AUTO_EN), 0);
	if (0 != error) {
		dev_err(dev, "Enable Negative Charge Pump %d", error);
		return error;
	}

	/* Wait for negative charge pump to start */
	end_time = jiffies + msecs_to_jiffies(NCP_TIMEOUT);
	while (!(HW_REG_READ(IRQ_STATUS_MSB_REG) & NCP_READY_MASK)
				&& time_after_eq(end_time, jiffies)) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(1);
	}

	if (!(HW_REG_READ(IRQ_STATUS_MSB_REG) & NCP_READY_MASK)) {
		error = -EFAULT;
		dev_err(dev, "Negative Charge Pump start error % d", error);
		return error;
	}

	/* Enable DA1 for HSL */
	if (channel_index & e_CHANNEL_1) {

		/* Power Up HSL driver */
		error = HW_ACODEC_MODIFY_WRITE(ANALOG_OUTPUT_ENABLE_REG,
							EN_HSL_MASK, 0);
		if (0 != error) {
			dev_err(dev, "Power Up HSL Driver %d", error);
			return error;
		}

		initialVal_DA = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);

		if (EN_DA1 & initialVal_DA)
			return 0;

		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA1_REG,
							SLOT08_FOR_DA_PATH, 0);
		if (0 != error) {
			dev_err(dev, "Data sent to DA1 from Slot 08 %d",
									error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_MUXES_REG1,
								DA1_TO_HSL, 0);
		if (0 != error) {
			dev_err(dev,
				"DA_IN1 path mixed with sidetone FIR %d",
									error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_DA_CHANNELS_ENABLE_REG,
								EN_DA1, 0);
		if (0 != error) {
			dev_err(dev, "Power up HSL %d ", error);
			return error;
		}

		/* Power Up HSL DAC driver */
		error = HW_ACODEC_MODIFY_WRITE(ADC_DAC_ENABLE_REG,
							POWER_UP_HSL_DAC, 0);
		if (0 != error) {
			dev_err(dev, "Power Up HSL DAC driver %d", error);
			return error;
		}

		/* Power up HSL DAC and digital path */
		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_OUTPUT_ENABLE_REG,
							EN_HSL_MASK, 0);
		if (0 != error) {
			dev_err(dev,
				"Power up HSL DAC and digital path %d",
									error);
			return error;
		}

		/*
		* Disable short detection. Pull Down output to ground,
		* Use local oscillator, Gain change without zero cross control
		*/
		error = HW_ACODEC_MODIFY_WRITE(SHORT_CIRCUIT_DISABLE_REG,
			HS_SHORT_DIS|HS_PULL_DOWN_EN|HS_OSC_EN|HS_ZCD_DIS, 0);
		if (0 != error) {
			dev_err(dev, "Disable short detection."
			"Pull Down output to ground,Use local oscillator,Gain"
			"change without zero cross control %d", error);
			return error;
		}
	}

	/* Enable DA2 for HSR */
	if (channel_index & e_CHANNEL_2) {

		/* Power Up HSR driver */
		error = HW_ACODEC_MODIFY_WRITE(ANALOG_OUTPUT_ENABLE_REG,
							EN_HSR_MASK, 0);
		if (0 != error) {
			dev_err(dev, "Power Up HSR Driver %d", error);
			return error;
		}

		initialVal_DA = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);
		if (EN_DA2 & initialVal_DA)
			return 0;

		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA2_REG,
							SLOT09_FOR_DA_PATH, 0);
		if (0 != error) {
			dev_err(dev,
				"Data sent to DA2 from Slot 09 %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_MUXES_REG1, DA2_TO_HSR,
									0);
		if (0 != error) {
			dev_err(dev,
				"DA_IN2 path mixed with sidetone FIR %d",
									error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_DA_CHANNELS_ENABLE_REG,
								EN_DA2, 0);
		if (0 != error) {
			dev_err(dev, "Power up HSR %d ", error);
			return error;
		}

					/* Power Up HSR DAC driver */
		error = HW_ACODEC_MODIFY_WRITE(ADC_DAC_ENABLE_REG,
							POWER_UP_HSR_DAC, 0);
		if (0 != error) {
			dev_err(dev, "Power Up HSR DAC driver %d", error);
			return error;
		}

			/* Power up HSR DAC and digital path */
		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_OUTPUT_ENABLE_REG,
							EN_HSR_MASK, 0);
		if (0 != error) {
			dev_err(dev,
				"Power up HSR DAC and digital path %d",
									error);
			return error;
		}

	/*
	* TEST START .havent cleared the bits in power down.Disable short
	* detection. Pull Down output to ground, Use local oscillator,
	* Gain change without zero cross control
	*/

		error = HW_ACODEC_MODIFY_WRITE(SHORT_CIRCUIT_DISABLE_REG,
			HS_SHORT_DIS|HS_PULL_DOWN_EN|HS_OSC_EN|HS_ZCD_DIS, 0);
		if (0 != error) {
			dev_err(dev, "Disable short detection."
			"Pull Down output to ground, Use local oscillator,"
			"Gain change without zero cross control %d", error);
			return error;
		}
	/* TEST END */
	}
	ste_audio_io_unmute_headset(channel_index, 0, dev);
	dump_acodec_registers(__func__, dev);
	return error;
}

/**
 * @brief Power down headset on a specific channel
 * @channel_index Channel-index of headset
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_power_down_headset(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal_DA = 0;
	unsigned long end_time;

		/* Check if HS Power Down request is mono or Stereo channel */
	if (!(channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		dev_err(dev, "HS should have mono or stereo channels");
		return -EINVAL;
	}

	/* Disable Negative Charge Pump */
	error = HW_ACODEC_MODIFY_WRITE(NCP_ENABLE_HS_AUTOSTART_REG,
						(EN_NEG_CP|HS_AUTO_EN), 0);
	if (0 != error) {
		dev_err(dev, "NCP not fully controlled with EnCpHs bit %d",
								error);
		return error;
	}
	error = HW_ACODEC_MODIFY_WRITE(NCP_ENABLE_HS_AUTOSTART_REG, 0,
						EN_NEG_CP);
	if (0 != error) {
		dev_err(dev, "Disable Negative Charge Pump %d", error);
		return error;
	}

	/* Wait for negative charge pump to stop */
	end_time = jiffies + msecs_to_jiffies(NCP_TIMEOUT);
	while ((HW_REG_READ(IRQ_STATUS_MSB_REG) & NCP_READY_MASK)
				&& time_after_eq(end_time, jiffies)) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(1);
	}

	if (HW_REG_READ(IRQ_STATUS_MSB_REG) & NCP_READY_MASK) {
		error = -EFAULT;
		dev_err(dev, "Negative Charge Pump stop error % d", error);
		return error;
	}

	if (channel_index & e_CHANNEL_1) {
		initialVal_DA = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);
		if (!(initialVal_DA & EN_DA1))
			return 0;

		/* Power Down HSL driver */
		error = HW_ACODEC_MODIFY_WRITE(ANALOG_OUTPUT_ENABLE_REG, 0,
								EN_HSL_MASK);
		if (0 != error) {
			dev_err(dev, "Power down HSL Driver %d", error);
			return error;
		}

		/* Power Down HSL DAC driver */
		error = HW_ACODEC_MODIFY_WRITE(ADC_DAC_ENABLE_REG, 0,
							POWER_UP_HSL_DAC);
		if (0 != error) {
			dev_err(dev, "Power Up HSL DAC Driver %d", error);
			return error;
		}

		/* Power Down HSL DAC and digital path */
		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_OUTPUT_ENABLE_REG, 0,
								EN_HSL_MASK);
		if (0 != error) {
			dev_err(dev,
				"Power down HSL DAC and digital path %d",
									error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_DA_CHANNELS_ENABLE_REG,
						0, EN_DA1);
		if (0 != error) {
			dev_err(dev, "Disable DA1 %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_MUXES_REG1,
					0, DA1_TO_HSL);
		if (0 != error) {
			dev_err(dev,
			"Clear DA_IN1 path mixed with sidetone FIR %d",
									error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA1_REG, 0,
							SLOT08_FOR_DA_PATH);
		if (0 != error) {
			dev_err(dev,
				"Data sent to DA1 cleared from Slot 08 %d",
									error);
			return error;
		}


	}
	/* Enable DA2 for HSR */

	if (channel_index & e_CHANNEL_2) {
		initialVal_DA = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);
		if (!(initialVal_DA & EN_DA2))
			return 0;

		/* Power Down HSR driver */
		error = HW_ACODEC_MODIFY_WRITE(ANALOG_OUTPUT_ENABLE_REG, 0,
								EN_HSR_MASK);
		if (0 != error) {
			dev_err(dev, "Power down HSR Driver %d", error);
			return error;
		}

		/* Power Down HSR DAC driver */
		error = HW_ACODEC_MODIFY_WRITE(ADC_DAC_ENABLE_REG, 0,
							POWER_UP_HSR_DAC);
		if (0 != error) {
			dev_err(dev, "Power down HSR DAC Driver %d", error);
			return error;
		}

			/* Power Down HSR DAC and digital path */
		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_OUTPUT_ENABLE_REG, 0,
								EN_HSR_MASK);
		if (0 != error) {
			dev_err(dev,
				"Power down HSR DAC and digital path %d",
									error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_DA_CHANNELS_ENABLE_REG,
								0, EN_DA2);
		if (0 != error) {
			dev_err(dev, "Disable DA2 %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_MUXES_REG1, 0,
								DA2_TO_HSR);
		if (0 != error) {
			dev_err(dev,
			"Clear DA_IN2 path mixed with sidetone FIR %d",
									error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA2_REG, 0,
							SLOT09_FOR_DA_PATH);
		if (0 != error) {
			dev_err(dev,
				"Data sent to DA2 cleared from Slot 09 %d",
									error);
			return error;
		}

	}
	dump_acodec_registers(__func__, dev);
	return error;
}

/**
 * @brief Mute headset on a specific channel
 * @channel_index Headeset channel-index
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_mute_headset(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
		/* Check if HS Mute request is mono or Stereo channel */
	if (!(channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		dev_err(dev, "HS should have mono or stereo channels");
		return -EINVAL;
	}

	if (channel_index & e_CHANNEL_1) {
		/* Mute HSL */
		error = HW_ACODEC_MODIFY_WRITE(MUTE_HS_EAR_REG,
					       EN_HSL_MASK | EN_HSL_DAC_MASK,
					       0);
		if (0 != error) {
			dev_err(dev, "Mute HSL %d", error);
			return error;
		}
	}

	if (channel_index & e_CHANNEL_2) {
		/* Mute HSR */
		error = HW_ACODEC_MODIFY_WRITE(MUTE_HS_EAR_REG,
					       EN_HSR_MASK | EN_HSR_DAC_MASK,
					       0);
		if (0 != error) {
			dev_err(dev, "Mute HSR %d", error);
			return error;
		}
	}

	dump_acodec_registers(__func__, dev);
	return error;
}

/**
 * @brief Unmute headset on a specific channel
 * @channel_index Headeset channel-index
 * @gain Gain index of headset
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_unmute_headset(enum AUDIOIO_CH_INDEX channel_index, int *gain,
					struct device *dev)
{
	int error = 0;

	/* Check if HS UnMute request is mono or Stereo channel */
	if (!(channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		dev_err(dev, "HS should have mono or stereo channels");
		return -EINVAL;
	}

	if (channel_index & e_CHANNEL_1) {
		/* UnMute HSL */
		error = HW_ACODEC_MODIFY_WRITE(MUTE_HS_EAR_REG, 0,
					       EN_HSL_MASK | EN_HSL_DAC_MASK);
		if (0 != error) {
			dev_err(dev, "UnMute HSL %d", error);
			return error;
		}
	}

	if (channel_index & e_CHANNEL_2) {
		/* UnMute HSR */
		error = HW_ACODEC_MODIFY_WRITE(MUTE_HS_EAR_REG, 0,
					       EN_HSR_MASK | EN_HSR_DAC_MASK);
		if (0 != error) {
			dev_err(dev, "UnMute HSR %d", error);
			return error;
		}
	}
	dump_acodec_registers(__func__, dev);
	return error;
}

/**
 * @brief Enables fading of headset on a specific channel
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_enable_fade_headset(struct device *dev)
{
	int error = 0;

	error = HW_ACODEC_MODIFY_WRITE(SHORT_CIRCUIT_DISABLE_REG,
						0, DIS_HS_FAD);
	if (0 != error) {
		dev_err(dev, "Enable fading for HS %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(DA1_DIGITAL_GAIN_REG, 0, DIS_FADING);
	if (0 != error) {
		dev_err(dev, "Enable fading for HSL %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(HSL_EAR_DIGITAL_GAIN_REG, 0,
							DIS_DIG_GAIN_FADING);
	if (0 != error) {
		dev_err(dev, "Enable fading for Digital Gain of HSL %d",
									error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(DA2_DIGITAL_GAIN_REG, 0, DIS_FADING);
	if (0 != error) {
		dev_err(dev, "Enable fading for HSR %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(HSR_DIGITAL_GAIN_REG, 0,
							DIS_DIG_GAIN_FADING);
	if (0 != error) {
		dev_err(dev, "Enable fading for Digital Gain of HSR %d",
									error);
		return error;
	}

	return error;
}
/**
 * @brief Disables fading of headset on a specific channel
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_disable_fade_headset(struct device *dev)
{
	int error = 0;
	error = HW_ACODEC_MODIFY_WRITE(SHORT_CIRCUIT_DISABLE_REG,
						DIS_HS_FAD, 0);
	if (0 != error) {
		dev_err(dev, "Disable fading for HS %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(DA1_DIGITAL_GAIN_REG, DIS_FADING, 0);
	if  (0 != error) {
		dev_err(dev, "Disable fading for HSL %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(HSL_EAR_DIGITAL_GAIN_REG,
						DIS_DIG_GAIN_FADING, 0);
	if (0 != error) {
		dev_err(dev, "Disable fading for Digital Gain of HSL %d",
									error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(DA2_DIGITAL_GAIN_REG, DIS_FADING, 0);
	if (0 != error) {
		dev_err(dev, "Disable fading for HSR %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(HSR_DIGITAL_GAIN_REG,
						DIS_DIG_GAIN_FADING, 0);
	if (0 != error) {
		dev_err(dev, "Disable fading for Digital Gain of HSR %d",
									error);
		return error;
	}
	return error;
}
/**
 * @brief Power up earpiece
 * @channel_index Channel-index
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_power_up_earpiece(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal_DA = 0;

	/* Check if Earpiece PowerUp request is mono channel */
	if (!(channel_index & e_CHANNEL_1)) {
		dev_err(dev, "EARPIECE should have mono channel");
		return -EINVAL;
	}

	initialVal_DA = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);

	/* Check if Earpiece is already powered up or DA1 being used by HS */
	if (EN_DA1 & initialVal_DA)
		return 0;

	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_MUXES_REG1,
							DA1_TO_HSL, 0);
	if (0 != error) {
		dev_err(dev,
			"DA_IN1 path mixed with sidetone FIR %d", error);
		return error;
	}

	/* Enable DA1 */
	error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA1_REG,
							SLOT08_FOR_DA_PATH, 0);
	if (0 != error) {
		dev_err(dev, "Data sent to DA1 from Slot 08 %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_DA_CHANNELS_ENABLE_REG,
				EN_DA1, 0);
	if (0 != error) {
		dev_err(dev, "Enable DA1 %d", error);
		return error;
	}

	/* Power Up EAR class-AB driver */
	error = HW_ACODEC_MODIFY_WRITE(ANALOG_OUTPUT_ENABLE_REG,
					EN_EAR_MASK, 0);
	if (0 != error) {
		dev_err(dev, "Power Up EAR class-AB driver %d", error);
		return error;
	}

	/* Power up EAR DAC and digital path */
	error = HW_ACODEC_MODIFY_WRITE(
				DIGITAL_OUTPUT_ENABLE_REG, EN_EAR_MASK, 0);
	if (0 != error) {
		dev_err(dev, "Power up EAR DAC and digital path %d", error);
		return error;
	}
	dump_acodec_registers(__func__, dev);
	return error;
}
/**
 * @brief Power down earpiece
 * @channel_index Channel-index
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_power_down_earpiece(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal_DA = 0;

	/* Check if Earpiece PowerDown request is mono channel */
	if (!(channel_index & e_CHANNEL_1)) {
		dev_err(dev, "EARPIECE should have mono channel");
		return -EINVAL;
	}

	/* Check if Earpiece is already powered down or DA1 being used by HS */
	initialVal_DA = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);
	if (!(initialVal_DA & EN_DA1))
		return 0;

	/* Power Down EAR class-AB driver */
	error = HW_ACODEC_MODIFY_WRITE(ANALOG_OUTPUT_ENABLE_REG,
						0, EN_EAR_MASK);
	if (0 != error) {
		dev_err(dev, "Power Down EAR class-AB driver %d", error);
		return error;
	}

		/* Power Down EAR DAC and digital path */
	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_OUTPUT_ENABLE_REG,
					0, EN_EAR_MASK);
	if (0 != error) {
		dev_err(dev,
			"Power Down EAR DAC and digital path %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_MUXES_REG1, 0, DA1_TO_HSL);
	if (0 != error) {
		dev_err(dev,
			"Clear DA_IN1 path mixed with sidetone FIR %d",
									error);
		return error;
	}

	/* Disable DA1 */
	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_DA_CHANNELS_ENABLE_REG,
						0, EN_DA1);
	if (0 != error) {
		dev_err(dev, "Disable DA1 %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA1_REG, 0,
							SLOT08_FOR_DA_PATH);
	if (0 != error) {
		dev_err(dev,
			"Data sent to DA1 cleared from Slot 08 %d", error);
		return error;
	}
	dump_acodec_registers(__func__, dev);
	return error;
}
/**
 * @brief Mute earpiece
 * @channel_index Channel-index
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_mute_earpiece(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;

	/* Check if Earpiece Mute request is mono channel */
	if (!(channel_index & e_CHANNEL_1)) {
		dev_err(dev, "EARPIECE should have mono channel");
		return -EINVAL;
	}

	/* Mute Earpiece */
	error = HW_ACODEC_MODIFY_WRITE(MUTE_HS_EAR_REG,
				       EN_EAR_MASK | EN_EAR_DAC_MASK, 0);
	if (0 != error) {
		dev_err(dev, "Mute Earpiece %d", error);
		return error;
	}
	dump_acodec_registers(__func__, dev);
	return error;
}
/**
 * @brief Unmute earpiece
 * @channel_index Channel-index
 * @gain Gain index of earpiece
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_unmute_earpiece(enum AUDIOIO_CH_INDEX channel_index, int *gain,
					struct device *dev)
{
	int error = 0;

	/* Check if Earpiece UnMute request is mono channel */
	if (!(channel_index & e_CHANNEL_1)) {
		dev_err(dev, "EARPIECE should have mono channel");
		return -EINVAL;
	}

	/* UnMute Earpiece */
	error = HW_ACODEC_MODIFY_WRITE(MUTE_HS_EAR_REG, 0,
				       EN_EAR_MASK | EN_EAR_DAC_MASK);
	if (0 != error) {
		dev_err(dev, "UnMute Earpiece %d", error);
		return error;
	}
	dump_acodec_registers(__func__, dev);
	return error;
}
/**
 * @brief Enables fading of earpiece
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_enable_fade_earpiece(struct device *dev)
{
	int error = 0;

	error = HW_ACODEC_MODIFY_WRITE(DA1_DIGITAL_GAIN_REG, 0, DIS_FADING);
	if (0 != error) {
		dev_err(dev, "Enable fading for Ear %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(HSL_EAR_DIGITAL_GAIN_REG, 0,
							DIS_DIG_GAIN_FADING);
	if (0 != error) {
		dev_err(dev,
			"Enable fading for Digital Gain of Ear %d", error);
		return error;
	}

	return error;
}
/**
 * @brief Disables fading of earpiece
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_disable_fade_earpiece(struct device *dev)
{
	int error = 0;
	error = HW_ACODEC_MODIFY_WRITE(DA1_DIGITAL_GAIN_REG, DIS_FADING, 0);
	if (0 != error) {
		dev_err(dev, "Disable fading for Ear %d", error);
		return error;
	}
	return error;
}
/**
 * @brief Power up IHF on a specific channel
 * @channel_index Channel-index
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_power_up_ihf(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal_DA = 0;

	/* Check if IHF PowerUp request is mono or Stereo channel */
	if (!(channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		dev_err(dev, "IHF should have mono or stereo channels");
		return -EINVAL;
	}

	if (channel_index & e_CHANNEL_1) {
		initialVal_DA = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);
		if (EN_DA3 & initialVal_DA)
			return 0;

		/* Enable DA3 for IHFL */
		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA3_REG,
							SLOT10_FOR_DA_PATH, 0);
		if (0 != error) {
			dev_err(dev, "Data sent to DA3 from Slot 10 %d",
									error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_DA_CHANNELS_ENABLE_REG,
								EN_DA3, 0);
		if (0 != error) {
			dev_err(dev, "Power up IHFL %d", error);
			return error;
		}

		/* Power Up HFL Class-D driver */
		error = HW_ACODEC_MODIFY_WRITE(ANALOG_OUTPUT_ENABLE_REG,
								EN_HFL_MASK, 0);
		if (0 != error) {
			dev_err(dev, "Power Up HFL Class-D Driver %d", error);
			return error;
		}

		/* Power up HFL Class D driver and digital path */
		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_OUTPUT_ENABLE_REG,
								EN_HFL_MASK, 0);
		if (0 != error) {
			dev_err(dev,
			"Power up HFL Class D driver & digital path %d",
									error);
			return error;
		}
	}

	/* Enable DA4 for IHFR */
	if (channel_index & e_CHANNEL_2) {
		initialVal_DA = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);
		if (EN_DA4 & initialVal_DA)
			return 0;

		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA4_REG,
							SLOT11_FOR_DA_PATH, 0);
		if (0 != error) {
			dev_err(dev, "Data sent to DA4 from Slot 11 %d",
									error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_DA_CHANNELS_ENABLE_REG,
								EN_DA4, 0);
		if (0 != error) {
			dev_err(dev, "Enable DA4 %d", error);
			return error;
		}

		/* Power Up HFR Class-D driver */
		error = HW_ACODEC_MODIFY_WRITE(ANALOG_OUTPUT_ENABLE_REG,
								EN_HFR_MASK, 0);
		if (0 != error) {
			dev_err(dev, "Power Up HFR Class-D Driver %d", error);
			return error;
		}

		/* Power up HFR Class D driver and digital path */
		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_OUTPUT_ENABLE_REG,
							EN_HFR_MASK, 0);
		if (0 != error) {
			dev_err(dev,
			"Power up HFR Class D driver and digital path %d",
									error);
			return error;
		}
	}
	dump_acodec_registers(__func__, dev);
	return error;
}
/**
 * @brief Power down IHF on a specific channel
 * @channel_index Channel-index
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_power_down_ihf(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal_DA = 0;

		/* Check if IHF Power Down request is mono or Stereo channel */
	if (!(channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		dev_err(dev, "IHF should have mono or stereo channels");
		return -EINVAL;
	}

	if (channel_index & e_CHANNEL_1) {
		initialVal_DA = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);
		if (!(initialVal_DA & EN_DA3))
			return 0;

		/* Power Down HFL Class-D driver */
		error = HW_ACODEC_MODIFY_WRITE(ANALOG_OUTPUT_ENABLE_REG, 0,
								EN_HFL_MASK);
		if (0 != error) {
			dev_err(dev, "Power Down HFL Class-D Driver %d",
									error);
			return error;
		}

		/* Power Down HFL Class D driver and digital path */
		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_OUTPUT_ENABLE_REG, 0,
								EN_HFL_MASK);
		if (0 != error) {
			dev_err(dev,
			"Power Down HFL Class D driver & digital path %d",
									error);
			return error;
		}

		/* Disable DA3 for IHFL */
		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_DA_CHANNELS_ENABLE_REG,
								0, EN_DA3);
		if (0 != error) {
			dev_err(dev, "Disable DA3 %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA3_REG, 0,
							SLOT10_FOR_DA_PATH);
		if (0 != error) {
			dev_err(dev,
				"Data sent to DA3 cleared from Slot 10 %d",
									error);
			return error;
		}
	}

	if (channel_index & e_CHANNEL_2) {
		initialVal_DA = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);

		/* Check if IHF is already powered Down */
		if (!(initialVal_DA & EN_DA4))
			return 0;

		/* Power Down HFR Class-D Driver */
		error = HW_ACODEC_MODIFY_WRITE(ANALOG_OUTPUT_ENABLE_REG, 0,
								EN_HFR_MASK);
		if (0 != error) {
			dev_err(dev, "Power Down HFR Class-D Driver %d",
									error);
			return error;
		}

		/* Power Down HFR Class D driver and digital path */
		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_OUTPUT_ENABLE_REG, 0,
								EN_HFR_MASK);
		if (0 != error) {
			dev_err(dev,
			"Power Down HFR Class D driver & digital path %d",
									error);
			return error;
		}

			/* Disable DA4 for IHFR */
		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_DA_CHANNELS_ENABLE_REG,
								0, EN_DA4);
		if (0 != error) {
			dev_err(dev, "Disable DA4 %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA4_REG, 0,
							SLOT11_FOR_DA_PATH);
		if (0 != error) {
			dev_err(dev,
				"Data sent to DA4 cleared from Slot 11 %d",
									error);
			return error;
		}
	}
	dump_acodec_registers(__func__, dev);
	return error;
}
/**
 * @brief Mute IHF on a specific channel
 * @channel_index Channel-index
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_mute_ihf(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;

	if ((channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		error = ste_audio_io_set_ihf_gain(channel_index, 0, -63,
								0, dev);
		if (0 != error) {
			dev_err(dev, "Mute ihf %d", error);
			return error;
		}
	}
	dump_acodec_registers(__func__, dev);
	return error;
}
/**
 * @brief Unmute IHF on a specific channel
 * @channel_index Channel-index
 * @gain Gain index of IHF
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_unmute_ihf(enum AUDIOIO_CH_INDEX channel_index, int *gain,
					struct device *dev)
{
	int error = 0;

	if (channel_index & e_CHANNEL_1) {
		error = ste_audio_io_set_ihf_gain(channel_index, 0, gain[0],
									0, dev);
		if (0 != error) {
			dev_err(dev, "UnMute ihf %d", error);
			return error;
		}
		}

	if (channel_index & e_CHANNEL_2) {
		error = ste_audio_io_set_ihf_gain(channel_index, 0, gain[1],
									0, dev);
		if (0 != error) {
			dev_err(dev, "UnMute ihf %d", error);
			return error;
		}
	}
	dump_acodec_registers(__func__, dev);
	return error;
}
/**
 * @brief Enable fading of IHF
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_enable_fade_ihf(struct device *dev)
{
	int error = 0;

	error = HW_ACODEC_MODIFY_WRITE(DA3_DIGITAL_GAIN_REG, 0, DIS_FADING);
	if (0 != error) {
		dev_err(dev, "Enable fading for HFL %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(DA4_DIGITAL_GAIN_REG, 0, DIS_FADING);
	if (0 != error) {
		dev_err(dev, "Enable fading for HFR %d", error);
		return error;
	}
	return error;
}
/**
 * @brief Disable fading of IHF
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_disable_fade_ihf(struct device *dev)
{
	int error = 0;

	error = HW_ACODEC_MODIFY_WRITE(DA3_DIGITAL_GAIN_REG, DIS_FADING, 0);
	if (0 != error) {
		dev_err(dev, "Disable fading for HFL %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(DA4_DIGITAL_GAIN_REG, DIS_FADING, 0);
	if (0 != error) {
		dev_err(dev, "Disable fading for HFR %d", error);
		return error;
	}
	return error;
}
/**
 * @brief Power up VIBL
 * @channel_index Channel-index of VIBL
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_power_up_vibl(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal_DA = 0;

	/* Check if VibL PowerUp request is mono channel */
	if (!(channel_index & e_CHANNEL_1)) {
		dev_err(dev, "VibL should have mono channel");
		return -EINVAL;
	}

	/* Try to allocate vibrator for audio left channel */
	error = ste_audioio_vibrator_alloc(STE_AUDIOIO_CLIENT_AUDIO_L,
		STE_AUDIOIO_CLIENT_AUDIO_R | STE_AUDIOIO_CLIENT_AUDIO_L);
	if (error) {
		dev_err(dev, "  Couldn't allocate vibrator %d, client %d",
			error, STE_AUDIOIO_CLIENT_AUDIO_L);
		return error;
	}

	initialVal_DA = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);

	/* Check if VibL is already powered up */
	if (initialVal_DA & EN_DA5)
		return 0;

	/* Enable DA5 for vibl */
	error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA5_REG,
							SLOT12_FOR_DA_PATH, 0);
	if (0 != error) {
		dev_err(dev, "Data sent to DA5 from Slot 12 %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_DA_CHANNELS_ENABLE_REG,
					EN_DA5, 0);
	if (0 != error) {
		dev_err(dev, "Enable DA5 for VibL %d", error);
		return error;
	}

	/* Power Up VibL Class-D driver */
	error = HW_ACODEC_MODIFY_WRITE(
				ANALOG_OUTPUT_ENABLE_REG, EN_VIBL_MASK, 0);
	if (0 != error) {
		dev_err(dev, "Power Up VibL Class-D Driver %d", error);
		return error;
	}

	/* Power up VibL Class D driver and digital path */
	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_OUTPUT_ENABLE_REG,
						EN_VIBL_MASK, 0);
	if (0 != error) {
		dev_err(dev,
			"Power up VibL Class D driver and digital path %d",
									error);
		return error;
	}
	return error;
}
/**
 * @brief Power down VIBL
 * @channel_index Channel-index of VIBL
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_power_down_vibl(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal_DA = 0;

	/* Check if VibL Power Down request is mono channel */
	if (!(channel_index & e_CHANNEL_1)) {
		dev_err(dev, "VibL should have mono channel");
		return -EINVAL;
	}

	initialVal_DA = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);

	/* Check if VibL is already powered down */
	if (!(initialVal_DA & EN_DA5))
		return 0;


	/* Power Down VibL Class-D driver */
	error = HW_ACODEC_MODIFY_WRITE(ANALOG_OUTPUT_ENABLE_REG,
					0, EN_VIBL_MASK);
	if (0 != error)	{
		dev_err(dev, "Power Down VibL Class-D Driver %d", error);
		return error;
	}

	/* Power Down VibL Class D driver and digital path */
	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_OUTPUT_ENABLE_REG, 0,
								EN_VIBL_MASK);
	if (0 != error) {
		dev_err(dev,
			"Power Down VibL Class D driver & digital path %d",
									error);
		return error;
	}

	/* Disable DA5 for VibL */
	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_DA_CHANNELS_ENABLE_REG,
					0, EN_DA5);
	if (0 != error) {
		dev_err(dev, "Disable DA5 for VibL %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA5_REG, 0,
							SLOT12_FOR_DA_PATH);
	if (0 != error) {
		dev_err(dev,
			"Data sent to DA5 cleared from Slot 12 %d", error);
		return error;
	}

	/* Release vibrator */
	ste_audioio_vibrator_release(STE_AUDIOIO_CLIENT_AUDIO_L);

	return error;
}
/**
 * @brief Enable fading of VIBL
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_enable_fade_vibl(struct device *dev)
{
	int error = 0;

	error = HW_ACODEC_MODIFY_WRITE(DA5_DIGITAL_GAIN_REG, 0, DIS_FADING);
	if (0 != error) {
		dev_err(dev, "Enable fading for VibL %d", error);
		return error;
	}
	return error;
}
/**
 * @brief Disable fading of VIBL
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_disable_fade_vibl(struct device *dev)
{
	int error = 0;

	error = HW_ACODEC_MODIFY_WRITE(DA5_DIGITAL_GAIN_REG, DIS_FADING, 0);
	if (0 != error) {
		dev_err(dev, "Disable fading for VibL %d", error);
		return error;
	}
	return error;
}
/**
 * @brief Power up VIBR
 * @channel_index Channel-index of VIBR
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_power_up_vibr(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal_DA = 0;

	/* Check if VibR PowerUp request is mono channel */
	if (!(channel_index & e_CHANNEL_1)) {
		dev_err(dev, "VibR should have mono channel");
		return -EINVAL;
	}

	/* Try to allocate vibrator for audio right channel */
	error = ste_audioio_vibrator_alloc(STE_AUDIOIO_CLIENT_AUDIO_R,
		STE_AUDIOIO_CLIENT_AUDIO_R | STE_AUDIOIO_CLIENT_AUDIO_L);
	if (error) {
		dev_err(dev, "  Couldn't allocate vibrator %d, client %d",
			error, STE_AUDIOIO_CLIENT_AUDIO_R);
		return error;
	}

	initialVal_DA = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);

	/* Check if VibR is already powered up */
	if (initialVal_DA & EN_DA6)
		return 0;

	/* Enable DA6 for vibr */
	error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA6_REG,
							SLOT13_FOR_DA_PATH, 0);
	if (0 != error) {
		dev_err(dev, "Data sent to DA5 from Slot 13 %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(
				DIGITAL_DA_CHANNELS_ENABLE_REG, EN_DA6, 0);
	if (0 != error) {
		dev_err(dev, "Enable DA6 for VibR %d", error);
		return error;
	}

	/* Power Up VibR Class-D driver */
	error = HW_ACODEC_MODIFY_WRITE(
				ANALOG_OUTPUT_ENABLE_REG, EN_VIBR_MASK, 0);
	if (0 != error) {
		dev_err(dev, "Power Up VibR Class-D Driver %d", error);
		return error;
	}

	/* Power up VibR Class D driver and digital path */
	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_OUTPUT_ENABLE_REG,
							EN_VIBR_MASK, 0);
	if (0 != error) {
		dev_err(dev,
			"Power up VibR Class D driver & digital path %d",
									error);
		return error;
	}
	return error;
}
/**
 * @brief Power down VIBR
 * @channel_index Channel-index of VIBR
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_power_down_vibr(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal_DA = 0;

	/* Check if VibR PowerDown request is mono channel */
	if (!(channel_index & e_CHANNEL_1)) {
		dev_err(dev, "VibR should have mono channel");
		return -EINVAL;
	}

	initialVal_DA = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);

	/* Check if VibR is already powered down */
	if (!(initialVal_DA & EN_DA6))
		return 0;


	/* Power Down VibR Class-D driver */
	error = HW_ACODEC_MODIFY_WRITE(ANALOG_OUTPUT_ENABLE_REG, 0,
							EN_VIBR_MASK);
	if (0 != error) {
		dev_err(dev, "Power Down VibR Class-D Driver %d", error);
		return error;
	}

	/* Power Down VibR Class D driver and digital path */
	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_OUTPUT_ENABLE_REG, 0,
								EN_VIBR_MASK);
	if (0 != error) {
		dev_err(dev,
			"Power Down VibR Class D driver & digital path %d",
									error);
		return error;
	}

	/* Disable DA6 for VibR */
	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_DA_CHANNELS_ENABLE_REG,
					0, EN_DA6);
	if (0 != error) {
		dev_err(dev, "Disable DA6 for VibR %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA6_REG, 0,
							SLOT13_FOR_DA_PATH);
	if (0 != error) {
		dev_err(dev, "Data sent to DA5 cleared from Slot 13 %d",
									error);
		return error;
	}

	/* Release vibrator */
	ste_audioio_vibrator_release(STE_AUDIOIO_CLIENT_AUDIO_R);

	return error;
}
/**
 * @brief Enable fading of VIBR
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_enable_fade_vibr(struct device *dev)
{
	int error = 0;

	error = HW_ACODEC_MODIFY_WRITE(DA6_DIGITAL_GAIN_REG, 0, DIS_FADING);
	if (0 != error) {
		dev_err(dev, "Enable fading for VibR %d", error);
		return error;
	}
	return error;
}
/**
 * @brief Disable fading of VIBR
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_disable_fade_vibr(struct device *dev)
{
	int error = 0;

	error = HW_ACODEC_MODIFY_WRITE(DA6_DIGITAL_GAIN_REG, DIS_FADING, 0);
	if (0 != error) {
		dev_err(dev, "Disable fading for VibR %d", error);
		return error;
	}
	return error;
}
/**
 * @brief Power up MIC1A
 * @channel_index Channel-index of MIC1A
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_power_up_mic1a(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal_AD = 0;

	/* Check if Mic1 PowerUp request is mono channel */

	if (!(channel_index & e_CHANNEL_1)) {
		dev_err(dev, "MIC1 should have mono channel");
		return -EINVAL;
	}

	initialVal_AD = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);
	/* Check if Mic1 is already powered up or used by Dmic3 */
	if (EN_AD3 & initialVal_AD)
		return 0;

	error = HW_REG_WRITE(AD_ALLOCATION_TO_SLOT0_1_REG, DATA_FROM_AD_OUT3);
	if (0 != error) {
		dev_err(dev, "Slot 02 outputs data from AD_OUT3 %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_AD_CHANNELS_ENABLE_REG,
						EN_AD3, 0);
	if (0 != error) {
		dev_err(dev, "Enable AD3 for Mic1 %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_MUXES_REG1, 0,
							SEL_DMIC3_FOR_AD_OUT3);
	if (0 != error) {
		dev_err(dev, "Select ADC1 for AD_OUT3 %d", error);
		return error;
	}

	/* Select MIC1A */
	error = HW_ACODEC_MODIFY_WRITE(ADC_DAC_ENABLE_REG, 0,
							SEL_MIC1B_CLR_MIC1A);
	if (0 != error) {
		dev_err(dev, "Select MIC1A %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(LINE_IN_MIC_CONF_REG, EN_MIC1, 0);
	if (0 != error) {
		dev_err(dev, "Power up Mic1 %d", error);
		return error;
	}

	/* Power Up ADC1 */
	error = HW_ACODEC_MODIFY_WRITE(ADC_DAC_ENABLE_REG, POWER_UP_ADC1, 0);
	if (0 != error) {
		dev_err(dev, "Power Up ADC1 %d", error);
		return error;
	}

return error;
}
/**
 * @brief Power down MIC1A
 * @channel_index Channel-index of MIC1A
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_power_down_mic1a(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal_AD = 0;

	/* Check if Mic1 PowerDown request is mono channel */

	if (!(channel_index & e_CHANNEL_1)) {
		dev_err(dev, "Mic1 should have mono channel");
		return -EINVAL;
	}

	initialVal_AD = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);
	/* Check if Mic1 is already powered down or used by Dmic3 */
	if (!(initialVal_AD & EN_AD3))
		return 0;

	error = HW_ACODEC_MODIFY_WRITE(LINE_IN_MIC_CONF_REG, 0, EN_MIC1);
	if (0 != error) {
		dev_err(dev, "Power Down Mic1 %d", error);
		return error;
	}

		/* Power Down ADC1 */
	error = HW_ACODEC_MODIFY_WRITE(ADC_DAC_ENABLE_REG, 0, POWER_UP_ADC1);
	if (0 != error) {
		dev_err(dev, "Power Down ADC1 %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_AD_CHANNELS_ENABLE_REG,
						0, EN_AD3);
	if (0 != error) {
		dev_err(dev, "Disable AD3 for Mic1 %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(AD_ALLOCATION_TO_SLOT2_3_REG, 0,
							DATA_FROM_AD_OUT3);
	if (0 != error) {
		dev_err(dev, "Slot 02 outputs data cleared from AD_OUT3 %d",
									error);
		return error;
	}
	return error;
}
/**
 * @brief Mute MIC1A
 * @channel_index Channel-index of MIC1A
 * @return 0 on success otherwise negative error code
 */


int ste_audio_io_mute_mic1a(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	if (!(channel_index & e_CHANNEL_1)) {
		dev_err(dev, "MIC1 should have mono channel");
		return -EINVAL;
	}

	/* Mute mic1 */
	error = HW_ACODEC_MODIFY_WRITE(LINE_IN_MIC_CONF_REG, MUT_MIC1, 0);
	if (0 != error) {
		dev_err(dev, "Mute Mic1 %d", error);
		return error;
	}
	return error;
}
/**
 * @brief Unmute MIC1A
 * @channel_index Channel-index of MIC1A
 * @gain Gain index of MIC1A
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_unmute_mic1a(enum AUDIOIO_CH_INDEX channel_index, int *gain,
					struct device *dev)
{
	int error = 0;
	if (!(channel_index & e_CHANNEL_1)) {
		dev_err(dev, "Mic1 should have mono channel");
		return -EINVAL;
	}
	/* UnMute mic1 */
	error = HW_ACODEC_MODIFY_WRITE(LINE_IN_MIC_CONF_REG, 0, MUT_MIC1);
	if (0 != error) {
		dev_err(dev, "UnMute Mic1 %d", error);
		return error;
	}
	return error;
}
/**
 * @brief Enable fading of MIC1A
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_enable_fade_mic1a(struct device *dev)
{
	int error = 0;

	error = HW_ACODEC_MODIFY_WRITE(AD3_DIGITAL_GAIN_REG, 0, DIS_FADING);
	if (0 != error) {
		dev_err(dev, "Enable fading for Mic1 %d", error);
		return error;
	}
	return error;
}
/**
 * @brief Disable fading of MIC1A
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_disable_fade_mic1a(struct device *dev)
{
	int error = 0;

	error = HW_ACODEC_MODIFY_WRITE(AD3_DIGITAL_GAIN_REG, DIS_FADING, 0);
	if (0 != error) {
		dev_err(dev, "Disable fading for Mic1 %d", error);
		return error;
	}
	return error;
}
/**
 * @brief Power up MIC1B
 * @channel_index Channel-index of MIC1B
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_power_up_mic1b(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error;
	unsigned char initialVal_AD = 0;

	error = regulator_enable(regulator_avsource);
	if (0 != error) {
		dev_err(dev, "regulator avsource enable failed = %d", error);
		return error;
	}
	/* GPIO35 settings to enable MIC 1B input instead of TVOUT */
	error = HW_ACODEC_MODIFY_WRITE(AB8500_GPIO_DIR5_REG,
						GPIO35_DIR_OUTPUT, 0);
	if (0 != error) {
		dev_err(dev, "setting AB8500_GPIO_DIR5_REG reg %d", error);
		return error;
	}
	error = HW_ACODEC_MODIFY_WRITE(AB8500_GPIO_OUT5_REG,
						GPIO35_DIR_OUTPUT, 0);
	if (0 != error) {
		dev_err(dev, "setting AB8500_GPIO_OUT5_REG reg %d", error);
		return error;
	}

	if (!(channel_index & e_CHANNEL_1)) {
		dev_err(dev, "Mic1 should have mono channel");
		return -EINVAL;
	}

	initialVal_AD = HW_REG_READ(DIGITAL_AD_CHANNELS_ENABLE_REG);
	/* Check if Mic1 is already powered up or used by Dmic3 */
	if (EN_AD3 & initialVal_AD)
		return 0;

	error = HW_REG_WRITE(AD_ALLOCATION_TO_SLOT0_1_REG, DATA_FROM_AD_OUT3);
	if (0 != error) {
		dev_err(dev, "Slot 02 outputs data from AD_OUT3 %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_AD_CHANNELS_ENABLE_REG,
						EN_AD3, 0);
	if (0 != error) {
		dev_err(dev, "Enable AD3 for Mic1 %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_MUXES_REG1, 0,
							SEL_DMIC3_FOR_AD_OUT3);
	if (0 != error) {
		dev_err(dev, "Select ADC1 for AD_OUT3 %d", error);
		return error;
	}

	/* Select MIC1B */
	error = HW_ACODEC_MODIFY_WRITE(ADC_DAC_ENABLE_REG, SEL_MIC1B_CLR_MIC1A,
									0);
	if (0 != error) {
		dev_err(dev, "Select MIC1B %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(LINE_IN_MIC_CONF_REG, EN_MIC1, 0);
	if (0 != error) {
		dev_err(dev, "Power up Mic1 %d", error);
		return error;
	}

	/* Power Up ADC1 */
	error = HW_ACODEC_MODIFY_WRITE(ADC_DAC_ENABLE_REG, POWER_UP_ADC1, 0);
	if (0 != error) {
		dev_err(dev, "Power Up ADC1 %d", error);
		return error;
	}
	return error;
}
/**
 * @brief Power down MIC1B
 * @channel_index Channel-index of MIC1B
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_power_down_mic1b(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error;
	unsigned char initialVal_AD = 0;

	/* Check if Mic1 PowerDown request is mono channel */
	if (!(channel_index & e_CHANNEL_1)) {
		dev_err(dev, "Mic1 should have mono channel");
		return -EINVAL;
	}

	initialVal_AD = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);

	/* Check if Mic1 is already powered down or used by Dmic3 */
	if (!(initialVal_AD & EN_AD3))
		return 0;


	error = HW_ACODEC_MODIFY_WRITE(LINE_IN_MIC_CONF_REG, 0, EN_MIC1);
	if (0 != error) {
		dev_err(dev, "Power Down Mic1 %d", error);
		return error;
	}

	/* Power Down ADC1 */
	error = HW_ACODEC_MODIFY_WRITE(ADC_DAC_ENABLE_REG, 0, POWER_UP_ADC1);
	if (0 != error) {
		dev_err(dev, "Power Down ADC1 %d", error);
		return error;
	}
	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_AD_CHANNELS_ENABLE_REG, 0,
			EN_AD3);
	if (0 != error) {
		dev_err(dev, "Disable AD3 for Mic1 %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(AD_ALLOCATION_TO_SLOT2_3_REG, 0,
							DATA_FROM_AD_OUT3);
	if (0 != error) {
		dev_err(dev, "Slot 02 outputs data cleared from AD_OUT3 %d",
									error);
		return error;
	}

	/* undo GPIO35 settings */
	error = HW_ACODEC_MODIFY_WRITE(AB8500_GPIO_DIR5_REG,
						0, GPIO35_DIR_OUTPUT);
	if (0 != error) {
		dev_err(dev, "resetting AB8500_GPIO_DIR5_REG reg %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(AB8500_GPIO_OUT5_REG,
						0, GPIO35_DIR_OUTPUT);
	if (0 != error) {
		dev_err(dev, "resetting AB8500_GPIO_OUT5_REG reg %d", error);
		return error;
	}

	error = regulator_disable(regulator_avsource);
	if (0 != error) {
		dev_err(dev, "regulator avsource disable failed = %d", error);
		return error;
	}
	dump_acodec_registers(__func__, dev);
	return error;
}

/**
 * @brief enable hardware loop of mic1b
 * @chnl_index Channel-index of MIC1B
 * @hw_loop type of hardware loop
 * @loop_gain gain value to be used in hardware loop
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_enable_loop_mic1b(enum AUDIOIO_CH_INDEX chnl_index,
					enum AUDIOIO_HAL_HW_LOOPS hw_loop,
					int loop_gain, struct device *dev,
					void *cookie)
{
	int error;
	struct transducer_context_t *trnsdr;
	trnsdr = (struct transducer_context_t *)cookie;

	switch (hw_loop) {
		/* Check if HSL is active */
	case AUDIOIO_SIDETONE_LOOP:
		if (!(trnsdr[HS_CH].is_power_up[e_CHANNEL_1])
			&& !(trnsdr[EAR_CH].is_power_up[e_CHANNEL_1])) {
			error = -EFAULT;
			dev_err(dev,
			"HS or Earpiece not powered up error = %d",
					error);
			return error;
		}

		/* For ch1, Power On STFIR1, data comes from AD3*/
		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_MUXES_REG2,
						FIR1_FROMAD3, 0);
		if (error)
			dev_err(dev, "FIR1 data comes from AD_OUT3 %d",
								error);
		error = HW_REG_WRITE(SIDETONE_FIR1_GAIN_REG, loop_gain);
			if (error) {
				dev_err(dev,
					"Set FIR1 Gain index = %d",
							error);
				return error;
			}
		break;
	default:
		error = -EINVAL;
		dev_err(dev, "loop not supported %d", error);
	}
	return error;
}

/**
 * @brief disable hardware loop of mic1b
 * @chnl_index Channel-index of MIC1B
 * @hw_loop type of hardware loop
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_disable_loop_mic1b(enum AUDIOIO_CH_INDEX chnl_index,
					enum AUDIOIO_HAL_HW_LOOPS hw_loop,
					struct device *dev, void *cookie)
{
	int error;
	struct transducer_context_t *trnsdr;
	trnsdr = (struct transducer_context_t *)cookie;

	switch (hw_loop) {
	/* Check if HSL is active */
	case AUDIOIO_SIDETONE_LOOP:
		if (!trnsdr[HS_CH].is_power_up[e_CHANNEL_1]
			&& !trnsdr[EAR_CH].is_power_up[e_CHANNEL_1]) {
			error = -EFAULT;
			dev_err(dev, "HS or Earpiece not powered up, err = %d",
					error);
			return error;
		}

		/* For ch1, Power down STFIR1, data comes from AD3*/
		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_MUXES_REG2,
						0, FIR1_FROMAD3);
		if (error) {
			dev_err(dev, "FIR1 data comes from AD_OUT3, err = %d",
								error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(FILTERS_CONTROL_REG,
						0, FIR_FILTERCONTROL);
		if (error) {
			dev_err(dev,
				"ST FIR Filters disable failed %d", error);
				return error;
		}
		break;
	default:
		error = -EINVAL;
		dev_err(dev, "loop not supported %d", error);
	}
	return error;
}

/**
 * @brief Power up MIC2
 * @channel_index Channel-index of MIC2
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_power_up_mic2(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal_AD = 0;

	/* Check if Mic2 PowerUp request is mono channel */
	if (!(channel_index & e_CHANNEL_1)) {
		dev_err(dev, "Mic2 should have mono channel");
		return -EINVAL;
	}

	initialVal_AD = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);

	/* Check if Mic2 is already powered up or used by LINR or Dmic2 */
	if (EN_AD2 & initialVal_AD)
		return 0;


	error = HW_REG_WRITE(AD_ALLOCATION_TO_SLOT0_1_REG, DATA_FROM_AD_OUT2);
	if (0 != error) {
		dev_err(dev, "Slot 01 outputs data from AD_OUT2 %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_AD_CHANNELS_ENABLE_REG, EN_AD2,
									0);
	if (0 != error) {
		dev_err(dev, "Enable AD2 for Mic2 %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_MUXES_REG1, 0,
							SEL_DMIC2_FOR_AD_OUT2);
	if (0 != error) {
		dev_err(dev, "Select ADC2 for AD_OUT2 %d", error);
		return error;
	}

		/* Select mic2 */
	error = HW_ACODEC_MODIFY_WRITE(ADC_DAC_ENABLE_REG, 0,
							SEL_LINR_CLR_MIC2);
	if (0 != error) {
		dev_err(dev, "Select MIC2 %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(LINE_IN_MIC_CONF_REG, EN_MIC2, 0);
	if (0 != error) {
		dev_err(dev, "Power up Mic2 %d", error);
		return error;
	}

	/* Power Up ADC1 */
	error = HW_ACODEC_MODIFY_WRITE(ADC_DAC_ENABLE_REG, POWER_UP_ADC2, 0);
	if (0 != error) {
		dev_err(dev, "Power Up ADC2 %d", error);
		return error;
	}
	return error;
}
/**
 * @brief Power down MIC2
 * @channel_index Channel-index of MIC2
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_power_down_mic2(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal_AD = 0;

	/* Check if Mic2 PowerDown request is mono channel */
	if (!(channel_index & e_CHANNEL_1)) {
		dev_err(dev, "Mic2 should have mono channel");
		return -EINVAL;
	}

	initialVal_AD = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);

	/* Check if Mic2 is already powered down or used by LINR or Dmic2 */
	if (!(initialVal_AD & EN_AD2))
		return 0;

	error = HW_ACODEC_MODIFY_WRITE(LINE_IN_MIC_CONF_REG, 0, EN_MIC2);
	if (0 != error) {
		dev_err(dev, "Power Down Mic2 %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_AD_CHANNELS_ENABLE_REG,
					0, EN_AD2);
	if (0 != error) {
		dev_err(dev, "Disable AD2 for Mic2 %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(AD_ALLOCATION_TO_SLOT0_1_REG, 0,
						(DATA_FROM_AD_OUT2<<4));
	if (0 != error) {
		dev_err(dev, "Slot 01 outputs data cleared from AD_OUT2 %d",
									error);
		return error;
	}
	return error;
}
/**
 * @brief Mute MIC2
 * @channel_index Channel-index of MIC2
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_mute_mic2(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	if (!(channel_index & e_CHANNEL_1)) {
		dev_err(dev, "Mic2 should have mono channel");
		return -EINVAL;
	}

	/* Mute mic2 */
	error = HW_ACODEC_MODIFY_WRITE(LINE_IN_MIC_CONF_REG, MUT_MIC2, 0);
	if (0 != error) {
		dev_err(dev, "Mute Mic2 %d", error);
		return error;
	}
	return error;
}
/**
 * @brief Unmute MIC2
 * @channel_index Channel-index of MIC2
 * @gain Gain index of MIC2
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_unmute_mic2(enum AUDIOIO_CH_INDEX channel_index, int *gain,
					struct device *dev)
{
	int error = 0;
	if (!(channel_index & e_CHANNEL_1)) {
		dev_err(dev, "Mic2 should have mono channel");
		return -EINVAL;
	}
	/* UnMute mic2 */
	error = HW_ACODEC_MODIFY_WRITE(LINE_IN_MIC_CONF_REG, 0, MUT_MIC2);
	if (0 != error) {
		dev_err(dev, "UnMute Mic2 %d", error);
		return error;
	}
	return error;
}
/**
 * @brief Enable fading of MIC2
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_enable_fade_mic2(struct device *dev)
{
	int error = 0;

	error = HW_ACODEC_MODIFY_WRITE(AD2_DIGITAL_GAIN_REG, 0, DIS_FADING);
	if (0 != error) {
		dev_err(dev, "Enable fading for Mic2 %d", error);
		return error;
	}
	return error;
}
/**
 * @brief Disable fading of MIC2
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_disable_fade_mic2(struct device *dev)
{
	int error = 0;

	error = HW_ACODEC_MODIFY_WRITE(AD2_DIGITAL_GAIN_REG, DIS_FADING, 0);
	if (0 != error) {
		dev_err(dev, "Disable fading for Mic2 %d", error);
		return error;
	}

	return error;
}
/**
 * @brief Power up LinIn
 * @channel_index Channel-index of LinIn
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_power_up_lin(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal_AD = 0;

	/* Check if LinIn PowerUp request is mono or Stereo channel */
	if (!(channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		dev_err(dev, "LinIn should have mono or stereo channels");
		return -EINVAL;
	}

	/* Enable AD1 for LinInL */
	if (channel_index & e_CHANNEL_1) {
		initialVal_AD = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);
		if (initialVal_AD & EN_AD1)
			return 0;

		error = HW_ACODEC_MODIFY_WRITE(AD_ALLOCATION_TO_SLOT0_1_REG,
							DATA_FROM_AD_OUT1, 0);
		if (0 != error) {
			dev_err(dev, "Slot 00 outputs data from AD_OUT1 %d",
									error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_AD_CHANNELS_ENABLE_REG,
								EN_AD1, 0);
		if (0 != error) {
			dev_err(dev, "Enable AD1 for LinInL %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_MUXES_REG1, 0,
							SEL_DMIC1_FOR_AD_OUT1);
		if (0 != error) {
			dev_err(dev, "Select ADC3 for AD_OUT1  %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(
				LINE_IN_MIC_CONF_REG, EN_LIN_IN_L, 0);
		if (0 != error) {
			dev_err(dev, "Power up LinInL %d", error);
			return error;
		}

		/* Power Up ADC3 */
		error = HW_ACODEC_MODIFY_WRITE(ADC_DAC_ENABLE_REG,
					POWER_UP_ADC3, 0);
		if (0 != error) {
			dev_err(dev, "Power Up ADC3 %d", error);
			return error;
		}
	}
	/* Enable AD2 for LinInR */

	if (channel_index & e_CHANNEL_2) {
		initialVal_AD = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);
		if (EN_AD2 & initialVal_AD)
			return 0;

		error = HW_ACODEC_MODIFY_WRITE(AD_ALLOCATION_TO_SLOT0_1_REG,
						(DATA_FROM_AD_OUT2<<4), 0);
		if (0 != error) {
			dev_err(dev, "Slot 01 outputs data from AD_OUT2 %d",
									error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_AD_CHANNELS_ENABLE_REG,
								EN_AD2, 0);
		if (0 != error) {
			dev_err(dev, "Enable AD2 LinInR %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_MUXES_REG1, 0,
							SEL_DMIC2_FOR_AD_OUT2);
		if (0 != error) {
			dev_err(dev, "Select ADC2 for AD_OUT2 %d", error);
			return error;
		}

		/* Select LinInR */
		error = HW_ACODEC_MODIFY_WRITE(ADC_DAC_ENABLE_REG,
							SEL_LINR_CLR_MIC2, 0);
		if (0 != error) {
			dev_err(dev, "Select LinInR %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(LINE_IN_MIC_CONF_REG,
						EN_LIN_IN_R, 0);
		if (0 != error) {
			dev_err(dev, "Power up LinInR %d", error);
			return error;
		}

		/* Power Up ADC2 */
		error = HW_ACODEC_MODIFY_WRITE(
				ADC_DAC_ENABLE_REG, POWER_UP_ADC2, 0);
		if (0 != error) {
			dev_err(dev, "Power Up ADC2 %d", error);
			return error;
		}
	}
	return error;
}
/**
 * @brief Power down LinIn
 * @channel_index Channel-index of LinIn
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_power_down_lin(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal_AD = 0;

	/* Check if LinIn PowerDown request is mono or Stereo channel */
	if (!(channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		dev_err(dev, "LinIn should have mono or stereo channels");
		return -EINVAL;
	}

	/* Enable AD1 for LinInL */
	if (channel_index & e_CHANNEL_1) {
		initialVal_AD = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);
		if (!(initialVal_AD & EN_AD1))
			return 0;

		error = HW_ACODEC_MODIFY_WRITE(LINE_IN_MIC_CONF_REG, 0,
								EN_LIN_IN_L);
		if (0 != error) {
			dev_err(dev, "Power Down LinInL %d", error);
			return error;
		}

			/* Power Down ADC3 */
		error = HW_ACODEC_MODIFY_WRITE(ADC_DAC_ENABLE_REG, 0,
								POWER_UP_ADC3);
		if (0 != error) {
			dev_err(dev, "Power Down ADC3 %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_AD_CHANNELS_ENABLE_REG,
						0, EN_AD1);
		if (0 != error) {
			dev_err(dev, "Disable AD1 for LinInL %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(AD_ALLOCATION_TO_SLOT0_1_REG, 0,
							DATA_FROM_AD_OUT1);
		if (0 != error) {
			dev_err(dev,
			"Slot 00 outputs data cleared from AD_OUT1 %d",
								error);
			return error;
		}
	}

	/* Enable AD2 for LinInR */
	if (channel_index & e_CHANNEL_2) {
		initialVal_AD = HW_REG_READ(DIGITAL_DA_CHANNELS_ENABLE_REG);
		if (!(initialVal_AD & EN_AD2))
			return 0;

		error = HW_ACODEC_MODIFY_WRITE(LINE_IN_MIC_CONF_REG, 0,
								EN_LIN_IN_R);
		if (0 != error) {
			dev_err(dev, "Power Down LinInR %d", error);
			return error;
		}

		/* Power Down ADC2 */
		error = HW_ACODEC_MODIFY_WRITE(ADC_DAC_ENABLE_REG, 0,
								POWER_UP_ADC2);
		if (0 != error) {
			dev_err(dev, "Power Down ADC2 %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_AD_CHANNELS_ENABLE_REG,
							0, EN_AD2);
		if (0 != error) {
			dev_err(dev, "Disable AD2 LinInR %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(AD_ALLOCATION_TO_SLOT0_1_REG, 0,
						(DATA_FROM_AD_OUT2<<4));
		if (0 != error) {
			dev_err(dev,
				"Slot01 outputs data cleared from AD_OUT2 %d",
									error);
			return error;
		}
	}
	return error;
}
/**
 * @brief Mute LinIn
 * @channel_index Channel-index of LinIn
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_mute_lin(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;

	if (!(channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		dev_err(dev, "LinIn should have mono or stereo channels");
		return -EINVAL;
	}

	if (channel_index & e_CHANNEL_1) {
		/* Mute LinInL */
		error = HW_ACODEC_MODIFY_WRITE(LINE_IN_MIC_CONF_REG,
							MUT_LIN_IN_L, 0);
		if (0 != error) {
			dev_err(dev, "Mute LinInL %d", error);
			return error;
		}
	}

	if (channel_index & e_CHANNEL_2) {
		/* Mute LinInR */
		error = HW_ACODEC_MODIFY_WRITE(LINE_IN_MIC_CONF_REG,
								MUT_LIN_IN_R,
								0);
		if (0 != error) {
			dev_err(dev, "Mute LinInR %d", error);
			return error;
		}
	}
	return error;
}
/**
 * @brief Unmute LinIn
 * @channel_index Channel-index of LinIn
 * @gain Gain index of LinIn
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_unmute_lin(enum AUDIOIO_CH_INDEX channel_index, int *gain,
					struct device *dev)
{
	int error = 0;

	if (!(channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		dev_err(dev, "LinIn should have mono or stereo channels");
		return -EINVAL;
	}

	if (channel_index & e_CHANNEL_1) {
		/* UnMute LinInL */
		error = HW_ACODEC_MODIFY_WRITE(LINE_IN_MIC_CONF_REG, 0,
								MUT_LIN_IN_L);
		if (0 != error) {
			dev_err(dev, "UnMute LinInL %d", error);
			return error;
		}
	}

	if (channel_index & e_CHANNEL_2) {
		/* UnMute LinInR */
		error = HW_ACODEC_MODIFY_WRITE(LINE_IN_MIC_CONF_REG, 0,
								MUT_LIN_IN_R);
		if (0 != error) {
			dev_err(dev, "UnMute LinInR %d", error);
			return error;
		}
	}
	return error;
}
/**
 * @brief Enables fading of LinIn
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_enable_fade_lin(struct device *dev)
{
	int error = 0;

	error = HW_ACODEC_MODIFY_WRITE(AD1_DIGITAL_GAIN_REG, 0, DIS_FADING);
	if (0 != error) {
		dev_err(dev, "Enable fading for LinInL %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(AD2_DIGITAL_GAIN_REG, 0, DIS_FADING);
	if (0 != error) {
		dev_err(dev, "Enable fading for LinInR %d", error);
		return error;
	}
	return error;
}
/**
 * @brief Disables fading of LinIn
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_disable_fade_lin(struct device *dev)
{
	int error = 0;

	error = HW_ACODEC_MODIFY_WRITE(AD1_DIGITAL_GAIN_REG, DIS_FADING, 0);
	if (0 != error) {
		dev_err(dev, "Disable fading for LinInL %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(AD2_DIGITAL_GAIN_REG, DIS_FADING, 0);
	if (0 != error) {
		dev_err(dev, "Disable fading for LinInR %d", error);
		return error;
	}
	return error;
}
/**
 * @brief Power Up DMIC12 LinIn
 * @channel_index Channel-index of DMIC12
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_power_up_dmic12(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal_AD = 0;

	/* Check if DMic12 request is mono or Stereo */
	if (!(channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		dev_err(dev, "DMic12 does not support more than 2 channels");

		return -EINVAL;
	}

	/* Setting Direction for GPIO pins on AB8500 */
	error = HW_REG_WRITE(AB8500_GPIO_DIR4_REG, GPIO27_DIR_OUTPUT);
	if (0 != error) {
		dev_err(dev, "Setting Direction for GPIO pins on AB8500 %d",
									error);
		return error;
	}

	/* Enable AD1 for Dmic1 */
	if (channel_index & e_CHANNEL_1) {
		/* Check if DMIC1 is already powered up or used by LinInL */
		initialVal_AD = HW_REG_READ(DIGITAL_AD_CHANNELS_ENABLE_REG);
		if (initialVal_AD & EN_AD1)
			return 0;

		error = HW_REG_WRITE(AD_ALLOCATION_TO_SLOT0_1_REG,
							DATA_FROM_AD_OUT1);
		if (0 != error) {
			dev_err(dev, "Slot 00 outputs data from AD_OUT1 %d",
									error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_AD_CHANNELS_ENABLE_REG,
								EN_AD1, 0);
		if (0 != error) {
			dev_err(dev, "Enable AD1 for DMIC1 %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_MUXES_REG1,
						SEL_DMIC1_FOR_AD_OUT1, 0);
		if (0 != error) {
			dev_err(dev, "Select DMIC1 for AD_OUT1 %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DMIC_ENABLE_REG, EN_DMIC1, 0);
		if (0 != error) {
			dev_err(dev, "Enable DMIC1 %d", error);
			return error;
		}
	}
	/* Enable AD2 for Dmic2 */

	if (channel_index & e_CHANNEL_2) {
		/* Check if DMIC2 is already powered up
		or used by Mic2 or LinInR */
		initialVal_AD = HW_REG_READ(DIGITAL_AD_CHANNELS_ENABLE_REG);
		if (initialVal_AD & EN_AD2)
			return 0;

		error = HW_ACODEC_MODIFY_WRITE(AD_ALLOCATION_TO_SLOT0_1_REG,
						(DATA_FROM_AD_OUT2<<4), 0);
		if (0 != error) {
			dev_err(dev, "Slot 01 outputs data from AD_OUT2 %d",
									error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_AD_CHANNELS_ENABLE_REG,
								EN_AD2, 0);
		if (0 != error) {
			dev_err(dev, "Enable AD2 for DMIC2 %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_MUXES_REG1,
						SEL_DMIC2_FOR_AD_OUT2, 0);
		if (0 != error) {
			dev_err(dev, "Select DMIC2 for AD_OUT2 %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DMIC_ENABLE_REG, EN_DMIC2, 0);
		if (0 != error) {
			dev_err(dev, "Enable DMIC2 %d", error);
			return error;
		}
	}

	return error;
}
/**
 * @brief Power down DMIC12 LinIn
 * @channel_index Channel-index of DMIC12
 * @return 0 on success otherwise negative error code
 */


int ste_audio_io_power_down_dmic12(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal_AD = 0;

	/* Check if DMic12 request is mono or Stereo or multi channel */
	if (!(channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		dev_err(dev, "DMic12 does not support more than 2 channels");

		return -EINVAL;
	}

	/* Setting Direction for GPIO pins on AB8500 */
	error = HW_ACODEC_MODIFY_WRITE(AB8500_GPIO_DIR4_REG, 0,
							GPIO27_DIR_OUTPUT);
	if (0 != error) {
		dev_err(dev, "Clearing Direction for GPIO pins on AB8500 %d",
									error);
		return error;
	}
	/* Enable AD1 for Dmic1 */
	if (channel_index & e_CHANNEL_1) {
		/* Check if DMIC1 is already powered Down or used by LinInL */
		initialVal_AD = HW_REG_READ(DIGITAL_AD_CHANNELS_ENABLE_REG);
		if (!(initialVal_AD & EN_AD1))
			return 0;

		error = HW_ACODEC_MODIFY_WRITE(DMIC_ENABLE_REG, 0, EN_DMIC1);
		if (0 != error) {
			dev_err(dev, "Enable DMIC1 %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_AD_CHANNELS_ENABLE_REG,
							0, EN_AD1);
		if (0 != error) {
			dev_err(dev, "Disable AD1 for DMIC1 %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(AD_ALLOCATION_TO_SLOT0_1_REG, 0,
							DATA_FROM_AD_OUT1);
		if (0 != error) {
			dev_err(dev,
			"Slot 00 outputs data cleared from AD_OUT1 %d",
								error);
			return error;
		}
	}

	/* Enable AD2 for Dmic2 */
	if (channel_index & e_CHANNEL_2) {
		/* MIC2 is already powered Down or used by Mic2 or LinInR */
		initialVal_AD = HW_REG_READ(DIGITAL_AD_CHANNELS_ENABLE_REG);
		if (!(initialVal_AD & EN_AD2))
			return 0;

		error = HW_ACODEC_MODIFY_WRITE(DMIC_ENABLE_REG, 0, EN_DMIC2);
		if (0 != error) {
			dev_err(dev, "Enable DMIC2 %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_AD_CHANNELS_ENABLE_REG,
							0, EN_AD2);
		if (0 != error) {
			dev_err(dev, "Disable AD2 for DMIC2 %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(AD_ALLOCATION_TO_SLOT0_1_REG, 0,
						(DATA_FROM_AD_OUT2<<4));
		if (0 != error) {
			dev_err(dev,
			"Slot 01 outputs data cleared from AD_OUT2 %d",
								error);
			return error;
		}
	}
	return error;
}
/**
 * @brief  Get headset gain
 * @left_volume
 * @right_volume
 * @return 0 on success otherwise negative error code
 */


int ste_audio_io_get_headset_gain(int *left_volume, int *right_volume,
			u16 gain_index, struct device *dev)
{
	int i = 0;
	if (gain_index == 0) {

		*left_volume = 0 - HW_REG_READ(DA1_DIGITAL_GAIN_REG);
		*right_volume = 0 - HW_REG_READ(DA2_DIGITAL_GAIN_REG);

	}

	if (gain_index == 1) {
		*left_volume = 8 - HW_REG_READ(HSL_EAR_DIGITAL_GAIN_REG);
		*right_volume = 8 - HW_REG_READ(HSR_DIGITAL_GAIN_REG);
	}

	if (gain_index == 2) {
		i = (HW_REG_READ(ANALOG_HS_GAIN_REG)>>4);
		*left_volume = hs_analog_gain_table[i];
		i = (HW_REG_READ(ANALOG_HS_GAIN_REG) & MASK_QUARTET0);
		*right_volume = hs_analog_gain_table[i];
	}
	return 0;
}
/**
 * @brief  Get earpiece gain
 * @left_volume
 * @right_volume
 * @return 0 on success otherwise negative error code
 */


int ste_audio_io_get_earpiece_gain(int *left_volume, int *right_volume,
			u16 gain_index, struct device *dev)
{
	if (0 == gain_index)
		*left_volume = 0 - HW_REG_READ(DA1_DIGITAL_GAIN_REG);
	if (1 == gain_index)
		*left_volume = 8 - HW_REG_READ(HSL_EAR_DIGITAL_GAIN_REG);
	return 0;
}
/**
 * @brief  Get ihf gain
 * @left_volume
 * @right_volume
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_get_ihf_gain(int *left_volume, int *right_volume,
			u16 gain_index, struct device *dev)
{

	*left_volume = 0 - HW_REG_READ(DA3_DIGITAL_GAIN_REG);
	*right_volume = 0 - HW_REG_READ(DA4_DIGITAL_GAIN_REG);
	return 0;
}
/**
 * @brief  Get vibl gain
 * @left_volume
 * @right_volume
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_get_vibl_gain(int *left_volume, int *right_volume,
			u16 gain_index, struct device *dev)
{

	*left_volume = 0 - HW_REG_READ(DA5_DIGITAL_GAIN_REG);

	return 0;
}
/**
 * @brief  Get vibr gain
 * @left_volume
 * @right_volume
 * @return 0 on success otherwise negative error code
 */


int ste_audio_io_get_vibr_gain(int *left_volume, int *right_volume,
			u16 gain_index, struct device *dev)
{

	*right_volume = 0 - HW_REG_READ(DA6_DIGITAL_GAIN_REG);
	return 0;
}
/**
 * @brief  Get MIC1A & MIC2A gain
 * @left_volume
 * @right_volume
 * @return 0 on success otherwise negative error code
 */


int ste_audio_io_get_mic1a_gain(int *left_volume, int *right_volume,
			u16 gain_index, struct device *dev)
{
	if (gain_index == 0)
		*left_volume = 31 - HW_REG_READ(AD3_DIGITAL_GAIN_REG);
	if (gain_index == 1)
		*left_volume = HW_REG_READ(ANALOG_MIC1_GAIN_REG);

	return 0;
}
/**
 * @brief  Get MIC2 gain
 * @left_volume
 * @right_volume
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_get_mic2_gain(int *left_volume, int *right_volume,
			u16 gain_index, struct device *dev)
{
	if (gain_index == 0)
		*left_volume = 31 - HW_REG_READ(AD2_DIGITAL_GAIN_REG);
	if (gain_index == 1)
		*left_volume = HW_REG_READ(ANALOG_MIC2_GAIN_REG);

	return 0;
}
/**
 * @brief  Get Lin IN gain
 * @left_volume
 * @right_volume
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_get_lin_gain(int *left_volume, int *right_volume,
			u16 gain_index, struct device *dev)
{
	if (gain_index == 0) {
		*left_volume = 31 - HW_REG_READ(AD1_DIGITAL_GAIN_REG);
		*right_volume = 31 - HW_REG_READ(AD2_DIGITAL_GAIN_REG);
	}

	if (gain_index == 0) {
		*left_volume = 2 * ((HW_REG_READ(ANALOG_HS_GAIN_REG)>>4) - 5);
		*right_volume = 2 * (HW_REG_READ(ANALOG_LINE_IN_GAIN_REG) - 5);
	}

	return 0;
}
/**
 * @brief  Get DMIC12 gain
 * @left_volume
 * @right_volume
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_get_dmic12_gain(int *left_volume, int *right_volume,
			u16 gain_index, struct device *dev)
{

	*left_volume = HW_REG_READ(AD1_DIGITAL_GAIN_REG);

	*right_volume = HW_REG_READ(AD2_DIGITAL_GAIN_REG);

	return 0;
}
/**
 * @brief  Get DMIC34 gain
 * @left_volume
 * @right_volume
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_get_dmic34_gain(int *left_volume, int *right_volume,
			u16 gain_index, struct device *dev)
{
	*left_volume = HW_REG_READ(AD3_DIGITAL_GAIN_REG);
	*right_volume = HW_REG_READ(AD4_DIGITAL_GAIN_REG);

	return 0;
}
/**
 * @brief  Get DMIC56 gain
 * @left_volume
 * @right_volume
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_get_dmic56_gain(int *left_volume, int *right_volume,
			u16 gain_index, struct device *dev)
{
	*left_volume = HW_REG_READ(AD5_DIGITAL_GAIN_REG);

	*right_volume = HW_REG_READ(AD6_DIGITAL_GAIN_REG);
	return 0;
}
/**
 * @brief Set gain of headset along a specified channel
 * @channel_index Channel-index of headset
 * @gain_index Gain index of headset
 * @gain_value Gain value of headset
 * @linear
 * @return 0 on success otherwise negative error code
 */


int ste_audio_io_set_headset_gain(enum AUDIOIO_CH_INDEX channel_index,
		u16 gain_index, int gain_value, u32 linear,
					struct device *dev)
{
	int error = 0;
	unsigned char initial_val = 0;
	int i = 0;
	int acodec_device_id;

	acodec_device_id = abx500_get_chip_id(dev);

	if (channel_index & e_CHANNEL_1) {
		if (gain_index == 0) {
			int gain = 0;
			gain = 0 - gain_value;

			initial_val = HW_REG_READ(DA1_DIGITAL_GAIN_REG);
			/* Write gain */
			error = HW_REG_WRITE(DA1_DIGITAL_GAIN_REG,
						((initial_val
			& (~DIGITAL_GAIN_MASK)) | (gain & DIGITAL_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain HSL gainindex = %d %d",
							gain_index, error);
				return error;
			}
		}

		if (gain_index == 1) {
			int gain = 0;
			gain = 8 - gain_value;

			initial_val = HW_REG_READ(HSL_EAR_DIGITAL_GAIN_REG);
			/* Write gain */
			error = HW_REG_WRITE(HSL_EAR_DIGITAL_GAIN_REG,
			((initial_val & (~HS_DIGITAL_GAIN_MASK)) | (gain &
						HS_DIGITAL_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain HSL gain index = %d %d",
							gain_index, error);
				return error;
			}
		}

		if (gain_index == 2) {
			/* Set Analog gain */
			int gain = -1;

			if (gain_value % 2) {
				gain_value -= 1;
				dev_err(dev,
		"Odd Gain received.Fixing it to 2dB step gain_value = %d",
								gain_value);
			}
			/* Fix for 4dB step gains. Select one lower value */
			if (gain_value == -22)
				gain_value = -24;

			if (gain_value == -26)
				gain_value = -28;

			if (gain_value == -30)
				gain_value = -32;

			for (i = 0 ; i < 16; i++) {
				if (hs_analog_gain_table[i] == gain_value) {
						gain = i<<4;
						break;
					}
			}
			if (gain == -1)
				return -1;

			if ((AB8500_REV_10 == acodec_device_id) ||
					(AB8500_REV_11 == acodec_device_id)) {
				if (!gain)
					gain = 0x10;
				gain = 0xF0 - gain;
			}
			initial_val = HW_REG_READ(ANALOG_HS_GAIN_REG);

			/* Write gain */
			error = HW_REG_WRITE(ANALOG_HS_GAIN_REG, ((initial_val &
			(~L_ANALOG_GAIN_MASK)) | (gain & L_ANALOG_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain HSL gain index = %d %d",
							gain_index, error);
				return error;
			}
		}
	}

	/* for HSR */
	if (channel_index & e_CHANNEL_2) {
		/* Set Gain HSR */
		if (gain_index == 0) {
			int gain = 0;
			gain = 0 - gain_value;

			initial_val = HW_REG_READ(DA2_DIGITAL_GAIN_REG);
			/* Write gain */
			error = HW_REG_WRITE(DA2_DIGITAL_GAIN_REG, ((initial_val
			& (~DIGITAL_GAIN_MASK)) | (gain & DIGITAL_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain HSR gain index = %d %d",
							gain_index, error);
				return error;
			}
		}

		if (gain_index == 1) {
			int gain = 0;
			gain = 8 - gain_value;

			initial_val = HW_REG_READ(HSR_DIGITAL_GAIN_REG);
			/* Write gain */
			error = HW_REG_WRITE(HSR_DIGITAL_GAIN_REG, ((initial_val
			& (~HS_DIGITAL_GAIN_MASK)) | (gain &
							HS_DIGITAL_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain HSR gain index = %d %d",
							gain_index, error);
				return error;
			}

		}

		if (gain_index == 2) {
			/* Set Analog gain */
			int gain = -1;

			if (gain_value % 2) {
				gain_value -= 1;
				dev_err(dev,
		"Odd Gain received.Fixing it to 2dB step gain_value = %d",
								gain_value);
			}
			/* Fix for 4dB step gains. Select one lower value */
			if (gain_value == -22)
				gain_value = -24;

			if (gain_value == -26)
				gain_value = -28;

			if (gain_value == -30)
				gain_value = -32;

			for (i = 0 ; i < 16 ; i++) {
				if (hs_analog_gain_table[i] == gain_value) {
						gain = i;
						break;
					}
			}
			if (gain == -1)
				return -1;

			if ((AB8500_REV_10 == acodec_device_id) ||
					(AB8500_REV_11 == acodec_device_id)) {
				if (!gain)
					gain = 1;
				gain = 0x0F - gain;
			}
			initial_val = HW_REG_READ(ANALOG_HS_GAIN_REG);
			/* Write gain */
			error = HW_REG_WRITE(ANALOG_HS_GAIN_REG, ((initial_val &
			(~R_ANALOG_GAIN_MASK)) | (gain & R_ANALOG_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain HSR gainindex = %d %d",
							gain_index, error);
				return error;
			}
		}
	}
	dump_acodec_registers(__func__, dev);
	return error;
}
/**
 * @brief Set gain of earpiece
 * @channel_index Channel-index of earpiece
 * @gain_index Gain index of earpiece
 * @gain_value Gain value of earpiece
 * @linear
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_set_earpiece_gain(enum AUDIOIO_CH_INDEX channel_index,
			u16 gain_index, int gain_value, u32 linear,
					struct device *dev)
{
	int error = 0;
	unsigned char initial_val = 0;
	if (channel_index & e_CHANNEL_1) {
		if (0 == gain_index) {
			int gain = 0;
			gain = 0 - gain_value;

			initial_val = HW_REG_READ(DA1_DIGITAL_GAIN_REG);
			/* Write gain */
			error = HW_REG_WRITE(DA1_DIGITAL_GAIN_REG, ((initial_val
			& (~DIGITAL_GAIN_MASK)) | (gain & DIGITAL_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain Ear gainindex = %d %d",
							gain_index, error);
				return error;
			}
		}

		if (gain_index == 1) {
			int gain = 0;
			gain = 8 - gain_value;

			initial_val = HW_REG_READ(HSL_EAR_DIGITAL_GAIN_REG);
			/* Write gain */
			error = HW_REG_WRITE(HSL_EAR_DIGITAL_GAIN_REG,
			((initial_val & (~HS_DIGITAL_GAIN_MASK)) | (gain &
							HS_DIGITAL_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain Ear gainindex = %d %d",
							gain_index, error);
				return error;
			}
		}
	}
	dump_acodec_registers(__func__, dev);
	return error;
}
/**
 * @brief Set gain of vibl
 * @channel_index Channel-index of vibl
 * @gain_index Gain index of vibl
 * @gain_value Gain value of vibl
 * @linear
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_set_vibl_gain(enum AUDIOIO_CH_INDEX channel_index,
			u16 gain_index, int gain_value, u32 linear,
					struct device *dev)
{

	int error = 0;
	unsigned char initial_val = 0;

	if (channel_index & e_CHANNEL_1) {
		/* Set Gain vibl */
		if (gain_index == 0) {
			int gain = 0;
			gain = 0 - gain_value;

			initial_val = HW_REG_READ(DA5_DIGITAL_GAIN_REG);
			/* Write gain */
			error = HW_REG_WRITE(DA5_DIGITAL_GAIN_REG, ((initial_val
			& (~DIGITAL_GAIN_MASK)) | (gain & DIGITAL_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain VibL gain index = %d %d",
							gain_index, error);
				return error;
			}
		}
	}
	return error;
}
/**
 * @brief Set gain of vibr
 * @channel_index Channel-index of vibr
 * @gain_index Gain index of vibr
 * @gain_value Gain value of vibr
 * @linear
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_set_vibr_gain(enum AUDIOIO_CH_INDEX channel_index,
				u16 gain_index, int gain_value,
				u32 linear,
					struct device *dev)
{

	int error = 0;
	unsigned char initial_val = 0;

	if (channel_index & e_CHANNEL_1) {
		/* Set Gain vibr */
		if (gain_index == 0) {
			int gain = 0;
			gain = 0 - gain_value;

			initial_val = HW_REG_READ(DA6_DIGITAL_GAIN_REG);
			/* Write gain */
			error = HW_REG_WRITE(DA6_DIGITAL_GAIN_REG,
					((initial_val
			& (~DIGITAL_GAIN_MASK)) | (gain & DIGITAL_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain VibR gain index = %d %d",
							gain_index, error);
				return error;
			}
		}
	}
	return error;
}
/**
 * @brief Set gain of ihf along a specified channel
 * @channel_index Channel-index of ihf
 * @gain_index Gain index of ihf
 * @gain_value Gain value of ihf
 * @linear
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_set_ihf_gain(enum AUDIOIO_CH_INDEX channel_index,
				u16 gain_index, int gain_value, u32 linear,
					struct device *dev)
{
	int error = 0;
	unsigned char initial_val = 0;

	if (channel_index & e_CHANNEL_1) {
		/* Set Gain IHFL */
		if (gain_index == 0) {
			int gain = 0;
			gain = 0 - gain_value;

			initial_val = HW_REG_READ(DA3_DIGITAL_GAIN_REG);
			error = HW_REG_WRITE(DA3_DIGITAL_GAIN_REG, ((initial_val
			& (~DIGITAL_GAIN_MASK)) | (gain & DIGITAL_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain IHFL gain index = %d %d",
							gain_index, error);
				return error;
			}

		}
	}
	if (channel_index & e_CHANNEL_2) {
		/* Set Gain IHFR */
		if (gain_index == 0) {
			int gain = 0;
			gain = 0 - gain_value;

			initial_val = HW_REG_READ(DA4_DIGITAL_GAIN_REG);
			error = HW_REG_WRITE(DA4_DIGITAL_GAIN_REG, ((initial_val
			&  (~DIGITAL_GAIN_MASK)) | (gain & DIGITAL_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain IHFR gain index = %d %d",
							gain_index, error);
				return error;
			}
		}
	}

	return error;
}
/**
 * @brief Set gain of MIC1A & MIC1B
 * @channel_index Channel-index of MIC1
 * @gain_index Gain index of MIC1
 * @gain_value Gain value of MIC1
 * @linear
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_set_mic1a_gain(enum AUDIOIO_CH_INDEX channel_index,
			u16 gain_index, int gain_value, u32 linear,
					struct device *dev)
{
	int error = 0;
	unsigned char initial_val = 0;

	if (channel_index & e_CHANNEL_1) {
		/* Set Gain mic1 */
		if (gain_index == 0) {
			int gain = 0;
			gain = 31 - gain_value;

			initial_val = HW_REG_READ(AD3_DIGITAL_GAIN_REG);
			/* Write gain */
			error = HW_REG_WRITE(AD3_DIGITAL_GAIN_REG, ((initial_val
			&  (~DIGITAL_GAIN_MASK)) | (gain & DIGITAL_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain Mic1 gain index = %d %d",
							gain_index, error);
				return error;
			}

		}

		if (gain_index == 1) {
			/* Set Analog gain */
			initial_val = HW_REG_READ(ANALOG_MIC1_GAIN_REG);

			/* Write gain */
			error = HW_REG_WRITE(ANALOG_MIC1_GAIN_REG, ((initial_val
			& (~MIC_ANALOG_GAIN_MASK)) | (gain_value &
							MIC_ANALOG_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain Mic1 gain index = %d %d",
							gain_index, error);
				return error;
			}
		}
	}
	return error;
}
/**
 * @brief Set gain of MIC2
 * @channel_index Channel-index of MIC2
 * @gain_index Gain index of MIC2
 * @gain_value Gain value of MIC2
 * @linear
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_set_mic2_gain(enum AUDIOIO_CH_INDEX channel_index,
				u16 gain_index, int gain_value,
				u32 linear,
					struct device *dev)
{
	int error = 0;
	unsigned char initial_val = 0;

	if (channel_index & e_CHANNEL_1) {
		/* Set Gain mic2 */
		if (gain_index == 0) {
			int gain = 0;
			gain = 31 - gain_value;

		    initial_val = HW_REG_READ(AD2_DIGITAL_GAIN_REG);
			/* Write gain */
			error = HW_REG_WRITE(AD2_DIGITAL_GAIN_REG, ((initial_val
			& (~DIGITAL_GAIN_MASK)) | (gain & DIGITAL_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain Mic2 gain index = %d %d",
							gain_index, error);
				return error;
			}

		}

		if (gain_index == 1) {
			/* Set Analog gain */
			initial_val = HW_REG_READ(ANALOG_MIC2_GAIN_REG);

			/* Write gain */
			error = HW_REG_WRITE(ANALOG_MIC2_GAIN_REG, ((initial_val
			& (~MIC_ANALOG_GAIN_MASK)) | (gain_value &
							MIC_ANALOG_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain Mic2 gain index = %d %d",
							gain_index, error);
				return error;
			}
		}
	}
	return error;
}
/**
 * @brief Set gain of Lin IN along a specified channel
 * @channel_index Channel-index of Lin In
 * @gain_index Gain index of Lin In
 * @gain_value Gain value of Lin In
 * @linear
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_set_lin_gain(enum AUDIOIO_CH_INDEX channel_index,
				u16 gain_index, int gain_value, u32 linear,
					struct device *dev)
{
	int error = 0;
	unsigned char initial_val = 0;

	if (channel_index & e_CHANNEL_1) {
		if (gain_index == 0) {
			int gain = 0;
			gain = 31 - gain_value;

			initial_val = HW_REG_READ(AD1_DIGITAL_GAIN_REG);
			/* Write gain */
			error = HW_REG_WRITE(AD1_DIGITAL_GAIN_REG,
							((initial_val
			& (~DIGITAL_GAIN_MASK)) | (gain & DIGITAL_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain LinInL gain index = %d %d",
							gain_index, error);
				return error;
			}

		}

		if (gain_index == 1) {
			int gain = 0;
			/*
			 * Converting -10 to 20 range into 0 - 15
			 * & shifting it left by 4 bits
			 */
			gain = ((gain_value/2) + 5)<<4;

			initial_val = HW_REG_READ(ANALOG_LINE_IN_GAIN_REG);
			/* Write gain */
			error = HW_REG_WRITE(ANALOG_LINE_IN_GAIN_REG,
			((initial_val & (~L_ANALOG_GAIN_MASK)) | (gain &
							L_ANALOG_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain LinInL gain index = %d %d",
							gain_index, error);
				return error;
			}
		}
	}

	if (channel_index & e_CHANNEL_2) {
		/* Set Gain LinInR */
		if (gain_index == 0) {
			int gain = 0;
			gain = 31 - gain_value;

			initial_val = HW_REG_READ(AD2_DIGITAL_GAIN_REG);
			/* Write gain */
			error = HW_REG_WRITE(AD2_DIGITAL_GAIN_REG,
							((initial_val
			& (~DIGITAL_GAIN_MASK)) | (gain & DIGITAL_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain LinInR gain index = %d%d",
							gain_index, error);
				return error;
			}
		}
		if (gain_index == 1) {
			int gain = 0;
			/* Converting -10 to 20 range into 0 - 15 */
			gain = ((gain_value/2) + 5);

			initial_val = HW_REG_READ(ANALOG_LINE_IN_GAIN_REG);
			/* Write gain */
			error = HW_REG_WRITE(ANALOG_LINE_IN_GAIN_REG,
			((initial_val & (~R_ANALOG_GAIN_MASK)) | (gain &
							R_ANALOG_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain LinInR gain index = %d %d",
							gain_index, error);
				return error;
			}
		}
	}

	return error;
}
/**
 * @brief Set gain of DMIC12 along a specified channel
 * @channel_index Channel-index of DMIC12
 * @gain_index Gain index of DMIC12
 * @gain_value Gain value of DMIC12
 * @linear
 * @return 0 on success otherwise negative error code
 */

int ste_audio_io_set_dmic12_gain(enum AUDIOIO_CH_INDEX channel_index,
			u16 gain_index, int gain_value, u32 linear,
					struct device *dev)
{
	int error = 0;
	unsigned char initial_val = 0;

	if (channel_index & e_CHANNEL_1) {
		/* Set Gain Dmic1 */
		if (gain_index == 0) {
			int gain = 0;
			gain = 31 - gain_value;

			initial_val = HW_REG_READ(AD1_DIGITAL_GAIN_REG);
			error = HW_REG_WRITE(AD1_DIGITAL_GAIN_REG,
				((initial_val
			& (~DIGITAL_GAIN_MASK)) | (gain & DIGITAL_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain DMic1 gain index = %d %d",
							gain_index, error);
				return error;
			 }
		 }
	 }
	if (channel_index & e_CHANNEL_2) {
		/* Set Gain Dmic2 */
		if (gain_index == 0) {
			int gain = 0;
			gain = 31 - gain_value;

			initial_val = HW_REG_READ(AD2_DIGITAL_GAIN_REG);
			error = HW_REG_WRITE(AD2_DIGITAL_GAIN_REG,
					((initial_val
			& (~DIGITAL_GAIN_MASK)) | (gain & DIGITAL_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain DMic2 gain index = %d %d",
							gain_index, error);
				return error;
			}
		}
	}
	return error;
}

int ste_audio_io_switch_to_burst_mode_headset(int burst_fifo_switch_frame,
					struct device *dev)
{
	int error = 0;

	error = HW_ACODEC_MODIFY_WRITE(BURST_FIFO_INT_CONTROL_REG,
						WAKEUP_SIGNAL_SAMPLE_COUNT, 0);
	if (0 != error)
		return error;

	error = HW_ACODEC_MODIFY_WRITE(BURST_FIFO_LENGTH_REG,
					BURST_FIFO_TRANSFER_LENGTH, 0);
	if (0 != error)
		return error;

	error = HW_ACODEC_MODIFY_WRITE(BURST_FIFO_CONTROL_REG,
			(BURST_FIFO_INF_RUNNING | BURST_FIFO_INF_IN_MASTER_MODE
						|PRE_BIT_CLK0_COUNT), 0);
	if (0 != error)
		return error;

	error = HW_ACODEC_MODIFY_WRITE(BURST_FIFO_WAKE_UP_DELAY_REG,
						BURST_FIFO_WAKUP_DEALAY, 0);
	if (0 != error)
		return error;

	error = HW_REG_WRITE(BURST_FIFO_SWITCH_FRAME_REG,
						burst_fifo_switch_frame);
	if (0 != error)
		return error;

	error = HW_ACODEC_MODIFY_WRITE(TDM_IF_BYPASS_B_FIFO_REG,
							IF0_BFifoEn, 0);
	if (0 != error)
		return error;

	return error;
}
int ste_audio_io_switch_to_normal_mode_headset(
					struct device *dev)
{
	int error = 0;

	error = HW_ACODEC_MODIFY_WRITE(TDM_IF_BYPASS_B_FIFO_REG, 0,
								IF0_BFifoEn);
	if (0 != error)
		return error;

	error = HW_ACODEC_MODIFY_WRITE(BURST_FIFO_INT_CONTROL_REG,
						0, WAKEUP_SIGNAL_SAMPLE_COUNT);
	if (0 != error)
		return error;

	error = HW_ACODEC_MODIFY_WRITE(BURST_FIFO_LENGTH_REG,
						0, BURST_FIFO_TRANSFER_LENGTH);
	if (0 != error)
		return error;

	error = HW_ACODEC_MODIFY_WRITE(BURST_FIFO_CONTROL_REG, 0,
			(BURST_FIFO_INF_RUNNING | BURST_FIFO_INF_IN_MASTER_MODE
						|PRE_BIT_CLK0_COUNT));
	if (0 != error)
		return error;

	error = HW_ACODEC_MODIFY_WRITE(BURST_FIFO_WAKE_UP_DELAY_REG,
						0, BURST_FIFO_WAKUP_DEALAY);
	if (0 != error)
		return error;

	return error;
}


int ste_audio_io_mute_vibl(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	return 0;
}

int ste_audio_io_unmute_vibl(enum AUDIOIO_CH_INDEX channel_index, int *gain,
					struct device *dev)
{
	return 0;
}

int ste_audio_io_mute_vibr(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	return 0;
}
int ste_audio_io_unmute_vibr(enum AUDIOIO_CH_INDEX channel_index, int *gain,
					struct device *dev)
{
	return 0;
}

int ste_audio_io_mute_dmic12(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	if ((channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		error = ste_audio_io_set_dmic12_gain(channel_index, 0, -32,
									0, dev);
		if (0 != error) {
			dev_err(dev, "Mute dmic12 %d", error);
			return error;
		}
	}

	return error;

}

int ste_audio_io_unmute_dmic12(enum AUDIOIO_CH_INDEX channel_index, int *gain,
					struct device *dev)
{
	int error = 0;
	if ((channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		error = ste_audio_io_set_dmic12_gain(channel_index,
					0, gain[0], 0, dev);
		if (0 != error) {
			dev_err(dev, "UnMute dmic12 %d", error);
			return error;
		}
	}
	return error;
}
int ste_audio_io_enable_fade_dmic12(struct device *dev)
{
	return 0;
}

int ste_audio_io_disable_fade_dmic12(struct device *dev)
{
	return 0;
}

/**
 * @brief enable hardware loop of dmic12
 * @chnl_index Channel-index of dmic12
 * @hw_loop type of hardware loop
 * @loop_gain gain value to be used in hardware loop
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_enable_loop_dmic12(enum AUDIOIO_CH_INDEX chnl_index,
					enum AUDIOIO_HAL_HW_LOOPS hw_loop,
					int loop_gain, struct device *dev,
					void *cookie)
{
	int error = 0;
	struct transducer_context_t *trnsdr;
	trnsdr = (struct transducer_context_t *)cookie;

	switch (hw_loop) {
	/* Check if HSL is active */
	case AUDIOIO_SIDETONE_LOOP:
		if (!trnsdr[HS_CH].is_power_up[e_CHANNEL_1]
			&& !trnsdr[EAR_CH].is_power_up[e_CHANNEL_1]) {
			error = -EFAULT;
			dev_err(dev,
		"Sidetone enable needs HS or Earpiece powered up, err = %d",
								error);
			return error;
		}

		if (chnl_index & e_CHANNEL_1) {
			/* For ch1, Power On STFIR1, data comes from AD1*/
			error = HW_ACODEC_MODIFY_WRITE(DIGITAL_MUXES_REG2,
						0, FIR1_FROMAD1);
			if (error) {
				dev_err(dev, "FIR1 data comes from AD_OUT1 %d",
								error);
				return error;
			}

			error = HW_REG_WRITE(SIDETONE_FIR1_GAIN_REG, loop_gain);
			if (error) {
				dev_err(dev,
					"Set FIR1 Gain index = %d", error);
				return error;
			}
		}

		if (chnl_index & e_CHANNEL_2) {
			/* For ch2, Power On STFIR1, data comes from AD2*/
			error = HW_ACODEC_MODIFY_WRITE(DIGITAL_MUXES_REG2,
							0, FIR1_FROMAD2);
			if (error) {
				dev_err(dev, "FIR1 data comes from AD_OUT2 %d",
									error);
				return error;
			}
			error = HW_REG_WRITE(SIDETONE_FIR2_GAIN_REG, loop_gain);
			if (error) {
				dev_err(dev,
					"Set FIR2 Gain error = %d", error);
				return error;
			}
		}
		break;
	default:
		error = -EINVAL;
		dev_err(dev, "loop not supported %d", error);
	}
	dump_acodec_registers(__func__, dev);
	return error;
}

/**
 * @brief disable hardware loop of dmic12
 * @chnl_index Channel-index of dmic12
 * @hw_loop type of hardware loop
 * @return 0 on success otherwise negative error code
 */
int ste_audio_io_disable_loop_dmic12(enum AUDIOIO_CH_INDEX chnl_index,
					enum AUDIOIO_HAL_HW_LOOPS hw_loop,
					struct device *dev, void *cookie)
{
	int error = -EINVAL;
	struct transducer_context_t *trnsdr;
	trnsdr = (struct transducer_context_t *)cookie;

	switch (hw_loop) {
	/* Check if HSL is active */
	case AUDIOIO_SIDETONE_LOOP:
		if (!trnsdr[HS_CH].is_power_up[e_CHANNEL_1]
			&& !trnsdr[EAR_CH].is_power_up[e_CHANNEL_1]) {
			error = -EFAULT;
			dev_err(dev,
		"Sidetone disable needs HS or Earpiece powered up, err = %d",
					error);
			return error;
		}

		if (chnl_index & e_CHANNEL_1) {
			/* For ch1, Power On STFIR1, data comes from AD1*/
			error =
			HW_ACODEC_MODIFY_WRITE(DIGITAL_MUXES_REG2,
						0, FIR1_FROMAD1);
			if (error)
				dev_err(dev, "FIR1 data comes from AD_OUT1 %d",
								error);
		}

		if (chnl_index & e_CHANNEL_2) {
			/* For ch2, Power On STFIR1, data comes from AD2*/
			error =
			HW_ACODEC_MODIFY_WRITE(DIGITAL_MUXES_REG2,
						0, FIR1_FROMAD2);
			if (error)
				dev_err(dev, "FIR1 data comes from AD_OUT2 %d",
							error);
		}
		error = HW_ACODEC_MODIFY_WRITE(FILTERS_CONTROL_REG,
						0, FIR_FILTERCONTROL);
		if (error) {
			dev_err(dev,
				"ST FIR Filters disable failed %d", error);
				return error;
		}
		break;
	default:
		dev_err(dev, "loop not supported %d", error);
	}
	dump_acodec_registers(__func__, dev);
	return error;
}

int ste_audio_io_power_up_dmic34(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal_AD = 0;

	/* Check if DMic34 request is mono or Stereo */
	if (!(channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		dev_err(dev, "DMic34 does not support more than 2 channels");
		return -EINVAL;
	}

	/* Setting Direction for GPIO pins on AB8500 */
	error = HW_REG_WRITE(AB8500_GPIO_DIR4_REG, GPIO29_DIR_OUTPUT);
	if (0 != error) {
		dev_err(dev, "Setting Direction for GPIO pins on AB8500 %d",
									error);
		return error;
	}

	if (channel_index & e_CHANNEL_1) {
		/* Check if DMIC3 is already powered up or used by Mic1A
		or Mic1B */
		initialVal_AD = HW_REG_READ(DIGITAL_AD_CHANNELS_ENABLE_REG);

	if (initialVal_AD & (EN_AD3))
		return 0;


	error = HW_ACODEC_MODIFY_WRITE(AD_ALLOCATION_TO_SLOT2_3_REG,
							DATA_FROM_AD_OUT3, 0);
	if (0 != error) {
		dev_err(dev, "Slot 02 outputs data from AD_OUT3 %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_AD_CHANNELS_ENABLE_REG, EN_AD3,
									0);
	if (0 != error) {
		dev_err(dev, "Enable AD3 for DMIC3 %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(DIGITAL_MUXES_REG1,
					SEL_DMIC3_FOR_AD_OUT3,
									0);
	if (0 != error) {
		dev_err(dev, "Select DMIC3 for AD_OUT3 %d", error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(DMIC_ENABLE_REG, EN_DMIC3, 0);
	if (0 != error) {
		dev_err(dev, "Enable DMIC3 %d", error);
		return error;
	}
}

	/* Enable AD4 for Dmic4 */
	if (channel_index & e_CHANNEL_2) {
		/* Check if DMIC4 is already powered up */
		if (initialVal_AD & (EN_AD4))
			return 0;


		error = HW_ACODEC_MODIFY_WRITE(AD_ALLOCATION_TO_SLOT2_3_REG,
						(DATA_FROM_AD_OUT4<<4), 0);
		if (0 != error) {
			dev_err(dev, "Slot 03 outputs data from AD_OUT4 %d",
									error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_AD_CHANNELS_ENABLE_REG,
								EN_AD4, 0);
		if (0 != error) {
			dev_err(dev, "Enable AD4 for DMIC4 %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DMIC_ENABLE_REG, EN_DMIC4, 0);
		if (0 != error) {
			dev_err(dev, "Enable DMIC4 %d", error);
			return error;
		}
	}
	return error;
}

int ste_audio_io_power_down_dmic34(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal_AD = 0;


	if (!(channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		dev_err(dev, "DMic34 does not support more than 2 channels");
		return -EINVAL;
	}

	/* Setting Direction for GPIO pins on AB8500 */
	error = HW_ACODEC_MODIFY_WRITE(AB8500_GPIO_DIR4_REG, 0,
							GPIO29_DIR_OUTPUT);
	if (0 != error) {
		dev_err(dev, "Clearing Direction for GPIO pins on AB8500 %d",
									error);
		return error;
	}

	/* Enable AD1 for Dmic1 */
	if (channel_index & e_CHANNEL_1) {
		/* Check if DMIC3 is already powered Down or used by Mic1A
							or Mic1B */
		initialVal_AD = HW_REG_READ(DIGITAL_AD_CHANNELS_ENABLE_REG);
		if (!(initialVal_AD & EN_AD3))
			return 0;

		error = HW_ACODEC_MODIFY_WRITE(DMIC_ENABLE_REG, 0, EN_DMIC3);
		if (0 != error) {
			dev_err(dev, "Enable DMIC3 %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_AD_CHANNELS_ENABLE_REG,
								0,
								EN_AD3);
		if (0 != error) {
			dev_err(dev, "Disable AD3 for DMIC3 %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(AD_ALLOCATION_TO_SLOT2_3_REG, 0,
							DATA_FROM_AD_OUT3);
		if (0 != error) {
			dev_err(dev,
			"Slot 02 outputs data cleared from AD_OUT3 %d",
									error);
			return error;
		}
	}

	/* Enable AD4 for Dmic4 */
	if (channel_index & e_CHANNEL_2) {
		/* Check if DMIC4 is already powered down */
		initialVal_AD = HW_REG_READ(DIGITAL_AD_CHANNELS_ENABLE_REG);
		if (!(initialVal_AD & EN_AD4))
			return 0;

		error = HW_ACODEC_MODIFY_WRITE(DMIC_ENABLE_REG, 0, EN_DMIC4);
		if (0 != error) {
			dev_err(dev, "Enable DMIC4 %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(DIGITAL_AD_CHANNELS_ENABLE_REG,
							0, EN_AD4);
		if (0 != error) {
			dev_err(dev, "Disable AD4 for DMIC4 %d", error);
			return error;
		}

		error = HW_ACODEC_MODIFY_WRITE(AD_ALLOCATION_TO_SLOT2_3_REG, 0,
						(DATA_FROM_AD_OUT4<<4));
		if (0 != error) {
			dev_err(dev,
			"Slot 03 outputs data cleared from AD_OUT4 %d",
									error);
			return error;
		}
	}
	return error;
}
int ste_audio_io_set_dmic34_gain(enum AUDIOIO_CH_INDEX channel_index,
			u16 gain_index, int gain_value, u32 linear,
					struct device *dev)
{
	int error = 0;
	unsigned char initial_val = 0;

	if (channel_index & e_CHANNEL_1) {
		/* Set Gain Dmic3 */
		if (gain_index == 0) {
			int gain = 0;
			gain = 31 - gain_value;

			initial_val = HW_REG_READ(AD3_DIGITAL_GAIN_REG);
			error = HW_REG_WRITE(AD3_DIGITAL_GAIN_REG,
						((initial_val
			& (~DIGITAL_GAIN_MASK)) | (gain & DIGITAL_GAIN_MASK)));
			if (0 != error) {
				dev_err(dev,
					"Set Gain DMic3 gain index = %d %d",
							gain_index, error);
				return error;
			}
		}
	}

	if (channel_index & e_CHANNEL_2) {
		/* Set Gain Dmic4 */
		if (gain_index == 0) {
			int gain = 0;
			gain = 31 - gain_value;

			initial_val = HW_REG_READ(AD4_DIGITAL_GAIN_REG);
			error = HW_REG_WRITE(AD4_DIGITAL_GAIN_REG,
						((initial_val
			& (~DIGITAL_GAIN_MASK)) | (gain & DIGITAL_GAIN_MASK)));

			if (0 != error) {
				dev_err(dev,
					"Set Gain DMic4 gain index = %d %d",
							gain_index, error);
				return error;
			}
		}
	}

	return error;
}
int ste_audio_io_mute_dmic34(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	if ((channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		error = ste_audio_io_set_dmic34_gain(channel_index, 0, -32,
									0, dev);
		if (0 != error) {
			dev_err(dev, "Mute dmic34 %d", error);
			return error;
		}
	}
	return error;
}
int ste_audio_io_unmute_dmic34(enum AUDIOIO_CH_INDEX channel_index, int *gain,
					struct device *dev)
{
	int error = 0;
	if ((channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		error = ste_audio_io_set_dmic34_gain(channel_index,
					0, gain[0], 0, dev);
		if (0 != error) {
			dev_err(dev, "UnMute dmic34 %d", error);
			return error;
		}
	}
	return error;
}
int ste_audio_io_enable_fade_dmic34(struct device *dev)
{
	return 0;
}

int ste_audio_io_disable_fade_dmic34(struct device *dev)
{
	return 0;
}

int ste_audio_io_power_up_dmic56(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	return 0;
}
int ste_audio_io_power_down_dmic56(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	return 0;
}
int ste_audio_io_set_dmic56_gain(enum AUDIOIO_CH_INDEX channel_index,
			u16 gain_index, int gain_value, u32 linear,
					struct device *dev)
{
	return 0;
}
int ste_audio_io_mute_dmic56(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	return 0;
}
int ste_audio_io_unmute_dmic56(enum AUDIOIO_CH_INDEX channel_index, int *gain,
					struct device *dev)
{
	return 0;
}
int ste_audio_io_enable_fade_dmic56(struct device *dev)
{
	return 0;
}

int ste_audio_io_disable_fade_dmic56(struct device *dev)
{
	return 0;
}

int ste_audio_io_configure_if1(struct device *dev)
{
	int error = 0;

	error = HW_REG_WRITE(IF1_CONF_REG, IF_DELAYED |
			I2S_LEFT_ALIGNED_FORMAT | WORD_LENGTH_16);
	if (error != 0) {
		dev_err(dev,
		"Configure IF1: I2S Format 16 Bits word length error = %d",
							error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(TDM_IF_BYPASS_B_FIFO_REG, IF1_MASTER, 0);
	if (error != 0) {
		dev_err(dev,
			"Configure IF1: IF1 master error = %d",
								error);
		return error;
	}

	error = HW_ACODEC_MODIFY_WRITE(IF0_IF1_MASTER_CONF_REG,
							EN_FSYNC_BITCLK1, 0);
	if (error != 0) {
		dev_err(dev,
	"ConfigIF1 bitclk is 32x48KHz, enable Fsync1 and Bitclk1 error = %d",
								error);
		return error;
	}
	return error;
}

int ste_audio_io_power_up_fmrx(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal = 0;
	if (!(channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		dev_err(dev, "FMRX should have mono or stereo channels");
		return -EINVAL;
	}

	ste_audio_io_configure_if1(dev);

	if (channel_index & e_CHANNEL_1) {
		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA7_REG,
							SLOT24_FOR_DA_PATH, 0);
		if (0 != error) {
			dev_err(dev, "Data sent to DA_IN7 from Slot 24 %d",
									error);
			return error;
		}
		/* DA_IN7 to AD_OUT8 path */
		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA5_REG,
						SEL_AD_OUT8_FROM_DAIN7, 0);
		if (0 != error) {
			dev_err(dev, "Data sent to AD_OUT5 from DA_IN7 %d",
									error);
			return error;
		}

		initialVal = HW_REG_READ(AD_ALLOCATION_TO_SLOT6_7_REG);
		error = HW_REG_WRITE(AD_ALLOCATION_TO_SLOT6_7_REG,
			((initialVal & MASK_QUARTET1)|SEL_IF6_FROM_AD_OUT5));
		if (0 != error) {
			dev_err(dev, "Data sent to IF slot 6 from AD_OUT5 %d",
									error);
			return error;
		}
	}

	if (channel_index & e_CHANNEL_2) {
		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA8_REG,
							SLOT25_FOR_DA_PATH, 0);
		if (0 != error) {
			dev_err(dev, "Data sent to DA_IN8 from Slot 25 %d",
									error);
			return error;
		}

		/* DA_IN7 to AD_OUT8 path */
		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA6_REG,
						SEL_AD_OUT6_FROM_DAIN8, 0);
		if (0 != error) {
			dev_err(dev, "Data sent to AD_OUT6 from DA_IN8 %d",
									error);
			return error;
		}

		initialVal = HW_REG_READ(AD_ALLOCATION_TO_SLOT6_7_REG);

		error = HW_REG_WRITE(AD_ALLOCATION_TO_SLOT6_7_REG,
		(initialVal & MASK_QUARTET0)|SEL_IF7_FROM_AD_OUT6);
							/* 5x is written */
		if (0 != error) {
			dev_err(dev, "Data sent to IF7 from AD_OUT6  %d",
									error);
			return error;
		}
	}
	return error;
}
int ste_audio_io_power_down_fmrx(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;

	if (!(channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		dev_err(dev, "FMRX should have mono or stereo channels");
		return -EINVAL;
	}
	if (channel_index & e_CHANNEL_1) {
		/* data sent to DA7 input of DA filter form IF1 */
		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA7_REG, 0,
							SLOT24_FOR_DA_PATH);
	if (0 != error) {
		dev_err(dev, "Clearing Data sent to DA_IN7 from Slot 24 %d",
									error);
		return error;
	}
	/* DA_IN7 to AD_OUT8 path */
	error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA5_REG, 0,
						SEL_AD_OUT8_FROM_DAIN7);
	if (0 != error) {
		dev_err(dev, "Clearing Data sent to AD_OUT5 from DA_IN7 %d",
									error);
		return error;
	}
	error = HW_ACODEC_MODIFY_WRITE(AD_ALLOCATION_TO_SLOT6_7_REG, 0,
						SEL_IF6_FROM_AD_OUT5);
	if (0 != error) {
		dev_err(dev,
			"Clearing Data sent to IF slot 6 from AD_OUT5 %d",
									error);
		return error;
	}
}

	if (channel_index & e_CHANNEL_2) {
		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA8_REG, 0,
						SLOT25_FOR_DA_PATH);
		if (0 != error) {
			dev_err(dev,
			"Clearing Data sent to DA_IN8 from Slot 25 %d",
									error);
			return error;
		}

		/* DA_IN7 to AD_OUT8 path */
		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA6_REG, 0,
						SEL_AD_OUT6_FROM_DAIN8);
		if (0 != error) {
			dev_err(dev,
			"Clearing Data sent to AD_OUT6 from DA_IN8 %d",
									error);
			return error;
			}
		error = HW_ACODEC_MODIFY_WRITE(AD_ALLOCATION_TO_SLOT6_7_REG, 0,
							SEL_IF7_FROM_AD_OUT6);
		if (0 != error) {
			dev_err(dev,
				"Clearing Data sent to IF7 from AD_OUT6 %d",
									error);
			return error;
		}
	}
	return error;
}

int ste_audio_io_power_up_fmtx(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal = 0;

	if (!(channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		dev_err(dev, "FMTX should have mono or stereo channels");
		return -EINVAL;
	}

	ste_audio_io_configure_if1(dev);

	if (channel_index & e_CHANNEL_1) {
		/* data sent to DA7 input of DA filter form IF1 14 slot */
		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA7_REG,
							SLOT14_FOR_DA_PATH, 0);
		if (0 != error) {
			dev_err(dev,
				"Data sent to DA_IN7 from Slot 14 %d", error);
			return error;
		}
		/* DA_IN7 to AD_OUT5 path */
		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA5_REG,
						SEL_AD_OUT5_FROM_DAIN7, 0);
		if (0 != error) {
			dev_err(dev, "Data sent to AD_OUT5 from DA_IN7 %d",
									error);
			return error;
		}

		initialVal = HW_REG_READ(AD_ALLOCATION_TO_SLOT16_17_REG);
		error = HW_REG_WRITE(AD_ALLOCATION_TO_SLOT16_17_REG,
			(initialVal & MASK_QUARTET1)|SEL_IF6_FROM_AD_OUT5);
		if (0 != error) {
			dev_err(dev, "Data sent to IF16 from AD_OUT5  %d",
									error);
			return error;
		}
	}

	if (channel_index & e_CHANNEL_2) {
		/* data sent to DA8 input of DA filter */
		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA8_REG,
							SLOT15_FOR_DA_PATH, 0);
		if (0 != error) {
			dev_err(dev, "Data sent to DA_IN8 from  Slot 15 %d",
									error);
			return error;
		}

		/* DA_IN8 to AD_OUT6 path */
		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA6_REG,
						SEL_AD_OUT6_FROM_DAIN8, 0);
		if (0 != error) {
			dev_err(dev, "Data sent to AD_OUT6 from DA_IN8 %d",
									error);
			return error;
		}

		initialVal = HW_REG_READ(AD_ALLOCATION_TO_SLOT16_17_REG);
		error = HW_REG_WRITE(AD_ALLOCATION_TO_SLOT16_17_REG,
			(initialVal & MASK_QUARTET0)|SEL_IF17_FROM_AD_OUT6);
		if (0 != error) {
			dev_err(dev, "Data sent to IF17 from AD_OUT6  %d",
								error);
			return error;
		}
	}
	return error;
}

int ste_audio_io_power_down_fmtx(enum AUDIOIO_CH_INDEX channel_index,
					struct device *dev)
{
	int error = 0;
	unsigned char initialVal_AD = 0;

	if (!(channel_index & (e_CHANNEL_1 | e_CHANNEL_2))) {
		dev_err(dev, "FMTX should have mono or stereo channels");
		return -EINVAL;
	}

	if (channel_index & e_CHANNEL_1) {
		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA7_REG, 0,
							SLOT14_FOR_DA_PATH);
		if (0 != error) {
			dev_err(dev,
			"Clearing Data sent to DA_IN7 from Slot 14 %d",
									error);
			return error;
		}
		/* DA_IN7 to AD_OUT8 path */
		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA5_REG, 0,
						SEL_AD_OUT5_FROM_DAIN7);
		if (0 != error) {
			dev_err(dev,
			"Clearing Data sent to AD_OUT5 from DA_IN7 %d",
								error);
			return error;
		}
		error = HW_REG_WRITE(AD_ALLOCATION_TO_SLOT16_17_REG,
							SEL_IF6_FROM_AD_OUT5);
		if (0 != error) {
			dev_err(dev,
				"Clearing Data sent to IF16 from AD_OUT8 %d",
								error);
			return error;
		}
	}

	 if (channel_index & e_CHANNEL_2) {
		/* data sent to DA8 input of DA filter */
		initialVal_AD = HW_REG_READ(SLOT_SELECTION_TO_DA8_REG);

		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA8_REG, 0,
							SLOT15_FOR_DA_PATH);
		if (0 != error) {
			dev_err(dev,
			"Clearing Data sent to DA_IN8 from Slot 15 %d",
									error);
			return error;
		}

		/* DA_IN7 to AD_OUT8 path */
		error = HW_ACODEC_MODIFY_WRITE(SLOT_SELECTION_TO_DA6_REG, 0,
						SEL_AD_OUT6_FROM_DAIN8);
		if (0 != error) {
			dev_err(dev,
			"Clearing Data sent to AD_OUT6 from DA_IN8 %d",
									error);
			return error;
		}
		error = HW_REG_WRITE(AD_ALLOCATION_TO_SLOT16_17_REG,
							SEL_IF17_FROM_AD_OUT6);
		if (0 != error) {
			dev_err(dev,
				"Clearing Data sent to IF17 from AD_OUT6 %d",
									error);
			return error;
		}
	}
	return error;
}
int ste_audio_io_power_up_bluetooth(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev)
{
	int error = 0;
	struct ab8500 *ab8500 = dev_get_drvdata(dev->parent);
	struct ab8500_platform_data *pdata = dev_get_platdata(ab8500->dev);
	if (bluetooth_power_up_count++)
		return error;

	if (pdata) {
		if (pdata->audio) {
			error = pdata->audio->ste_gpio_altf_init();
			if (error == 0) {
				clk_ptr_msp0 = clk_get_sys("msp0", NULL);
				if (!IS_ERR(clk_ptr_msp0)) {
					error = clk_enable(clk_ptr_msp0);
					return error;
				} else
					return -EFAULT;
			}
		}
	}
	return error;
}

int ste_audio_io_power_down_bluetooth(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev)
{
	int error = 0;
	struct ab8500 *ab8500 = dev_get_drvdata(dev->parent);
	struct ab8500_platform_data *pdata = dev_get_platdata(ab8500->dev);

	if (--bluetooth_power_up_count)
		return error;

	if (pdata) {
		if (pdata->audio) {
			error = pdata->audio->ste_gpio_altf_exit();
			if (error == 0) {
				clk_disable(clk_ptr_msp0);
				clk_put(clk_ptr_msp0);
			}
		}
	}
	return error;
}

int dump_acodec_registers(const char *str, struct device *dev)
{
	int reg_count = REVISION_REG & 0xff;
	if (1 == acodec_reg_dump) {
		u8 i = 0;
		dev_info(dev, "\n func  : %s\n", str);
		for (i = 0; i <= reg_count; i++)
			dev_info(dev,
				"block = 0x0D, adr = %x = %x\n",
				i, HW_REG_READ((AB8500_AUDIO << 8) | i));
	}
	str = str; /* keep compiler happy */
	return 0;
}

int debug_audioio(int x)
{

	if (1 == x)
		acodec_reg_dump = 1;
	else
		acodec_reg_dump = 0;
	return 0;
}





