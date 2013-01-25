#ifndef __CORE_H
#define __CORE_H

#define SOCFPGA_RSTMGR_CTRL	0x04
#define SOCFPGA_RSTMGR_MODPERRST	0x14
#define SOCFPGA_RSTMGR_BRGMODRST	0x1c

/* System Manager bits */
#define RSTMGR_CTRL_SWCOLDRSTREQ	0x1	/* Cold Reset */
#define RSTMGR_CTRL_SWWARMRSTREQ	0x2	/* Warm Reset */

#define SYSMGR_EMACGRP_CTRL_OFFSET 0x60
#define SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_GMII_MII 0x0
#define SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RGMII 0x1
#define SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RMII 0x2
#define SYSMGR_EMACGRP_CTRL_PHYSEL_WIDTH 2

#define SYSMGR_EMACGRP_CTRL_PHYSEL_MASK 0x00000003

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
