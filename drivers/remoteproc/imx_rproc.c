// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017 Pengutronix, Oleksij Rempel <kernel@pengutronix.de>
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/remoteproc.h>
#include <linux/mailbox_client.h>

#include "remoteproc_internal.h"

#define IMX7D_SRC_SCR			0x0C
#define IMX7D_ENABLE_M4			BIT(3)
#define IMX7D_SW_M4P_RST		BIT(2)
#define IMX7D_SW_M4C_RST		BIT(1)
#define IMX7D_SW_M4C_NON_SCLR_RST	BIT(0)

#define IMX7D_M4_RST_MASK		(IMX7D_ENABLE_M4 | IMX7D_SW_M4P_RST \
					 | IMX7D_SW_M4C_RST \
					 | IMX7D_SW_M4C_NON_SCLR_RST)

#define IMX7D_M4_START			(IMX7D_ENABLE_M4 | IMX7D_SW_M4P_RST \
					 | IMX7D_SW_M4C_RST)
#define IMX7D_M4_STOP			IMX7D_SW_M4C_NON_SCLR_RST

/* Address: 0x020D8000 */
#define IMX6SX_SRC_SCR			0x00
#define IMX6SX_ENABLE_M4		BIT(22)
#define IMX6SX_SW_M4P_RST		BIT(12)
#define IMX6SX_SW_M4C_NON_SCLR_RST	BIT(4)
#define IMX6SX_SW_M4C_RST		BIT(3)

#define IMX6SX_M4_START			(IMX6SX_ENABLE_M4 | IMX6SX_SW_M4P_RST \
					 | IMX6SX_SW_M4C_RST)
#define IMX6SX_M4_STOP			IMX6SX_SW_M4C_NON_SCLR_RST
#define IMX6SX_M4_RST_MASK		(IMX6SX_ENABLE_M4 | IMX6SX_SW_M4P_RST \
					 | IMX6SX_SW_M4C_NON_SCLR_RST \
					 | IMX6SX_SW_M4C_RST)

#define IMX7D_RPROC_MEM_MAX		8

#define IMX_BOOT_PC			0x4

#define IMX_MBOX_NB_VQ			2
#define IMX_MBOX_NB_MBX		2

#define IMX_MBX_VQ0		"vq0"
#define IMX_MBX_VQ1		"vq1"

/**
 * struct imx_rproc_mem - slim internal memory structure
 * @cpu_addr: MPU virtual address of the memory region
 * @sys_addr: Bus address used to access the memory region
 * @size: Size of the memory region
 */
struct imx_rproc_mem {
	void __iomem *cpu_addr;
	phys_addr_t sys_addr;
	size_t size;
};

/* att flags */
/* M4 own area. Can be mapped at probe */
#define ATT_OWN		BIT(1)

/* address translation table */
struct imx_rproc_att {
	u32 da;	/* device address (From Cortex M4 view)*/
	u32 sa;	/* system bus address */
	u32 size; /* size of reg range */
	int flags;
};

struct imx_rproc_dcfg {
	u32				src_reg;
	u32				src_mask;
	u32				src_start;
	u32				src_stop;
	const struct imx_rproc_att	*att;
	size_t				att_size;
};

struct imx_mbox {
	const unsigned char name[10];
	struct mbox_chan *chan;
	struct mbox_client client;
	struct work_struct vq_work;
	int vq_id;
};

struct imx_rproc {
	struct device			*dev;
	struct regmap			*regmap;
	struct rproc			*rproc;
	const struct imx_rproc_dcfg	*dcfg;
	struct imx_rproc_mem		mem[IMX7D_RPROC_MEM_MAX];
	struct clk			*clk;
	void __iomem			*bootreg;
	struct imx_mbox mb[IMX_MBOX_NB_MBX];
	struct workqueue_struct *workqueue;
};

