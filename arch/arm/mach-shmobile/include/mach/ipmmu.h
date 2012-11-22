#ifdef CONFIG_SHMOBILE_IPMMU_TLB
void ipmmu_tlb_flush(struct device *ipmmu_dev);
void ipmmu_tlb_set(struct device *ipmmu_dev, unsigned long phys, int size,
		   int asid);
void ipmmu_add_device(struct device *dev);
int ipmmu_iommu_init(struct device *dev);
#else
static inline void ipmmu_add_device(struct device *dev)
{
}

static int ipmmu_iommu_init(struct device *dev)
{
	return -EINVAL;
}
#endif
