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
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_CONT ", LBAsect=%llu",
			(unsigned long long)ide_get_lba_addr(&cmd, lba48));
#else
		;
#endif
	else
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_CONT ", CHS=%d/%d/%d", (tf->lbah << 8) + tf->lbam,
			tf->device & 0xf, tf->lbal);
#else
		;
#endif
}

static void ide_dump_ata_error(ide_drive_t *drive, u8 err)
{
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_CONT "{ ");
#else
	;
#endif
	if (err & ATA_ABORTED)
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_CONT "DriveStatusError ");
#else
		;
#endif
	if (err & ATA_ICRC)
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_CONT "%s",
			(err & ATA_ABORTED) ? "BadCRC " : "BadSector ");
#else
		;
#endif
	if (err & ATA_UNC)
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_CONT "UncorrectableError ");
#else
		;
#endif
	if (err & ATA_IDNF)
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_CONT "SectorIdNotFound ");
#else
		;
#endif
	if (err & ATA_TRK0NF)
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_CONT "TrackZeroNotFound ");
#else
		;
#endif
	if (err & ATA_AMNF)
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_CONT "AddrMarkNotFound ");
#else
		;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_CONT "}");
#else
	;
#endif
	if ((err & (ATA_BBK | ATA_ABORTED)) == ATA_BBK ||
	    (err & (ATA_UNC | ATA_IDNF | ATA_AMNF))) {
		struct request *rq = drive->hwif->rq;

		ide_dump_sector(drive);

		if (rq)
#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_CONT ", sector=%llu",
			       (unsigned long long)blk_rq_pos(rq));
#else
			;
#endif
	}
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_CONT "\n");
#else
	;
#endif
}

static void ide_dump_atapi_error(ide_drive_t *drive, u8 err)
{
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_CONT "{ ");
#else
	;
#endif
	if (err & ATAPI_ILI)
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_CONT "IllegalLengthIndication ");
#else
		;
#endif
	if (err & ATAPI_EOM)
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_CONT "EndOfMedia ");
#else
		;
#endif
	if (err & ATA_ABORTED)
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_CONT "AbortedCommand ");
#else
		;
#endif
	if (err & ATA_MCR)
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_CONT "MediaChangeRequested ");
#else
		;
#endif
	if (err & ATAPI_LFS)
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_CONT "LastFailedSense=0x%02x ",
			(err & ATAPI_LFS) >> 4);
#else
		;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_CONT "}\n");
#else
	;
#endif
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

	printk(KERN_ERR "%s: %s: status=0x%02x { ", drive->name, msg, stat);
	if (stat & ATA_BUSY)
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_CONT "Busy ");
#else
		;
#endif
	else {
		if (stat & ATA_DRDY)
#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_CONT "DriveReady ");
#else
			;
#endif
		if (stat & ATA_DF)
#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_CONT "DeviceFault ");
#else
			;
#endif
		if (stat & ATA_DSC)
#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_CONT "SeekComplete ");
#else
			;
#endif
		if (stat & ATA_DRQ)
#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_CONT "DataRequest ");
#else
			;
#endif
		if (stat & ATA_CORR)
#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_CONT "CorrectedError ");
#else
			;
#endif
		if (stat & ATA_IDX)
#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_CONT "Index ");
#else
			;
#endif
		if (stat & ATA_ERR)
#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_CONT "Error ");
#else
			;
#endif
	}
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_CONT "}\n");
#else
	;
#endif
	if ((stat & (ATA_BUSY | ATA_ERR)) == ATA_ERR) {
		err = ide_read_error(drive);
		printk(KERN_ERR "%s: %s: error=0x%02x ", drive->name, msg, err);
		if (drive->media == ide_disk)
			ide_dump_ata_error(drive, err);
		else
			ide_dump_atapi_error(drive, err);
	}

	printk(KERN_ERR "%s: possibly failed opcode: 0x%02x\n",
		drive->name, drive->hwif->cmd.tf.command);

	return err;
}
EXPORT_SYMBOL(ide_dump_status);
