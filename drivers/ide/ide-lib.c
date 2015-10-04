#include <linux/types.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/ide.h>
#include <linux/bitops.h>

/**
 *	ide_toggle_bounce	-	handle bounce buffering
 *	@drive: drive to update
 *	@on: on/off boolean
 *
 *	Enable or disable bounce buffering for the device. Drives move
 *	between PIO and DMA and that changes the rules we need.
 */

void ide_toggle_bounce(ide_drive_t *drive, int on)
{
	u64 addr = BLK_BOUNCE_HIGH;	/* dma64_addr_t */

	if (!PCI_DMA_BUS_IS_PHYS) {
		addr = BLK_BOUNCE_ANY;
	} else if (on && drive->media == ide_disk) {
		struct device *dev = drive->hwif->dev;

		if (dev && dev->dma_mask)
			addr = *dev->dma_mask;
	}

	if (drive->queue)
		blk_queue_bounce_limit(drive->queue, addr);
}

u64 ide_get_lba_addr(struct ide_cmd *cmd, int lba48)
{
	struct ide_taskfile *tf = &cmd->tf;
	u32 high, low;

	low  = (tf->lbah << 16) | (tf->lbam << 8) | tf->lbal;
	if (lba48) {
		tf = &cmd->hob;
		high = (tf->lbah << 16) | (tf->lbam << 8) | tf->lbal;
	} else
		high = tf->device & 0xf;

	return ((u64)high << 24) | low;
}
EXPORT_SYMBOL_GPL(ide_get_lba_addr);

static void ide_dump_sector(ide_drive_t *drive)
{
	struct ide_cmd cmd;
	struct ide_taskfile *tf = &cmd.tf;
	u8 lba48 = !!(drive->dev_flags & IDE_DFLAG_LBA48);

	memset(&cmd, 0, sizeof(cmd));
	if (lba48) {
		cmd.valid.in.tf  = IDE_VALID_LBA;
		cmd.valid.in.hob = IDE_VALID_LBA;
		cmd.tf_flags = IDE_TFLAG_LBA48;
	} else
		cmd.valid.in.tf  = IDE_VALID_LBA | IDE_VALID_DEVICE;

	ide_tf_readback(drive, &cmd);

	if (lba48 || (tf->device & ATA_LBA))
//		printk(KERN_CONT ", LBAsect=%llu",
;
	else
//		printk(KERN_CONT ", CHS=%d/%d/%d", (tf->lbah << 8) + tf->lbam,
;
}

static void ide_dump_ata_error(ide_drive_t *drive, u8 err)
{
;
	if (err & ATA_ABORTED)
;
	if (err & ATA_ICRC)
//		printk(KERN_CONT "%s",
;
	if (err & ATA_UNC)
;
	if (err & ATA_IDNF)
;
	if (err & ATA_TRK0NF)
;
	if (err & ATA_AMNF)
;
;
	if ((err & (ATA_BBK | ATA_ABORTED)) == ATA_BBK ||
	    (err & (ATA_UNC | ATA_IDNF | ATA_AMNF))) {
		struct request *rq = drive->hwif->rq;

		ide_dump_sector(drive);

		if (rq)
//			printk(KERN_CONT ", sector=%llu",
;
	}
;
}

static void ide_dump_atapi_error(ide_drive_t *drive, u8 err)
{
;
	if (err & ATAPI_ILI)
;
	if (err & ATAPI_EOM)
;
	if (err & ATA_ABORTED)
;
	if (err & ATA_MCR)
;
	if (err & ATAPI_LFS)
//		printk(KERN_CONT "LastFailedSense=0x%02x ",
;
;
}

/**
 *	ide_dump_status		-	translate ATA/ATAPI error
 *	@drive: drive that status applies to
 *	@msg: text message to print
 *	@stat: status byte to decode
 *
 *	Error reporting, in human readable form (luxurious, but a memory hog).
 *	Combines the drive name, message and status byte to provide a
 *	user understandable explanation of the device error.
 */

u8 ide_dump_status(ide_drive_t *drive, const char *msg, u8 stat)
{
	u8 err = 0;

;
	if (stat & ATA_BUSY)
;
	else {
		if (stat & ATA_DRDY)
;
		if (stat & ATA_DF)
;
		if (stat & ATA_DSC)
;
		if (stat & ATA_DRQ)
;
		if (stat & ATA_CORR)
;
		if (stat & ATA_IDX)
;
		if (stat & ATA_ERR)
;
	}
;
	if ((stat & (ATA_BUSY | ATA_ERR)) == ATA_ERR) {
		err = ide_read_error(drive);
;
		if (drive->media == ide_disk)
			ide_dump_ata_error(drive, err);
		else
			ide_dump_atapi_error(drive, err);
	}

//	printk(KERN_ERR "%s: possibly failed opcode: 0x%02x\n",
;

	return err;
}
EXPORT_SYMBOL(ide_dump_status);
