// SPDX-License-Identifier: GPL-2.0-only
/*
 * Technologic Systems TS72xx NAND driver platform code
 * Copyright (C) 2021 Nikita Shubin <nikita.shubin@maquefel.me>
 */

#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/rawnand.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/platnand.h>
#include <linux/of.h>
#include <linux/of_platform.h>

#define TS72XX_NAND_CONTROL_ADDR_LINE	22	/* 0xN0400000 */
#define TS72XX_NAND_BUSY_ADDR_LINE	23	/* 0xN0800000 */

static void ts72xx_nand_hwcontrol(struct nand_chip *chip,
				  int cmd, unsigned int ctrl)
{
	if (ctrl & NAND_CTRL_CHANGE) {
		void __iomem *addr = chip->legacy.IO_ADDR_R;
		unsigned char bits;

		addr += (1 << TS72XX_NAND_CONTROL_ADDR_LINE);

		bits = __raw_readb(addr) & ~0x07;
		bits |= (ctrl & NAND_NCE) << 2;	/* bit 0 -> bit 2 */
		bits |= (ctrl & NAND_CLE);	/* bit 1 -> bit 1 */
		bits |= (ctrl & NAND_ALE) >> 2;	/* bit 2 -> bit 0 */

		__raw_writeb(bits, addr);
	}

	if (cmd != NAND_CMD_NONE)
		__raw_writeb(cmd, chip->legacy.IO_ADDR_W);
}

static int ts72xx_nand_device_ready(struct nand_chip *chip)
{
	void __iomem *addr = chip->legacy.IO_ADDR_R;

	addr += (1 << TS72XX_NAND_BUSY_ADDR_LINE);

	return !!(__raw_readb(addr) & 0x20);
}

static struct platform_nand_data ts72xx_nand_data = {
	.chip = {
		.nr_chips	= 1,
		.chip_offset	= 0,
		.chip_delay	= 15,
	},
	.ctrl = {
		.cmd_ctrl	= ts72xx_nand_hwcontrol,
		.dev_ready	= ts72xx_nand_device_ready,
	},
};

static int __init ts72xx_nand_setup(void)
{
	struct device_node *np;
	struct platform_device *pdev;

	/* Multiplatform guard, only proceed on ts7250 */
	if (!of_machine_is_compatible("technologic,ts7250"))
		return 0;

	np = of_find_compatible_node(NULL, NULL, "technologic,ts72xx-nand");
	if (!np) {
		pr_err("%s : nand device tree node not found.\n", __func__);
		return -EINVAL;
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		pr_err("%s : nand device not found.\n", __func__);
		return -EINVAL;
	}

	pdev->dev.platform_data = &ts72xx_nand_data;
	put_device(&pdev->dev);
	of_node_put(np);

	return 0;
}

subsys_initcall(ts72xx_nand_setup);
