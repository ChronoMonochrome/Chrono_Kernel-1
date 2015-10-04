/*
 *  Driver for the NXP SAA7164 PCIe bridge
 *
 *  Copyright (c) 2010 Steven Toth <stoth@kernellabs.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/wait.h>
#include <linux/slab.h>

#include "saa7164.h"

int saa7164_api_get_load_info(struct saa7164_dev *dev, struct tmFwInfoStruct *i)
{
	int ret;

	if (!(saa_debug & DBGLVL_CPU))
		return 0;

;

	i->deviceinst = 0;
	i->devicespec = 0;
	i->mode = 0;
	i->status = 0;

	ret = saa7164_cmd_send(dev, 0, GET_CUR,
		GET_FW_STATUS_CONTROL, sizeof(struct tmFwInfoStruct), i);
	if (ret != SAA_OK)
;

;

	return ret;
}

int saa7164_api_collect_debug(struct saa7164_dev *dev)
{
	struct tmComResDebugGetData d;
	u8 more = 255;
	int ret;

;

	while (more--) {

		memset(&d, 0, sizeof(d));

		ret = saa7164_cmd_send(dev, 0, GET_CUR,
			GET_DEBUG_DATA_CONTROL, sizeof(d), &d);
		if (ret != SAA_OK)
//			printk(KERN_ERR "%s() error, ret = 0x%x\n",
;

		if (d.dwResult != SAA_OK)
			break;

//		printk(KERN_INFO "saa7164[%d]-FWMSG: %s", dev->nr,
;
	}

	return 0;
}

int saa7164_api_set_debug(struct saa7164_dev *dev, u8 level)
{
	struct tmComResDebugSetLevel lvl;
	int ret;

;

	/* Retrieve current state */
	ret = saa7164_cmd_send(dev, 0, GET_CUR,
		SET_DEBUG_LEVEL_CONTROL, sizeof(lvl), &lvl);
	if (ret != SAA_OK)
;

;

	lvl.dwDebugLevel = level;

	/* set new state */
	ret = saa7164_cmd_send(dev, 0, SET_CUR,
		SET_DEBUG_LEVEL_CONTROL, sizeof(lvl), &lvl);
	if (ret != SAA_OK)
;

	return ret;
}

int saa7164_api_set_vbi_format(struct saa7164_port *port)
{
	struct saa7164_dev *dev = port->dev;
	struct tmComResProbeCommit fmt, rsp;
	int ret;

//	dprintk(DBGLVL_API, "%s(nr=%d, unitid=0x%x)\n", __func__,
;

	fmt.bmHint = 0;
	fmt.bFormatIndex = 1;
	fmt.bFrameIndex = 1;

	/* Probe, see if it can support this format */
	ret = saa7164_cmd_send(port->dev, port->hwcfg.unitid,
		SET_CUR, SAA_PROBE_CONTROL, sizeof(fmt), &fmt);
	if (ret != SAA_OK)
;

	/* See of the format change was successful */
	ret = saa7164_cmd_send(port->dev, port->hwcfg.unitid,
		GET_CUR, SAA_PROBE_CONTROL, sizeof(rsp), &rsp);
	if (ret != SAA_OK) {
;
	} else {
		/* Compare requested vs received, should be same */
		if (memcmp(&fmt, &rsp, sizeof(rsp)) == 0) {
;

			/* Ask the device to select the negotiated format */
			ret = saa7164_cmd_send(port->dev, port->hwcfg.unitid,
				SET_CUR, SAA_COMMIT_CONTROL, sizeof(fmt), &fmt);
			if (ret != SAA_OK)
//				printk(KERN_ERR "%s() commit error, ret = 0x%x\n",
;

			ret = saa7164_cmd_send(port->dev, port->hwcfg.unitid,
				GET_CUR, SAA_COMMIT_CONTROL, sizeof(rsp), &rsp);
			if (ret != SAA_OK)
//				printk(KERN_ERR "%s() GET commit error, ret = 0x%x\n",
;

			if (memcmp(&fmt, &rsp, sizeof(rsp)) != 0) {
//				printk(KERN_ERR "%s() memcmp error, ret = 0x%x\n",
;
			} else
;

;
//			dprintk(DBGLVL_API, "rsp.bFormatIndex = 0x%x\n",
;
//			dprintk(DBGLVL_API, "rsp.bFrameIndex = 0x%x\n",
;
		} else
;
	}

	if (ret == SAA_OK)
;

	return ret;
}

int saa7164_api_set_gop_size(struct saa7164_port *port)
{
	struct saa7164_dev *dev = port->dev;
	struct tmComResEncVideoGopStructure gs;
	int ret;

;

	gs.ucRefFrameDist = port->encoder_params.refdist;
	gs.ucGOPSize = port->encoder_params.gop_size;
	ret = saa7164_cmd_send(port->dev, port->hwcfg.sourceid, SET_CUR,
		EU_VIDEO_GOP_STRUCTURE_CONTROL,
		sizeof(gs), &gs);
	if (ret != SAA_OK)
;

	return ret;
}

