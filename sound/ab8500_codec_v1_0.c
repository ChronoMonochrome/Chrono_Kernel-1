/*****************************************************************************/

/**
*  © ST-Ericsson, 2009 - All rights reserved
*  Reproduction and Communication of this document is strictly prohibited
*  unless specifically authorized in writing by ST-Ericsson
*
* \brief   This module provides some support routines for the AB8500 CODEC
* \author  ST-Ericsson
*/
/*****************************************************************************/

/*----------------------------------------------------------------------------
 * Includes
 *---------------------------------------------------------------------------*/

#ifdef CONFIG_U8500_AB8500_CUT10
#include <mach/ab8500_codec_v1_0.h>
#include <mach/ab8500_codec_p_v1_0.h>
#else /*  */
#include <mach/ab8500_codec.h>
#include <mach/ab8500_codec_p.h>
#endif /*  */

/*--------------------------------------------------------------------------*
 * debug stuff *
 *--------------------------------------------------------------------------*/
#ifdef __DEBUG
#define MY_DEBUG_LEVEL_VAR_NAME myDebugLevel_AB8500_CODEC
#define MY_DEBUG_ID             myDebugID_AB8500_CODEC
PRIVATE t_dbg_level MY_DEBUG_LEVEL_VAR_NAME = DEBUG_LEVEL0;
PRIVATE t_dbg_id MY_DEBUG_ID = AB8500_CODEC_HCL_DBG_ID;

#endif /*  */

/*--------------------------------------------------------------------------*
 * Global data for interrupt mode management *
 *--------------------------------------------------------------------------*/
PRIVATE t_ab8500_codec_system_context g_ab8500_codec_system_context;

/*--------------------------------------------------------------------------*
 * Default Values *
 *--------------------------------------------------------------------------*/
#define AB8500_CODEC_DEFAULT_SLAVE_ADDRESS_OF_CODEC 0xD
#define AB8500_CODEC_DEFAULT_DIRECTION              AB8500_CODEC_DIRECTION_OUT

#define AB8500_CODEC_DEFAULT_MODE_IN                AB8500_CODEC_MODE_VOICE
#define AB8500_CODEC_DEFAULT_MODE_OUT               AB8500_CODEC_MODE_VOICE

#define AB8500_CODEC_DEFAULT_INPUT_SRC              AB8500_CODEC_SRC_MICROPHONE_1A
#define AB8500_CODEC_DEFAULT_OUTPUT_DEST            AB8500_CODEC_DEST_HEADSET

#define AB8500_CODEC_DEFAULT_VOLUME_LEFT_IN         75
#define AB8500_CODEC_DEFAULT_VOLUME_RIGHT_IN        75
#define AB8500_CODEC_DEFAULT_VOLUME_LEFT_OUT        75
#define AB8500_CODEC_DEFAULT_VOLUME_RIGHT_OUT       75

/*---------------------------------------------------------------------
 * PRIVATE APIs
 *--------------------------------------------------------------------*/
PRIVATE t_ab8500_codec_error ab8500_codec_ADSlotAllocationSwitch1(IN
								  t_ab8500_codec_slot
								  ad_slot,
								  IN
								  t_ab8500_codec_cr31_to_cr46_ad_data_allocation
								  value);
PRIVATE t_ab8500_codec_error ab8500_codec_ADSlotAllocationSwitch2(IN
								   t_ab8500_codec_slot
								   ad_slot,
								   IN
								   t_ab8500_codec_cr31_to_cr46_ad_data_allocation
								   value);
PRIVATE t_ab8500_codec_error ab8500_codec_ADSlotAllocationSwitch3(IN
								   t_ab8500_codec_slot
								   ad_slot,
								   IN
								   t_ab8500_codec_cr31_to_cr46_ad_data_allocation
								   value);
PRIVATE t_ab8500_codec_error ab8500_codec_ADSlotAllocationSwitch4(IN
								   t_ab8500_codec_slot
								   ad_slot,
								   IN
								   t_ab8500_codec_cr31_to_cr46_ad_data_allocation
								   value);
PRIVATE t_ab8500_codec_error ab8500_codec_SrcPowerControlSwitch1(IN
								  t_ab8500_codec_src
								  src_device,
								  t_ab8500_codec_src_state
								  state);
PRIVATE t_ab8500_codec_error ab8500_codec_SrcPowerControlSwitch2(IN
								  t_ab8500_codec_src
								  src_device,
								  t_ab8500_codec_src_state
								  state);
PRIVATE t_ab8500_codec_error ab8500_codec_SetModeAndDirectionUpdateCR(void);
PRIVATE t_ab8500_codec_error ab8500_codec_SetSrcVolumeUpdateCR(void);
PRIVATE t_ab8500_codec_error ab8500_codec_SetDestVolumeUpdateCR(void);
PRIVATE t_ab8500_codec_error ab8500_codec_ProgramDirectionIN(void);
PRIVATE t_ab8500_codec_error ab8500_codec_ProgramDirectionOUT(void);
PRIVATE t_ab8500_codec_error ab8500_codec_DestPowerControlUpdateCR(void);

/********************************************************************************************/
/* Name: ab8500_codec_SingleWrite                                                       */
/********************************************************************************************/
    PRIVATE t_ab8500_codec_error ab8500_codec_SingleWrite(t_uint8
							  register_offset,
							  t_uint8 data)
{
	return (t_ab8500_codec_error) (AB8500_CODEC_Write
					(register_offset, 0x01, &data));
}

#if 0

/********************************************************************************************/
/* Name: ab8500_codec_SingleRead                                                        */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_SingleRead(t_uint8
						     register_offset,
						     t_uint8 data)
{
	t_uint8 dummy_data = 0xAA;
	return (t_ab8500_codec_error) (AB8500_CODEC_Read
					 (register_offset, 0x01, &dummy_data,
					  &data));
}

#endif /*  */

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR0                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR0(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				      (t_uint8) p_ab8500_codec_configuration->
				      cr0_powerup, AB8500_CODEC_MASK_ONE_BIT,
				      AB8500_CODEC_CR0_POWERUP );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr0_enaana, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR0_ENAANA );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR0, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR1                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR1(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr1_swreset, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR1_SWRESET );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR1, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR2                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR2(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr2_enad1, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR2_ENAD1 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr2_enad2, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR2_ENAD2 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr2_enad3, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR2_ENAD3 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr2_enad4, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR2_ENAD4 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr2_enad5, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR2_ENAD5 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr2_enad6, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR2_ENAD6 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR2, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR3                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR3(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr3_enda1, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR3_ENDA1 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr3_enda2, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR3_ENDA2 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr3_enda3, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR3_ENDA3 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr3_enda4, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR3_ENDA4 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr3_enda5, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR3_ENDA5 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr3_enda6, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR3_ENDA6 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR3, value));
}

#if 0

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR4                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR4(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr4_lowpowhs, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR4_LOWPOWHS );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr4_lowpowdachs,
				    AB8500_CODEC_MASK_TWO_BITS,
				    AB8500_CODEC_CR4_LOWPOWDACHS );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr4_lowpowear, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR4_LOWPOWEAR );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr4_ear_sel_cm, AB8500_CODEC_MASK_TWO_BITS,
				    AB8500_CODEC_CR4_EAR_SEL_CM );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr4_hs_hp_en, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR4_HS_HP_EN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR4, value));
}

#endif /*  */

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR5                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR5(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr5_enmic1, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR5_ENMIC1 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr5_enmic2, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR5_ENMIC2 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr5_enlinl, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR5_ENLINL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr5_enlinr, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR5_ENLINR );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr5_mutmic1, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR5_MUTMIC1 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr5_mutmic2, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR5_MUTMIC2 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr5_mutlinl, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR5_MUTELINL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr5_mutlinr, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR5_MUTELINR );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR5, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR6                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR6(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr6_endmic1, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR6_ENDMIC1 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr6_endmic2, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR6_ENDMIC2 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr6_endmic3, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR6_ENDMIC3 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr6_endmic4, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR6_ENDMIC4 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr6_endmic5, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR6_ENDMIC5 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr6_endmic6, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR6_ENDMIC6 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR6, value));
}


/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR7                                                          */
/********************************************************************************************/
    PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR7(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr7_mic1sel, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR7_MIC1SEL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr7_linrsel, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR7_LINRSEL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr7_endrvhsl, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR7_ENDRVHSL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr7_endrvhsr, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR7_ENDRVHSR );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr7_enadcmic, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR7_ENADCMIC );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr7_enadclinl, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR7_ENADCLINL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr7_enadclinr, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR7_ENADCLINR );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR7, value));
}


/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR8                                                          */
/********************************************************************************************/
    PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR8(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr8_cp_dis_pldwn,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR8_CP_DIS_PLDWN );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr8_enear, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR8_ENEAR );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr8_enhsl, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR8_ENHSL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr8_enhsr, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR8_ENHSR );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr8_enhfl, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR8_ENHFL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr8_enhfr, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR8_ENHFR );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr8_envibl, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR8_ENVIBL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr8_envibr, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR8_ENVIBR );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR8, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR9                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR9(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr9_endacear, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR9_ENADACEAR );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr9_endachsl, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR9_ENADACHSL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr9_endachsr, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR9_ENADACHSR );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr9_endachfl, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR9_ENADACHFL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr9_endachfr, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR9_ENADACHFR );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr9_endacvibl, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR9_ENADACVIBL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr9_endacvibr, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR9_ENADACVIBR );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR9, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR10                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR10(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr10_muteear, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR10_MUTEEAR );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr10_mutehsl, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR10_MUTEHSL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr10_mutehsr, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR10_MUTEHSR );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR10, value));
}

#if 0

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR11                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR11(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr11_earshortpwd,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR11_ENSHORTPWD );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr11_earshortdis,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR11_EARSHORTDIS );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr11_hsshortdis, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR11_HSSHORTDIS );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr11_hspullden, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR11_HSPULLDEN );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr11_hsoscen, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR11_HSOSCEN );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr11_hsfaden, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR11_HSFADEN );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr11_hszcddis, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR11_HSZCDDIS );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR11, value));
}

#endif /*  */

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR12                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR12(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr12_encphs, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR12_ENCPHS );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr12_hsautoen, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR12_HSAUTOEN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR12, value));
}

#if 0

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR13                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR13(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr13_envdet_hthresh,
				     AB8500_CODEC_MASK_FOUR_BITS,
				     AB8500_CODEC_CR13_ENVDET_HTHRESH );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr13_envdet_lthresh,
				    AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR13_ENVDET_LTHRESH );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR13, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR14                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR14(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr14_smpslven, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR14_SMPSLVEN );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr14_envdetsmpsen,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR14_ENVDETSMPSEN );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr14_cplven, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR14_CPLVEN );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr14_envdetcpen, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR14_ENVDETCPEN );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr14_envet_time,
				    AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR14_ENVDET_TIME );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR14, value));
}

#endif /*  */

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR15                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR15(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr15_pwmtovibl, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR15_PWMTOVIBL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr15_pwmtovibr, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR15_PWMTOVIBR );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr15_pwmlctrl, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR15_PWMLCTRL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr15_pwmrctrl, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR15_PWMRCTRL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr15_pwmnlctrl, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR15_PWMNLCTRL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr15_pwmplctrl, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR15_PWMPLCTRL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr15_pwmnrctrl, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR15_PWMNRCTRL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr15_pwmprctrl, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR15_PWMPRCTRL );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR15, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR16                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR16(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr16_pwmnlpol, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR16_PWMNLPOL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr16_pwmnldutycycle,
				    AB8500_CODEC_MASK_SEVEN_BITS,
				    AB8500_CODEC_CR16_PWMNLDUTYCYCLE );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR16, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR17                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR17(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr17_pwmplpol, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR17_PWMPLPOL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr17_pwmpldutycycle,
				    AB8500_CODEC_MASK_SEVEN_BITS,
				    AB8500_CODEC_CR17_PWMLPDUTYCYCLE );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR17, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR18                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR18(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr18_pwmnrpol, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR18_PWMNRPOL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr18_pwmnrdutycycle,
				    AB8500_CODEC_MASK_SEVEN_BITS,
				    AB8500_CODEC_CR18_PWMNRDUTYCYCLE );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR18, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR19                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR19(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr19_pwmprpol, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR19_PWMPRPOL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr19_pwmprdutycycle,
				    AB8500_CODEC_MASK_SEVEN_BITS,
				    AB8500_CODEC_CR19_PWMRPDUTYCYCLE );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR19, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR20                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR20(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr20_en_se_mic1,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR20_EN_SE_MIC1 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr20_low_pow_mic1,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR20_LOW_POW_MIC1 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr20_mic1_gain,
				    AB8500_CODEC_MASK_FIVE_BITS,
				    AB8500_CODEC_CR20_MIC1_GAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR20, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR21                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR21(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr21_en_se_mic2,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR21_EN_SE_MIC2 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr21_low_pow_mic2,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR21_LOW_POW_MIC2 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr21_mic2_gain,
				    AB8500_CODEC_MASK_FIVE_BITS,
				    AB8500_CODEC_CR21_MIC2_GAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR21, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR22                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR22(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr22_hsl_gain,
				     AB8500_CODEC_MASK_FOUR_BITS,
				     AB8500_CODEC_CR22_HSL_GAIN );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr22_hsr_gain, AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR22_HSR_GAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR22, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR23                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR23(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr23_linl_gain,
				     AB8500_CODEC_MASK_FOUR_BITS,
				     AB8500_CODEC_CR23_LINL_GAIN );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr23_linr_gain,
				    AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR23_LINR_GAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR23, value));
}


/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR24                                                          */
/********************************************************************************************/
    PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR24(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr24_lintohsl_gain,
				     AB8500_CODEC_MASK_FIVE_BITS,
				     AB8500_CODEC_CR24_LINTOHSL_GAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR24, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR25                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR25(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr25_lintohsr_gain,
				     AB8500_CODEC_MASK_FIVE_BITS,
				     AB8500_CODEC_CR25_LINTOHSR_GAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR25, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR26                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR26(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr26_ad1nh, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR26_AD1NH );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr26_ad2nh, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR26_AD2NH );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr26_ad3nh, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR26_AD3NH );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr26_ad4nh, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR26_AD4NH );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr26_ad1_voice, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR26_AD1_VOICE );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr26_ad2_voice, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR26_AD2_VOICE );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr26_ad3_voice, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR26_AD3_VOICE );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr26_ad4_voice, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR26_AD4_VOICE );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR26, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR27                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR27(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr27_en_mastgen,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR27_EN_MASTGEN );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr27_if1_bitclk_osr,
				    AB8500_CODEC_MASK_TWO_BITS,
				    AB8500_CODEC_CR27_IF1_BITCLK_OSR );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr27_enfs_bitclk1,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR27_ENFS_BITCLK1 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr27_if0_bitclk_osr,
				    AB8500_CODEC_MASK_TWO_BITS,
				    AB8500_CODEC_CR27_IF0_BITCLK_OSR );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr27_enfs_bitclk0,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR27_ENFS_BITCLK0 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR27, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR28                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR28(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr28_fsync0p, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR28_FSYNC0P );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr28_bitclk0p, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR28_BITCLK0P );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr28_if0del, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR28_IF0DEL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr28_if0format, AB8500_CODEC_MASK_TWO_BITS,
				    AB8500_CODEC_CR28_IF0FORMAT );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr28_if0wl, AB8500_CODEC_MASK_TWO_BITS,
				    AB8500_CODEC_CR28_IF0WL );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR28, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR29                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR29(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr29_if0datoif1ad,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR29_IF0DATOIF1AD );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr29_if0cktoif1ck,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR29_IF0CKTOIF1CK );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr29_if1master, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR29_IF1MASTER );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr29_if1datoif0ad,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR29_IF1DATOIF0AD );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr29_if1cktoif0ck,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR29_IF1CKTOIF0CK );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr29_if0master, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR29_IF0MASTER );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr29_if0bfifoen, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR29_IF0BFIFOEN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR29, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR30                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR30(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr30_fsync1p, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR30_FSYNC1P );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr30_bitclk1p, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR30_BITCLK1P );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr30_if1del, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR30_IF1DEL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr30_if1format, AB8500_CODEC_MASK_TWO_BITS,
				    AB8500_CODEC_CR30_IF1FORMAT );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr30_if1wl, AB8500_CODEC_MASK_TWO_BITS,
				    AB8500_CODEC_CR30_IF1WL );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR30, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR31                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR31(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr31_adotoslot1,
				     AB8500_CODEC_MASK_FOUR_BITS,
				     AB8500_CODEC_CR31_ADOTOSLOT1 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr31_adotoslot0,
				    AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR31_ADOTOSLOT0 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR31, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR32                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR32(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr32_adotoslot3,
				     AB8500_CODEC_MASK_FOUR_BITS,
				     AB8500_CODEC_CR32_ADOTOSLOT3 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr32_adotoslot2,
				    AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR32_ADOTOSLOT2 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR32, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR33                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR33(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr33_adotoslot5,
				     AB8500_CODEC_MASK_FOUR_BITS,
				     AB8500_CODEC_CR33_ADOTOSLOT5 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr33_adotoslot4,
				    AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR33_ADOTOSLOT4 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR33, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR34                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR34(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr34_adotoslot7,
				     AB8500_CODEC_MASK_FOUR_BITS,
				     AB8500_CODEC_CR34_ADOTOSLOT7 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr34_adotoslot6,
				    AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR34_ADOTOSLOT6 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR34, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR35                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR35(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr35_adotoslot9,
				     AB8500_CODEC_MASK_FOUR_BITS,
				     AB8500_CODEC_CR35_ADOTOSLOT9 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr35_adotoslot8,
				    AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR35_ADOTOSLOT8 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR35, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR36                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR36(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr36_adotoslot11,
				     AB8500_CODEC_MASK_FOUR_BITS,
				     AB8500_CODEC_CR36_ADOTOSLOT11 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr36_adotoslot10,
				    AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR36_ADOTOSLOT10 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR36, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR37                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR37(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr37_adotoslot13,
				     AB8500_CODEC_MASK_FOUR_BITS,
				     AB8500_CODEC_CR37_ADOTOSLOT13 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr37_adotoslot12,
				    AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR37_ADOTOSLOT12 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR37, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR38                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR38(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr38_adotoslot15,
				     AB8500_CODEC_MASK_FOUR_BITS,
				     AB8500_CODEC_CR38_ADOTOSLOT15 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr38_adotoslot14,
				    AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR38_ADOTOSLOT14 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR38, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR39                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR39(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr39_adotoslot17,
				     AB8500_CODEC_MASK_FOUR_BITS,
				     AB8500_CODEC_CR39_ADOTOSLOT17 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr39_adotoslot16,
				    AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR39_ADOTOSLOT16 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR39, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR40                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR40(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr40_adotoslot19,
				     AB8500_CODEC_MASK_FOUR_BITS,
				     AB8500_CODEC_CR40_ADOTOSLOT19 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr40_adotoslot18,
				    AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR40_ADOTOSLOT18 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR40, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR41                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR41(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr41_adotoslot21,
				     AB8500_CODEC_MASK_FOUR_BITS,
				     AB8500_CODEC_CR41_ADOTOSLOT21 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr41_adotoslot20,
				    AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR41_ADOTOSLOT20 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR41, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR42                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR42(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr42_adotoslot23,
				     AB8500_CODEC_MASK_FOUR_BITS,
				     AB8500_CODEC_CR42_ADOTOSLOT23 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr42_adotoslot22,
				    AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR42_ADOTOSLOT22 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR42, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR43                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR43(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr43_adotoslot25,
				     AB8500_CODEC_MASK_FOUR_BITS,
				     AB8500_CODEC_CR43_ADOTOSLOT25 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr43_adotoslot24,
				    AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR43_ADOTOSLOT24 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR43, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR44                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR44(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr44_adotoslot27,
				     AB8500_CODEC_MASK_FOUR_BITS,
				     AB8500_CODEC_CR44_ADOTOSLOT27 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr44_adotoslot26,
				    AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR44_ADOTOSLOT26 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR44, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR45                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR45(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr45_adotoslot29,
				     AB8500_CODEC_MASK_FOUR_BITS,
				     AB8500_CODEC_CR45_ADOTOSLOT29 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr45_adotoslot28,
				    AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR45_ADOTOSLOT28 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR45, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR46                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR46(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr46_adotoslot31,
				     AB8500_CODEC_MASK_FOUR_BITS,
				     AB8500_CODEC_CR46_ADOTOSLOT31 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr46_adotoslot30,
				    AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR46_ADOTOSLOT30 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR46, value));
}

#if 0

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR47                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR47(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr47_hiz_sl7, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR47_HIZ_SL7 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr47_hiz_sl6, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR47_HIZ_SL6 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr47_hiz_sl5, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR47_HIZ_SL5 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr47_hiz_sl4, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR47_HIZ_SL4 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr47_hiz_sl3, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR47_HIZ_SL3 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr47_hiz_sl2, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR47_HIZ_SL2 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr47_hiz_sl1, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR47_HIZ_SL1 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr47_hiz_sl0, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR47_HIZ_SL0 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR47, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR48                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR48(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr48_hiz_sl15, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR48_HIZ_SL15 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr48_hiz_sl14, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR48_HIZ_SL14 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr48_hiz_sl13, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR48_HIZ_SL13 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr48_hiz_sl12, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR48_HIZ_SL12 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr48_hiz_sl11, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR48_HIZ_SL11 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr48_hiz_sl10, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR48_HIZ_SL10 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr48_hiz_sl9, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR48_HIZ_SL9 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr48_hiz_sl8, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR48_HIZ_SL8 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR48, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR49                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR49(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr49_hiz_sl23, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR49_HIZ_SL23 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr49_hiz_sl22, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR49_HIZ_SL22 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr49_hiz_sl21, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR49_HIZ_SL21 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr49_hiz_sl20, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR49_HIZ_SL20 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr49_hiz_sl19, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR49_HIZ_SL19 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr49_hiz_sl18, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR49_HIZ_SL18 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr49_hiz_sl17, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR49_HIZ_SL17 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr49_hiz_sl16, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR49_HIZ_SL16 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR49, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR50                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR50(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr50_hiz_sl31, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR50_HIZ_SL31 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr50_hiz_sl30, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR50_HIZ_SL30 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr50_hiz_sl29, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR50_HIZ_SL29 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr50_hiz_sl28, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR50_HIZ_SL28 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr50_hiz_sl27, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR50_HIZ_SL27 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr50_hiz_sl26, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR50_HIZ_SL26 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr50_hiz_sl25, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR50_HIZ_SL25 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr50_hiz_sl24, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR50_HIZ_SL24 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR50, value));
}

#endif /*  */

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR51                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR51(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr51_da12_voice,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR51_DA12_VOICE );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr51_swapda12_34,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR51_SWAP_DA12_34 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr51_sldai7toslado1,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR51_SLDAI7TOSLADO1 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr51_sltoda1, AB8500_CODEC_MASK_FIVE_BITS,
				    AB8500_CODEC_CR51_SLTODA1 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR51, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR52                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR52(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr52_sldai8toslado2,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR52_SLDAI8TOSLADO2 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr52_sltoda2, AB8500_CODEC_MASK_FIVE_BITS,
				    AB8500_CODEC_CR52_SLTODA2 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR52, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR53                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR53(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr53_da34_voice,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR53_DA34_VOICE );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr53_sldai7toslado3,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR53_SLDAI7TOSLADO3 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr53_sltoda3, AB8500_CODEC_MASK_FIVE_BITS,
				    AB8500_CODEC_CR53_SLTODA3 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR53, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR54                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR54(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr54_sldai8toslado4,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR54_SLDAI8TOSLADO4 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr54_sltoda4, AB8500_CODEC_MASK_FIVE_BITS,
				    AB8500_CODEC_CR54_SLTODA4 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR54, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR55                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR55(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr55_da56_voice,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR55_DA56_VOICE );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr55_sldai7toslado5,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR55_SLDAI7TOSLADO5 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr55_sltoda5, AB8500_CODEC_MASK_FIVE_BITS,
				    AB8500_CODEC_CR55_SLTODA5 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR55, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR56                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR56(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr56_sldai8toslado6,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR56_SLDAI8TOSLADO6 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr56_sltoda6, AB8500_CODEC_MASK_FIVE_BITS,
				    AB8500_CODEC_CR56_SLTODA6 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR56, value));
}


/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR57                                                          */
/********************************************************************************************/
    PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR57(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr57_sldai8toslado7,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR57_SLDAI8TOSLADO7 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr57_sltoda7, AB8500_CODEC_MASK_FIVE_BITS,
				    AB8500_CODEC_CR57_SLTODA7 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR57, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR58                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR58(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr58_sldai7toslado8,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR58_SLDAI7TOSLADO8 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr58_sltoda8, AB8500_CODEC_MASK_FIVE_BITS,
				    AB8500_CODEC_CR58_SLTODA8 );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR58, value));
}

#if 0

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR59                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR59(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr59_parlhf, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR59_PARLHF );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr59_parlvib, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR59_PARLVIB );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr59_classdvib1_swapen,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR59_CLASSDVIB1SWAPEN );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr59_classdvib2_swapen,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR59_CLASSDVIB2SWAPEN );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr59_classdhfl_swapen,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR59_CLASSDHFLSWAPEN );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr59_classdhfr_swapen,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR59_CLASSDHFRSWAPEN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR59, value));
}


/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR60                                                          */
/********************************************************************************************/
    PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR60(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr60_classd_firbyp,
				     AB8500_CODEC_MASK_FOUR_BITS,
				     AB8500_CODEC_CR60_CLASSD_FIR_BYP );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr60_classd_highvolen,
				    AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR60_CLASSD_HIGHVOL_EN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR60, value));
}


/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR61                                                          */
/********************************************************************************************/
    PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR61(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;

	/* 5 bits are Read Only */
	AB8500_CODEC_WRITE_BITS
	    (value,
	     (t_uint8) p_ab8500_codec_configuration->cr61_classddith_hpgain,
	     AB8500_CODEC_MASK_FOUR_BITS,
	     AB8500_CODEC_CR61_CLASSD_DITH_HPGAIN );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr61_classddith_wgain,
				    AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR61_CLASSD_DITH_WGAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR61, value));
}

