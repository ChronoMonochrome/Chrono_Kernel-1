/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Rickard Andersson <rickard.andersson@stericsson.com> for
 *         ST-Ericsson.
 * License terms: GNU General Public License (GPL) version 2
 *
 */

#include <linux/io.h>
#include <linux/percpu.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/nomadik.h>

#include <asm/hardware/gic.h>
#include <asm/processor.h>

#include <mach/hardware.h>
#include <mach/pm.h>

#define STABILIZATION_TIME 30 /* us */
#define GIC_FREEZE_DELAY 1 /* us */

#define PRCM_ARM_WFI_STANDBY_CPU0_WFI 0x8
#define PRCM_ARM_WFI_STANDBY_CPU1_WFI 0x10

/* Dual A9 core interrupt management unit registers */
#define PRCM_A9_MASK_REQ	(_PRCMU_BASE + 0x328)
#define PRCM_A9_MASK_REQ_PRCM_A9_MASK_REQ	0x1
#define PRCM_A9_MASK_ACK	(_PRCMU_BASE + 0x32c)

#define PRCM_ARMITMSK31TO0	(_PRCMU_BASE + 0x11c)
#define PRCM_ARMITMSK63TO32	(_PRCMU_BASE + 0x120)
#define PRCM_ARMITMSK95TO64	(_PRCMU_BASE + 0x124)
#define PRCM_ARMITMSK127TO96	(_PRCMU_BASE + 0x128)
#define PRCM_POWER_STATE_VAL	(_PRCMU_BASE + 0x25C)
#define PRCM_ARMITVAL31TO0	(_PRCMU_BASE + 0x260)
#define PRCM_ARMITVAL63TO32	(_PRCMU_BASE + 0x264)
#define PRCM_ARMITVAL95TO64	(_PRCMU_BASE + 0x268)
#define PRCM_ARMITVAL127TO96	(_PRCMU_BASE + 0x26C)

/* ARM WFI Standby signal register */
#define PRCM_ARM_WFI_STANDBY    (_PRCMU_BASE + 0x130)

/* IO force */
#define PRCM_IOCR		(_PRCMU_BASE + 0x310)
#define PRCM_IOCR_IOFORCE			0x1

static u32 u8500_gpio_banks[] = {U8500_GPIOBANK0_BASE,
				 U8500_GPIOBANK1_BASE,
				 U8500_GPIOBANK2_BASE,
				 U8500_GPIOBANK3_BASE,
				 U8500_GPIOBANK4_BASE,
				 U8500_GPIOBANK5_BASE,
				 U8500_GPIOBANK6_BASE,
				 U8500_GPIOBANK7_BASE,
				 U8500_GPIOBANK8_BASE};

static u32 u5500_gpio_banks[] = {U5500_GPIOBANK0_BASE,
				 U5500_GPIOBANK1_BASE,
				 U5500_GPIOBANK2_BASE,
				 U5500_GPIOBANK3_BASE,
				 U5500_GPIOBANK4_BASE,
				 U5500_GPIOBANK5_BASE,
				 U5500_GPIOBANK6_BASE,
				 U5500_GPIOBANK7_BASE};

static u32 ux500_gpio_wks[ARRAY_SIZE(u8500_gpio_banks)];

inline int ux500_pm_arm_on_ext_clk(bool leave_arm_pll_on)
{
	return 0;
}

/* Decouple GIC from the interrupt bus */
void ux500_pm_gic_decouple(void)
{
	writel(readl(PRCM_A9_MASK_REQ) | PRCM_A9_MASK_REQ_PRCM_A9_MASK_REQ,
		PRCM_A9_MASK_REQ);

	while (!readl(PRCM_A9_MASK_REQ))
		cpu_relax();

	/* TODO: Use the ack bit when possible */
	udelay(GIC_FREEZE_DELAY); /* Wait for the GIC to freeze */
}

/* Recouple GIC with the interrupt bus */
void ux500_pm_gic_recouple(void)
{
	writel((readl(PRCM_A9_MASK_REQ) & ~PRCM_A9_MASK_REQ_PRCM_A9_MASK_REQ),
	       PRCM_A9_MASK_REQ);

	/* TODO: Use the ack bit when possible */
}

