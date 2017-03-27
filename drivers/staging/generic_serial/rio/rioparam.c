/*
** -----------------------------------------------------------------------------
**
**  Perle Specialix driver for Linux
**  Ported from existing RIO Driver for SCO sources.
 *
 *  (C) 1990 - 2000 Specialix International Ltd., Byfleet, Surrey, UK.
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
**	Module		: rioparam.c
**	SID		: 1.3
**	Last Modified	: 11/6/98 10:33:45
**	Retrieved	: 11/6/98 10:33:50
**
**  ident @(#)rioparam.c	1.3
**
** -----------------------------------------------------------------------------
*/

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/tty.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/string.h>
#include <asm/uaccess.h>

#include <linux/termios.h>
#include <linux/serial.h>

#include <linux/generic_serial.h>


#include "linux_compat.h"
#include "rio_linux.h"
#include "pkt.h"
#include "daemon.h"
#include "rio.h"
#include "riospace.h"
#include "cmdpkt.h"
#include "map.h"
#include "rup.h"
#include "port.h"
#include "riodrvr.h"
#include "rioinfo.h"
#include "func.h"
#include "errors.h"
#include "pci.h"

#include "parmmap.h"
#include "unixrup.h"
#include "board.h"
#include "host.h"
#include "phb.h"
#include "link.h"
#include "cmdblk.h"
#include "route.h"
#include "cirrus.h"
#include "rioioctl.h"
#include "param.h"



/*
** The Scam, based on email from jeremyr@bugs.specialix.co.uk....
**
** To send a command on a particular port, you put a packet with the
** command bit set onto the port. The command bit is in the len field,
** and gets ORed in with the actual byte count.
**
** When you send a packet with the command bit set the first
** data byte (data[0]) is interpreted as the command to execute.
** It also governs what data structure overlay should accompany the packet.
** Commands are defined in cirrus/cirrus.h
**
** If you want the command to pre-emt data already on the queue for the
** port, set the pre-emptive bit in conjunction with the command bit.
** It is not defined what will happen if you set the preemptive bit
** on a packet that is NOT a command.
**
** Pre-emptive commands should be queued at the head of the queue using
** add_start(), whereas normal commands and data are enqueued using
** add_end().
**
** Most commands do not use the remaining bytes in the data array. The
** exceptions are OPEN MOPEN and CONFIG. (NB. As with the SI CONFIG and
** OPEN are currently analogous). With these three commands the following
** 11 data bytes are all used to pass config information such as baud rate etc.
** The fields are also defined in cirrus.h. Some contain straightforward
** information such as the transmit XON character. Two contain the transmit and
** receive baud rates respectively. For most baud rates there is a direct
** mapping between the rates defined in <sys/termio.h> and the byte in the
** packet. There are additional (non UNIX-standard) rates defined in
** /u/dos/rio/cirrus/h/brates.h.
**
** The rest of the data fields contain approximations to the Cirrus registers
** that are used to program number of bits etc. Each registers bit fields is
** defined in cirrus.h.
** 
** NB. Only use those bits that are defined as being driver specific
** or common to the RTA and the driver.
** 
** All commands going from RTA->Host will be dealt with by the Host code - you
** will never see them. As with the SI there will be three fields to look out
** for in each phb (not yet defined - needs defining a.s.a.p).
** 
** modem_status	- current state of handshake pins.
**
** port_status	 - current port status - equivalent to hi_stat for SI, indicates
** if port is IDLE_OPEN, IDLE_CLOSED etc.
**
** break_status	- bit X set if break has been received.
** 
** Happy hacking.
** 
*/

