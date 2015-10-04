/*
 * Workbit NinjaSCSI-32Bi/UDE PCI/CardBus SCSI Host Bus Adapter driver
 * Debug routine
 *
 * This software may be used and distributed according to the terms of
 * the GNU General Public License.
 */

/*
 * Show the command data of a command
 */
static const char unknown[] = "UNKNOWN";

static const char * group_0_commands[] = {
/* 00-03 */ "Test Unit Ready", "Rezero Unit", unknown, "Request Sense",
/* 04-07 */ "Format Unit", "Read Block Limits", unknown, "Reassign Blocks",
/* 08-0d */ "Read (6)", unknown, "Write (6)", "Seek (6)", unknown, unknown,
/* 0e-12 */ unknown, "Read Reverse", "Write Filemarks", "Space", "Inquiry",  
/* 13-16 */ unknown, "Recover Buffered Data", "Mode Select", "Reserve",
/* 17-1b */ "Release", "Copy", "Erase", "Mode Sense", "Start/Stop Unit",
/* 1c-1d */ "Receive Diagnostic", "Send Diagnostic", 
/* 1e-1f */ "Prevent/Allow Medium Removal", unknown,
};


static const char *group_1_commands[] = {
/* 20-22 */  unknown, unknown, unknown,
/* 23-28 */ unknown, unknown, "Read Capacity", unknown, unknown, "Read (10)",
/* 29-2d */ unknown, "Write (10)", "Seek (10)", unknown, unknown,
/* 2e-31 */ "Write Verify","Verify", "Search High", "Search Equal",
/* 32-34 */ "Search Low", "Set Limits", "Prefetch or Read Position", 
/* 35-37 */ "Synchronize Cache","Lock/Unlock Cache", "Read Defect Data",
/* 38-3c */ "Medium Scan", "Compare","Copy Verify", "Write Buffer", "Read Buffer",
/* 3d-3f */ "Update Block", "Read Long",  "Write Long",
};


static const char *group_2_commands[] = {
/* 40-41 */ "Change Definition", "Write Same", 
/* 42-48 */ "Read Sub-Ch(cd)", "Read TOC", "Read Header(cd)", "Play Audio(cd)", unknown, "Play Audio MSF(cd)", "Play Audio Track/Index(cd)", 
/* 49-4f */ "Play Track Relative(10)(cd)", unknown, "Pause/Resume(cd)", "Log Select", "Log Sense", unknown, unknown,
/* 50-55 */ unknown, unknown, unknown, unknown, unknown, "Mode Select (10)",
/* 56-5b */ unknown, unknown, unknown, unknown, "Mode Sense (10)", unknown,
/* 5c-5f */ unknown, unknown, unknown,
};

#define group(opcode) (((opcode) >> 5) & 7)

#define RESERVED_GROUP  0
#define VENDOR_GROUP    1
#define NOTEXT_GROUP    2

static const char **commands[] = {
    group_0_commands, group_1_commands, group_2_commands, 
    (const char **) RESERVED_GROUP, (const char **) RESERVED_GROUP, 
    (const char **) NOTEXT_GROUP, (const char **) VENDOR_GROUP, 
    (const char **) VENDOR_GROUP
};

static const char reserved[] = "RESERVED";
static const char vendor[] = "VENDOR SPECIFIC";

static void print_opcodek(unsigned char opcode)
{
	const char **table = commands[ group(opcode) ];

	switch ((unsigned long) table) {
	case RESERVED_GROUP:
;
		break;
	case NOTEXT_GROUP:
;
		break;
	case VENDOR_GROUP:
;
		break;
	default:
		if (table[opcode & 0x1f] != unknown)
;
		else
;
		break;
	}
}

static void print_commandk (unsigned char *command)
{
	int i,s;
;
	print_opcodek(command[0]);
	/*printk(KERN_DEBUG "%s ", __func__);*/
	if ((command[0] >> 5) == 6 ||
	    (command[0] >> 5) == 7 ) {
		s = 12; /* vender specific */
	} else {
		s = COMMAND_SIZE(command[0]);
	}

	for ( i = 1; i < s; ++i) {
;
	}

	switch (s) {
	case 6:
//		printk("LBA=%d len=%d",
//		       (((unsigned int)command[1] & 0x0f) << 16) |
//		       ( (unsigned int)command[2]         <<  8) |
//		       ( (unsigned int)command[3]              ),
//		       (unsigned int)command[4]
;
		break;
	case 10:
//		printk("LBA=%d len=%d",
//		       ((unsigned int)command[2] << 24) |
//		       ((unsigned int)command[3] << 16) |
//		       ((unsigned int)command[4] <<  8) |
//		       ((unsigned int)command[5]      ),
//		       ((unsigned int)command[7] <<  8) |
//		       ((unsigned int)command[8]      )
;
		break;
	case 12:
//		printk("LBA=%d len=%d",
//		       ((unsigned int)command[2] << 24) |
//		       ((unsigned int)command[3] << 16) |
//		       ((unsigned int)command[4] <<  8) |
//		       ((unsigned int)command[5]      ),
//		       ((unsigned int)command[6] << 24) |
//		       ((unsigned int)command[7] << 16) |
//		       ((unsigned int)command[8] <<  8) |
//		       ((unsigned int)command[9]      )
;
		break;
	default:
		break;
	}
;
}

static void show_command(Scsi_Cmnd *SCpnt)
{
	print_commandk(SCpnt->cmnd);
}

static void show_busphase(unsigned char stat)
{
	switch(stat) {
	case BUSPHASE_COMMAND:
;
		break;
	case BUSPHASE_MESSAGE_IN:
;
		break;
	case BUSPHASE_MESSAGE_OUT:
;
		break;
	case BUSPHASE_DATA_IN:
;
		break;
	case BUSPHASE_DATA_OUT:
;
		break;
	case BUSPHASE_STATUS:
;
		break;
	case BUSPHASE_SELECT:
;
		break;
	default:
;
		break;
	}
}

static void show_autophase(unsigned short i)
{
;

	if(i & COMMAND_PHASE) {
;
	}
	if(i & DATA_IN_PHASE) {
;
	}
	if(i & DATA_OUT_PHASE) {
;
	}
	if(i & MSGOUT_PHASE) {
;
	}
	if(i & STATUS_PHASE) {
;
	}
	if(i & ILLEGAL_PHASE) {
;
	}
	if(i & BUS_FREE_OCCUER) {
;
	}
	if(i & MSG_IN_OCCUER) {
;
	}
	if(i & MSG_OUT_OCCUER) {
;
	}
	if(i & SELECTION_TIMEOUT) {
;
	}
	if(i & MSGIN_00_VALID) {
;
	}
	if(i & MSGIN_02_VALID) {
;
	}
	if(i & MSGIN_03_VALID) {
;
	}
	if(i & MSGIN_04_VALID) {
;
	}
	if(i & AUTOSCSI_BUSY) {
;
	}

;
}

static void nsp32_print_register(int base)
{
	if (!(NSP32_DEBUG_MASK & NSP32_SPECIAL_PRINT_REGISTER))
		return;

;
;
;
;
;
;
;
;
;
;
;
;
;
;
;
;
;
;
;
;
;
;

	if (0) {
;
;
;
	}
}

/* end */
