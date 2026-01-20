// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2022-2023 Arm Technology (China) Co., Ltd.
 * ALL RIGHTS RESERVED
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/acpi.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/pm_runtime.h>
#include <linux/aperture.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_module.h>
#include <drm/drm_of.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
#include <drm/drm_fbdev_generic.h>
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 13, 0)
#include <drm/drm_fbdev_ttm.h>
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 14, 0)
#include <drm/drm_client_setup.h>
#else
#include <drm/clients/drm_client_setup.h>
#endif
#include "linlondp_dev.h"
#include "linlondp_kms.h"
#include "linlondp_pm.h"

#if IS_ENABLED(CONFIG_ACPI)
#include <acpi/acpi_bus.h>

/*
 * ACPI cluster (CIXH50C0): each DPU (CIXH5010) must not register its own DRM
 * card — same as DT "cix,linlon-dpu-slave". Firmware may set _DSD
 * cix,linlon-dpu-slave; if not, treat DPU as slave when its ACPI parent is the
 * cluster device (AML nests DPU under cluster).
 */
static bool linlondp_acpi_is_cluster_dpu_slave(struct device *dev)
{
	struct acpi_device *adev, *parent;

	if (!has_acpi_companion(dev))
		return false;
	adev = ACPI_COMPANION(dev);
	parent = acpi_dev_parent(adev);
	return parent && acpi_dev_hid_uid_match(parent, "CIXH50C0", NULL);
}
#else
static bool linlondp_acpi_is_cluster_dpu_slave(struct device *dev)
{
	return false;
}
#endif

bool enable_fb = true;
module_param_named(enable_fb, enable_fb, bool, 0644);
MODULE_PARM_DESC(enable_fb, "Enable/Disable drm framebuffer support");

struct linlondp_drv {
	struct linlondp_dev *mdev;
	struct linlondp_kms_dev *kms;
	struct linlondp_pm_state pm;
};

/*
 * Cluster slave probe stores linlondp_dev * in drvdata; component master stores
 * struct linlondp_drv *.  DT "cix,linlon-dpu-slave" is not always present; a
 * real linlondp_dev always has mdev->dev == dev.  Mis-reading slave drvdata as
 * mdrv made mdrv->mdev NULL in rt_pm_resume (e.g. modetest).
 */
static bool linlondp_drvdata_is_direct_mdev(struct device *dev)
{
	void *drv = dev_get_drvdata(dev);

	return drv && ((struct linlondp_dev *)drv)->dev == dev;
}

struct linlondp_dev *dev_to_mdev(struct device *dev)
{
	void *drv = dev_get_drvdata(dev);
	struct linlondp_drv *mdrv;

	if (!drv)
		return NULL;

	if (linlondp_drvdata_is_direct_mdev(dev))
		return drv;

	mdrv = drv;
	return mdrv->mdev;
}

static void linlondp_unbind(struct device *dev)
{
	struct linlondp_drv *mdrv = dev_get_drvdata(dev);

	if (!mdrv)
		return;

	linlondp_kms_detach(mdrv->kms);

	if (pm_runtime_enabled(dev))
		pm_runtime_disable(dev);
	else
		linlondp_dev_suspend(mdrv->mdev);

	linlondp_dev_destroy(mdrv->mdev);

	dev_set_drvdata(dev, NULL);
	devm_kfree(dev, mdrv);
}

static int linlondp_bind(struct device *dev)
{
	struct linlondp_drv *mdrv;
	int err;

	/* Remove existing drivers that may own the framebuffer memory. */
	err = aperture_remove_all_conflicting_devices("linlondp");
	if (err) {
		DRM_DEV_ERROR(dev, "Failed to remove existing framebuffers - %d.\n",err);
		return err;
	}

	mdrv = devm_kzalloc(dev, sizeof(*mdrv), GFP_KERNEL);
	if (!mdrv)
		return -ENOMEM;

	pm_runtime_enable(dev);
	mdrv->mdev = linlondp_dev_create(dev);
	if (IS_ERR(mdrv->mdev)) {
		err = PTR_ERR(mdrv->mdev);
		goto free_mdrv;
	}

	if (!pm_runtime_enabled(dev))
		linlondp_dev_resume(mdrv->mdev);

	mdrv->kms = linlondp_kms_attach(mdrv->mdev);
	if (IS_ERR(mdrv->kms)) {
		err = PTR_ERR(mdrv->kms);
		goto destroy_mdev;
	}

	dev_set_drvdata(dev, mdrv);
	if (enable_fb)
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
		drm_fbdev_generic_setup(&mdrv->kms->base, 32);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 13, 0)
		drm_fbdev_ttm_setup(&mdrv->kms->base, 32);
