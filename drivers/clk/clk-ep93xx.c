// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * arch/arm/mach-ep93xx/clock.c
 * Clock control for Cirrus EP93xx chips.
 *
 * Copyright (C) 2006 Lennert Buytenhek <buytenh@wantstofly.org>
 */

#define DEBUG

#define pr_fmt(fmt) "ep93xx " KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/spinlock.h>
#include <linux/clkdev.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/soc/cirrus/ep93xx.h>
#include <dt-bindings/clock/cirrus,ep93xx-clock.h>

#include <asm/div64.h>

#define EP93XX_EXT_CLK_RATE	14745600
#define EP93XX_EXT_RTC_RATE	32768

#define EP93XX_KEYTCHCLK_DIV4	(EP93XX_EXT_CLK_RATE / 4)
#define EP93XX_KEYTCHCLK_DIV16	(EP93XX_EXT_CLK_RATE / 16)

#define EP93XX_APB_VIRT_BASE            0xfed00000
#define EP93XX_APB_IOMEM(x)             IOMEM(EP93XX_APB_VIRT_BASE + (x))
#define EP93XX_SYSCON_BASE		EP93XX_APB_IOMEM(0x00130000)
#define EP93XX_SYSCON_REG(x)		(EP93XX_SYSCON_BASE + (x))
#define EP93XX_SYSCON_DEVCFG		0x80
#define EP93XX_SYSCON_POWER_STATE	0x00
#define EP93XX_SYSCON_PWRCNT		0x04
#define EP93XX_SYSCON_PWRCNT_FIR_EN	(1<<31)
#define EP93XX_SYSCON_PWRCNT_UARTBAUD	BIT(29)
#define EP93XX_SYSCON_PWRCNT_USH_EN	BIT(28)
#define EP93XX_SYSCON_PWRCNT_DMA_M2M1	BIT(27)
#define EP93XX_SYSCON_PWRCNT_DMA_M2M0	BIT(26)
#define EP93XX_SYSCON_PWRCNT_DMA_M2P8	BIT(25)
#define EP93XX_SYSCON_PWRCNT_DMA_M2P9	BIT(24)
#define EP93XX_SYSCON_PWRCNT_DMA_M2P6	BIT(23)
#define EP93XX_SYSCON_PWRCNT_DMA_M2P7	BIT(22)
#define EP93XX_SYSCON_PWRCNT_DMA_M2P4	BIT(21)
#define EP93XX_SYSCON_PWRCNT_DMA_M2P5	BIT(20)
#define EP93XX_SYSCON_PWRCNT_DMA_M2P2	BIT(19)
#define EP93XX_SYSCON_PWRCNT_DMA_M2P3	BIT(18)
#define EP93XX_SYSCON_PWRCNT_DMA_M2P0	BIT(17)
#define EP93XX_SYSCON_PWRCNT_DMA_M2P1	BIT(16)
#define EP93XX_SYSCON_HALT		0x08
#define EP93XX_SYSCON_STANDBY		0x0c
#define EP93XX_SYSCON_CLKSET1		0x20
#define EP93XX_SYSCON_CLKSET1_NBYP1	(1<<23)
#define EP93XX_SYSCON_CLKSET2		0x24
#define EP93XX_SYSCON_CLKSET2_NBYP2	(1<<19)
#define EP93XX_SYSCON_CLKSET2_PLL2_EN	(1<<18)
#define EP93XX_SYSCON_DEVCFG		0x80
#define EP93XX_SYSCON_DEVCFG_SWRST	(1<<31)
#define EP93XX_SYSCON_DEVCFG_D1ONG	(1<<30)
#define EP93XX_SYSCON_DEVCFG_D0ONG	(1<<29)
#define EP93XX_SYSCON_DEVCFG_IONU2	(1<<28)
#define EP93XX_SYSCON_DEVCFG_GONK	(1<<27)
#define EP93XX_SYSCON_DEVCFG_TONG	(1<<26)
#define EP93XX_SYSCON_DEVCFG_MONG	(1<<25)
#define EP93XX_SYSCON_DEVCFG_U3EN	BIT(24)
#define EP93XX_SYSCON_DEVCFG_CPENA	(1<<23)
#define EP93XX_SYSCON_DEVCFG_A2ONG	(1<<22)
#define EP93XX_SYSCON_DEVCFG_A1ONG	(1<<21)
#define EP93XX_SYSCON_DEVCFG_U2EN	BIT(20)
#define EP93XX_SYSCON_DEVCFG_EXVC	(1<<19)
#define EP93XX_SYSCON_DEVCFG_U1EN	BIT(18)
#define EP93XX_SYSCON_DEVCFG_TIN	(1<<17)
#define EP93XX_SYSCON_DEVCFG_HC3IN	(1<<15)
#define EP93XX_SYSCON_DEVCFG_HC3EN	(1<<14)
#define EP93XX_SYSCON_DEVCFG_HC1IN	(1<<13)
#define EP93XX_SYSCON_DEVCFG_HC1EN	(1<<12)
#define EP93XX_SYSCON_DEVCFG_HONIDE	(1<<11)
#define EP93XX_SYSCON_DEVCFG_GONIDE	(1<<10)
#define EP93XX_SYSCON_DEVCFG_PONG	(1<<9)
#define EP93XX_SYSCON_DEVCFG_EONIDE	(1<<8)
#define EP93XX_SYSCON_DEVCFG_I2SONSSP	(1<<7)
#define EP93XX_SYSCON_DEVCFG_I2SONAC97	(1<<6)
#define EP93XX_SYSCON_DEVCFG_RASONP3	(1<<4)
#define EP93XX_SYSCON_DEVCFG_RAS	(1<<3)
#define EP93XX_SYSCON_DEVCFG_ADCPD	(1<<2)
#define EP93XX_SYSCON_DEVCFG_KEYS	(1<<1)
#define EP93XX_SYSCON_DEVCFG_SHENA	(1<<0)
#define EP93XX_SYSCON_VIDCLKDIV		0x84
#define EP93XX_SYSCON_CLKDIV_ENABLE	(1<<15)
#define EP93XX_SYSCON_CLKDIV_ESEL	(1<<14)
#define EP93XX_SYSCON_CLKDIV_PSEL	(1<<13)
#define EP93XX_SYSCON_CLKDIV_PDIV_SHIFT	8
#define EP93XX_SYSCON_I2SCLKDIV		0x8c
#define EP93XX_SYSCON_I2SCLKDIV_SENA	(1<<31)
#define EP93XX_SYSCON_I2SCLKDIV_ORIDE   (1<<29)
#define EP93XX_SYSCON_I2SCLKDIV_SPOL	(1<<19)
#define EP93XX_I2SCLKDIV_SDIV		(1 << 16)
#define EP93XX_I2SCLKDIV_LRDIV32	(0 << 17)
#define EP93XX_I2SCLKDIV_LRDIV64	(1 << 17)
#define EP93XX_I2SCLKDIV_LRDIV128	(2 << 17)
#define EP93XX_I2SCLKDIV_LRDIV_MASK	(3 << 17)
#define EP93XX_SYSCON_KEYTCHCLKDIV	0x90
#define EP93XX_SYSCON_KEYTCHCLKDIV_TSEN	(1<<31)
#define EP93XX_SYSCON_KEYTCHCLKDIV_ADIV	(1<<16)
#define EP93XX_SYSCON_KEYTCHCLKDIV_KEN	(1<<15)
#define EP93XX_SYSCON_KEYTCHCLKDIV_KDIV	(1<<0)
#define EP93XX_SYSCON_CHIPID		0x94
#define EP93XX_SYSCON_SYSCFG		0x9c
#define EP93XX_SYSCON_SYSCFG_REV_MASK	(0xf0000000)
#define EP93XX_SYSCON_SYSCFG_REV_SHIFT	(28)
#define EP93XX_SYSCON_SYSCFG_SBOOT	(1<<8)
#define EP93XX_SYSCON_SYSCFG_LCSN7	(1<<7)
#define EP93XX_SYSCON_SYSCFG_LCSN6	(1<<6)
#define EP93XX_SYSCON_SYSCFG_LASDO	(1<<5)
#define EP93XX_SYSCON_SYSCFG_LEEDA	(1<<4)
#define EP93XX_SYSCON_SYSCFG_LEECLK	(1<<3)
#define EP93XX_SYSCON_SYSCFG_LCSN2	(1<<1)
#define EP93XX_SYSCON_SYSCFG_LCSN1	(1<<0)
#define EP93XX_SYSCON_SWLOCK		0xc0