/* 
** RIOParam is used to open or configure a port. You pass it a PortP,
** which will have a tty struct attached to it. You also pass a command,
** either OPEN or CONFIG. The port's setup is taken from the t_ fields
** of the tty struct inside the PortP, and the port is either opened
** or re-configured. You must also tell RIOParam if the device is a modem
** device or not (i.e. top bit of minor number set or clear - take special
** care when deciding on this!).
** RIOParam neither flushes nor waits for drain, and is NOT preemptive.
**
** RIOParam assumes it will be called at splrio(), and also assumes
** that CookMode is set correctly in the port structure.
**
** NB. for MPX
**	tty lock must NOT have been previously acquired.
*/
int RIOParam(struct Port *PortP, int cmd, int Modem, int SleepFlag)
{
	struct tty_struct *TtyP;
	int retval;
	struct phb_param __iomem *phb_param_ptr;
	struct PKT __iomem *PacketP;
	int res;
	u8 Cor1 = 0, Cor2 = 0, Cor4 = 0, Cor5 = 0;
	u8 TxXon = 0, TxXoff = 0, RxXon = 0, RxXoff = 0;
	u8 LNext = 0, TxBaud = 0, RxBaud = 0;
	int retries = 0xff;
	unsigned long flags;

	func_enter();

	TtyP = PortP->gs.port.tty;

;

	if (!TtyP) {
;

		func_exit();

		return RIO_FAIL;
	}
	rio_spin_lock_irqsave(&PortP->portSem, flags);

	if (cmd == RIOC_OPEN) {
		/*
		 ** If the port is set to store or lock the parameters, and it is
		 ** paramed with OPEN, we want to restore the saved port termio, but
		 ** only if StoredTermio has been saved, i.e. NOT 1st open after reboot.
		 */
	}

	/*
	 ** wait for space
	 */
	while (!(res = can_add_transmit(&PacketP, PortP)) || (PortP->InUse != NOT_INUSE)) {
		if (retries-- <= 0) {
			break;
		}
		if (PortP->InUse != NOT_INUSE) {
;
		}

		if (!res) {
;
		}

		if (SleepFlag != OK_TO_SLEEP) {
			rio_spin_unlock_irqrestore(&PortP->portSem, flags);
			func_exit();

			return RIO_FAIL;
		}

;
		rio_spin_unlock_irqrestore(&PortP->portSem, flags);
		retval = RIODelay(PortP, HUNDRED_MS);
		rio_spin_lock_irqsave(&PortP->portSem, flags);
		if (retval == RIO_FAIL) {
;
			rio_spin_unlock_irqrestore(&PortP->portSem, flags);
			func_exit();
			return -EINTR;
		}
		if (PortP->State & RIO_DELETED) {
			rio_spin_unlock_irqrestore(&PortP->portSem, flags);
			func_exit();
			return 0;
		}
	}

	if (!res) {
		rio_spin_unlock_irqrestore(&PortP->portSem, flags);
		func_exit();

		return RIO_FAIL;
	}

;
;

	phb_param_ptr = (struct phb_param __iomem *) PacketP->data;


	switch (TtyP->termios->c_cflag & CSIZE) {
	case CS5:
		{
;
			Cor1 |= RIOC_COR1_5BITS;
			break;
		}
	case CS6:
		{
;
			Cor1 |= RIOC_COR1_6BITS;
			break;
		}
	case CS7:
		{
;
			Cor1 |= RIOC_COR1_7BITS;
			break;
		}
	case CS8:
		{
;
			Cor1 |= RIOC_COR1_8BITS;
			break;
		}
	}

	if (TtyP->termios->c_cflag & CSTOPB) {
;
		Cor1 |= RIOC_COR1_2STOP;
	} else {
;
		Cor1 |= RIOC_COR1_1STOP;
	}

	if (TtyP->termios->c_cflag & PARENB) {
;
		Cor1 |= RIOC_COR1_NORMAL;
	} else {
;
		Cor1 |= RIOC_COR1_NOP;
	}
	if (TtyP->termios->c_cflag & PARODD) {
;
		Cor1 |= RIOC_COR1_ODD;
	} else {
;
		Cor1 |= RIOC_COR1_EVEN;
	}

	/*
	 ** COR 2
	 */
	if (TtyP->termios->c_iflag & IXON) {
;
		Cor2 |= RIOC_COR2_IXON;
	} else {
		if (PortP->Config & RIO_IXON) {
;
			Cor2 |= RIOC_COR2_IXON;
		} else
;
	}

	if (TtyP->termios->c_iflag & IXANY) {
		if (PortP->Config & RIO_IXANY) {
;
			Cor2 |= RIOC_COR2_IXANY;
		} else
;
	}

	if (TtyP->termios->c_iflag & IXOFF) {
;
		Cor2 |= RIOC_COR2_IXOFF;
	}

	if (TtyP->termios->c_cflag & HUPCL) {
;
		Cor2 |= RIOC_COR2_HUPCL;
	}

	if (C_CRTSCTS(TtyP)) {
;
		Cor2 |= RIOC_COR2_CTSFLOW;
		Cor2 |= RIOC_COR2_RTSFLOW;
	} else {
;
		Cor2 &= ~RIOC_COR2_CTSFLOW;
		Cor2 &= ~RIOC_COR2_RTSFLOW;
	}


	if (TtyP->termios->c_cflag & CLOCAL) {
;
	} else {
;
	}

	/*
	 ** COR 4 (there is no COR 3)
	 */
	if (TtyP->termios->c_iflag & IGNBRK) {
;
		Cor4 |= RIOC_COR4_IGNBRK;
	}
	if (!(TtyP->termios->c_iflag & BRKINT)) {
;
		Cor4 |= RIOC_COR4_NBRKINT;
	} else {
;
	}

	if (TtyP->termios->c_iflag & INLCR) {
;
		Cor4 |= RIOC_COR4_INLCR;
	}

	if (TtyP->termios->c_iflag & IGNCR) {
;
		Cor4 |= RIOC_COR4_IGNCR;
	}

	if (TtyP->termios->c_iflag & ICRNL) {
;
		Cor4 |= RIOC_COR4_ICRNL;
	}
	if (TtyP->termios->c_iflag & IGNPAR) {
;
		Cor4 |= RIOC_COR4_IGNPAR;
	}
	if (TtyP->termios->c_iflag & PARMRK) {
;
		Cor4 |= RIOC_COR4_PARMRK;
	}

	/*
	 ** Set the RAISEMOD flag to ensure that the modem lines are raised
	 ** on reception of a config packet.
	 ** The download code handles the zero baud condition.
	 */
	Cor4 |= RIOC_COR4_RAISEMOD;

	/*
	 ** COR 5
	 */

	Cor5 = RIOC_COR5_CMOE;

	/*
	 ** Set to monitor tbusy/tstop (or not).
	 */

	if (PortP->MonitorTstate)
		Cor5 |= RIOC_COR5_TSTATE_ON;
	else
		Cor5 |= RIOC_COR5_TSTATE_OFF;

	/*
	 ** Could set LNE here if you wanted LNext processing. SVR4 will use it.
	 */
	if (TtyP->termios->c_iflag & ISTRIP) {
;
		if (!(PortP->State & RIO_TRIAD_MODE)) {
			Cor5 |= RIOC_COR5_ISTRIP;
		}
	}

	if (TtyP->termios->c_oflag & ONLCR) {
;
		if (PortP->CookMode == COOK_MEDIUM)
			Cor5 |= RIOC_COR5_ONLCR;
	}
	if (TtyP->termios->c_oflag & OCRNL) {
;
		if (PortP->CookMode == COOK_MEDIUM)
			Cor5 |= RIOC_COR5_OCRNL;
	}
	if ((TtyP->termios->c_oflag & TABDLY) == TAB3) {
;
		if (PortP->CookMode == COOK_MEDIUM)
			Cor5 |= RIOC_COR5_TAB3;
	}

	/*
	 ** Flow control bytes.
	 */
	TxXon = TtyP->termios->c_cc[VSTART];
	TxXoff = TtyP->termios->c_cc[VSTOP];
	RxXon = TtyP->termios->c_cc[VSTART];
	RxXoff = TtyP->termios->c_cc[VSTOP];
	/*
	 ** LNEXT byte
	 */
	LNext = 0;

	/*
	 ** Baud rate bytes
	 */
;

	switch (TtyP->termios->c_cflag & CBAUD) {
#define e(b) case B ## b : RxBaud = TxBaud = RIO_B ## b ;break
		e(50);
		e(75);
		e(110);
		e(134);
		e(150);
		e(200);
		e(300);
		e(600);
		e(1200);
		e(1800);
		e(2400);
		e(4800);
		e(9600);
		e(19200);
		e(38400);
		e(57600);
		e(115200);	/* e(230400);e(460800); e(921600);  */
	}

;


	/*
	 ** Leftovers
	 */
	if (TtyP->termios->c_cflag & CREAD)
;
#ifdef RCV1EN
	if (TtyP->termios->c_cflag & RCV1EN)
;
#endif
#ifdef XMT1EN
	if (TtyP->termios->c_cflag & XMT1EN)
;
#endif
	if (TtyP->termios->c_lflag & ISIG)
;
	if (TtyP->termios->c_lflag & ICANON)
;
	if (TtyP->termios->c_lflag & XCASE)
;
	if (TtyP->termios->c_lflag & ECHO)
;
	if (TtyP->termios->c_lflag & ECHOE)
;
	if (TtyP->termios->c_lflag & ECHOK)
;
	if (TtyP->termios->c_lflag & ECHONL)
;
	if (TtyP->termios->c_lflag & NOFLSH)
;
#ifdef TOSTOP
	if (TtyP->termios->c_lflag & TOSTOP)
;
#endif
#ifdef XCLUDE
	if (TtyP->termios->c_lflag & XCLUDE)
;
#endif
	if (TtyP->termios->c_iflag & IUCLC)
;
	if (TtyP->termios->c_oflag & OPOST)
;
	if (TtyP->termios->c_oflag & OLCUC)
;
	if (TtyP->termios->c_oflag & ONOCR)
;
	if (TtyP->termios->c_oflag & ONLRET)
;
	if (TtyP->termios->c_oflag & OFILL)
;
	if (TtyP->termios->c_oflag & OFDEL)
;
	if (TtyP->termios->c_oflag & NLDLY)
;
	if (TtyP->termios->c_oflag & CRDLY)
;
	if (TtyP->termios->c_oflag & TABDLY)
;
	/*
	 ** These things are kind of useful in a later life!
	 */
	PortP->Cor2Copy = Cor2;

	if (PortP->State & RIO_DELETED) {
		rio_spin_unlock_irqrestore(&PortP->portSem, flags);
		func_exit();

		return RIO_FAIL;
	}

	/*
	 ** Actually write the info into the packet to be sent
	 */
	writeb(cmd, &phb_param_ptr->Cmd);
	writeb(Cor1, &phb_param_ptr->Cor1);
	writeb(Cor2, &phb_param_ptr->Cor2);
	writeb(Cor4, &phb_param_ptr->Cor4);
	writeb(Cor5, &phb_param_ptr->Cor5);
	writeb(TxXon, &phb_param_ptr->TxXon);
	writeb(RxXon, &phb_param_ptr->RxXon);
	writeb(TxXoff, &phb_param_ptr->TxXoff);
	writeb(RxXoff, &phb_param_ptr->RxXoff);
	writeb(LNext, &phb_param_ptr->LNext);
	writeb(TxBaud, &phb_param_ptr->TxBaud);
	writeb(RxBaud, &phb_param_ptr->RxBaud);

	/*
	 ** Set the length/command field
	 */
	writeb(12 | PKT_CMD_BIT, &PacketP->len);

	/*
	 ** The packet is formed - now, whack it off
	 ** to its final destination:
	 */
	add_transmit(PortP);
	/*
	 ** Count characters transmitted for port statistics reporting
	 */
	if (PortP->statsGather)
		PortP->txchars += 12;

	rio_spin_unlock_irqrestore(&PortP->portSem, flags);

;
	/*
	 ** job done.
	 */
	func_exit();

	return 0;
}


