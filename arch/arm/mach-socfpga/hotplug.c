/*
 * Copyright 2011 Freescale Semiconductor, Inc.
 * Copyright 2011 Linaro Ltd.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/errno.h>
#include <linux/io.h>
#include <asm/cacheflush.h>
#include <asm/cp15.h>
#include "core.h"

static inline void cpu_enter_lowpower(void)
{
}

static inline void cpu_leave_lowpower(void)
{
}

int platform_cpu_kill(unsigned int cpu)
{
	return 1;
}

/*
 * platform-specific code to shutdown a CPU
 *
 * Called with IRQs disabled
 */
void platform_cpu_die(unsigned int cpu)
{
	/* Flush the L1 data cache. */
	flush_cache_all();

	/* This will put CPU #1 into reset.*/
	__raw_writel(RSTMGR_MPUMODRST_CPU1, rst_manager_base_addr + 0x10);

	cpu_do_idle();

        /* We should have never returned from idle */
	panic("cpu %d unexpectedly exit from shutdown\n", cpu);
}

int platform_cpu_disable(unsigned int cpu)
{
	/*
	 * CPU0 should not be shut down via hotplug.  cpu_idle can WFI
	 * or a proper shutdown or hibernate should be used.
	 */
	return cpu == 0 ? -EPERM : 0;
}
