// SPDX-License-Identifier: GPL-2.0
/*
 * CIX linlon display cluster — one DRM card for multiple DPU platform devices.
 *
 * Device tree:
 *   dpu_display_cluster {
 *     compatible = "cix,linlon-display-cluster";
 *     dpus = <&dpu0 &dpu1 ...>;
 *   };
 *   &dpu0 { cix,linlon-dpu-slave; };
 *   (repeat for each DPU in the cluster)
 *
 * ACPI (cluster device _HID CIXH50C0; last 4 chars must be hex per ACPI EISA ID):
 *   Either list full ACPI paths of each DPU device in _DSD property
 *   "cix,dpu-acpi-paths" (comma-separated, e.g. "\\_SB.DPU0,\\_SB.DPU1"), or
 *   declare each DPU as a child ACPI device with _HID CIXH5010 under the
 *   cluster scope (enumeration order follows AML child order).
 *
 * SMMU: Each DPU node keeps its own `iommus = <&smmu_mmhub stream-id>, ...`.
 * GEM is still allocated on the first DPU's `struct device` (drm dev parent).
 * `linlondp_kms.c` mirrors each buffer's IOVA→PA mapping into every other
 * cluster DPU's IOMMU domain so scanout on dpu1+ does not fault (AXIE).
 * Per-DPU TBU `connect_iommu` still runs on resume.
 *
 * `device-id` in DT is the driver's logical id (DSM/debugfs), not the SMMU SID.
 */
#include <linux/component.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/aperture.h>
#include <linux/pm_runtime.h>
#include <linux/version.h>
#if IS_ENABLED(CONFIG_ACPI)
#include <linux/pm.h>
#endif
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/acpi.h>

#if IS_ENABLED(CONFIG_ACPI)
#include <acpi/acpi_bus.h>
#endif

#include <drm/drm_atomic.h>
#include <drm/drm_connector.h>
#include <drm/drm_fb_helper.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
#include <drm/drm_fbdev_generic.h>
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 13, 0)
#include <drm/drm_fbdev_ttm.h>
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 14, 0)
#include <drm/drm_client_setup.h>
#else
#include <drm/clients/drm_client_setup.h>
#endif
#include <drm/drm_module.h>
#include <drm/drm_modeset_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_edid.h>
#include <linux/i2c.h>
#include "linlondp_dev.h"
#include "linlondp_kms.h"
#include "linlondp_pm.h"

extern bool enable_fb;

struct linlondp_cluster {
	struct linlondp_kms_dev *kms;
	struct linlondp_dev *mdevs[LINLONDP_MAX_CLUSTER_DPUS];
	struct device_link *dpu_links[LINLONDP_MAX_CLUSTER_DPUS];
	unsigned int n_mdevs;
	struct linlondp_pm_state pm;
};

static void linlondp_cluster_dpu_links_del(struct linlondp_cluster *cl)
{
	unsigned int i;

	for (i = 0; i < cl->n_mdevs; i++) {
		if (cl->dpu_links[i]) {
			device_link_del(cl->dpu_links[i]);
			cl->dpu_links[i] = NULL;
		}
	}
}

/*
 * consumer=cluster, supplier=DPU slave: resume DPU (linlondp_dev_resume) before
 * cluster PM; pairs with trilin_dptx device_link(cluster<-dptx) for DP->DPU->cluster.
 * Use DL_FLAG_STATELESS so the link can be manually deleted on unbind
 * (device_link_del() only works on stateless links).
 */
static int linlondp_cluster_dpu_links_add(struct device *dev,
					struct linlondp_cluster *cl)
{
	unsigned int i;

	for (i = 0; i < cl->n_mdevs; i++) {
		cl->dpu_links[i] = device_link_add(dev, cl->mdevs[i]->dev,
						   DL_FLAG_STATELESS);
		if (!cl->dpu_links[i]) {
			linlondp_cluster_dpu_links_del(cl);
			return -ENOMEM;
		}
	}
	return 0;
}