int saa7164_api_set_encoder(struct saa7164_port *port)
{
	struct saa7164_dev *dev = port->dev;
	struct tmComResEncVideoBitRate vb;
	struct tmComResEncAudioBitRate ab;
	int ret;

//	dprintk(DBGLVL_ENC, "%s() unitid=0x%x\n", __func__,
;

	if (port->encoder_params.stream_type == V4L2_MPEG_STREAM_TYPE_MPEG2_PS)
		port->encoder_profile = EU_PROFILE_PS_DVD;
	else
		port->encoder_profile = EU_PROFILE_TS_HQ;

	ret = saa7164_cmd_send(port->dev, port->hwcfg.sourceid, SET_CUR,
		EU_PROFILE_CONTROL, sizeof(u8), &port->encoder_profile);
	if (ret != SAA_OK)
;

	/* Resolution */
	ret = saa7164_cmd_send(port->dev, port->hwcfg.sourceid, SET_CUR,
		EU_PROFILE_CONTROL, sizeof(u8), &port->encoder_profile);
	if (ret != SAA_OK)
;

	/* Establish video bitrates */
	if (port->encoder_params.bitrate_mode ==
		V4L2_MPEG_VIDEO_BITRATE_MODE_CBR)
		vb.ucVideoBitRateMode = EU_VIDEO_BIT_RATE_MODE_CONSTANT;
	else
		vb.ucVideoBitRateMode = EU_VIDEO_BIT_RATE_MODE_VARIABLE_PEAK;
	vb.dwVideoBitRate = port->encoder_params.bitrate;
	vb.dwVideoBitRatePeak = port->encoder_params.bitrate_peak;
	ret = saa7164_cmd_send(port->dev, port->hwcfg.sourceid, SET_CUR,
		EU_VIDEO_BIT_RATE_CONTROL,
		sizeof(struct tmComResEncVideoBitRate),
		&vb);
	if (ret != SAA_OK)
;

	/* Establish audio bitrates */
	ab.ucAudioBitRateMode = 0;
	ab.dwAudioBitRate = 384000;
	ab.dwAudioBitRatePeak = ab.dwAudioBitRate;
	ret = saa7164_cmd_send(port->dev, port->hwcfg.sourceid, SET_CUR,
		EU_AUDIO_BIT_RATE_CONTROL,
		sizeof(struct tmComResEncAudioBitRate),
		&ab);
	if (ret != SAA_OK)
//		printk(KERN_ERR "%s() error, ret = 0x%x\n", __func__,
;

	saa7164_api_set_aspect_ratio(port);
	saa7164_api_set_gop_size(port);

	return ret;
}

int saa7164_api_get_encoder(struct saa7164_port *port)
{
	struct saa7164_dev *dev = port->dev;
	struct tmComResEncVideoBitRate v;
	struct tmComResEncAudioBitRate a;
	struct tmComResEncVideoInputAspectRatio ar;
	int ret;

//	dprintk(DBGLVL_ENC, "%s() unitid=0x%x\n", __func__,
;

	port->encoder_profile = 0;
	port->video_format = 0;
	port->video_resolution = 0;
	port->audio_format = 0;

	ret = saa7164_cmd_send(port->dev, port->hwcfg.sourceid, GET_CUR,
		EU_PROFILE_CONTROL, sizeof(u8), &port->encoder_profile);
	if (ret != SAA_OK)
;

	ret = saa7164_cmd_send(port->dev, port->hwcfg.sourceid, GET_CUR,
		EU_VIDEO_RESOLUTION_CONTROL, sizeof(u8),
		&port->video_resolution);
	if (ret != SAA_OK)
;

	ret = saa7164_cmd_send(port->dev, port->hwcfg.sourceid, GET_CUR,
		EU_VIDEO_FORMAT_CONTROL, sizeof(u8), &port->video_format);
	if (ret != SAA_OK)
;

	ret = saa7164_cmd_send(port->dev, port->hwcfg.sourceid, GET_CUR,
		EU_VIDEO_BIT_RATE_CONTROL, sizeof(v), &v);
	if (ret != SAA_OK)
;

	ret = saa7164_cmd_send(port->dev, port->hwcfg.sourceid, GET_CUR,
		EU_AUDIO_FORMAT_CONTROL, sizeof(u8), &port->audio_format);
	if (ret != SAA_OK)
;

	ret = saa7164_cmd_send(port->dev, port->hwcfg.sourceid, GET_CUR,
		EU_AUDIO_BIT_RATE_CONTROL, sizeof(a), &a);
	if (ret != SAA_OK)
;

	/* Aspect Ratio */
	ar.width = 0;
	ar.height = 0;
	ret = saa7164_cmd_send(port->dev, port->hwcfg.sourceid, GET_CUR,
		EU_VIDEO_INPUT_ASPECT_CONTROL,
		sizeof(struct tmComResEncVideoInputAspectRatio), &ar);
	if (ret != SAA_OK)
;

;
;
;
;
//	dprintk(DBGLVL_ENC, "v.ucVideoBitRateMode = %d\n",
;
//	dprintk(DBGLVL_ENC, "v.dwVideoBitRate     = %d\n",
;
//	dprintk(DBGLVL_ENC, "v.dwVideoBitRatePeak = %d\n",
;
//	dprintk(DBGLVL_ENC, "a.ucVideoBitRateMode = %d\n",
;
//	dprintk(DBGLVL_ENC, "a.dwVideoBitRate     = %d\n",
;
//	dprintk(DBGLVL_ENC, "a.dwVideoBitRatePeak = %d\n",
;
//	dprintk(DBGLVL_ENC, "aspect.width / height = %d:%d\n",
;

	return ret;
}

int saa7164_api_set_aspect_ratio(struct saa7164_port *port)
{
	struct saa7164_dev *dev = port->dev;
	struct tmComResEncVideoInputAspectRatio ar;
	int ret;

//	dprintk(DBGLVL_ENC, "%s(%d)\n", __func__,
;

	switch (port->encoder_params.ctl_aspect) {
	case V4L2_MPEG_VIDEO_ASPECT_1x1:
		ar.width = 1;
		ar.height = 1;
		break;
	case V4L2_MPEG_VIDEO_ASPECT_4x3:
		ar.width = 4;
		ar.height = 3;
		break;
	case V4L2_MPEG_VIDEO_ASPECT_16x9:
		ar.width = 16;
		ar.height = 9;
		break;
	case V4L2_MPEG_VIDEO_ASPECT_221x100:
		ar.width = 221;
		ar.height = 100;
		break;
	default:
		BUG();
	}

//	dprintk(DBGLVL_ENC, "%s(%d) now %d:%d\n", __func__,
//		port->encoder_params.ctl_aspect,
;

	/* Aspect Ratio */
	ret = saa7164_cmd_send(port->dev, port->hwcfg.sourceid, SET_CUR,
		EU_VIDEO_INPUT_ASPECT_CONTROL,
		sizeof(struct tmComResEncVideoInputAspectRatio), &ar);
	if (ret != SAA_OK)
;

	return ret;
}

