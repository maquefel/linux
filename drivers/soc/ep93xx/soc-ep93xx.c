// SPDX-License-Identifier: GPL-2.0
/*
 * Soc driver Cirrus EP93xx chips.
 * Copyright (C) 2021 Nikita Shubin <nikita.shubin@maquefel.me>
 */
#include <linux/ep93xx.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/of.h>

#define EP93XX_SWLOCK_MAGICK		0xaa
#define EP93XX_SYSCON_SWLOCK		0xc0
#define EP93XX_SYSCON_SYSCFG		0x9c
#define EP93XX_SYSCON_SYSCFG_REV_MASK	(0xf0000000)
#define EP93XX_SYSCON_SYSCFG_REV_SHIFT	(28)

#define EP93XX_CHIP_REV_D0	3
#define EP93XX_CHIP_REV_D1	4
#define EP93XX_CHIP_REV_E0	5
#define EP93XX_CHIP_REV_E1	6
#define EP93XX_CHIP_REV_E2	7

static DEFINE_SPINLOCK(syscon_swlock);
static struct regmap *map;

/* EP93xx System Controller software locked register write */
void ep93xx_syscon_swlocked_write(unsigned int val, unsigned int reg)
{
	unsigned long flags;

	spin_lock_irqsave(&syscon_swlock, flags);

	regmap_write(map, EP93XX_SYSCON_SWLOCK, EP93XX_SWLOCK_MAGICK);
	regmap_write(map, reg, val);

	spin_unlock_irqrestore(&syscon_swlock, flags);
}
EXPORT_SYMBOL_GPL(ep93xx_syscon_swlocked_write);

void ep93xx_devcfg_set_clear(unsigned int set_bits, unsigned int clear_bits, unsigned int reg)
{
	unsigned long flags;
	unsigned int val;

	spin_lock_irqsave(&syscon_swlock, flags);

	regmap_read(map, reg, &val);
	val &= ~clear_bits;
	val |= set_bits;
	regmap_write(map, EP93XX_SYSCON_SWLOCK, EP93XX_SWLOCK_MAGICK);
	regmap_write(map, reg, val);

	spin_unlock_irqrestore(&syscon_swlock, flags);
}
EXPORT_SYMBOL_GPL(ep93xx_devcfg_set_clear);

/**
 * ep93xx_chip_revision() - returns the EP93xx chip revision
 *
 */
unsigned int ep93xx_chip_revision(void)
{
	unsigned int val;

	regmap_read(map, EP93XX_SYSCON_SYSCFG, &val);
	val &= EP93XX_SYSCON_SYSCFG_REV_MASK;
	val >>= EP93XX_SYSCON_SYSCFG_REV_SHIFT;
	return val;
}
EXPORT_SYMBOL_GPL(ep93xx_chip_revision);

static const char __init *ep93xx_get_soc_rev(void)
{
	int rev = ep93xx_chip_revision();

	switch (rev) {
	case EP93XX_CHIP_REV_D0:
		return "D0";
	case EP93XX_CHIP_REV_D1:
		return "D1";
	case EP93XX_CHIP_REV_E0:
		return "E0";
	case EP93XX_CHIP_REV_E1:
		return "E1";
	case EP93XX_CHIP_REV_E2:
		return "E2";
	default:
		return "unknown";
	}
}

// TODO: move ChipId stuff here
// TODO: soc_device_attribute
static int __init ep93xx_soc_init(void)
{
	/* Multiplatform guard, only proceed on ep93xx */
	if (!of_machine_is_compatible("cirrus,ep93xx"))
		return 0;

	map = syscon_regmap_lookup_by_compatible("cirrus,ep93xx-syscon");
	if (IS_ERR(map))
		return PTR_ERR(map);

	pr_info("EP93xx SoC revision %s\n", ep93xx_get_soc_rev());

	return 0;
}
core_initcall(ep93xx_soc_init);
