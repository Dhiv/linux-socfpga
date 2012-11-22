/*
 * IOMMU for IPMMU/IPMMUI
 * Copyright (C) 2012  Hideki EIRAKU
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <linux/io.h>
#include <linux/dmapool.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/iommu.h>
#include <linux/dma-mapping.h>
#include <mach/ipmmu.h>
#include <asm/dma-iommu.h>

#define L1_SIZE CONFIG_SHMOBILE_IOMMU_L1SIZE
#define L1_LEN (L1_SIZE / 4)
#define L1_ALIGN L1_SIZE
#define L2_SIZE 0x400
#define L2_LEN (L2_SIZE / 4)
#define L2_ALIGN L2_SIZE

struct shmobile_iommu_priv_pgtable {
	uint32_t *pgtable;
	dma_addr_t handle;
};

struct shmobile_iommu_priv {
	struct shmobile_iommu_priv_pgtable l1, l2[L1_LEN];
	spinlock_t map_lock;
	atomic_t active;
};

static struct dma_iommu_mapping *iommu_mapping;
static struct device *ipmmu_devices;
static struct dma_pool *l1pool, *l2pool;
static spinlock_t lock;
static DEFINE_SPINLOCK(lock_add);
static struct shmobile_iommu_priv *attached;
static int num_attached_devices;
static struct device *ipmmu_access_device;

static int shmobile_iommu_domain_init(struct iommu_domain *domain)
{
	struct shmobile_iommu_priv *priv;
	int i;

	priv = kmalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	priv->l1.pgtable = dma_pool_alloc(l1pool, GFP_KERNEL,
					  &priv->l1.handle);
	if (!priv->l1.pgtable) {
		kfree(priv);
		return -ENOMEM;
	}
	for (i = 0; i < L1_LEN; i++)
		priv->l2[i].pgtable = NULL;
	memset(priv->l1.pgtable, 0, L1_SIZE);
	spin_lock_init(&priv->map_lock);
	atomic_set(&priv->active, 0);
	domain->priv = priv;
	return 0;
}

static void shmobile_iommu_domain_destroy(struct iommu_domain *domain)
{
	struct shmobile_iommu_priv *priv = domain->priv;
	int i;

	for (i = 0; i < L1_LEN; i++) {
		if (priv->l2[i].pgtable)
			dma_pool_free(l2pool, priv->l2[i].pgtable,
				      priv->l2[i].handle);
	}
	dma_pool_free(l1pool, priv->l1.pgtable, priv->l1.handle);
	kfree(priv);
	domain->priv = NULL;
}

static int shmobile_iommu_attach_device(struct iommu_domain *domain,
					struct device *dev)
{
	struct shmobile_iommu_priv *priv = domain->priv;
	int ret = -EBUSY;

	spin_lock(&lock);
	if (attached != priv) {
		if (attached)
			goto err;
		atomic_set(&priv->active, 1);
		ipmmu_tlb_set(ipmmu_access_device, priv->l1.handle, L1_SIZE,
			      0);
		wmb();
		ipmmu_tlb_flush(ipmmu_access_device);
		attached = priv;
		num_attached_devices = 0;
	}
	num_attached_devices++;
	ret = 0;
err:
	spin_unlock(&lock);
	return ret;
}

static void shmobile_iommu_detach_device(struct iommu_domain *domain,
					 struct device *dev)
{
	struct shmobile_iommu_priv *priv = domain->priv;

	spin_lock(&lock);
	atomic_set(&priv->active, 0);
	num_attached_devices--;
	if (!num_attached_devices) {
		ipmmu_tlb_set(ipmmu_access_device, 0, 0, 0);
		ipmmu_tlb_flush(ipmmu_access_device);
		attached = NULL;
	}
	spin_unlock(&lock);
}

static int
l2alloc(struct shmobile_iommu_priv *priv, unsigned int l1index)
{
	if (!priv->l2[l1index].pgtable) {
		priv->l2[l1index].pgtable = dma_pool_alloc(l2pool, GFP_KERNEL,
						&priv->l2[l1index].handle);
		if (!priv->l2[l1index].pgtable)
			return -ENOMEM;
		memset(priv->l2[l1index].pgtable, 0, L2_SIZE);
	}
	priv->l1.pgtable[l1index] = priv->l2[l1index].handle | 0x1;
	return 0;
}

static void
l2realfree(struct shmobile_iommu_priv_pgtable *l2)
{
	if (l2->pgtable)
		dma_pool_free(l2pool, l2->pgtable, l2->handle);
}

static int
l2free(struct shmobile_iommu_priv *priv, unsigned int l1index,
	struct shmobile_iommu_priv_pgtable *l2)
{
	priv->l1.pgtable[l1index] = 0;
	if (priv->l2[l1index].pgtable) {
		*l2 = priv->l2[l1index];
		priv->l2[l1index].pgtable = NULL;
	}
	return 0;
}

static int shmobile_iommu_map(struct iommu_domain *domain, unsigned long iova,
			      phys_addr_t paddr, size_t size, int prot)
{
	struct shmobile_iommu_priv_pgtable l2 = { .pgtable = NULL };
	struct shmobile_iommu_priv *priv = domain->priv;
	unsigned int l1index, l2index, i;
	int ret;

	l1index = iova >> 20;
	switch (size) {
	case 0x1000:
		l2index = (iova >> 12) & 0xff;
		spin_lock(&priv->map_lock);
		ret = l2alloc(priv, l1index);
		if (!ret)
			priv->l2[l1index].pgtable[l2index] = paddr | 0xff2;
		spin_unlock(&priv->map_lock);
		break;
	case 0x10000:
		l2index = (iova >> 12) & 0xf0;
		spin_lock(&priv->map_lock);
		ret = l2alloc(priv, l1index);
		if (!ret) {
			for (i = 0; i < 0x10; i++)
				priv->l2[l1index].pgtable[l2index + i] =
					paddr | 0xff1;
		}
		spin_unlock(&priv->map_lock);
		break;
	case 0x100000:
		spin_lock(&priv->map_lock);
		l2free(priv, l1index, &l2);
		priv->l1.pgtable[l1index] = paddr | 0xc02;
		spin_unlock(&priv->map_lock);
		ret = 0;
		break;
	default:
		ret = -EINVAL;
	}
	if (!ret && atomic_read(&priv->active)) {
		wmb();
		ipmmu_tlb_flush(ipmmu_access_device);
		l2realfree(&l2);
	}
	return ret;
}

static size_t shmobile_iommu_unmap(struct iommu_domain *domain,
				   unsigned long iova, size_t size)
{
	struct shmobile_iommu_priv_pgtable l2 = { .pgtable = NULL };
	struct shmobile_iommu_priv *priv = domain->priv;
	unsigned int l1index, l2index, i;
	uint32_t l2entry = 0;
	size_t ret = 0;

	l1index = iova >> 20;
	if (!(iova & 0xFFFFF) && size >= 0x100000) {
		spin_lock(&priv->map_lock);
		l2free(priv, l1index, &l2);
		spin_unlock(&priv->map_lock);
		ret = 0x100000;
		goto done;
	}
	l2index = (iova >> 12) & 0xff;
	spin_lock(&priv->map_lock);
	if (priv->l2[l1index].pgtable)
		l2entry = priv->l2[l1index].pgtable[l2index];
	switch (l2entry & 3) {
	case 1:
		if (l2index & 0xf)
			break;
		for (i = 0; i < 0x10; i++)
			priv->l2[l1index].pgtable[l2index + i] = 0;
		ret = 0x10000;
		break;
	case 2:
		priv->l2[l1index].pgtable[l2index] = 0;
		ret = 0x1000;
		break;
	}
	spin_unlock(&priv->map_lock);
done:
	if (ret && atomic_read(&priv->active)) {
		wmb();
		ipmmu_tlb_flush(ipmmu_access_device);
		l2realfree(&l2);
	}
	return ret;
}

static phys_addr_t shmobile_iommu_iova_to_phys(struct iommu_domain *domain,
					       unsigned long iova)
{
	struct shmobile_iommu_priv *priv = domain->priv;
	uint32_t l1entry = 0, l2entry = 0;
	unsigned int l1index, l2index;

	l1index = iova >> 20;
	l2index = (iova >> 12) & 0xff;
	spin_lock(&priv->map_lock);
	if (priv->l2[l1index].pgtable)
		l2entry = priv->l2[l1index].pgtable[l2index];
	else
		l1entry = priv->l1.pgtable[l1index];
	spin_unlock(&priv->map_lock);
	switch (l2entry & 3) {
	case 1:
		return (l2entry & ~0xffff) | (iova & 0xffff);
	case 2:
		return (l2entry & ~0xfff) | (iova & 0xfff);
	default:
		if ((l1entry & 3) == 2)
			return (l1entry & ~0xfffff) | (iova & 0xfffff);
		return 0;
	}
}

static struct iommu_ops shmobile_iommu_ops = {
	.domain_init = shmobile_iommu_domain_init,
	.domain_destroy = shmobile_iommu_domain_destroy,
	.attach_dev = shmobile_iommu_attach_device,
	.detach_dev = shmobile_iommu_detach_device,
	.map = shmobile_iommu_map,
	.unmap = shmobile_iommu_unmap,
	.iova_to_phys = shmobile_iommu_iova_to_phys,
	.pgsize_bitmap = 0x111000,
};

static int shmobile_iommu_attach_all_devices(void)
{
	struct device *dev;
	int ret = 0;

	spin_lock(&lock_add);
	iommu_mapping = arm_iommu_create_mapping(&platform_bus_type, 0x0,
						 L1_LEN << 20, 0);
	if (IS_ERR_OR_NULL(iommu_mapping)) {
		ret = PTR_ERR(iommu_mapping);
		goto err;
	}
	for (dev = ipmmu_devices; dev; dev = dev->archdata.iommu) {
		if (arm_iommu_attach_device(dev, iommu_mapping))
			pr_err("arm_iommu_attach_device failed\n");
	}
err:
	spin_unlock(&lock_add);
	return 0;
}

void ipmmu_add_device(struct device *dev)
{
	spin_lock(&lock_add);
	dev->archdata.iommu = ipmmu_devices;
	ipmmu_devices = dev;
	if (!IS_ERR_OR_NULL(iommu_mapping)) {
		if (arm_iommu_attach_device(dev, iommu_mapping))
			pr_err("arm_iommu_attach_device failed\n");
	}
	spin_unlock(&lock_add);
}

int ipmmu_iommu_init(struct device *dev)
{
	dma_set_coherent_mask(dev, DMA_BIT_MASK(32));
	l1pool = dma_pool_create("shmobile-iommu-pgtable1", dev,
				 L1_SIZE, L1_ALIGN, 0);
	if (!l1pool)
		goto nomem_pool1;
	l2pool = dma_pool_create("shmobile-iommu-pgtable2", dev,
				 L2_SIZE, L2_ALIGN, 0);
	if (!l2pool)
		goto nomem_pool2;
	spin_lock_init(&lock);
	attached = NULL;
	ipmmu_access_device = dev;
	bus_set_iommu(&platform_bus_type, &shmobile_iommu_ops);
	if (shmobile_iommu_attach_all_devices())
		pr_err("shmobile_iommu_attach_all_devices failed\n");
	return 0;
nomem_pool2:
	dma_pool_destroy(l1pool);
nomem_pool1:
	return -ENOMEM;
}
