#include <linux/platform_device.h>
#include <linux/pm_clock.h>
#include <linux/pm_runtime.h>
#include <linux/property.h>

static int clk_gate_link_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const char *linked_clk;
	int ret;

	ret = device_property_read_string(dev, "linked-clk", &linked_clk);
	if (ret)
		return ret;

	ret = devm_pm_runtime_enable(dev);
	if (ret)
		return ret;

	ret = devm_pm_clk_create(dev);
	if (ret) {
		return ret;
	}

	return pm_clk_add(dev, linked_clk);
}

static const struct dev_pm_ops clk_gate_link_pm_ops = {
	SET_RUNTIME_PM_OPS(pm_clk_suspend, pm_clk_resume, NULL)
};

struct platform_driver clk_gate_link_driver = {
	.probe		= clk_gate_link_probe,
	.driver		= {
		.name	= "rockchip-gate-link-clk",
		.pm = &clk_gate_link_pm_ops,
		.suppress_bind_attrs = true,
	},
};

static int __init rockchip_clk_gate_link_drv_register(void)
{
	return platform_driver_register(&clk_gate_link_driver);
}
core_initcall(rockchip_clk_gate_link_drv_register);
