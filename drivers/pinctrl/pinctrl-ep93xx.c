/*
 * Driver for the EP93xx pin controller
 *
 * Copyright (C) 2021 Nikita Shubin <nikita.shubin@maquefel.me>
 *
 * This is a group-only pin controller.
 */

// There are several system configuration options selectable by the DeviceCfg and SysCfg
// registers. These registers provide the selection of several pin multiplexing options and also
// provide software access to the system reset configuration options. Please refer to the
// descriptions of the registers, “DeviceCfg” on page 5-25 and “SysCfg” on page 5-34, for a
// detailed explanation.



#define EP93XX_SYSCON_DEVCFG_I2SONAC97	BIT(6)
#define EP93XX_SYSCON_DEVCFG_I2SONSSP	BIT(7)
#define EP93XX_SYSCON_DEVCFG_PONG	BIT(9)
#define EP93XX_SYSCON_DEVCFG_KEYS	BIT(1)
#define EP93XX_SYSCON_DEVCFG_GONK	BIT(27)
#define EP93XX_SYSCON_DEVCFG_EONIDE	BIT(8)
#define EP93XX_SYSCON_DEVCFG_GONIDE	BIT(9)
#define EP93XX_SYSCON_DEVCFG_HONIDE	BIT(10)
#define EP93XX_SYSCON_DEVCFG_TIN	BIT(17)
#define EP93XX_SYSCON_DEVCFG_ADCPD	BIT(2)

#define EP93XX_SYSCON_DEVCFG_CPENA	BIT(23)

// I2SonAC97:
// Audio - I2S on AC97 pins. The I2S block uses the AC97
// pins. See Audio Interface pin assignments in Table 5-7.

// I2S
// #define EP93XX_SYSCON_DEVCFG_I2S_MASK	(EP93XX_SYSCON_DEVCFG_I2SONSSP | \
// EP93XX_SYSCON_DEVCFG_I2SONAC97)
// ep93xx_devcfg_set_clear(EP93XX_SYSCON_DEVCFG_I2SONAC97, EP93XX_SYSCON_DEVCFG_I2S_MASK);
// ep93xx_devcfg_clear_bits(EP93XX_SYSCON_DEVCFG_I2S_MASK);

/* Configure ep93xx's I2S to use AC97 pins */
// ep93xx_devcfg_set_bits(EP93XX_SYSCON_DEVCFG_I2SONAC97);

// SPI
// ep93xx_devcfg_clear_bits(EP93XX_SYSCON_DEVCFG_I2SONSSP);

// PWM
// ep93xx_devcfg_set_bits(EP93XX_SYSCON_DEVCFG_PONG);
// ep93xx_devcfg_clear_bits(EP93XX_SYSCON_DEVCFG_PONG);

// KEYPAD
/* Enable the keypad controller; GPIO ports C and D used for keypad */
// ep93xx_devcfg_clear_bits(EP93XX_SYSCON_DEVCFG_KEYS | EP93XX_SYSCON_DEVCFG_GONK);
/* Disable the keypad controller; GPIO ports C and D used for GPIO */
// ep93xx_devcfg_set_bits(EP93XX_SYSCON_DEVCFG_KEYS | EP93XX_SYSCON_DEVCFG_GONK);

/* Make sure that the AC97 pins are not used by I2S. */
// ep93xx_devcfg_clear_bits(EP93XX_SYSCON_DEVCFG_I2SONAC97);

// IDE
// ep93xx_devcfg_clear_bits(EP93XX_SYSCON_DEVCFG_EONIDE | EP93XX_SYSCON_DEVCFG_GONIDE |EP93XX_SYSCON_DEVCFG_HONIDE);

// ADC
/* Power up ADC, deactivate Touch Screen Controller */
// ep93xx_devcfg_set_clear(EP93XX_SYSCON_DEVCFG_TIN, EP93XX_SYSCON_DEVCFG_ADCPD);

