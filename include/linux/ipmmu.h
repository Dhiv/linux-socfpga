#ifndef __LINUX_IPMMU_PMB_H__
#define __LINUX_IPMMU_PMB_H__

struct ipmmu_pmb_info {
	int		enabled;
	unsigned long	paddr;
	int             size_mb;
};

struct pmb_tile_info {
	int             tile_width;
	int             tile_height;
	int             buffer_pitch;
	int             enabled;
};

/* IOCTL commands. */

#define IPMMU_SET_PMB		_IOW('S', 37, struct ipmmu_pmb_phys *)
#define IPMMU_GET_PMB		_IOR('S', 38, struct ipmmu_pmb_phys *)
#define IPMMU_GET_PMB_HANDLE	_IOR('S', 39, unsigned long *)
#define IPMMU_SET_PMB_TL	_IOW('S', 41, struct ipmmu_pmb_tile_info *)
#define IPMMU_GET_PMB_TL	_IOR('S', 42, struct ipmmu_pmb_tile_info *)

#ifdef __kernel

#endif /* __kernel */

#endif /* __LINUX_IPMMU_PMB_H__ */
