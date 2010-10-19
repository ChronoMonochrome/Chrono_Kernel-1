/*****************************************************************************/
/**
*  © ST-Ericsson, 2009 - All rights reserved
*  Reproduction and Communication of this document is strictly prohibited
*  unless specifically authorized in writing by ST-Ericsson
*
* \brief   Public header file for AB8500 Codec
* \author  ST-Ericsson
*/
/*****************************************************************************/

#ifndef _AB8500_CODEC_V1_0_H_
#define _AB8500_CODEC_V1_0_H_

/*---------------------------------------------------------------------
 * Includes
 *--------------------------------------------------------------------*/
#include "hcl_defs.h"
#include "debug.h"
#include <mach/ab8500_codec_p_v1_0.h>
/*---------------------------------------------------------------------
 * Define
 *--------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif
	typedef enum {
		AB8500_CODEC_OK,
		AB8500_CODEC_ERROR,
		AB8500_CODEC_UNSUPPORTED_FEATURE,
		AB8500_CODEC_INVALID_PARAMETER,
		AB8500_CODEC_CONFIG_NOT_COHERENT,
		AB8500_CODEC_TRANSACTION_FAILED
	} t_ab8500_codec_error;

	typedef enum {
		AB8500_CODEC_SRC_STATE_DISABLE,
		AB8500_CODEC_SRC_STATE_ENABLE
	} t_ab8500_codec_src_state;

	typedef enum {
		AB8500_CODEC_DEST_STATE_DISABLE,
		AB8500_CODEC_DEST_STATE_ENABLE
	} t_ab8500_codec_dest_state;

	typedef enum {
		AB8500_CODEC_MASTER_MODE_DISABLE,
		AB8500_CODEC_MASTER_MODE_ENABLE
	} t_ab8500_codec_master_mode;

	typedef enum {
		AB8500_CODEC_SLOT0,
		AB8500_CODEC_SLOT1,
		AB8500_CODEC_SLOT2,
		AB8500_CODEC_SLOT3,
		AB8500_CODEC_SLOT4,
		AB8500_CODEC_SLOT5,
		AB8500_CODEC_SLOT6,
		AB8500_CODEC_SLOT7,
		AB8500_CODEC_SLOT8,
		AB8500_CODEC_SLOT9,
		AB8500_CODEC_SLOT10,
		AB8500_CODEC_SLOT11,
		AB8500_CODEC_SLOT12,
		AB8500_CODEC_SLOT13,
		AB8500_CODEC_SLOT14,
		AB8500_CODEC_SLOT15,
		AB8500_CODEC_SLOT16,
		AB8500_CODEC_SLOT17,
		AB8500_CODEC_SLOT18,
		AB8500_CODEC_SLOT19,
		AB8500_CODEC_SLOT20,
		AB8500_CODEC_SLOT21,
		AB8500_CODEC_SLOT22,
		AB8500_CODEC_SLOT23,
		AB8500_CODEC_SLOT24,
		AB8500_CODEC_SLOT25,
		AB8500_CODEC_SLOT26,
		AB8500_CODEC_SLOT27,
		AB8500_CODEC_SLOT28,
		AB8500_CODEC_SLOT29,
		AB8500_CODEC_SLOT30,
		AB8500_CODEC_SLOT31,
		AB8500_CODEC_SLOT_UNDEFINED
	} t_ab8500_codec_slot;

	typedef enum {
		AB8500_CODEC_DA_CHANNEL_NUMBER_1,
		AB8500_CODEC_DA_CHANNEL_NUMBER_2,
		AB8500_CODEC_DA_CHANNEL_NUMBER_3,
		AB8500_CODEC_DA_CHANNEL_NUMBER_4,
		AB8500_CODEC_DA_CHANNEL_NUMBER_5,
		AB8500_CODEC_DA_CHANNEL_NUMBER_6,
		AB8500_CODEC_DA_CHANNEL_NUMBER_7,
		AB8500_CODEC_DA_CHANNEL_NUMBER_8,
		AB8500_CODEC_DA_CHANNEL_NUMBER_UNDEFINED
	} t_ab8500_codec_da_channel_number;

	typedef struct {
		t_ab8500_codec_cr105_bfifomsk cr105_bfifomsk;
		t_ab8500_codec_cr105_bfifoint cr105_bfifoint;
		t_ab8500_codec_cr106_bfifotx cr106_bfifotx;
		t_ab8500_codec_cr107_bfifoexsl cr107_bfifoexsl;
		t_ab8500_codec_cr107_prebitclk0 cr107_prebitclk0;
		t_ab8500_codec_cr107_bfifomast cr107_bfifomast;
		t_ab8500_codec_cr107_bfiforun cr107_bfiforun;
		t_ab8500_codec_cr108_bfifoframsw cr108_bfifoframsw;
		t_ab8500_codec_cr109_bfifowakeup cr109_bfifowakeup;
	} t_ab8500_codec_burst_fifo_config;

	typedef struct {
		t_ab8500_codec_cr27_if1_bitclk_osr cr27_if1_bitclk_osr;
		t_ab8500_codec_cr27_if0_bitclk_osr cr27_if0_bitclk_osr;
		t_ab8500_codec_cr28_if0wl cr28_if0wl;
		t_ab8500_codec_cr30_if1wl cr30_if1wl;
		t_ab8500_codec_cr28_bitclk0p cr28_bitclk0p;
		t_ab8500_codec_cr28_if0del cr28_if0del;
	} t_ab8500_codec_tdm_config;

/************************************************************/
/*---------------------------------------------------------------------
 * Exported APIs
 *--------------------------------------------------------------------*/
