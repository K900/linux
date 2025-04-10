// SPDX-License-Identifier: GPL-2.0-only
/*
 * SCMI based Cix Energy Model driver
 *
 * Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.
 */

#include <linux/acpi.h>
#include <linux/energy_model.h>
#include <linux/module.h>
#include <linux/pm_opp.h>
#include <linux/scmi_perf_domain.h>
#include <linux/units.h>
#include <linux/cix/cix_scmi_em.h>

/*
 * Cached SCMI perf domain ID for GPU.
 * We discover this by parsing fwnode properties.
 * A value of -1 means not yet discovered.
 */
static int gpu_scmi_domain_id = -1;

/*
 * cix_scmi_dev_domain_id() - Get SCMI perf domain ID from fwnode
 * @dev: The GPU device
 *
 * This function parses the domain ID directly from firmware (DT/ACPI)
 * properties, following the same pattern used by upstream SCMI cpufreq
 * driver after perf_ops->device_domain_id was removed in Linux v6.7.
 *
 * The algorithm:
 * 1. Look for "power-domain-names" property to find the index of "perf" domain
 * 2. Get the phandle args from "power-domains" property at that index
 * 3. The first arg is the SCMI performance domain ID
 * 4. Fallback: if no "perf" named domain, use first power-domain or clock
 *
 * Returns the domain ID, or negative error.
 */
static int cix_scmi_dev_domain_id(struct device *dev)
{
	struct fwnode_reference_args fwnode_args;
	int index;

	/* Find the corresponding index for power-domain "perf". */
	index = fwnode_property_match_string(dev->fwnode,
					     "power-domain-names", "perf");
	if (index < 0) {
		dev_dbg(dev, "No 'perf' power-domain name, trying first clock\n");
		/* Fallback: use first clock phandle as the domain ID */
		if (fwnode_property_get_reference_args(dev->fwnode, "clocks",
						       "#clock-cells", 1, 0,
						       &fwnode_args)) {
			dev_err(dev, "Failed to get domain ID from clocks\n");
			return -EINVAL;
		}
	} else {
		/* Found "perf" power-domain name, get its phandle args */
		if (fwnode_property_get_reference_args(dev->fwnode,
						       "power-domains",
						       "#power-domain-cells",
						       1, index, &fwnode_args)) {
			dev_err(dev, "Failed to get domain ID from power-domains\n");
			return -EINVAL;
		}
	}

	dev_info(dev, "SCMI perf domain ID from fwnode: %llu\n",
		 fwnode_args.args[0]);

	return fwnode_args.args[0];
}

/*
 * cix_scmi_discover_gpu_domain() - Discover SCMI perf domain ID for GPU
 * @dev: The GPU device (e.g., mali device)
 *
 * Parses domain ID directly from fwnode (DT/ACPI) properties, following
 * the upstream pattern used in the SCMI cpufreq driver.
 *
 * Returns the domain ID on success, or negative error.
 */
static int cix_scmi_discover_gpu_domain(struct device *dev)
{
	int domain_id;

	/* Already discovered */
	if (gpu_scmi_domain_id >= 0)
		return gpu_scmi_domain_id;

	/*
	 * Parse domain ID directly from fwnode (DT/ACPI).
	 * This is the upstream-approved method after v6.7 removed
	 * perf_ops->device_domain_id.
	 */
	domain_id = cix_scmi_dev_domain_id(dev);
	if (domain_id < 0) {
		dev_err(dev, "Failed to parse SCMI perf domain ID from fwnode\n");
		return domain_id;
	}

	/* Verify the domain is actually available via SCMI */
	if (scmi_perf_domain_power_scale_by_id(domain_id) == SCMI_POWER_BOGOWATTS) {
		dev_err(dev, "SCMI perf domain %d (from fwnode) is not available\n",
			domain_id);
		return -ENODEV;
	}

	dev_info(dev, "SCMI perf domain %d (from fwnode)\n", domain_id);
	gpu_scmi_domain_id = domain_id;
	return domain_id;
}

static __maybe_unused int
cix_scmi_get_em_power(struct device *dev, unsigned long *power,
		      unsigned long *KHz)
{
	enum scmi_power_scale power_scale;
	unsigned long Hz;
	int domain_id, ret;

	/* Discover the domain ID on first call */
	domain_id = cix_scmi_discover_gpu_domain(dev);
	if (domain_id < 0)
		return domain_id;

	power_scale = scmi_perf_domain_power_scale_by_id(domain_id);
	if (power_scale == SCMI_POWER_BOGOWATTS) {
		dev_err(dev, "SCMI perf domain %d not available\n", domain_id);
		return -ENODEV;
	}

	dev_dbg(dev, "SCMI: using domain %d, power scale: %s\n",
		domain_id,
		power_scale == SCMI_POWER_MILLIWATTS ? "mW" :
		power_scale == SCMI_POWER_MICROWATTS ? "uW" : "BogoWatts");

	/* Get the power from SCMI performance domain. */
	Hz = *KHz * 1000;
	ret = scmi_perf_domain_est_power_by_id(domain_id, &Hz, power);
	if (ret) {
		dev_err(dev, "SCMI est_power_get failed: %d\n", ret);
		return ret;
	}

	dev_dbg(dev, "SCMI: freq=%lu Hz, power=%lu (raw)\n", Hz, *power);

	/* Convert to micro-Watts if needed (EM framework only supports uW) */
	if (power_scale == SCMI_POWER_MILLIWATTS) {
		*power *= MICROWATT_PER_MILLIWATT;
		dev_dbg(dev, "Converted mW -> uW: %lu\n", *power);
	}

	*KHz = Hz / 1000;

	return 0;
}

int cix_scmi_register_em(struct device *dev)
{
	struct em_data_callback em_cb = EM_DATA_CB(cix_scmi_get_em_power);
	int ret, nr_opp;

	dev_dbg(dev, "Registering Energy Model via SCMI\n");

	nr_opp = dev_pm_opp_get_opp_count(dev);
	if (nr_opp <= 0) {
		dev_err(dev, "Failed to get OPP counts: %d\n", nr_opp);
		return -EINVAL;
	}

	dev_dbg(dev, "Found %d OPPs\n", nr_opp);

	/*
	 * Linux v6.7+ Energy Model framework only supports micro-Watt
	 * power values. Since our callback converts milli-Watts to
	 * micro-Watts when needed, we always pass microwatts=true.
	 */
	ret = em_dev_register_perf_domain(dev, nr_opp, &em_cb, NULL, true);
	if (ret)
		dev_err(dev, "Failed to register Energy Model: %d\n", ret);
	else
		dev_info(dev, "Energy Model registered successfully (domain %d)\n",
			 gpu_scmi_domain_id);

	return ret;
}
EXPORT_SYMBOL_GPL(cix_scmi_register_em);

MODULE_AUTHOR("Cixtech,Inc.");
MODULE_DESCRIPTION("CIX SCMI Energy Model interface driver");
MODULE_LICENSE("GPL v2");