static const struct imx_rproc_att imx_rproc_att_imx7d[] = {
	/* dev addr , sys addr  , size	    , flags */
	/* OCRAM_S (M4 Boot code) - alias */
	{ 0x00000000, 0x00180000, 0x00008000, 0 },
	/* OCRAM_S (Code) */
	{ 0x00180000, 0x00180000, 0x00008000, ATT_OWN },
	/* OCRAM (Code) - alias */
	{ 0x00900000, 0x00900000, 0x00020000, 0 },
	/* OCRAM_EPDC (Code) - alias */
	{ 0x00920000, 0x00920000, 0x00020000, 0 },
	/* OCRAM_PXP (Code) - alias */
	{ 0x00940000, 0x00940000, 0x00008000, 0 },
	/* TCML (Code) */
	{ 0x1FFF8000, 0x007F8000, 0x00008000, ATT_OWN },
	/* DDR (Code) - alias, first part of DDR (Data) */
	{ 0x10000000, 0x80000000, 0x0FFF0000, 0 },

	/* TCMU (Data) */
	{ 0x20000000, 0x00800000, 0x00008000, ATT_OWN },
	/* OCRAM (Data) */
	{ 0x20200000, 0x00900000, 0x00020000, 0 },
	/* OCRAM_EPDC (Data) */
	{ 0x20220000, 0x00920000, 0x00020000, 0 },
	/* OCRAM_PXP (Data) */
	{ 0x20240000, 0x00940000, 0x00008000, 0 },
	/* DDR (Data) */
	{ 0x80000000, 0x80000000, 0x60000000, 0 },
};

static const struct imx_rproc_att imx_rproc_att_imx6sx[] = {
	/* dev addr , sys addr  , size	    , flags */
	/* TCML (M4 Boot Code) - alias */
	{ 0x00000000, 0x007F8000, 0x00008000, 0 },
	/* OCRAM_S (Code) */
	{ 0x00180000, 0x008F8000, 0x00004000, 0 },
	/* OCRAM_S (Code) - alias */
	{ 0x00180000, 0x008FC000, 0x00004000, 0 },
	/* TCML (Code) */
	{ 0x1FFF8000, 0x007F8000, 0x00008000, ATT_OWN },
	/* DDR (Code) - alias, first part of DDR (Data) */
	{ 0x10000000, 0x80000000, 0x0FFF8000, 0 },

	/* TCMU (Data) */
	{ 0x20000000, 0x00800000, 0x00008000, ATT_OWN },
	/* OCRAM_S (Data) - alias? */
	{ 0x208F8000, 0x008F8000, 0x00004000, 0 },
	/* DDR (Data) */
	{ 0x80000000, 0x80000000, 0x60000000, 0 },
};

static const struct imx_rproc_dcfg imx_rproc_cfg_imx7d = {
	.src_reg	= IMX7D_SRC_SCR,
	.src_mask	= IMX7D_M4_RST_MASK,
	.src_start	= IMX7D_M4_START,
	.src_stop	= IMX7D_M4_STOP,
	.att		= imx_rproc_att_imx7d,
	.att_size	= ARRAY_SIZE(imx_rproc_att_imx7d),
};

static const struct imx_rproc_dcfg imx_rproc_cfg_imx6sx = {
	.src_reg	= IMX6SX_SRC_SCR,
	.src_mask	= IMX6SX_M4_RST_MASK,
	.src_start	= IMX6SX_M4_START,
	.src_stop	= IMX6SX_M4_STOP,
	.att		= imx_rproc_att_imx6sx,
	.att_size	= ARRAY_SIZE(imx_rproc_att_imx6sx),
};

static int imx_rproc_start(struct rproc *rproc)
{
	struct imx_rproc *priv = rproc->priv;
	const struct imx_rproc_dcfg *dcfg = priv->dcfg;
	struct device *dev = priv->dev;
	int ret;

	/* write entry point to program counter */
	writel(rproc->bootaddr, priv->bootreg);

	ret = regmap_update_bits(priv->regmap, dcfg->src_reg,
				 dcfg->src_mask, dcfg->src_start);
	if (ret)
		dev_err(dev, "Failed to enable M4!\n");

	dev_info(&rproc->dev, "Started from 0x%x\n", rproc->bootaddr);

	return ret;
}