/* Initialization */
	t_ab8500_codec_error AB8500_CODEC_Init(IN t_uint8
					       slave_address_of_codec);
	t_ab8500_codec_error AB8500_CODEC_Reset(void);

/* Audio Codec basic configuration */
	t_ab8500_codec_error AB8500_CODEC_SetModeAndDirection(IN
							      t_ab8500_codec_direction
							      ab8500_codec_direction,
							      IN
							      t_ab8500_codec_mode
							      ab8500_codec_mode_in,
							      IN
							      t_ab8500_codec_mode
							      ab8500_codec_mode_out,
							      IN
							      t_ab8500_codec_tdm_config
							      const *const
							      p_tdm_config);
	t_ab8500_codec_error AB8500_CODEC_SelectInput(IN t_ab8500_codec_src
						      ab8500_codec_src);
	t_ab8500_codec_error AB8500_CODEC_SelectOutput(IN t_ab8500_codec_dest
						       ab8500_codec_dest);

/* Burst FIFO configuration */
	t_ab8500_codec_error AB8500_CODEC_ConfigureBurstFifo(IN
							     t_ab8500_codec_burst_fifo_config
							     const *const
							     p_burst_fifo_config);
	t_ab8500_codec_error AB8500_CODEC_EnableBurstFifo(void);
	t_ab8500_codec_error AB8500_CODEC_DisableBurstFifo(void);

/* Audio Codec Master mode configuration */
	t_ab8500_codec_error AB8500_CODEC_SetMasterMode(IN
							t_ab8500_codec_master_mode
							mode);

/* APIs to be implemented by user */
	t_ab8500_codec_error AB8500_CODEC_Write(IN t_uint8 register_offset,
						IN t_uint8 count,
						IN t_uint8 * p_data);
	t_ab8500_codec_error AB8500_CODEC_Read(IN t_uint8 register_offset,
					       IN t_uint8 count,
					       IN t_uint8 * p_dummy_data,
					       IN t_uint8 * p_data);

/* Volume Management */
	t_ab8500_codec_error AB8500_CODEC_SetSrcVolume(IN t_ab8500_codec_src
						       src_device,
						       IN t_uint8
						       in_left_volume,
						       IN t_uint8
						       in_right_volume);
	t_ab8500_codec_error AB8500_CODEC_SetDestVolume(IN t_ab8500_codec_dest
							dest_device,
							IN t_uint8
							out_left_volume,
							IN t_uint8
							out_right_volume);

/* Power management */
	t_ab8500_codec_error AB8500_CODEC_PowerDown(void);
	t_ab8500_codec_error AB8500_CODEC_PowerUp(void);

/* Interface Management */
	t_ab8500_codec_error AB8500_CODEC_SelectInterface(IN
							  t_ab8500_codec_audio_interface
							  audio_interface);
	t_ab8500_codec_error AB8500_CODEC_GetInterface(OUT
						       t_ab8500_codec_audio_interface
						       * p_audio_interface);

/* Slot Allocation */
	t_ab8500_codec_error AB8500_CODEC_ADSlotAllocation(IN
							   t_ab8500_codec_slot
							   ad_slot,
							   IN
							   t_ab8500_codec_cr31_to_cr46_ad_data_allocation
							   value);
	t_ab8500_codec_error AB8500_CODEC_DASlotAllocation(IN
							   t_ab8500_codec_da_channel_number
							   channel_number,
							   IN
							   t_ab8500_codec_cr51_to_cr58_sltoda
							   slot);

/* Loopback Management */
	t_ab8500_codec_error AB8500_CODEC_SetAnalogLoopback(IN t_uint8
							    out_left_volume,
							    IN t_uint8
							    out_right_volume);
	t_ab8500_codec_error AB8500_CODEC_RemoveAnalogLoopback(void);

/* Bypass Management */
	t_ab8500_codec_error AB8500_CODEC_EnableBypassMode(void);
	t_ab8500_codec_error AB8500_CODEC_DisableBypassMode(void);

/* Power Control Management */
	t_ab8500_codec_error AB8500_CODEC_SrcPowerControl(IN t_ab8500_codec_src
							  src_device,
							  t_ab8500_codec_src_state
							  state);
	t_ab8500_codec_error AB8500_CODEC_DestPowerControl(IN
							   t_ab8500_codec_dest
							   dest_device,
							   t_ab8500_codec_dest_state
							   state);