#else
		drm_client_setup(&mdrv->kms->base, NULL);
#endif

	if (mdrv->mdev->enabled_by_gop)
		pm_runtime_set_active(dev);

	return 0;

destroy_mdev:
	if (pm_runtime_enabled(dev))
		pm_runtime_disable(dev);
	else
		linlondp_dev_suspend(mdrv->mdev);

	linlondp_dev_destroy(mdrv->mdev);

free_mdrv:
	devm_kfree(dev, mdrv);
	return err;
}

static const struct component_master_ops linlondp_master_ops = {
	.bind = linlondp_bind,
	.unbind = linlondp_unbind,
};

static int compare_of(struct device *dev, void *data)
{
	int ret;

	if (has_acpi_companion(dev)) {
		ret = dev->fwnode == data;
	} else {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
		ret = component_compare_of(dev, data);
#else
		ret = dev->of_node == data;
#endif
	}

	return ret;
}

static void drm_release_fwnode(struct device *dev, void *data)
{
	fwnode_handle_put(data);
}

static void linlondp_add_acpi_slave(struct device *master,
				    struct component_match **match,
				    struct fwnode_handle *np, u32 port,
				    u32 endpoint)
{
	struct fwnode_handle *remote;

	remote = fwnode_graph_get_remote_node(np, port, endpoint);

	if (remote) {
		pr_info("%s. remote.name=%s\n", __func__, dev_name(remote->dev));
		fwnode_handle_get(remote);
		component_match_add_release(master, match, drm_release_fwnode,
					    compare_of, remote);
		fwnode_handle_put(remote);
	}
}

static void linlondp_add_slave(struct device *master,
			       struct component_match **match,
			       struct device_node *np, u32 port, u32 endpoint)
{
	struct device_node *remote;

	remote = of_graph_get_remote_node(np, port, endpoint);
	if (remote) {
		drm_of_component_match_add(master, match, compare_of, remote);
		of_node_put(remote);
	}
}

int linlondp_dpu_add_graph_slaves(struct device *master,
				  struct component_match **match,
				  struct device *dpu_dev)
{
	struct device_node *np = dpu_dev->of_node;
	struct device_node *of_child;

	if (has_acpi_companion(dpu_dev) && dev_fwnode(dpu_dev)) {
		struct fwnode_handle *acpi_child;
		const char *tmp_name = NULL;

		fwnode_for_each_child_node(dev_fwnode(dpu_dev), acpi_child) {
			tmp_name = acpi_child->ops->get_name(acpi_child);
			if (strncmp(tmp_name, "pipeline", 8))
				continue;

			linlondp_add_acpi_slave(master, match, acpi_child,
						LINLONDP_OF_PORT_OUTPUT, 0);
			linlondp_add_acpi_slave(master, match, acpi_child,
						LINLONDP_OF_PORT_OUTPUT, 1);
		}
		return 0;
	}

	if (!np)
		return 0;

	for_each_available_child_of_node(np, of_child) {
		if (of_node_cmp(of_child->name, "pipeline") != 0)
			continue;

		linlondp_add_slave(master, match, of_child,
				   LINLONDP_OF_PORT_OUTPUT, 0);
		linlondp_add_slave(master, match, of_child,
				   LINLONDP_OF_PORT_OUTPUT, 1);
	}

	return 0;
}

static int linlondp_dpu_slave_probe(struct platform_device *pdev)
{
	struct linlondp_dev *mdev;

	mdev = linlondp_dev_create(&pdev->dev);
	if (IS_ERR(mdev))
		return PTR_ERR(mdev);

	/*
	 * Enable runtime PM only after identify/init inside create completes.
	 * Calling pm_runtime_enable *before* linlondp_dev_create() reorders GenPD
	 * vs SMMU during probe and triggered intermittent faults; enabling here keeps
	 * the stable probe sequence while allowing runtime_suspend to gate clocks /
	 * GenPD when idle (usage count is 0 after create’s pm_runtime_put).
	 */
	pm_runtime_enable(&pdev->dev);
	pm_runtime_idle(&pdev->dev);

	platform_set_drvdata(pdev, mdev);
	return 0;
}