int saa7164_api_set_usercontrol(struct saa7164_port *port, u8 ctl)
{
	struct saa7164_dev *dev = port->dev;
	int ret;
	u16 val;

	if (ctl == PU_BRIGHTNESS_CONTROL)
		val = port->ctl_brightness;
	else
	if (ctl == PU_CONTRAST_CONTROL)
		val = port->ctl_contrast;
	else
	if (ctl == PU_HUE_CONTROL)
		val = port->ctl_hue;
	else
	if (ctl == PU_SATURATION_CONTROL)
		val = port->ctl_saturation;
	else
	if (ctl == PU_SHARPNESS_CONTROL)
		val = port->ctl_sharpness;
	else
		return -EINVAL;

//	dprintk(DBGLVL_ENC, "%s() unitid=0x%x ctl=%d, val=%d\n",
;

	ret = saa7164_cmd_send(port->dev, port->encunit.vsourceid, SET_CUR,
		ctl, sizeof(u16), &val);
	if (ret != SAA_OK)
;

	return ret;
}

int saa7164_api_get_usercontrol(struct saa7164_port *port, u8 ctl)
{
	struct saa7164_dev *dev = port->dev;
	int ret;
	u16 val;

	ret = saa7164_cmd_send(port->dev, port->encunit.vsourceid, GET_CUR,
		ctl, sizeof(u16), &val);
	if (ret != SAA_OK) {
;
		return ret;
	}

//	dprintk(DBGLVL_ENC, "%s() ctl=%d, val=%d\n",
;

	if (ctl == PU_BRIGHTNESS_CONTROL)
		port->ctl_brightness = val;
	else
	if (ctl == PU_CONTRAST_CONTROL)
		port->ctl_contrast = val;
	else
	if (ctl == PU_HUE_CONTROL)
		port->ctl_hue = val;
	else
	if (ctl == PU_SATURATION_CONTROL)
		port->ctl_saturation = val;
	else
	if (ctl == PU_SHARPNESS_CONTROL)
		port->ctl_sharpness = val;

	return ret;
}

int saa7164_api_set_videomux(struct saa7164_port *port)
{
	struct saa7164_dev *dev = port->dev;
	u8 inputs[] = { 1, 2, 2, 2, 5, 5, 5 };
	int ret;

//	dprintk(DBGLVL_ENC, "%s() v_mux=%d a_mux=%d\n",
;

	/* Audio Mute */
	ret = saa7164_api_audio_mute(port, 1);
	if (ret != SAA_OK)
;

	/* Video Mux */
	ret = saa7164_cmd_send(port->dev, port->vidproc.sourceid, SET_CUR,
		SU_INPUT_SELECT_CONTROL, sizeof(u8), &port->mux_input);
	if (ret != SAA_OK)
;

	/* Audio Mux */
	ret = saa7164_cmd_send(port->dev, port->audfeat.sourceid, SET_CUR,
		SU_INPUT_SELECT_CONTROL, sizeof(u8),
		&inputs[port->mux_input - 1]);
	if (ret != SAA_OK)
;

	/* Audio UnMute */
	ret = saa7164_api_audio_mute(port, 0);
	if (ret != SAA_OK)
;

	return ret;
}

int saa7164_api_audio_mute(struct saa7164_port *port, int mute)
{
	struct saa7164_dev *dev = port->dev;
	u8 v = mute;
	int ret;

;

	ret = saa7164_cmd_send(port->dev, port->audfeat.unitid, SET_CUR,
		MUTE_CONTROL, sizeof(u8), &v);
	if (ret != SAA_OK)
;

	return ret;
}

/* 0 = silence, 0xff = full */
int saa7164_api_set_audio_volume(struct saa7164_port *port, s8 level)
{
	struct saa7164_dev *dev = port->dev;
	s16 v, min, max;
	int ret;

;

	/* Obtain the min/max ranges */
	ret = saa7164_cmd_send(port->dev, port->audfeat.unitid, GET_MIN,
		VOLUME_CONTROL, sizeof(u16), &min);
	if (ret != SAA_OK)
;

	ret = saa7164_cmd_send(port->dev, port->audfeat.unitid, GET_MAX,
		VOLUME_CONTROL, sizeof(u16), &max);
	if (ret != SAA_OK)
;

	ret = saa7164_cmd_send(port->dev, port->audfeat.unitid, GET_CUR,
		(0x01 << 8) | VOLUME_CONTROL, sizeof(u16), &v);
	if (ret != SAA_OK)
;

//	dprintk(DBGLVL_API, "%s(%d) min=%d max=%d cur=%d\n", __func__,
;

	v = level;
	if (v < min)
		v = min;
	if (v > max)
		v = max;

	/* Left */
	ret = saa7164_cmd_send(port->dev, port->audfeat.unitid, SET_CUR,
		(0x01 << 8) | VOLUME_CONTROL, sizeof(s16), &v);
	if (ret != SAA_OK)
;

	/* Right */
	ret = saa7164_cmd_send(port->dev, port->audfeat.unitid, SET_CUR,
		(0x02 << 8) | VOLUME_CONTROL, sizeof(s16), &v);
	if (ret != SAA_OK)
;

	ret = saa7164_cmd_send(port->dev, port->audfeat.unitid, GET_CUR,
		(0x01 << 8) | VOLUME_CONTROL, sizeof(u16), &v);
	if (ret != SAA_OK)
;

//	dprintk(DBGLVL_API, "%s(%d) min=%d max=%d cur=%d\n", __func__,
;

	return ret;
}

int saa7164_api_set_audio_std(struct saa7164_port *port)
{
	struct saa7164_dev *dev = port->dev;
	struct tmComResAudioDefaults lvl;
	struct tmComResTunerStandard tvaudio;
	int ret;

;

	/* Establish default levels */
	lvl.ucDecoderLevel = TMHW_LEV_ADJ_DECLEV_DEFAULT;
	lvl.ucDecoderFM_Level = TMHW_LEV_ADJ_DECLEV_DEFAULT;
	lvl.ucMonoLevel = TMHW_LEV_ADJ_MONOLEV_DEFAULT;
	lvl.ucNICAM_Level = TMHW_LEV_ADJ_NICLEV_DEFAULT;
	lvl.ucSAP_Level = TMHW_LEV_ADJ_SAPLEV_DEFAULT;
	lvl.ucADC_Level = TMHW_LEV_ADJ_ADCLEV_DEFAULT;
	ret = saa7164_cmd_send(port->dev, port->audfeat.unitid, SET_CUR,
		AUDIO_DEFAULT_CONTROL, sizeof(struct tmComResAudioDefaults),
		&lvl);
	if (ret != SAA_OK)
;

	/* Manually select the appropriate TV audio standard */
	if (port->encodernorm.id & V4L2_STD_NTSC) {
		tvaudio.std = TU_STANDARD_NTSC_M;
		tvaudio.country = 1;
	} else {
		tvaudio.std = TU_STANDARD_PAL_I;
		tvaudio.country = 44;
	}

	ret = saa7164_cmd_send(port->dev, port->tunerunit.unitid, SET_CUR,
		TU_STANDARD_CONTROL, sizeof(tvaudio), &tvaudio);
	if (ret != SAA_OK)
//		printk(KERN_ERR "%s() TU_STANDARD_CONTROL error, ret = 0x%x\n",
;
	return ret;
}

