// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2022-2023 Arm Technology (China) Co., Ltd.
 * ALL RIGHTS RESERVED
 *
 */
#include <linux/component.h>
#include <linux/interrupt.h>
#include <linux/iommu.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_managed.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>
#include <drm/drm_self_refresh_helper.h>
#include <drm/drm_gem.h>

#include "linlondp_dev.h"
#include "linlondp_framebuffer.h"
#include "linlondp_kms.h"

DEFINE_DRM_GEM_DMA_FOPS(linlondp_cma_fops);

static bool enable_render = false;
module_param(enable_render, bool, 0644);
MODULE_PARM_DESC(enable_render, "Enable render node support (true/false)");

#if IS_ENABLED(CONFIG_IOMMU_API)
/*
 * Cluster KMS allocates GEM/DMA on dpu0's struct device. Each DPU has distinct
 * SMMU stream IDs, so scanout on non-primary DPUs still needs domain mappings.
 *
 * To reduce import-time overhead, mappings are established on-demand for the
 * DPU domains actually used by current atomic state, instead of pre-mirroring
 * all cluster DPUs on every import.
 */
static void linlondp_cluster_iommu_unmap_mirror(struct linlondp_kms_dev *kms,
						struct drm_gem_dma_object *dma_obj)
{
	struct iommu_domain *d0;
	dma_addr_t iova = dma_obj->dma_addr;
	size_t size = dma_obj->base.size;
	unsigned long off;
	unsigned int i;

	d0 = iommu_get_domain_for_dev(kms->hw_mdevs[0]->dev);

	for (i = 1; i < kms->n_hw_mdevs; i++) {
		struct iommu_domain *di =
			iommu_get_domain_for_dev(kms->hw_mdevs[i]->dev);

		if (!di || di == d0)
			continue;
		for (off = 0; off < size; off += PAGE_SIZE) {
			if (iommu_iova_to_phys(di, iova + off)) {
				iommu_unmap(di, iova + off, PAGE_SIZE);
			}
		}
	}
}

static int linlondp_cluster_get_hw_idx(struct linlondp_kms_dev *kms,
				       struct linlondp_dev *mdev)
{
	unsigned int i;

	for (i = 0; i < kms->n_hw_mdevs; i++) {
		if (kms->hw_mdevs[i] == mdev)
			return i;
	}

	return -ENODEV;
}

static int linlondp_cluster_iommu_map_domain(struct linlondp_kms_dev *kms,
					     struct drm_gem_dma_object *dma_obj,
					     unsigned int target_idx)
{
	struct iommu_domain *d0;
	struct iommu_domain *dt;
	dma_addr_t iova = dma_obj->dma_addr;
	size_t size = dma_obj->base.size;
	unsigned long off;
	int ret;
	const int prot = IOMMU_READ | IOMMU_WRITE;

	if (!target_idx || target_idx >= kms->n_hw_mdevs)
		return 0;

	d0 = iommu_get_domain_for_dev(kms->hw_mdevs[0]->dev);
	dt = iommu_get_domain_for_dev(kms->hw_mdevs[target_idx]->dev);
	if (!dt || dt == d0)
		return 0;

	if (!d0) {
		if (!dma_obj->vaddr) {
			DRM_WARN("linlon cluster: skip IOMMU mirror (no dpu0 domain, no vaddr)\n");
			return 0;
		}
		for (off = 0; off < size; off += PAGE_SIZE) {
			phys_addr_t phys;

			if (iommu_iova_to_phys(dt, iova + off))
				continue;
			phys = virt_to_phys((char *)dma_obj->vaddr + off);
			ret = iommu_map(dt, iova + off, phys, PAGE_SIZE, prot,
					GFP_KERNEL);
			if (ret) {
				linlondp_cluster_iommu_unmap_mirror(kms, dma_obj);
				return ret;
			}
		}
	} else {
		for (off = 0; off < size; off += PAGE_SIZE) {
			phys_addr_t phys;

			if (iommu_iova_to_phys(dt, iova + off))
				continue;

			phys = iommu_iova_to_phys(d0, iova + off);
			if (!phys) {
				DRM_ERROR(
					"linlon cluster: no PA for IOVA %pad+%lx in dpu0 IOMMU domain\n",
					&iova, off);
				linlondp_cluster_iommu_unmap_mirror(kms, dma_obj);
				return -EFAULT;
			}
			ret = iommu_map(dt, iova + off, phys, PAGE_SIZE, prot,
					GFP_KERNEL);
			if (ret) {
				DRM_ERROR(
					"linlon cluster: IOMMU map DPU%u IOVA %pad: %d\n",
					target_idx, &iova, ret);
				linlondp_cluster_iommu_unmap_mirror(kms, dma_obj);
				return ret;
			}
		}
	}
	return 0;
}
#else
static void linlondp_cluster_iommu_unmap_mirror(struct linlondp_kms_dev *kms,
						struct drm_gem_dma_object *dma_obj)
{
	(void)kms;
	(void)dma_obj;
}

