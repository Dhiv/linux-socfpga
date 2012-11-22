/*
 * IPMMU/IPMMUI
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

#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <mach/ipmmu.h>
#include <linux/ipmmu.h>

#define IMCTR1		0x00
#define IMCTR1_TLBEN (1 << 0)
#define IMCTR1_FLUSH (1 << 1)
#define IMCTR2		0x04
#define IMCTR2_PMBEN	0x01
#define IMASID		0x010
#define IMTTBR		0x014
#define IMTTBCR		0x018
#define IMPMBA_BASE	0x80
#define IMPBMA_V	(1 << 8)
#define IMPMBD_BASE	0xC0
#define IMPBMD_V	(1 << 8)
#define IMPBMD_SZ_16M	0x00
#define IMPBMD_SZ_64M	0x10
#define IMPBMD_SZ_128M	0x80
#define IMPBMD_SZ_512M	0x90
#define IMPBMD_BV	(1 << 9)
#define IMPBMD_TBM_MASK	(7 << 12)
#define IMPBMD_TBM_POS	12
#define IMPBMD_HBM_MASK	(7 << 16)
#define IMPBMD_HBM_POS	16
#define IMPBMD_VBM_MASK	(7 << 20)
#define IMPBMD_VBM_POS	20

#define IMPBMA(x) (IMPMBA_BASE + 0x4*x)
#define IMPBMD(x) (IMPMBD_BASE + 0x4*x)

/* the smallest size that can be reserverd in the pmb */
#define PMB_GRANULARITY (16 << 20)
#define PMB_START_ADDR	0x80000000
#define PMB_SIZE	0x40000000
#define NUM_BITS(x)	((x) / PMB_GRANULARITY)
#define NR_BITMAPS	((NUM_BITS(PMB_SIZE) + BITS_PER_LONG - 1) \
				>> ilog2(BITS_PER_LONG))

struct ipmmu_priv {
	struct device	*dev;
	void __iomem	*ipmmu_base;
	int		tlb_enabled;
	struct mutex	flush_lock;
	struct mutex	alloc_lock;
	unsigned long	alloc_bitmaps[NR_BITMAPS];
	void		*pmb_priv;
};

static int valid_size(int size_mb)
{
	switch (size_mb) {
	case 16:
	case 64:
	case 128:
	case 512:
		return 1;
	}
	return 0;

}
unsigned long handle_alloc(struct device *dev,
				     int size_mb)
{
	int i;
	int idx;
	unsigned long tmp_bitmap;
	unsigned long alloc_mask;
	unsigned long align_mask;
	int alloc_bits;
	struct ipmmu_priv *priv;

	if (!valid_size(size_mb))
		return -1;

	priv = dev_get_drvdata(dev);

	alloc_bits = NUM_BITS(size_mb << 20);
	alloc_mask = alloc_bits < BITS_PER_LONG ?
				(1 << alloc_bits) - 1 : -1;


	align_mask = alloc_mask - 1;
	for (i = BITS_PER_LONG >> 1; i >= alloc_bits; i = i >> 1)
		align_mask = align_mask | (align_mask << i);

	mutex_lock(&priv->alloc_lock);
	for (i = 0; i < NR_BITMAPS; i++) {
		tmp_bitmap = priv->alloc_bitmaps[i];
		tmp_bitmap |= align_mask;
		idx = 0;
		while (idx < BITS_PER_LONG) {
			idx = find_next_zero_bit(&tmp_bitmap, BITS_PER_LONG,
					idx);
			if (!((alloc_mask << idx) & priv->alloc_bitmaps[i]) ||
					(idx == BITS_PER_LONG))
				break;
			idx++;
		}
		if (idx < BITS_PER_LONG)
			break;
	}
	if (i == NR_BITMAPS) {
		mutex_unlock(&priv->alloc_lock);
		return 0;
	}

	priv->alloc_bitmaps[i] |= (alloc_mask << idx);
	mutex_unlock(&priv->alloc_lock);

	return PMB_START_ADDR + (i * BITS_PER_LONG + idx) * PMB_GRANULARITY;
}

void handle_free(struct device *dev,
			unsigned long handle,
			int size_mb)
{
	int idx;
	unsigned long offset;
	unsigned long alloc_bits;
	unsigned long alloc_mask;
	struct ipmmu_priv *priv;

	priv = dev_get_drvdata(dev);

	alloc_bits = NUM_BITS(size_mb << 20);
	alloc_mask = alloc_bits < BITS_PER_LONG ?
				(1 << alloc_bits) - 1 : -1;
	offset = handle - PMB_START_ADDR;
	offset /= PMB_GRANULARITY;
	idx = offset & (BITS_PER_LONG - 1);
	offset = offset / BITS_PER_LONG;
	mutex_lock(&priv->alloc_lock);
	priv->alloc_bitmaps[offset] &= ~(alloc_mask << idx);
	mutex_unlock(&priv->alloc_lock);
}

static void ipmmu_reg_write(struct ipmmu_priv *priv, unsigned long reg_off,
			    unsigned long data)
{
	iowrite32(data, priv->ipmmu_base + reg_off);
}

int ipmmu_pmb_enable(struct device *dev,
		      int enable)
{

	struct ipmmu_priv *priv;

	priv = dev_get_drvdata(dev);
	ipmmu_reg_write(priv, IMCTR2, enable ? IMCTR2_PMBEN : 0);
	return 0;

}
int ipmmu_pmb_set_addr(struct device *dev,
		 int index,
		 unsigned long addr,
		 int enabled)
{

	struct ipmmu_priv *priv;

	priv = dev_get_drvdata(dev);
	if (!enabled) {
		ipmmu_reg_write(priv, IMPBMA(index), 0);
		return 0;
	}

	ipmmu_reg_write(priv, IMPBMA(index), addr |
		IMPBMD_V);
	return 0;

}