int saa7164_api_set_audio_detection(struct saa7164_port *port, int autodetect)
{
	struct saa7164_dev *dev = port->dev;
	struct tmComResTunerStandardAuto p;
	int ret;

;

	/* Disable TV Audio autodetect if not already set (buggy) */
	if (autodetect)
		p.mode = TU_STANDARD_AUTO;
	else
		p.mode = TU_STANDARD_MANUAL;
	ret = saa7164_cmd_send(port->dev, port->tunerunit.unitid, SET_CUR,
		TU_STANDARD_AUTO_CONTROL, sizeof(p), &p);
	if (ret != SAA_OK)
//		printk(KERN_ERR
//			"%s() TU_STANDARD_AUTO_CONTROL error, ret = 0x%x\n",
;

	return ret;
}

int saa7164_api_get_videomux(struct saa7164_port *port)
{
	struct saa7164_dev *dev = port->dev;
	int ret;

	ret = saa7164_cmd_send(port->dev, port->vidproc.sourceid, GET_CUR,
		SU_INPUT_SELECT_CONTROL, sizeof(u8), &port->mux_input);
	if (ret != SAA_OK)
;

//	dprintk(DBGLVL_ENC, "%s() v_mux=%d\n",
;

	return ret;
}

int saa7164_api_set_dif(struct saa7164_port *port, u8 reg, u8 val)
{
	struct saa7164_dev *dev = port->dev;

	u16 len = 0;
	u8 buf[256];
	int ret;
	u8 mas;

//	dprintk(DBGLVL_API, "%s(nr=%d type=%d val=%x)\n", __func__,
;

	if (port->nr == 0)
		mas = 0xd0;
	else
		mas = 0xe0;

	memset(buf, 0, sizeof(buf));

	buf[0x00] = 0x04;
	buf[0x01] = 0x00;
	buf[0x02] = 0x00;
	buf[0x03] = 0x00;

	buf[0x04] = 0x04;
	buf[0x05] = 0x00;
	buf[0x06] = 0x00;
	buf[0x07] = 0x00;

	buf[0x08] = reg;
	buf[0x09] = 0x26;
	buf[0x0a] = mas;
	buf[0x0b] = 0xb0;

	buf[0x0c] = val;
	buf[0x0d] = 0x00;
	buf[0x0e] = 0x00;
	buf[0x0f] = 0x00;

	ret = saa7164_cmd_send(dev, port->ifunit.unitid, GET_LEN,
		EXU_REGISTER_ACCESS_CONTROL, sizeof(len), &len);
	if (ret != SAA_OK) {
;
		return -EIO;
	}

	ret = saa7164_cmd_send(dev, port->ifunit.unitid, SET_CUR,
		EXU_REGISTER_ACCESS_CONTROL, len, &buf);
	if (ret != SAA_OK)
;
#if 0
	saa7164_dumphex16(dev, buf, 16);
#endif
	return ret == SAA_OK ? 0 : -EIO;
}

/* Disable the IF block AGC controls */
int saa7164_api_configure_dif(struct saa7164_port *port, u32 std)
{
	struct saa7164_dev *dev = port->dev;
	int ret = 0;
	u8 agc_disable;

;

	if (std & V4L2_STD_NTSC) {
;
		saa7164_api_set_dif(port, 0x00, 0x01); /* Video Standard */
		agc_disable = 0;
	} else if (std & V4L2_STD_PAL_I) {
;
		saa7164_api_set_dif(port, 0x00, 0x08); /* Video Standard */
		agc_disable = 0;
	} else if (std & V4L2_STD_PAL_M) {
;
		saa7164_api_set_dif(port, 0x00, 0x01); /* Video Standard */
		agc_disable = 0;
	} else if (std & V4L2_STD_PAL_N) {
;
		saa7164_api_set_dif(port, 0x00, 0x01); /* Video Standard */
		agc_disable = 0;
	} else if (std & V4L2_STD_PAL_Nc) {
;
		saa7164_api_set_dif(port, 0x00, 0x01); /* Video Standard */
		agc_disable = 0;
	} else if (std & V4L2_STD_PAL_B) {
;
		saa7164_api_set_dif(port, 0x00, 0x02); /* Video Standard */
		agc_disable = 0;
	} else if (std & V4L2_STD_PAL_DK) {
;
		saa7164_api_set_dif(port, 0x00, 0x10); /* Video Standard */
		agc_disable = 0;
	} else if (std & V4L2_STD_SECAM_L) {
;
		saa7164_api_set_dif(port, 0x00, 0x20); /* Video Standard */
		agc_disable = 0;
	} else {
		/* Unknown standard, assume DTV */
;
		/* Undefinded Video Standard */
		saa7164_api_set_dif(port, 0x00, 0x80);
		agc_disable = 1;
	}

	saa7164_api_set_dif(port, 0x48, 0xa0); /* AGC Functions 1 */
	saa7164_api_set_dif(port, 0xc0, agc_disable); /* AGC Output Disable */
	saa7164_api_set_dif(port, 0x7c, 0x04); /* CVBS EQ */
	saa7164_api_set_dif(port, 0x04, 0x01); /* Active */
	msleep(100);
	saa7164_api_set_dif(port, 0x04, 0x00); /* Active (again) */
	msleep(100);

	return ret;
}

/* Ensure the dif is in the correct state for the operating mode
 * (analog / dtv). We only configure the diff through the analog encoder
 * so when we're in digital mode we need to find the appropriate encoder
 * and use it to configure the DIF.
 */
