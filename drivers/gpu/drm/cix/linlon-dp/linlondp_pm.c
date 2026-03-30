// SPDX-License-Identifier: GPL-2.0
/*
 * Shared PM helpers for linlon-dp (cluster and non-cluster).
 *
 * EDID fingerprint capture/match for STR: detect monitor swap during suspend
 * and force userspace re-modeset via discard + hotplug.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/slab.h>

#include <drm/drm_atomic.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>
#include <drm/drm_edid.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_modeset_helper.h>
#include <drm/drm_probe_helper.h>

#include "linlondp_kms.h"
#include "linlondp_pm.h"

#define LP_EDID_HDR_BYTES	16
#define LP_EDID_DDC_RETRIES	4
#define LP_EDID_DDC_RETRY_MS	5

static bool linlondp_pm_edid_identity(const struct drm_connector *connector,
				      u16 *mfg, u16 *product, u32 *serial)
{
	struct drm_property_blob *blob = connector->edid_blob_ptr;
	const u8 *raw;

	if (!blob || blob->length < 16)
		return false;

	raw = blob->data;
	*mfg     = raw[8]  | (raw[9]  << 8);
	*product = raw[10] | (raw[11] << 8);
	*serial  = (u32)raw[12] | ((u32)raw[13] << 8) |
		   ((u32)raw[14] << 16) | ((u32)raw[15] << 24);
	return true;
}

static int linlondp_pm_read_live_edid(struct drm_connector *connector,
				      u16 *mfg, u16 *product, u32 *serial)
{
	struct i2c_adapter *ddc = connector->ddc;
	u8 hdr[LP_EDID_HDR_BYTES];
	unsigned char start = 0;
	struct i2c_msg msgs[] = {
		{ .addr = DDC_ADDR, .flags = 0,
		  .len = 1,                .buf = &start },
		{ .addr = DDC_ADDR, .flags = I2C_M_RD,
		  .len = LP_EDID_HDR_BYTES, .buf = hdr    },
	};
	int ret, try;

	if (!ddc)
		return -ENODEV;
	if (connector->status != connector_status_connected)
		return -ENOTCONN;

	for (try = 0; try < LP_EDID_DDC_RETRIES; try++) {
		ret = i2c_transfer(ddc, msgs, ARRAY_SIZE(msgs));
		if (ret == ARRAY_SIZE(msgs)) {
			*mfg     = hdr[8]  | (hdr[9]  << 8);
			*product = hdr[10] | (hdr[11] << 8);
			*serial  = (u32)hdr[12] | ((u32)hdr[13] << 8) |
				   ((u32)hdr[14] << 16) | ((u32)hdr[15] << 24);
			return 0;
		}
		msleep(LP_EDID_DDC_RETRY_MS);
	}
	return -EIO;
}

int linlondp_pm_capture_conn_fp(struct drm_device *drm,
				struct linlondp_pm_conn_fp **out,
				unsigned int *out_count,
				bool connected_only)
{
	struct drm_connector_list_iter iter;
	struct drm_connector *connector;
	struct linlondp_pm_conn_fp *fp;
	unsigned int count = 0, i = 0;

	*out = NULL;
	*out_count = 0;

	drm_connector_list_iter_begin(drm, &iter);
	drm_for_each_connector_iter(connector, &iter) {
		if (connected_only &&
		    connector->status != connector_status_connected)
			continue;
		count++;
	}
	drm_connector_list_iter_end(&iter);

	if (!count)
		return 0;

	fp = kcalloc(count, sizeof(*fp), GFP_KERNEL);
	if (!fp)
		return -ENOMEM;

	drm_connector_list_iter_begin(drm, &iter);
	drm_for_each_connector_iter(connector, &iter) {
		if (connected_only &&
		    connector->status != connector_status_connected)
			continue;

		fp[i].connector_id = connector->base.id;
		fp[i].edid_valid = linlondp_pm_edid_identity(connector,
							     &fp[i].edid_mfg,
							     &fp[i].edid_product,
							     &fp[i].edid_serial);
		i++;
	}
	drm_connector_list_iter_end(&iter);

	*out = fp;
	*out_count = count;
	return 0;
}

bool linlondp_pm_conn_fp_match(struct device *dev,
			       struct drm_device *drm,
			       const struct linlondp_pm_conn_fp *suspend_fp,
			       unsigned int suspend_count)
{
	struct drm_connector_list_iter iter;
	struct drm_connector *connector;
	unsigned int i;
	int ret;
	u16 mfg, product;
	u32 serial;

	if (!suspend_count) {
		goto mismatch;
	}
	for (i = 0; i < suspend_count; i++) {
		bool found = false;

		if (!suspend_fp[i].edid_valid)
			goto mismatch;

		drm_connector_list_iter_begin(drm, &iter);
		drm_for_each_connector_iter(connector, &iter) {
			if (connector->base.id == suspend_fp[i].connector_id) {
				found = true;
				break;
			}
		}

		if (!found) {
			drm_connector_list_iter_end(&iter);
			goto mismatch;
		}

		/* connector is still valid (iter not ended yet) */
		ret = linlondp_pm_read_live_edid(connector,
						 &mfg, &product, &serial);
		drm_connector_list_iter_end(&iter);

		if (ret)
			goto mismatch;

		if (suspend_fp[i].edid_mfg != mfg ||
		    suspend_fp[i].edid_product != product ||
		    suspend_fp[i].edid_serial != serial) {
			dev_info(dev,
				 "pm_fp_match id=%u suspend mfg=0x%04x prod=0x%04x serial=0x%08x "
				 "live mfg=0x%04x prod=0x%04x serial=0x%08x\n",
				 suspend_fp[i].connector_id,
				 suspend_fp[i].edid_mfg,
				 suspend_fp[i].edid_product,
				 suspend_fp[i].edid_serial,
				 mfg, product, serial);
			goto mismatch;
		}
	}
	return true;

