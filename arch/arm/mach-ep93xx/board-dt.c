// SPDX-License-Identifier: GPL-2.0
/*
 * EP93XX Device Tree boot support
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/system_misc.h>
#include <asm/proc-fns.h>

static const char *ep93xx_board_compat[] = {
	"cirrus,ep93xx",
	NULL,
};

DT_MACHINE_START(EP93XX_DT, "EP93XX (Device Tree)")
	.atag_offset	= 0x100,
	.dt_compat	= ep93xx_board_compat,
MACHINE_END
