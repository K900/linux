/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _LINUX_CIX_CPU_IPA_H
#define _LINUX_CIX_CPU_IPA_H

#include <linux/types.h>
#include <linux/cpumask.h>

#ifdef CONFIG_CIX_THERMAL
int cix_get_static_power_cpus(const struct cpumask *cpus);
int cix_get_dynamic_power_cpus(const struct cpumask *cpus);
#else
static inline int cix_get_static_power_cpus(const struct cpumask *cpus)
{
	return 0;
}

static inline int cix_get_dynamic_power_cpus(const struct cpumask *cpus)
{
	return 0;
}
#endif

#endif /* _LINUX_CIX_CPU_IPA_H */