static int linlondp_dpu_slave_remove(struct platform_device *pdev)
{
	struct linlondp_dev *mdev = platform_get_drvdata(pdev);

	if (pm_runtime_enabled(&pdev->dev))
		pm_runtime_disable(&pdev->dev);
	if (mdev)
		linlondp_dev_destroy(mdev);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static int linlondp_platform_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct component_match *match = NULL;
	struct fwnode_handle *acpi_child;
	struct device_node *of_child;
	const char *tmp_name = NULL;

	if (device_property_read_bool(dev, "cix,linlon-dpu-slave") ||
	    linlondp_acpi_is_cluster_dpu_slave(dev))
		return linlondp_dpu_slave_probe(pdev);

	pr_info("%s enter. dev.name=%s\n", __func__, dev_name(dev));
	pr_info("linlondp enable fb is %d", enable_fb);
	if (has_acpi_companion(dev)) {
		pr_info("%s via acpi.\n", __func__);
		fwnode_for_each_child_node(dev->fwnode, acpi_child) {
			tmp_name = acpi_child->ops->get_name(acpi_child);
			if (strncmp(tmp_name, "pipeline", 8))
				continue;

			/* add connector */
			pr_info("%s enter to add connector.\n", __func__);
			linlondp_add_acpi_slave(dev, &match, acpi_child,
						LINLONDP_OF_PORT_OUTPUT, 0);
			linlondp_add_acpi_slave(dev, &match, acpi_child,
						LINLONDP_OF_PORT_OUTPUT, 1);

			dev_pm_set_driver_flags(&pdev->dev, DPM_FLAG_NO_DIRECT_COMPLETE);
		}
	} else {
		pr_info("%s via dt.\n", __func__);
		for_each_available_child_of_node(dev->of_node, of_child) {
			if (of_node_cmp(of_child->name, "pipeline") != 0)
				continue;

			/* add connector */
			linlondp_add_slave(dev, &match, of_child,
					   LINLONDP_OF_PORT_OUTPUT, 0);
			linlondp_add_slave(dev, &match, of_child,
					   LINLONDP_OF_PORT_OUTPUT, 1);
		}
	}
	pr_info("%s end. match=%p\n", __func__, match);
	return component_master_add_with_match(dev, &linlondp_master_ops,
					       match);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0))
static int linlondp_platform_remove(struct platform_device *pdev)
#else
static void linlondp_platform_remove(struct platform_device *pdev)
#endif
{
	if (device_property_read_bool(&pdev->dev, "cix,linlon-dpu-slave") ||
	    linlondp_acpi_is_cluster_dpu_slave(&pdev->dev))
		return linlondp_dpu_slave_remove(pdev);
	component_master_del(&pdev->dev, &linlondp_master_ops);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0))
	return 0;
#endif
}

static const struct of_device_id linlondp_of_match[] = {
	{
		.compatible = "armchina,linlon-d8",
		.data = dp_identify,
	},
	{
		.compatible = "armchina,linlon-d6",
		.data = dp_identify,
	},
	{
		.compatible = "armchina,linlon-d2",
		.data = dp_identify,
	},
	{},
};

MODULE_DEVICE_TABLE(of, linlondp_of_match);

static const struct acpi_device_id linlondp_acpi_match[] = {
	{
		.id = "CIXH5010",
		.driver_data = (kernel_ulong_t)dp_identify,
	},
	{},
};

MODULE_DEVICE_TABLE(acpi, linlondp_acpi_match);

static int __maybe_unused linlondp_rt_pm_suspend(struct device *dev)
{
	struct linlondp_dev *mdev = dev_to_mdev(dev);

	return mdev ? linlondp_dev_suspend(mdev) : 0;
}

static int __maybe_unused linlondp_rt_pm_resume(struct device *dev)
{
	struct linlondp_dev *mdev = dev_to_mdev(dev);

	return mdev ? linlondp_dev_resume(mdev) : 0;
}

static int __maybe_unused linlondp_pm_suspend(struct device *dev)
{
	struct linlondp_drv *mdrv = dev_get_drvdata(dev);
	int res;

	dev_info(dev, "%s\n", __func__);

	if (linlondp_drvdata_is_direct_mdev(dev)) {
		struct linlondp_dev *mdev = dev_get_drvdata(dev);

		if (!pm_runtime_status_suspended(dev))
			linlondp_dev_suspend(mdev);
		mdev->enabled_by_gop = 0;
		return 0;
	}

	if (!mdrv) {
		dev_info(dev, "%s, mdrv is null\n", __func__);
		return 0;
	}

	res = linlondp_pm_suspend_capture(dev, &mdrv->kms->base, &mdrv->pm);

	if (!pm_runtime_status_suspended(dev))
		linlondp_dev_suspend(mdrv->mdev);

	mdrv->mdev->enabled_by_gop = 0;

	return res;
}