static int imx_rproc_stop(struct rproc *rproc)
{
	struct imx_rproc *priv = rproc->priv;
	const struct imx_rproc_dcfg *dcfg = priv->dcfg;
	struct device *dev = priv->dev;
	int ret;

	ret = regmap_update_bits(priv->regmap, dcfg->src_reg,
				 dcfg->src_mask, dcfg->src_stop);
	if (ret)
		dev_err(dev, "Failed to stop M4!\n");

	/* clear entry points */
	writel(0, priv->bootreg);

	return ret;
}

static int imx_rproc_da_to_sys(struct imx_rproc *priv, u64 da,
			       size_t len, u64 *sys)
{
	const struct imx_rproc_dcfg *dcfg = priv->dcfg;
	int i;

	/* parse address translation table */
	for (i = 0; i < dcfg->att_size; i++) {
		const struct imx_rproc_att *att = &dcfg->att[i];

		if (da >= att->da && da + len < att->da + att->size) {
			unsigned int offset = da - att->da;

			*sys = att->sa + offset;
			return 0;
		}
	}

	dev_warn(priv->dev, "Translation failed: da = 0x%llx len = 0x%zx\n",
		 da, len);
	return -ENOENT;
}

static void *imx_rproc_da_to_va(struct rproc *rproc, u64 da, size_t len)
{
	struct imx_rproc *priv = rproc->priv;
	void *va = NULL;
	u64 sys;
	int i;

	if (len == 0)
		return NULL;

	/*
	 * On device side we have many aliases, so we need to convert device
	 * address (M4) to system bus address first.
	 */
	if (imx_rproc_da_to_sys(priv, da, len, &sys))
		return NULL;

	for (i = 0; i < IMX7D_RPROC_MEM_MAX; i++) {
		if (sys >= priv->mem[i].sys_addr && sys + len <
		    priv->mem[i].sys_addr +  priv->mem[i].size) {
			unsigned int offset = sys - priv->mem[i].sys_addr;
			/* __force to make sparse happy with type conversion */
			va = (__force void *)(priv->mem[i].cpu_addr + offset);
			break;
		}
	}

	dev_dbg(&rproc->dev, "da = 0x%llx len = 0x%zx va = 0x%p\n",
		da, len, va);

	return va;
}

static void imx_rproc_mb_vq_work(struct work_struct *work)
{
	struct imx_mbox *mb = container_of(work, struct imx_mbox, vq_work);
	struct rproc *rproc = dev_get_drvdata(mb->client.dev);

	if (rproc_vq_interrupt(rproc, mb->vq_id) == IRQ_NONE)
		dev_dbg(&rproc->dev, "no message found in vq%d\n", mb->vq_id);
}

static void imx_rproc_mb_callback(struct mbox_client *cl, void *data)
{
	struct rproc *rproc = dev_get_drvdata(cl->dev);
	struct imx_mbox *mb = container_of(cl, struct imx_mbox, client);
	struct imx_rproc *ddata = rproc->priv;

	queue_work(ddata->workqueue, &mb->vq_work);
}

static const struct imx_mbox imx_rproc_mbox[IMX_MBOX_NB_MBX] = {
	{
		.name = IMX_MBX_VQ0,
		.vq_id = 0,
		.client = {
			.rx_callback = imx_rproc_mb_callback,
			.tx_block = false,
		},
	},
	{
		.name = IMX_MBX_VQ1,
		.vq_id = 1,
		.client = {
			.rx_callback = imx_rproc_mb_callback,
			.tx_block = false,
		},
	},
};

