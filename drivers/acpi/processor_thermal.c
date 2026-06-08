// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * processor_thermal.c - Passive cooling submodule of the ACPI processor driver
 *
 *  Copyright (C) 2001, 2002 Andy Grover <andrew.grover@intel.com>
 *  Copyright (C) 2001, 2002 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *  Copyright (C) 2004       Dominik Brodowski <linux@brodo.de>
 *  Copyright (C) 2004  Anil S Keshavamurthy <anil.s.keshavamurthy@intel.com>
 *  			- Added processor hotplug support
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/acpi.h>
#include <acpi/processor.h>
#include <linux/uaccess.h>
#include <linux/cix/cpu_ipa.h>
#include <linux/energy_model.h>
#include <acpi/cppc_acpi.h>
#include <asm/cputype.h>

#include "internal.h"

#ifdef CONFIG_CPU_FREQ

/* If a passive cooling situation is detected, primarily CPUfreq is used, as it
 * offers (in most cases) voltage scaling in addition to frequency scaling, and
 * thus a cubic (instead of linear) reduction of energy. Also, we allow for
 * _any_ cpufreq driver and not only the acpi-cpufreq driver.
 */

#define CPUFREQ_THERMAL_MIN_STEP 0

static int cpufreq_thermal_max_step __read_mostly = 3;

/*
 * Minimum throttle percentage for processor_thermal cooling device.
 * The processor_thermal driver uses it to calculate the percentage amount by
 * which cpu frequency must be reduced for each cooling state. This is also used
 * to calculate the maximum number of throttling steps or cooling states.
 */
static int cpufreq_thermal_reduction_pctg __read_mostly = 20;

static DEFINE_PER_CPU(unsigned int, cpufreq_thermal_reduction_step);

#ifdef CONFIG_ARCH_CIX
#define reduction_step(cpu) \
	per_cpu(cpufreq_thermal_reduction_step, cpu)
#else
#define reduction_step(cpu) \
	per_cpu(cpufreq_thermal_reduction_step, phys_package_first_cpu(cpu))
#endif

#ifndef CONFIG_ARCH_CIX
/*
 * Emulate "per package data" using per cpu data (which should really be
 * provided elsewhere)
 *
 * Note we can lose a CPU on cpu hotunplug, in this case we forget the state
 * temporarily. Fortunately that's not a big issue here (I hope)
 */
static int phys_package_first_cpu(int cpu)
{
	int i;
	int id = topology_physical_package_id(cpu);

	for_each_online_cpu(i)
		if (topology_physical_package_id(i) == id)
			return i;
	return 0;
}
#endif

#ifdef CONFIG_CIX_THERMAL
static unsigned int cix_get_static_power(unsigned int cpu)
{
	int pcpu = MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 1);
	unsigned long long temp;
	struct acpi_object_list arg_list;
	union acpi_object arg;
	acpi_status status;

	arg_list.count = 1;
	arg_list.pointer = &arg;
	arg.type = ACPI_TYPE_INTEGER;
	arg.integer.value = pcpu;

	if (pcpu > 12) {
		pr_err("CIX: Invalid CPU[%d %u] for SPRG\n", pcpu, cpu);
		return 0;
	}

	status = acpi_evaluate_integer(NULL, "\\_SB.SPRG", &arg_list, &temp);
	if (ACPI_FAILURE(status) && status != AE_NOT_FOUND) {
		pr_err("failed to evaluate SPRG %s\n",
		       acpi_format_exception(status));
		return 0;
	}

	return (unsigned int)(temp & 0xFFFFFFFF);
}

static unsigned int cix_get_dynamic_power(unsigned int cpu)
{
	int pcpu = MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 1);
	unsigned long long temp;
	struct acpi_object_list arg_list;
	union acpi_object arg;
	acpi_status status;

	arg_list.count = 1;
	arg_list.pointer = &arg;
	arg.type = ACPI_TYPE_INTEGER;
	arg.integer.value = pcpu;

	if (pcpu > 12) {
		pr_err("CIX: Invalid CPU[%d %u] for DPRG\n", pcpu, cpu);
		return 0;
	}

	status = acpi_evaluate_integer(NULL, "\\_SB.DPRG", &arg_list, &temp);
	if (ACPI_FAILURE(status) && status != AE_NOT_FOUND) {
		pr_err("failed to evaluate DPRG %s\n",
		       acpi_format_exception(status));
		return 0;
	}

	return (unsigned int)(temp & 0xFFFFFFFF);
}