#define GIC_NUMBER_REGS 5
bool ux500_pm_gic_pending_interrupt(void)
{
	u32 pr; /* Pending register */
	u32 er; /* Enable register */
	int i;

	/* 5 registers. STI & PPI not skipped */
	for (i = 0; i < GIC_NUMBER_REGS; i++) {

		pr = readl_relaxed(__io_address(U8500_GIC_DIST_BASE) +
				   GIC_DIST_PENDING_SET + i * 4);
		er = readl_relaxed(__io_address(U8500_GIC_DIST_BASE) +
				   GIC_DIST_ENABLE_SET + i * 4);

		if (pr & er)
			return true; /* There is a pending interrupt */
	}
	return false;
}

#define GIC_NUMBER_SPI_REGS 4
bool ux500_pm_prcmu_pending_interrupt(void)
{
	u32 it;
	u32 im;
	int i;

	for (i = 0; i < GIC_NUMBER_SPI_REGS; i++) { /* There are 4 registers */

		it = readl(PRCM_ARMITVAL31TO0 + i * 4);
		im = readl(PRCM_ARMITMSK31TO0 + i * 4);

		if (it & im)
			return true; /* There is a pending interrupt */
	}

	return false;
}

void ux500_pm_prcmu_set_ioforce(bool enable)
{
	if (enable)
		writel(readl(PRCM_IOCR) | PRCM_IOCR_IOFORCE, PRCM_IOCR);
	else
		writel(readl(PRCM_IOCR) & ~PRCM_IOCR_IOFORCE, PRCM_IOCR);
}

void ux500_pm_prcmu_copy_gic_settings(void)
{
	u32 er; /* Enable register */
	int i;

	for (i = 0; i < GIC_NUMBER_SPI_REGS; i++) { /* 4*32 SPI interrupts */
		/* +1 due to skip STI and PPI */
		er = readl_relaxed(__io_address(U8500_GIC_DIST_BASE) +
			   GIC_DIST_ENABLE_SET + (i + 1) * 4);
		writel(er, PRCM_ARMITMSK31TO0 + i * 4);
	}
}

void ux500_pm_gpio_save_wake_up_status(void)
{
	int num_banks;
	u32 *banks;
	int i;

	if (cpu_is_u5500()) {
		num_banks = ARRAY_SIZE(u5500_gpio_banks);
		banks = u5500_gpio_banks;
	} else {
		num_banks = ARRAY_SIZE(u8500_gpio_banks);
		banks = u8500_gpio_banks;
	}

	nmk_gpio_clocks_enable();

	for (i = 0; i < num_banks; i++)
		ux500_gpio_wks[i] = readl(__io_address(banks[i]) + NMK_GPIO_WKS);

	nmk_gpio_clocks_disable();
}

u32 ux500_pm_gpio_read_wake_up_status(unsigned int bank_num)
{
	if (WARN_ON(cpu_is_u5500() && bank_num >=
		    ARRAY_SIZE(u5500_gpio_banks)))
		return 0;

	if (WARN_ON(cpu_is_u8500() && bank_num >=
		    ARRAY_SIZE(u8500_gpio_banks)))
		return 0;

	return ux500_gpio_wks[bank_num];
}

/* Check if the other CPU is in WFI */
bool ux500_pm_other_cpu_wfi(void)
{
	if (smp_processor_id()) {
		/* We are CPU 1 => check if CPU0 is in WFI */
		if (readl(PRCM_ARM_WFI_STANDBY) &
		    PRCM_ARM_WFI_STANDBY_CPU0_WFI)
			return true;
	} else {
		/* We are CPU 0 => check if CPU1 is in WFI */
		if (readl(PRCM_ARM_WFI_STANDBY) &
		    PRCM_ARM_WFI_STANDBY_CPU1_WFI)
			return true;
	}

	return false;
}

/* PRCM_ACK_MB0_AP_PWRSTTR_STATUS */
#define DB8500_PRCMU_STATUS_REGISTER	0x801b8e08
#define DB5500_PRCMU_STATUS_REGISTER	0x80168f38

enum prcmu_idle_stat ux500_pm_prcmu_idle_stat(void)
{
	u32 val;
	void __iomem *prcmu_status_reg;

	if (cpu_is_u8500())
		prcmu_status_reg = __io_address(DB8500_PRCMU_STATUS_REGISTER);
	else if (cpu_is_u5500())
		prcmu_status_reg = __io_address(DB5500_PRCMU_STATUS_REGISTER);
	else
		ux500_unknown_soc();

	val = readl(prcmu_status_reg) & 0xff;

	return (enum prcmu_idle_stat)val;
}
