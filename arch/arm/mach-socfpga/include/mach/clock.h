#ifndef __MACH_CLOCK_H
#define __MACH_CLOCK_H

#include <asm/hardware/icst.h>

struct clk;

struct clk_ops {
	long	(*round)(struct clk *, unsigned long);
	int	(*set)(struct clk *, unsigned long);
	void	(*setvco)(struct clk *, struct icst_vco);
};

#endif