static void imx_rproc_request_mbox(struct rproc *rproc)
{
	struct imx_rproc *ddata = rproc->priv;
	struct device *dev = &rproc->dev;
	unsigned int i;
	const unsigned char *name;
	struct mbox_client *cl;

	/* Initialise mailbox structure table */
	memcpy(ddata->mb, imx_rproc_mbox, sizeof(imx_rproc_mbox));

	for (i = 0; i < IMX_MBOX_NB_MBX; i++) {
		name = ddata->mb[i].name;

		cl = &ddata->mb[i].client;
		cl->dev = dev->parent;

		ddata->mb[i].chan = mbox_request_channel_byname(cl, name);

		dev_dbg(dev, "%s: name=%s, idx=%u\n",
			__func__, name, ddata->mb[i].vq_id);

		if (IS_ERR(ddata->mb[i].chan)) {
			dev_warn(dev, "cannot get %s mbox\n", name);
			ddata->mb[i].chan = NULL;
		}

		if (ddata->mb[i].vq_id >= 0)
			INIT_WORK(&ddata->mb[i].vq_work, imx_rproc_mb_vq_work);
	}
}

static void imx_rproc_free_mbox(struct rproc *rproc)
{
	struct imx_rproc *ddata = rproc->priv;
	unsigned int i;

	dev_dbg(&rproc->dev, "%s: %d boxes\n",
		__func__, ARRAY_SIZE(ddata->mb));

	for (i = 0; i < ARRAY_SIZE(ddata->mb); i++) {
		if (ddata->mb[i].chan)
			mbox_free_channel(ddata->mb[i].chan);
		ddata->mb[i].chan = NULL;
	}
}

static void imx_rproc_kick(struct rproc *rproc, int vqid)
{
	struct imx_rproc *ddata = rproc->priv;
	unsigned int i;
	int err;

	if (WARN_ON(vqid >= IMX_MBOX_NB_VQ))
		return;

	for (i = 0; i < IMX_MBOX_NB_MBX; i++) {
		if (vqid != ddata->mb[i].vq_id)
			continue;
		if (!ddata->mb[i].chan)
			return;
		dev_dbg(&rproc->dev, "sending message : vqid = %d\n", vqid);
		err = mbox_send_message(ddata->mb[i].chan, &vqid);
		if (err < 0)
			dev_err(&rproc->dev, "%s: failed (%s, err:%d)\n",
					__func__, ddata->mb[i].name, err);
			return;
	}
}

static const struct rproc_ops imx_rproc_ops = {
	.start		= imx_rproc_start,
	.stop		= imx_rproc_stop,
	.da_to_va	= imx_rproc_da_to_va,
	.kick		= imx_rproc_kick,
	.get_boot_addr	= rproc_elf_get_boot_addr,
};

static int imx_rproc_addr_init(struct imx_rproc *priv,
			       struct platform_device *pdev)
{
	const struct imx_rproc_dcfg *dcfg = priv->dcfg;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int a, b = 0, err, nph;

	/* remap required addresses */
	for (a = 0; a < dcfg->att_size; a++) {
		const struct imx_rproc_att *att = &dcfg->att[a];

		if (!(att->flags & ATT_OWN))
			continue;

		if (b >= IMX7D_RPROC_MEM_MAX)
			break;

		priv->mem[b].cpu_addr = devm_ioremap(&pdev->dev,
						     att->sa, att->size);
		if (!priv->mem[b].cpu_addr) {
			dev_err(dev, "devm_ioremap_resource failed\n");
			return -ENOMEM;
		}
		priv->mem[b].sys_addr = att->sa;
		priv->mem[b].size = att->size;
		b++;
	}

	/* memory-region is optional property */
	nph = of_count_phandle_with_args(np, "memory-region", NULL);
	if (nph <= 0)
		return 0;

	/* remap optional addresses */
	for (a = 0; a < nph; a++) {
		struct device_node *node;
		struct resource res;

		node = of_parse_phandle(np, "memory-region", a);
		err = of_address_to_resource(node, 0, &res);
		if (err) {
			dev_err(dev, "unable to resolve memory region\n");
			return err;
		}

		if (b >= IMX7D_RPROC_MEM_MAX)
			break;

		priv->mem[b].cpu_addr = devm_ioremap_resource(&pdev->dev, &res);
		if (IS_ERR(priv->mem[b].cpu_addr)) {
			dev_err(dev, "devm_ioremap_resource failed\n");
			err = PTR_ERR(priv->mem[b].cpu_addr);
			return err;
		}
		priv->mem[b].sys_addr = res.start;
		priv->mem[b].size = resource_size(&res);
		b++;
	}