static int __maybe_unused linlondp_pm_resume(struct device *dev)
{
	struct linlondp_drv *mdrv = dev_get_drvdata(dev);

	dev_info(dev, "%s\n", __func__);

	if (linlondp_drvdata_is_direct_mdev(dev)) {
		struct linlondp_dev *mdev = dev_get_drvdata(dev);

		if (!pm_runtime_status_suspended(dev))
			linlondp_dev_resume(mdev);
		return 0;
	}

	if (!mdrv) {
		dev_info(dev, "%s, mdrv is null\n", __func__);
		return 0;
	}

	if (!pm_runtime_status_suspended(dev))
		linlondp_dev_resume(mdrv->mdev);

	return linlondp_pm_resume_check(dev, &mdrv->kms->base, &mdrv->pm);
}

static int __maybe_unused linlondp_pm_restore(struct device *dev)
{
	struct linlondp_drv *mdrv = dev_get_drvdata(dev);
	u32 gop;

	if (linlondp_drvdata_is_direct_mdev(dev)) {
		struct linlondp_dev *mdev = dev_get_drvdata(dev);

		if (device_property_read_u32(dev, "enabled_by_gop", &gop) == 0)
			mdev->enabled_by_gop = gop;
		if (!pm_runtime_status_suspended(dev))
			linlondp_dev_resume(mdev);
		return 0;
	}

	if (!mdrv)
		return 0;

	if (device_property_read_u32(dev, "enabled_by_gop", &gop) == 0)
		mdrv->mdev->enabled_by_gop = gop;

	if (!pm_runtime_status_suspended(dev))
		linlondp_dev_resume(mdrv->mdev);

	return drm_mode_config_helper_resume(&mdrv->kms->base);
}

static void linlondp_platform_shutdown(struct platform_device *pdev)
{
	struct linlondp_drv *mdrv;

	if (linlondp_drvdata_is_direct_mdev(&pdev->dev)) {
		struct linlondp_dev *mdev = platform_get_drvdata(pdev);

		linlondp_pm_suspend(&pdev->dev);
		mdev->shutdown = true;
		return;
	}

	mdrv = dev_get_drvdata(&pdev->dev);

	linlondp_pm_suspend(&pdev->dev);

	if (mdrv)
		mdrv->mdev->shutdown = true;
}

static const struct dev_pm_ops linlondp_pm_ops = {
	.suspend = linlondp_pm_suspend,
	.resume = linlondp_pm_resume,
	.restore = linlondp_pm_restore,
	.freeze = linlondp_pm_suspend,
	.thaw = linlondp_pm_resume,
	SET_RUNTIME_PM_OPS(linlondp_rt_pm_suspend, linlondp_rt_pm_resume, NULL)
};

static struct platform_driver linlondp_platform_driver = {
	.probe = linlondp_platform_probe,
	.remove = linlondp_platform_remove,
	.shutdown = linlondp_platform_shutdown,
	.driver = {
		.name = "linlondp",
		.of_match_table = linlondp_of_match,
		.acpi_match_table = ACPI_PTR(linlondp_acpi_match),
		.pm = &linlondp_pm_ops,
	},
};

extern struct platform_driver linlondp_cluster_driver;

static int __init linlondp_mod_init(void)
{
	int err;

	err = drm_platform_driver_register(&linlondp_platform_driver);
	if (err)
		return err;
	err = drm_platform_driver_register(&linlondp_cluster_driver);
	if (err)
		goto err_cluster;

	return 0;

err_cluster:
	platform_driver_unregister(&linlondp_platform_driver);
	return err;
}

static void __exit linlondp_mod_exit(void)
{
	platform_driver_unregister(&linlondp_cluster_driver);
	platform_driver_unregister(&linlondp_platform_driver);
}

module_init(linlondp_mod_init);
module_exit(linlondp_mod_exit);

MODULE_DESCRIPTION("Linlondp KMS driver");
MODULE_AUTHOR("ARMChina");
MODULE_LICENSE("GPL v2");