static DEFINE_SPINLOCK(syscon_swlock);
static struct regmap *ep93xx_map;

static void ep93xx_syscon_swlocked_write(unsigned int val, void __iomem *reg)
{
	unsigned long flags;
	
	spin_lock_irqsave(&syscon_swlock, flags);
	
	__raw_writel(0xaa, EP93XX_SYSCON_SWLOCK);
	__raw_writel(val, reg);
	
	spin_unlock_irqrestore(&syscon_swlock, flags);
}

static void ep93xx_devcfg_set_clear(unsigned int set_bits, unsigned int clear_bits)
{
	unsigned long flags;
	unsigned int val;
	
	spin_lock_irqsave(&syscon_swlock, flags);
	
	val = __raw_readl(EP93XX_SYSCON_DEVCFG);
	val &= ~clear_bits;
	val |= set_bits;
	__raw_writel(0xaa, EP93XX_SYSCON_SWLOCK);
	__raw_writel(val, EP93XX_SYSCON_DEVCFG);
	
	spin_unlock_irqrestore(&syscon_swlock, flags);
}

/* Keeps track of all clocks */
static struct clk_hw_onecell_data *ep93xx_clk_data;

struct clk {
	struct clk	*parent;
	unsigned long	rate;
	int		users;
	int		sw_locked;
	void __iomem	*enable_reg;
	u32		enable_mask;
	
	unsigned long	(*get_rate)(struct clk *clk);
	int		(*set_rate)(struct clk *clk, unsigned long rate);
};


static unsigned long get_uart_rate(struct clk *clk);

static int set_keytchclk_rate(struct clk *clk, unsigned long rate);
static int set_div_rate(struct clk *clk, unsigned long rate);
static int set_i2s_sclk_rate(struct clk *clk, unsigned long rate);
static int set_i2s_lrclk_rate(struct clk *clk, unsigned long rate);