static int processor_get_static_power_cpus(const struct cpumask *cpus)
{
	int power = cix_get_static_power_cpus(cpus);
	unsigned int cpu;

	if (power)
		return power;

	for_each_cpu(cpu, cpus)
		power += cix_get_static_power(cpu);

	return power;
}

static int processor_get_dynamic_power_cpus(const struct cpumask *cpus)
{
	int power = cix_get_dynamic_power_cpus(cpus);
	unsigned int cpu;

	if (power)
		return power;

	for_each_cpu(cpu, cpus)
		power += cix_get_dynamic_power(cpu);

	return power;
}
#endif

static bool cpu_has_cpufreq(unsigned int cpu)
{
	if (!acpi_processor_cpufreq_init)
		return 0;

	struct cpufreq_policy *policy __free(put_cpufreq_policy) = cpufreq_cpu_get(cpu);

	return policy != NULL;
}

static int cpufreq_get_max_state(unsigned int cpu)
{
#ifndef CONFIG_CIX_THERMAL
	if (!cpu_has_cpufreq(cpu))
		return 0;

	return cpufreq_thermal_max_step;
#else
	struct cpufreq_policy *policy __free(put_cpufreq_policy) =
		cpufreq_cpu_get(cpu);
	const struct cppc_cpudata *cpu_data;

	if (!policy)
		return cpufreq_thermal_max_step;

	cpu_data = policy->driver_data;
	if (cpu_data && cpu_data->opp_level_num > 1)
		return cpu_data->opp_level_num - 1;

	return cpufreq_thermal_max_step;
#endif
}

static int cpufreq_get_cur_state(unsigned int cpu)
{
	if (!cpu_has_cpufreq(cpu))
		return 0;

	return reduction_step(cpu);
}

static bool cpufreq_update_thermal_limit(unsigned int cpu, struct acpi_processor *pr)
{
	unsigned long max_freq;
	int ret;

	struct cpufreq_policy *policy __free(put_cpufreq_policy) = cpufreq_cpu_get(cpu);
	if (!policy)
		return false;

	max_freq = (policy->cpuinfo.max_freq *
		(100 - reduction_step(cpu) * cpufreq_thermal_reduction_pctg)) / 100;

	ret = freq_qos_update_request(&pr->thermal_req, max_freq);
	if (ret < 0) {
		pr_warn("Failed to update thermal freq constraint: CPU%d (%d)\n",
			pr->id, ret);
	}

	return true;
}

static int cpufreq_set_cur_state(unsigned int cpu, int state)
{
	struct acpi_processor *pr;
	int i;

	if (!cpu_has_cpufreq(cpu))
		return 0;

	reduction_step(cpu) = state;

	/*
	 * Update all the CPUs in the same package because they all
	 * contribute to the temperature and often share the same
	 * frequency.
	 */
	for_each_online_cpu(i) {
		if (topology_physical_package_id(i) !=
		    topology_physical_package_id(cpu))
			continue;

		pr = per_cpu(processors, i);

		if (unlikely(!freq_qos_request_active(&pr->thermal_req)))
			continue;

		if (!cpufreq_update_thermal_limit(i, pr))
			return -EINVAL;
	}
	return 0;
}

static void acpi_thermal_cpufreq_config(void)
{
	int cpufreq_pctg = acpi_arch_thermal_cpufreq_pctg();

	if (!cpufreq_pctg)
		return;

	cpufreq_thermal_reduction_pctg = cpufreq_pctg;

	/*
	 * Derive the MAX_STEP from minimum throttle percentage so that the reduction
	 * percentage doesn't end up becoming negative. Also, cap the MAX_STEP so that
	 * the CPU performance doesn't become 0.
	 */
	cpufreq_thermal_max_step = (100 / cpufreq_pctg) - 2;
}

void acpi_thermal_cpufreq_init(struct cpufreq_policy *policy)
{
	unsigned int cpu;

	acpi_thermal_cpufreq_config();

	for_each_cpu(cpu, policy->related_cpus) {
		struct acpi_processor *pr = per_cpu(processors, cpu);
		int ret;

		if (!pr)
			continue;

		ret = freq_qos_add_request(&policy->constraints,
					   &pr->thermal_req,
					   FREQ_QOS_MAX, INT_MAX);
		if (ret < 0) {
			pr_err("Failed to add freq constraint for CPU%d (%d)\n",
			       cpu, ret);
			continue;
		}

		thermal_cooling_device_update(pr->cdev);
	}
}