int ipmmu_pmb_set_data(struct device *dev,
		 int index,
		 struct ipmmu_pmb_info *info,
		 struct pmb_tile_info *tile)
{
	int vbm, hbm, tbm;
	int w, h;
	unsigned long temp;
	struct ipmmu_priv *priv;

	priv = dev_get_drvdata(dev);

	if (!info || !info->enabled) {
		ipmmu_reg_write(priv, IMPBMD(index), 0);
		return 0;
	}

	temp = info->paddr;

	switch (info->size_mb) {
	case 16:
		temp |= IMPBMD_SZ_16M;
		break;
	case 64:
		temp |= IMPBMD_SZ_64M;
		break;
	case 128:
		temp |= IMPBMD_SZ_128M;
		break;
	case 512:
		temp |= IMPBMD_SZ_512M;
		break;
	default:
		break;
	}

	temp |= IMPBMD_V;

	if (!tile || !tile->enabled) {
		ipmmu_reg_write(priv, IMPBMD(index), temp);
		return 0;
	}

	w = tile->tile_width;
	h = tile->tile_height;

	if (w & (w - 1) || h & (h - 1))
		return -EINVAL;

	tbm = ilog2(tile->tile_width);
	vbm = ilog2(tile->tile_height) - 1;
	hbm = ilog2(tile->buffer_pitch) - tbm - 1;
	tbm -= 4;

	temp |= (tbm << IMPBMD_TBM_POS) & IMPBMD_TBM_MASK;
	temp |= (vbm << IMPBMD_VBM_POS) & IMPBMD_VBM_MASK;
	temp |= (hbm << IMPBMD_HBM_POS) & IMPBMD_HBM_MASK;
	temp |= IMPBMD_BV;
	ipmmu_reg_write(priv, IMPBMD(index),
			temp);
	ipmmu_tlb_flush(priv->dev);
	return 0;
}

void ipmmu_tlb_flush(struct device *dev)
{
	struct ipmmu_priv *priv;

	if (!dev)
		return;
	priv = dev_get_drvdata(dev);
	mutex_lock(&priv->flush_lock);
	if (priv->tlb_enabled)
		ipmmu_reg_write(priv, IMCTR1, IMCTR1_FLUSH | IMCTR1_TLBEN);
	else
		ipmmu_reg_write(priv, IMCTR1, IMCTR1_FLUSH);
	mutex_unlock(&priv->flush_lock);
}

void ipmmu_tlb_set(struct device *dev, unsigned long phys, int size, int asid)
{
	struct ipmmu_priv *priv;

	if (!dev)
		return;
	priv = dev_get_drvdata(dev);
	mutex_lock(&priv->flush_lock);
	switch (size) {
	default:
		priv->tlb_enabled = 0;
		break;
	case 0x2000:
		ipmmu_reg_write(priv, IMTTBCR, 1);
		priv->tlb_enabled = 1;
		break;
	case 0x1000:
		ipmmu_reg_write(priv, IMTTBCR, 2);
		priv->tlb_enabled = 1;
		break;
	case 0x800:
		ipmmu_reg_write(priv, IMTTBCR, 3);
		priv->tlb_enabled = 1;
		break;
	case 0x400:
		ipmmu_reg_write(priv, IMTTBCR, 4);
		priv->tlb_enabled = 1;
		break;
	case 0x200:
		ipmmu_reg_write(priv, IMTTBCR, 5);
		priv->tlb_enabled = 1;
		break;
	case 0x100:
		ipmmu_reg_write(priv, IMTTBCR, 6);
		priv->tlb_enabled = 1;
		break;
	case 0x80:
		ipmmu_reg_write(priv, IMTTBCR, 7);
		priv->tlb_enabled = 1;
		break;
	}
	ipmmu_reg_write(priv, IMTTBR, phys);
	ipmmu_reg_write(priv, IMASID, asid);
	mutex_unlock(&priv->flush_lock);
}

static int __devinit ipmmu_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct ipmmu_priv *priv;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "cannot get platform resources\n");
		return -ENOENT;
	}
	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&pdev->dev, "cannot allocate device data\n");
		return -ENOMEM;
	}
	mutex_init(&priv->flush_lock);
	mutex_init(&priv->alloc_lock);

	priv->ipmmu_base = ioremap_nocache(res->start, resource_size(res));
	if (!priv->ipmmu_base) {
		dev_err(&pdev->dev, "ioremap_nocache failed\n");
		kfree(priv);
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, priv);
	priv->dev = &pdev->dev;
	ipmmu_reg_write(priv, IMCTR1, 0x0); /* disable TLB */
	ipmmu_reg_write(priv, IMCTR2, 0x0); /* disable PMB */
	ipmmu_iommu_init(&pdev->dev);
	priv->pmb_priv = ipmmu_pmb_init(&pdev->dev);
	return 0;
}

static struct platform_driver ipmmu_driver = {
	.probe = ipmmu_probe,
	.driver = {
		.owner = THIS_MODULE,
		.name = "ipmmu",
	},
};

static int __init ipmmu_init(void)
{
	return platform_driver_register(&ipmmu_driver);
}
subsys_initcall(ipmmu_init);
