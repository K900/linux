/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _PCIE_SKY1_ACPI_H
#define _PCIE_SKY1_ACPI_H

struct acpi_device;

/*
 * Platform data passed from pci-sky1-acpi.c scan handler to pci-sky1.c
 * probe function.  CIXH2020 platform devices are created without fwnode
 * (to avoid fw_devlink at device_add time), then the ACPI companion is
 * bound post-registration via acpi_bind_one().  The pdata carries the
 * acpi_device for fallback binding in the driver's probe function.
 */
struct sky1_pcie_acpi_pdata {
	struct acpi_device *adev;
};

#endif /* _PCIE_SKY1_ACPI_H */