#endif /*  */

/* CR62 is Read Only */
/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR63                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR63(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr63_datohslen, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR63_DATOHSLEN );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr63_datohsren, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR63_DATOHSREN );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr63_ad1sel, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR63_AD1SEL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr63_ad2sel, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR63_AD2SEL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr63_ad3sel, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR63_AD3SEL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr63_ad5sel, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR63_AD5SEL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr63_ad6sel, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR63_AD6SEL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr63_ancsel, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR63_ANCSEL );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR63, value));
}

#if 0
/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR64                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR64(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr64_datohfren, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR64_DATOHFREN );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr64_datohflen, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR64_DATOHFLEN );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr64_hfrsel, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR64_HFRSEL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr64_hflsel, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR64_HFLSEL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr64_stfir1sel, AB8500_CODEC_MASK_TWO_BITS,
				    AB8500_CODEC_CR64_STFIR1SEL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr64_stfir2sel, AB8500_CODEC_MASK_TWO_BITS,
				    AB8500_CODEC_CR64_STFIR2SEL );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR64, value));
}

#endif /*  */

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR65                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR65(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr65_fadedis_ad1,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR65_FADEDIS_AD1 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr65_ad1gain, AB8500_CODEC_MASK_SIX_BITS,
				    AB8500_CODEC_CR65_AD1GAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR65, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR66                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR66(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr66_fadedis_ad2,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR66_FADEDIS_AD2 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr66_ad2gain, AB8500_CODEC_MASK_SIX_BITS,
				    AB8500_CODEC_CR66_AD2GAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR66, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR67                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR67(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr67_fadedis_ad3,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR67_FADEDIS_AD3 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr67_ad3gain, AB8500_CODEC_MASK_SIX_BITS,
				    AB8500_CODEC_CR67_AD3GAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR67, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR68                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR68(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr68_fadedis_ad4,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR68_FADEDIS_AD4 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr68_ad4gain, AB8500_CODEC_MASK_SIX_BITS,
				    AB8500_CODEC_CR68_AD4GAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR68, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR69                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR69(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr69_fadedis_ad5,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR69_FADEDIS_AD5 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr69_ad5gain, AB8500_CODEC_MASK_SIX_BITS,
				    AB8500_CODEC_CR69_AD5GAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR69, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR70                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR70(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr70_fadedis_ad6,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR70_FADEDIS_AD6 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr70_ad6gain, AB8500_CODEC_MASK_SIX_BITS,
				    AB8500_CODEC_CR70_AD6GAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR70, value));
}


/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR71                                                          */
/********************************************************************************************/
    PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR71(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr71_fadedis_da1,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR71_FADEDIS_DA1 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr71_da1gain, AB8500_CODEC_MASK_SIX_BITS,
				    AB8500_CODEC_CR71_DA1GAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR71, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR72                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR72(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr72_fadedis_da2,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR72_FADEDIS_DA2 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr72_da2gain, AB8500_CODEC_MASK_SIX_BITS,
				    AB8500_CODEC_CR72_DA2GAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR72, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR73                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR73(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr73_fadedis_da3,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR73_FADEDIS_DA3 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr73_da3gain, AB8500_CODEC_MASK_SIX_BITS,
				    AB8500_CODEC_CR73_DA3GAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR73, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR74                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR74(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr74_fadedis_da4,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR74_FADEDIS_DA4 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr74_da4gain, AB8500_CODEC_MASK_SIX_BITS,
				    AB8500_CODEC_CR74_DA4GAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR74, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR75                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR75(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr75_fadedis_da5,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR75_FADEDIS_DA5 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr75_da5gain, AB8500_CODEC_MASK_SIX_BITS,
				    AB8500_CODEC_CR75_DA5GAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR75, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR76                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR76(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr76_fadedis_da6,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR76_FADEDIS_DA6 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr76_da6gain, AB8500_CODEC_MASK_SIX_BITS,
				    AB8500_CODEC_CR76_DA6GAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR76, value));
}

#if 0

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR77                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR77(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr77_fadedis_ad1l,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR77_FADEDIS_AD1L );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr77_ad1lbgain_to_hfl,
				    AB8500_CODEC_MASK_SIX_BITS,
				    AB8500_CODEC_CR77_AD1LBGAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR77, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR78                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR78(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr78_fadedis_ad2l,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR78_FADEDIS_AD2L );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr78_ad2lbgain_to_hfr,
				    AB8500_CODEC_MASK_SIX_BITS,
				    AB8500_CODEC_CR78_AD2LBGAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR78, value));
}

#endif /*  */

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR79                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR79(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr79_hssinc1, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR79_HSSINC1 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr79_fadedis_hsl,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR79_FADEDIS_HSL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr79_hsldgain, AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR79_HSLDGAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR79, value));
}


/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR80                                                          */
/********************************************************************************************/
    PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR80(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr80_fade_speed,
				     AB8500_CODEC_MASK_TWO_BITS,
				     AB8500_CODEC_CR80_FADE_SPEED );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr80_fadedis_hsr,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR80_FADEDIS_HSR );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr80_hsrdgain, AB8500_CODEC_MASK_FOUR_BITS,
				    AB8500_CODEC_CR80_HSRDGAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR80, value));
}

#if 0

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR81                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR81(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr81_stfir1gain,
				     AB8500_CODEC_MASK_FIVE_BITS,
				     AB8500_CODEC_CR81_STFIR1GAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR81, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR82                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR82(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr82_stfir2gain,
				     AB8500_CODEC_MASK_FIVE_BITS,
				     AB8500_CODEC_CR82_STFIR2GAIN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR82, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR83                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR83(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr83_enanc, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR83_ENANC );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr83_anciirinit, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR83_ANCIIRINIT );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr83_ancfirupdate,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR83_ANCFIRUPDATE );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR83, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR84                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR84(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr84_ancinshift,
				     AB8500_CODEC_MASK_FIVE_BITS,
				     AB8500_CODEC_CR84_ANCINSHIFT );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR84, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR85                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR85(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr85_ancfiroutshift,
				     AB8500_CODEC_MASK_FIVE_BITS,
				     AB8500_CODEC_CR85_ANCFIROUTSHIFT );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR85, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR86                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR86(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr86_ancshiftout,
				     AB8500_CODEC_MASK_FIVE_BITS,
				     AB8500_CODEC_CR86_ANCSHIFTOUT );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR86, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR87                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR87(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr87_ancfircoeff_msb,
				     AB8500_CODEC_MASK_EIGHT_BITS,
				     AB8500_CODEC_CR87_ANCFIRCOEFF_MSB );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR87, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR88                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR88(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr88_ancfircoeff_lsb,
				     AB8500_CODEC_MASK_EIGHT_BITS,
				     AB8500_CODEC_CR88_ANCFIRCOEFF_LSB );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR88, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR89                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR89(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr89_anciircoeff_msb,
				     AB8500_CODEC_MASK_EIGHT_BITS,
				     AB8500_CODEC_CR89_ANCIIRCOEFF_MSB );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR89, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR90                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR90(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr90_anciircoeff_lsb,
				     AB8500_CODEC_MASK_EIGHT_BITS,
				     AB8500_CODEC_CR90_ANCIIRCOEFF_LSB );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR90, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR91                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR91(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr91_ancwarpdel_msb,
				     AB8500_CODEC_MASK_EIGHT_BITS,
				     AB8500_CODEC_CR91_ANCWARPDEL_MSB );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR91, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR92                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR92(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr92_ancwarpdel_lsb,
				     AB8500_CODEC_MASK_EIGHT_BITS,
				     AB8500_CODEC_CR92_ANCWARPDEL_LSB );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR92, value));
}

/* CR93 is Read Only */
/* CR94 is Read Only */
/* CR95 is Read Only */
/* CR96 is Read Only */
/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR97                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR97(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr97_stfir_set, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR97_STFIR_SET );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr97_stfir_addr,
				    AB8500_CODEC_MASK_SEVEN_BITS,
				    AB8500_CODEC_CR97_STFIR_ADDR );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR97, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR98                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR98(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr98_stfir_coeff_msb,
				     AB8500_CODEC_MASK_EIGHT_BITS,
				     AB8500_CODEC_CR98_STFIR_COEFF_MSB );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR98, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR99                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR99(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr99_stfir_coeff_lsb,
				     AB8500_CODEC_MASK_EIGHT_BITS,
				     AB8500_CODEC_CR99_STFIR_COEFF_LSB );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR99, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR100                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR100(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr100_enstfirs, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR100_ENSTFIRS );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr100_stfirstoif1,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR100_STFIRSTOIF1 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr100_stfir_busy,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR100_STFIR_BUSY );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR100, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR101                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR101(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr101_hsoffst_mask,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR101_HSOFFSTMASK );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr101_fifofull_mask,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR101_FIFOFULLMASK );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr101_fifoempty_mask,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR101_FIFOEMPTYMASK );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr101_dasat_mask,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR101_DASATMASK );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr101_adsat_mask,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR101_ADSATMASK );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr101_addsp_mask,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR101_ADDSPMASK );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr101_dadsp_mask,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR101_DADSPMASK );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr101_firsid_mask,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR101_FIRSIDMASK );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR101, value));
}

/* CR102 is Read Only */

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR103                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR103(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr103_vssready_mask,
				     AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR103_VSSREADYMASK );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr103_shorthsl_mask,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR103_SHORTHSLMASK );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr103_shorthsr_mask,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR103_SHORTHSRMASK );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr103_shortear_mask,
				    AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR103_SHORTEARMASK );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR103, value));
}

#endif /*  */

/* CR104 is Read Only */

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR105                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR105(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr105_bfifomsk, AB8500_CODEC_MASK_ONE_BIT,
				     AB8500_CODEC_CR105_BFIFOMASK );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr105_bfifoint, AB8500_CODEC_MASK_SIX_BITS,
				    AB8500_CODEC_CR105_BFIFOINT );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR105, value));
}


/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR106                                                          */
/********************************************************************************************/
    PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR106(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr106_bfifotx,
				     AB8500_CODEC_MASK_EIGHT_BITS,
				     AB8500_CODEC_CR106_BFIFOTX );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR106, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR107                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR107(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr107_bfifoexsl,
				     AB8500_CODEC_MASK_THREE_BITS,
				     AB8500_CODEC_CR107_BFIFOEXSL );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr107_prebitclk0,
				    AB8500_CODEC_MASK_THREE_BITS,
				    AB8500_CODEC_CR107_PREBITCLK0 );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr107_bfifomast, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR107_BFIFOMAST );
	AB8500_CODEC_WRITE_BITS  (value,
				    (t_uint8) p_ab8500_codec_configuration->
				    cr107_bfiforun, AB8500_CODEC_MASK_ONE_BIT,
				    AB8500_CODEC_CR107_BFIFORUN );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR107, value));
}


/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR108                                                          */
/********************************************************************************************/
    PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR108(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr108_bfifoframsw,
				     AB8500_CODEC_MASK_EIGHT_BITS,
				     AB8500_CODEC_CR108_BFIFOFRAMESW );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR108, value));
}

/********************************************************************************************/
/* Name: ab8500_codec_UpdateCR109                                                          */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_UpdateCR109(void)
{
	t_uint8 value = 0x00;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	AB8500_CODEC_WRITE_BITS  (value,
				     (t_uint8) p_ab8500_codec_configuration->
				     cr109_bfifowakeup,
				     AB8500_CODEC_MASK_EIGHT_BITS,
				     AB8500_CODEC_CR109_BFIFOWAKEUP );
	return (ab8500_codec_SingleWrite(AB8500_CODEC_CR109, value));
}

/* CR110 is Read Only */

/* CR111 is Read Only */

/********************************************************************************************/
/* Name: ab8500_codec_Reset()                                                              */

/********************************************************************************************/
PRIVATE t_ab8500_codec_error ab8500_codec_Reset(void)
{
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	p_ab8500_codec_configuration->cr1_swreset =
	    AB8500_CODEC_CR1_SWRESET_ENABLED;
	ab8500_codec_error = ab8500_codec_UpdateCR1();
	if (AB8500_CODEC_OK != ab8500_codec_error)
		 {
		return (ab8500_codec_error);
		}
	return (ab8500_codec_error);
}

PRIVATE t_ab8500_codec_error ab8500_codec_ProgramDirection(IN
							     t_ab8500_codec_direction
							     ab8500_codec_direction)
    /*only IN or OUT must be passed (not INOUT) */
{
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	if (AB8500_CODEC_DIRECTION_IN == ab8500_codec_direction)
		 {
		ab8500_codec_error = ab8500_codec_ProgramDirectionIN();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		}
	if (AB8500_CODEC_DIRECTION_OUT == ab8500_codec_direction)
		 {
		ab8500_codec_error = ab8500_codec_ProgramDirectionOUT();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		}
	return (ab8500_codec_error);
}

