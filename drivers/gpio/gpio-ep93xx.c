// SPDX-License-Identifier: GPL-2.0
/*
 * Generic EP93xx GPIO handling
 *
 * Copyright (c) 2008 Ryan Mallon
 * Copyright (c) 2011 H Hartley Sweeten <hsweeten@visionengravers.com>
 *
 * Based on code originally from:
 *  linux/arch/arm/mach-ep93xx/core.c
 */
#define DEBUG
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/gpio/driver.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>

struct ep93xx_gpio_irq_chip {
	struct irq_chip ic;
	void __iomem *base;
	u8 int_unmasked;
	u8 int_enabled;
	u8 int_type1;
	u8 int_type2;
	u8 int_debounce;
};

struct ep93xx_gpio_chip {
	void __iomem			*base;
	struct gpio_chip		gc;
	struct ep93xx_gpio_irq_chip	*eic;
};

#define to_ep93xx_gpio_chip(x) container_of(x, struct ep93xx_gpio_chip, gc)

static struct ep93xx_gpio_irq_chip *to_ep93xx_gpio_irq_chip(struct gpio_chip *gc)
{
	struct ep93xx_gpio_chip *egc = to_ep93xx_gpio_chip(gc);

	return egc->eic;
}

/*************************************************************************
 * Interrupt handling for EP93xx on-chip GPIOs
 *************************************************************************/
#define EP93XX_INT_TYPE1_OFFSET		0x00
#define EP93XX_INT_TYPE2_OFFSET		0x04
#define EP93XX_INT_EOI_OFFSET		0x08
#define EP93XX_INT_EN_OFFSET		0x0c
#define EP93XX_INT_STATUS_OFFSET	0x10
#define EP93XX_INT_RAW_STATUS_OFFSET	0x14
#define EP93XX_INT_DEBOUNCE_OFFSET	0x18

static void ep93xx_gpio_update_int_params(struct ep93xx_gpio_irq_chip *eic)
{
	dev_dbg(eic->ic.parent_device, "ep93xx_gpio_update_int_params 0x%px\n", eic->base);
	
	writeb_relaxed(0, eic->base + EP93XX_INT_EN_OFFSET);

	writeb_relaxed(eic->int_type2,
		       eic->base + EP93XX_INT_TYPE2_OFFSET);

	writeb_relaxed(eic->int_type1,
		       eic->base + EP93XX_INT_TYPE1_OFFSET);

	writeb(eic->int_unmasked & eic->int_enabled,
		eic->base + EP93XX_INT_EN_OFFSET);
}

static void ep93xx_gpio_int_debounce(struct gpio_chip *gc,
				     unsigned int offset, bool enable)
{
	struct ep93xx_gpio_irq_chip *eic = to_ep93xx_gpio_irq_chip(gc);
	int port_mask = BIT(offset);

	if (enable)
		eic->int_debounce |= port_mask;
	else
		eic->int_debounce &= ~port_mask;

	writeb(eic->int_debounce,
	       eic->base + EP93XX_INT_DEBOUNCE_OFFSET);
}

static u32 ep93xx_gpio_ab_irq_handler(struct gpio_chip *gc)
{
	struct ep93xx_gpio_irq_chip *eic = to_ep93xx_gpio_irq_chip(gc);
	unsigned long stat;
	int offset;
	
	stat = readb(eic->base + EP93XX_INT_STATUS_OFFSET);
	
	for_each_set_bit(offset, &stat, 8)
		generic_handle_irq(irq_find_mapping(gc->irq.domain,
						    offset));
	
	return stat;
}

static irqreturn_t ep93xx_ab_irq_handler(int irq, void *dev_id)
{
	return IRQ_RETVAL(ep93xx_gpio_ab_irq_handler(dev_id));
}

static void ep93xx_gpio_f_irq_handler(struct irq_desc *desc)
{
	struct irq_chip *irqchip = irq_desc_get_chip(desc);
	struct gpio_chip *gc = irq_desc_get_handler_data(desc);
	struct gpio_irq_chip *gic = &gc->irq;
	unsigned int parent = irq_desc_get_irq(desc);
	unsigned int i;

	chained_irq_enter(irqchip, desc);
	for (i = 0; i < gic->num_parents; i++)
		if (gic->parents[i] == parent)
			break;

	if (i < gic->num_parents)
		generic_handle_irq(irq_find_mapping(gc->irq.domain, i));

	chained_irq_exit(irqchip, desc);
}