static int linlondp_cluster_get_hw_idx(struct linlondp_kms_dev *kms,
				       struct linlondp_dev *mdev)
{
	(void)kms;
	(void)mdev;
	return -ENODEV;
}

static int linlondp_cluster_iommu_map_domain(struct linlondp_kms_dev *kms,
					     struct drm_gem_dma_object *dma_obj,
					     unsigned int target_idx)
{
	(void)kms;
	(void)dma_obj;
	(void)target_idx;
	return 0;
}
#endif

static void linlondp_cluster_gem_free(struct drm_gem_object *obj);

static const struct drm_gem_object_funcs linlondp_cluster_gem_funcs = {
	.free = linlondp_cluster_gem_free,
	.print_info = drm_gem_dma_object_print_info,
	.get_sg_table = drm_gem_dma_object_get_sg_table,
	.vmap = drm_gem_dma_object_vmap,
	.mmap = drm_gem_dma_object_mmap,
	.vm_ops = &drm_gem_dma_vm_ops,
};

static void linlondp_cluster_gem_free(struct drm_gem_object *obj)
{
	struct drm_gem_dma_object *dma_obj = to_drm_gem_dma_obj(obj);
	struct drm_device *drm = obj->dev;
	struct linlondp_kms_dev *kms = to_kdev(drm);

	if (kms->n_hw_mdevs > 1)
		linlondp_cluster_iommu_unmap_mirror(kms, dma_obj);
	drm_gem_dma_object_free(obj);
}

static int linlondp_gem_dma_dumb_create(struct drm_file *file,
					struct drm_device *dev,
					struct drm_mode_create_dumb *args)
{
	struct linlondp_kms_dev *kms = to_kdev(dev);
	u32 pitch = DIV_ROUND_UP(args->width * args->bpp, 8);
	u32 bus = kms->hw_mdevs[0]->chip.bus_width;
	unsigned int i;
	unsigned int min_pitch;
	struct drm_gem_dma_object *dma_obj;
	int ret;

	for (i = 1; i < kms->n_hw_mdevs; i++)
		bus = max(bus, kms->hw_mdevs[i]->chip.bus_width);

	args->pitch = ALIGN(pitch, bus);

	min_pitch = DIV_ROUND_UP(args->width * args->bpp, 8);
	if (args->pitch < min_pitch)
		args->pitch = min_pitch;
	if (args->size < args->pitch * args->height)
		args->size = args->pitch * args->height;

	if (kms->n_hw_mdevs < 2)
		return drm_gem_dma_dumb_create_internal(file, dev, args);

	dma_obj = drm_gem_dma_create(dev, args->size);
	if (IS_ERR(dma_obj))
		return PTR_ERR(dma_obj);

	dma_obj->base.funcs = &linlondp_cluster_gem_funcs;

	ret = drm_gem_handle_create(file, &dma_obj->base, &args->handle);
	drm_gem_object_put(&dma_obj->base);
	return ret;
}