int saa7164_api_initialize_dif(struct saa7164_port *port)
{
	struct saa7164_dev *dev = port->dev;
	struct saa7164_port *p = NULL;
	int ret = -EINVAL;
	u32 std = 0;

//	dprintk(DBGLVL_API, "%s(nr=%d type=%d)\n", __func__,
;

	if (port->type == SAA7164_MPEG_ENCODER) {
		/* Pick any analog standard to init the diff.
		 * we'll come back during encoder_init'
		 * and set the correct standard if requried.
		 */
		std = V4L2_STD_NTSC;
	} else
	if (port->type == SAA7164_MPEG_DVB) {
		if (port->nr == SAA7164_PORT_TS1)
			p = &dev->ports[SAA7164_PORT_ENC1];
		else
			p = &dev->ports[SAA7164_PORT_ENC2];
	} else
	if (port->type == SAA7164_MPEG_VBI) {
		std = V4L2_STD_NTSC;
		if (port->nr == SAA7164_PORT_VBI1)
			p = &dev->ports[SAA7164_PORT_ENC1];
		else
			p = &dev->ports[SAA7164_PORT_ENC2];
	} else
		BUG();

	if (p)
		ret = saa7164_api_configure_dif(p, std);

	return ret;
}

int saa7164_api_transition_port(struct saa7164_port *port, u8 mode)
{
	struct saa7164_dev *dev = port->dev;

	int ret;

//	dprintk(DBGLVL_API, "%s(nr=%d unitid=0x%x,%d)\n",
;

	ret = saa7164_cmd_send(port->dev, port->hwcfg.unitid, SET_CUR,
		SAA_STATE_CONTROL, sizeof(mode), &mode);
	if (ret != SAA_OK)
//		printk(KERN_ERR "%s(portnr %d unitid 0x%x) error, ret = 0x%x\n",
;

	return ret;
}

int saa7164_api_get_fw_version(struct saa7164_dev *dev, u32 *version)
{
	int ret;

	ret = saa7164_cmd_send(dev, 0, GET_CUR,
		GET_FW_VERSION_CONTROL, sizeof(u32), version);
	if (ret != SAA_OK)
;

	return ret;
}

int saa7164_api_read_eeprom(struct saa7164_dev *dev, u8 *buf, int buflen)
{
	u8 reg[] = { 0x0f, 0x00 };

	if (buflen < 128)
		return -ENOMEM;

	/* Assumption: Hauppauge eeprom is at 0xa0 on on bus 0 */
	/* TODO: Pull the details from the boards struct */
	return saa7164_api_i2c_read(&dev->i2c_bus[0], 0xa0 >> 1, sizeof(reg),
		&reg[0], 128, buf);
}

int saa7164_api_configure_port_vbi(struct saa7164_dev *dev,
	struct saa7164_port *port)
{
	struct tmComResVBIFormatDescrHeader *fmt = &port->vbi_fmt_ntsc;

;
;
;
;
;
;

	/* Cache the hardware configuration in the port */

	port->bufcounter = port->hwcfg.BARLocation;
	port->pitch = port->hwcfg.BARLocation + (2 * sizeof(u32));
	port->bufsize = port->hwcfg.BARLocation + (3 * sizeof(u32));
	port->bufoffset = port->hwcfg.BARLocation + (4 * sizeof(u32));
	port->bufptr32l = port->hwcfg.BARLocation +
		(4 * sizeof(u32)) +
		(sizeof(u32) * port->hwcfg.buffercount) + sizeof(u32);
	port->bufptr32h = port->hwcfg.BARLocation +
		(4 * sizeof(u32)) +
		(sizeof(u32) * port->hwcfg.buffercount);
	port->bufptr64 = port->hwcfg.BARLocation +
		(4 * sizeof(u32)) +
		(sizeof(u32) * port->hwcfg.buffercount);
//	dprintk(DBGLVL_API, "   = port->hwcfg.BARLocation = 0x%x\n",
;

//	dprintk(DBGLVL_API, "   = VS_FORMAT_VBI (becomes dev->en[%d])\n",
;

	return 0;
}

int saa7164_api_configure_port_mpeg2ts(struct saa7164_dev *dev,
	struct saa7164_port *port,
	struct tmComResTSFormatDescrHeader *tsfmt)
{
;
;
;
;
;

	/* Cache the hardware configuration in the port */

	port->bufcounter = port->hwcfg.BARLocation;
	port->pitch = port->hwcfg.BARLocation + (2 * sizeof(u32));
	port->bufsize = port->hwcfg.BARLocation + (3 * sizeof(u32));
	port->bufoffset = port->hwcfg.BARLocation + (4 * sizeof(u32));
	port->bufptr32l = port->hwcfg.BARLocation +
		(4 * sizeof(u32)) +
		(sizeof(u32) * port->hwcfg.buffercount) + sizeof(u32);
	port->bufptr32h = port->hwcfg.BARLocation +
		(4 * sizeof(u32)) +
		(sizeof(u32) * port->hwcfg.buffercount);
	port->bufptr64 = port->hwcfg.BARLocation +
		(4 * sizeof(u32)) +
		(sizeof(u32) * port->hwcfg.buffercount);
//	dprintk(DBGLVL_API, "   = port->hwcfg.BARLocation = 0x%x\n",
;

//	dprintk(DBGLVL_API, "   = VS_FORMAT_MPEGTS (becomes dev->ts[%d])\n",
;

	return 0;
}

int saa7164_api_configure_port_mpeg2ps(struct saa7164_dev *dev,
	struct saa7164_port *port,
	struct tmComResPSFormatDescrHeader *fmt)
{
;
;
;
;

	/* Cache the hardware configuration in the port */
	/* TODO: CHECK THIS in the port config */
	port->bufcounter = port->hwcfg.BARLocation;
	port->pitch = port->hwcfg.BARLocation + (2 * sizeof(u32));
	port->bufsize = port->hwcfg.BARLocation + (3 * sizeof(u32));
	port->bufoffset = port->hwcfg.BARLocation + (4 * sizeof(u32));
	port->bufptr32l = port->hwcfg.BARLocation +
		(4 * sizeof(u32)) +
		(sizeof(u32) * port->hwcfg.buffercount) + sizeof(u32);
	port->bufptr32h = port->hwcfg.BARLocation +
		(4 * sizeof(u32)) +
		(sizeof(u32) * port->hwcfg.buffercount);
	port->bufptr64 = port->hwcfg.BARLocation +
		(4 * sizeof(u32)) +
		(sizeof(u32) * port->hwcfg.buffercount);
//	dprintk(DBGLVL_API, "   = port->hwcfg.BARLocation = 0x%x\n",
;

//	dprintk(DBGLVL_API, "   = VS_FORMAT_MPEGPS (becomes dev->enc[%d])\n",
;