/*
** We can add another packet to a transmit queue if the packet pointer pointed
** to by the TxAdd pointer has PKT_IN_USE clear in its address.
*/
int can_add_transmit(struct PKT __iomem **PktP, struct Port *PortP)
{
	struct PKT __iomem *tp;

	*PktP = tp = (struct PKT __iomem *) RIO_PTR(PortP->Caddr, readw(PortP->TxAdd));

	return !((unsigned long) tp & PKT_IN_USE);
}

/*
** To add a packet to the queue, you set the PKT_IN_USE bit in the address,
** and then move the TxAdd pointer along one position to point to the next
** packet pointer. You must wrap the pointer from the end back to the start.
*/
void add_transmit(struct Port *PortP)
{
	if (readw(PortP->TxAdd) & PKT_IN_USE) {
;
	}
	writew(readw(PortP->TxAdd) | PKT_IN_USE, PortP->TxAdd);
	PortP->TxAdd = (PortP->TxAdd == PortP->TxEnd) ? PortP->TxStart : PortP->TxAdd + 1;
	writew(RIO_OFF(PortP->Caddr, PortP->TxAdd), &PortP->PhbP->tx_add);
}

/****************************************
 * Put a packet onto the end of the
 * free list
 ****************************************/
void put_free_end(struct Host *HostP, struct PKT __iomem *PktP)
{
	struct rio_free_list __iomem *tmp_pointer;
	unsigned short old_end, new_end;
	unsigned long flags;

	rio_spin_lock_irqsave(&HostP->HostLock, flags);

	 /*************************************************
	* Put a packet back onto the back of the free list
	*
	************************************************/

;

	if ((old_end = readw(&HostP->ParmMapP->free_list_end)) != TPNULL) {
		new_end = RIO_OFF(HostP->Caddr, PktP);
		tmp_pointer = (struct rio_free_list __iomem *) RIO_PTR(HostP->Caddr, old_end);
		writew(new_end, &tmp_pointer->next);
		writew(old_end, &((struct rio_free_list __iomem *) PktP)->prev);
		writew(TPNULL, &((struct rio_free_list __iomem *) PktP)->next);
		writew(new_end, &HostP->ParmMapP->free_list_end);
	} else {		/* First packet on the free list this should never happen! */
;
		writew(RIO_OFF(HostP->Caddr, PktP), &HostP->ParmMapP->free_list_end);
		tmp_pointer = (struct rio_free_list __iomem *) PktP;
		writew(TPNULL, &tmp_pointer->prev);
		writew(TPNULL, &tmp_pointer->next);
	}
;
	rio_spin_unlock_irqrestore(&HostP->HostLock, flags);
}

/*
** can_remove_receive(PktP,P) returns non-zero if PKT_IN_USE is set
** for the next packet on the queue. It will also set PktP to point to the
** relevant packet, [having cleared the PKT_IN_USE bit]. If PKT_IN_USE is clear,
** then can_remove_receive() returns 0.
*/
int can_remove_receive(struct PKT __iomem **PktP, struct Port *PortP)
{
	if (readw(PortP->RxRemove) & PKT_IN_USE) {
		*PktP = (struct PKT __iomem *) RIO_PTR(PortP->Caddr, readw(PortP->RxRemove) & ~PKT_IN_USE);
		return 1;
	}
	return 0;
}

/*
** To remove a packet from the receive queue you clear its PKT_IN_USE bit,
** and then bump the pointers. Once the pointers get to the end, they must
** be wrapped back to the start.
*/
void remove_receive(struct Port *PortP)
{
	writew(readw(PortP->RxRemove) & ~PKT_IN_USE, PortP->RxRemove);
	PortP->RxRemove = (PortP->RxRemove == PortP->RxEnd) ? PortP->RxStart : PortP->RxRemove + 1;
	writew(RIO_OFF(PortP->Caddr, PortP->RxRemove), &PortP->PhbP->rx_remove);
}
