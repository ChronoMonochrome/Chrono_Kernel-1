/*
 * ab8500_gpadc.c - AB8500 GPADC Driver
 *
 * Copyright (C) 2010 ST-Ericsson SA
 * Licensed under GPLv2.
 *
 * Author: Arun R Murthy <arun.murthy@stericsson.com>
 */

#ifndef	_AB8500_GPADC_H
#define _Ab8500_GPADC_H

/* GPADC source: From datasheer(ADCSwSel[4:0] in GPADCCtrl2) */
#define BAT_CTRL        0x01
#define ACC_DETECT1     0x04
#define ACC_DETECT2     0x05
#define MAIN_BAT_V      0x08
#define BK_BAT_V        0x0C
#define VBUS_V          0x09
#define MAIN_CHARGER_V  0x03
#define MAIN_CHARGER_C  0x0A
#define USB_CHARGER_C   0x0B
#define DIE_TEMP        0x0D
#define BTEMP_BALL      0x02

struct ab8500_gpadc_device_info {
	struct completion ab8500_gpadc_complete;
	struct mutex ab8500_gpadc_lock;
#if defined(CONFIG_REGULATOR)
	struct regulator *regu;
#endif
};

int ab8500_gpadc_conversion(int input);

#endif /* _AB8500_GPADC_H */