	return 0;
}

int saa7164_api_dump_subdevs(struct saa7164_dev *dev, u8 *buf, int len)
{
	struct saa7164_port *tsport = NULL;
	struct saa7164_port *encport = NULL;
	struct saa7164_port *vbiport = NULL;
	u32 idx, next_offset;
	int i;
	struct tmComResDescrHeader *hdr, *t;
	struct tmComResExtDevDescrHeader *exthdr;
	struct tmComResPathDescrHeader *pathhdr;
	struct tmComResAntTermDescrHeader *anttermhdr;
	struct tmComResTunerDescrHeader *tunerunithdr;
	struct tmComResDMATermDescrHeader *vcoutputtermhdr;
	struct tmComResTSFormatDescrHeader *tsfmt;
	struct tmComResPSFormatDescrHeader *psfmt;
	struct tmComResSelDescrHeader *psel;
	struct tmComResProcDescrHeader *pdh;
	struct tmComResAFeatureDescrHeader *afd;
	struct tmComResEncoderDescrHeader *edh;
	struct tmComResVBIFormatDescrHeader *vbifmt;
	u32 currpath = 0;

//	dprintk(DBGLVL_API,
//		"%s(?,?,%d) sizeof(struct tmComResDescrHeader) = %d bytes\n",
;

	for (idx = 0; idx < (len - sizeof(struct tmComResDescrHeader));) {

		hdr = (struct tmComResDescrHeader *)(buf + idx);

		if (hdr->type != CS_INTERFACE)
			return SAA_ERR_NOT_SUPPORTED;

;
		switch (hdr->subtype) {
		case GENERAL_REQUEST:
;
			break;
		case VC_TUNER_PATH:
;
			pathhdr = (struct tmComResPathDescrHeader *)(buf + idx);
//			dprintk(DBGLVL_API, "  pathid = 0x%x\n",
;
			currpath = pathhdr->pathid;
			break;
		case VC_INPUT_TERMINAL:
;
			anttermhdr =
				(struct tmComResAntTermDescrHeader *)(buf + idx);
//			dprintk(DBGLVL_API, "  terminalid   = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  terminaltype = 0x%x\n",
;
			switch (anttermhdr->terminaltype) {
			case ITT_ANTENNA:
;
				break;
			case LINE_CONNECTOR:
;
				break;
			case SPDIF_CONNECTOR:
;
				break;
			case COMPOSITE_CONNECTOR:
//				dprintk(DBGLVL_API,
;
				break;
			case SVIDEO_CONNECTOR:
;
				break;
			case COMPONENT_CONNECTOR:
//				dprintk(DBGLVL_API,
;
				break;
			case STANDARD_DMA:
;
				break;
			default:
//				dprintk(DBGLVL_API, "   = undefined (0x%x)\n",
;
			}
//			dprintk(DBGLVL_API, "  assocterminal= 0x%x\n",
;
//			dprintk(DBGLVL_API, "  iterminal    = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  controlsize  = 0x%x\n",
;
			break;
		case VC_OUTPUT_TERMINAL:
;
			vcoutputtermhdr =
				(struct tmComResDMATermDescrHeader *)(buf + idx);
//			dprintk(DBGLVL_API, "  unitid = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  terminaltype = 0x%x\n",
;
			switch (vcoutputtermhdr->terminaltype) {
			case ITT_ANTENNA:
;
				break;
			case LINE_CONNECTOR:
;
				break;
			case SPDIF_CONNECTOR:
;
				break;
			case COMPOSITE_CONNECTOR:
//				dprintk(DBGLVL_API,
;
				break;
			case SVIDEO_CONNECTOR:
;
				break;
			case COMPONENT_CONNECTOR:
//				dprintk(DBGLVL_API,
;
				break;
			case STANDARD_DMA:
;
				break;
			default:
//				dprintk(DBGLVL_API, "   = undefined (0x%x)\n",
;
			}
//			dprintk(DBGLVL_API, "  assocterminal= 0x%x\n",
;
//			dprintk(DBGLVL_API, "  sourceid     = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  iterminal    = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  BARLocation  = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  flags        = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  interruptid  = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  buffercount  = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  metadatasize = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  controlsize  = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  numformats   = 0x%x\n",
;

			t = (struct tmComResDescrHeader *)
				((struct tmComResDMATermDescrHeader *)(buf + idx));
			next_offset = idx + (vcoutputtermhdr->len);
			for (i = 0; i < vcoutputtermhdr->numformats; i++) {
				t = (struct tmComResDescrHeader *)
					(buf + next_offset);
				switch (t->subtype) {
				case VS_FORMAT_MPEG2TS:
					tsfmt =
					(struct tmComResTSFormatDescrHeader *)t;
					if (currpath == 1)
						tsport = &dev->ports[SAA7164_PORT_TS1];
					else
						tsport = &dev->ports[SAA7164_PORT_TS2];
					memcpy(&tsport->hwcfg, vcoutputtermhdr,
						sizeof(*vcoutputtermhdr));
					saa7164_api_configure_port_mpeg2ts(dev,
						tsport, tsfmt);
					break;
				case VS_FORMAT_MPEG2PS:
					psfmt =
					(struct tmComResPSFormatDescrHeader *)t;
					if (currpath == 1)
						encport = &dev->ports[SAA7164_PORT_ENC1];
					else
						encport = &dev->ports[SAA7164_PORT_ENC2];
					memcpy(&encport->hwcfg, vcoutputtermhdr,
						sizeof(*vcoutputtermhdr));
					saa7164_api_configure_port_mpeg2ps(dev,
						encport, psfmt);
					break;
				case VS_FORMAT_VBI:
					vbifmt =
					(struct tmComResVBIFormatDescrHeader *)t;
					if (currpath == 1)
						vbiport = &dev->ports[SAA7164_PORT_VBI1];
					else
						vbiport = &dev->ports[SAA7164_PORT_VBI2];
					memcpy(&vbiport->hwcfg, vcoutputtermhdr,
						sizeof(*vcoutputtermhdr));
					memcpy(&vbiport->vbi_fmt_ntsc, vbifmt,
						sizeof(*vbifmt));
					saa7164_api_configure_port_vbi(dev,
						vbiport);
					break;
				case VS_FORMAT_RDS:
//					dprintk(DBGLVL_API,
;
					break;
				case VS_FORMAT_UNCOMPRESSED:
//					dprintk(DBGLVL_API,
;
					break;
				case VS_FORMAT_TYPE:
//					dprintk(DBGLVL_API,
;
					break;
				default:
//					dprintk(DBGLVL_API,
//						"   = undefined (0x%x)\n",
;
				}
				next_offset += t->len;
			}

			break;
		case TUNER_UNIT:
;
			tunerunithdr =
				(struct tmComResTunerDescrHeader *)(buf + idx);
//			dprintk(DBGLVL_API, "  unitid = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  sourceid = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  iunit = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  tuningstandards = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  controlsize = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  controls = 0x%x\n",
;

			if (tunerunithdr->unitid == tunerunithdr->iunit) {
				if (currpath == 1)
					encport = &dev->ports[SAA7164_PORT_ENC1];
				else
					encport = &dev->ports[SAA7164_PORT_ENC2];
				memcpy(&encport->tunerunit, tunerunithdr,
					sizeof(struct tmComResTunerDescrHeader));
//				dprintk(DBGLVL_API,
//					"  (becomes dev->enc[%d] tuner)\n",
;
			}
			break;
		case VC_SELECTOR_UNIT:
			psel = (struct tmComResSelDescrHeader *)(buf + idx);
;
//			dprintk(DBGLVL_API, "  unitid = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  nrinpins = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  sourceid = 0x%x\n",
;
			break;
		case VC_PROCESSING_UNIT:
			pdh = (struct tmComResProcDescrHeader *)(buf + idx);
;
//			dprintk(DBGLVL_API, "  unitid = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  sourceid = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  controlsize = 0x%x\n",
;
			if (pdh->controlsize == 0x04) {
				if (currpath == 1)
					encport = &dev->ports[SAA7164_PORT_ENC1];
				else
					encport = &dev->ports[SAA7164_PORT_ENC2];
				memcpy(&encport->vidproc, pdh,
					sizeof(struct tmComResProcDescrHeader));
//				dprintk(DBGLVL_API, "  (becomes dev->enc[%d])\n",
;
			}
			break;
		case FEATURE_UNIT:
			afd = (struct tmComResAFeatureDescrHeader *)(buf + idx);
;
//			dprintk(DBGLVL_API, "  unitid = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  sourceid = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  controlsize = 0x%x\n",
;
			if (currpath == 1)
				encport = &dev->ports[SAA7164_PORT_ENC1];
			else
				encport = &dev->ports[SAA7164_PORT_ENC2];
			memcpy(&encport->audfeat, afd,
				sizeof(struct tmComResAFeatureDescrHeader));
//			dprintk(DBGLVL_API, "  (becomes dev->enc[%d])\n",
;
			break;
		case ENCODER_UNIT:
			edh = (struct tmComResEncoderDescrHeader *)(buf + idx);
;
;
;
//			dprintk(DBGLVL_API, "  vsourceid = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  asourceid = 0x%x\n",
;
;
			if (edh->iunit == edh->unitid) {
				if (currpath == 1)
					encport = &dev->ports[SAA7164_PORT_ENC1];
				else
					encport = &dev->ports[SAA7164_PORT_ENC2];
				memcpy(&encport->encunit, edh,
					sizeof(struct tmComResEncoderDescrHeader));
//				dprintk(DBGLVL_API,
//					"  (becomes dev->enc[%d])\n",
;
			}
			break;
		case EXTENSION_UNIT:
;
			exthdr = (struct tmComResExtDevDescrHeader *)(buf + idx);
//			dprintk(DBGLVL_API, "  unitid = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  deviceid = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  devicetype = 0x%x\n",
;
			if (exthdr->devicetype & 0x1)
;
			if (exthdr->devicetype & 0x2)
;
			if (exthdr->devicetype & 0x4)
;
			if (exthdr->devicetype & 0x8)
;
			if (exthdr->devicetype & 0x20)
;
			if (exthdr->devicetype & 0x40)
;
			if (exthdr->devicetype & 0x80)
;
			if (exthdr->devicetype & 0x100)
;
			if (exthdr->devicetype & 0x200)
;
			if (exthdr->devicetype & 0x400)
;
			if (exthdr->devicetype & 0x800)
;
			if (exthdr->devicetype & 0x1000)
;
			if (exthdr->devicetype & 0x2000)
//				dprintk(DBGLVL_API,
;
			if (exthdr->devicetype & 0x10000)
//				dprintk(DBGLVL_API,
;
			if (exthdr->devicetype & 0x20000)
//				dprintk(DBGLVL_API,
;
			if (exthdr->devicetype & 0x40000000)
//				dprintk(DBGLVL_API,
;
			if (exthdr->devicetype & 0x80000000)
//				dprintk(DBGLVL_API,
;
//			dprintk(DBGLVL_API, "  numgpiopins = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  numgpiogroups = 0x%x\n",
;
//			dprintk(DBGLVL_API, "  controlsize = 0x%x\n",
;
			if (exthdr->devicetype & 0x80) {
				if (currpath == 1)
					encport = &dev->ports[SAA7164_PORT_ENC1];
				else
					encport = &dev->ports[SAA7164_PORT_ENC2];
				memcpy(&encport->ifunit, exthdr,
					sizeof(struct tmComResExtDevDescrHeader));
//				dprintk(DBGLVL_API,
//					"  (becomes dev->enc[%d])\n",
;
			}
			break;
		case PVC_INFRARED_UNIT:
;
			break;
		case DRM_UNIT:
;
			break;
		default:
;
		}

;
;
;
;

		idx += hdr->len;
	}

	return 0;
}

