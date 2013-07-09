/*
 *  linux/arch/arm/mach-socfpga/clock.c
 *
 *  Copyright (C) 2004 ARM Limited.
 *  Written by Deep Blue Solutions Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/clkdev.h>

struct clk {
        unsigned long rate;
};

int clk_enable(struct clk *clk)
{
	return 0;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
}
EXPORT_SYMBOL(clk_disable);

unsigned long clk_get_rate(struct clk *clk)
{
	return clk->rate;
}
EXPORT_SYMBOL(clk_get_rate);

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	return clk->rate;
}
EXPORT_SYMBOL(clk_round_rate);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	return 0;
}
EXPORT_SYMBOL(clk_set_rate);

static struct clk main_pll = { .rate = 1600000000 };
static struct clk per_clk = { .rate = 1000000000 };

static struct clk_lookup lookups[] = {
	{ .clk = &per_clk, .con_id = "pclk", },
	{ .clk = &per_clk, .dev_id = "fff00000.spi", },
	{ .clk = &per_clk, .dev_id = "fff01000.spi", },
	{ .clk = &per_clk, .dev_id = "ffc04000.i2c", },
	{ .clk = &per_clk, .dev_id = "ffc05000.i2c", },
	{ .clk = &per_clk, .dev_id = "ffc06000.i2c", },
	{ .clk = &per_clk, .dev_id = "ffc07000.i2c", },
};

void __init socfpga_init_clocks(void)
{
	clkdev_add_table(lookups, ARRAY_SIZE(lookups));
}