/* Disallow access to MaverickCrunch initially */
// ep93xx_devcfg_clear_bits(EP93XX_SYSCON_DEVCFG_CPENA);

/* Default all ports to GPIO */
// ep93xx_devcfg_set_bits(EP93XX_SYSCON_DEVCFG_KEYS |
// EP93XX_SYSCON_DEVCFG_GONK |
// EP93XX_SYSCON_DEVCFG_EONIDE |
// EP93XX_SYSCON_DEVCFG_GONIDE |
// EP93XX_SYSCON_DEVCFG_HONIDE);

/* Ordered by bit index */
static const char * const ep93xx_padgroups[] = {
	"serial flash",
	"parallel flash",
	"NAND flash",
	"DRAM",
	"IDE",
	"PCI",
	"LPC",
	"LCD",
	"SSP",
	"TVC",
	NULL, NULL, NULL, NULL, NULL, NULL,
	"LPC CLK",
	"PCI CLK",
	NULL, NULL,
	"TVC CLK",
	NULL, NULL, NULL, NULL, NULL,
	"GMAC1",
};

/** ep9301, ep9302*/
static const struct pinctrl_pin_desc ep9301_pins[] = {
	PINCTRL_PIN(1, "CSn[7]"),
	PINCTRL_PIN(2, "CSn[6]"),
	PINCTRL_PIN(3, "CSn[3]"),
	PINCTRL_PIN(4, "CSn[2]"),
	PINCTRL_PIN(5, "CSn[1]"),
	PINCTRL_PIN(6, "AD[25]"),
	PINCTRL_PIN(7, "vdd_ring"),
	PINCTRL_PIN(8, "gnd_ring"),
	PINCTRL_PIN(9, "AD[24]"),
	PINCTRL_PIN(10, "SDCLK"),
	PINCTRL_PIN(11, "AD[23]"),
	PINCTRL_PIN(12, "vdd_core"),
	PINCTRL_PIN(13, "gnd_core"),
	PINCTRL_PIN(14, "SDWEn"),
	PINCTRL_PIN(15, "SDCSn[3]"),
	PINCTRL_PIN(16, "SDCSn[2]"),
	PINCTRL_PIN(17, "SDCSn[1]"),
	PINCTRL_PIN(18, "SDCSn[0]"),
	PINCTRL_PIN(19, "vdd_ring"),
	PINCTRL_PIN(20, "gnd_ring"),
	PINCTRL_PIN(21, "RASn"),
	PINCTRL_PIN(22, "CASn"),
	PINCTRL_PIN(23, "DQMn[1]"),
	PINCTRL_PIN(24, "DQMn[0]"),
	PINCTRL_PIN(25, "AD[22]"),
	PINCTRL_PIN(26, "AD[21]"),
	PINCTRL_PIN(27, "vdd_ring"),
	PINCTRL_PIN(28, "gnd_ring"),
	PINCTRL_PIN(29, "DA[15]"),
	PINCTRL_PIN(30, "AD[7]"),
	PINCTRL_PIN(31, "DA[14]"),
	PINCTRL_PIN(32, "AD[6]"),
	PINCTRL_PIN(33, "DA[13]"),
	PINCTRL_PIN(34, "vdd_core"),
	PINCTRL_PIN(35, "gnd_core"),
	PINCTRL_PIN(36, "AD[5]"),
	PINCTRL_PIN(37, "DA[12]"),
	PINCTRL_PIN(38, "AD[4]"),
	PINCTRL_PIN(39, "DA[11]"),
	PINCTRL_PIN(40, "AD[3]"),
	PINCTRL_PIN(41, "vdd_ring"),
	PINCTRL_PIN(42, "gnd_ring"),
	PINCTRL_PIN(43, "DA[10]"),
	PINCTRL_PIN(44, "AD[2]"),
	PINCTRL_PIN(45, "DA[9]"),
	PINCTRL_PIN(46, "AD[1]"),
	PINCTRL_PIN(47, "DA[8]"),
	PINCTRL_PIN(48, "AD[0]"),
	PINCTRL_PIN(49, "vdd_ring"),
	PINCTRL_PIN(50, "gnd_ring"),
	PINCTRL_PIN(51, "NC"),
	PINCTRL_PIN(52, "NC"),
	PINCTRL_PIN(53, "vdd_ring"),
	PINCTRL_PIN(54, "gnd_ring"),
	PINCTRL_PIN(55, "AD[15]"),
	PINCTRL_PIN(56, "DA[7]"),
	PINCTRL_PIN(57, "vdd_core"),
	PINCTRL_PIN(58, "gnd_core"),
	PINCTRL_PIN(59, "AD[14]"),
	PINCTRL_PIN(60, "DA[6]"),
	PINCTRL_PIN(61, "AD[13]"),
	PINCTRL_PIN(62, "DA[5]"),
	PINCTRL_PIN(63, "AD[12]"),
	PINCTRL_PIN(64, "DA[4]"),
	PINCTRL_PIN(65, "AD[11]"),
	PINCTRL_PIN(66, "vdd_ring"),
	PINCTRL_PIN(67, "gnd_ring"),
	PINCTRL_PIN(68, "DA[3]"),
	PINCTRL_PIN(69, "AD[10]"),
	PINCTRL_PIN(70, "DA[2]"),
	PINCTRL_PIN(71, "AD[9]"),
	PINCTRL_PIN(72, "DA[1]"),
	PINCTRL_PIN(73, "AD[8]"),
	PINCTRL_PIN(74, "DA[0]"),
	PINCTRL_PIN(75, "DSRn"),
	PINCTRL_PIN(76, "DTRn"),
	PINCTRL_PIN(77, "TCK"),
	PINCTRL_PIN(78, "TDI"),
	PINCTRL_PIN(79, "TDO"),
	PINCTRL_PIN(80, "TMS"),
	PINCTRL_PIN(81, "vdd_ring"),
	PINCTRL_PIN(82, "gnd_ring"),
	PINCTRL_PIN(83, "BOOT[1]"),
	PINCTRL_PIN(84, "BOOT[0]"),
	PINCTRL_PIN(85, "gnd_ring"),
	PINCTRL_PIN(86, "NC"),
	PINCTRL_PIN(87, "EECLK"),
	PINCTRL_PIN(88, "EEDAT"),
	PINCTRL_PIN(89, "ASYNC"),
	PINCTRL_PIN(90, "vdd_core"),
	PINCTRL_PIN(91, "gnd_core"),
	PINCTRL_PIN(92, "ASDO"),
	PINCTRL_PIN(93, "SCLK1"),
	PINCTRL_PIN(94, "SFRM1"),
	PINCTRL_PIN(95, "SSPRX1"),
	PINCTRL_PIN(96, "SSPTX1"),
	PINCTRL_PIN(97, "GRLED"),
	PINCTRL_PIN(98, "RDLED"),
	PINCTRL_PIN(99, "vdd_ring"),
	PINCTRL_PIN(100, "gnd_ring"),
	PINCTRL_PIN(101, "INT[3]"),
	PINCTRL_PIN(102, "INT[1]"),
	PINCTRL_PIN(103, "INT[0]"),
	PINCTRL_PIN(104, "RTSn"),
	PINCTRL_PIN(105, "USBm[0]"),
	PINCTRL_PIN(106, "USBp[0]"),
	PINCTRL_PIN(107, "ABITCLK"),
	PINCTRL_PIN(108, "CTSn"),
	PINCTRL_PIN(109, "RXD[0]"),
	PINCTRL_PIN(110, "RXD[1]"),
	PINCTRL_PIN(111, "vdd_ring"),
	PINCTRL_PIN(112, "gnd_ring"),
	PINCTRL_PIN(113, "TXD[0]"),
	PINCTRL_PIN(114, "TXD[1]"),
	PINCTRL_PIN(115, "CGPIO[0]"),
	PINCTRL_PIN(116, "gnd_core"),
	PINCTRL_PIN(117, "PLL_GND"),
	PINCTRL_PIN(118, "XTALI"),
	PINCTRL_PIN(119, "XTALO"),
	PINCTRL_PIN(120, "PLL_VDD"),
	PINCTRL_PIN(121, "vdd_core"),
	PINCTRL_PIN(122, "gnd_ring"),
	PINCTRL_PIN(123, "vdd_ring"),
	PINCTRL_PIN(124, "RSTOn"),
	PINCTRL_PIN(125, "PRSTn"),
	PINCTRL_PIN(126, "CSn[0]"),
	PINCTRL_PIN(127, "gnd_core"),
	PINCTRL_PIN(128, "vdd_core"),
	PINCTRL_PIN(129, "gnd_ring"),
	PINCTRL_PIN(130, "vdd_ring"),
	PINCTRL_PIN(131, "ADC[4]"),
	PINCTRL_PIN(132, "ADC[3]"),
	PINCTRL_PIN(133, "ADC[2]"),
	PINCTRL_PIN(134, "ADC[1]"),
	PINCTRL_PIN(135, "ADC[0]"),
	PINCTRL_PIN(136, "ADC_VDD"),
	PINCTRL_PIN(137, "RTCXTALI"),
	PINCTRL_PIN(138, "RTCXTALO"),
	PINCTRL_PIN(139, "ADC_GND"),
	PINCTRL_PIN(140, "EGPIO[11]"),
	PINCTRL_PIN(141, "EGPIO[10]"),
	PINCTRL_PIN(142, "EGPIO[9]"),
	PINCTRL_PIN(143, "EGPIO[8]"),
	PINCTRL_PIN(144, "EGPIO[7]"),
	PINCTRL_PIN(145, "EGPIO[6]"),
	PINCTRL_PIN(146, "EGPIO[5]"),
	PINCTRL_PIN(147, "EGPIO[4]"),
	PINCTRL_PIN(148, "EGPIO[3]"),
	PINCTRL_PIN(149, "gnd_ring"),
	PINCTRL_PIN(150, "vdd_ring"),
	PINCTRL_PIN(151, "EGPIO[2]"),
	PINCTRL_PIN(152, "EGPIO[1]"),
	PINCTRL_PIN(153, "EGPIO[0]"),
	PINCTRL_PIN(154, "ARSTn"),
	PINCTRL_PIN(155, "TRSTn"),
	PINCTRL_PIN(156, "ASDI"),
	PINCTRL_PIN(157, "USBm[2]"),
	PINCTRL_PIN(158, "USBp[2]"),
	PINCTRL_PIN(159, "WAITn"),
	PINCTRL_PIN(160, "EGPIO[15]"),
	PINCTRL_PIN(161, "gnd_ring"),
	PINCTRL_PIN(162, "vdd_ring"),
	PINCTRL_PIN(163, "EGPIO[14]"),
	PINCTRL_PIN(164, "EGPIO[13]"),
	PINCTRL_PIN(165, "EGPIO[12]"),
	PINCTRL_PIN(166, "gnd_core"),
	PINCTRL_PIN(167, "vdd_core"),
	PINCTRL_PIN(168, "FGPIO[3]"),
	PINCTRL_PIN(169, "FGPIO[2]"),
	PINCTRL_PIN(170, "FGPIO[1]"),
	PINCTRL_PIN(171, "gnd_ring"),
	PINCTRL_PIN(172, "vdd_ring"),
	PINCTRL_PIN(173, "CLD"),
	PINCTRL_PIN(174, "CRS"),
	PINCTRL_PIN(175, "TXERR"),
	PINCTRL_PIN(176, "TXEN"),
	PINCTRL_PIN(177, "MIITXD[0]"),
	PINCTRL_PIN(178, "MIITXD[1]"),
	PINCTRL_PIN(179, "MIITXD[2]"),
	PINCTRL_PIN(180, "MIITXD[3]"),
	PINCTRL_PIN(181, "TXCLK"),
	PINCTRL_PIN(182, "RXERR"),
	PINCTRL_PIN(183, "RXDVAL"),
	PINCTRL_PIN(184, "MIIRXD[0]"),
	PINCTRL_PIN(185, "MIIRXD[1]"),
	PINCTRL_PIN(186, "MIIRXD[2]"),
	PINCTRL_PIN(187, "gnd_ring"),
	PINCTRL_PIN(188, "vdd_ring"),
	PINCTRL_PIN(189, "MIIRXD[3]"),
	PINCTRL_PIN(190, "RXCLK"),
	PINCTRL_PIN(191, "MDIO"),
	PINCTRL_PIN(192, "MDC"),
	PINCTRL_PIN(193, "RDn"),
	PINCTRL_PIN(194, "WRn"),
	PINCTRL_PIN(195, "AD[16]"),
	PINCTRL_PIN(196, "AD[17]"),
	PINCTRL_PIN(197, "gnd_core"),
	PINCTRL_PIN(198, "vdd_core"),
	PINCTRL_PIN(199, "HGPIO[2]"),
	PINCTRL_PIN(200, "HGPIO[3]"),
	PINCTRL_PIN(201, "HGPIO[4]"),
	PINCTRL_PIN(202, "HGPIO[5]"),
	PINCTRL_PIN(203, "gnd_ring"),
	PINCTRL_PIN(204, "vdd_ring"),
	PINCTRL_PIN(205, "AD[18]"),
	PINCTRL_PIN(206, "AD[19]"),
	PINCTRL_PIN(207, "AD[20]"),
	PINCTRL_PIN(208, "SDCLKEN"),
};