int saa7164_api_enum_subdevs(struct saa7164_dev *dev)
{
	int ret;
	u32 buflen = 0;
	u8 *buf;

;

	/* Get the total descriptor length */
	ret = saa7164_cmd_send(dev, 0, GET_LEN,
		GET_DESCRIPTORS_CONTROL, sizeof(buflen), &buflen);
	if (ret != SAA_OK)
;

//	dprintk(DBGLVL_API, "%s() total descriptor size = %d bytes.\n",
;

	/* Allocate enough storage for all of the descs */
	buf = kzalloc(buflen, GFP_KERNEL);
	if (!buf)
		return SAA_ERR_NO_RESOURCES;

	/* Retrieve them */
	ret = saa7164_cmd_send(dev, 0, GET_CUR,
		GET_DESCRIPTORS_CONTROL, buflen, buf);
	if (ret != SAA_OK) {
;
		goto out;
	}

	if (saa_debug & DBGLVL_API)
		saa7164_dumphex16(dev, buf, (buflen/16)*16);

	saa7164_api_dump_subdevs(dev, buf, buflen);

out:
	kfree(buf);
	return ret;
}

int saa7164_api_i2c_read(struct saa7164_i2c *bus, u8 addr, u32 reglen, u8 *reg,
	u32 datalen, u8 *data)
{
	struct saa7164_dev *dev = bus->dev;
	u16 len = 0;
	int unitid;
	u32 regval;
	u8 buf[256];
	int ret;

;

	if (reglen > 4)
		return -EIO;

	if (reglen == 1)
		regval = *(reg);
	else
	if (reglen == 2)
		regval = ((*(reg) << 8) || *(reg+1));
	else
	if (reglen == 3)
		regval = ((*(reg) << 16) | (*(reg+1) << 8) | *(reg+2));
	else
	if (reglen == 4)
		regval = ((*(reg) << 24) | (*(reg+1) << 16) |
			(*(reg+2) << 8) | *(reg+3));

	/* Prepare the send buffer */
	/* Bytes 00-03 source register length
	 *       04-07 source bytes to read
	 *       08... register address
	 */
	memset(buf, 0, sizeof(buf));
	memcpy((buf + 2 * sizeof(u32) + 0), reg, reglen);
	*((u32 *)(buf + 0 * sizeof(u32))) = reglen;
	*((u32 *)(buf + 1 * sizeof(u32))) = datalen;

	unitid = saa7164_i2caddr_to_unitid(bus, addr);
	if (unitid < 0) {
//		printk(KERN_ERR
//			"%s() error, cannot translate regaddr 0x%x to unitid\n",
;
		return -EIO;
	}

	ret = saa7164_cmd_send(bus->dev, unitid, GET_LEN,
		EXU_REGISTER_ACCESS_CONTROL, sizeof(len), &len);
	if (ret != SAA_OK) {
;
		return -EIO;
	}

;

	if (saa_debug & DBGLVL_I2C)
		saa7164_dumphex16(dev, buf, 2 * 16);

	ret = saa7164_cmd_send(bus->dev, unitid, GET_CUR,
		EXU_REGISTER_ACCESS_CONTROL, len, &buf);
	if (ret != SAA_OK)
;
	else {
		if (saa_debug & DBGLVL_I2C)
			saa7164_dumphex16(dev, buf, sizeof(buf));
		memcpy(data, (buf + 2 * sizeof(u32) + reglen), datalen);
	}

	return ret == SAA_OK ? 0 : -EIO;
}

