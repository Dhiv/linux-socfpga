#include <linux/ipmmu.h>
#ifndef __SHMOBILE_IPMMU_H__
#define __SHMOBILE_IPMMU_H__

#ifdef CONFIG_SHMOBILE_IPMMU
void ipmmu_tlb_flush(struct device *ipmmu_dev);
#endif

#ifdef CONFIG_SHMOBILE_IPMMU_TLB
void ipmmu_tlb_set(struct device *ipmmu_dev, unsigned long phys, int size,
		   int asid);
void ipmmu_add_device(struct device *dev);
int ipmmu_iommu_init(struct device *dev);
#else
static inline void ipmmu_add_device(struct device *dev)
{
}

static inline int ipmmu_iommu_init(struct device *dev)
{
	return -EINVAL;
}
#endif
#ifdef CONFIG_SHMOBILE_PMB
/* Access functions used by PMB device */
void handle_free(struct device *dev, unsigned long handle, int size_mb);
unsigned long handle_alloc(struct device *dev, int size_mb);
int ipmmu_pmb_set_addr(struct device *dev, int index, unsigned long addr,
		 int enabled);
int ipmmu_pmb_set_data(struct device *dev, int index,
		 struct ipmmu_pmb_info *info,
		 struct pmb_tile_info *tile);
int ipmmu_pmb_enable(struct device *dev, int index);
/* PMB initialization */
void *ipmmu_pmb_init(struct device *dev);
void ipmmu_pmb_deinit(void *pmb_priv);
#else
static inline void *ipmmu_pmb_init(struct device *dev)
{
	return NULL;
}
static inline void ipmmu_pmb_deinit(void *pmb_priv)
{
}
#endif
#endif /* __SHMOBILE_IPMMU_H__ */