/* Version Management */
	t_ab8500_codec_error AB8500_CODEC_GetVersion(OUT t_version * p_version);

#if 0
/* Debug management */
	t_ab8500_codec_error AB8500_CODEC_SetDbgLevel(IN t_dbg_level dbg_level);
	t_ab8500_codec_error AB8500_CODEC_GetDbgLevel(OUT t_dbg_level *
						      p_dbg_level);
#endif

/*
** following is added by $kardad$
*/

/* duplicate copy of enum from msp.h */
/* for MSPConfiguration.in_clock_freq parameter to select msp clock freq */
	typedef enum {
		CODEC_MSP_INPUT_FREQ_1MHZ = 1024,
		CODEC_MSP_INPUT_FREQ_2MHZ = 2048,
		CODEC_MSP_INPUT_FREQ_3MHZ = 3072,
		CODEC_MSP_INPUT_FREQ_4MHZ = 4096,
		CODEC_MSP_INPUT_FREQ_5MHZ = 5760,
		CODEC_MSP_INPUT_FREQ_6MHZ = 6144,
		CODEC_MSP_INPUT_FREQ_8MHZ = 8192,
		CODEC_MSP_INPUT_FREQ_11MHZ = 11264,
		CODEC_MSP_INPUT_FREQ_12MHZ = 12288,
		CODEC_MSP_INPUT_FREQ_16MHZ = 16384,
		CODEC_MSP_INPUT_FREQ_22MHZ = 22579,
		CODEC_MSP_INPUT_FREQ_24MHZ = 24576,
		CODEC_MSP_INPUT_FREQ_48MHZ = 49152
	} codec_msp_in_clock_freq_type;

/* msp clock source internal/external for srg_clock_sel */
	typedef enum {
		CODEC_MSP_APB_CLOCK = 0,
		CODEC_MSP_SCK_CLOCK = 2,
		CODEC_MSP_SCK_SYNC_CLOCK = 3
	} codec_msp_srg_clock_sel_type;

/* Sample rate supported by Codec */

	typedef enum {
		CODEC_FREQUENCY_DONT_CHANGE = -100,
		CODEC_SAMPLING_FREQ_RESET = -1,
		CODEC_SAMPLING_FREQ_MINLIMIT = 7,
		CODEC_SAMPLING_FREQ_8KHZ = 8,	/*default */
		CODEC_SAMPLING_FREQ_11KHZ = 11,
		CODEC_SAMPLING_FREQ_12KHZ = 12,
		CODEC_SAMPLING_FREQ_16KHZ = 16,
		CODEC_SAMPLING_FREQ_22KHZ = 22,
		CODEC_SAMPLING_FREQ_24KHZ = 24,
		CODEC_SAMPLING_FREQ_32KHZ = 32,
		CODEC_SAMPLING_FREQ_44KHZ = 44,
		CODEC_SAMPLING_FREQ_48KHZ = 48,
		CODEC_SAMPLING_FREQ_64KHZ = 64,	/*the frequencies below this line are not supported in stw5094A */
		CODEC_SAMPLING_FREQ_88KHZ = 88,
		CODEC_SAMPLING_FREQ_96KHZ = 96,
		CODEC_SAMPLING_FREQ_128KHZ = 128,
		CODEC_SAMPLING_FREQ_176KHZ = 176,
		CODEC_SAMPLING_FREQ_192KHZ = 192,
		CODEC_SAMPLING_FREQ_MAXLIMIT = 193
	} t_codec_sample_frequency;

#define RESET       -1
#define DEFAULT     -100
/***********************************************************/
/*
** following stuff is added to compile code without debug print support $kardad$
*/

#define DBGEXIT(cr)
#define DBGEXIT0(cr)
#define DBGEXIT1(cr,ch,p1)
#define DBGEXIT2(cr,ch,p1,p2)
#define DBGEXIT3(cr,ch,p1,p2,p3)
#define DBGEXIT4(cr,ch,p1,p2,p3,p4)
#define DBGEXIT5(cr,ch,p1,p2,p3,p4,p5)
#define DBGEXIT6(cr,ch,p1,p2,p3,p4,p5,p6)

#define DBGENTER()
#define DBGENTER0()
#define DBGENTER1(ch,p1)
#define DBGENTER2(ch,p1,p2)
#define DBGENTER3(ch,p1,p2,p3)
#define DBGENTER4(ch,p1,p2,p3,p4)
#define DBGENTER5(ch,p1,p2,p3,p4,p5)
#define DBGENTER6(ch,p1,p2,p3,p4,p5,p6)

#define DBGPRINT(dbg_level,dbg_string)
#define DBGPRINTHEX(dbg_level,dbg_string,uint32)
#define DBGPRINTDEC(dbg_level,dbg_string,uint32)
/***********************************************************/

#ifdef __cplusplus
}				/* allow C++ to use these headers */
#endif				/* __cplusplus */
#endif				/* _AB8500_CODEC_H_ */
/* End of file ab8500_codec.h*/