PRIVATE t_ab8500_codec_error ab8500_codec_SetDirection(IN
							 t_ab8500_codec_direction
							 ab8500_codec_direction)
{
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	switch (ab8500_codec_direction)
		 {
	case AB8500_CODEC_DIRECTION_IN:
		ab8500_codec_error =
		    ab8500_codec_ProgramDirection(AB8500_CODEC_DIRECTION_IN);
		break;
	case AB8500_CODEC_DIRECTION_OUT:
		ab8500_codec_error =
		    ab8500_codec_ProgramDirection(AB8500_CODEC_DIRECTION_OUT);
		break;
	case AB8500_CODEC_DIRECTION_INOUT:
		ab8500_codec_error =
		    ab8500_codec_ProgramDirection(AB8500_CODEC_DIRECTION_IN);
		if (AB8500_CODEC_OK == ab8500_codec_error)
			 {
			ab8500_codec_error =
			    ab8500_codec_ProgramDirection
			    (AB8500_CODEC_DIRECTION_OUT);
			}
		break;
		}
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR5();
	if (AB8500_CODEC_OK != ab8500_codec_error)
		 {
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR6();
	if (AB8500_CODEC_OK != ab8500_codec_error)
		 {
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR7();
	if (AB8500_CODEC_OK != ab8500_codec_error)
		 {
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR8();
	if (AB8500_CODEC_OK != ab8500_codec_error)
		 {
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR9();
	if (AB8500_CODEC_OK != ab8500_codec_error)
		 {
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR10();
	if (AB8500_CODEC_OK != ab8500_codec_error)
		 {
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR12();
	if (AB8500_CODEC_OK != ab8500_codec_error)
		 {
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR15();
	if (AB8500_CODEC_OK != ab8500_codec_error)
		 {
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR63();
	if (AB8500_CODEC_OK != ab8500_codec_error)
		 {
		return (ab8500_codec_error);
		}
	return (ab8500_codec_error);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_Init                                                  */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Initialize the global variables & stores the slave address of codec.     */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN:                                                                      */
/* slave_address_of_ab8500_codec: Audio codec slave address                 */
/* OUT:                                                                     */
/* None                                                                     */
/*                                                                          */
/* RETURN:                                                                  */
/* Returns AB8500_CODEC_OK                                                  */
/* COMMENTS:                                                                */
/* 1) Saves the supplied slave_address_of_codec in global variable          */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Non Re-Entrant                                               */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_Init(IN t_uint8
					      slave_address_of_ab8500_codec)
{
	DBGENTER1(" (%lx)", slave_address_of_ab8500_codec);
	g_ab8500_codec_system_context.slave_address_of_ab8500_codec =
	    slave_address_of_ab8500_codec;
	DBGEXIT(AB8500_CODEC_OK);
	return (AB8500_CODEC_OK);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_Reset                                                 */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Reset the global variables and clear audiocodec settings to default.     */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN:                                                                      */
/* None                                                                     */
/* OUT:                                                                     */
/* None                                                                     */
/*                                                                          */
/* RETURN:                                                                  */
/* AB8500_CODEC_TRANSACTION_FAILED: If transaction fails.                   */
/* AB8500_CODEC_OK: if successful.                                          */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Non Re-Entrant                                               */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_Reset(void)
{
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	DBGENTER();
	g_ab8500_codec_system_context.ab8500_codec_direction =
	    AB8500_CODEC_DEFAULT_DIRECTION;
	g_ab8500_codec_system_context.ab8500_codec_mode_in =
	    AB8500_CODEC_DEFAULT_MODE_IN;
	g_ab8500_codec_system_context.ab8500_codec_mode_out =
	    AB8500_CODEC_DEFAULT_MODE_OUT;
	g_ab8500_codec_system_context.ab8500_codec_src =
	    AB8500_CODEC_DEFAULT_INPUT_SRC;
	g_ab8500_codec_system_context.ab8500_codec_dest =
	    AB8500_CODEC_DEFAULT_OUTPUT_DEST;
	g_ab8500_codec_system_context.in_left_volume =
	    AB8500_CODEC_DEFAULT_VOLUME_LEFT_IN;
	g_ab8500_codec_system_context.in_right_volume =
	    AB8500_CODEC_DEFAULT_VOLUME_RIGHT_IN;
	g_ab8500_codec_system_context.out_left_volume =
	    AB8500_CODEC_DEFAULT_VOLUME_LEFT_OUT;
	g_ab8500_codec_system_context.out_right_volume =
	    AB8500_CODEC_DEFAULT_VOLUME_RIGHT_OUT;
	ab8500_codec_error = ab8500_codec_Reset();
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_SetModeAndDirection                                   */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Configures the whole audio codec to work in audio mode                   */
/* (using I2S protocol).                                                    */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN:                                                                      */
/* direction: select the direction (IN, OUT or INOUT)                       */
/* in_mode: codec mode for recording. If direction is OUT only,             */
/* this parameter is ignored.                                               */
/* out_mode: codec mode for playing. If direction is IN only,               */
/* this parameter is ignored.                                               */
/* p_tdm_config: TDM configuration required to be configured by user        */
/* OUT:                                                                     */
/* None                                                                     */
/*                                                                          */
/* RETURN:                                                                  */
/* AB8500_CODEC_UNSUPPORTED_FEATURE: The API may not allow setting          */
/* 2 different modes, in which case it should return this value.            */
/* AB8500_CODEC_TRANSACTION_FAILED: If transaction fails.                   */
/* AB8500_CODEC_OK: if successful.                                          */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Non Re-Entrant                                               */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_SetModeAndDirection
    (IN t_ab8500_codec_direction ab8500_codec_direction,
     IN t_ab8500_codec_mode ab8500_codec_mode_in,
     IN t_ab8500_codec_mode ab8500_codec_mode_out,
     IN t_ab8500_codec_tdm_config const *const p_tdm_config ) {
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	DBGENTER3(" (%lx %lx %lx)", ab8500_codec_direction,
		    ab8500_codec_mode_in, ab8500_codec_mode_out);
	if (AB8500_CODEC_AUDIO_INTERFACE_1 ==
	       g_ab8500_codec_system_context.audio_interface)
		 {
		if (AB8500_CODEC_DIRECTION_OUT == ab8500_codec_direction
		     || AB8500_CODEC_DIRECTION_INOUT ==
		     ab8500_codec_direction )
			 {
			p_ab8500_codec_configuration->cr3_enda1 =
			    AB8500_CODEC_CR3_ENDA1_ENABLED;
			p_ab8500_codec_configuration->cr3_enda2 =
			    AB8500_CODEC_CR3_ENDA2_ENABLED;
			p_ab8500_codec_configuration->cr3_enda3 =
			    AB8500_CODEC_CR3_ENDA3_ENABLED;
			p_ab8500_codec_configuration->cr3_enda4 =
			    AB8500_CODEC_CR3_ENDA4_ENABLED;
			p_ab8500_codec_configuration->cr3_enda5 =
			    AB8500_CODEC_CR3_ENDA5_ENABLED;
			p_ab8500_codec_configuration->cr3_enda6 =
			    AB8500_CODEC_CR3_ENDA6_ENABLED;
			p_ab8500_codec_configuration->cr27_if1_bitclk_osr =
			    p_tdm_config->cr27_if1_bitclk_osr;
			if (AB8500_CODEC_MODE_HIFI == ab8500_codec_mode_out)
				 {
				p_ab8500_codec_configuration->cr30_fsync1p =
				    AB8500_CODEC_CR30_FSYNC1P_FALLING_EDGE;
				p_ab8500_codec_configuration->cr30_bitclk1p =
				    AB8500_CODEC_CR30_BITCLK1P_FALLING_EDGE;
				p_ab8500_codec_configuration->cr30_if1del =
				    AB8500_CODEC_CR30_IF1DEL_DELAYED;
				p_ab8500_codec_configuration->cr30_if1format =
				    AB8500_CODEC_CR30_IF1FORMAT_I2S_LEFTALIGNED;
				p_ab8500_codec_configuration->cr30_if1wl =
				    p_tdm_config->cr30_if1wl;
				}

			else
				 {
				p_ab8500_codec_configuration->cr30_fsync1p =
				    AB8500_CODEC_CR30_FSYNC1P_FALLING_EDGE;
				p_ab8500_codec_configuration->cr30_bitclk1p =
				    AB8500_CODEC_CR30_BITCLK1P_FALLING_EDGE;
				p_ab8500_codec_configuration->cr30_if1del =
				    AB8500_CODEC_CR30_IF1DEL_DELAYED;
				p_ab8500_codec_configuration->cr30_if1format =
				    AB8500_CODEC_CR30_IF1FORMAT_TDM;
				p_ab8500_codec_configuration->cr30_if1wl =
				    p_tdm_config->cr30_if1wl;
				}
			}
		if (AB8500_CODEC_DIRECTION_IN == ab8500_codec_direction
		      || AB8500_CODEC_DIRECTION_INOUT ==
		      ab8500_codec_direction )
			 {
			p_ab8500_codec_configuration->cr2_enad1 =
			    AB8500_CODEC_CR2_ENAD1_ENABLED;
			p_ab8500_codec_configuration->cr2_enad2 =
			    AB8500_CODEC_CR2_ENAD2_ENABLED;
			p_ab8500_codec_configuration->cr2_enad3 =
			    AB8500_CODEC_CR2_ENAD3_ENABLED;
			p_ab8500_codec_configuration->cr2_enad4 =
			    AB8500_CODEC_CR2_ENAD4_ENABLED;
			p_ab8500_codec_configuration->cr2_enad5 =
			    AB8500_CODEC_CR2_ENAD5_ENABLED;
			p_ab8500_codec_configuration->cr2_enad6 =
			    AB8500_CODEC_CR2_ENAD6_ENABLED;
			p_ab8500_codec_configuration->cr27_if1_bitclk_osr =
			    p_tdm_config->cr27_if1_bitclk_osr;
			if (AB8500_CODEC_MODE_HIFI == ab8500_codec_mode_in)
				 {
				p_ab8500_codec_configuration->cr30_fsync1p =
				    AB8500_CODEC_CR30_FSYNC1P_FALLING_EDGE;
				p_ab8500_codec_configuration->cr30_bitclk1p =
				    AB8500_CODEC_CR30_BITCLK1P_FALLING_EDGE;
				p_ab8500_codec_configuration->cr30_if1del =
				    AB8500_CODEC_CR30_IF1DEL_DELAYED;
				p_ab8500_codec_configuration->cr30_if1format =
				    AB8500_CODEC_CR30_IF1FORMAT_I2S_LEFTALIGNED;
				p_ab8500_codec_configuration->cr30_if1wl =
				    p_tdm_config->cr30_if1wl;
				}

			else
				 {
				p_ab8500_codec_configuration->cr30_fsync1p =
				    AB8500_CODEC_CR30_FSYNC1P_RISING_EDGE;
				p_ab8500_codec_configuration->cr30_bitclk1p =
				    AB8500_CODEC_CR30_BITCLK1P_FALLING_EDGE;
				p_ab8500_codec_configuration->cr30_if1del =
				    AB8500_CODEC_CR30_IF1DEL_NOT_DELAYED;
				p_ab8500_codec_configuration->cr30_if1format =
				    AB8500_CODEC_CR30_IF1FORMAT_TDM;
				p_ab8500_codec_configuration->cr30_if1wl =
				    p_tdm_config->cr30_if1wl;
				}
			}
		}

	else
		 {
		if (AB8500_CODEC_DIRECTION_OUT == ab8500_codec_direction
		     || AB8500_CODEC_DIRECTION_INOUT ==
		     ab8500_codec_direction )
			 {
			p_ab8500_codec_configuration->cr3_enda1 =
			    AB8500_CODEC_CR3_ENDA1_ENABLED;
			p_ab8500_codec_configuration->cr3_enda2 =
			    AB8500_CODEC_CR3_ENDA2_ENABLED;
			p_ab8500_codec_configuration->cr3_enda3 =
			    AB8500_CODEC_CR3_ENDA3_ENABLED;
			p_ab8500_codec_configuration->cr3_enda4 =
			    AB8500_CODEC_CR3_ENDA4_ENABLED;
			p_ab8500_codec_configuration->cr3_enda5 =
			    AB8500_CODEC_CR3_ENDA5_ENABLED;
			p_ab8500_codec_configuration->cr3_enda6 =
			    AB8500_CODEC_CR3_ENDA6_ENABLED;
			p_ab8500_codec_configuration->cr27_if0_bitclk_osr =
			    p_tdm_config->cr27_if0_bitclk_osr;
			p_ab8500_codec_configuration->cr63_datohslen =
			    AB8500_CODEC_CR63_DATOHSLEN_ENABLED;
			p_ab8500_codec_configuration->cr63_datohsren =
			    AB8500_CODEC_CR63_DATOHSREN_ENABLED;
			if (AB8500_CODEC_MODE_HIFI == ab8500_codec_mode_out)
				 {
				p_ab8500_codec_configuration->cr28_fsync0p =
				    AB8500_CODEC_CR28_FSYNC0P_FALLING_EDGE;
				p_ab8500_codec_configuration->cr28_bitclk0p = p_tdm_config->cr28_bitclk0p;	/*AB8500_CODEC_CR28_BITCLK0P_FALLING_EDGE; */
				p_ab8500_codec_configuration->cr28_if0del = p_tdm_config->cr28_if0del;	/*AB8500_CODEC_CR28_IF0DEL_DELAYED; */
				p_ab8500_codec_configuration->cr28_if0format =
				    AB8500_CODEC_CR28_IF0FORMAT_I2S_LEFTALIGNED;
				p_ab8500_codec_configuration->cr28_if0wl =
				    p_tdm_config->cr28_if0wl;
				}

			else
				 {
				p_ab8500_codec_configuration->cr28_fsync0p =
				    AB8500_CODEC_CR28_FSYNC0P_FALLING_EDGE;
				p_ab8500_codec_configuration->cr28_bitclk0p = p_tdm_config->cr28_bitclk0p;	/*AB8500_CODEC_CR28_BITCLK0P_FALLING_EDGE; */
				p_ab8500_codec_configuration->cr28_if0del = p_tdm_config->cr28_if0del;	/*AB8500_CODEC_CR28_IF0DEL_DELAYED; */
				p_ab8500_codec_configuration->cr28_if0format =
				    AB8500_CODEC_CR28_IF0FORMAT_TDM;
				p_ab8500_codec_configuration->cr28_if0wl =
				    p_tdm_config->cr28_if0wl;
				}
			}
		if (AB8500_CODEC_DIRECTION_IN == ab8500_codec_direction
		      || AB8500_CODEC_DIRECTION_INOUT ==
		      ab8500_codec_direction )
			 {
			p_ab8500_codec_configuration->cr2_enad1 =
			    AB8500_CODEC_CR2_ENAD1_ENABLED;
			p_ab8500_codec_configuration->cr2_enad2 =
			    AB8500_CODEC_CR2_ENAD2_ENABLED;
			p_ab8500_codec_configuration->cr2_enad3 =
			    AB8500_CODEC_CR2_ENAD3_ENABLED;
			p_ab8500_codec_configuration->cr2_enad4 =
			    AB8500_CODEC_CR2_ENAD4_ENABLED;
			p_ab8500_codec_configuration->cr2_enad5 =
			    AB8500_CODEC_CR2_ENAD5_ENABLED;
			p_ab8500_codec_configuration->cr2_enad6 =
			    AB8500_CODEC_CR2_ENAD6_ENABLED;
			p_ab8500_codec_configuration->cr26_ad1_voice =
			    AB8500_CODEC_CR26_AD1_VOICE_LOWLATENCYFILTER;
			p_ab8500_codec_configuration->cr26_ad2_voice =
			    AB8500_CODEC_CR26_AD2_VOICE_LOWLATENCYFILTER;
			p_ab8500_codec_configuration->cr26_ad3_voice =
			    AB8500_CODEC_CR26_AD3_VOICE_LOWLATENCYFILTER;
			p_ab8500_codec_configuration->cr26_ad4_voice =
			    AB8500_CODEC_CR26_AD4_VOICE_LOWLATENCYFILTER;
			p_ab8500_codec_configuration->cr27_if0_bitclk_osr =
			    p_tdm_config->cr27_if0_bitclk_osr;
			if (AB8500_CODEC_MODE_HIFI == ab8500_codec_mode_in)
				 {
				p_ab8500_codec_configuration->cr28_fsync0p =
				    AB8500_CODEC_CR28_FSYNC0P_RISING_EDGE;
				p_ab8500_codec_configuration->cr28_bitclk0p = p_tdm_config->cr28_bitclk0p;	/*AB8500_CODEC_CR28_BITCLK0P_RISING_EDGE; */
				p_ab8500_codec_configuration->cr28_if0del = p_tdm_config->cr28_if0del;	/*AB8500_CODEC_CR28_IF0DEL_NOT_DELAYED; */
				p_ab8500_codec_configuration->cr28_if0format =
				    AB8500_CODEC_CR28_IF0FORMAT_I2S_LEFTALIGNED;
				p_ab8500_codec_configuration->cr28_if0wl =
				    p_tdm_config->cr28_if0wl;
				}

			else
				 {
				p_ab8500_codec_configuration->cr28_fsync0p =
				    AB8500_CODEC_CR28_FSYNC0P_RISING_EDGE;
				p_ab8500_codec_configuration->cr28_bitclk0p = p_tdm_config->cr28_bitclk0p;	/*AB8500_CODEC_CR28_BITCLK0P_FALLING_EDGE; */
				p_ab8500_codec_configuration->cr28_if0del = p_tdm_config->cr28_if0del;	/*AB8500_CODEC_CR28_IF0DEL_NOT_DELAYED; */
				p_ab8500_codec_configuration->cr28_if0format =
				    AB8500_CODEC_CR28_IF0FORMAT_TDM;
				p_ab8500_codec_configuration->cr28_if0wl =
				    p_tdm_config->cr28_if0wl;
				}
			}
		}
	ab8500_codec_error = ab8500_codec_SetModeAndDirectionUpdateCR();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	g_ab8500_codec_system_context.ab8500_codec_direction =
	    ab8500_codec_direction;
	g_ab8500_codec_system_context.ab8500_codec_mode_in =
	    ab8500_codec_mode_in;
	g_ab8500_codec_system_context.ab8500_codec_mode_out =
	    ab8500_codec_mode_out;
	ab8500_codec_error =
	    ab8500_codec_SetDirection(ab8500_codec_direction);
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_SetSrcVolume                                          */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Sets the record volumes.                                                 */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN:                                                                      */
/* t_ab8500_codec_src: select source device for recording.                  */
/* in_left_volume: record volume for left channel.                          */
/* in_right_volume: record volume for right channel.                        */
/* OUT:                                                                     */
/* None                                                                     */
/*                                                                          */
/* RETURN:                                                                  */
/* AB8500_CODEC_TRANSACTION_FAILED: If transaction fails.                   */
/* AB8500_CODEC_OK: if successful.                                          */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Non Re-Entrant                                               */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_SetSrcVolume
    (IN t_ab8500_codec_src src_device, IN t_uint8 in_left_volume,
     IN t_uint8 in_right_volume )  {
	t_ab8500_codec_error ab8500_codec_error;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	DBGENTER3(" (%lx %lx %lx)", src_device, in_left_volume,
		   in_right_volume);
	if (in_left_volume > AB8500_CODEC_MAX_VOLUME)
		 {
		in_left_volume = AB8500_CODEC_MAX_VOLUME;
		}
	if (in_right_volume > AB8500_CODEC_MAX_VOLUME)
		 {
		in_right_volume = AB8500_CODEC_MAX_VOLUME;
		}
	g_ab8500_codec_system_context.in_left_volume = in_left_volume;
	g_ab8500_codec_system_context.in_right_volume = in_right_volume;
	p_ab8500_codec_configuration->cr65_ad1gain =
	    AB8500_CODEC_AD_D_VOLUME_MIN +
	    (in_left_volume *
	     (AB8500_CODEC_AD_D_VOLUME_MAX -
	      AB8500_CODEC_AD_D_VOLUME_MIN)) / 100;
	p_ab8500_codec_configuration->cr66_ad2gain =
	    AB8500_CODEC_AD_D_VOLUME_MIN +
	    (in_left_volume *
	     (AB8500_CODEC_AD_D_VOLUME_MAX -
	      AB8500_CODEC_AD_D_VOLUME_MIN)) / 100;
	p_ab8500_codec_configuration->cr67_ad3gain =
	    AB8500_CODEC_AD_D_VOLUME_MIN +
	    (in_left_volume *
	     (AB8500_CODEC_AD_D_VOLUME_MAX -
	      AB8500_CODEC_AD_D_VOLUME_MIN)) / 100;
	p_ab8500_codec_configuration->cr68_ad4gain =
	    AB8500_CODEC_AD_D_VOLUME_MIN +
	    (in_left_volume *
	     (AB8500_CODEC_AD_D_VOLUME_MAX -
	      AB8500_CODEC_AD_D_VOLUME_MIN)) / 100;
	p_ab8500_codec_configuration->cr69_ad5gain =
	    AB8500_CODEC_AD_D_VOLUME_MIN +
	    (in_left_volume *
	     (AB8500_CODEC_AD_D_VOLUME_MAX -
	      AB8500_CODEC_AD_D_VOLUME_MIN)) / 100;
	p_ab8500_codec_configuration->cr70_ad6gain =
	    AB8500_CODEC_AD_D_VOLUME_MIN +
	    (in_left_volume *
	     (AB8500_CODEC_AD_D_VOLUME_MAX -
	      AB8500_CODEC_AD_D_VOLUME_MIN)) / 100;

	    /* Set mininimum volume if volume is zero */
	    switch (src_device)
		 {
	case AB8500_CODEC_SRC_LINEIN:
		p_ab8500_codec_configuration->cr23_linl_gain =
		    AB8500_CODEC_LINEIN_VOLUME_MIN +
		    (in_left_volume *
		     (AB8500_CODEC_LINEIN_VOLUME_MAX -
		      AB8500_CODEC_LINEIN_VOLUME_MIN)) / 100;
		p_ab8500_codec_configuration->cr23_linr_gain =
		    AB8500_CODEC_LINEIN_VOLUME_MIN +
		    (in_right_volume *
		     (AB8500_CODEC_LINEIN_VOLUME_MAX -
		      AB8500_CODEC_LINEIN_VOLUME_MIN)) / 100;
		break;
	case AB8500_CODEC_SRC_MICROPHONE_1A:
	case AB8500_CODEC_SRC_MICROPHONE_1B:
		p_ab8500_codec_configuration->cr20_mic1_gain =
		    AB8500_CODEC_MIC_VOLUME_MIN +
		    (in_left_volume *
		     (AB8500_CODEC_MIC_VOLUME_MAX -
		      AB8500_CODEC_MIC_VOLUME_MIN)) / 100;
		break;
	case AB8500_CODEC_SRC_MICROPHONE_2:
		p_ab8500_codec_configuration->cr21_mic2_gain =
		    AB8500_CODEC_MIC_VOLUME_MIN +
		    (in_left_volume *
		     (AB8500_CODEC_MIC_VOLUME_MAX -
		      AB8500_CODEC_MIC_VOLUME_MIN)) / 100;
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_1:
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_2:
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_3:
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_4:
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_5:
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_6:
		break;
	case AB8500_CODEC_SRC_ALL:
		p_ab8500_codec_configuration->cr23_linl_gain =
		    AB8500_CODEC_LINEIN_VOLUME_MIN +
		    (in_left_volume *
		     (AB8500_CODEC_LINEIN_VOLUME_MAX -
		      AB8500_CODEC_LINEIN_VOLUME_MIN)) / 100;
		p_ab8500_codec_configuration->cr23_linr_gain =
		    AB8500_CODEC_LINEIN_VOLUME_MIN +
		    (in_right_volume *
		     (AB8500_CODEC_LINEIN_VOLUME_MAX -
		      AB8500_CODEC_LINEIN_VOLUME_MIN)) / 100;
		p_ab8500_codec_configuration->cr20_mic1_gain =
		    AB8500_CODEC_MIC_VOLUME_MIN +
		    (in_left_volume *
		     (AB8500_CODEC_MIC_VOLUME_MAX -
		      AB8500_CODEC_MIC_VOLUME_MIN)) / 100;
		p_ab8500_codec_configuration->cr21_mic2_gain =
		    AB8500_CODEC_MIC_VOLUME_MIN +
		    (in_left_volume *
		     (AB8500_CODEC_MIC_VOLUME_MAX -
		      AB8500_CODEC_MIC_VOLUME_MIN)) / 100;
		break;
	default:
		ab8500_codec_error = AB8500_CODEC_INVALID_PARAMETER;
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_SetSrcVolumeUpdateCR();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_SetDestVolume                                         */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Sets the play volumes.                                                   */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN:                                                                      */
/* out_left_volume: play volume for left channel.                           */
/* out_right_volume: play volume for right channel.                         */
/* OUT:                                                                     */
/* None                                                                     */
/*                                                                          */
/* RETURN:                                                                  */
/* AB8500_CODEC_TRANSACTION_FAILED: If transaction fails.                   */
/* AB8500_CODEC_OK: if successful.                                          */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Non Re-Entrant                                               */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_SetDestVolume
    (IN t_ab8500_codec_dest dest_device, IN t_uint8 out_left_volume,
     IN t_uint8 out_right_volume )  {
	t_ab8500_codec_error ab8500_codec_error;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	DBGENTER3(" (%lx %lx %lx)", dest_device, out_left_volume,
		   out_right_volume);
	if (out_left_volume > AB8500_CODEC_MAX_VOLUME)
		 {
		out_left_volume = AB8500_CODEC_MAX_VOLUME;
		}
	if (out_right_volume > AB8500_CODEC_MAX_VOLUME)
		 {
		out_right_volume = AB8500_CODEC_MAX_VOLUME;
		}
	g_ab8500_codec_system_context.out_left_volume = out_left_volume;
	g_ab8500_codec_system_context.out_right_volume = out_right_volume;
	p_ab8500_codec_configuration->cr71_da1gain =
	    AB8500_CODEC_DA_D_VOLUME_MIN +
	    (out_left_volume *
	     (AB8500_CODEC_DA_D_VOLUME_MAX -
	      AB8500_CODEC_DA_D_VOLUME_MIN)) / 100;
	p_ab8500_codec_configuration->cr72_da2gain =
	    AB8500_CODEC_DA_D_VOLUME_MIN +
	    (out_left_volume *
	     (AB8500_CODEC_DA_D_VOLUME_MAX -
	      AB8500_CODEC_DA_D_VOLUME_MIN)) / 100;
	p_ab8500_codec_configuration->cr73_da3gain =
	    AB8500_CODEC_DA_D_VOLUME_MIN +
	    (out_left_volume *
	     (AB8500_CODEC_DA_D_VOLUME_MAX -
	      AB8500_CODEC_DA_D_VOLUME_MIN)) / 100;
	p_ab8500_codec_configuration->cr74_da4gain =
	    AB8500_CODEC_DA_D_VOLUME_MIN +
	    (out_left_volume *
	     (AB8500_CODEC_DA_D_VOLUME_MAX -
	      AB8500_CODEC_DA_D_VOLUME_MIN)) / 100;
	p_ab8500_codec_configuration->cr75_da5gain =
	    AB8500_CODEC_DA_D_VOLUME_MIN +
	    (out_left_volume *
	     (AB8500_CODEC_DA_D_VOLUME_MAX -
	      AB8500_CODEC_DA_D_VOLUME_MIN)) / 100;
	p_ab8500_codec_configuration->cr76_da6gain =
	    AB8500_CODEC_DA_D_VOLUME_MIN +
	    (out_left_volume *
	     (AB8500_CODEC_DA_D_VOLUME_MAX -
	      AB8500_CODEC_DA_D_VOLUME_MIN)) / 100;

	    /* Set mininimum volume if volume is zero */
	    switch (dest_device)
		 {
	case AB8500_CODEC_DEST_HEADSET:
		p_ab8500_codec_configuration->cr22_hsl_gain =
		    AB8500_CODEC_HEADSET_VOLUME_MIN +
		    (out_left_volume *
		     (AB8500_CODEC_HEADSET_VOLUME_MAX -
		      AB8500_CODEC_HEADSET_VOLUME_MIN)) / 100;
		p_ab8500_codec_configuration->cr22_hsr_gain =
		    AB8500_CODEC_HEADSET_VOLUME_MIN +
		    (out_right_volume *
		     (AB8500_CODEC_HEADSET_VOLUME_MAX -
		      AB8500_CODEC_HEADSET_VOLUME_MIN)) / 100;
		p_ab8500_codec_configuration->cr79_hsldgain =
		    AB8500_CODEC_HEADSET_D_VOLUME_0DB;
		p_ab8500_codec_configuration->cr80_hsrdgain =
		    AB8500_CODEC_HEADSET_D_VOLUME_0DB;
		break;
	case AB8500_CODEC_DEST_EARPIECE:
		p_ab8500_codec_configuration->cr79_hsldgain =
		    AB8500_CODEC_HEADSET_D_VOLUME_0DB;
		break;
	case AB8500_CODEC_DEST_HANDSFREE:
		break;
	case AB8500_CODEC_DEST_VIBRATOR_L:
		p_ab8500_codec_configuration->cr16_pwmnldutycycle =
		    AB8500_CODEC_VIBRATOR_VOLUME_MIN;
		p_ab8500_codec_configuration->cr17_pwmpldutycycle =
		    AB8500_CODEC_VIBRATOR_VOLUME_MIN +
		    (out_right_volume *
		     (AB8500_CODEC_VIBRATOR_VOLUME_MAX -
		      AB8500_CODEC_VIBRATOR_VOLUME_MIN)) / 100;
		break;
	case AB8500_CODEC_DEST_VIBRATOR_R:
		p_ab8500_codec_configuration->cr18_pwmnrdutycycle =
		    AB8500_CODEC_VIBRATOR_VOLUME_MIN;
		p_ab8500_codec_configuration->cr19_pwmprdutycycle =
		    AB8500_CODEC_VIBRATOR_VOLUME_MIN +
		    (out_right_volume *
		     (AB8500_CODEC_VIBRATOR_VOLUME_MAX -
		      AB8500_CODEC_VIBRATOR_VOLUME_MIN)) / 100;
		break;
	case AB8500_CODEC_DEST_ALL:
		p_ab8500_codec_configuration->cr22_hsl_gain =
		    AB8500_CODEC_HEADSET_VOLUME_MIN +
		    (out_left_volume *
		     (AB8500_CODEC_HEADSET_VOLUME_MAX -
		      AB8500_CODEC_HEADSET_VOLUME_MIN)) / 100;
		p_ab8500_codec_configuration->cr22_hsr_gain =
		    AB8500_CODEC_HEADSET_VOLUME_MIN +
		    (out_right_volume *
		     (AB8500_CODEC_HEADSET_VOLUME_MAX -
		      AB8500_CODEC_HEADSET_VOLUME_MIN)) / 100;
		p_ab8500_codec_configuration->cr79_hsldgain =
		    AB8500_CODEC_HEADSET_D_VOLUME_0DB;
		break;
	default:
		ab8500_codec_error = AB8500_CODEC_INVALID_PARAMETER;
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_SetDestVolumeUpdateCR();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_SetMasterMode                                         */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Set the Audio Codec in Master mode.                                      */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN: t_codec_master_mode: Enable/disable master mode                      */
/* OUT:                                                                     */
/* None                                                                     */
/* RETURN:                                                                  */
/* AB8500_CODEC_TRANSACTION_FAILED: If transaction fails.                   */
/* AB8500_CODEC_OK: if successful.                                          */
/* REMARK: Call this API after calling AB8500_CODEC_SetModeAndDirection() API*/
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Non Re-Entrant                                               */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_SetMasterMode(IN
						       t_ab8500_codec_master_mode
						       mode)
{
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	DBGENTER0();
	if (AB8500_CODEC_AUDIO_INTERFACE_1 ==
	      g_ab8500_codec_system_context.audio_interface)
		 {
		p_ab8500_codec_configuration->cr27_en_mastgen =
		    AB8500_CODEC_CR27_EN_MASTGEN_ENABLED;
		p_ab8500_codec_configuration->cr27_enfs_bitclk1 =
		    AB8500_CODEC_CR27_ENFS_BITCLK1_ENABLED;
		if (AB8500_CODEC_MASTER_MODE_ENABLE == mode)
			 {
			p_ab8500_codec_configuration->cr29_if1master =
			    AB8500_CODEC_CR29_IF1MASTER_FS1CK1_OUTPUT;
			}

		else
			 {
			p_ab8500_codec_configuration->cr29_if1master =
			    AB8500_CODEC_CR29_IF1MASTER_FS1CK1_INPUT;
			}
		}

	else
		 {
		p_ab8500_codec_configuration->cr27_en_mastgen =
		    AB8500_CODEC_CR27_EN_MASTGEN_ENABLED;
		p_ab8500_codec_configuration->cr27_enfs_bitclk0 =
		    AB8500_CODEC_CR27_ENFS_BITCLK0_ENABLED;
		if (AB8500_CODEC_MASTER_MODE_ENABLE == mode)
			 {
			p_ab8500_codec_configuration->cr29_if0master =
			    AB8500_CODEC_CR29_IF0MASTER_FS0CK0_OUTPUT;
			}

		else
			 {
			p_ab8500_codec_configuration->cr29_if0master =
			    AB8500_CODEC_CR29_IF0MASTER_FS0CK0_INPUT;
			}
		}
	ab8500_codec_error = ab8500_codec_UpdateCR27();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR29();
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_SelectInput                                           */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Select input source for recording.                                       */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN:                                                                      */
/* input_src: select input source for recording when several sources        */
/* are supported in codec.                                                  */
/* OUT:                                                                     */
/* None                                                                     */
/*                                                                          */
/* RETURN:                                                                  */
/* AB8500_CODEC_INVALID_PARAMETER: If input_src provided is invalid         */
/* by the codec hardware in use.                                            */
/* AB8500_CODEC_TRANSACTION_FAILED: If transaction fails.                   */
/* AB8500_CODEC_OK: if successful.                                          */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Non Re-Entrant                                               */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_SelectInput(IN t_ab8500_codec_src
						     ab8500_codec_src)
{
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	DBGENTER1(" (%lx)", ab8500_codec_src);
	g_ab8500_codec_system_context.ab8500_codec_src = ab8500_codec_src;
	ab8500_codec_error =
	    ab8500_codec_SetDirection(AB8500_CODEC_DIRECTION_IN);
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_SelectOutput                                          */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Select output desination for playing.                                    */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN:                                                                      */
/* output_dest: select output destination for playing when several are      */
/* supported by codec hardware.                                             */
/* OUT:                                                                     */
/* None                                                                     */
/*                                                                          */
/* RETURN:                                                                  */
/* AB8500_CODEC_INVALID_PARAMETER: If output_src provided is invalid        */
/* AB8500_CODEC_TRANSACTION_FAILED: If transaction fails.                   */
/* AB8500_CODEC_OK: if successful.                                          */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Non Re-Entrant                                               */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_SelectOutput(IN t_ab8500_codec_dest
						      ab8500_codec_dest)
{
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	g_ab8500_codec_system_context.ab8500_codec_dest = ab8500_codec_dest;
	DBGENTER1(" (%lx)", ab8500_codec_dest);
	ab8500_codec_error =
	    ab8500_codec_SetDirection(AB8500_CODEC_DIRECTION_OUT);
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_PowerDown                                             */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Shuts the audio codec down completely.                                   */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN:                                                                      */
/* OUT:                                                                     */
/*                                                                          */
/* RETURN:                                                                  */
/* AB8500_CODEC_TRANSACTION_FAILED: If transaction fails.                   */
/* AB8500_CODEC_OK: if successful.                                          */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Non Re-Entrant                                               */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_PowerDown(void)
{
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	g_ab8500_codec_system_context.ab8500_codec_configuration.cr0_powerup =
	    AB8500_CODEC_CR0_POWERUP_OFF;
	ab8500_codec_error = ab8500_codec_UpdateCR0();
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_PowerUp                                               */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Switch on the audio codec.                                               */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN:                                                                      */
/* None                                                                     */
/* OUT:                                                                     */
/* None                                                                     */
/* RETURN:                                                                  */
/* AB8500_CODEC_TRANSACTION_FAILED: If transaction fails.                   */
/* CODEC_OK: if successful.                                                 */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Non Re-Entrant                                               */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_PowerUp(void)
{
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	DBGENTER();
	g_ab8500_codec_system_context.ab8500_codec_configuration.cr0_powerup =
	    AB8500_CODEC_CR0_POWERUP_ON;
	g_ab8500_codec_system_context.ab8500_codec_configuration.cr0_enaana =
	    AB8500_CODEC_CR0_ENAANA_ON;
	ab8500_codec_error = ab8500_codec_UpdateCR0();
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_SelectInterface                                       */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Select the Audio Interface 0 or 1.                                       */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN: t_ab8500_codec_audio_interface: The selected interface               */
/*                                                                          */
/* OUT:                                                                     */
/* None                                                                     */
/* RETURN:                                                                  */
/* AB8500_CODEC_OK: Always.                                                 */
/* REMARK: Call this API before using a function of the low level drivers   */
/* to select the interface that you want to configure                       */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Non Re-Entrant                                               */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_SelectInterface(IN
							 t_ab8500_codec_audio_interface
							 audio_interface)
{
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	DBGENTER0();
	g_ab8500_codec_system_context.audio_interface = audio_interface;
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_GetInterface                                          */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Get the Audio Interface 0 or 1.                                          */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN:                                                                      */
/* None                                                                     */
/* OUT: p_audio_interface: Store the selected interface                     */
/* RETURN:                                                                  */
/* AB8500_CODEC_OK: Always                                                  */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Re-Entrant                                                   */
/* REENTRANCY ISSUES: No Issues                                             */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_GetInterface(OUT
						      t_ab8500_codec_audio_interface
						      * p_audio_interface)
{
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	DBGENTER0();
	*p_audio_interface = g_ab8500_codec_system_context.audio_interface;
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_SetAnalogLoopback                                     */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Sets Line-In to HeadSet loopback with the required gain.                 */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN:                                                                      */
/* out_left_volume: play volume for left channel.                           */
/* out_right_volume: play volume for right channel.                         */
/* OUT:                                                                     */
/* None                                                                     */
/*                                                                          */
/* RETURN:                                                                  */
/* AB8500_CODEC_TRANSACTION_FAILED: If transaction fails.                   */
/* AB8500_CODEC_OK: if successful.                                          */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Non Re-Entrant                                               */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_SetAnalogLoopback(IN t_uint8
							   out_left_volume,
							   IN t_uint8
							   out_right_volume)
{
	t_ab8500_codec_error ab8500_codec_error;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.ab8500_codec_configuration;
	DBGENTER2(" (%lx %lx)", out_left_volume, out_right_volume);
	if (out_left_volume > AB8500_CODEC_MAX_VOLUME)
		 {
		out_left_volume = AB8500_CODEC_MAX_VOLUME;
		}
	if (out_right_volume > AB8500_CODEC_MAX_VOLUME)
		 {
		out_right_volume = AB8500_CODEC_MAX_VOLUME;
		}
	g_ab8500_codec_system_context.out_left_volume = out_left_volume;
	g_ab8500_codec_system_context.out_right_volume = out_right_volume;
	p_ab8500_codec_configuration->cr24_lintohsl_gain =
	    AB8500_CODEC_LINEIN_TO_HS_L_R_VOLUME_MIN +
	    (out_left_volume *
	     (AB8500_CODEC_LINEIN_TO_HS_L_R_VOLUME_MAX -
	      AB8500_CODEC_LINEIN_TO_HS_L_R_VOLUME_MIN)) / 100;
	p_ab8500_codec_configuration->cr25_lintohsr_gain =
	    AB8500_CODEC_LINEIN_TO_HS_L_R_VOLUME_MIN +
	    (out_right_volume *
	     (AB8500_CODEC_LINEIN_TO_HS_L_R_VOLUME_MAX -
	      AB8500_CODEC_LINEIN_TO_HS_L_R_VOLUME_MIN)) / 100;
	ab8500_codec_error = ab8500_codec_UpdateCR24();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR25();
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_RemoveAnalogLoopback                                  */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Remove Line-In to HeadSet loopback.                                      */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN:                                                                      */
/* None                                                                     */
/* OUT:                                                                     */
/* None                                                                     */
/*                                                                          */
/* RETURN:                                                                  */
/* AB8500_CODEC_TRANSACTION_FAILED: If transaction fails.                   */
/* AB8500_CODEC_OK: if successful.                                          */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Non Re-Entrant                                               */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_RemoveAnalogLoopback(void)
{
	t_ab8500_codec_error ab8500_codec_error;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.ab8500_codec_configuration;
	DBGENTER0();
	p_ab8500_codec_configuration->cr24_lintohsl_gain =
	    AB8500_CODEC_LINEIN_TO_HS_L_R_LOOP_OPEN;
	p_ab8500_codec_configuration->cr25_lintohsr_gain =
	    AB8500_CODEC_LINEIN_TO_HS_L_R_LOOP_OPEN;
	ab8500_codec_error = ab8500_codec_UpdateCR24();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR25();
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_EnableBypassMode                                      */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Enables IF0 to IF1 path or vice versa                                    */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN:                                                                      */
/* None                                                                     */
/* OUT:                                                                     */
/* None                                                                     */
/*                                                                          */
/* RETURN:                                                                  */
/* AB8500_CODEC_TRANSACTION_FAILED: If transaction fails.                   */
/* AB8500_CODEC_OK: if successful.                                          */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Non Re-Entrant                                               */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_EnableBypassMode(void)
{
	t_ab8500_codec_error ab8500_codec_error;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.ab8500_codec_configuration;
	DBGENTER0();
	if (AB8500_CODEC_AUDIO_INTERFACE_1 ==
	       g_ab8500_codec_system_context.audio_interface)
		 {
		p_ab8500_codec_configuration->cr29_if1datoif0ad =
		    AB8500_CODEC_CR29_IF1DATOIF0AD_SENT;
		p_ab8500_codec_configuration->cr29_if1cktoif0ck =
		    AB8500_CODEC_CR29_IF1CKTOIF0CK_SENT;
		}

	else
		 {
		p_ab8500_codec_configuration->cr29_if0datoif1ad =
		    AB8500_CODEC_CR29_IF0DATOIF1AD_SENT;
		p_ab8500_codec_configuration->cr29_if0cktoif1ck =
		    AB8500_CODEC_CR29_IF0CKTOIF1CK_SENT;
		}
	ab8500_codec_error = ab8500_codec_UpdateCR29();
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_DisableBypassMode                                     */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Disables IF0 to IF1 path or vice versa                                   */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN:                                                                      */
/* None                                                                     */
/* OUT:                                                                     */
/* None                                                                     */
/*                                                                          */
/* RETURN:                                                                  */
/* AB8500_CODEC_TRANSACTION_FAILED: If transaction fails.                   */
/* AB8500_CODEC_OK: if successful.                                          */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Non Re-Entrant                                               */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_DisableBypassMode(void)
{
	t_ab8500_codec_error ab8500_codec_error;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.ab8500_codec_configuration;
	DBGENTER0();
	if (AB8500_CODEC_AUDIO_INTERFACE_1 ==
	       g_ab8500_codec_system_context.audio_interface)
		 {
		p_ab8500_codec_configuration->cr29_if1datoif0ad =
		    AB8500_CODEC_CR29_IF1DATOIF0AD_NOTSENT;
		p_ab8500_codec_configuration->cr29_if1cktoif0ck =
		    AB8500_CODEC_CR29_IF1CKTOIF0CK_NOTSENT;
		}

	else
		 {
		p_ab8500_codec_configuration->cr29_if0datoif1ad =
		    AB8500_CODEC_CR29_IF0DATOIF1AD_NOTSENT;
		p_ab8500_codec_configuration->cr29_if0cktoif1ck =
		    AB8500_CODEC_CR29_IF0CKTOIF1CK_NOTSENT;
		}
	ab8500_codec_error = ab8500_codec_UpdateCR29();
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_SrcPowerControl                                       */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Enables/Disables & UnMute/Mute the desired source                        */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN:                                                                      */
/* t_ab8500_codec_src: select source device for enabling/disabling.         */
/* t_ab8500_codec_src_state: Enable/Disable                                 */
/* OUT:                                                                     */
/* None                                                                     */
/*                                                                          */
/* RETURN:                                                                  */
/* AB8500_CODEC_TRANSACTION_FAILED: If transaction fails.                   */
/* AB8500_CODEC_OK: if successful.                                          */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Non Re-Entrant                                               */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_SrcPowerControl(IN
							 t_ab8500_codec_src
							 src_device,
							 t_ab8500_codec_src_state
							 state)
{
	t_ab8500_codec_error ab8500_codec_error;
	DBGENTER2(" (%lx %lx)", src_device, state);
	if (src_device <= AB8500_CODEC_SRC_D_MICROPHONE_2)
		 {
		ab8500_codec_error =
		    ab8500_codec_SrcPowerControlSwitch1(src_device, state);
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		}

	else if (src_device <= AB8500_CODEC_SRC_ALL)
		 {
		ab8500_codec_error =
		    ab8500_codec_SrcPowerControlSwitch2(src_device, state);
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		}

	else
		 {
		ab8500_codec_error = AB8500_CODEC_INVALID_PARAMETER;
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR5();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR6();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR7();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR63();
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_DestPowerControl                                      */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Enables/Disables & UnMute/Mute the desired destination                   */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN:                                                                      */
/* t_ab8500_codec_dest: select destination device for enabling/disabling.   */
/* t_ab8500_codec_dest_state: Enable/Disable                                */
/* OUT:                                                                     */
/* None                                                                     */
/*                                                                          */
/* RETURN:                                                                  */
/* AB8500_CODEC_TRANSACTION_FAILED: If transaction fails.                   */
/* AB8500_CODEC_OK: if successful.                                          */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Non Re-Entrant                                               */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_DestPowerControl(IN
							  t_ab8500_codec_dest
							  dest_device,
							  t_ab8500_codec_dest_state
							  state)
{
	t_ab8500_codec_error ab8500_codec_error;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.ab8500_codec_configuration;
	DBGENTER2(" (%lx %lx)", dest_device, state);
	switch (dest_device)
		 {
	case AB8500_CODEC_DEST_HEADSET:
		if (AB8500_CODEC_DEST_STATE_ENABLE == state)
			 {
			p_ab8500_codec_configuration->cr8_enhsl =
			    AB8500_CODEC_CR8_ENHSL_ENABLED;
			p_ab8500_codec_configuration->cr8_enhsr =
			    AB8500_CODEC_CR8_ENHSR_ENABLED;
			p_ab8500_codec_configuration->cr10_mutehsl =
			    AB8500_CODEC_CR10_MUTEHSL_DISABLED;
			p_ab8500_codec_configuration->cr10_mutehsr =
			    AB8500_CODEC_CR10_MUTEHSR_DISABLED;
			p_ab8500_codec_configuration->cr9_endachsl =
			    AB8500_CODEC_CR9_ENDACHSL_ENABLED;
			p_ab8500_codec_configuration->cr9_endachsr =
			    AB8500_CODEC_CR9_ENDACHSR_ENABLED;
			}

		else
			 {
			p_ab8500_codec_configuration->cr8_enhsl =
			    AB8500_CODEC_CR8_ENHSL_DISABLED;
			p_ab8500_codec_configuration->cr8_enhsr =
			    AB8500_CODEC_CR8_ENHSR_DISABLED;
			p_ab8500_codec_configuration->cr10_mutehsl =
			    AB8500_CODEC_CR10_MUTEHSL_ENABLED;
			p_ab8500_codec_configuration->cr10_mutehsr =
			    AB8500_CODEC_CR10_MUTEHSR_ENABLED;
			p_ab8500_codec_configuration->cr9_endachsl =
			    AB8500_CODEC_CR9_ENDACHSL_DISABLED;
			p_ab8500_codec_configuration->cr9_endachsr =
			    AB8500_CODEC_CR9_ENDACHSR_DISABLED;
			}
		break;
	case AB8500_CODEC_DEST_EARPIECE:
		if (AB8500_CODEC_DEST_STATE_ENABLE == state)
			 {
			p_ab8500_codec_configuration->cr8_enear =
			    AB8500_CODEC_CR8_ENEAR_ENABLED;
			p_ab8500_codec_configuration->cr9_endacear =
			    AB8500_CODEC_CR9_ENDACEAR_ENABLED;
			p_ab8500_codec_configuration->cr10_muteear =
			    AB8500_CODEC_CR10_MUTEEAR_DISABLED;
			}

		else
			 {
			p_ab8500_codec_configuration->cr8_enear =
			    AB8500_CODEC_CR8_ENEAR_DISABLED;
			p_ab8500_codec_configuration->cr9_endacear =
			    AB8500_CODEC_CR9_ENDACEAR_DISABLED;
			p_ab8500_codec_configuration->cr10_muteear =
			    AB8500_CODEC_CR10_MUTEEAR_ENABLED;
			}
		break;
	case AB8500_CODEC_DEST_HANDSFREE:
		if (AB8500_CODEC_DEST_STATE_ENABLE == state)
			 {
			p_ab8500_codec_configuration->cr8_enhfl =
			    AB8500_CODEC_CR8_ENHFL_ENABLED;
			p_ab8500_codec_configuration->cr8_enhfr =
			    AB8500_CODEC_CR8_ENHFR_ENABLED;
			p_ab8500_codec_configuration->cr9_endachfl =
			    AB8500_CODEC_CR9_ENDACHFL_ENABLED;
			p_ab8500_codec_configuration->cr9_endachfr =
			    AB8500_CODEC_CR9_ENDACHFR_ENABLED;
			}

		else
			 {
			p_ab8500_codec_configuration->cr8_enhfl =
			    AB8500_CODEC_CR8_ENHFL_DISABLED;
			p_ab8500_codec_configuration->cr8_enhfr =
			    AB8500_CODEC_CR8_ENHFR_DISABLED;
			p_ab8500_codec_configuration->cr9_endachfl =
			    AB8500_CODEC_CR9_ENDACHFL_DISABLED;
			p_ab8500_codec_configuration->cr9_endachfr =
			    AB8500_CODEC_CR9_ENDACHFR_DISABLED;
			}
		break;
	case AB8500_CODEC_DEST_VIBRATOR_L:
		if (AB8500_CODEC_DEST_STATE_ENABLE == state)
			 {
			p_ab8500_codec_configuration->cr8_envibl =
			    AB8500_CODEC_CR8_ENVIBL_ENABLED;
			p_ab8500_codec_configuration->cr9_endacvibl =
			    AB8500_CODEC_CR9_ENDACVIBL_ENABLED;
			p_ab8500_codec_configuration->cr15_pwmtovibl =
			    AB8500_CODEC_CR15_PWMTOVIBL_PWM;
			p_ab8500_codec_configuration->cr15_pwmlctrl =
			    AB8500_CODEC_CR15_PWMLCTRL_PWMNPLDUTYCYCLE;
			p_ab8500_codec_configuration->cr15_pwmnlctrl =
			    AB8500_CODEC_CR15_PWMNLCTRL_PWMNLDUTYCYCLE;
			p_ab8500_codec_configuration->cr15_pwmplctrl =
			    AB8500_CODEC_CR15_PWMPLCTRL_PWMPLDUTYCYCLE;
			}

		else
			 {
			p_ab8500_codec_configuration->cr8_envibl =
			    AB8500_CODEC_CR8_ENVIBL_DISABLED;
			p_ab8500_codec_configuration->cr9_endacvibl =
			    AB8500_CODEC_CR9_ENDACVIBL_DISABLED;
			p_ab8500_codec_configuration->cr15_pwmtovibl =
			    AB8500_CODEC_CR15_PWMTOVIBL_DA_PATH;
			p_ab8500_codec_configuration->cr15_pwmlctrl =
			    AB8500_CODEC_CR15_PWMLCTRL_PWMNPLGPOL;
			p_ab8500_codec_configuration->cr15_pwmnlctrl =
			    AB8500_CODEC_CR15_PWMNLCTRL_PWMNLGPOL;
			p_ab8500_codec_configuration->cr15_pwmplctrl =
			    AB8500_CODEC_CR15_PWMPLCTRL_PWMPLGPOL;
			}
		break;
	case AB8500_CODEC_DEST_VIBRATOR_R:
		if (AB8500_CODEC_DEST_STATE_ENABLE == state)
			 {
			p_ab8500_codec_configuration->cr8_envibr =
			    AB8500_CODEC_CR8_ENVIBR_ENABLED;
			p_ab8500_codec_configuration->cr9_endacvibr =
			    AB8500_CODEC_CR9_ENDACVIBR_ENABLED;
			p_ab8500_codec_configuration->cr15_pwmtovibr =
			    AB8500_CODEC_CR15_PWMTOVIBR_PWM;
			p_ab8500_codec_configuration->cr15_pwmrctrl =
			    AB8500_CODEC_CR15_PWMRCTRL_PWMNPRDUTYCYCLE;
			p_ab8500_codec_configuration->cr15_pwmnrctrl =
			    AB8500_CODEC_CR15_PWMNRCTRL_PWMNRDUTYCYCLE;
			p_ab8500_codec_configuration->cr15_pwmprctrl =
			    AB8500_CODEC_CR15_PWMPRCTRL_PWMPRDUTYCYCLE;
			}

		else
			 {
			p_ab8500_codec_configuration->cr8_envibr =
			    AB8500_CODEC_CR8_ENVIBR_DISABLED;
			p_ab8500_codec_configuration->cr9_endacvibr =
			    AB8500_CODEC_CR9_ENDACVIBR_DISABLED;
			p_ab8500_codec_configuration->cr15_pwmtovibr =
			    AB8500_CODEC_CR15_PWMTOVIBR_DA_PATH;
			p_ab8500_codec_configuration->cr15_pwmrctrl =
			    AB8500_CODEC_CR15_PWMRCTRL_PWMNPRGPOL;
			p_ab8500_codec_configuration->cr15_pwmnrctrl =
			    AB8500_CODEC_CR15_PWMNRCTRL_PWMNRGPOL;
			p_ab8500_codec_configuration->cr15_pwmprctrl =
			    AB8500_CODEC_CR15_PWMPRCTRL_PWMPRGPOL;
			}
		break;
	case AB8500_CODEC_DEST_ALL:
		if (AB8500_CODEC_DEST_STATE_ENABLE == state)
			 {
			p_ab8500_codec_configuration->cr8_enhsl =
			    AB8500_CODEC_CR8_ENHSL_ENABLED;
			p_ab8500_codec_configuration->cr8_enhsr =
			    AB8500_CODEC_CR8_ENHSR_ENABLED;
			p_ab8500_codec_configuration->cr8_enear =
			    AB8500_CODEC_CR8_ENEAR_ENABLED;
			p_ab8500_codec_configuration->cr8_enhfl =
			    AB8500_CODEC_CR8_ENHFL_ENABLED;
			p_ab8500_codec_configuration->cr8_enhfr =
			    AB8500_CODEC_CR8_ENHFR_ENABLED;
			p_ab8500_codec_configuration->cr8_envibl =
			    AB8500_CODEC_CR8_ENVIBL_ENABLED;
			p_ab8500_codec_configuration->cr8_envibr =
			    AB8500_CODEC_CR8_ENVIBR_ENABLED;
			p_ab8500_codec_configuration->cr9_endacear =
			    AB8500_CODEC_CR9_ENDACEAR_ENABLED;
			p_ab8500_codec_configuration->cr9_endachfl =
			    AB8500_CODEC_CR9_ENDACHFL_ENABLED;
			p_ab8500_codec_configuration->cr9_endachfr =
			    AB8500_CODEC_CR9_ENDACHFR_ENABLED;
			p_ab8500_codec_configuration->cr9_endacvibl =
			    AB8500_CODEC_CR9_ENDACVIBL_ENABLED;
			p_ab8500_codec_configuration->cr9_endacvibr =
			    AB8500_CODEC_CR9_ENDACVIBR_ENABLED;
			p_ab8500_codec_configuration->cr10_mutehsl =
			    AB8500_CODEC_CR10_MUTEHSL_DISABLED;
			p_ab8500_codec_configuration->cr10_mutehsr =
			    AB8500_CODEC_CR10_MUTEHSR_DISABLED;
			p_ab8500_codec_configuration->cr10_muteear =
			    AB8500_CODEC_CR10_MUTEEAR_DISABLED;
			p_ab8500_codec_configuration->cr15_pwmtovibl =
			    AB8500_CODEC_CR15_PWMTOVIBL_PWM;
			p_ab8500_codec_configuration->cr15_pwmlctrl =
			    AB8500_CODEC_CR15_PWMLCTRL_PWMNPLDUTYCYCLE;
			p_ab8500_codec_configuration->cr15_pwmnlctrl =
			    AB8500_CODEC_CR15_PWMNLCTRL_PWMNLDUTYCYCLE;
			p_ab8500_codec_configuration->cr15_pwmplctrl =
			    AB8500_CODEC_CR15_PWMPLCTRL_PWMPLDUTYCYCLE;
			p_ab8500_codec_configuration->cr15_pwmtovibr =
			    AB8500_CODEC_CR15_PWMTOVIBR_PWM;
			p_ab8500_codec_configuration->cr15_pwmrctrl =
			    AB8500_CODEC_CR15_PWMRCTRL_PWMNPRDUTYCYCLE;
			p_ab8500_codec_configuration->cr15_pwmnrctrl =
			    AB8500_CODEC_CR15_PWMNRCTRL_PWMNRDUTYCYCLE;
			p_ab8500_codec_configuration->cr15_pwmprctrl =
			    AB8500_CODEC_CR15_PWMPRCTRL_PWMPRDUTYCYCLE;
			}

		else
			 {
			p_ab8500_codec_configuration->cr8_enhsl =
			    AB8500_CODEC_CR8_ENHSL_DISABLED;
			p_ab8500_codec_configuration->cr8_enhsr =
			    AB8500_CODEC_CR8_ENHSR_DISABLED;
			p_ab8500_codec_configuration->cr8_enear =
			    AB8500_CODEC_CR8_ENEAR_DISABLED;
			p_ab8500_codec_configuration->cr8_enhfl =
			    AB8500_CODEC_CR8_ENHFL_DISABLED;
			p_ab8500_codec_configuration->cr8_enhfr =
			    AB8500_CODEC_CR8_ENHFR_DISABLED;
			p_ab8500_codec_configuration->cr8_envibl =
			    AB8500_CODEC_CR8_ENVIBL_DISABLED;
			p_ab8500_codec_configuration->cr8_envibr =
			    AB8500_CODEC_CR8_ENVIBR_DISABLED;
			p_ab8500_codec_configuration->cr9_endacear =
			    AB8500_CODEC_CR9_ENDACEAR_DISABLED;
			p_ab8500_codec_configuration->cr9_endachfl =
			    AB8500_CODEC_CR9_ENDACHFL_DISABLED;
			p_ab8500_codec_configuration->cr9_endachfr =
			    AB8500_CODEC_CR9_ENDACHFR_DISABLED;
			p_ab8500_codec_configuration->cr9_endacvibl =
			    AB8500_CODEC_CR9_ENDACVIBL_DISABLED;
			p_ab8500_codec_configuration->cr9_endacvibr =
			    AB8500_CODEC_CR9_ENDACVIBR_DISABLED;
			p_ab8500_codec_configuration->cr10_mutehsl =
			    AB8500_CODEC_CR10_MUTEHSL_ENABLED;
			p_ab8500_codec_configuration->cr10_mutehsr =
			    AB8500_CODEC_CR10_MUTEHSR_ENABLED;
			p_ab8500_codec_configuration->cr10_muteear =
			    AB8500_CODEC_CR10_MUTEEAR_ENABLED;
			p_ab8500_codec_configuration->cr15_pwmtovibl =
			    AB8500_CODEC_CR15_PWMTOVIBL_DA_PATH;
			p_ab8500_codec_configuration->cr15_pwmlctrl =
			    AB8500_CODEC_CR15_PWMLCTRL_PWMNPLGPOL;
			p_ab8500_codec_configuration->cr15_pwmnlctrl =
			    AB8500_CODEC_CR15_PWMNLCTRL_PWMNLGPOL;
			p_ab8500_codec_configuration->cr15_pwmplctrl =
			    AB8500_CODEC_CR15_PWMPLCTRL_PWMPLGPOL;
			p_ab8500_codec_configuration->cr15_pwmtovibr =
			    AB8500_CODEC_CR15_PWMTOVIBR_DA_PATH;
			p_ab8500_codec_configuration->cr15_pwmrctrl =
			    AB8500_CODEC_CR15_PWMRCTRL_PWMNPRGPOL;
			p_ab8500_codec_configuration->cr15_pwmnrctrl =
			    AB8500_CODEC_CR15_PWMNRCTRL_PWMNRGPOL;
			p_ab8500_codec_configuration->cr15_pwmprctrl =
			    AB8500_CODEC_CR15_PWMPRCTRL_PWMPRGPOL;
			}
		break;
	default:
		ab8500_codec_error = AB8500_CODEC_INVALID_PARAMETER;
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_DestPowerControlUpdateCR();
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_GetVersion                                            */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* This routine populates the pVersion structure with                       */
/* the current version of HCL.                                              */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN:                                                                      */
/* None                                                                     */
/* OUT:                                                                     */
/* p_version: this parameter is used to return current HCL version.         */
/*                                                                          */
/* RETURN:                                                                  */
/* AB8500_CODEC_ERROR: if p_version is NULL.                                */
/* AB8500_CODEC_OK: if successful                                           */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Re-Entrant                                                   */
/* REENTRANCY ISSUES: No Issues                                             */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_GetVersion(OUT t_version * p_version)
{
	DBGENTER1(" (%lx)", p_version);
	if (p_version != NULL)
		 {
		p_version->minor = AB8500_CODEC_HCL_MINOR_ID;
		p_version->major = AB8500_CODEC_HCL_MAJOR_ID;
		p_version->version = AB8500_CODEC_HCL_VERSION_ID;
		DBGEXIT0(AB8500_CODEC_OK);
		return (AB8500_CODEC_OK);
		}

	else
		 {
		DBGEXIT0(AB8500_CODEC_INVALID_PARAMETER);
		return (AB8500_CODEC_INVALID_PARAMETER);
		}
}

/****************************************************************************/
/* NAME: AB8500_CODEC_SetDbgLevel                                           */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Set the debug level used by the debug module (mask-like value).          */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN:                                                                      */
/* debug_level: debug level to be set                                       */
/* OUT:                                                                     */
/* None                                                                     */
/*                                                                          */
/* RETURN:                                                                  */
/* AB8500_CODEC_OK: always                                                  */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Re-Entrant                                                   */
/* REENTRANCY ISSUES: No Issues                                             */

/****************************************************************************/
/*
PUBLIC t_ab8500_codec_error AB8500_CODEC_SetDbgLevel(IN t_dbg_level dbg_level)
{
    DBGENTER1(" (%d)", dbg_level);
    dbg_level = dbg_level;
#ifdef __DEBUG
    MY_DEBUG_LEVEL_VAR_NAME = dbg_level;
#endif
    DBGEXIT(AB8500_CODEC_OK);
    return(AB8500_CODEC_OK);
}
 */

/****************************************************************************/
/* NAME: AB8500_CODEC_GetDbgLevel                                           */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Set the debug level used by the debug module (mask-like value).          */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN:                                                                      */
/* None                                                                     */
/* OUT:                                                                     */
/* p_dbg_level: this parameter is used to return debug level.               */
/*                                                                          */
/* RETURN:                                                                  */
/* AB8500_CODEC_ERROR: if p_version is NULL.                                */
/* AB8500_CODEC_OK: if successful                                           */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Re-Entrant                                                   */
/* REENTRANCY ISSUES: No Issues                                             */

/****************************************************************************/
/*
PUBLIC t_ab8500_codec_error AB8500_CODEC_GetDbgLevel(OUT t_dbg_level *p_dbg_level)
{
    if (NULL == p_dbg_level)
    {
        DBGEXIT(AB8500_CODEC_INVALID_PARAMETER);
        return(AB8500_CODEC_INVALID_PARAMETER);
    }

#ifdef __DEBUG
    * p_dbg_level = MY_DEBUG_LEVEL_VAR_NAME;
#endif
    DBGEXIT(AB8500_CODEC_OK);
    return(AB8500_CODEC_OK);
}
*/
/****************************************************************************/
/* NAME: AB8500_CODEC_ADSlotAllocation                                      */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* AD Data Allocation in slots.                                             */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN: t_ab8500_codec_slot: The slot to be allocated.                       */
/* IN: t_ab8500_codec_cr31_to_cr46_ad_data_allocation: The value            */
/*                                                     to be allocated.     */
/* OUT:                                                                     */
/* None                                                                     */
/* RETURN:                                                                  */
/* AB8500_CODEC_INVALID_PARAMETER: If invalid slot number                   */
/* AB8500_CODEC_OK: if successful.                                          */
/* REMARK:                                                                  */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Non Re-Entrant                                               */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_ADSlotAllocation
    (IN t_ab8500_codec_slot ad_slot,
     IN t_ab8500_codec_cr31_to_cr46_ad_data_allocation value )  {
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	DBGENTER2(" (%lx %lx)", ad_slot, value);
	if (ad_slot <= AB8500_CODEC_SLOT7)
		 {
		ab8500_codec_error =
		    ab8500_codec_ADSlotAllocationSwitch1(ad_slot, value);
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		}

	else if (ad_slot <= AB8500_CODEC_SLOT15)
		 {
		ab8500_codec_error =
		    ab8500_codec_ADSlotAllocationSwitch2(ad_slot, value);
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		}

	else if (ad_slot <= AB8500_CODEC_SLOT23)
		 {
		ab8500_codec_error =
		    ab8500_codec_ADSlotAllocationSwitch3(ad_slot, value);
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		}

	else if (ad_slot <= AB8500_CODEC_SLOT31)
		 {
		ab8500_codec_error =
		    ab8500_codec_ADSlotAllocationSwitch4(ad_slot, value);
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		}

	else
		 {
		ab8500_codec_error = AB8500_CODEC_INVALID_PARAMETER;
		}
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_DASlotAllocation                                      */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Allocate the Audio Interface slot for DA paths.                          */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN: t_ab8500_codec_da_channel_number: Channel number 1/2/3/4/5/6         */
/* IN: t_ab8500_codec_cr51_to_cr56_sltoda: Slot number                      */
/*                                                                          */
/* OUT:                                                                     */
/* None                                                                     */
/* RETURN:                                                                  */
/* AB8500_CODEC_INVALID_PARAMETER: If invalid channel number                */
/* AB8500_CODEC_OK: if successful.                                          */
/* REMARK:                                                                  */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Non Re-Entrant                                               */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_DASlotAllocation
    (IN t_ab8500_codec_da_channel_number channel_number,
     IN t_ab8500_codec_cr51_to_cr58_sltoda slot )  {
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	t_ab8500_codec_cr0_powerup ab8500_codec_cr0_powerup;
	DBGENTER2(" (%lx %lx)", channel_number, slot);
	p_ab8500_codec_configuration->cr51_da12_voice =
	    AB8500_CODEC_CR51_DA12_VOICE_LOWLATENCYFILTER;
	switch (channel_number)
		 {
	case AB8500_CODEC_DA_CHANNEL_NUMBER_1:
		p_ab8500_codec_configuration->cr51_sltoda1 = slot;
		break;
	case AB8500_CODEC_DA_CHANNEL_NUMBER_2:
		p_ab8500_codec_configuration->cr52_sltoda2 = slot;
		break;
	case AB8500_CODEC_DA_CHANNEL_NUMBER_3:
		p_ab8500_codec_configuration->cr53_sltoda3 = slot;
		break;
	case AB8500_CODEC_DA_CHANNEL_NUMBER_4:
		p_ab8500_codec_configuration->cr54_sltoda4 = slot;
		break;
	case AB8500_CODEC_DA_CHANNEL_NUMBER_5:
		p_ab8500_codec_configuration->cr55_sltoda5 = slot;
		break;
	case AB8500_CODEC_DA_CHANNEL_NUMBER_6:
		p_ab8500_codec_configuration->cr56_sltoda6 = slot;
		break;
	case AB8500_CODEC_DA_CHANNEL_NUMBER_7:
		p_ab8500_codec_configuration->cr57_sltoda7 = slot;
		break;
	case AB8500_CODEC_DA_CHANNEL_NUMBER_8:
		p_ab8500_codec_configuration->cr58_sltoda8 = slot;
		break;
	default:
		ab8500_codec_error = AB8500_CODEC_INVALID_PARAMETER;
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_cr0_powerup = p_ab8500_codec_configuration->cr0_powerup;
	p_ab8500_codec_configuration->cr0_powerup =
	    AB8500_CODEC_CR0_POWERUP_OFF;
	ab8500_codec_error = ab8500_codec_UpdateCR0();
	if (AB8500_CODEC_OK != ab8500_codec_error)
		 {
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR51();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR52();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR53();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR54();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR55();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR56();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR57();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR58();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	p_ab8500_codec_configuration->cr0_powerup = ab8500_codec_cr0_powerup;
	ab8500_codec_error = ab8500_codec_UpdateCR0();
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_ConfigureBurstFifo                                    */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Configuration for Burst FIFO control                                     */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN: t_ab8500_codec_burst_fifo_config: structure for configuration of     */
/*										 burst FIFO                         */
/* OUT:                                                                     */
/* None                                                                     */
/* RETURN:                                                                  */
/* AB8500_CODEC_INVALID_PARAMETER: If invalid parameter                     */
/* AB8500_CODEC_UNSUPPORTED_FEATURE: If interface 1 selected                */
/* AB8500_CODEC_OK: if successful.                                          */
/* REMARK:                                                                  */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Non Re-Entrant                                               */

/****************************************************************************/
t_ab8500_codec_error AB8500_CODEC_ConfigureBurstFifo(IN
						     t_ab8500_codec_burst_fifo_config
						     const *const
						     p_burst_fifo_config)
{
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	DBGENTER1(" (%lx)", p_burst_fifo_config);
	if (AB8500_CODEC_AUDIO_INTERFACE_0 ==
	      g_ab8500_codec_system_context.audio_interface)
		 {
		if (AB8500_CODEC_CR27_EN_MASTGEN_ENABLED ==
		     p_ab8500_codec_configuration->cr27_en_mastgen)
			 {
			p_ab8500_codec_configuration->cr105_bfifomsk =
			    p_burst_fifo_config->cr105_bfifomsk;
			p_ab8500_codec_configuration->cr105_bfifoint =
			    p_burst_fifo_config->cr105_bfifoint;
			p_ab8500_codec_configuration->cr106_bfifotx =
			    p_burst_fifo_config->cr106_bfifotx;
			p_ab8500_codec_configuration->cr107_bfifoexsl =
			    p_burst_fifo_config->cr107_bfifoexsl;
			p_ab8500_codec_configuration->cr107_bfifomast =
			    p_burst_fifo_config->cr107_bfifomast;
			p_ab8500_codec_configuration->cr107_bfiforun =
			    p_burst_fifo_config->cr107_bfiforun;
			p_ab8500_codec_configuration->cr108_bfifoframsw =
			    p_burst_fifo_config->cr108_bfifoframsw;
			p_ab8500_codec_configuration->cr109_bfifowakeup =
			    p_burst_fifo_config->cr109_bfifowakeup;
			ab8500_codec_error = ab8500_codec_UpdateCR105();
			if (ab8500_codec_error != AB8500_CODEC_OK)
				 {
				DBGEXIT(ab8500_codec_error);
				return (ab8500_codec_error);
				}
			ab8500_codec_error = ab8500_codec_UpdateCR106();
			if (ab8500_codec_error != AB8500_CODEC_OK)
				 {
				DBGEXIT(ab8500_codec_error);
				return (ab8500_codec_error);
				}
			ab8500_codec_error = ab8500_codec_UpdateCR107();
			if (ab8500_codec_error != AB8500_CODEC_OK)
				 {
				DBGEXIT(ab8500_codec_error);
				return (ab8500_codec_error);
				}
			ab8500_codec_error = ab8500_codec_UpdateCR108();
			if (ab8500_codec_error != AB8500_CODEC_OK)
				 {
				DBGEXIT(ab8500_codec_error);
				return (ab8500_codec_error);
				}
			ab8500_codec_error = ab8500_codec_UpdateCR109();
			if (ab8500_codec_error != AB8500_CODEC_OK)
				 {
				DBGEXIT(ab8500_codec_error);
				return (ab8500_codec_error);
				}
			}

		else
			 {
			ab8500_codec_error = AB8500_CODEC_INVALID_PARAMETER;
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		}

	else
		 {
		ab8500_codec_error = AB8500_CODEC_UNSUPPORTED_FEATURE;
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_EnableBurstFifo                                       */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Enable the Burst FIFO for Interface 0                                    */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN:                                                                      */
/* None                                                                     */
/* OUT:                                                                     */
/* None                                                                     */
/* RETURN:                                                                  */
/* AB8500_CODEC_UNSUPPORTED_FEATURE: If Interface 1 is selected             */
/* AB8500_CODEC_OK: if successful.                                          */
/* REMARK:                                                                  */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Non Re-Entrant                                               */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_EnableBurstFifo(void)
{
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	DBGENTER0();
	if (AB8500_CODEC_AUDIO_INTERFACE_0 ==
	      g_ab8500_codec_system_context.audio_interface)
		 {
		p_ab8500_codec_configuration->cr29_if0bfifoen =
		    AB8500_CODEC_CR29_IF0BFIFOEN_BURST_MODE;
		ab8500_codec_error = ab8500_codec_UpdateCR29();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		}

	else
		 {
		ab8500_codec_error = AB8500_CODEC_UNSUPPORTED_FEATURE;
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

/****************************************************************************/
/* NAME: AB8500_CODEC_DisableBurstFifo                                      */
/*--------------------------------------------------------------------------*/
/* DESCRIPTION:                                                             */
/* Disable the Burst FIFO for Interface 0                                   */
/*                                                                          */
/* ARGUMENTS                                                                */
/* IN:                                                                      */
/* None                                                                     */
/* OUT:                                                                     */
/* None                                                                     */
/* RETURN:                                                                  */
/* AB8500_CODEC_UNSUPPORTED_FEATURE: If Interface 1 is selected             */
/* AB8500_CODEC_OK: if successful.                                          */
/* REMARK:                                                                  */
/*--------------------------------------------------------------------------*/
/* REENTRANCY: Non Re-Entrant                                               */

/****************************************************************************/
PUBLIC t_ab8500_codec_error AB8500_CODEC_DisableBurstFifo(void)
{
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	DBGENTER0();
	if (AB8500_CODEC_AUDIO_INTERFACE_0 ==
	      g_ab8500_codec_system_context.audio_interface)
		 {
		p_ab8500_codec_configuration->cr29_if0bfifoen =
		    AB8500_CODEC_CR29_IF0BFIFOEN_NORMAL_MODE;
		ab8500_codec_error = ab8500_codec_UpdateCR29();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		}

	else
		 {
		ab8500_codec_error = AB8500_CODEC_UNSUPPORTED_FEATURE;
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

PRIVATE t_ab8500_codec_error ab8500_codec_ADSlotAllocationSwitch1
    (IN t_ab8500_codec_slot ad_slot,
     IN t_ab8500_codec_cr31_to_cr46_ad_data_allocation value )  {
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	switch (ad_slot)
		 {
	case AB8500_CODEC_SLOT0:
		p_ab8500_codec_configuration->cr31_adotoslot0 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR31();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT1:
		p_ab8500_codec_configuration->cr31_adotoslot1 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR31();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT2:
		p_ab8500_codec_configuration->cr32_adotoslot2 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR32();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT3:
		p_ab8500_codec_configuration->cr32_adotoslot3 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR32();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT4:
		p_ab8500_codec_configuration->cr33_adotoslot4 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR33();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT5:
		p_ab8500_codec_configuration->cr33_adotoslot5 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR33();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT6:
		p_ab8500_codec_configuration->cr34_adotoslot6 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR34();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT7:
		p_ab8500_codec_configuration->cr34_adotoslot7 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR34();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	default:
		ab8500_codec_error = AB8500_CODEC_INVALID_PARAMETER;
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

PRIVATE t_ab8500_codec_error ab8500_codec_ADSlotAllocationSwitch2
    (IN t_ab8500_codec_slot ad_slot,
     IN t_ab8500_codec_cr31_to_cr46_ad_data_allocation value )  {
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	switch (ad_slot)
		 {
	case AB8500_CODEC_SLOT8:
		p_ab8500_codec_configuration->cr35_adotoslot8 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR35();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT9:
		p_ab8500_codec_configuration->cr35_adotoslot9 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR35();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT10:
		p_ab8500_codec_configuration->cr36_adotoslot10 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR36();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT11:
		p_ab8500_codec_configuration->cr36_adotoslot11 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR36();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT12:
		p_ab8500_codec_configuration->cr37_adotoslot12 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR37();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT13:
		p_ab8500_codec_configuration->cr37_adotoslot13 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR37();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT14:
		p_ab8500_codec_configuration->cr38_adotoslot14 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR38();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT15:
		p_ab8500_codec_configuration->cr38_adotoslot15 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR38();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	default:
		ab8500_codec_error = AB8500_CODEC_INVALID_PARAMETER;
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

PRIVATE t_ab8500_codec_error ab8500_codec_ADSlotAllocationSwitch3
    (IN t_ab8500_codec_slot ad_slot,
     IN t_ab8500_codec_cr31_to_cr46_ad_data_allocation value )  {
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	switch (ad_slot)
		 {
	case AB8500_CODEC_SLOT16:
		p_ab8500_codec_configuration->cr39_adotoslot16 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR39();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT17:
		p_ab8500_codec_configuration->cr39_adotoslot17 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR39();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT18:
		p_ab8500_codec_configuration->cr40_adotoslot18 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR40();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT19:
		p_ab8500_codec_configuration->cr40_adotoslot19 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR40();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT20:
		p_ab8500_codec_configuration->cr41_adotoslot20 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR41();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT21:
		p_ab8500_codec_configuration->cr41_adotoslot21 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR41();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT22:
		p_ab8500_codec_configuration->cr42_adotoslot22 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR42();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT23:
		p_ab8500_codec_configuration->cr42_adotoslot23 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR42();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	default:
		ab8500_codec_error = AB8500_CODEC_INVALID_PARAMETER;
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

PRIVATE t_ab8500_codec_error ab8500_codec_ADSlotAllocationSwitch4
    (IN t_ab8500_codec_slot ad_slot,
     IN t_ab8500_codec_cr31_to_cr46_ad_data_allocation value )  {
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	switch (ad_slot)
		 {
	case AB8500_CODEC_SLOT24:
		p_ab8500_codec_configuration->cr43_adotoslot24 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR43();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT25:
		p_ab8500_codec_configuration->cr43_adotoslot25 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR43();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT26:
		p_ab8500_codec_configuration->cr44_adotoslot26 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR44();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT27:
		p_ab8500_codec_configuration->cr44_adotoslot27 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR44();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT28:
		p_ab8500_codec_configuration->cr45_adotoslot28 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR45();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT29:
		p_ab8500_codec_configuration->cr45_adotoslot29 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR45();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT30:
		p_ab8500_codec_configuration->cr46_adotoslot30 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR46();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	case AB8500_CODEC_SLOT31:
		p_ab8500_codec_configuration->cr46_adotoslot31 = value;
		ab8500_codec_error = ab8500_codec_UpdateCR46();
		if (ab8500_codec_error != AB8500_CODEC_OK)
			 {
			DBGEXIT(ab8500_codec_error);
			return (ab8500_codec_error);
			}
		break;
	default:
		ab8500_codec_error = AB8500_CODEC_INVALID_PARAMETER;
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

PRIVATE t_ab8500_codec_error ab8500_codec_SrcPowerControlSwitch1(IN
								   t_ab8500_codec_src
								   src_device,
								   t_ab8500_codec_src_state
								   state)
{
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	switch (src_device)
		 {
	case AB8500_CODEC_SRC_LINEIN:
		if (AB8500_CODEC_SRC_STATE_ENABLE == state)
			 {
			p_ab8500_codec_configuration->cr5_enlinl =
			    AB8500_CODEC_CR5_ENLINL_ENABLED;
			p_ab8500_codec_configuration->cr5_enlinr =
			    AB8500_CODEC_CR5_ENLINR_ENABLED;
			p_ab8500_codec_configuration->cr5_mutlinl =
			    AB8500_CODEC_CR5_MUTLINL_DISABLED;
			p_ab8500_codec_configuration->cr5_mutlinr =
			    AB8500_CODEC_CR5_MUTLINR_DISABLED;
			p_ab8500_codec_configuration->cr7_linrsel =
			    AB8500_CODEC_CR7_LINRSEL_LINR;
			p_ab8500_codec_configuration->cr7_enadclinl =
			    AB8500_CODEC_CR7_ENADCLINL_ENABLED;
			p_ab8500_codec_configuration->cr7_enadclinr =
			    AB8500_CODEC_CR7_ENADCLINR_ENABLED;
			}

		else
			 {
			p_ab8500_codec_configuration->cr5_enlinl =
			    AB8500_CODEC_CR5_ENLINL_DISABLED;
			p_ab8500_codec_configuration->cr5_enlinr =
			    AB8500_CODEC_CR5_ENLINR_DISABLED;
			p_ab8500_codec_configuration->cr5_mutlinl =
			    AB8500_CODEC_CR5_MUTLINL_ENABLED;
			p_ab8500_codec_configuration->cr5_mutlinr =
			    AB8500_CODEC_CR5_MUTLINR_ENABLED;
			p_ab8500_codec_configuration->cr7_linrsel =
			    AB8500_CODEC_CR7_LINRSEL_MIC2;
			p_ab8500_codec_configuration->cr7_enadclinl =
			    AB8500_CODEC_CR7_ENADCLINL_DISABLED;
			p_ab8500_codec_configuration->cr7_enadclinr =
			    AB8500_CODEC_CR7_ENADCLINR_DISABLED;
			}
		break;
	case AB8500_CODEC_SRC_MICROPHONE_1A:
		if (AB8500_CODEC_SRC_STATE_ENABLE == state)
			 {
			p_ab8500_codec_configuration->cr5_enmic1 =
			    AB8500_CODEC_CR5_ENMIC1_ENABLED;
			p_ab8500_codec_configuration->cr5_mutmic1 =
			    AB8500_CODEC_CR5_MUTMIC1_DISABLED;
			p_ab8500_codec_configuration->cr7_mic1sel =
			    AB8500_CODEC_CR7_MIC1SEL_MIC1A;
			p_ab8500_codec_configuration->cr7_enadcmic =
			    AB8500_CODEC_CR7_ENADCMIC_ENABLED;
			}

		else
			 {
			p_ab8500_codec_configuration->cr5_enmic1 =
			    AB8500_CODEC_CR5_ENMIC1_DISABLED;
			p_ab8500_codec_configuration->cr5_mutmic1 =
			    AB8500_CODEC_CR5_MUTMIC1_ENABLED;
			p_ab8500_codec_configuration->cr7_enadcmic =
			    AB8500_CODEC_CR7_ENADCMIC_DISABLED;
			}
		break;
	case AB8500_CODEC_SRC_MICROPHONE_1B:
		if (AB8500_CODEC_SRC_STATE_ENABLE == state)
			 {
			p_ab8500_codec_configuration->cr5_enmic1 =
			    AB8500_CODEC_CR5_ENMIC1_ENABLED;
			p_ab8500_codec_configuration->cr5_mutmic1 =
			    AB8500_CODEC_CR5_MUTMIC1_DISABLED;
			p_ab8500_codec_configuration->cr7_mic1sel =
			    AB8500_CODEC_CR7_MIC1SEL_MIC1B;
			p_ab8500_codec_configuration->cr7_enadcmic =
			    AB8500_CODEC_CR7_ENADCMIC_ENABLED;
			}

		else
			 {
			p_ab8500_codec_configuration->cr5_enmic1 =
			    AB8500_CODEC_CR5_ENMIC1_DISABLED;
			p_ab8500_codec_configuration->cr5_mutmic1 =
			    AB8500_CODEC_CR5_MUTMIC1_ENABLED;
			p_ab8500_codec_configuration->cr7_enadcmic =
			    AB8500_CODEC_CR7_ENADCMIC_DISABLED;
			}
		break;
	case AB8500_CODEC_SRC_MICROPHONE_2:
		if (AB8500_CODEC_SRC_STATE_ENABLE == state)
			 {
			p_ab8500_codec_configuration->cr5_enmic2 =
			    AB8500_CODEC_CR5_ENMIC2_ENABLED;
			p_ab8500_codec_configuration->cr5_mutmic2 =
			    AB8500_CODEC_CR5_MUTMIC2_DISABLED;
			p_ab8500_codec_configuration->cr7_linrsel =
			    AB8500_CODEC_CR7_LINRSEL_MIC2;
			p_ab8500_codec_configuration->cr7_enadclinr =
			    AB8500_CODEC_CR7_ENADCLINR_ENABLED;
			}

		else
			 {
			p_ab8500_codec_configuration->cr5_enmic2 =
			    AB8500_CODEC_CR5_ENMIC2_DISABLED;
			p_ab8500_codec_configuration->cr5_mutmic2 =
			    AB8500_CODEC_CR5_MUTMIC2_ENABLED;
			p_ab8500_codec_configuration->cr7_linrsel =
			    AB8500_CODEC_CR7_LINRSEL_LINR;
			p_ab8500_codec_configuration->cr7_enadclinr =
			    AB8500_CODEC_CR7_ENADCLINR_DISABLED;
			}
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_1:
		if (AB8500_CODEC_SRC_STATE_ENABLE == state)
			 {
			p_ab8500_codec_configuration->cr6_endmic1 =
			    AB8500_CODEC_CR6_ENDMIC1_ENABLED;
			p_ab8500_codec_configuration->cr63_ad1sel =
			    AB8500_CODEC_CR63_AD1SEL_DMIC1_SELECTED;
			}

		else
			 {
			p_ab8500_codec_configuration->cr6_endmic1 =
			    AB8500_CODEC_CR6_ENDMIC1_DISABLED;
			p_ab8500_codec_configuration->cr63_ad1sel =
			    AB8500_CODEC_CR63_AD1SEL_LINLADL_SELECTED;
			}
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_2:
		if (AB8500_CODEC_SRC_STATE_ENABLE == state)
			 {
			p_ab8500_codec_configuration->cr6_endmic2 =
			    AB8500_CODEC_CR6_ENDMIC2_ENABLED;
			p_ab8500_codec_configuration->cr63_ad2sel =
			    AB8500_CODEC_CR63_AD2SEL_DMIC2_SELECTED;
			}

		else
			 {
			p_ab8500_codec_configuration->cr6_endmic2 =
			    AB8500_CODEC_CR6_ENDMIC2_DISABLED;
			p_ab8500_codec_configuration->cr63_ad2sel =
			    AB8500_CODEC_CR63_AD2SEL_LINRADR_SELECTED;
			}
		break;
	default:
		ab8500_codec_error = AB8500_CODEC_INVALID_PARAMETER;
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

PRIVATE t_ab8500_codec_error ab8500_codec_SrcPowerControlSwitch2(IN
								   t_ab8500_codec_src
								   src_device,
								   t_ab8500_codec_src_state
								   state)
{
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	switch (src_device)
		 {
	case AB8500_CODEC_SRC_D_MICROPHONE_3:
		if (AB8500_CODEC_SRC_STATE_ENABLE == state)
			 {
			p_ab8500_codec_configuration->cr6_endmic3 =
			    AB8500_CODEC_CR6_ENDMIC3_ENABLED;
			p_ab8500_codec_configuration->cr63_ad3sel =
			    AB8500_CODEC_CR63_AD3SEL_DMIC3_SELECTED;
			}

		else
			 {
			p_ab8500_codec_configuration->cr6_endmic3 =
			    AB8500_CODEC_CR6_ENDMIC3_DISABLED;
			p_ab8500_codec_configuration->cr63_ad3sel =
			    AB8500_CODEC_CR63_AD3SEL_ADMO_SELECTED;
			}
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_4:
		if (AB8500_CODEC_SRC_STATE_ENABLE == state)
			 {
			p_ab8500_codec_configuration->cr6_endmic4 =
			    AB8500_CODEC_CR6_ENDMIC4_ENABLED;
			}

		else
			 {
			p_ab8500_codec_configuration->cr6_endmic4 =
			    AB8500_CODEC_CR6_ENDMIC4_DISABLED;
			}
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_5:
		if (AB8500_CODEC_SRC_STATE_ENABLE == state)
			 {
			p_ab8500_codec_configuration->cr6_endmic5 =
			    AB8500_CODEC_CR6_ENDMIC5_ENABLED;
			p_ab8500_codec_configuration->cr63_ad5sel =
			    AB8500_CODEC_CR63_AD5SEL_DMIC5_SELECTED;
			}

		else
			 {
			p_ab8500_codec_configuration->cr6_endmic5 =
			    AB8500_CODEC_CR6_ENDMIC5_DISABLED;
			p_ab8500_codec_configuration->cr63_ad5sel =
			    AB8500_CODEC_CR63_AD5SEL_AMADR_SELECTED;
			}
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_6:
		if (AB8500_CODEC_SRC_STATE_ENABLE == state)
			 {
			p_ab8500_codec_configuration->cr6_endmic6 =
			    AB8500_CODEC_CR6_ENDMIC6_ENABLED;
			p_ab8500_codec_configuration->cr63_ad6sel =
			    AB8500_CODEC_CR63_AD6SEL_DMIC6_SELECTED;
			}

		else
			 {
			p_ab8500_codec_configuration->cr6_endmic6 =
			    AB8500_CODEC_CR6_ENDMIC6_DISABLED;
			p_ab8500_codec_configuration->cr63_ad6sel =
			    AB8500_CODEC_CR63_AD6SEL_ADMO_SELECTED;
			}
		break;
	case AB8500_CODEC_SRC_ALL:
		if (AB8500_CODEC_SRC_STATE_ENABLE == state)
			 {
			p_ab8500_codec_configuration->cr5_enlinl =
			    AB8500_CODEC_CR5_ENLINL_ENABLED;
			p_ab8500_codec_configuration->cr5_enlinr =
			    AB8500_CODEC_CR5_ENLINR_ENABLED;
			p_ab8500_codec_configuration->cr5_enmic1 =
			    AB8500_CODEC_CR5_ENMIC1_ENABLED;
			p_ab8500_codec_configuration->cr5_enmic2 =
			    AB8500_CODEC_CR5_ENMIC2_ENABLED;
			p_ab8500_codec_configuration->cr6_endmic1 =
			    AB8500_CODEC_CR6_ENDMIC1_ENABLED;
			p_ab8500_codec_configuration->cr6_endmic2 =
			    AB8500_CODEC_CR6_ENDMIC2_ENABLED;
			p_ab8500_codec_configuration->cr6_endmic3 =
			    AB8500_CODEC_CR6_ENDMIC3_ENABLED;
			p_ab8500_codec_configuration->cr6_endmic4 =
			    AB8500_CODEC_CR6_ENDMIC4_ENABLED;
			p_ab8500_codec_configuration->cr6_endmic5 =
			    AB8500_CODEC_CR6_ENDMIC5_ENABLED;
			p_ab8500_codec_configuration->cr6_endmic6 =
			    AB8500_CODEC_CR6_ENDMIC6_ENABLED;
			p_ab8500_codec_configuration->cr7_enadcmic =
			    AB8500_CODEC_CR7_ENADCMIC_ENABLED;
			p_ab8500_codec_configuration->cr5_mutlinl =
			    AB8500_CODEC_CR5_MUTLINL_DISABLED;
			p_ab8500_codec_configuration->cr5_mutlinr =
			    AB8500_CODEC_CR5_MUTLINR_DISABLED;
			p_ab8500_codec_configuration->cr5_mutmic1 =
			    AB8500_CODEC_CR5_MUTMIC1_DISABLED;
			p_ab8500_codec_configuration->cr5_mutmic2 =
			    AB8500_CODEC_CR5_MUTMIC2_DISABLED;
			}

		else
			 {
			p_ab8500_codec_configuration->cr5_enlinl =
			    AB8500_CODEC_CR5_ENLINL_DISABLED;
			p_ab8500_codec_configuration->cr5_enlinr =
			    AB8500_CODEC_CR5_ENLINR_DISABLED;
			p_ab8500_codec_configuration->cr5_enmic1 =
			    AB8500_CODEC_CR5_ENMIC1_DISABLED;
			p_ab8500_codec_configuration->cr5_enmic2 =
			    AB8500_CODEC_CR5_ENMIC2_DISABLED;
			p_ab8500_codec_configuration->cr6_endmic1 =
			    AB8500_CODEC_CR6_ENDMIC1_DISABLED;
			p_ab8500_codec_configuration->cr6_endmic2 =
			    AB8500_CODEC_CR6_ENDMIC2_DISABLED;
			p_ab8500_codec_configuration->cr6_endmic3 =
			    AB8500_CODEC_CR6_ENDMIC3_DISABLED;
			p_ab8500_codec_configuration->cr6_endmic4 =
			    AB8500_CODEC_CR6_ENDMIC4_DISABLED;
			p_ab8500_codec_configuration->cr6_endmic5 =
			    AB8500_CODEC_CR6_ENDMIC5_DISABLED;
			p_ab8500_codec_configuration->cr6_endmic6 =
			    AB8500_CODEC_CR6_ENDMIC6_DISABLED;
			p_ab8500_codec_configuration->cr7_enadcmic =
			    AB8500_CODEC_CR7_ENADCMIC_DISABLED;
			p_ab8500_codec_configuration->cr5_mutlinl =
			    AB8500_CODEC_CR5_MUTLINL_ENABLED;
			p_ab8500_codec_configuration->cr5_mutlinr =
			    AB8500_CODEC_CR5_MUTLINR_ENABLED;
			p_ab8500_codec_configuration->cr5_mutmic1 =
			    AB8500_CODEC_CR5_MUTMIC1_ENABLED;
			p_ab8500_codec_configuration->cr5_mutmic2 =
			    AB8500_CODEC_CR5_MUTMIC2_ENABLED;
			}
		break;
	case AB8500_CODEC_SRC_FM_RX:
		break;
	default:
		ab8500_codec_error = AB8500_CODEC_INVALID_PARAMETER;
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

PRIVATE t_ab8500_codec_error ab8500_codec_SetModeAndDirectionUpdateCR(void)
{
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	t_ab8500_codec_cr0_powerup ab8500_codec_cr0_powerup;
	ab8500_codec_cr0_powerup = p_ab8500_codec_configuration->cr0_powerup;
	p_ab8500_codec_configuration->cr0_powerup =
	    AB8500_CODEC_CR0_POWERUP_OFF;
	ab8500_codec_error = ab8500_codec_UpdateCR0();
	if (AB8500_CODEC_OK != ab8500_codec_error)
		 {
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR2();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR3();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR26();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR27();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR28();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR30();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR63();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	p_ab8500_codec_configuration->cr0_powerup = ab8500_codec_cr0_powerup;
	ab8500_codec_error = ab8500_codec_UpdateCR0();
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

PRIVATE t_ab8500_codec_error ab8500_codec_SetSrcVolumeUpdateCR(void)
{
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	ab8500_codec_error = ab8500_codec_UpdateCR20();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR21();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR23();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR65();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR66();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR67();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR68();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR69();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR70();
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

PRIVATE t_ab8500_codec_error ab8500_codec_SetDestVolumeUpdateCR(void)
{
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	ab8500_codec_error = ab8500_codec_UpdateCR16();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR17();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR18();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR19();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR22();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR71();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR72();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR73();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR74();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR75();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR76();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR79();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR80();
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

PRIVATE t_ab8500_codec_error ab8500_codec_ProgramDirectionIN(void)
{
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	switch (g_ab8500_codec_system_context.ab8500_codec_src)
		 {
	case AB8500_CODEC_SRC_LINEIN:
		p_ab8500_codec_configuration->cr5_enlinl =
		    AB8500_CODEC_CR5_ENLINL_ENABLED;
		p_ab8500_codec_configuration->cr5_enlinr =
		    AB8500_CODEC_CR5_ENLINR_ENABLED;
		p_ab8500_codec_configuration->cr6_endmic1 =
		    AB8500_CODEC_CR6_ENDMIC1_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic2 =
		    AB8500_CODEC_CR6_ENDMIC2_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic3 =
		    AB8500_CODEC_CR6_ENDMIC3_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic4 =
		    AB8500_CODEC_CR6_ENDMIC4_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic5 =
		    AB8500_CODEC_CR6_ENDMIC5_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic6 =
		    AB8500_CODEC_CR6_ENDMIC6_DISABLED;
		p_ab8500_codec_configuration->cr5_enmic1 =
		    AB8500_CODEC_CR5_ENMIC1_DISABLED;
		p_ab8500_codec_configuration->cr5_enmic2 =
		    AB8500_CODEC_CR5_ENMIC2_DISABLED;
		p_ab8500_codec_configuration->cr7_linrsel =
		    AB8500_CODEC_CR7_LINRSEL_LINR;
		p_ab8500_codec_configuration->cr7_enadclinl =
		    AB8500_CODEC_CR7_ENADCLINL_ENABLED;
		p_ab8500_codec_configuration->cr7_enadclinr =
		    AB8500_CODEC_CR7_ENADCLINR_ENABLED;
		p_ab8500_codec_configuration->cr5_mutlinl =
		    AB8500_CODEC_CR5_MUTLINL_DISABLED;
		p_ab8500_codec_configuration->cr5_mutlinr =
		    AB8500_CODEC_CR5_MUTLINR_DISABLED;
		p_ab8500_codec_configuration->cr5_mutmic1 =
		    AB8500_CODEC_CR5_MUTMIC1_ENABLED;
		p_ab8500_codec_configuration->cr5_mutmic2 =
		    AB8500_CODEC_CR5_MUTMIC2_ENABLED;
		break;
	case AB8500_CODEC_SRC_MICROPHONE_1A:
		p_ab8500_codec_configuration->cr5_enlinl =
		    AB8500_CODEC_CR5_ENLINL_DISABLED;
		p_ab8500_codec_configuration->cr5_enlinr =
		    AB8500_CODEC_CR5_ENLINR_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic1 =
		    AB8500_CODEC_CR6_ENDMIC1_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic2 =
		    AB8500_CODEC_CR6_ENDMIC2_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic3 =
		    AB8500_CODEC_CR6_ENDMIC3_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic4 =
		    AB8500_CODEC_CR6_ENDMIC4_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic5 =
		    AB8500_CODEC_CR6_ENDMIC5_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic6 =
		    AB8500_CODEC_CR6_ENDMIC6_DISABLED;
		p_ab8500_codec_configuration->cr5_enmic1 =
		    AB8500_CODEC_CR5_ENMIC1_ENABLED;
		p_ab8500_codec_configuration->cr5_enmic2 =
		    AB8500_CODEC_CR5_ENMIC2_DISABLED;
		p_ab8500_codec_configuration->cr7_mic1sel =
		    AB8500_CODEC_CR7_MIC1SEL_MIC1A;
		p_ab8500_codec_configuration->cr7_enadcmic =
		    AB8500_CODEC_CR7_ENADCMIC_ENABLED;
		p_ab8500_codec_configuration->cr7_enadclinl =
		    AB8500_CODEC_CR7_ENADCLINL_DISABLED;
		p_ab8500_codec_configuration->cr7_enadclinr =
		    AB8500_CODEC_CR7_ENADCLINR_DISABLED;
		p_ab8500_codec_configuration->cr5_mutlinl =
		    AB8500_CODEC_CR5_MUTLINL_ENABLED;
		p_ab8500_codec_configuration->cr5_mutlinr =
		    AB8500_CODEC_CR5_MUTLINR_ENABLED;
		p_ab8500_codec_configuration->cr5_mutmic1 =
		    AB8500_CODEC_CR5_MUTMIC1_DISABLED;
		p_ab8500_codec_configuration->cr5_mutmic2 =
		    AB8500_CODEC_CR5_MUTMIC2_ENABLED;
		break;
	case AB8500_CODEC_SRC_MICROPHONE_1B:
		p_ab8500_codec_configuration->cr5_enlinl =
		    AB8500_CODEC_CR5_ENLINL_DISABLED;
		p_ab8500_codec_configuration->cr5_enlinr =
		    AB8500_CODEC_CR5_ENLINR_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic1 =
		    AB8500_CODEC_CR6_ENDMIC1_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic2 =
		    AB8500_CODEC_CR6_ENDMIC2_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic3 =
		    AB8500_CODEC_CR6_ENDMIC3_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic4 =
		    AB8500_CODEC_CR6_ENDMIC4_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic5 =
		    AB8500_CODEC_CR6_ENDMIC5_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic6 =
		    AB8500_CODEC_CR6_ENDMIC6_DISABLED;
		p_ab8500_codec_configuration->cr5_enmic1 =
		    AB8500_CODEC_CR5_ENMIC1_ENABLED;
		p_ab8500_codec_configuration->cr5_enmic2 =
		    AB8500_CODEC_CR5_ENMIC2_DISABLED;
		p_ab8500_codec_configuration->cr7_mic1sel =
		    AB8500_CODEC_CR7_MIC1SEL_MIC1B;
		p_ab8500_codec_configuration->cr7_enadcmic =
		    AB8500_CODEC_CR7_ENADCMIC_ENABLED;
		p_ab8500_codec_configuration->cr7_enadclinl =
		    AB8500_CODEC_CR7_ENADCLINL_DISABLED;
		p_ab8500_codec_configuration->cr7_enadclinr =
		    AB8500_CODEC_CR7_ENADCLINR_DISABLED;
		p_ab8500_codec_configuration->cr5_mutlinl =
		    AB8500_CODEC_CR5_MUTLINL_ENABLED;
		p_ab8500_codec_configuration->cr5_mutlinr =
		    AB8500_CODEC_CR5_MUTLINR_ENABLED;
		p_ab8500_codec_configuration->cr5_mutmic1 =
		    AB8500_CODEC_CR5_MUTMIC1_DISABLED;
		p_ab8500_codec_configuration->cr5_mutmic2 =
		    AB8500_CODEC_CR5_MUTMIC2_ENABLED;
		break;
	case AB8500_CODEC_SRC_MICROPHONE_2:
		p_ab8500_codec_configuration->cr5_enlinl =
		    AB8500_CODEC_CR5_ENLINL_DISABLED;
		p_ab8500_codec_configuration->cr5_enlinr =
		    AB8500_CODEC_CR5_ENLINR_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic1 =
		    AB8500_CODEC_CR6_ENDMIC1_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic2 =
		    AB8500_CODEC_CR6_ENDMIC2_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic3 =
		    AB8500_CODEC_CR6_ENDMIC3_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic4 =
		    AB8500_CODEC_CR6_ENDMIC4_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic5 =
		    AB8500_CODEC_CR6_ENDMIC5_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic6 =
		    AB8500_CODEC_CR6_ENDMIC6_DISABLED;
		p_ab8500_codec_configuration->cr5_enmic1 =
		    AB8500_CODEC_CR5_ENMIC1_DISABLED;
		p_ab8500_codec_configuration->cr5_enmic2 =
		    AB8500_CODEC_CR5_ENMIC2_ENABLED;
		p_ab8500_codec_configuration->cr7_enadcmic =
		    AB8500_CODEC_CR7_ENADCMIC_ENABLED;
		p_ab8500_codec_configuration->cr7_linrsel =
		    AB8500_CODEC_CR7_LINRSEL_MIC2;
		p_ab8500_codec_configuration->cr7_enadclinl =
		    AB8500_CODEC_CR7_ENADCLINL_DISABLED;
		p_ab8500_codec_configuration->cr7_enadclinr =
		    AB8500_CODEC_CR7_ENADCLINR_ENABLED;
		p_ab8500_codec_configuration->cr5_mutlinl =
		    AB8500_CODEC_CR5_MUTLINL_ENABLED;
		p_ab8500_codec_configuration->cr5_mutlinr =
		    AB8500_CODEC_CR5_MUTLINR_ENABLED;
		p_ab8500_codec_configuration->cr5_mutmic1 =
		    AB8500_CODEC_CR5_MUTMIC1_ENABLED;
		p_ab8500_codec_configuration->cr5_mutmic2 =
		    AB8500_CODEC_CR5_MUTMIC2_DISABLED;
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_1:
		p_ab8500_codec_configuration->cr5_enlinl =
		    AB8500_CODEC_CR5_ENLINL_DISABLED;
		p_ab8500_codec_configuration->cr5_enlinr =
		    AB8500_CODEC_CR5_ENLINR_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic1 =
		    AB8500_CODEC_CR6_ENDMIC1_ENABLED;
		p_ab8500_codec_configuration->cr6_endmic2 =
		    AB8500_CODEC_CR6_ENDMIC2_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic3 =
		    AB8500_CODEC_CR6_ENDMIC3_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic4 =
		    AB8500_CODEC_CR6_ENDMIC4_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic5 =
		    AB8500_CODEC_CR6_ENDMIC5_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic6 =
		    AB8500_CODEC_CR6_ENDMIC6_DISABLED;
		p_ab8500_codec_configuration->cr5_enmic1 =
		    AB8500_CODEC_CR5_ENMIC1_DISABLED;
		p_ab8500_codec_configuration->cr5_enmic2 =
		    AB8500_CODEC_CR5_ENMIC2_DISABLED;
		p_ab8500_codec_configuration->cr7_enadclinl =
		    AB8500_CODEC_CR7_ENADCLINL_DISABLED;
		p_ab8500_codec_configuration->cr7_enadclinr =
		    AB8500_CODEC_CR7_ENADCLINR_DISABLED;
		p_ab8500_codec_configuration->cr63_ad1sel =
		    AB8500_CODEC_CR63_AD1SEL_DMIC1_SELECTED;
		p_ab8500_codec_configuration->cr5_mutlinl =
		    AB8500_CODEC_CR5_MUTLINL_ENABLED;
		p_ab8500_codec_configuration->cr5_mutlinr =
		    AB8500_CODEC_CR5_MUTLINR_ENABLED;
		p_ab8500_codec_configuration->cr5_mutmic1 =
		    AB8500_CODEC_CR5_MUTMIC1_ENABLED;
		p_ab8500_codec_configuration->cr5_mutmic2 =
		    AB8500_CODEC_CR5_MUTMIC2_ENABLED;
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_2:
		p_ab8500_codec_configuration->cr5_enlinl =
		    AB8500_CODEC_CR5_ENLINL_DISABLED;
		p_ab8500_codec_configuration->cr5_enlinr =
		    AB8500_CODEC_CR5_ENLINR_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic1 =
		    AB8500_CODEC_CR6_ENDMIC1_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic2 =
		    AB8500_CODEC_CR6_ENDMIC2_ENABLED;
		p_ab8500_codec_configuration->cr6_endmic3 =
		    AB8500_CODEC_CR6_ENDMIC3_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic4 =
		    AB8500_CODEC_CR6_ENDMIC4_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic5 =
		    AB8500_CODEC_CR6_ENDMIC5_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic6 =
		    AB8500_CODEC_CR6_ENDMIC6_DISABLED;
		p_ab8500_codec_configuration->cr5_enmic1 =
		    AB8500_CODEC_CR5_ENMIC1_DISABLED;
		p_ab8500_codec_configuration->cr5_enmic2 =
		    AB8500_CODEC_CR5_ENMIC2_DISABLED;
		p_ab8500_codec_configuration->cr7_enadclinl =
		    AB8500_CODEC_CR7_ENADCLINL_DISABLED;
		p_ab8500_codec_configuration->cr7_enadclinr =
		    AB8500_CODEC_CR7_ENADCLINR_DISABLED;
		p_ab8500_codec_configuration->cr63_ad2sel =
		    AB8500_CODEC_CR63_AD2SEL_DMIC2_SELECTED;
		p_ab8500_codec_configuration->cr5_mutlinl =
		    AB8500_CODEC_CR5_MUTLINL_ENABLED;
		p_ab8500_codec_configuration->cr5_mutlinr =
		    AB8500_CODEC_CR5_MUTLINR_ENABLED;
		p_ab8500_codec_configuration->cr5_mutmic1 =
		    AB8500_CODEC_CR5_MUTMIC1_ENABLED;
		p_ab8500_codec_configuration->cr5_mutmic2 =
		    AB8500_CODEC_CR5_MUTMIC2_ENABLED;
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_3:
		p_ab8500_codec_configuration->cr5_enlinl =
		    AB8500_CODEC_CR5_ENLINL_DISABLED;
		p_ab8500_codec_configuration->cr5_enlinr =
		    AB8500_CODEC_CR5_ENLINR_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic1 =
		    AB8500_CODEC_CR6_ENDMIC1_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic2 =
		    AB8500_CODEC_CR6_ENDMIC2_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic3 =
		    AB8500_CODEC_CR6_ENDMIC3_ENABLED;
		p_ab8500_codec_configuration->cr6_endmic4 =
		    AB8500_CODEC_CR6_ENDMIC4_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic5 =
		    AB8500_CODEC_CR6_ENDMIC5_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic6 =
		    AB8500_CODEC_CR6_ENDMIC6_DISABLED;
		p_ab8500_codec_configuration->cr5_enmic1 =
		    AB8500_CODEC_CR5_ENMIC1_DISABLED;
		p_ab8500_codec_configuration->cr5_enmic2 =
		    AB8500_CODEC_CR5_ENMIC2_DISABLED;
		p_ab8500_codec_configuration->cr7_enadclinl =
		    AB8500_CODEC_CR7_ENADCLINL_DISABLED;
		p_ab8500_codec_configuration->cr7_enadclinr =
		    AB8500_CODEC_CR7_ENADCLINR_DISABLED;
		p_ab8500_codec_configuration->cr63_ad3sel =
		    AB8500_CODEC_CR63_AD3SEL_DMIC3_SELECTED;
		p_ab8500_codec_configuration->cr5_mutlinl =
		    AB8500_CODEC_CR5_MUTLINL_ENABLED;
		p_ab8500_codec_configuration->cr5_mutlinr =
		    AB8500_CODEC_CR5_MUTLINR_ENABLED;
		p_ab8500_codec_configuration->cr5_mutmic1 =
		    AB8500_CODEC_CR5_MUTMIC1_ENABLED;
		p_ab8500_codec_configuration->cr5_mutmic2 =
		    AB8500_CODEC_CR5_MUTMIC2_ENABLED;
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_4:
		p_ab8500_codec_configuration->cr5_enlinl =
		    AB8500_CODEC_CR5_ENLINL_DISABLED;
		p_ab8500_codec_configuration->cr5_enlinr =
		    AB8500_CODEC_CR5_ENLINR_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic1 =
		    AB8500_CODEC_CR6_ENDMIC1_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic2 =
		    AB8500_CODEC_CR6_ENDMIC2_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic3 =
		    AB8500_CODEC_CR6_ENDMIC3_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic4 =
		    AB8500_CODEC_CR6_ENDMIC4_ENABLED;
		p_ab8500_codec_configuration->cr6_endmic5 =
		    AB8500_CODEC_CR6_ENDMIC5_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic6 =
		    AB8500_CODEC_CR6_ENDMIC6_DISABLED;
		p_ab8500_codec_configuration->cr5_enmic1 =
		    AB8500_CODEC_CR5_ENMIC1_DISABLED;
		p_ab8500_codec_configuration->cr5_enmic2 =
		    AB8500_CODEC_CR5_ENMIC2_DISABLED;
		p_ab8500_codec_configuration->cr7_enadclinl =
		    AB8500_CODEC_CR7_ENADCLINL_DISABLED;
		p_ab8500_codec_configuration->cr7_enadclinr =
		    AB8500_CODEC_CR7_ENADCLINR_DISABLED;
		p_ab8500_codec_configuration->cr5_mutlinl =
		    AB8500_CODEC_CR5_MUTLINL_ENABLED;
		p_ab8500_codec_configuration->cr5_mutlinr =
		    AB8500_CODEC_CR5_MUTLINR_ENABLED;
		p_ab8500_codec_configuration->cr5_mutmic1 =
		    AB8500_CODEC_CR5_MUTMIC1_ENABLED;
		p_ab8500_codec_configuration->cr5_mutmic2 =
		    AB8500_CODEC_CR5_MUTMIC2_ENABLED;
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_5:
		p_ab8500_codec_configuration->cr5_enlinl =
		    AB8500_CODEC_CR5_ENLINL_DISABLED;
		p_ab8500_codec_configuration->cr5_enlinr =
		    AB8500_CODEC_CR5_ENLINR_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic1 =
		    AB8500_CODEC_CR6_ENDMIC1_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic2 =
		    AB8500_CODEC_CR6_ENDMIC2_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic3 =
		    AB8500_CODEC_CR6_ENDMIC3_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic4 =
		    AB8500_CODEC_CR6_ENDMIC4_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic5 =
		    AB8500_CODEC_CR6_ENDMIC5_ENABLED;
		p_ab8500_codec_configuration->cr6_endmic6 =
		    AB8500_CODEC_CR6_ENDMIC6_DISABLED;
		p_ab8500_codec_configuration->cr5_enmic1 =
		    AB8500_CODEC_CR5_ENMIC1_DISABLED;
		p_ab8500_codec_configuration->cr5_enmic2 =
		    AB8500_CODEC_CR5_ENMIC2_DISABLED;
		p_ab8500_codec_configuration->cr7_enadclinl =
		    AB8500_CODEC_CR7_ENADCLINL_DISABLED;
		p_ab8500_codec_configuration->cr7_enadclinr =
		    AB8500_CODEC_CR7_ENADCLINR_DISABLED;
		p_ab8500_codec_configuration->cr63_ad5sel =
		    AB8500_CODEC_CR63_AD5SEL_DMIC5_SELECTED;
		p_ab8500_codec_configuration->cr5_mutlinl =
		    AB8500_CODEC_CR5_MUTLINL_ENABLED;
		p_ab8500_codec_configuration->cr5_mutlinr =
		    AB8500_CODEC_CR5_MUTLINR_ENABLED;
		p_ab8500_codec_configuration->cr5_mutmic1 =
		    AB8500_CODEC_CR5_MUTMIC1_ENABLED;
		p_ab8500_codec_configuration->cr5_mutmic2 =
		    AB8500_CODEC_CR5_MUTMIC2_ENABLED;
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_6:
		p_ab8500_codec_configuration->cr5_enlinl =
		    AB8500_CODEC_CR5_ENLINL_DISABLED;
		p_ab8500_codec_configuration->cr5_enlinr =
		    AB8500_CODEC_CR5_ENLINR_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic1 =
		    AB8500_CODEC_CR6_ENDMIC1_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic2 =
		    AB8500_CODEC_CR6_ENDMIC2_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic3 =
		    AB8500_CODEC_CR6_ENDMIC3_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic4 =
		    AB8500_CODEC_CR6_ENDMIC4_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic5 =
		    AB8500_CODEC_CR6_ENDMIC5_DISABLED;
		p_ab8500_codec_configuration->cr6_endmic6 =
		    AB8500_CODEC_CR6_ENDMIC6_ENABLED;
		p_ab8500_codec_configuration->cr5_enmic1 =
		    AB8500_CODEC_CR5_ENMIC1_DISABLED;
		p_ab8500_codec_configuration->cr5_enmic2 =
		    AB8500_CODEC_CR5_ENMIC2_DISABLED;
		p_ab8500_codec_configuration->cr7_enadclinl =
		    AB8500_CODEC_CR7_ENADCLINL_DISABLED;
		p_ab8500_codec_configuration->cr7_enadclinr =
		    AB8500_CODEC_CR7_ENADCLINR_DISABLED;
		p_ab8500_codec_configuration->cr63_ad6sel =
		    AB8500_CODEC_CR63_AD6SEL_DMIC6_SELECTED;
		p_ab8500_codec_configuration->cr5_mutlinl =
		    AB8500_CODEC_CR5_MUTLINL_ENABLED;
		p_ab8500_codec_configuration->cr5_mutlinr =
		    AB8500_CODEC_CR5_MUTLINR_ENABLED;
		p_ab8500_codec_configuration->cr5_mutmic1 =
		    AB8500_CODEC_CR5_MUTMIC1_ENABLED;
		p_ab8500_codec_configuration->cr5_mutmic2 =
		    AB8500_CODEC_CR5_MUTMIC2_ENABLED;
		break;
	case AB8500_CODEC_SRC_ALL:
		p_ab8500_codec_configuration->cr5_enlinl =
		    AB8500_CODEC_CR5_ENLINL_ENABLED;
		p_ab8500_codec_configuration->cr5_enlinr =
		    AB8500_CODEC_CR5_ENLINR_ENABLED;
		p_ab8500_codec_configuration->cr5_enmic1 =
		    AB8500_CODEC_CR5_ENMIC1_ENABLED;
		p_ab8500_codec_configuration->cr5_enmic2 =
		    AB8500_CODEC_CR5_ENMIC2_ENABLED;
		p_ab8500_codec_configuration->cr6_endmic1 =
		    AB8500_CODEC_CR6_ENDMIC1_ENABLED;
		p_ab8500_codec_configuration->cr6_endmic2 =
		    AB8500_CODEC_CR6_ENDMIC2_ENABLED;
		p_ab8500_codec_configuration->cr6_endmic3 =
		    AB8500_CODEC_CR6_ENDMIC3_ENABLED;
		p_ab8500_codec_configuration->cr6_endmic4 =
		    AB8500_CODEC_CR6_ENDMIC4_ENABLED;
		p_ab8500_codec_configuration->cr6_endmic5 =
		    AB8500_CODEC_CR6_ENDMIC5_ENABLED;
		p_ab8500_codec_configuration->cr6_endmic6 =
		    AB8500_CODEC_CR6_ENDMIC6_ENABLED;
		p_ab8500_codec_configuration->cr7_enadcmic =
		    AB8500_CODEC_CR7_ENADCMIC_ENABLED;
		p_ab8500_codec_configuration->cr7_enadclinl =
		    AB8500_CODEC_CR7_ENADCLINL_ENABLED;
		p_ab8500_codec_configuration->cr7_enadclinr =
		    AB8500_CODEC_CR7_ENADCLINR_ENABLED;
		p_ab8500_codec_configuration->cr5_mutlinl =
		    AB8500_CODEC_CR5_MUTLINL_DISABLED;
		p_ab8500_codec_configuration->cr5_mutlinr =
		    AB8500_CODEC_CR5_MUTLINR_DISABLED;
		p_ab8500_codec_configuration->cr5_mutmic1 =
		    AB8500_CODEC_CR5_MUTMIC1_DISABLED;
		p_ab8500_codec_configuration->cr5_mutmic2 =
		    AB8500_CODEC_CR5_MUTMIC2_DISABLED;
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_12:
	case AB8500_CODEC_SRC_D_MICROPHONE_34:
	case AB8500_CODEC_SRC_D_MICROPHONE_56:
		break;
	default:
		ab8500_codec_error = AB8500_CODEC_INVALID_PARAMETER;
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

PRIVATE t_ab8500_codec_error ab8500_codec_ProgramDirectionOUT(void)
{
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	t_ab8500_codec_configuration * p_ab8500_codec_configuration =
	    &g_ab8500_codec_system_context.  ab8500_codec_configuration;
	switch (g_ab8500_codec_system_context.ab8500_codec_dest)
		 {
	case AB8500_CODEC_DEST_HEADSET:
		p_ab8500_codec_configuration->cr7_endrvhsl =
		    AB8500_CODEC_CR7_ENDRVHSL_ENABLED;
		p_ab8500_codec_configuration->cr7_endrvhsr =
		    AB8500_CODEC_CR7_ENDRVHSR_ENABLED;
		p_ab8500_codec_configuration->cr8_enear =
		    AB8500_CODEC_CR8_ENEAR_DISABLED;
		p_ab8500_codec_configuration->cr8_enhsl =
		    AB8500_CODEC_CR8_ENHSL_ENABLED;
		p_ab8500_codec_configuration->cr8_enhsr =
		    AB8500_CODEC_CR8_ENHSR_ENABLED;
		p_ab8500_codec_configuration->cr8_enhfl =
		    AB8500_CODEC_CR8_ENHFL_DISABLED;
		p_ab8500_codec_configuration->cr8_enhfr =
		    AB8500_CODEC_CR8_ENHFR_DISABLED;
		p_ab8500_codec_configuration->cr8_envibl =
		    AB8500_CODEC_CR8_ENVIBL_DISABLED;
		p_ab8500_codec_configuration->cr8_envibr =
		    AB8500_CODEC_CR8_ENVIBR_DISABLED;
		p_ab8500_codec_configuration->cr9_endachsl =
		    AB8500_CODEC_CR9_ENDACHSL_ENABLED;
		p_ab8500_codec_configuration->cr9_endachsr =
		    AB8500_CODEC_CR9_ENDACHSR_ENABLED;
		p_ab8500_codec_configuration->cr10_muteear =
		    AB8500_CODEC_CR10_MUTEEAR_ENABLED;
		p_ab8500_codec_configuration->cr10_mutehsl =
		    AB8500_CODEC_CR10_MUTEHSL_DISABLED;
		p_ab8500_codec_configuration->cr10_mutehsr =
		    AB8500_CODEC_CR10_MUTEHSR_DISABLED;
		p_ab8500_codec_configuration->cr12_encphs =
		    AB8500_CODEC_CR12_ENCPHS_ENABLED;
		break;
	case AB8500_CODEC_DEST_EARPIECE:
		p_ab8500_codec_configuration->cr8_enear =
		    AB8500_CODEC_CR8_ENEAR_ENABLED;
		p_ab8500_codec_configuration->cr8_enhsl =
		    AB8500_CODEC_CR8_ENHSL_DISABLED;
		p_ab8500_codec_configuration->cr8_enhsr =
		    AB8500_CODEC_CR8_ENHSR_DISABLED;
		p_ab8500_codec_configuration->cr8_enhfl =
		    AB8500_CODEC_CR8_ENHFL_DISABLED;
		p_ab8500_codec_configuration->cr8_enhfr =
		    AB8500_CODEC_CR8_ENHFR_DISABLED;
		p_ab8500_codec_configuration->cr8_envibl =
		    AB8500_CODEC_CR8_ENVIBL_DISABLED;
		p_ab8500_codec_configuration->cr8_envibr =
		    AB8500_CODEC_CR8_ENVIBR_DISABLED;
		p_ab8500_codec_configuration->cr9_endacear =
		    AB8500_CODEC_CR9_ENDACEAR_ENABLED;
		p_ab8500_codec_configuration->cr10_muteear =
		    AB8500_CODEC_CR10_MUTEEAR_DISABLED;
		p_ab8500_codec_configuration->cr10_mutehsl =
		    AB8500_CODEC_CR10_MUTEHSL_ENABLED;
		p_ab8500_codec_configuration->cr10_mutehsr =
		    AB8500_CODEC_CR10_MUTEHSR_ENABLED;
		break;
	case AB8500_CODEC_DEST_HANDSFREE:
		p_ab8500_codec_configuration->cr8_enear =
		    AB8500_CODEC_CR8_ENEAR_DISABLED;
		p_ab8500_codec_configuration->cr8_enhsl =
		    AB8500_CODEC_CR8_ENHSL_DISABLED;
		p_ab8500_codec_configuration->cr8_enhsr =
		    AB8500_CODEC_CR8_ENHSR_DISABLED;
		p_ab8500_codec_configuration->cr8_enhfl =
		    AB8500_CODEC_CR8_ENHFL_ENABLED;
		p_ab8500_codec_configuration->cr8_enhfr =
		    AB8500_CODEC_CR8_ENHFR_ENABLED;
		p_ab8500_codec_configuration->cr8_envibl =
		    AB8500_CODEC_CR8_ENVIBL_DISABLED;
		p_ab8500_codec_configuration->cr8_envibr =
		    AB8500_CODEC_CR8_ENVIBR_DISABLED;
		p_ab8500_codec_configuration->cr9_endachfl =
		    AB8500_CODEC_CR9_ENDACHFL_ENABLED;
		p_ab8500_codec_configuration->cr9_endachfr =
		    AB8500_CODEC_CR9_ENDACHFR_ENABLED;
		p_ab8500_codec_configuration->cr10_muteear =
		    AB8500_CODEC_CR10_MUTEEAR_ENABLED;
		p_ab8500_codec_configuration->cr10_mutehsl =
		    AB8500_CODEC_CR10_MUTEHSL_ENABLED;
		p_ab8500_codec_configuration->cr10_mutehsr =
		    AB8500_CODEC_CR10_MUTEHSR_ENABLED;
		break;
	case AB8500_CODEC_DEST_VIBRATOR_L:
		p_ab8500_codec_configuration->cr8_enear =
		    AB8500_CODEC_CR8_ENEAR_DISABLED;
		p_ab8500_codec_configuration->cr8_enhsl =
		    AB8500_CODEC_CR8_ENHSL_DISABLED;
		p_ab8500_codec_configuration->cr8_enhsr =
		    AB8500_CODEC_CR8_ENHSR_DISABLED;
		p_ab8500_codec_configuration->cr8_enhfl =
		    AB8500_CODEC_CR8_ENHFL_DISABLED;
		p_ab8500_codec_configuration->cr8_enhfr =
		    AB8500_CODEC_CR8_ENHFR_DISABLED;
		p_ab8500_codec_configuration->cr8_envibl =
		    AB8500_CODEC_CR8_ENVIBL_ENABLED;
		p_ab8500_codec_configuration->cr8_envibr =
		    AB8500_CODEC_CR8_ENVIBR_DISABLED;
		p_ab8500_codec_configuration->cr9_endacvibl =
		    AB8500_CODEC_CR9_ENDACVIBL_ENABLED;
		p_ab8500_codec_configuration->cr10_muteear =
		    AB8500_CODEC_CR10_MUTEEAR_ENABLED;
		p_ab8500_codec_configuration->cr10_mutehsl =
		    AB8500_CODEC_CR10_MUTEHSL_ENABLED;
		p_ab8500_codec_configuration->cr10_mutehsr =
		    AB8500_CODEC_CR10_MUTEHSR_ENABLED;
		p_ab8500_codec_configuration->cr15_pwmtovibl =
		    AB8500_CODEC_CR15_PWMTOVIBL_PWM;
		p_ab8500_codec_configuration->cr15_pwmlctrl =
		    AB8500_CODEC_CR15_PWMLCTRL_PWMNPLDUTYCYCLE;
		p_ab8500_codec_configuration->cr15_pwmnlctrl =
		    AB8500_CODEC_CR15_PWMNLCTRL_PWMNLDUTYCYCLE;
		p_ab8500_codec_configuration->cr15_pwmplctrl =
		    AB8500_CODEC_CR15_PWMPLCTRL_PWMPLDUTYCYCLE;
		break;
	case AB8500_CODEC_DEST_VIBRATOR_R:
		p_ab8500_codec_configuration->cr8_enear =
		    AB8500_CODEC_CR8_ENEAR_DISABLED;
		p_ab8500_codec_configuration->cr8_enhsl =
		    AB8500_CODEC_CR8_ENHSL_DISABLED;
		p_ab8500_codec_configuration->cr8_enhsr =
		    AB8500_CODEC_CR8_ENHSR_DISABLED;
		p_ab8500_codec_configuration->cr8_enhfl =
		    AB8500_CODEC_CR8_ENHFL_DISABLED;
		p_ab8500_codec_configuration->cr8_enhfr =
		    AB8500_CODEC_CR8_ENHFR_DISABLED;
		p_ab8500_codec_configuration->cr8_envibl =
		    AB8500_CODEC_CR8_ENVIBL_DISABLED;
		p_ab8500_codec_configuration->cr8_envibr =
		    AB8500_CODEC_CR8_ENVIBR_ENABLED;
		p_ab8500_codec_configuration->cr9_endacvibr =
		    AB8500_CODEC_CR9_ENDACVIBR_ENABLED;
		p_ab8500_codec_configuration->cr10_muteear =
		    AB8500_CODEC_CR10_MUTEEAR_ENABLED;
		p_ab8500_codec_configuration->cr10_mutehsl =
		    AB8500_CODEC_CR10_MUTEHSL_ENABLED;
		p_ab8500_codec_configuration->cr10_mutehsr =
		    AB8500_CODEC_CR10_MUTEHSR_ENABLED;
		p_ab8500_codec_configuration->cr15_pwmtovibr =
		    AB8500_CODEC_CR15_PWMTOVIBR_PWM;
		p_ab8500_codec_configuration->cr15_pwmrctrl =
		    AB8500_CODEC_CR15_PWMRCTRL_PWMNPRDUTYCYCLE;
		p_ab8500_codec_configuration->cr15_pwmnrctrl =
		    AB8500_CODEC_CR15_PWMNRCTRL_PWMNRDUTYCYCLE;
		p_ab8500_codec_configuration->cr15_pwmprctrl =
		    AB8500_CODEC_CR15_PWMPRCTRL_PWMPRDUTYCYCLE;
		break;
	case AB8500_CODEC_DEST_ALL:
		p_ab8500_codec_configuration->cr8_enhsl =
		    AB8500_CODEC_CR8_ENHSL_ENABLED;
		p_ab8500_codec_configuration->cr8_enhsr =
		    AB8500_CODEC_CR8_ENHSR_ENABLED;
		p_ab8500_codec_configuration->cr8_enear =
		    AB8500_CODEC_CR8_ENEAR_ENABLED;
		p_ab8500_codec_configuration->cr8_enhfl =
		    AB8500_CODEC_CR8_ENHFL_ENABLED;
		p_ab8500_codec_configuration->cr8_enhfr =
		    AB8500_CODEC_CR8_ENHFR_ENABLED;
		p_ab8500_codec_configuration->cr8_envibl =
		    AB8500_CODEC_CR8_ENVIBL_ENABLED;
		p_ab8500_codec_configuration->cr8_envibr =
		    AB8500_CODEC_CR8_ENVIBR_ENABLED;
		p_ab8500_codec_configuration->cr9_endacear =
		    AB8500_CODEC_CR9_ENDACEAR_ENABLED;
		p_ab8500_codec_configuration->cr9_endachfl =
		    AB8500_CODEC_CR9_ENDACHFL_ENABLED;
		p_ab8500_codec_configuration->cr9_endachfr =
		    AB8500_CODEC_CR9_ENDACHFR_ENABLED;
		p_ab8500_codec_configuration->cr9_endacvibl =
		    AB8500_CODEC_CR9_ENDACVIBL_ENABLED;
		p_ab8500_codec_configuration->cr9_endacvibr =
		    AB8500_CODEC_CR9_ENDACVIBR_ENABLED;
		p_ab8500_codec_configuration->cr10_mutehsl =
		    AB8500_CODEC_CR10_MUTEHSL_DISABLED;
		p_ab8500_codec_configuration->cr10_mutehsr =
		    AB8500_CODEC_CR10_MUTEHSR_DISABLED;
		p_ab8500_codec_configuration->cr10_muteear =
		    AB8500_CODEC_CR10_MUTEEAR_DISABLED;
		p_ab8500_codec_configuration->cr15_pwmtovibl =
		    AB8500_CODEC_CR15_PWMTOVIBL_PWM;
		p_ab8500_codec_configuration->cr15_pwmlctrl =
		    AB8500_CODEC_CR15_PWMLCTRL_PWMNPLDUTYCYCLE;
		p_ab8500_codec_configuration->cr15_pwmnlctrl =
		    AB8500_CODEC_CR15_PWMNLCTRL_PWMNLDUTYCYCLE;
		p_ab8500_codec_configuration->cr15_pwmplctrl =
		    AB8500_CODEC_CR15_PWMPLCTRL_PWMPLDUTYCYCLE;
		p_ab8500_codec_configuration->cr15_pwmtovibr =
		    AB8500_CODEC_CR15_PWMTOVIBR_PWM;
		p_ab8500_codec_configuration->cr15_pwmrctrl =
		    AB8500_CODEC_CR15_PWMRCTRL_PWMNPRDUTYCYCLE;
		p_ab8500_codec_configuration->cr15_pwmnrctrl =
		    AB8500_CODEC_CR15_PWMNRCTRL_PWMNRDUTYCYCLE;
		p_ab8500_codec_configuration->cr15_pwmprctrl =
		    AB8500_CODEC_CR15_PWMPRCTRL_PWMPRDUTYCYCLE;
		break;
	default:
		ab8500_codec_error = AB8500_CODEC_INVALID_PARAMETER;
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}

PRIVATE t_ab8500_codec_error ab8500_codec_DestPowerControlUpdateCR(void)
{
	t_ab8500_codec_error ab8500_codec_error = AB8500_CODEC_OK;
	ab8500_codec_error = ab8500_codec_UpdateCR8();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR9();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR10();
	if (ab8500_codec_error != AB8500_CODEC_OK)
		 {
		DBGEXIT(ab8500_codec_error);
		return (ab8500_codec_error);
		}
	ab8500_codec_error = ab8500_codec_UpdateCR15();
	DBGEXIT(ab8500_codec_error);
	return (ab8500_codec_error);
}
