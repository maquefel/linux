// SPDX-License-Identifier: GPL-2.0
/*
 * EP93XX Device Tree boot support
 */
//#include <linux/kernel.h>
//#include <linux/init.h>
//#include <linux/io.h>

//#include <asm/mach/arch.h>
//#include <asm/mach/map.h>
//#include <asm/system_misc.h>
//#include <asm/proc-fns.h>

#include "ts72xx.c"

// #ifdef CONFIG_DEBUG_EP93XX
// /* This is needed for LL-debug/earlyprintk/debug-macro.S */
// static struct map_desc ep93xx_io_desc[] __initdata = {
// 	{
// 		.virtual = CONFIG_DEBUG_UART_VIRT,
// 		.pfn = __phys_to_pfn(CONFIG_DEBUG_UART_PHYS),
// 		.length = SZ_4K,
// 		.type = MT_DEVICE,
// 	},
// };
// 
// static void __init ep93xx_map_io(void)
// {
// 	iotable_init(ep93xx_io_desc, ARRAY_SIZE(ep93xx_io_desc));
// }
// #else
// #define ep93xx_map_io NULL
// #endif

// static void __init ts72xx_map_io(void)
// {
// 	ep93xx_map_io();
// 	iotable_init(ts72xx_io_desc, ARRAY_SIZE(ts72xx_io_desc));
// }

static void __init ep93xx_init_machine(void)
{
	// ep93xx_init_devices();
}

static const char *ep93xx_board_compat[] = {
	"cirrus,ep93xx",
	NULL,
};

// DT_MACHINE_START(EP93XX_DT, "EP93XX (Device Tree)")
// 	.map_io		= ep93xx_map_io,
// 	.init_machine	= ep93xx_init_machine,
// 	.dt_compat	= ep93xx_board_compat,
// MACHINE_END

DT_MACHINE_START(EP93XX_DT, "EP93XX (Device Tree)")
	.atag_offset	= 0x100,
	.nr_irqs	= NR_EP93XX_IRQS,
	.map_io		= ts72xx_map_io,
	.init_irq	= ep93xx_init_irq,
	.init_machine	= ts72xx_init_machine,
	.init_late	= ep93xx_init_late,
	.restart	= ep93xx_restart,
	.dt_compat	= ep93xx_board_compat,
MACHINE_END