/*
 * Cluster expects each DPU device to be probed as "slave" and store
 * struct linlondp_dev directly in drvdata. If ACPI slave detection mismatches
 * (e.g. unexpected cluster HID), drvdata may be struct linlondp_drv instead.
 */
static bool linlondp_cluster_is_direct_mdev(struct device *dev,
					    struct linlondp_dev *mdev)
{
	return mdev && !IS_ERR(mdev) && mdev->dev == dev;
}

static int linlondp_cluster_bind(struct device *dev)
{
	struct linlondp_cluster *cl = dev_get_drvdata(dev);
	struct device *d0 = cl->mdevs[0]->dev;
	unsigned int i;
	int err;

	/* Remove existing drivers that may own the framebuffer memory. */
	err = aperture_remove_all_conflicting_devices("linlondp");
	if (err) {
		DRM_DEV_ERROR(dev, "Failed to remove existing framebuffers - %d.\n",err);
		return err;
	}

	/*
	 * Cluster DPUs are slaves: linlondp_dpu_slave_probe() already calls
	 * pm_runtime_enable() before linlondp_dev_create() (same order as master /
	 * DPTSW-13544). Only enable here if not yet enabled (older probe path).
	 * atomic_enable still requires pm_runtime_enabled() on every mdev.
	 */
	for (i = 0; i < cl->n_mdevs; i++) {
		if (!pm_runtime_enabled(cl->mdevs[i]->dev)) {
			pm_runtime_enable(cl->mdevs[i]->dev);
			dev_notice(dev,
				   "linlon-cluster: pm_runtime_enable DPU%u %s\n", i,
				   dev_name(cl->mdevs[i]->dev));
		}
	}

	err = linlondp_cluster_dpu_links_add(dev, cl);
	if (err)
		return err;

	cl->kms = linlondp_kms_attach_cluster(dev, cl->mdevs, cl->n_mdevs);
	if (IS_ERR(cl->kms)) {
		err = PTR_ERR(cl->kms);
		cl->kms = NULL;
		linlondp_cluster_dpu_links_del(cl);
		for (i = 0; i < cl->n_mdevs; i++) {
			if (pm_runtime_enabled(cl->mdevs[i]->dev))
				pm_runtime_disable(cl->mdevs[i]->dev);
		}
		return err;
	}

	if (enable_fb)
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
		drm_fbdev_generic_setup(&cl->kms->base, 32);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 13, 0)
		drm_fbdev_ttm_setup(&cl->kms->base, 32);
#else
		drm_client_setup(&cl->kms->base, NULL);
#endif

	if (cl->mdevs[0]->enabled_by_gop)
		pm_runtime_set_active(d0);

	return 0;
}

static void linlondp_cluster_unbind(struct device *dev)
{
	struct linlondp_cluster *cl = dev_get_drvdata(dev);
	unsigned int i;

	if (!cl)
		return;

	linlondp_cluster_dpu_links_del(cl);

	if (cl->kms) {
		linlondp_kms_detach(cl->kms);
		cl->kms = NULL;
	}

	for (i = 0; i < cl->n_mdevs; i++) {
		if (pm_runtime_enabled(cl->mdevs[i]->dev))
			pm_runtime_disable(cl->mdevs[i]->dev);
	}
}

static const struct component_master_ops linlondp_cluster_master_ops = {
	.bind = linlondp_cluster_bind,
	.unbind = linlondp_cluster_unbind,
};

static const struct acpi_device_id linlondp_cluster_acpi_match[] = {
	{ "CIXH50C0", 0 },
	{},
};
MODULE_DEVICE_TABLE(acpi, linlondp_cluster_acpi_match);

#if IS_ENABLED(CONFIG_ACPI)
struct linlondp_cluster_acpi_ctx {
	struct device *cluster_dev;
	struct linlondp_cluster *cl;
	struct component_match **match;
	unsigned int idx;
};