// Pin	Normal Mode	i2s on SSP Mode	i2s on AC'97 Mode
// Name	Pin Description	Pin Description	Pin Description
// SCLK1	SPI Bit Clock	i2s Serial Clock	SPI Bit Clock
// SFRM1	SPI Frame Clock	i²S Frame Clock	SPI Frame Clock
// SSPRX1	SPI Serial Input	i2s Serial Input	SPI Serial Input
// SSPTX1	SPI Serial Output	i2s Serial Output	SPI Serial Output
// (No Rs Master Clock)	
// ARSTn	AC'97 Reset	AC'97 Reset	i2s Master Clock
// ABITCLK	AC'97 Bit Clock	AC'97 Bit Clock	i2s Serial Clock
// ASYNC	AC'97 Frame Clock	AC'97 Frame Clock	i2s Frame Clock
// ASDI	AC'97 Serial Input	AC'97 Serial Input	i2S Serial Input
// ASDO	AC'97 Serial Output	AC'97 Serial Output	i2s Serial Output

static const unsigned int ssp_pins[] = {
	93, 94, 95, 96
};

static const unsigned int i2s_on_ssp_pins[] = {
	93, 94, 95, 96
};

static const unsigned int ac97_pins[] = {
	154, 107, 89, 156, 92
};