static void ep93xx_gpio_irq_ack(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct ep93xx_gpio_irq_chip *eic = to_ep93xx_gpio_irq_chip(gc);
	int port_mask = BIT(irqd_to_hwirq(d));

	if (irqd_get_trigger_type(d) == IRQ_TYPE_EDGE_BOTH) {
		eic->int_type2 ^= port_mask; /* switch edge direction */
		ep93xx_gpio_update_int_params(eic);
	}

	writeb(port_mask, eic->base + EP93XX_INT_EOI_OFFSET);
}

static void ep93xx_gpio_irq_mask_ack(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct ep93xx_gpio_irq_chip *eic = to_ep93xx_gpio_irq_chip(gc);
	int port_mask = BIT(irqd_to_hwirq(d));

	if (irqd_get_trigger_type(d) == IRQ_TYPE_EDGE_BOTH)
		eic->int_type2 ^= port_mask; /* switch edge direction */

	eic->int_unmasked &= ~port_mask;
	ep93xx_gpio_update_int_params(eic);

	writeb(port_mask, eic->base + EP93XX_INT_EOI_OFFSET);
}

static void ep93xx_gpio_irq_mask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct ep93xx_gpio_irq_chip *eic = to_ep93xx_gpio_irq_chip(gc);

	eic->int_unmasked &= ~BIT(irqd_to_hwirq(d));
	ep93xx_gpio_update_int_params(eic);
}

static void ep93xx_gpio_irq_unmask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct ep93xx_gpio_irq_chip *eic = to_ep93xx_gpio_irq_chip(gc);

	eic->int_unmasked |= BIT(irqd_to_hwirq(d));
	ep93xx_gpio_update_int_params(eic);
}

/*
 * gpio_int_type1 controls whether the interrupt is level (0) or
 * edge (1) triggered, while gpio_int_type2 controls whether it
 * triggers on low/falling (0) or high/rising (1).
 */
static int ep93xx_gpio_irq_type(struct irq_data *d, unsigned int type)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct ep93xx_gpio_irq_chip *eic = to_ep93xx_gpio_irq_chip(gc);
	int offset = irqd_to_hwirq(d);
	int port_mask = BIT(offset);
	irq_flow_handler_t handler;

	dev_dbg(eic->ic.parent_device, "ep93xx_gpio_irq_type : %d, offset=%d\n", d->irq, offset);

	gc->direction_input(gc, offset);

	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		eic->int_type1 |= port_mask;
		eic->int_type2 |= port_mask;
		handler = handle_edge_irq;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		eic->int_type1 |= port_mask;
		eic->int_type2 &= ~port_mask;
		handler = handle_edge_irq;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		eic->int_type1 &= ~port_mask;
		eic->int_type2 |= port_mask;
		handler = handle_level_irq;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		eic->int_type1 &= ~port_mask;
		eic->int_type2 &= ~port_mask;
		handler = handle_level_irq;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		eic->int_type1 |= port_mask;
		/* set initial polarity based on current input level */
		if (gc->get(gc, offset))
			eic->int_type2 &= ~port_mask; /* falling */
		else
			eic->int_type2 |= port_mask; /* rising */
		handler = handle_edge_irq;
		break;
	default:
		return -EINVAL;
	}

	irq_set_handler_locked(d, handler);

	eic->int_enabled |= port_mask;

	ep93xx_gpio_update_int_params(eic);

	return 0;
}

static int ep93xx_gpio_set_config(struct gpio_chip *gc, unsigned offset,
				  unsigned long config)
{
	u32 debounce;

	if (pinconf_to_config_param(config) != PIN_CONFIG_INPUT_DEBOUNCE)
		return -ENOTSUPP;

	debounce = pinconf_to_config_argument(config);
	ep93xx_gpio_int_debounce(gc, offset, debounce ? true : false);

	return 0;
}

static void ep93xx_init_irq_chip(struct device *dev, struct irq_chip *ic)
{
	ic->irq_ack = ep93xx_gpio_irq_ack;
	ic->irq_mask_ack = ep93xx_gpio_irq_mask_ack;
	ic->irq_mask = ep93xx_gpio_irq_mask;
	ic->irq_unmask = ep93xx_gpio_irq_unmask;
	ic->irq_set_type = ep93xx_gpio_irq_type;
}

static int ep93xx_setup_irqs(struct platform_device *pdev,
			     struct ep93xx_gpio_chip *egc)
{
	struct gpio_chip *gc = &egc->gc;
	struct device *dev = &pdev->dev;
	struct gpio_irq_chip *girq = &gc->irq;
	struct irq_chip *ic;
	int ret, irq, i = 0;
	const char *label = 0;
	void __iomem *intr = devm_platform_ioremap_resource_byname(pdev, "intr");
	
