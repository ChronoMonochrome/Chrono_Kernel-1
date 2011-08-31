/*----------------------------------------------------------------------------------*/
/*  copyright STMicroelectronics, 2007.                                            */
/*                                                                                  */
/* This program is free software; you can redistribute it and/or modify it under    */
/* the terms of the GNU General Public License as published by the Free	            */
/* Software Foundation; either version 2.1 of the License, or (at your option)      */
/* any later version.                                                               */
/*                                                                                  */
/* This program is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS    */
/* FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.   */
/*                                                                                  */
/* You should have received a copy of the GNU General Public License                */
/* along with this program. If not, see <http://www.gnu.org/licenses/>.             */
/*----------------------------------------------------------------------------------*/


#ifndef STM_MSP_HEADER
#define STM_MSP_HEADER

#define MSP_DR 		0x00
#define MSP_GCR		0x04
#define MSP_TCF		0x08
#define MSP_RCF		0x0c
#define MSP_SRG		0x10
#define MSP_FLR		0x14
#define MSP_DMACR	0x18

#define MSP_IMSC	0x20
#define MSP_RIS		0x24
#define MSP_MIS 	0x28
#define MSP_ICR		0x2c
#define MSP_MCR		0x30
#define MSP_RCV 	0x34
#define MSP_RCM 	0x38

#define MSP_TCE0	0x40
#define MSP_TCE1	0x44
#define MSP_TCE2 	0x48
#define MSP_TCE3 	0x4c

#define MSP_RCE0	0x60
#define MSP_RCE1	0x64
#define MSP_RCE2	0x68
#define MSP_RCE3 	0x6c

#define MSP_ITCR	0x80
#define MSP_ITIP	0x84
#define MSP_ITOP	0x88
#define MSP_TSTDR	0x8c

#define MSP_PID0	0xfe0
#define MSP_PID1	0xfe4
#define MSP_PID2 	0xfe8
#define MSP_PID3	0xfec

#define MSP_CID0	0xff0
#define MSP_CID1 	0xff4
#define MSP_CID2	0xff8
#define MSP_CID3 	0xffc


/* Single or dual phase mode */
enum
{
	MSP_SINGLE_PHASE,
	MSP_DUAL_PHASE
};


/* Transmit/Receive shifter status
-----------------------------------*/
enum
{
        MSP_SxHIFTER_IDLE   = 0,
        MSP_SHIFTER_WORKING = 1
};


/* Transmit/Receive FIFO status
---------------------------------*/
enum
{
	MSP_FIFO_FULL,
	MSP_FIFO_PART_FILLED,
	MSP_FIFO_EMPTY
};


/* Frame length
------------------*/
enum
{
	MSP_FRAME_LENGTH_1		= 0,
	MSP_FRAME_LENGTH_2		= 1,
	MSP_FRAME_LENGTH_4		= 3,
	MSP_FRAME_LENGTH_8		= 7,
	MSP_FRAME_LENGTH_12		= 11,
	MSP_FRAME_LENGTH_16		= 15,
	MSP_FRAME_LENGTH_20		= 19,
	MSP_FRAME_LENGTH_32		= 31,
	MSP_FRAME_LENGTH_48		= 47,
	MSP_FRAME_LENGTH_64		= 63
};

/* Element length */
enum
{
	MSP_ELEM_LENGTH_8		= 0,
	MSP_ELEM_LENGTH_10		= 1,
	MSP_ELEM_LENGTH_12		= 2,
	MSP_ELEM_LENGTH_14		= 3,
	MSP_ELEM_LENGTH_16		= 4,
	MSP_ELEM_LENGTH_20		= 5,
	MSP_ELEM_LENGTH_24		= 6,
	MSP_ELEM_LENGTH_32		= 7
};


/* Data delay (in bit clock cycles)
---------------------------------------*/
enum
{
	MSP_DELAY_0 			= 0,
	MSP_DELAY_1 			= 1,
	MSP_DELAY_2 			= 2,
	MSP_DELAY_3 			= 3
};


/* Configurations of clocks (transmit, receive or sample rate generator)
-------------------------------------------------------------------------*/
enum
{
	MSP_RISING_EDGE			= 0,
	MSP_FALLING_EDGE		= 1
};

