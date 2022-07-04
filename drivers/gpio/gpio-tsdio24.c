/*
 * Technologic Systems TS-DIO24 supprt.
 *
 * Copyright (C) 2010 Matthieu Crapet <mcrapet@gmail.com>
 * Copyright (C) 2020 Nikita Shubin <nikita.shubin@maquefel.me>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Watch out: all gpio of a same port have the same direction.
 * gpiolib index (add "chip.base" number):
 *	00: A0 (pin 47)		08: B0 (pin 31)		16: C0 (pin 15)
 *	01: A1 (pin 45)		09: B1 (pin 29)		17: C1 (pin 13)
 *	02: A2 (pin 43)		10: B2 (pin 27)		18: C2 (pin 11)
 *	03: A3 (pin 41)		11: B3 (pin 25)		19: C3 (pin 9)
 *	04: A4 (pin 39)		12: B4 (pin 23)		20: C4 (pin 7)
 *	05: A5 (pin 37)		13: B5 (pin 21)		21: C5 (pin 5)
 *	06: A6 (pin 35)		14: B6 (pin 19)		22: C6 (pin 3)
 *	07: A7 (pin 33)		15: B7 (pin 17)		23: C7 (pin 1)
 *      ---------------         ---------------         ---------------
 *        output only          TTL in / 24ma out       TTL in / 24ma out
 *       A0-A3: 48ma out                                irq for C0-C3
 */

#define DEBUG

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/isa.h>

/* PLD register space size */
#define TSDIO24_PLD_ADDRESS 0x100

/* PLD register space size */
#define TSDIO24_PLD_SIZE 8

struct tsdio24_chip {
	unsigned		dir_bit;
	struct gpio_chip	gc;
};

struct tsdio24 {
	void __iomem *mmio_base;
	struct tsdio24_chip tc[3];
};

#define TS_DIO24_GPIO_BANK(_label, _data, _dir_bit, _output_only, _names) \
	{									\
		.label			= _label,				\
		.data			= _data,				\
		.dir_bit		= _dir_bit,				\
		.output_only		= _output_only,				\
		.names			= _names,				\
	}

struct tsdio24_bank {
	const char		*label;
	int			data;
	unsigned		dir_bit;
	bool			output_only;
	const char *const	*names;
};

static const char * const tsdio24_bank_a_names[] =
	{"A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7"};

static const char * const tsdio24_bank_b_names[] =
	{"B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7"};

static const char * const tsdio24_bank_c_names[] =
	{"C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7"};

static struct tsdio24_bank tsdio24_banks[] = {
	TS_DIO24_GPIO_BANK("tsdio24-A", 0x05, 0, true, tsdio24_bank_a_names),
	TS_DIO24_GPIO_BANK("tsdio24-B", 0x06, 1, false, tsdio24_bank_b_names),
	TS_DIO24_GPIO_BANK("tsdio24-C", 0x07, 0, false, tsdio24_bank_c_names),
};

/* Board identifier: first byte must be 'T' */
#define is_tsdio24_present(__iomem, __offset) \
	(__raw_readb(__iomem + __offset) == 0x54)

#define TS_DIO24_GPIO_NR	24

/* Registers offset */
#define GPIO_INT	3
#define GPIO_DIR	4
#define GPIO_DATA	5

#define TS72XX_OPTIONS_PHYS_BASE 0x11e00000

static int tsdio24_get_dir_a(struct gpio_chip *gc, unsigned int gpio)
{
	return GPIO_LINE_DIRECTION_OUT;
}

static int tsdio24_set_dir_in_err(struct gpio_chip *gc, unsigned int offset)
{
	return -EINVAL;
}

static int tsdio24_set_dir_out_a(struct gpio_chip *gc, unsigned int offset, int value)
{
	gc->set(gc, offset, value);
	return 0;
}

static int tsdio24_set_dir_in(struct gpio_chip *gc, unsigned int offset)
{
	struct tsdio24_chip* tc = gpiochip_get_data(gc);
	unsigned long flags;
	u8 reg;

	spin_lock_irqsave(&gc->bgpio_lock, flags);

	reg = readb(gc->reg_dir_out);

	reg &= ~BIT(tc->dir_bit);

	writeb(reg, gc->reg_dir_out);

	spin_unlock_irqrestore(&gc->bgpio_lock, flags);

	return 0;
}