void acpi_thermal_cpufreq_exit(struct cpufreq_policy *policy)
{
	unsigned int cpu;

	for_each_cpu(cpu, policy->related_cpus) {
		struct acpi_processor *pr = per_cpu(processors, cpu);

		if (!pr)
			continue;

		freq_qos_remove_request(&pr->thermal_req);

		thermal_cooling_device_update(pr->cdev);
	}
}
#else				/* ! CONFIG_CPU_FREQ */
static int cpufreq_get_max_state(unsigned int cpu)
{
	return 0;
}

static int cpufreq_get_cur_state(unsigned int cpu)
{
	return 0;
}

static int cpufreq_set_cur_state(unsigned int cpu, int state)
{
	return 0;
}

#endif

/* thermal cooling device callbacks */
static int acpi_processor_max_state(struct acpi_processor *pr)
{
	int max_state = 0;

	/*
	 * There exists four states according to
	 * cpufreq_thermal_reduction_step. 0, 1, 2, 3
	 */
	max_state += cpufreq_get_max_state(pr->id);
	if (pr->flags.throttling)
		max_state += (pr->throttling.state_count -1);

	return max_state;
}
static int
processor_get_max_state(struct thermal_cooling_device *cdev,
			unsigned long *state)
{
	struct acpi_device *device = cdev->devdata;
	struct acpi_processor *pr;

	if (!device)
		return -EINVAL;

	pr = acpi_driver_data(device);
	if (!pr)
		return -EINVAL;

	*state = acpi_processor_max_state(pr);
	return 0;
}

static int
processor_get_cur_state(struct thermal_cooling_device *cdev,
			unsigned long *cur_state)
{
	struct acpi_device *device = cdev->devdata;
	struct acpi_processor *pr;

	if (!device)
		return -EINVAL;

	pr = acpi_driver_data(device);
	if (!pr)
		return -EINVAL;

	*cur_state = cpufreq_get_cur_state(pr->id);
	if (pr->flags.throttling)
		*cur_state += pr->throttling.state;
	return 0;
}

static int
processor_set_cur_state(struct thermal_cooling_device *cdev,
			unsigned long state)
{
	struct acpi_device *device = cdev->devdata;
	struct acpi_processor *pr;
	int result = 0;
	int max_pstate;

	if (!device)
		return -EINVAL;

	pr = acpi_driver_data(device);
	if (!pr)
		return -EINVAL;

	max_pstate = cpufreq_get_max_state(pr->id);

	if (state > acpi_processor_max_state(pr))
		return -EINVAL;

	if (state <= max_pstate) {
		if (pr->flags.throttling && pr->throttling.state)
			result = acpi_processor_set_throttling(pr, 0, false);
		cpufreq_set_cur_state(pr->id, state);
	} else {
		cpufreq_set_cur_state(pr->id, max_pstate);
		result = acpi_processor_set_throttling(pr,
				state - max_pstate, false);
	}
	return result;
}

#ifdef CONFIG_CIX_THERMAL
static int processor_get_requested_power(struct thermal_cooling_device *cdev,
					 u32 *power)
{
	struct acpi_device *device = cdev->devdata;
	struct cpufreq_policy *policy;
	struct acpi_processor *pr;

	if (!device)
		return -EINVAL;

	pr = acpi_driver_data(device);
	if (!pr || pr->id >= num_possible_cpus())
		return -EINVAL;

	policy = cpufreq_cpu_get(pr->id);
	if (!policy)
		return -EINVAL;

	*power = processor_get_static_power_cpus(policy->cpus) +
		 processor_get_dynamic_power_cpus(policy->cpus);
	cpufreq_cpu_put(policy);

	return 0;
}

static int processor_state2power(struct thermal_cooling_device *cdev,
				 unsigned long state, u32 *power)
{
	unsigned int opp_power, num_cpus, idx;
	struct cpufreq_policy *policy;
	struct cppc_cpudata *cpu_data;
	struct em_perf_domain *em;
	struct em_perf_state *table;
	struct acpi_device *device = cdev->devdata;
	struct acpi_processor *pr;

	if (!device)
		return -EINVAL;

	pr = acpi_driver_data(device);
	if (!pr || pr->id >= num_possible_cpus())
		return -EINVAL;

	policy = cpufreq_cpu_get(pr->id);
	if (!policy)
		return -EINVAL;

	cpu_data = policy->driver_data;
	if (!cpu_data || !cpu_data->opp_level_num) {
		cpufreq_cpu_put(policy);
		return -ENODEV;
	}

	if (state >= cpu_data->opp_level_num) {
		cpufreq_cpu_put(policy);
		return -EINVAL;
	}

	em = em_cpu_get(policy->cpu);
	if (!em) {
		cpufreq_cpu_put(policy);
		return -ENODEV;
	}

	num_cpus = cpumask_weight(policy->cpus);
	idx = cpu_data->opp_level_num - state - 1;

	rcu_read_lock();
	table = em_perf_state_from_pd(em);
	opp_power = table[idx].power;
	rcu_read_unlock();

	*power = opp_power * num_cpus;
	*power += processor_get_static_power_cpus(policy->cpus);
	cpufreq_cpu_put(policy);

	return 0;
}