/* For a given 8 bit i2c address device, write the buffer */
int saa7164_api_i2c_write(struct saa7164_i2c *bus, u8 addr, u32 datalen,
	u8 *data)
{
	struct saa7164_dev *dev = bus->dev;
	u16 len = 0;
	int unitid;
	int reglen;
	u8 buf[256];
	int ret;

;

	if ((datalen == 0) || (datalen > 232))
		return -EIO;

	memset(buf, 0, sizeof(buf));

	unitid = saa7164_i2caddr_to_unitid(bus, addr);
	if (unitid < 0) {
//		printk(KERN_ERR
//			"%s() error, cannot translate regaddr 0x%x to unitid\n",
;
		return -EIO;
	}

	reglen = saa7164_i2caddr_to_reglen(bus, addr);
	if (reglen < 0) {
//		printk(KERN_ERR
//			"%s() error, cannot translate regaddr to reglen\n",
;
		return -EIO;
	}

	ret = saa7164_cmd_send(bus->dev, unitid, GET_LEN,
		EXU_REGISTER_ACCESS_CONTROL, sizeof(len), &len);
	if (ret != SAA_OK) {
;
		return -EIO;
	}

;

	/* Prepare the send buffer */
	/* Bytes 00-03 dest register length
	 *       04-07 dest bytes to write
	 *       08... register address
	 */
	*((u32 *)(buf + 0 * sizeof(u32))) = reglen;
	*((u32 *)(buf + 1 * sizeof(u32))) = datalen - reglen;
	memcpy((buf + 2 * sizeof(u32)), data, datalen);

	if (saa_debug & DBGLVL_I2C)
		saa7164_dumphex16(dev, buf, sizeof(buf));

	ret = saa7164_cmd_send(bus->dev, unitid, SET_CUR,
		EXU_REGISTER_ACCESS_CONTROL, len, &buf);
	if (ret != SAA_OK)
;

	return ret == SAA_OK ? 0 : -EIO;
}

int saa7164_api_modify_gpio(struct saa7164_dev *dev, u8 unitid,
	u8 pin, u8 state)
{
	int ret;
	struct tmComResGPIO t;

//	dprintk(DBGLVL_API, "%s(0x%x, %d, %d)\n",
;

	if ((pin > 7) || (state > 2))
		return SAA_ERR_BAD_PARAMETER;

	t.pin = pin;
	t.state = state;

	ret = saa7164_cmd_send(dev, unitid, SET_CUR,
		EXU_GPIO_CONTROL, sizeof(t), &t);
	if (ret != SAA_OK)
//		printk(KERN_ERR "%s() error, ret = 0x%x\n",
;

	return ret;
}

int saa7164_api_set_gpiobit(struct saa7164_dev *dev, u8 unitid,
	u8 pin)
{
	return saa7164_api_modify_gpio(dev, unitid, pin, 1);
}

int saa7164_api_clear_gpiobit(struct saa7164_dev *dev, u8 unitid,
	u8 pin)
{
	return saa7164_api_modify_gpio(dev, unitid, pin, 0);
}