static int linlondp_cluster_acpi_child_cb(struct acpi_device *child, void *data)
{
	struct linlondp_cluster_acpi_ctx *ctx = data;
	struct linlondp_dev *mdev;
	struct device *phys;
	int err;

	if (!acpi_dev_hid_uid_match(child, "CIXH5010", NULL))
		return 0;

	if (ctx->idx >= LINLONDP_MAX_CLUSTER_DPUS)
		return -EINVAL;

	phys = acpi_get_first_physical_node(child);
	if (!phys) {
		dev_warn(ctx->cluster_dev,
			 "ACPI cluster: skip child %s, no physical node (trimmed?)\n",
			 dev_name(&child->dev));
		return 0;
	}

	if (!dev_is_platform(phys)) {
		dev_warn(ctx->cluster_dev,
			 "ACPI cluster: skip child %s, non-platform physical node %s\n",
			 dev_name(&child->dev), dev_name(phys));
		put_device(phys);
		return 0;
	}

	mdev = platform_get_drvdata(to_platform_device(phys));
	if (!linlondp_cluster_is_direct_mdev(phys, mdev)) {
		dev_warn(ctx->cluster_dev,
			 "ACPI cluster: skip child %s, invalid DPU drvdata on %s (not slave mdev)\n",
			 dev_name(&child->dev), dev_name(phys));
		put_device(phys);
		return 0;
	}
	put_device(phys);

	ctx->cl->mdevs[ctx->idx] = mdev;
	err = linlondp_dpu_add_graph_slaves(ctx->cluster_dev, ctx->match, mdev->dev);
	if (err) {
		dev_warn(ctx->cluster_dev,
			 "ACPI cluster: skip child %s, add_graph_slaves failed: %d\n",
			 dev_name(&child->dev), err);
		return 0;
	}
	ctx->idx++;
	return 0;
}

/* Returns DPU count, -ENOENT if property absent, else error. */
static int linlondp_cluster_collect_dpus_acpi_paths(struct device *dev,
						    struct linlondp_cluster *cl,
						    struct component_match **match)
{
	const char *buf;
	char *kbuf, *p, *token;
	int n = 0, err;
	acpi_handle h;
	struct acpi_device *adev;
	struct device *phys;

	if (!device_property_present(dev, "cix,dpu-acpi-paths"))
		return -ENOENT;

	if (device_property_read_string(dev, "cix,dpu-acpi-paths", &buf))
		return -EINVAL;

	kbuf = kstrdup(buf, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	p = kbuf;
	while ((token = strsep(&p, ","))) {
		const char *path = token;

		while (*token == ' ' || *token == '\t')
			token++;
		if (!*token)
			continue;

		if (n >= LINLONDP_MAX_CLUSTER_DPUS) {
			kfree(kbuf);
			return -EINVAL;
		}

		if (ACPI_FAILURE(acpi_get_handle(ACPI_ROOT_OBJECT, token, &h))) {
			dev_warn(dev,
				 "linlon-cluster: skip bad ACPI path \"%s\"\n", path);
			continue;
		}

		adev = acpi_fetch_acpi_dev(h);
		if (!adev) {
			dev_warn(dev,
				 "linlon-cluster: skip ACPI path \"%s\", no acpi_device\n",
				 path);
			continue;
		}

		phys = acpi_get_first_physical_node(adev);
		if (!phys) {
			dev_warn(dev,
				 "linlon-cluster: skip ACPI path \"%s\", no physical node (trimmed?)\n",
				 path);
			continue;
		}

		if (!dev_is_platform(phys)) {
			dev_warn(dev,
				 "linlon-cluster: skip ACPI path \"%s\", non-platform node %s\n",
				 path, dev_name(phys));
			put_device(phys);
			continue;
		}

		cl->mdevs[n] = platform_get_drvdata(to_platform_device(phys));
		if (!linlondp_cluster_is_direct_mdev(phys, cl->mdevs[n])) {
			dev_warn(dev,
				 "linlon-cluster: skip ACPI path \"%s\", invalid DPU drvdata on %s (not slave mdev)\n",
				 path, dev_name(phys));
			put_device(phys);
			continue;
		}
		put_device(phys);

		err = linlondp_dpu_add_graph_slaves(dev, match, cl->mdevs[n]->dev);
		if (err) {
			dev_warn(dev,
				 "linlon-cluster: skip ACPI path \"%s\", add_graph_slaves failed: %d\n",
				 path, err);
			continue;
		}
		n++;
	}

	kfree(kbuf);
	if (!n)
		return -EINVAL;
	return n;
}

static int linlondp_cluster_collect_dpus_acpi_children(struct acpi_device *parent,
						       struct device *cluster_dev,
						       struct linlondp_cluster *cl,
						       struct component_match **match)
{
	struct linlondp_cluster_acpi_ctx ctx = {
		.cluster_dev = cluster_dev,
		.cl = cl,
		.match = match,
		.idx = 0,
	};
	int ret;