/* Protocol dependant parameters list */
struct msp_protocol_desc
{
        u32 phase_mode;
        u32 frame_len_1;
        u32 frame_len_2;
        u32 element_len_1;
        u32 element_len_2;
        u32 data_delay;
        u32 tx_clock_edge;
        u32 rx_clock_edge;
};
#define RX_ENABLE_MASK         0x00000001
#define RX_FIFO_ENABLE_MASK    0x00000002
#define RX_FRAME_SYNC_MASK     0x00000004
#define DIRECT_COMPANDING_MASK 0x00000008
#define RX_SYNC_SEL_MASK       0x00000010
#define RX_CLK_POL_MASK        0x00000020
#define RX_CLK_SEL_MASK        0x00000040
#define LOOPBACK_MASK          0x00000080
#define TX_ENABLE_MASK         0x00000100
#define TX_FIFO_ENABLE_MASK    0x00000200
#define TX_FRAME_SYNC_MASK     0x00000400
#define TX_MSP_TDR_TSR         0x00000800
#define TX_SYNC_SEL_MASK       0x00001800
#define TX_CLK_POL_MASK        0x00002000
#define TX_CLK_SEL_MASK        0x00004000
#define TX_EXTRA_DELAY_MASK    0x00008000
#define SRG_ENABLE_MASK        0x00010000
#define SRG_CLK_POL_MASK       0x00020000
#define SRG_CLK_SEL_MASK       0x000C0000
#define FRAME_GEN_EN_MASK      0x00100000
#define SPI_CLK_MODE_MASK      0x00600000
#define SPI_BURST_MODE_MASK    0x00800000

#define RXEN_BIT		0
#define RFFEN_BIT		1
#define RFSPOL_BIT		2
#define DCM_BIT		3
#define RFSSEL_BIT		4
#define RCKPOL_BIT		5
#define RCKSEL_BIT		6
#define LBM_BIT		7
#define TXEN_BIT		8
#define TFFEN_BIT		9
#define TFSPOL_BIT		10
#define TFSSEL_BIT		11
#define TCKPOL_BIT		13
#define TCKSEL_BIT		14
#define TXDDL_BIT		15
#define SGEN_BIT		16
#define SCKPOL_BIT		17
#define SCKSEL_BIT		18
#define FGEN_BIT		20
#define SPICKM_BIT		21

#define msp_rx_clkpol_bit(n)     ((n & 1) << RCKPOL_BIT)
#define msp_tx_clkpol_bit(n)     ((n & 1) << TCKPOL_BIT)
#define msp_spi_clk_mode_bits(n) ((n & 3) << SPICKM_BIT)


/* Use this to clear the clock mode bits to non-spi */
#define MSP_NON_SPI_CLK_MASK 0x00600000

#define P1ELEN_BIT		0
#define P1FLEN_BIT		3
#define DTYP_BIT		10
#define ENDN_BIT		12
#define DDLY_BIT		13
#define FSIG_BIT		15
#define P2ELEN_BIT		16
#define P2FLEN_BIT		19
#define P2SM_BIT		26
#define P2EN_BIT		27

#define msp_p1_elem_len_bits(n) (n & 0x00000007)
#define msp_p2_elem_len_bits(n)  (((n) << P2ELEN_BIT) & 0x00070000)
#define msp_p1_frame_len_bits(n) (((n) << P1FLEN_BIT) & 0x00000378)
#define msp_p2_frame_len_bits(n) (((n) << P2FLEN_BIT) & 0x03780000)
#define msp_data_delay_bits(n)   (((n) << DDLY_BIT) & 0x00003000)
#define msp_data_type_bits(n)    (((n) << DTYP_BIT) & 0x00000600)
#define msp_p2_start_mode_bit(n) (n << P2SM_BIT)
#define msp_p2_enable_bit(n)     (n << P2EN_BIT)

/* Flag register
--------------------*/
#define RX_BUSY       0x00000001
#define RX_FIFO_EMPTY 0x00000002
#define RX_FIFO_FULL  0x00000004
#define TX_BUSY       0x00000008
#define TX_FIFO_EMPTY 0x00000010
#define TX_FIFO_FULL  0x00000020

#define RBUSY_BIT		0
#define RFE_BIT			1
#define RFU_BIT			2
#define TBUSY_BIT		3
#define TFE_BIT			4
#define TFU_BIT			5

/* Multichannel control register
---------------------------------*/
#define RMCEN_BIT		0
#define RMCSF_BIT		1
#define RCMPM_BIT		3
#define TMCEN_BIT		5
#define TNCSF_BIT		6

/* Sample rate generator register
------------------------------------*/
#define SCKDIV_BIT		0
#define FRWID_BIT		10
#define FRPER_BIT		16