static const unsigned int i2s_on_ac97_pins[] = {
	154, 107, 89, 156, 92
};

// Note: The EP9307 processor has one PWM with one output, PWMOUT.
// Note: The EP9301, EP9302, EP9312, and EP9315 processors each have two PWMs with
// two outputs, PWMOUT and PWMO1. PWMO1 is an alternate function for EGPIO14.

static const unsigned int pwm1_out[] = {
	163
};

/* Groups for the ep9301/ep9302 SoC/package */
static const struct ep93xx_pin_group ep9301_pin_groups[] = {
	{
		.name = "ssp",
		.pins = ssp_pins,
		.num_pins = ARRAY_SIZE(ssp_pins),
	},
	{
		.name = "i2s_on_ssp",
		.pins = ssp_pins,
		.num_pins = ARRAY_SIZE(ssp_pins),
	},
	{
		.name = "ac97",
		.pins = ac97_pins,
		.num_pins = ARRAY_SIZE(ac97_pins),
	},
	{
		.name = "i2s_on_ac97",
		.pins = ac97_pins,
		.num_pins = ARRAY_SIZE(ac97_pins),
	},
	{
		.name = "pwm1_out",
		.pins = pwm1_out,
		.num_pins = ARRAY_SIZE(pwm1_out),
	},
};