static struct clk clk_xtali = {
	.rate		= EP93XX_EXT_CLK_RATE,
};
static struct clk clk_uart1 = {
	.parent		= &clk_xtali,
	.sw_locked	= 1,
	.enable_reg	= EP93XX_SYSCON_DEVCFG,
	.enable_mask	= EP93XX_SYSCON_DEVCFG_U1EN,
	.get_rate	= get_uart_rate,
};
static struct clk clk_uart2 = {
	.parent		= &clk_xtali,
	.sw_locked	= 1,
	.enable_reg	= EP93XX_SYSCON_DEVCFG,
	.enable_mask	= EP93XX_SYSCON_DEVCFG_U2EN,
	.get_rate	= get_uart_rate,
};
static struct clk clk_uart3 = {
	.parent		= &clk_xtali,
	.sw_locked	= 1,
	.enable_reg	= EP93XX_SYSCON_DEVCFG,
	.enable_mask	= EP93XX_SYSCON_DEVCFG_U3EN,
	.get_rate	= get_uart_rate,
};
static struct clk clk_pll1 = {
	.parent		= &clk_xtali,
};
static struct clk clk_f = {
	.parent		= &clk_pll1,
};
static struct clk clk_h = {
	.parent		= &clk_pll1,
};
static struct clk clk_p = {
	.parent		= &clk_pll1,
};
static struct clk clk_pll2 = {
	.parent		= &clk_xtali,
};
static struct clk clk_usb_host = {
	.parent		= &clk_pll2,
	.enable_reg	= EP93XX_SYSCON_PWRCNT,
	.enable_mask	= EP93XX_SYSCON_PWRCNT_USH_EN,
};
static struct clk clk_keypad = {
	.parent		= &clk_xtali,
	.sw_locked	= 1,
	.enable_reg	= EP93XX_SYSCON_KEYTCHCLKDIV,
	.enable_mask	= EP93XX_SYSCON_KEYTCHCLKDIV_KEN,
	.set_rate	= set_keytchclk_rate,
};
static struct clk clk_adc = {
	.parent		= &clk_xtali,
	.sw_locked	= 1,
	.enable_reg	= EP93XX_SYSCON_KEYTCHCLKDIV,
	.enable_mask	= EP93XX_SYSCON_KEYTCHCLKDIV_TSEN,
	.set_rate	= set_keytchclk_rate,
};
static struct clk clk_spi = {
	.parent		= &clk_xtali,
	.rate		= EP93XX_EXT_CLK_RATE,
};
static struct clk clk_pwm = {
	.parent		= &clk_xtali,
	.rate		= EP93XX_EXT_CLK_RATE,
};

static struct clk clk_video = {
	.sw_locked	= 1,
	.enable_reg     = EP93XX_SYSCON_VIDCLKDIV,
	.enable_mask    = EP93XX_SYSCON_CLKDIV_ENABLE,
	.set_rate	= set_div_rate,
};

static struct clk clk_i2s_mclk = {
	.sw_locked	= 1,
	.enable_reg	= EP93XX_SYSCON_I2SCLKDIV,
	.enable_mask	= EP93XX_SYSCON_CLKDIV_ENABLE,
	.set_rate	= set_div_rate,
};

static struct clk clk_i2s_sclk = {
	.sw_locked	= 1,
	.parent		= &clk_i2s_mclk,
	.enable_reg	= EP93XX_SYSCON_I2SCLKDIV,
	.enable_mask	= EP93XX_SYSCON_I2SCLKDIV_SENA,
	.set_rate	= set_i2s_sclk_rate,
};

static struct clk clk_i2s_lrclk = {
	.sw_locked	= 1,
	.parent		= &clk_i2s_sclk,
	.enable_reg	= EP93XX_SYSCON_I2SCLKDIV,
	.enable_mask	= EP93XX_SYSCON_I2SCLKDIV_SENA,
	.set_rate	= set_i2s_lrclk_rate,
};

/* DMA Clocks */
static struct clk clk_m2p0 = {
	.parent		= &clk_h,
	.enable_reg	= EP93XX_SYSCON_PWRCNT,
	.enable_mask	= EP93XX_SYSCON_PWRCNT_DMA_M2P0,
};
static struct clk clk_m2p1 = {
	.parent		= &clk_h,
	.enable_reg	= EP93XX_SYSCON_PWRCNT,
	.enable_mask	= EP93XX_SYSCON_PWRCNT_DMA_M2P1,
};
static struct clk clk_m2p2 = {
	.parent		= &clk_h,
	.enable_reg	= EP93XX_SYSCON_PWRCNT,
	.enable_mask	= EP93XX_SYSCON_PWRCNT_DMA_M2P2,
};
static struct clk clk_m2p3 = {
	.parent		= &clk_h,
	.enable_reg	= EP93XX_SYSCON_PWRCNT,
	.enable_mask	= EP93XX_SYSCON_PWRCNT_DMA_M2P3,
};
static struct clk clk_m2p4 = {
	.parent		= &clk_h,
	.enable_reg	= EP93XX_SYSCON_PWRCNT,
	.enable_mask	= EP93XX_SYSCON_PWRCNT_DMA_M2P4,
};
static struct clk clk_m2p5 = {
	.parent		= &clk_h,
	.enable_reg	= EP93XX_SYSCON_PWRCNT,
	.enable_mask	= EP93XX_SYSCON_PWRCNT_DMA_M2P5,
};
static struct clk clk_m2p6 = {
	.parent		= &clk_h,
	.enable_reg	= EP93XX_SYSCON_PWRCNT,
	.enable_mask	= EP93XX_SYSCON_PWRCNT_DMA_M2P6,
};
static struct clk clk_m2p7 = {
	.parent		= &clk_h,
	.enable_reg	= EP93XX_SYSCON_PWRCNT,
	.enable_mask	= EP93XX_SYSCON_PWRCNT_DMA_M2P7,
};
static struct clk clk_m2p8 = {
	.parent		= &clk_h,
	.enable_reg	= EP93XX_SYSCON_PWRCNT,
	.enable_mask	= EP93XX_SYSCON_PWRCNT_DMA_M2P8,
};
static struct clk clk_m2p9 = {
	.parent		= &clk_h,
	.enable_reg	= EP93XX_SYSCON_PWRCNT,
	.enable_mask	= EP93XX_SYSCON_PWRCNT_DMA_M2P9,
};
static struct clk clk_m2m0 = {
	.parent		= &clk_h,
	.enable_reg	= EP93XX_SYSCON_PWRCNT,
	.enable_mask	= EP93XX_SYSCON_PWRCNT_DMA_M2M0,
};
static struct clk clk_m2m1 = {
	.parent		= &clk_h,
	.enable_reg	= EP93XX_SYSCON_PWRCNT,
	.enable_mask	= EP93XX_SYSCON_PWRCNT_DMA_M2M1,
};

