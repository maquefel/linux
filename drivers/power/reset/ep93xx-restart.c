// SPDX-License-Identifier: (GPL-2.0)
/*
 * Cirrus EP93xx SoC reset driver
 *
 * Copyright (C) 2021 Nikita Shubin <nikita.shubin@maquefel.me>
 */

#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>

#include <linux/ep93xx.h>

#define EP93XX_SYSCON_DEVCFG_SWRST	BIT(31)

static int ep93xx_restart_handle(struct notifier_block *this,
				 unsigned long mode, void *cmd)
{
	/* Issue the reboot */
	ep93xx_devcfg_set_clear(EP93XX_SYSCON_DEVCFG_SWRST, 0x00, EP93XX_SYSCON_DEVCFG);
	ep93xx_devcfg_set_clear(0x00, EP93XX_SYSCON_DEVCFG_SWRST, EP93XX_SYSCON_DEVCFG);
	
	mdelay(1000);
	
	pr_emerg("Unable to restart system\n");
	return NOTIFY_DONE;
}

static int ep93xx_reboot_probe(struct platform_device *pdev)
{
	struct notifier_block *res_han;
	struct device *dev = &pdev->dev;
	int err;

	res_han = devm_kzalloc(&pdev->dev, sizeof(*res_han), GFP_KERNEL);
	if (!res_han)
		return -ENOMEM;

	res_han->notifier_call = ep93xx_restart_handle;
	res_han->priority = 128;

	err = register_restart_handler(res_han);
	if (err)
		dev_err(dev, "can't register restart notifier (err=%d)\n", err);

	return err;
}

static const struct of_device_id ep93xx_reboot_of_match[] = {
	{
		.compatible = "cirrus,ep93xx-reboot",
	},
	{}
};

static struct platform_driver ep93xx_reboot_driver = {
	.probe = ep93xx_reboot_probe,
	.driver = {
		.name = "ep93xx-reboot",
		.of_match_table = ep93xx_reboot_of_match,
	},
};
builtin_platform_driver(ep93xx_reboot_driver);