static int tsdio24_set_dir_out(struct gpio_chip *gc, unsigned int offset, int val)
{
	struct tsdio24_chip* tc = gpiochip_get_data(gc);
	unsigned long flags;
	u8 reg;

	spin_lock_irqsave(&gc->bgpio_lock, flags);

	reg = readb(gc->reg_dir_out);

	reg |= BIT(tc->dir_bit);

	writeb(reg, gc->reg_dir_out);

	/** @derived from bgpio_set */
	if (val)
		gc->bgpio_data |= BIT(offset);
	else
		gc->bgpio_data &= ~BIT(offset);

	gc->write_reg(gc->reg_dat, gc->bgpio_data);

	spin_unlock_irqrestore(&gc->bgpio_lock, flags);

	return 0;
}

static int tsdio24_get_dir(struct gpio_chip *gc, unsigned int gpio)
{
	struct tsdio24_chip* tc = gpiochip_get_data(gc);
	u8 reg = readb(gc->reg_dir_out);

	if(reg & BIT(tc->dir_bit))
		return GPIO_LINE_DIRECTION_OUT;

	return GPIO_LINE_DIRECTION_IN;
}

static int tsdio24_add_chip(void __iomem* base,
			struct device *dev,
			struct tsdio24_chip *tc,
			struct tsdio24_bank *tb)
{
	struct gpio_chip *gc = &tc->gc;
	void __iomem *data = base + tb->data;
	void __iomem *dir = base + GPIO_DIR;
	int err;

	err = bgpio_init(gc, dev, 1, data, NULL, NULL, NULL, NULL, 0);
	if (err)
		return err;

	gc->label = tb->label;
	gc->names = tb->names;

	if(tb->output_only) {
		dev_dbg(dev, "marking %s as output only\n", gc->label);
		gc->get_direction = tsdio24_get_dir_a;
		gc->direction_input = tsdio24_set_dir_in_err;
		gc->direction_output = tsdio24_set_dir_out_a;
	} else {
		dev_dbg(dev, "marking %s as input/output\n", gc->label);
		gc->reg_dir_out = dir;
		gc->get_direction = tsdio24_get_dir;
		gc->direction_input = tsdio24_set_dir_in;
		gc->direction_output = tsdio24_set_dir_out;
	}

	return devm_gpiochip_add_data(dev, gc, tc);
}

static int tsdio24_probe(struct platform_device *pdev)
{
	struct tsdio24 *dio24;
	void __iomem *base;
	int i;

	dio24 = devm_kzalloc(&pdev->dev, sizeof(struct tsdio24), GFP_KERNEL);
	if (!dio24)
		return -ENOMEM;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);
 
	if (!is_tsdio24_present(base, 0)) {
		dev_err(&pdev->dev, "not detected\n");
		return -ENXIO;
	}
 
	for (i = 0; i < ARRAY_SIZE(tsdio24_banks); i++) {
		struct tsdio24_bank *tb = &tsdio24_banks[i];
		struct tsdio24_chip *tc = &dio24->tc[i];

		if (tsdio24_add_chip(base, &pdev->dev, tc, tb))
			dev_warn(&pdev->dev, "Unable to add gpio bank %s\n", tb->label);
	}

	return 0;
}

static const struct of_device_id tsdio24_gpio_match[] = {
	{ .compatible = "technologic,tsdio24-gpio" },
	{ /* end of table */ },
};
MODULE_DEVICE_TABLE(of, tsdio24_gpio_match);

static struct platform_driver tsdio24_driver = {
	.driver         = {
		.name   = "ts72xx-dio24",
		.of_match_table = tsdio24_gpio_match,
	},
	.probe          = tsdio24_probe,
};
module_platform_driver(tsdio24_driver);

MODULE_AUTHOR("Matthieu Crapet <mcrapet@gmail.com>");
MODULE_AUTHOR("Nikita Shubin <nikita.shubin@maquefel.me>");
MODULE_DESCRIPTION("TS-DIO24 driver");
MODULE_LICENSE("GPL v2");
