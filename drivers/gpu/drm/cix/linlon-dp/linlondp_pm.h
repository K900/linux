/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINLONDP_PM_H_
#define _LINLONDP_PM_H_

#include <drm/drm_connector.h>
#include <drm/drm_modeset_helper.h>

struct drm_device;
struct device;

struct linlondp_pm_conn_fp {
	u32 connector_id;
	u16 edid_mfg;
	u16 edid_product;
	u32 edid_serial;
	bool edid_valid;
};

struct linlondp_pm_state {
	struct linlondp_pm_conn_fp *suspend_fp;
	unsigned int suspend_fp_count;
	bool capture_failed;
};

static inline void linlondp_pm_free_conn_fp(struct linlondp_pm_state *pm)
{
	kfree(pm->suspend_fp);
	pm->suspend_fp = NULL;
	pm->suspend_fp_count = 0;
}

int linlondp_pm_capture_conn_fp(struct drm_device *drm,
				struct linlondp_pm_conn_fp **out,
				unsigned int *out_count,
				bool connected_only);

bool linlondp_pm_conn_fp_match(struct device *dev,
			       struct drm_device *drm,
			       const struct linlondp_pm_conn_fp *suspend_fp,
			       unsigned int suspend_count);

void linlondp_pm_discard_and_hotplug(struct device *dev,
				     struct drm_device *drm);

int linlondp_pm_suspend_capture(struct device *dev, struct drm_device *drm,
				struct linlondp_pm_state *pm);

int linlondp_pm_resume_check(struct device *dev, struct drm_device *drm,
			     struct linlondp_pm_state *pm);

#endif /* _LINLONDP_PM_H_ */
