/*
 * lsm303dlh.h
 * ST 3-Axis Accelerometer/Magnetometer header file
 *
 * Copyright (C) 2010 STMicroelectronics
 * Author: Carmine Iascone (carmine.iascone@st.com)
 * Author: Matteo Dameno (matteo.dameno@st.com)
 *
 * Copyright (C) 2010 STEricsson
 * Author: Mian Yousaf Kaukab <mian.yousaf.kaukab@stericsson.com>
 * Updated:Preetham Rao Kaskurthi <preetham.rao@stericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LSM303DLH_H__
#define __LSM303DLH_H__

#include <linux/ioctl.h>

#ifdef __KERNEL__
struct lsm303dlh_platform_data {

	/* name of device for regulator */

	const char *name_a; /* acelerometer name */
	const char *name_m; /* magnetometer name */

	/*  interrupt data */
	u32  irq_a1;      /* interrupt line 1 of accelrometer*/
	u32  irq_a2;      /* interrupt line 2 of accelrometer*/
	u32  irq_m;       /* interrupt line of magnetometer*/

	/* position of x,y and z axis */
	u8  axis_map_x; /* [0-2] */
	u8  axis_map_y; /* [0-2] */
	u8  axis_map_z; /* [0-2] */

	/* orientation of x,y and z axis */
	u8  negative_x; /* [0-1] */
	u8  negative_y; /* [0-1] */
	u8  negative_z; /* [0-1] */
};
#endif /* __KERNEL__ */

#endif  /* __LSM303DLH_H__ */
