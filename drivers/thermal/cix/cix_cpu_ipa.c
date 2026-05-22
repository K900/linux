// SPDX-License-Identifier: GPL-2.0-only
/*
 * CIX CPU IPA support driver
 *
 * Copyright (C) 2024 Cix Technology Group Co., Ltd.
 */

#include <linux/cix/cpu_ipa.h>
#include <linux/cpu.h>
#include <linux/device.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <asm/cputype.h>

#define REG_OFFSET	0x40

struct cpu_ipa_info {
	u32 off_cnt;
	u32 rsvd[13];
	s32 dynamic_power;
	s32 static_power;
};

struct cpu_ipa {
	struct device *dev;
	void __iomem *regs;
};

static struct cpu_ipa *cpu_ipa_dev;

static struct cpu_ipa_info *cpu_ipa_info_for_cpu(unsigned int cpu)
{
	int pcpu;

	if (!cpu_ipa_dev || !cpu_ipa_dev->regs)
		return NULL;

	pcpu = MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 1);
	if (pcpu > 12)
		return NULL;

	return cpu_ipa_dev->regs + pcpu * REG_OFFSET;
}

static int cix_get_static_power(unsigned int cpu)
{
	struct cpu_ipa_info *info = cpu_ipa_info_for_cpu(cpu);

	if (!info)
		return 0;

	return info->static_power;
}

static int cix_get_dynamic_power(unsigned int cpu)
{
	struct cpu_ipa_info *info = cpu_ipa_info_for_cpu(cpu);

	if (!info)
		return 0;

	return info->dynamic_power;
}

int cix_get_static_power_cpus(const struct cpumask *cpus)
{
	unsigned int cpu, total_power = 0;

	for_each_cpu(cpu, cpus)
		total_power += cix_get_static_power(cpu);

	return total_power;
}
EXPORT_SYMBOL_GPL(cix_get_static_power_cpus);

int cix_get_dynamic_power_cpus(const struct cpumask *cpus)
{
	unsigned int cpu, total_power = 0;

	for_each_cpu(cpu, cpus)
		total_power += cix_get_dynamic_power(cpu);

	return total_power;
}
EXPORT_SYMBOL_GPL(cix_get_dynamic_power_cpus);

static int cpu_ipa_probe(struct platform_device *pdev)
{
	struct cpu_ipa *ipa;

	ipa = devm_kzalloc(&pdev->dev, sizeof(*ipa), GFP_KERNEL);
	if (!ipa)
		return -ENOMEM;

	ipa->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(ipa->regs))
		return PTR_ERR(ipa->regs);

	ipa->dev = &pdev->dev;
	platform_set_drvdata(pdev, ipa);
	cpu_ipa_dev = ipa;

	return 0;
}

static void cpu_ipa_remove(struct platform_device *pdev)
{
	cpu_ipa_dev = NULL;
}

#ifdef CONFIG_PM_SLEEP
static int cpu_ipa_resume(struct device *dev)
{
	return 0;
}

static int cpu_ipa_suspend(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops cpu_ipa_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(cpu_ipa_suspend, cpu_ipa_resume)
};

static const struct of_device_id cpu_ipa_of_match[] = {
	{ .compatible = "cix,cpu-ipa", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, cpu_ipa_of_match);

static struct platform_driver cpu_ipa_platdrv = {
	.probe		= cpu_ipa_probe,
	.remove		= cpu_ipa_remove,
	.driver = {
		.name	= "cpu-ipa",
		.pm	= pm_sleep_ptr(&cpu_ipa_pm),
		.of_match_table = cpu_ipa_of_match,
	},
};
module_platform_driver(cpu_ipa_platdrv);

MODULE_DESCRIPTION("CIX CPU IPA support driver");
MODULE_AUTHOR("Cix Technology Group Co., Ltd.");
MODULE_LICENSE("GPL");