static int ep93xx_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct ep93xx_pmx *pmx = pinctrl_dev_get_drvdata(pctldev);

	if (pmx->is_9301 || pmx->is_9302)
		return ARRAY_SIZE(ep9301_pin_groups);
	return 0;
}

static const char *ep93xx_get_group_name(struct pinctrl_dev *pctldev,
					 unsigned int selector)
{
	struct ep93xx_pmx *pmx = pinctrl_dev_get_drvdata(pctldev);

	if (pmx->is_9301 || pmx->is_9302)
		return ep9301_pin_groups[selector].name;
	return NULL;
}

static int ep93xx_get_group_pins(struct pinctrl_dev *pctldev,
				 unsigned int selector,
				 const unsigned int **pins,
				 unsigned int *num_pins)
{
	struct ep93xx_pmx *pmx = pinctrl_dev_get_drvdata(pctldev);

	if (pmx->is_9301 || pmx->is_9302) {
		*pins = ep9301_pin_groups[selector].pins;
		*num_pins = ep9301_pin_groups[selector].num_pins;
	}
	return 0;
}

static void ep93xx_pin_dbg_show(struct pinctrl_dev *pctldev, struct seq_file *s,
				unsigned int offset)
{
	seq_printf(s, " " DRIVER_NAME);
}

