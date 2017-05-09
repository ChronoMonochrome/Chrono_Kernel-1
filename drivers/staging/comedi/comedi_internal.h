#ifndef _COMEDI_INTERNAL_H
#define _COMEDI_INTERNAL_H

#include <linux/types.h>

/*
 * various internal comedi functions
 */
int do_rangeinfo_ioctl(struct comedi_device *dev,
		       struct comedi_rangeinfo __user *arg);
int insn_inval(struct comedi_device *dev, struct comedi_subdevice *s,
	       struct comedi_insn *insn, unsigned int *data);
int comedi_alloc_board_minor(struct device *hardware_device);
void comedi_free_board_minor(unsigned minor);
void comedi_reset_async_buf(struct comedi_async *async);
int comedi_buf_alloc(struct comedi_device *dev, struct comedi_subdevice *s,
		     unsigned long new_size);
<<<<<<< HEAD:drivers/staging/comedi/internal.h
=======

extern unsigned int comedi_default_buf_size_kb;
extern unsigned int comedi_default_buf_maxsize_kb;
extern bool comedi_autoconfig;
extern struct comedi_driver *comedi_drivers;

#endif /* _COMEDI_INTERNAL_H */
>>>>>>> 419e9266884fa853179ab726c27a63a9d3ae46e3:drivers/staging/comedi/comedi_internal.h