static struct drm_gem_object *
linlondp_gem_prime_import_sg_table(struct drm_device *dev,
				   struct dma_buf_attachment *attach,
				   struct sg_table *sgt)
{
	struct drm_gem_object *obj;
	struct linlondp_kms_dev *kms = to_kdev(dev);

	obj = drm_gem_dma_prime_import_sg_table(dev, attach, sgt);
	if (IS_ERR(obj))
		return obj;

	if (kms->n_hw_mdevs > 1) {
		obj->funcs = &linlondp_cluster_gem_funcs;
	}
	return obj;
}

struct linlondp_irq_bundle {
	struct linlondp_kms_dev *kms;
	struct linlondp_dev *mdev;
};

static irqreturn_t linlondp_kms_irq_handler(int irq, void *data)
{
	struct linlondp_irq_bundle *b = data;
	struct linlondp_dev *mdev = b->mdev;
	struct linlondp_kms_dev *kms = b->kms;
	struct drm_device *drm = &kms->base;
	struct linlondp_events evts;
	irqreturn_t status;
	u32 i;

	memset(&evts, 0, sizeof(evts));
	status = mdev->funcs->irq_handler(mdev, &evts);

	linlondp_print_events(&evts, drm, mdev);

	for (i = 0; i < kms->n_crtcs; i++) {
		if (kms->crtcs[i].master->mdev != mdev)
			continue;
		linlondp_crtc_handle_event(&kms->crtcs[i], &evts);
	}

	return status;
}

static struct drm_driver linlondp_kms_driver = {
	.driver_features = DRIVER_GEM | DRIVER_MODESET | DRIVER_ATOMIC,
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
	.lastclose = drm_fb_helper_lastclose,
#endif
	.dumb_create = linlondp_gem_dma_dumb_create,
	.gem_prime_import_sg_table = linlondp_gem_prime_import_sg_table,
	.fops = &linlondp_cma_fops,
	.name = "linlondp",
	.desc = "Linlon Display Processor driver",
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 14, 0)
	.date = "20230426",
#endif
	.major = 0,
	.minor = 0,
};

static void linlondp_kms_atomic_commit_hw_done(struct drm_atomic_state *state)
{
	struct drm_device *dev = state->dev;
	struct linlondp_kms_dev *kms = to_kdev(dev);
	int i;
	unsigned long flags;

	for (i = 0; i < kms->n_crtcs; i++) {
		struct linlondp_crtc *kcrtc = &kms->crtcs[i];

		if (kcrtc->base.state->active) {
			struct completion *flip_done = NULL;

			spin_lock_irqsave(&dev->event_lock, flags);
			if (kcrtc->base.state->event)
				flip_done = kcrtc->base.state->event->base
						    .completion;
			spin_unlock_irqrestore(&dev->event_lock, flags);
			if (flip_done) {
				linlondp_crtc_flush_and_wait_for_flip_done(kcrtc,
									   flip_done, false);
			}
		}
	}
	drm_atomic_helper_commit_hw_done(state);
}

static void linlondp_kms_commit_tail(struct drm_atomic_state *old_state)
{
	struct drm_device *dev = old_state->dev;
	bool fence_cookie = dma_fence_begin_signalling();

	drm_atomic_helper_commit_modeset_disables(dev, old_state);

	drm_atomic_helper_commit_planes(dev, old_state,
					DRM_PLANE_COMMIT_ACTIVE_ONLY);

	drm_atomic_helper_commit_modeset_enables(dev, old_state);

	linlondp_kms_atomic_commit_hw_done(old_state);

	drm_atomic_helper_wait_for_flip_done(dev, old_state);

	dma_fence_end_signalling(fence_cookie);

	drm_atomic_helper_cleanup_planes(dev, old_state);
}

static const struct drm_mode_config_helper_funcs linlondp_mode_config_helpers = {
	.atomic_commit_tail = linlondp_kms_commit_tail,
};

