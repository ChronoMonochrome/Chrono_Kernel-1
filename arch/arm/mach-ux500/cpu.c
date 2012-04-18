/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Rabin Vincent <rabin.vincent@stericsson.com> for ST-Ericsson
 * Author: Lee Jones <lee.jones@linaro.org> for ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/mfd/dbx500-prcmu.h>
#include <linux/mfd/db8500-prcmu.h>
#include <linux/mfd/db5500-prcmu.h>
#include <linux/clksrc-dbx500-prcmu.h>
#include <linux/sys_soc.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/delay.h>

#include <asm/hardware/gic.h>
#include <asm/mach/map.h>
#include <asm/system_misc.h>

#include <mach/hardware.h>
#include <mach/setup.h>
#include <mach/devices.h>
#include <mach/reboot_reasons.h>
#include <mach/pm.h>

#include "clock.h"

void __iomem *_PRCMU_BASE;

static const struct of_device_id ux500_dt_irq_match[] = {
	{ .compatible = "arm,cortex-a9-gic", .data = gic_of_init, },
	{},
};

/*
 * The reboot reason string can be 255 characters long and the memory
 * in which we save the sw reset reason is 2 bytes. Therefore we need to
 * convert the string into a 16 bit pattern.
 *
 * See file reboot_reasons.h for conversion.
 */
static unsigned short map_cmd_to_code(const char *cmd)
{
	int i;

	if (cmd == NULL)
		/* normal reboot w/o argument */
		return SW_RESET_NO_ARGUMENT;

	/* Search through reboot reason list */
	for (i = 0; i < reboot_reasons_size; i++) {
		if (!strcmp(reboot_reasons[i].reason, cmd))
			return reboot_reasons[i].code;
	}

	/* No valid Reboot Reason found */
	return SW_RESET_CRASH;
}

void ux500_restart(char mode, const char *cmd)
{
	unsigned short reset_code;

	reset_code = map_cmd_to_code(cmd);
	prcmu_system_reset(reset_code);

	mdelay(1000);

	/*
	 * On 5500, the PRCMU firmware waits for up to 2 seconds for the modem
	 * to respond.
	 */
	if (cpu_is_u5500())
		mdelay(2000);

	printk(KERN_ERR "Reboot via PRCMU failed -- System halted\n");
	while (1)
		;
}

void __init ux500_init_irq(void)
{
	void __iomem *dist_base;
	void __iomem *cpu_base;

	if (cpu_is_u5500()) {
		dist_base = __io_address(U5500_GIC_DIST_BASE);
		cpu_base = __io_address(U5500_GIC_CPU_BASE);
	} else if (cpu_is_u8500() || cpu_is_u9540()) {
		dist_base = __io_address(U8500_GIC_DIST_BASE);
		cpu_base = __io_address(U8500_GIC_CPU_BASE);
	} else
		ux500_unknown_soc();

#ifdef CONFIG_OF
	if (of_have_populated_dt())
		of_irq_init(ux500_dt_irq_match);
	else
#endif
		gic_init(0, 29, dist_base, cpu_base);

	/*
	 * On WD reboot gic is in some cases decoupled.
	 * This will make sure that the GIC is correctly configured.
	 */
	ux500_pm_gic_recouple();

	/*
	 * Init clocks here so that they are available for system timer
	 * initialization.
	 */
	if (cpu_is_u5500())
		db5500_prcmu_early_init();
	if (cpu_is_u8500() || cpu_is_u9540())
		db8500_prcmu_early_init();

	arm_pm_restart = ux500_restart;
	clk_init();
}

static const char * __init ux500_get_machine(void)
{
	return kasprintf(GFP_KERNEL, "DB%4x", dbx500_partnumber());
}

static const char * __init ux500_get_family(void)
{
	return kasprintf(GFP_KERNEL, "ux500");
}

static const char * __init ux500_get_revision(void)
{
	unsigned int rev = dbx500_revision();

	if (rev == 0x01)
		return kasprintf(GFP_KERNEL, "%s", "ED");
	else if (rev >= 0xA0)
		return kasprintf(GFP_KERNEL, "%d.%d",
				 (rev >> 4) - 0xA + 1, rev & 0xf);

	return kasprintf(GFP_KERNEL, "%s", "Unknown");
}

static ssize_t ux500_get_process(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	if (dbx500_id.process == 0x00)
		return sprintf(buf, "Standard\n");

	return sprintf(buf, "%02xnm\n", dbx500_id.process);
}

static void __init soc_info_populate(struct soc_device_attribute *soc_dev_attr,
				     const char *soc_id)
{
	soc_dev_attr->soc_id   = soc_id;
	soc_dev_attr->machine  = ux500_get_machine();
	soc_dev_attr->family   = ux500_get_family();
	soc_dev_attr->revision = ux500_get_revision();
}

struct device_attribute ux500_soc_attr =
	__ATTR(process,  S_IRUGO, ux500_get_process,  NULL);

struct device * __init ux500_soc_device_init(const char *soc_id)
{
	struct device *parent;
	struct soc_device *soc_dev;
	struct soc_device_attribute *soc_dev_attr;

	soc_dev_attr = kzalloc(sizeof(*soc_dev_attr), GFP_KERNEL);
	if (!soc_dev_attr)
		return ERR_PTR(-ENOMEM);

	soc_info_populate(soc_dev_attr, soc_id);

	soc_dev = soc_device_register(soc_dev_attr);
	if (IS_ERR_OR_NULL(soc_dev)) {
	        kfree(soc_dev_attr);
		return NULL;
	}

	parent = soc_device_to_device(soc_dev);
	if (!IS_ERR_OR_NULL(parent))
		device_create_file(parent, &ux500_soc_attr);

	return parent;
}
