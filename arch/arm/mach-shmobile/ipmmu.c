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

#define IMCTR1 0x000
#define IMCTR2 0x004
#define IMASID 0x010
#define IMTTBR 0x014
#define IMTTBCR 0x018

#define IMCTR1_TLBEN (1 << 0)
#define IMCTR1_FLUSH (1 << 1)

struct ipmmu_priv {
	void __iomem *ipmmu_base;
	int tlb_enabled;
	struct mutex flush_lock;
};

static void ipmmu_reg_write(struct ipmmu_priv *priv, unsigned long reg_off,
			    unsigned long data)
{
	iowrite32(data, priv->ipmmu_base + reg_off);
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
	priv->ipmmu_base = ioremap_nocache(res->start, resource_size(res));
	if (!priv->ipmmu_base) {
		dev_err(&pdev->dev, "ioremap_nocache failed\n");
		kfree(priv);
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, priv);
	ipmmu_reg_write(priv, IMCTR1, 0x0); /* disable TLB */
	ipmmu_reg_write(priv, IMCTR2, 0x0); /* disable PMB */
	ipmmu_iommu_init(&pdev->dev);
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
