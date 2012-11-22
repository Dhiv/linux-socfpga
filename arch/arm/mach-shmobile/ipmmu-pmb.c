/*
 * IPMMU-PMB driver
 * Copyright (C) 2012 Damian Hobson-Garcia
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
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/ipmmu.h>
#include <mach/ipmmu.h>
#include <asm/uaccess.h>

#define PMB_DEVICE_NAME "pmb"

#define PMB_NR 16
struct ipmmu_pmb_data {
	struct ipmmu_pmb_priv	*priv;
	struct ipmmu_pmb_info	info;
	struct pmb_tile_info	tile;
	unsigned long		handle;
	int			index;
};

struct ipmmu_pmb_priv {
	struct cdev		cdev;
	struct class		*pmb_class;
	dev_t			pmb_dev;
	unsigned long		busy_pmbs;
	struct mutex		pmb_lock;
	struct ipmmu_pmb_data	pmbs[PMB_NR];
	struct device		*ipmmu_dev;
	int			pmb_enabled;
};

struct ipmmu_pmb_priv *static_priv;

static int set_pmb(struct ipmmu_pmb_data *data,
	    struct ipmmu_pmb_info *info)
{
	struct ipmmu_pmb_priv *priv = data->priv;
	unsigned long data_mask;
	int err;

	if (!info->enabled) {
		if (data->handle) {
			handle_free(priv->ipmmu_dev, data->handle,
				data->info.size_mb);
			data->handle = 0;
		}
		data->info = *info;
		ipmmu_pmb_set_data(priv->ipmmu_dev, data->index, NULL, NULL);
		ipmmu_pmb_set_addr(priv->ipmmu_dev, data->index, 0, 0);
		ipmmu_tlb_flush(priv->ipmmu_dev);
		return 0;
	}

	if (data->info.enabled) {
		err = -EBUSY;
		goto err_out;
	}

	data_mask = ~((info->size_mb) - 1);

	if (info->paddr & ~(data_mask)) {
		err = -EINVAL;
		goto err_out;
	}

	data->handle = handle_alloc(priv->ipmmu_dev, info->size_mb);

	if (!data->handle) {
		err = -ENOMEM;
		goto err_out;
	}

	data->info = *info;

	ipmmu_pmb_set_addr(priv->ipmmu_dev, data->index, data->handle, 1);
	ipmmu_pmb_set_data(priv->ipmmu_dev, data->index, &data->info,
		&data->tile);

	if (!data->priv->pmb_enabled) {
		ipmmu_pmb_enable(priv->ipmmu_dev, 1);
		data->priv->pmb_enabled = 1;
	}

	ipmmu_tlb_flush(priv->ipmmu_dev);

	return 0;

err_out:
	info->enabled = 0;
	return err;
}

static int set_tile(struct ipmmu_pmb_data *data,
	    struct pmb_tile_info *tile)
{
	struct ipmmu_pmb_priv *priv = data->priv;
	data->tile = *tile;
	return ipmmu_pmb_set_data(priv->ipmmu_dev, data->index, &data->info,
		&data->tile);
}

static int ipmmu_pmb_open(struct inode *inode, struct file *filp)
{
	struct ipmmu_pmb_priv *priv;
	int idx;
	priv = container_of(inode->i_cdev, struct ipmmu_pmb_priv,
		cdev);

	mutex_lock(&priv->pmb_lock);
	idx = find_first_zero_bit(&priv->busy_pmbs, PMB_NR);
	if (idx == PMB_NR)
		return -EBUSY;

	__set_bit(idx, &priv->busy_pmbs);
	mutex_unlock(&priv->pmb_lock);
	priv->pmbs[idx].index = idx;
	priv->pmbs[idx].priv = priv;
	filp->private_data = &priv->pmbs[idx];
	return 0;
}

static int ipmmu_pmb_release(struct inode *inode, struct file *filp)
{
	struct ipmmu_pmb_data *pmb;
	pmb = filp->private_data;
	if (pmb->info.enabled) {
		pmb->info.enabled = 0;
		set_pmb(pmb, &pmb->info);
	}

	mutex_lock(&pmb->priv->pmb_lock);
	__clear_bit(pmb->index, &pmb->priv->busy_pmbs);
	mutex_unlock(&pmb->priv->pmb_lock);
	return 0;
}

static long ipmmu_pmb_ioctl(struct file *filp, unsigned int cmd_in,
		       unsigned long arg)
{
	struct ipmmu_pmb_data *pmb;
	struct ipmmu_pmb_info user_set;
	struct pmb_tile_info user_tile;
	long ret = -EINVAL;
	pmb = filp->private_data;
	switch (cmd_in) {
	case IPMMU_GET_PMB:
		ret = copy_to_user((char *)arg, &pmb->info, sizeof(pmb->info));
		break;
	case IPMMU_SET_PMB:
		ret = copy_from_user(&user_set, (char *)arg, sizeof(user_set));
		if (ret)
			break;
		ret = set_pmb(pmb, &user_set);
		if (!ret)
			pmb->info = user_set;
		break;
	case IPMMU_GET_PMB_HANDLE:
		ret = copy_to_user((char *)arg, &pmb->handle,
			sizeof(pmb->handle));
		break;
	case IPMMU_GET_PMB_TL:
		ret = copy_to_user((char *)arg, &pmb->tile, sizeof(pmb->tile));
		break;
	case IPMMU_SET_PMB_TL:
		ret = copy_from_user(&user_tile, (char *)arg,
			sizeof(user_tile));
		if (ret)
			break;
		ret = set_tile(pmb, &user_tile);
		if (!ret)
			pmb->tile = user_tile;
		break;
	}
	return ret;
}

static const struct file_operations ipmmu_pmb_fops = {
	.owner = THIS_MODULE,
	.open = ipmmu_pmb_open,
	.release = ipmmu_pmb_release,
	.unlocked_ioctl = ipmmu_pmb_ioctl,
};

void ipmmu_pmb_deinit(void *arg)
{
	struct ipmmu_pmb_priv *priv = arg;

	if (!priv || IS_ERR(priv))
		return;

	cdev_del(&priv->cdev);
	device_destroy(priv->pmb_class, priv->pmb_dev);
	unregister_chrdev_region(priv->pmb_dev, 1);
	class_destroy(priv->pmb_class);
	kfree(priv);
}

void *ipmmu_pmb_init(struct device *ipmmu_dev)
{
	int err = -ENOENT;
	struct ipmmu_pmb_priv *priv;

	priv = kzalloc(sizeof(struct ipmmu_pmb_priv), GFP_KERNEL);
	if (!priv) {
		dev_err(ipmmu_dev, "cannot allocate device data\n");
		return ERR_PTR(-ENOMEM);
	}

	priv->ipmmu_dev = ipmmu_dev;
	static_priv = priv;

	mutex_init(&priv->pmb_lock);

	priv->pmb_class = class_create(THIS_MODULE, "ipmmu-pmb");
	if (!priv->pmb_class)
		goto free_priv;

	err = alloc_chrdev_region(&priv->pmb_dev, 0, 1, PMB_DEVICE_NAME);
	if (err) {
		dev_err(ipmmu_dev, "cannot allocate device num\n");
		goto destroy_class;
	}

	if (!device_create(priv->pmb_class, ipmmu_dev, priv->pmb_dev, priv,
			"pmb"))
		goto unregister_region;

	cdev_init(&priv->cdev, &ipmmu_pmb_fops);
	priv->cdev.owner = THIS_MODULE;
	priv->cdev.ops = &ipmmu_pmb_fops;
	err = cdev_add(&priv->cdev, priv->pmb_dev, 1);
	if (err) {
		dev_err(ipmmu_dev, "cannot add ipmmu_pmb device\n");
		goto destroy_device;
	}

	return priv;

destroy_device:
	device_destroy(priv->pmb_class, priv->pmb_dev);
unregister_region:
	unregister_chrdev_region(priv->pmb_dev, 1);
destroy_class:
	class_destroy(priv->pmb_class);
free_priv:
	kfree(priv);
	return ERR_PTR(err);
}