	ret = acpi_dev_for_each_child(parent, linlondp_cluster_acpi_child_cb, &ctx);
	if (ret)
		return ret;
	if (!ctx.idx)
		return -ENOENT;
	return ctx.idx;
}

static int linlondp_cluster_probe_acpi(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct acpi_device *cluster_adev = ACPI_COMPANION(dev);
	struct linlondp_cluster *cl;
	struct component_match *match = NULL;
	const char *hid;
	int n, err;

	if (!cluster_adev)
		return -ENODEV;

	cl = devm_kzalloc(dev, sizeof(*cl), GFP_KERNEL);
	if (!cl)
		return -ENOMEM;

	hid = acpi_device_hid(cluster_adev);
	dev_info(dev,
		 "linlon-cluster: ACPI cluster HID %s (%s) — match CIXH50C0 / cix,dpu-acpi-paths or child CIXH5010\n",
		 hid ? hid : "?", dev_name(dev));

	n = linlondp_cluster_collect_dpus_acpi_paths(dev, cl, &match);
	if (n == -ENOENT)
		n = linlondp_cluster_collect_dpus_acpi_children(cluster_adev, dev,
								cl, &match);
	if (n < 0) {
		return dev_err_probe(dev, n == -ENOENT ? -EINVAL : n,
				     "ACPI cluster: no DPUs (paths or CIXH5010 children)\n");
	}

	cl->n_mdevs = n;
	dev_set_drvdata(dev, cl);

	dev_info(dev,
		 "linlon-cluster: registered %u DPU(s) for ACPI cluster %s, component master\n",
		 cl->n_mdevs, hid ? hid : "?");

	/*
	 * Match linlondp_platform_probe() ACPI branch only: OF master does not set
	 * this. Avoid direct_complete when ACPI + pipeline/component graph is used.
	 */
	dev_pm_set_driver_flags(&pdev->dev, DPM_FLAG_NO_DIRECT_COMPLETE);

	err = component_master_add_with_match(dev, &linlondp_cluster_master_ops,
					      match);
	if (err)
		dev_set_drvdata(dev, NULL);
	return err;
}
#endif /* CONFIG_ACPI */

static int linlondp_cluster_probe_of(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct linlondp_cluster *cl;
	struct component_match *match = NULL;
	struct device_node *np = dev->of_node;
	int n, i, used = 0, err;

	if (!np)
		return -ENODEV;

	n = of_count_phandle_with_args(np, "dpus", NULL);
	if (n <= 0)
		return dev_err_probe(dev, -EINVAL, "missing or empty dpus\n");

	if ((unsigned int)n > LINLONDP_MAX_CLUSTER_DPUS)
		return dev_err_probe(dev, -EINVAL, "too many dpus (%d)\n", n);

	cl = devm_kzalloc(dev, sizeof(*cl), GFP_KERNEL);
	if (!cl)
		return -ENOMEM;

	for (i = 0; i < n; i++) {
		struct device_node *dpu_np = of_parse_phandle(np, "dpus", i);
		struct platform_device *dpu_pdev;
		struct linlondp_dev *mdev;

		if (!dpu_np)
			return -EINVAL;

		dpu_pdev = of_find_device_by_node(dpu_np);
		of_node_put(dpu_np);
		if (!dpu_pdev) {
			dev_warn(dev,
				 "dpu %d platform device missing, skipping directly\n",
				 i);
			continue;
		}

		mdev = platform_get_drvdata(dpu_pdev);
		put_device(&dpu_pdev->dev);
		if (!mdev)
			return dev_err_probe(dev, -EPROBE_DEFER, "dpu %d has no mdev\n", i);
		if (IS_ERR(mdev))
			return dev_err_probe(dev, PTR_ERR(mdev), "dpu %d mdev error\n", i);
		dev_info(dev, "dpu %d ready: %s\n", i, dev_name(mdev->dev));

		cl->mdevs[used] = mdev;
		err = linlondp_dpu_add_graph_slaves(dev, &match, mdev->dev);
		if (err)
			return err;
		used++;
	}

	if (!used)
		return dev_err_probe(dev, -ENODEV,
				     "no available dpu in cluster\n");

	if (used != n)
		dev_warn(dev, "cluster degraded: %d/%d dpu available\n", used, n);

	cl->n_mdevs = used;
	dev_set_drvdata(dev, cl);

	return component_master_add_with_match(dev, &linlondp_cluster_master_ops,
					       match);
}

static int linlondp_cluster_probe(struct platform_device *pdev)
{
#if IS_ENABLED(CONFIG_ACPI)
	const struct acpi_device_id *aid;

	aid = acpi_match_device(linlondp_cluster_acpi_match, &pdev->dev);
	if (aid)
		return linlondp_cluster_probe_acpi(pdev);
#endif
	if (pdev->dev.of_node)
		return linlondp_cluster_probe_of(pdev);
	return -ENODEV;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0))
