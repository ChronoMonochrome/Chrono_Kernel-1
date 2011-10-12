/*
 * Copyright ST-Ericsson 2011.
 *
 * Author: Jarmo K. Kuronen <jarmo.kuronen@symbio.com> for ST Ericsson.
 * Licensed under GPLv2.
 */

#ifndef _AB8500_ACCDET_H
#define _AB8500_ACCDET_H

/*
* Debounce times for AccDet1 input
* @0x880 [2:0]
*/
#define ACCDET1_DB_0ms		0x00
#define ACCDET1_DB_10ms		0x01
#define ACCDET1_DB_20ms		0x02
#define ACCDET1_DB_30ms		0x03
#define ACCDET1_DB_40ms		0x04
#define ACCDET1_DB_50ms		0x05
#define ACCDET1_DB_60ms		0x06
#define ACCDET1_DB_70ms		0x07

/*
* Voltage threshold for AccDet1 input
* @0x880 [6:3]
*/
#define ACCDET1_TH_1100mV	0x40
#define ACCDET1_TH_1200mV	0x48
#define ACCDET1_TH_1300mV	0x50
#define ACCDET1_TH_1400mV	0x58
#define ACCDET1_TH_1500mV	0x60
#define ACCDET1_TH_1600mV	0x68
#define ACCDET1_TH_1700mV	0x70
#define ACCDET1_TH_1800mV	0x78

/*
* Voltage threshold for AccDet21 input
* @0x881 [3:0]
*/
#define ACCDET21_TH_300mV	0x00
#define ACCDET21_TH_400mV	0x01
#define ACCDET21_TH_500mV	0x02
#define ACCDET21_TH_600mV	0x03
#define ACCDET21_TH_700mV	0x04
#define ACCDET21_TH_800mV	0x05
#define ACCDET21_TH_900mV	0x06
#define ACCDET21_TH_1000mV	0x07
#define ACCDET21_TH_1100mV	0x08
#define ACCDET21_TH_1200mV	0x09
#define ACCDET21_TH_1300mV	0x0a
#define ACCDET21_TH_1400mV	0x0b
#define ACCDET21_TH_1500mV	0x0c
#define ACCDET21_TH_1600mV	0x0d
#define ACCDET21_TH_1700mV	0x0e
#define ACCDET21_TH_1800mV	0x0f

/*
* Voltage threshold for AccDet22 input
* @0x881 [7:4]
*/
#define ACCDET22_TH_300mV	0x00
#define ACCDET22_TH_400mV	0x10
#define ACCDET22_TH_500mV	0x20
#define ACCDET22_TH_600mV	0x30
#define ACCDET22_TH_700mV	0x40
#define ACCDET22_TH_800mV	0x50
#define ACCDET22_TH_900mV	0x60
#define ACCDET22_TH_1000mV	0x70
#define ACCDET22_TH_1100mV	0x80
#define ACCDET22_TH_1200mV	0x90
#define ACCDET22_TH_1300mV	0xa0
#define ACCDET22_TH_1400mV	0xb0
#define ACCDET22_TH_1500mV	0xc0
#define ACCDET22_TH_1600mV	0xd0
#define ACCDET22_TH_1700mV	0xe0
#define ACCDET22_TH_1800mV	0xf0

/**
 * struct ab8500_accdet_platform_data - AV Accessory detection specific
 * platform data
 * @btn_keycode		Keycode to be sent when accessory button is pressed.
 * @accdet1_dbth	Debounce time + voltage threshold for accdet 1 input.
 * @accdet2122_th	Voltage thresholds for accdet21 and accdet22 inputs.
 * @is_detection_inverted	Whether the accessory insert/removal, button
 * press/release irq's are inverted.
 */
struct ab8500_accdet_platform_data {
	int btn_keycode;
	u8 accdet1_dbth;
	u8 accdet2122_th;
	unsigned int video_ctrl_gpio;
	bool is_detection_inverted;
};

#endif /* _AB8500_ACCDET_H */