	return 0;
}

static int imx_rproc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct imx_rproc *priv;
	struct rproc *rproc;
	struct regmap_config config = { .name = "imx-rproc" };
	const struct imx_rproc_dcfg *dcfg;
	struct regmap *regmap;
	int ret;

	regmap = syscon_regmap_lookup_by_phandle(np, "syscon");
	if (IS_ERR(regmap)) {
		dev_err(dev, "failed to find syscon\n");
		return PTR_ERR(regmap);
	}
	regmap_attach_dev(dev, regmap, &config);

	/* set some other name then imx */
	rproc = rproc_alloc(dev, "imx-rproc", &imx_rproc_ops,
			    NULL, sizeof(*priv));
	if (!rproc)
		return -ENOMEM;

	dcfg = of_device_get_match_data(dev);
	if (!dcfg) {
		ret = -EINVAL;
		goto err_put_rproc;
	}

	priv = rproc->priv;
	priv->rproc = rproc;
	priv->regmap = regmap;
	priv->dcfg = dcfg;
	priv->dev = dev;

	dev_set_drvdata(dev, rproc);

	ret = imx_rproc_addr_init(priv, pdev);
	if (ret) {
		dev_err(dev, "failed on imx_rproc_addr_init\n");
		goto err_put_rproc;
	}

	priv->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(priv->clk)) {
		dev_err(dev, "Failed to get clock\n");
		ret = PTR_ERR(priv->clk);
		goto err_put_rproc;
	}

	priv->bootreg = imx_rproc_da_to_va(rproc, IMX_BOOT_PC, sizeof(u32));

	/*
	 * clk for M4 block including memory. Should be
	 * enabled before .start for FW transfer.
	 */
	ret = clk_prepare_enable(priv->clk);
	if (ret) {
		dev_err(&rproc->dev, "Failed to enable clock\n");
		goto err_put_rproc;
	}

	priv->workqueue = create_workqueue(dev_name(dev));
	if (!priv->workqueue) {
		dev_err(dev, "cannot create workqueue\n");
		ret = -ENOMEM;
		goto err_put_clk;
	}

	imx_rproc_request_mbox(rproc);

	ret = rproc_add(rproc);
	if (ret) {
		dev_err(dev, "rproc_add failed\n");
		goto err_free_mb;
	}

	return 0;

err_free_mb:
	imx_rproc_free_mbox(rproc);
	destroy_workqueue(priv->workqueue);
err_put_clk:
	clk_disable_unprepare(priv->clk);
err_put_rproc:
	rproc_free(rproc);

	return ret;
}

static int imx_rproc_remove(struct platform_device *pdev)
{
	struct rproc *rproc = platform_get_drvdata(pdev);
	struct imx_rproc *priv = rproc->priv;

	clk_disable_unprepare(priv->clk);
	rproc_del(rproc);
	imx_rproc_free_mbox(rproc);
	rproc_free(rproc);

	return 0;
}

static const struct of_device_id imx_rproc_of_match[] = {
	{ .compatible = "fsl,imx7d-cm4", .data = &imx_rproc_cfg_imx7d },
	{ .compatible = "fsl,imx6sx-cm4", .data = &imx_rproc_cfg_imx6sx },
	{},
};
MODULE_DEVICE_TABLE(of, imx_rproc_of_match);

static struct platform_driver imx_rproc_driver = {
	.probe = imx_rproc_probe,
	.remove = imx_rproc_remove,
	.driver = {
		.name = "imx-rproc",
		.of_match_table = imx_rproc_of_match,
	},
};

module_platform_driver(imx_rproc_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("IMX6SX/7D remote processor control driver");
MODULE_AUTHOR("Oleksij Rempel <o.rempel@pengutronix.de>");
