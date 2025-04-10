/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Helpers for querying SCMI perf domain firmware power estimates.
 *
 * Copyright (C) 2025 Sky1-Linux Contributors
 */

#ifndef _LINUX_SCMI_PERF_DOMAIN_H
#define _LINUX_SCMI_PERF_DOMAIN_H

#include <linux/scmi_protocol.h>

#if IS_ENABLED(CONFIG_ARM_SCMI_PERF_DOMAIN)

int scmi_perf_domain_est_power(struct device *dev,
			       unsigned long *rate, unsigned long *power);
enum scmi_power_scale scmi_perf_domain_power_scale(struct device *dev);

/* Direct domain ID access variants (bypasses genpd binding requirement) */
int scmi_perf_domain_est_power_by_id(u32 domain_id, unsigned long *rate,
				      unsigned long *power);
enum scmi_power_scale scmi_perf_domain_power_scale_by_id(u32 domain_id);

#else

static inline int scmi_perf_domain_est_power(struct device *dev,
					     unsigned long *rate,
					     unsigned long *power)
{
	return -ENODEV;
}

static inline enum scmi_power_scale scmi_perf_domain_power_scale(struct device *dev)
{
	return SCMI_POWER_BOGOWATTS;
}

static inline int scmi_perf_domain_est_power_by_id(u32 domain_id,
						    unsigned long *rate,
						    unsigned long *power)
{
	return -ENODEV;
}

static inline enum scmi_power_scale scmi_perf_domain_power_scale_by_id(u32 domain_id)
{
	return SCMI_POWER_BOGOWATTS;
}

#endif /* CONFIG_ARM_SCMI_PERF_DOMAIN */
#endif /* _LINUX_SCMI_PERF_DOMAIN_H */
