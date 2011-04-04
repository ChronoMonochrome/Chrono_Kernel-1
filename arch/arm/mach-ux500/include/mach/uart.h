/*
 * Copyright (C) 2009 ST-Ericsson SA
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef _U8500_UART_H_
#define _U8500_UART_H_

struct uart_amba_plat_data {
	void (*init) (void);
	void (*exit) (void);
};

#endif /* _U8500_UART_H_ */