static const struct pinctrl_ops ep93xx_pctrl_ops = {
	.get_groups_count = ep93xx_get_groups_count,
	.get_group_name = ep93xx_get_group_name,
	.get_group_pins = ep93xx_get_group_pins,
	.pin_dbg_show = ep93xx_pin_dbg_show,
	.dt_node_to_map = pinconf_generic_dt_node_to_map_all,
	.dt_free_map = pinconf_generic_dt_free_map,
};

static int ep93xx_pmx_set_mux(struct pinctrl_dev *pctldev,
			      unsigned int selector,
			      unsigned int group)
{
	return 0;
};

static int ep93xx_pmx_get_funcs_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(ep93xx_pmx_functions);
}

static const char *ep93xx_pmx_get_func_name(struct pinctrl_dev *pctldev,
					    unsigned int selector)
{
	return ep93xx_pmx_functions[selector].name;
}

static int ep93xx_pmx_get_groups(struct pinctrl_dev *pctldev,
				 unsigned int selector,
				 const char * const **groups,
				 unsigned int * const num_groups)
{
	*groups = ep93xx_pmx_functions[selector].groups;
	*num_groups = ep93xx_pmx_functions[selector].num_groups;
	return 0;
}

static const struct pinmux_ops ep93xx_pmx_ops = {
	.get_functions_count = ep93xx_pmx_get_funcs_count,
	.get_function_name = ep93xx_pmx_get_func_name,
	.get_function_groups = ep93xx_pmx_get_groups,
	.set_mux = ep93xx_pmx_set_mux,
};

static struct pinctrl_desc ep93xx_pmx_desc = {
	.name = DRIVER_NAME,
	.pctlops = &ep93xx_pctrl_ops,
	.pmxops = &ep93xx_pmx_ops,
	.owner = THIS_MODULE,
};

static int ep93xx_pmx_probe(struct platform_device *pdev)
{
	// syscon ?
	return 0;
};

static const struct of_device_id ep93xx_pinctrl_match[] = {
	{ .compatible = "cirrus,ep93xx-pinctrl" },
	{},
};

static struct platform_driver ep93xx_pmx_init = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = ep93xx_pinctrl_match,
	},
	.probe = ep93xx_pmx_probe,
};

static int __init ep93xx_pmx_init(void)
{
	return platform_driver_register(&ep93xx_pmx_init);
}
arch_initcall(ep93xx_pmx_init);