static int linlondp_plane_state_list_add(struct drm_plane_state *plane_st,
					 struct list_head *zorder_list)
{
	struct linlondp_plane_state *new = to_kplane_st(plane_st);
	struct linlondp_plane_state *node, *last;

	last = list_empty(zorder_list) ?
		       NULL :
		       list_last_entry(zorder_list, typeof(*last), zlist_node);

	/* Considering the list sequence is zpos increasing, so if list is empty
	 * or the zpos of new node bigger than the last node in list, no need
	 * loop and just insert the new one to the tail of the list.
	 */
	if (!last || (new->base.zpos > last->base.zpos)) {
		list_add_tail(&new->zlist_node, zorder_list);
		return 0;
	}

	/* Build the list by zpos increasing */
	list_for_each_entry(node, zorder_list, zlist_node) {
		if (new->base.zpos < node->base.zpos) {
			list_add_tail(&new->zlist_node, &node->zlist_node);
			break;
		} else if (node->base.zpos == new->base.zpos) {
			struct drm_plane *a = node->base.plane;
			struct drm_plane *b = new->base.plane;

			/* Linlondp doesn't support setting a same zpos for
			 * different planes.
			 */
			DRM_DEBUG_ATOMIC(
				"PLANE: %s and PLANE: %s are configured same zpos: %d.\n",
				a->name, b->name, node->base.zpos);
			return -EINVAL;
		}
	}

	return 0;
}

static int linlondp_crtc_normalize_zpos(struct drm_crtc *crtc,
					struct drm_crtc_state *crtc_st)
{
	struct drm_atomic_state *state = crtc_st->state;
	struct linlondp_crtc *kcrtc = to_kcrtc(crtc);
	struct linlondp_crtc_state *kcrtc_st = to_kcrtc_st(crtc_st);
	struct linlondp_plane_state *kplane_st;
	struct drm_plane_state *plane_st;
	struct drm_plane *plane;
	struct list_head zorder_list;
	int order = 0, err;
	bool split = 0;

	DRM_DEBUG_ATOMIC("[CRTC:%d:%s] calculating normalized zpos values\n",
			 crtc->base.id, crtc->name);

	INIT_LIST_HEAD(&zorder_list);

	/* This loop also added all effected planes into the new state */
	drm_for_each_plane_mask(plane, crtc->dev, crtc_st->plane_mask) {
		plane_st = drm_atomic_get_plane_state(state, plane);
		if (IS_ERR(plane_st))
			return PTR_ERR(plane_st);

		/* Build a list by zpos increasing */
		err = linlondp_plane_state_list_add(plane_st, &zorder_list);
		if (err)
			return err;
	}

	kcrtc_st->max_slave_zorder = 0;

	list_for_each_entry(kplane_st, &zorder_list, zlist_node) {
		plane_st = &kplane_st->base;
		plane = plane_st->plane;

		plane_st->normalized_zpos = order++;
		/* When layer_split has been enabled, one plane will be handled
		 * by two separated linlondp layers (left/right), which may needs
		 * two zorders.
		 * - zorder: for left_layer for left display part.
		 * - zorder + 1: will be reserved for right layer.
		 */
		split = to_kplane_st(plane_st)->layer_split;
		if (split)
			order++;

		DRM_DEBUG_ATOMIC("[PLANE:%d:%s] zpos:%d, normalized zpos: %d\n",
				 plane->base.id, plane->name, plane_st->zpos,
				 plane_st->normalized_zpos);

		/* calculate max slave zorder */
		if (has_bit(drm_plane_index(plane), kcrtc->slave_planes))
			kcrtc_st->max_slave_zorder =
				max(!split ? plane_st->normalized_zpos :
					     plane_st->normalized_zpos + 1,
				    kcrtc_st->max_slave_zorder);
	}

	crtc_st->zpos_changed = true;

	return 0;
}

/**
 * linlondp_vrr_match_mode - check if two modes are VRR-compatible.
 * I.e. if both mode have the same timings except for VFP.
 */