#define SCK_DIV_MASK 0x0000003FF
#define frame_width_bits(n) (((n) << FRWID_BIT)  &0x0000FC00)
#define frame_period_bits(n) (((n) << FRPER_BIT) &0x1FFF0000)


/* DMA controller register
---------------------------*/
#define RX_DMA_ENABLE 0x00000001
#define TX_DMA_ENABLE 0x00000002

#define RDMAE_BIT		0
#define TDMAE_BIT		1

/*Interrupt Register
-----------------------------------------*/
#define RECEIVE_SERVICE_INT         0x00000001
#define RECEIVE_OVERRUN_ERROR_INT   0x00000002
#define RECEIVE_FRAME_SYNC_ERR_INT  0x00000004
#define RECEIVE_FRAME_SYNC_INT      0x00000008
#define TRANSMIT_SERVICE_INT        0x00000010
#define TRANSMIT_UNDERRUN_ERR_INT   0x00000020
#define TRANSMIT_FRAME_SYNC_ERR_INT 0x00000040
#define TRANSMIT_FRAME_SYNC_INT     0x00000080
#define ALL_INT                     0x000000ff

/* Protocol configuration values
* I2S: Single phase, 16 bits, 2 words per frame
-----------------------------------------------*/
#define I2S_PROTOCOL_DESC			\
{									\
	MSP_SINGLE_PHASE,				\
	MSP_FRAME_LENGTH_1,				\
	MSP_FRAME_LENGTH_1,				\
	MSP_ELEM_LENGTH_32,				\
	MSP_ELEM_LENGTH_32,				\
	MSP_DELAY_1,					\
	MSP_FALLING_EDGE,				\
	MSP_FALLING_EDGE			\
}

#define PCM_PROTOCOL_DESC			\
{									\
	MSP_SINGLE_PHASE,				\
	MSP_FRAME_LENGTH_1,				\
	MSP_FRAME_LENGTH_1,				\
	MSP_ELEM_LENGTH_16,				\
	MSP_ELEM_LENGTH_16,				\
	MSP_DATA_DELAY,					\
	MSP_TX_CLOCK_EDGE,				\
	MSP_RX_CLOCK_EDGE			\
}

/* Companded PCM: Single phase, 8 bits, 1 word per frame
--------------------------------------------------------*/
#define PCM_COMPAND_PROTOCOL_DESC	\
{									\
	MSP_SINGLE_PHASE,				\
	MSP_FRAME_LENGTH_1,				\
	MSP_FRAME_LENGTH_1,				\
	MSP_ELEM_LENGTH_8,				\
	MSP_ELEM_LENGTH_8,				\
	MSP_DELAY_0,					\
	MSP_RISING_EDGE,					\
	MSP_FALLING_EDGE					\
}

/* AC97: Double phase, 1 element of 16 bits during first phase,
* 12 elements of 20 bits in second phase.
--------------------------------------------------------------*/
#define AC97_PROTOCOL_DESC			\
{									\
	MSP_DUAL_PHASE,					\
	MSP_FRAME_LENGTH_1,				\
	MSP_FRAME_LENGTH_12,			\
	MSP_ELEM_LENGTH_16,				\
	MSP_ELEM_LENGTH_20,				\
	MSP_DELAY_1,					\
	MSP_RISING_EDGE,					\
	MSP_FALLING_EDGE					\
}

#define SPI_MASTER_PROTOCOL_DESC	\
{									\
	MSP_SINGLE_PHASE,				\
	MSP_FRAME_LENGTH_1,				\
	MSP_FRAME_LENGTH_1,				\
	MSP_ELEM_LENGTH_8,				\
	MSP_ELEM_LENGTH_8,				\
	MSP_DELAY_1,					\
	MSP_FALLING_EDGE,				\
	MSP_RISING_EDGE				\
}
#define SPI_SLAVE_PROTOCOL_DESC		\
{									\
	MSP_SINGLE_PHASE,				\
	MSP_FRAME_LENGTH_1,				\
	MSP_FRAME_LENGTH_1,				\
	MSP_ELEM_LENGTH_8,				\
	MSP_ELEM_LENGTH_8,				\
	MSP_DELAY_1,					\
	MSP_FALLING_EDGE,				\
	MSP_RISING_EDGE				\
}

#define MSP_FRAME_PERIOD_IN_MONO_MODE 256
#define MSP_FRAME_PERIOD_IN_STEREO_MODE 32
#define MSP_FRAME_WIDTH_IN_STEREO_MODE 16

#endif