#define INIT_CK(dev,con,ck)					\
{ .dev_id = dev, .con_id = con, .clk = ck }

static struct clk_lookup clocks[] = {
	INIT_CK(NULL,			"xtali",	&clk_xtali), // [*]
	INIT_CK("apb:uart1",		NULL,		&clk_uart1), // [*]
	INIT_CK("apb:uart2",		NULL,		&clk_uart2), // [*]
	INIT_CK("apb:uart3",		NULL,		&clk_uart3), // [*]
	INIT_CK(NULL,			"pll1",		&clk_pll1), // [*]
	INIT_CK(NULL,			"fclk",		&clk_f), // [*]
	INIT_CK(NULL,			"hclk",		&clk_h), // [*]
	INIT_CK(NULL,			"apb_pclk",	&clk_p), // [*]
	INIT_CK(NULL,			"pll2",		&clk_pll2), // [*]
	INIT_CK("ohci-platform",	NULL,		&clk_usb_host), // [*]
	INIT_CK("ep93xx-keypad",	NULL,		&clk_keypad), // 
	INIT_CK("ep93xx-adc",		NULL,		&clk_adc), // 
	INIT_CK("ep93xx-fb",		NULL,		&clk_video),
	INIT_CK("ep93xx-spi.0",		NULL,		&clk_spi),// [*]
	INIT_CK("ep93xx-i2s",		"mclk",		&clk_i2s_mclk),
	INIT_CK("ep93xx-i2s",		"sclk",		&clk_i2s_sclk),
	INIT_CK("ep93xx-i2s",		"lrclk",	&clk_i2s_lrclk),
	INIT_CK(NULL,			"pwm_clk",	&clk_pwm),  // no PWM on ts7250, fixed rate 
	INIT_CK(NULL,			"m2p0",		&clk_m2p0), // [*]
	INIT_CK(NULL,			"m2p1",		&clk_m2p1), // [*]
	INIT_CK(NULL,			"m2p2",		&clk_m2p2), // [*]
	INIT_CK(NULL,			"m2p3",		&clk_m2p3), // [*]
	INIT_CK(NULL,			"m2p4",		&clk_m2p4), // [*]
	INIT_CK(NULL,			"m2p5",		&clk_m2p5), // [*]
	INIT_CK(NULL,			"m2p6",		&clk_m2p6), // [*]
	INIT_CK(NULL,			"m2p7",		&clk_m2p7), // [*]
	INIT_CK(NULL,			"m2p8",		&clk_m2p8), // [*]
	INIT_CK(NULL,			"m2p9",		&clk_m2p9), // [*]
	INIT_CK(NULL,			"m2m0",		&clk_m2m0), // [*]
	INIT_CK(NULL,			"m2m1",		&clk_m2m1), // [*]
};

struct ep93xx_gate_data {
	u8 bit_idx;
	const char *name;
	const char *parent_name;
	unsigned long flags;
};

/* DMA Clocks */
static const struct ep93xx_gate_data dma_gates[] = {
	{EP93XX_SYSCON_PWRCNT_DMA_M2P0, "m2p0", "hclk", 0},
	{EP93XX_SYSCON_PWRCNT_DMA_M2P1, "m2p1", "hclk", 0},
	{EP93XX_SYSCON_PWRCNT_DMA_M2P2, "m2p2", "hclk", 0},
	{EP93XX_SYSCON_PWRCNT_DMA_M2P3, "m2p3", "hclk", 0},
	{EP93XX_SYSCON_PWRCNT_DMA_M2P4, "m2p4", "hclk", 0},
	{EP93XX_SYSCON_PWRCNT_DMA_M2P5, "m2p5", "hclk", 0},
	{EP93XX_SYSCON_PWRCNT_DMA_M2P6, "m2p6", "hclk", 0},
	{EP93XX_SYSCON_PWRCNT_DMA_M2P7, "m2p7", "hclk", 0},
	{EP93XX_SYSCON_PWRCNT_DMA_M2P8, "m2p8", "hclk", 0},
	{EP93XX_SYSCON_PWRCNT_DMA_M2P9, "m2p9", "hclk", 0},
	{EP93XX_SYSCON_PWRCNT_DMA_M2M0, "m2m0", "hclk", 0},
	{EP93XX_SYSCON_PWRCNT_DMA_M2M1, "m2m1", "hclk", 0},
};