static bool linlondp_vrr_match_mode(const struct drm_display_mode *mode1,
				const struct drm_display_mode *mode2)
{
	return drm_mode_match(mode1, mode2,
		DRM_MODE_MATCH_CLOCK |
		DRM_MODE_MATCH_FLAGS |
		DRM_MODE_MATCH_3D_FLAGS|
		DRM_MODE_MATCH_ASPECT_RATIO) &&
		mode1->hdisplay == mode2->hdisplay &&
		mode1->hsync_start == mode2->hsync_start &&
		mode1->hsync_end == mode2->hsync_end &&
		mode1->htotal == mode2->htotal &&
		mode1->hskew == mode2->hskew &&
		mode1->vdisplay == mode2->vdisplay &&
		mode1->vsync_end - mode1->vsync_start ==
		mode2->vsync_end - mode2->vsync_start &&
		mode1->vtotal - mode1->vsync_end ==
		mode2->vtotal - mode2->vsync_end &&
		mode1->vscan == mode2->vscan;
}

static void linlondp_kms_put_pre_blank_locked(struct linlondp_kms_dev *kms)
{
	if (kms->pre_blank_state) {
		drm_atomic_state_put(kms->pre_blank_state);
		kms->pre_blank_state = NULL;
	}
}

static void linlondp_kms_maybe_save_pre_blank_state(struct linlondp_kms_dev *kms,
						    struct drm_atomic_state *state)
{
	struct drm_device *drm = &kms->base;
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_st, *new_st;
	struct drm_atomic_state *snap, *old_snap;
	bool blanking = false;
	int i;

	for_each_oldnew_crtc_in_state(state, crtc, old_st, new_st, i) {
		if (old_st->active && !new_st->active) {
			blanking = true;
			break;
		}
	}
	if (!blanking || !state->acquire_ctx)
		return;

	snap = drm_atomic_helper_duplicate_state(drm, state->acquire_ctx);
	if (IS_ERR_OR_NULL(snap))
		return;

	mutex_lock(&kms->pre_blank_lock);
	old_snap = kms->pre_blank_state;
	kms->pre_blank_state = snap;
	mutex_unlock(&kms->pre_blank_lock);

	if (old_snap)
		drm_atomic_state_put(old_snap);

	DRM_DEBUG_ATOMIC("linlondp: saved pre-blank state for STR\n");
}

void linlondp_kms_clear_pre_blank_state(struct drm_device *drm)
{
	struct linlondp_kms_dev *kms = to_kdev(drm);

	if (!kms)
		return;

	mutex_lock(&kms->pre_blank_lock);
	linlondp_kms_put_pre_blank_locked(kms);
	mutex_unlock(&kms->pre_blank_lock);
}

bool linlondp_kms_fixup_inactive_suspend_state(struct drm_device *drm)
{
	struct linlondp_kms_dev *kms = to_kdev(drm);
	struct drm_atomic_state *fallback, *inactive;
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_st;
	bool has_active = false;
	int i;

	if (!kms)
		return false;

	mutex_lock(&kms->pre_blank_lock);
	fallback = kms->pre_blank_state;
	kms->pre_blank_state = NULL;
	mutex_unlock(&kms->pre_blank_lock);

	if (!fallback)
		return false;

	for_each_new_crtc_in_state(fallback, crtc, crtc_st, i) {
		if (crtc_st->active) {
			has_active = true;
			break;
		}
	}
	if (!has_active) {
		drm_atomic_state_put(fallback);
		return false;
	}

	mutex_lock(&drm->mode_config.mutex);
	inactive = drm->mode_config.suspend_state;
	drm->mode_config.suspend_state = fallback;
	mutex_unlock(&drm->mode_config.mutex);

	if (inactive)
		drm_atomic_state_put(inactive);

	return true;
}

static int linlondp_kms_check(struct drm_device *dev,
			      struct drm_atomic_state *state)
{
	struct linlondp_kms_dev *kms = to_kdev(dev);
	struct drm_plane *plane;
	struct drm_plane_state *new_plane_st;
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_st, *new_crtc_st;
	int i, err;
	unsigned int ui;

