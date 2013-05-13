/*
 *  arch/arm/mach-socfpga/include/mach/uncompress.h
 *
 *  Copyright (C) 2011-2013 Altera Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <linux/serial_reg.h>

#define SOCFPGA_UART0_BASE 		0xffc02000

#define UART8250_UART_THR(base)	(*(volatile unsigned char *)((base) + 0x0))
#define UART8250_UART_LSR(base)	(*(volatile unsigned char *)((base) + 0x14))

/*
 * Return the UART base address
 */
static inline unsigned long get_uart_base(void)
{
	return SOCFPGA_UART0_BASE;
}

/*
 * This does not append a newline
 */
static inline void putc(int c)
{
	unsigned long base = get_uart_base();

	while ((UART8250_UART_LSR(base) & UART_LSR_THRE) == 0)
		barrier();

	UART8250_UART_THR(base) = c;
}

/*
 * Not implemented
 */
static inline void flush(void)
{
}

#define arch_decomp_setup()
#define arch_decomp_wdog()