static DEFINE_SPINLOCK(clk_lock);

static unsigned long get_uart_rate(struct clk *clk)
{
	unsigned long rate = clk_get_rate(clk->parent);
	u32 value;
	
	value = __raw_readl(EP93XX_SYSCON_PWRCNT);
	if (value & EP93XX_SYSCON_PWRCNT_UARTBAUD)
		return rate;
	else
		return rate / 2;
}

static int set_keytchclk_rate(struct clk *clk, unsigned long rate)
{
	u32 val;
	u32 div_bit;
	
	val = __raw_readl(clk->enable_reg);
	
	/*
	 * The Key Matrix and ADC clocks are configured using the same
	 * System Controller register.  The clock used will be either
	 * 1/4 or 1/16 the external clock rate depending on the
	 * EP93XX_SYSCON_KEYTCHCLKDIV_KDIV/EP93XX_SYSCON_KEYTCHCLKDIV_ADIV
	 * bit being set or cleared.
	 */
	div_bit = clk->enable_mask >> 15;
	
	if (rate == EP93XX_KEYTCHCLK_DIV4)
		val |= div_bit;
	else if (rate == EP93XX_KEYTCHCLK_DIV16)
		val &= ~div_bit;
	else
		return -EINVAL;
	
	ep93xx_syscon_swlocked_write(val, clk->enable_reg);
	clk->rate = rate;
	return 0;
}

static int calc_clk_div(struct clk *clk, unsigned long rate,
			int *psel, int *esel, int *pdiv, int *div)
{
	struct clk *mclk;
	unsigned long max_rate, actual_rate, mclk_rate, rate_err = -1;
	int i, found = 0, __div = 0, __pdiv = 0;
	
	/* Don't exceed the maximum rate */
	max_rate = max3(clk_pll1.rate / 4, clk_pll2.rate / 4, clk_xtali.rate / 4);
	rate = min(rate, max_rate);
	
	/*
	 * Try the two pll's and the external clock
	 * Because the valid predividers are 2, 2.5 and 3, we multiply
	 * all the clocks by 2 to avoid floating point math.
	 *
	 * This is based on the algorithm in the ep93xx raster guide:
	 * http://be-a-maverick.com/en/pubs/appNote/AN269REV1.pdf
	 *
	 */
	for (i = 0; i < 3; i++) {
		if (i == 0)
			mclk = &clk_xtali;
		else if (i == 1)
			mclk = &clk_pll1;
		else
			mclk = &clk_pll2;
		mclk_rate = mclk->rate * 2;
		
		/* Try each predivider value */
		for (__pdiv = 4; __pdiv <= 6; __pdiv++) {
			__div = mclk_rate / (rate * __pdiv);
			if (__div < 2 || __div > 127)
				continue;
			
			actual_rate = mclk_rate / (__pdiv * __div);
			
			if (!found || abs(actual_rate - rate) < rate_err) {
				*pdiv = __pdiv - 3;
				*div = __div;
				*psel = (i == 2);
				*esel = (i != 0);
				clk->parent = mclk;
				clk->rate = actual_rate;
				rate_err = abs(actual_rate - rate);
				found = 1;
			}
		}
	}
	
	if (!found)
		return -EINVAL;
	
	return 0;
}

static int set_div_rate(struct clk *clk, unsigned long rate)
{
	int err, psel = 0, esel = 0, pdiv = 0, div = 0;
	u32 val;
	
	err = calc_clk_div(clk, rate, &psel, &esel, &pdiv, &div);
	if (err)
		return err;
	
	/* Clear the esel, psel, pdiv and div bits */
	val = __raw_readl(clk->enable_reg);
	val &= ~0x7fff;
	
	/* Set the new esel, psel, pdiv and div bits for the new clock rate */
	val |= (esel ? EP93XX_SYSCON_CLKDIV_ESEL : 0) |
	(psel ? EP93XX_SYSCON_CLKDIV_PSEL : 0) |
	(pdiv << EP93XX_SYSCON_CLKDIV_PDIV_SHIFT) | div;
	ep93xx_syscon_swlocked_write(val, clk->enable_reg);
	return 0;
}

static char fclk_divisors[] = { 1, 2, 4, 8, 16, 1, 1, 1 };
static char hclk_divisors[] = { 1, 2, 4, 5, 6, 8, 16, 32 };
static char pclk_divisors[] = { 1, 2, 4, 8 };

/*
 * PLL rate = 14.7456 MHz * (X1FBD + 1) * (X2FBD + 1) / (X2IPD + 1) / 2^PS
 */
static unsigned long calc_pll_rate(u32 config_word)
{
	unsigned long long rate;
	int i;
	
	rate = clk_xtali.rate;
	rate *= ((config_word >> 11) & 0x1f) + 1;		/* X1FBD */
	rate *= ((config_word >> 5) & 0x3f) + 1;		/* X2FBD */
	do_div(rate, (config_word & 0x1f) + 1);			/* X2IPD */
	for (i = 0; i < ((config_word >> 16) & 3); i++)		/* PS */
		rate >>= 1;
	
	return (unsigned long)rate;
}