	for (ui = 0; ui < kms->n_hw_mdevs; ui++) {
		if (kms->hw_mdevs[ui]->shutdown)
			return -ENODEV;
	}

	err = drm_atomic_helper_check_modeset(dev, state);
	if (err)
		return err;

	/* Linlondp need to re-calculate resource assumption in every commit
	 * so need to add all affected_planes (even unchanged) to
	 * drm_atomic_state.
	 */

	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_st, new_crtc_st, i) {
		if (new_crtc_st->mode_changed &&
			(old_crtc_st->vrr_enabled || new_crtc_st->vrr_enabled) &&
			linlondp_vrr_match_mode(&old_crtc_st->mode, &new_crtc_st->mode)) {
			new_crtc_st->mode_changed = false;
		}

		err = drm_atomic_add_affected_planes(state, crtc);
		if (err)
			return err;

		err = linlondp_crtc_normalize_zpos(crtc, new_crtc_st);
		if (err)
			return err;
	}

	if (kms->n_hw_mdevs > 1) {
		for_each_new_plane_in_state(state, plane, new_plane_st, i) {
			struct linlondp_plane *kplane;
			struct drm_framebuffer *fb;
			struct linlondp_dev *pmdev;
			int hw_idx;
			unsigned int p;

			if (!new_plane_st->crtc || !new_plane_st->fb)
				continue;

			new_crtc_st = drm_atomic_get_new_crtc_state(
				state, new_plane_st->crtc);
			if (!new_crtc_st || !new_crtc_st->active)
				continue;

			kplane = to_kplane(plane);
			pmdev = kplane->layer->base.pipeline->mdev;
			hw_idx = linlondp_cluster_get_hw_idx(kms, pmdev);
			if (hw_idx <= 0)
				continue;

			fb = new_plane_st->fb;
			for (p = 0; p < fb->format->num_planes; p++) {
				struct drm_gem_object *obj = fb->obj[p];
				struct drm_gem_dma_object *dma_obj;

				if (!obj)
					continue;
				dma_obj = to_drm_gem_dma_obj(obj);
				err = linlondp_cluster_iommu_map_domain(
					kms, dma_obj, hw_idx);
				if (err) {
					DRM_ERROR(
						"linlon cluster: ensure map failed plane=%s dpu=%d err=%d\n",
						plane->name, hw_idx, err);
					return err;
				}
			}
		}
	}

	err = drm_atomic_helper_check_planes(dev, state);
	if (err)
		return err;

	drm_self_refresh_helper_alter_state(state);

	linlondp_kms_maybe_save_pre_blank_state(kms, state);

	return 0;
}