static int processor_power2state(struct thermal_cooling_device *cdev,
				 u32 power, unsigned long *state)
{
	struct cpufreq_policy *policy;
	struct cppc_cpudata *cpu_data;
	struct em_perf_domain *em;
	struct em_perf_state *table;
	u32 normalised_power, em_power_mw, static_power;
	int i;
	struct acpi_device *device = cdev->devdata;
	struct acpi_processor *pr;

	if (!device)
		return -EINVAL;

	pr = acpi_driver_data(device);
	if (!pr || pr->id >= num_possible_cpus())
		return -EINVAL;

	policy = cpufreq_cpu_get(pr->id);
	if (!policy)
		return -EINVAL;

	cpu_data = policy->driver_data;
	if (!cpu_data || !cpu_data->opp_level_num) {
		cpufreq_cpu_put(policy);
		return -ENODEV;
	}

	static_power = processor_get_static_power_cpus(policy->cpus);
	normalised_power = power > static_power ? power - static_power : 0;

	em = em_cpu_get(policy->cpu);
	if (!em) {
		cpufreq_cpu_put(policy);
		return -ENODEV;
	}

	rcu_read_lock();
	table = em_perf_state_from_pd(em);

	for (i = cpu_data->opp_level_num - 1; i >= 0; i--) {
		em_power_mw = table[i].power;
		if (normalised_power >= em_power_mw)
			break;
	}
	rcu_read_unlock();

	cpufreq_cpu_put(policy);

	if (i < 0) {
		*state = cpu_data->opp_level_num - 1;
		return 0;
	}

	*state = cpu_data->opp_level_num - i - 1;
	return 0;
}
#endif

const struct thermal_cooling_device_ops processor_cooling_ops = {
	.get_max_state = processor_get_max_state,
	.get_cur_state = processor_get_cur_state,
	.set_cur_state = processor_set_cur_state,
#ifdef CONFIG_CIX_THERMAL
	.get_requested_power = processor_get_requested_power,
	.power2state = processor_power2state,
	.state2power = processor_state2power,
#endif
};

int acpi_processor_thermal_init(struct acpi_processor *pr,
				struct acpi_device *device)
{
	int result = 0;

	pr->cdev = thermal_cooling_device_register("Processor", device,
						   &processor_cooling_ops);
	if (IS_ERR(pr->cdev)) {
		result = PTR_ERR(pr->cdev);
		return result;
	}

	dev_dbg(&device->dev, "registered as cooling_device%d\n",
		pr->cdev->id);

	result = sysfs_create_link(&device->dev.kobj,
				   &pr->cdev->device.kobj,
				   "thermal_cooling");
	if (result) {
		dev_err(&device->dev,
			"Failed to create sysfs link 'thermal_cooling'\n");
		goto err_thermal_unregister;
	}

	result = sysfs_create_link(&pr->cdev->device.kobj,
				   &device->dev.kobj,
				   "device");
	if (result) {
		dev_err(&pr->cdev->device,
			"Failed to create sysfs link 'device'\n");
		goto err_remove_sysfs_thermal;
	}

	return 0;

err_remove_sysfs_thermal:
	sysfs_remove_link(&device->dev.kobj, "thermal_cooling");
err_thermal_unregister:
	thermal_cooling_device_unregister(pr->cdev);

	return result;
}

void acpi_processor_thermal_exit(struct acpi_processor *pr,
				 struct acpi_device *device)
{
	if (pr->cdev) {
		sysfs_remove_link(&device->dev.kobj, "thermal_cooling");
		sysfs_remove_link(&pr->cdev->device.kobj, "device");
		thermal_cooling_device_unregister(pr->cdev);
		pr->cdev = NULL;
	}
}