/**
 * clk_hw_register_divider - register a divider clock with the clock framework
 * @dev: device registering this clock
 * @name: name of this clock
 * @parent_name: name of clock's parent
 * @flags: framework-specific flags
 * @reg: register address to adjust divider
 * @shift: number of bits to shift the bitfield
 * @width: width of the bitfield
 * @clk_divider_flags: divider-specific flags for this clock
 * @lock: shared register lock for this clock
 */
// #define clk_hw_register_divider(dev, name, parent_name, flags, reg, shift,    \
//width, clk_divider_flags, lock)		      \
//__clk_hw_register_divider((dev), NULL, (name), (parent_name), NULL,   \
//NULL, (flags), (reg), (shift), (width),     \
//(clk_divider_flags), NULL, (lock))

struct ep93xx_gate {
	unsigned int idx;
	unsigned int bit;
	const char* dev_id;
	const char* con_id;
};

static struct ep93xx_gate ep93xx_uarts[] = {
	{EP93XX_CLK_UART1, EP93XX_SYSCON_DEVCFG_U1EN, "apb:uart1", NULL},
	{EP93XX_CLK_UART2, EP93XX_SYSCON_DEVCFG_U2EN, "apb:uart2", NULL},
	{EP93XX_CLK_UART3, EP93XX_SYSCON_DEVCFG_U3EN, "apb:uart3", NULL},
};

static struct ep93xx_gate ep93xx_dmas[] = {	
	{EP93XX_CLK_M2P0, EP93XX_SYSCON_PWRCNT_DMA_M2P0, NULL, "m2p0"},
	{EP93XX_CLK_M2P1, EP93XX_SYSCON_PWRCNT_DMA_M2P1, NULL, "m2p1"},
	{EP93XX_CLK_M2P2, EP93XX_SYSCON_PWRCNT_DMA_M2P2, NULL, "m2p2"},
	{EP93XX_CLK_M2P3, EP93XX_SYSCON_PWRCNT_DMA_M2P3, NULL, "m2p3"},
	{EP93XX_CLK_M2P4, EP93XX_SYSCON_PWRCNT_DMA_M2P4, NULL, "m2p4"},
	{EP93XX_CLK_M2P5, EP93XX_SYSCON_PWRCNT_DMA_M2P5, NULL, "m2p5"},
	{EP93XX_CLK_M2P6, EP93XX_SYSCON_PWRCNT_DMA_M2P6, NULL, "m2p6"},
	{EP93XX_CLK_M2P7, EP93XX_SYSCON_PWRCNT_DMA_M2P7, NULL, "m2p7"},
	{EP93XX_CLK_M2P8, EP93XX_SYSCON_PWRCNT_DMA_M2P8, NULL, "m2p8"},
	{EP93XX_CLK_M2P9, EP93XX_SYSCON_PWRCNT_DMA_M2P9, NULL, "m2p9"},
	{EP93XX_CLK_M2M0, EP93XX_SYSCON_PWRCNT_DMA_M2M0, NULL, "m2m0"},
	{EP93XX_CLK_M2M1, EP93XX_SYSCON_PWRCNT_DMA_M2M1, NULL, "m2m1"},
};

static struct ep93xx_gate ep93xx_spi 
	= {EP93XX_CLK_SPI, 0, "ep93xx-spi.0", NULL};

static struct ep93xx_gate ep93xx_usb
	= {EP93XX_CLK_USB, EP93XX_SYSCON_PWRCNT_USH_EN, "ohci-platform", NULL};

static const struct clk_div_table ep93xx_adc_divs[] = {
	{ .val = 0, .div = 16, },
	{ .val = 1, .div = 4, },
	{}
};