mismatch:
	return false;
}

void linlondp_pm_discard_and_hotplug(struct device *dev,
				     struct drm_device *drm)
{
	struct drm_atomic_state *state;

	dev_info(dev, "%s: discard suspend_state, reset + hotplug\n", __func__);

	mutex_lock(&drm->mode_config.mutex);
	state = drm->mode_config.suspend_state;
	if (state && !IS_ERR(state))
		drm_atomic_state_put(state);
	drm->mode_config.suspend_state = NULL;
	drm_mode_config_reset(drm);
	mutex_unlock(&drm->mode_config.mutex);

	drm_fb_helper_set_suspend_unlocked(drm->fb_helper, 0);
	if (drm->mode_config.poll_enabled)
		drm_kms_helper_poll_enable(drm);
	drm_kms_helper_hotplug_event(drm);
}

static bool linlondp_pm_suspend_state_has_active_crtc(struct drm_device *drm)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	int i;

	if (!drm->mode_config.suspend_state ||
	    IS_ERR(drm->mode_config.suspend_state))
		return false;

	for_each_new_crtc_in_state(drm->mode_config.suspend_state, crtc,
				    crtc_state, i) {
		if (crtc_state->active)
			return true;
	}

	return false;
}

int linlondp_pm_suspend_capture(struct device *dev, struct drm_device *drm,
				struct linlondp_pm_state *pm)
{
	int ret;

	linlondp_pm_free_conn_fp(pm);
	pm->capture_failed = false;

	ret = linlondp_pm_capture_conn_fp(drm, &pm->suspend_fp,
					  &pm->suspend_fp_count, true);
	if (ret) {
		dev_warn(dev,
			 "%s: capture EDID identity failed %d, resume will discard\n",
			 __func__, ret);
		pm->capture_failed = true;
		linlondp_pm_free_conn_fp(pm);
	}

	ret = drm_mode_config_helper_suspend(drm);
	if (ret) {
		dev_info(dev, "%s: drm_mode_config_helper_suspend failed %d\n",
			 __func__, ret);
		linlondp_pm_free_conn_fp(pm);
		pm->capture_failed = false;
		return ret;
	}

	if (linlondp_pm_suspend_state_has_active_crtc(drm)) {
		linlondp_kms_clear_pre_blank_state(drm);
	} else if (pm->suspend_fp_count > 0 &&
		   linlondp_kms_fixup_inactive_suspend_state(drm)) {
		dev_info(dev,
			 "%s: suspend_state inactive, using pre-blank snapshot for helper_resume\n",
			 __func__);
	} else {
		linlondp_kms_clear_pre_blank_state(drm);
		if (!pm->suspend_fp_count) {
			dev_info(dev,
				 "%s: no connected displays, drop stale pre-blank snapshot\n",
				 __func__);
		} else {
			dev_info(dev,
				 "%s: suspend_state has no active CRTC and no pre-blank snapshot\n",
				 __func__);
		}
	}

	return 0;
}

int linlondp_pm_resume_check(struct device *dev, struct drm_device *drm,
			     struct linlondp_pm_state *pm)
{
	bool fp_match;
	bool can_restore;
	int ret;

	if (pm->capture_failed) {
		dev_info(dev, "%s: capture failed at suspend, force discard\n",
			 __func__);
		fp_match = false;
	} else {
		fp_match = linlondp_pm_conn_fp_match(dev, drm,
						     pm->suspend_fp,
						     pm->suspend_fp_count);
	}
	linlondp_pm_free_conn_fp(pm);
	pm->capture_failed = false;

	can_restore = fp_match && drm->mode_config.suspend_state &&
		      !IS_ERR(drm->mode_config.suspend_state) &&
		      linlondp_pm_suspend_state_has_active_crtc(drm);

	if (can_restore) {
		dev_dbg(dev, "%s: EDID identity match, helper_resume\n",
			 __func__);
		ret = drm_mode_config_helper_resume(drm);
		if (ret) {
			dev_info(dev,
				 "%s: helper_resume failed %d, discard+hotplug\n",
				 __func__, ret);
			linlondp_pm_discard_and_hotplug(dev, drm);
		}
		return 0;
	}

	dev_dbg(dev,
		 "%s: EDID identity mismatch or no suspend_state, discard+hotplug\n",
		 __func__);
	linlondp_pm_discard_and_hotplug(dev, drm);
	return 0;
}
