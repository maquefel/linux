/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Common EP93xx header providing
 * helper functions for accessing SYSCON_SWLOCK.
 *
 * Copyright (C) 2021 Nikita Shubin <nikita.shubin@maquefel.me>
 */

#ifndef _LINUX_EP93XX_H
#define _LINUX_EP93XX_H

#define EP93XX_SYSCON_DEVCFG	0x80
#define EP93XX_SYSCON_DEVCFG_I2SONSSP	BIT(7)

void ep93xx_syscon_swlocked_write(unsigned int val, unsigned int reg);
void ep93xx_devcfg_set_clear(unsigned int set_bits, unsigned int clear_bits, unsigned int reg);
unsigned int ep93xx_chip_revision(void);

#endif /* _LINUX_EP93XX_H */