static const struct drm_mode_config_funcs linlondp_mode_config_funcs = {
	.fb_create = linlondp_fb_create,
	.atomic_check = linlondp_kms_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static void linlondp_kms_mode_config_init(struct linlondp_kms_dev *kms,
					struct linlondp_dev **mdevs,
					unsigned int n_mdevs)
{
	struct drm_mode_config *config = &kms->base.mode_config;

	drm_mode_config_init(&kms->base);

	linlondp_kms_setup_crtcs(kms, mdevs, n_mdevs);

	/* Get value from dev */
	config->min_width = 0;
	config->min_height = 0;
	config->max_width = 4096;
	config->max_height = 4096;

	config->funcs = &linlondp_mode_config_funcs;
	config->helper_private = &linlondp_mode_config_helpers;
}

static int linlondp_kms_register_irqs(struct linlondp_kms_dev *kms)
{
	struct drm_device *drm = &kms->base;
	struct linlondp_irq_bundle *bundles;
	unsigned int i;
	int err;

	bundles = devm_kcalloc(drm->dev, kms->n_hw_mdevs, sizeof(*bundles),
			       GFP_KERNEL);
	if (!bundles)
		return -ENOMEM;

	for (i = 0; i < kms->n_hw_mdevs; i++) {
		bundles[i].kms = kms;
		bundles[i].mdev = kms->hw_mdevs[i];
		err = devm_request_irq(kms->hw_mdevs[i]->dev,
				       kms->hw_mdevs[i]->irq,
				       linlondp_kms_irq_handler, IRQF_SHARED,
				       dev_name(drm->dev), &bundles[i]);
		if (err)
			return err;
	}

	return 0;
}

struct linlondp_kms_dev *linlondp_kms_attach(struct linlondp_dev *mdev)
{
	struct linlondp_dev *mdev_arr[1] = { mdev };

	if (enable_render)
		linlondp_kms_driver.driver_features |= DRIVER_RENDER;

	return linlondp_kms_attach_cluster(mdev->dev, mdev_arr, 1);
}

struct linlondp_kms_dev *
linlondp_kms_attach_cluster(struct device *component_master_dev,
			    struct linlondp_dev **mdevs, unsigned int n_mdevs)
{
	struct linlondp_kms_dev *kms;
	struct drm_device *drm;
	struct device *drm_alloc_dev = mdevs[0]->dev;
	unsigned int i;
	int err;

	if (!n_mdevs || n_mdevs > LINLONDP_MAX_CLUSTER_DPUS)
		return ERR_PTR(-EINVAL);

	if (enable_render)
		linlondp_kms_driver.driver_features |= DRIVER_RENDER;

	kms = devm_drm_dev_alloc(drm_alloc_dev, &linlondp_kms_driver,
				 struct linlondp_kms_dev, base);
	if (IS_ERR(kms))
		return kms;

	drm = &kms->base;

	kms->n_hw_mdevs = n_mdevs;
	for (i = 0; i < n_mdevs; i++)
		kms->hw_mdevs[i] = mdevs[i];
	kms->component_master = component_master_dev;
	mutex_init(&kms->pre_blank_lock);

	drm->dev_private = mdevs[0];

	linlondp_kms_mode_config_init(kms, mdevs, n_mdevs);

	err = linlondp_kms_add_private_objs_multi(kms, mdevs, n_mdevs);
	if (err)
		goto cleanup_mode_config;

	err = linlondp_kms_add_planes_multi(kms, mdevs, n_mdevs);
	if (err)
		goto cleanup_mode_config;

	err = drm_vblank_init(drm, kms->n_crtcs);
	if (err)
		goto cleanup_mode_config;

	err = linlondp_kms_add_crtcs(kms, mdevs[0]);
	if (err)
		goto cleanup_mode_config;

	err = linlondp_kms_add_wb_connectors(kms, mdevs[0]);
	if (err)
		goto cleanup_mode_config;

	err = component_bind_all(kms->component_master, kms);
	if (err)
		goto cleanup_mode_config;

	drm_mode_config_reset(drm);
	err = linlondp_kms_register_irqs(kms);
	if (err)
		goto free_component_binding;

	drm_kms_helper_poll_init(drm);

	err = drm_dev_register(drm, 0);
	if (err)
		goto free_interrupts;
	return kms;

free_interrupts:
	drm_kms_helper_poll_fini(drm);
free_component_binding:
	component_unbind_all(kms->component_master, drm);
cleanup_mode_config:
	drm_mode_config_cleanup(drm);
	linlondp_kms_cleanup_private_objs(kms);
	drm->dev_private = NULL;
	return ERR_PTR(err);
}

void linlondp_kms_detach(struct linlondp_kms_dev *kms)
{
	struct drm_device *drm = &kms->base;

	linlondp_kms_clear_pre_blank_state(drm);
	drm_dev_unregister(drm);
	drm_kms_helper_poll_fini(drm);
	drm_atomic_helper_shutdown(drm);
	component_unbind_all(kms->component_master, drm);
	drm_mode_config_cleanup(drm);
	linlondp_kms_cleanup_private_objs(kms);
	drm->dev_private = NULL;
}