static int ep93xx_clk_probe(struct platform_device *pdev)
{
	int i;
	int ret;
	void __iomem *base;
	struct clk_hw *hw;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	u32 value;

	dev_info(dev, "%s\n", __func__);

	base = of_iomap(np, 0);
	/* Remap the system controller for the exclusive register */
	if (IS_ERR(base))
		return PTR_ERR(base);

	value = __raw_readl(base + EP93XX_SYSCON_CHIPID);
	dev_info(dev, "base=0x%px, chip_id=0x%x\n", base, value);

	/* register UART dividers */
// 	hw = clk_hw_register_divider(NULL, "uart", "xtali", 
// 				0, base + EP93XX_SYSCON_PWRCNT,
// 				EP93XX_SYSCON_PWRCNT_UARTBAUD, 1,
// 				CLK_DIVIDER_ONE_BASED | CLK_DIVIDER_ALLOW_ZERO));

	value = __raw_readl(base + EP93XX_SYSCON_PWRCNT);
	dev_info(dev, "%s : 0x%x", ep93xx_uarts[i].dev_id, value & EP93XX_SYSCON_PWRCNT_UARTBAUD);

	// 	0 - 1/2 * xtali
	// 	1 - 1 * xtali
	hw = clk_hw_register_fixed_factor(NULL, "uart", "xtali", 0, 1, 2);
	ep93xx_clk_data->hws[EP93XX_CLK_UART] = hw;

	// parenting uart gate clocks to uart clock
	for (i = 0; i < ARRAY_SIZE(ep93xx_uarts); i++) {
		value = __raw_readl(base + EP93XX_SYSCON_DEVCFG);
		dev_info(dev, "%s : 0x%x", ep93xx_uarts[i].dev_id, value & ep93xx_uarts[i].bit);

		hw = clk_hw_register_gate(NULL, ep93xx_uarts[i].dev_id, 
					"uart", 0, 
					base + EP93XX_SYSCON_DEVCFG,
					ep93xx_uarts[i].bit,
					0,
					&syscon_swlock);

		ep93xx_clk_data->hws[ep93xx_uarts[i].idx] = hw;

		ret = clk_hw_register_clkdev(hw, NULL, ep93xx_uarts[i].dev_id);
		if (ret)
			pr_err("%s: failed to register lookup %s\n",
			       __func__, ep93xx_uarts[i].dev_id);
	}

	/* register SPI fixed clock */	
	hw = clk_hw_register_fixed_rate(NULL, ep93xx_spi.dev_id, "xtali", 0, EP93XX_EXT_CLK_RATE);
	ep93xx_clk_data->hws[ep93xx_spi.idx] = hw;

	ret = clk_hw_register_clkdev(hw, NULL, ep93xx_spi.dev_id);
	if (ret)
		pr_err("%s: failed to register lookup %s\n",
		       __func__, ep93xx_spi.dev_id);

	/* register USB gate */
	hw = clk_hw_register_gate(NULL, ep93xx_usb.dev_id,
				"pll2", 0,
				base + EP93XX_SYSCON_DEVCFG,
				ep93xx_usb.bit,
				0,
				&syscon_swlock);
	
	ep93xx_clk_data->hws[ep93xx_usb.idx] = hw;
	ret = clk_hw_register_clkdev(hw, NULL, ep93xx_usb.dev_id);
	if (ret)
		pr_err("%s: failed to register lookup %s\n",
		       __func__, ep93xx_usb.dev_id);
	
	// TSEN - Touchscreen and ADC clock enable 
	// KEN - Matrix keyboard clock enable 
	
	/* ADC clock */
	/* ADIV :*/
	/* 0 devide by 16 */
	/* 1 devide by 4 */
	hw = devm_clk_hw_register_divider_table(dev, "adc", "xtali", 0, 
						base + EP93XX_SYSCON_KEYTCHCLKDIV, 0,
						15, CLK_DIVIDER_ALLOW_ZERO,
						ep93xx_adc_divs, NULL);
	ep93xx_clk_data->hws[EP93XX_CLK_ADC] = hw;
	
	ret = clk_hw_register_clkdev(hw, NULL, "adc");
	if (ret)
		pr_err("%s: failed to register lookup ep93xx-adc\n",
		       __func__);
	
	/* register ADC gate clock */
	hw = clk_hw_register_gate(NULL, "ep93xx-adc",
				"adc", 0,
				base + EP93XX_SYSCON_KEYTCHCLKDIV,
				EP93XX_SYSCON_KEYTCHCLKDIV_TSEN,
				0,
				&syscon_swlock);
	ep93xx_clk_data->hws[EP93XX_CLK_ADC_EN] = hw;

	ret = clk_hw_register_clkdev(hw, NULL, "ep93xx-adc");
	if (ret)
		pr_err("%s: failed to register lookup ep93xx-adc\n",
		       __func__);
	
	// hw = devm_clk_hw_register_divider_table(dev, clk_name, clk_parent, 0, reg, 0, 5, 0, divs, NULL);
	
	/* Keyboard clock */
	/* ep93xx-keypad */
	/* KDIV: */
	/* 0 devide by 16 */
	/* 1 devide by 4 */
	

	return 0;
}

static const struct of_device_id ep93xx_clk_dt_ids[] = {
	{ .compatible = "cirrus,ep93xx-syscon", },
	{ /* sentinel */ },
};

static struct platform_driver ep93xx_clk_driver = {
	.probe  = ep93xx_clk_probe,
	.driver = {
		.name = "ep93xx-clk",
		.of_match_table = ep93xx_clk_dt_ids,
		.suppress_bind_attrs = true,
	},
};
builtin_platform_driver(ep93xx_clk_driver);

