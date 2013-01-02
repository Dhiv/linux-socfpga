#ifndef __CORE_H
#define __CORE_H

#define SOCFPGA_MODPERRST	0x14
#define SOCFPGA_BRGMODRST	0x1c

extern void secondary_startup(void);
extern void __iomem *socfpga_scu_base_addr;
extern void __iomem *sys_manager_base_addr;
extern void __iomem *rst_manager_base_addr;

extern void socfpga_secondary_startup(void);
extern void socfpga_cpu_die(unsigned int cpu);
extern void socfpga_init_clocks(void);

extern char secondary_trampoline, secondary_trampoline_end;

extern struct dw_mci_board sdmmc_platform_data;
extern unsigned long cpu1start_addr;
extern void v7_secondary_startup(void);

#define SOCFPGA_SCU_VIRT_BASE	0xfffec000
#define SOCFPGA_SDMMC_BASE	0xff704000

#endif /* __CORE_H */