static int linlondp_cluster_remove(struct platform_device *pdev)
#else
static void linlondp_cluster_remove(struct platform_device *pdev)
#endif
{
	component_master_del(&pdev->dev, &linlondp_cluster_master_ops);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0))
	return 0;
#endif
}

/*
 * Cluster owns KMS PM. EDID swap: suspend blob product+serial, resume 16B DDC hdr16.
 *
 * Capture failure is non-fatal for system suspend: log a warning and keep going.
 * The resume path checks pm_capture_failed and forces the discard+hotplug slow
 * path so userspace re-modesets cleanly (DPTSW-22258 review risk #4).
 */
static int __maybe_unused linlondp_cluster_pm_prepare(struct device *dev)
{
	struct linlondp_cluster *cl = dev_get_drvdata(dev);
	struct drm_device *drm;

	dev_info(dev, "%s\n", __func__);

	if (!cl || !cl->kms)
		return 0;

	drm = &cl->kms->base;
	return linlondp_pm_suspend_capture(dev, drm, &cl->pm);
}

static int __maybe_unused linlondp_cluster_pm_resume(struct device *dev)
{
	struct linlondp_cluster *cl = dev_get_drvdata(dev);
	struct drm_device *drm;

	dev_info(dev, "%s\n", __func__);

	if (!cl || !cl->kms)
		return 0;

	drm = &cl->kms->base;
	return linlondp_pm_resume_check(dev, drm, &cl->pm);
}

static const struct dev_pm_ops linlondp_cluster_pm_ops = {
	.prepare = linlondp_cluster_pm_prepare,
	.resume = linlondp_cluster_pm_resume,
	.freeze = linlondp_cluster_pm_prepare,
	.thaw = linlondp_cluster_pm_resume,
	.poweroff = linlondp_cluster_pm_prepare,
	.restore = linlondp_cluster_pm_resume,
};

static const struct of_device_id linlondp_cluster_of_match[] = {
	{ .compatible = "cix,linlon-display-cluster" },
	{},
};
MODULE_DEVICE_TABLE(of, linlondp_cluster_of_match);

struct platform_driver linlondp_cluster_driver = {
	.probe = linlondp_cluster_probe,
	.remove = linlondp_cluster_remove,
	.driver = {
		.name = "linlondp-cluster",
		.of_match_table = linlondp_cluster_of_match,
		.acpi_match_table = ACPI_PTR(linlondp_cluster_acpi_match),
		.pm = &linlondp_cluster_pm_ops,
	},
};