static void __init ep93xx_clock_init(struct device_node *np)
{
	struct clk_hw *hw;
	u32 value;
	int i;
	int ret;
	void __iomem *base;
	
	unsigned long clk_pll1_rate;
	unsigned long clk_f_rate;
	unsigned long clk_h_rate;
	unsigned long clk_p_rate;
	unsigned long clk_pll2_rate;
	
	unsigned int clk_f_div;
	unsigned int clk_h_div;
	unsigned int clk_p_div;
	
	ep93xx_clk_data = kzalloc(struct_size(ep93xx_clk_data, hws,
					EP93XX_NUM_CLKS),
					GFP_KERNEL);

	if (!ep93xx_clk_data)
		return;

	/*
	 * This way all clock fetched before the platform device probes,
	 * except those we assign here for early use, will be deferred.
	 */
	for (i = 0; i < EP93XX_NUM_CLKS; i++)
		ep93xx_clk_data->hws[i] = ERR_PTR(-EPROBE_DEFER);

	base = of_iomap(np, 0);
	/* Remap the system controller for the exclusive register */
	if (IS_ERR(base)) {
		pr_err("failed to map base\n");
		return;
	}

	ep93xx_map = syscon_node_to_regmap(np);
	if (IS_ERR(ep93xx_map)) {
		pr_err("no syscon regmap\n");
		return;
	}

	// pr_debug("syscon regmap: 0x%x\n", );
	/*
	 * We check that the regmap works on this very first access,
	 * but as this is an MMIO-backed regmap, subsequent regmap
	 * access is not going to fail and we skip error checks from
	 * this point.
	 */
	ret = regmap_read(ep93xx_map, EP93XX_SYSCON_CHIPID, &value);
	if (ret) {
		pr_err("failed to read global status register\n");
		return;
	}
	pr_info("chip ID: 0x%x\n", value);

	hw = clk_hw_register_fixed_rate(NULL, "xtali", NULL, 0, EP93XX_EXT_CLK_RATE);
	if (IS_ERR_OR_NULL(hw)) {
		pr_err("%s : failed to register xtali", __func__);
	}

	pr_info("main crystal @%lu MHz\n", EP93XX_EXT_CLK_RATE / 1000000);

	/* Determine the bootloader configured pll1 rate */
	regmap_read(ep93xx_map, EP93XX_SYSCON_CLKSET1, &value);
	if (!(value & EP93XX_SYSCON_CLKSET1_NBYP1)) {
		pr_info("pll1 : setting clk_xtali.rate\n");
		clk_pll1_rate = EP93XX_EXT_CLK_RATE;
	} else {
		pr_info("pll1 : setting calc_pll_rate\n");
		clk_pll1_rate = calc_pll_rate(value);
	}

	hw = clk_hw_register_fixed_rate(NULL, "pll1", "xtali", 0, clk_pll1_rate);
	ep93xx_clk_data->hws[EP93XX_CLK_PLL1] = hw;

	/* Initialize the pll1 derived clocks */
	// clk_f_rate = clk_pll1_rate / fclk_divisors[(value >> 25) & 0x7];
	// clk_h_rate = clk_pll1_rate / hclk_divisors[(value >> 20) & 0x7];
	// clk_p_rate = clk_h_rate / pclk_divisors[(value >> 18) & 0x3];

	clk_f_div = fclk_divisors[(value >> 25) & 0x7];
	clk_h_div = hclk_divisors[(value >> 20) & 0x7];
	clk_p_div = pclk_divisors[(value >> 18) & 0x3];

	hw = clk_hw_register_fixed_factor(NULL, "fclk", "pll1", 0, 1, clk_f_div);
	ep93xx_clk_data->hws[EP93XX_CLK_FCLK] = hw;

	hw = clk_hw_register_fixed_factor(NULL, "hclk", "pll1", 0, 1, clk_h_div);
	ep93xx_clk_data->hws[EP93XX_CLK_HCLK] = hw;

	/* register DMA gates */
	// parenting DMA clocks to hclk 
	for (i = 0; i < ARRAY_SIZE(ep93xx_dmas); i++) {
		hw = clk_hw_register_gate(NULL, ep93xx_dmas[i].con_id, 
					"hclk", 0, 
					base + EP93XX_SYSCON_DEVCFG,
					ep93xx_dmas[i].bit,
					0,
					&syscon_swlock);
		
		ep93xx_clk_data->hws[ep93xx_dmas[i].idx] = hw;
		
		ret = clk_hw_register_clkdev(hw, ep93xx_dmas[i].con_id, NULL);
		if (ret)
			pr_err("%s: failed to register lookup %s\n",
			       __func__, ep93xx_dmas[i].con_id);
	}

	hw = clk_hw_register_fixed_factor(NULL, "pclk", "hclk", 0, 1, clk_p_div);
	ep93xx_clk_data->hws[EP93XX_CLK_PCLK] = hw;

	/* Determine the bootloader configured pll2 rate */
	ret = regmap_read(ep93xx_map, EP93XX_SYSCON_CLKSET2, &value);
	if (!(value & EP93XX_SYSCON_CLKSET2_NBYP2)) {
		pr_info("pll2 : setting clk_xtali.rate\n");
		clk_pll2_rate = EP93XX_EXT_CLK_RATE;
	} else if (value & EP93XX_SYSCON_CLKSET2_PLL2_EN) {
		pr_info("pll2 : setting calc_pll_rate\n");
		clk_pll2_rate = calc_pll_rate(value);
	} else {
		pr_info("pll2 : setting zero\n");
		clk_pll2_rate = 0;
	}

	hw = clk_hw_register_fixed_rate(NULL, "pll2", "xtali", 0, clk_pll2_rate);
	ep93xx_clk_data->hws[EP93XX_CLK_PLL2] = hw;

	/* Initialize the pll2 derived clocks */
	// clk_usb_host.rate = clk_pll2.rate / (((value >> 28) & 0xf) + 1);

	/*
	 * EP93xx SSP clock rate was doubled in version E2. For more information
	 * see:
	 *     http://www.cirrus.com/en/pubs/appNote/AN273REV4.pdf
	 */
	if (ep93xx_chip_revision() < EP93XX_CHIP_REV_E2)
		clk_spi.rate /= 2;

	of_clk_add_hw_provider(np, of_clk_hw_onecell_get, ep93xx_clk_data);

	pr_info("PLL1 running at %ld MHz, PLL2 at %ld MHz\n",
		clk_pll1_rate / 1000000, clk_pll2_rate / 1000000);
	pr_info("FCLK %ld MHz, HCLK %ld MHz, PCLK %ld MHz\n",
		clk_f_rate / 1000000, clk_h_rate / 1000000, clk_p_rate / 1000000);
}
CLK_OF_DECLARE_DRIVER(ep93xx_cc, "cirrus,ep93xx-syscon", ep93xx_clock_init);