	if (IS_ERR(intr))
		return PTR_ERR(intr);
	
	/* TODO: additional irq check */
	gc->set_config = ep93xx_gpio_set_config;
	egc->eic = devm_kcalloc(dev, 1,
				sizeof(*egc->eic),
				GFP_KERNEL);
	if (!egc->eic)
		return -ENOMEM;
	
	egc->eic->base = intr;
	ic = &egc->eic->ic;
	
	ret = device_property_read_string(dev, "chip-label", &label);
	if (ret)
		label = dev_name(dev);

	ic->name = devm_kasprintf(dev, GFP_KERNEL, "gpio-irq-%s", label);
	if (!ic->name)
		return -ENOMEM;

	ep93xx_init_irq_chip(dev, ic);
	girq->chip = ic;
	girq->num_parents = platform_irq_count(pdev);
	girq->parents = devm_kcalloc(dev, girq->num_parents,
				   sizeof(*girq->parents),
				   GFP_KERNEL);
	if (!girq->parents)
		return -ENOMEM;

	if (girq->num_parents == 1) { /* A/B irqchips */
		irq = platform_get_irq(pdev, 0);
		ret = devm_request_irq(dev, irq,
				ep93xx_ab_irq_handler,
				IRQF_SHARED, ic->name, gc);
		if (ret) {
			dev_err(dev, "error requesting IRQ : %d\n", irq);
			return ret;
		}
		
		girq->parents[0] = irq;
	} else { /* F irqchip */
		girq->parent_handler = ep93xx_gpio_f_irq_handler;

		for (i = 0; i < girq->num_parents; i++) {
			irq = platform_get_irq(pdev, i);
			if (irq <= 0)
				continue;

			girq->parents[i] = irq;
		}

		girq->map = girq->parents;
	}

	girq->default_type = IRQ_TYPE_NONE;
	girq->handler = handle_bad_irq;

	return 0;
}

static int ep93xx_gpio_probe(struct platform_device *pdev)
{
	struct ep93xx_gpio_chip *egc;
	struct gpio_chip *gc;
	void __iomem *data;
	void __iomem *dir;
	const char *name;
	int ret;

	egc = devm_kzalloc(&pdev->dev, sizeof(*egc), GFP_KERNEL);
	if (!egc)
		return -ENOMEM;

	data = devm_platform_ioremap_resource_byname(pdev, "data");
	if (IS_ERR(data))
		return PTR_ERR(data);

	dir = devm_platform_ioremap_resource_byname(pdev, "dir");
	if (IS_ERR(dir))
		return PTR_ERR(dir);
	gc = &egc->gc;
	ret = bgpio_init(gc, &pdev->dev, 1, data, NULL, NULL, dir, NULL, 0);
	if (ret) {
		dev_err(&pdev->dev, "unable to init generic GPIO\n");
		return ret;
	}

	ret = device_property_read_string(&pdev->dev, "chip-label", &name);
	if (ret)
		name = dev_name(&pdev->dev);

	gc->label = name;

	if (platform_irq_count(pdev) > 0) {
		dev_dbg(&pdev->dev, "setting up irqs for %s\n", dev_name(&pdev->dev));
		ret = ep93xx_setup_irqs(pdev, egc);
		if (ret)
			dev_err(&pdev->dev, "setup irqs failed for %s\n", dev_name(&pdev->dev));
	}

	return devm_gpiochip_add_data(&pdev->dev, gc, egc);
}

static const struct of_device_id ep93xx_gpio_match[] = {
	{ .compatible = "cirrus,ep93xx-gpio" },
	{ /* end of table */ },
};
MODULE_DEVICE_TABLE(of, ep93xx_gpio_match);

static struct platform_driver ep93xx_gpio_driver = {
	.driver		= {
		.name	= "gpio-ep93xx",
		.of_match_table = ep93xx_gpio_match,
	},
	.probe		= ep93xx_gpio_probe,
};

static int __init ep93xx_gpio_init(void)
{
	return platform_driver_register(&ep93xx_gpio_driver);
}
postcore_initcall(ep93xx_gpio_init);

MODULE_AUTHOR("Ryan Mallon <ryan@bluewatersys.com> "
		"H Hartley Sweeten <hsweeten@visionengravers.com>");
MODULE_DESCRIPTION("EP93XX GPIO driver");
MODULE_LICENSE("GPL");
