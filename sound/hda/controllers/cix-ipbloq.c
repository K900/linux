// SPDX-License-Identifier: GPL-2.0
// Copyright 2025 Cix Technology Group Co., Ltd.

#include <linux/acpi.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-direct.h>
#include <linux/dma-map-ops.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/string.h>

#include <sound/hda_codec.h>
#include "hda_controller.h"

#define CIX_IPBLOQ_JACKPOLL_DEFAULT_TIME_MS		1000
#define CIX_IPBLOQ_POWER_SAVE_DEFAULT_TIME_MS		100

#define CIX_IPBLOQ_SKY1_ADDR_HOST_TO_HDAC_OFFSET	(-0x90000000ULL)
/*
 * CIX Sky1 HDA DMA address translation.
 * The HDA controller sits behind the audio subsystem bus which has a
 * fixed offset: CPU phys 0x90000000 maps to DMA addr 0x0 on the controller.
 * Under DT this is handled via dma-ranges on the hda_bus simple-bus node.
 * Under ACPI we must configure the offset explicitly.
 */
#define CIX_HDA_DMA_CPU_START	0x90000000ULL
#define CIX_HDA_DMA_DEV_START	0x0ULL
#define CIX_HDA_DMA_SIZE	0x80000000ULL

struct cix_ipbloq_hda {
	struct azx chip;
	struct device *dev;
	void __iomem *regs;

	struct reset_control *reset;
	struct clk_bulk_data clocks[2];
	unsigned int nclocks;

	struct gpio_desc *pdb_gpiod;
};

static const struct hda_controller_ops cix_ipbloq_hda_ops;

static int cix_ipbloq_hda_dev_disconnect(struct snd_device *device)
{
	struct azx *chip = device->device_data;

	chip->bus.shutdown = 1;

	return 0;
}

static int cix_ipbloq_hda_dev_free(struct snd_device *device)
{
	struct azx *chip = device->device_data;

	if (azx_bus(chip)->chip_init) {
		azx_stop_all_streams(chip);
		azx_stop_chip(chip);
	}

	azx_free_stream_pages(chip);
	azx_free_streams(chip);
	snd_hdac_bus_exit(azx_bus(chip));

	return 0;
}

static int cix_ipbloq_hda_probe_codec(struct cix_ipbloq_hda *hda)
{
	struct azx *chip = &hda->chip;
	struct hdac_bus *bus = azx_bus(chip);
	int err;

	to_hda_bus(bus)->bus_probing = 1;

	/* create codec instances */
	err = azx_probe_codecs(chip, 8);
	if (err < 0) {
		dev_err(hda->dev, "probe codecs failed: %d\n", err);
		return err;
	}

	err = azx_codec_configure(chip);
	if (err < 0) {
		dev_err(hda->dev, "codec configure failed: %d\n", err);
		return err;
	}

	err = snd_card_register(chip->card);
	if (err < 0) {
		dev_err(hda->dev, "card register failed: %d\n", err);
		return err;
	}

	chip->running = 1;

	to_hda_bus(bus)->bus_probing = 0;

	snd_hda_set_power_save(&chip->bus, CIX_IPBLOQ_POWER_SAVE_DEFAULT_TIME_MS);

	return 0;
}

static int cix_ipbloq_hda_init(struct cix_ipbloq_hda *hda,
			       struct azx *chip,
			       struct platform_device *pdev)
{
	const char *sname = NULL, *drv_name = "cix-ipbloq-hda";
	bool has_model;
	struct hdac_bus *bus = azx_bus(chip);
	struct snd_card *card = chip->card;
	struct resource *res;
	unsigned short gcap;
	int irq_id, err;

	hda->regs = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(hda->regs)) {
		dev_err(hda->dev, "failed to get and ioremap resource\n");
		return PTR_ERR(hda->regs);
	}
	bus->remap_addr = hda->regs;
	bus->addr = res->start;

	irq_id = platform_get_irq(pdev, 0);
	if (irq_id < 0)
		return irq_id;

	err = devm_request_irq(hda->dev, irq_id, azx_interrupt,
			       0, KBUILD_MODNAME, chip);
	if (err < 0)
		return dev_err_probe(hda->dev, err,
				     "unable to request IRQ %d : err = %d\n", irq_id, err);
	bus->irq = irq_id;
	card->sync_irq = bus->irq;

	gcap = azx_readw(chip, GCAP);
	chip->capture_streams = (gcap >> 8) & 0x0f;
	chip->playback_streams = (gcap >> 12) & 0x0f;
	chip->capture_index_offset = 0;
	chip->playback_index_offset = chip->capture_streams;
	chip->num_streams = chip->playback_streams + chip->capture_streams;

	/* initialize streams */
	err = azx_init_streams(chip);
	if (err < 0) {
		dev_err(hda->dev, "failed to initialize streams: %d\n", err);
		return err;
	}

	err = azx_alloc_stream_pages(chip);
	if (err < 0) {
		dev_err(hda->dev, "failed to allocate stream pages: %d\n", err);
		return err;
	}

	/* initialize chip */
	azx_init_chip(chip, 1);

	/* codec detection */
	if (!bus->codec_mask) {
		dev_err(hda->dev, "no codecs found\n");
		return -ENODEV;
	}
	dev_dbg(card->dev, "codec detection mask = 0x%lx\n", bus->codec_mask);

	/* driver name */
	strscpy(card->driver, drv_name, sizeof(card->driver));

	/* shortname for card */
	has_model = !device_property_read_string(&pdev->dev, "cix,model", &sname);
	if (!has_model)
		sname = drv_name;
	if (strlen(sname) > sizeof(card->shortname))
		dev_dbg(card->dev, "truncating shortname for card\n");
	strscpy(card->shortname, sname, sizeof(card->shortname));

	/*
	 * When the driver owns codec initialization (firmware provides
	 * cix,model), expose the board model to the codec layer so the realtek
	 * fixup table is matched by model name and the board's verb-table init
	 * runs. modelname (in hda_bus) is kstrdup'd into codec->modelname at
	 * codec creation (codec.c), and snd_hda_pick_fixup() matches it against
	 * the codec driver's model table first, before SSID. This lets realtek
	 * pick a per-board fixup even though these boards share a codec SSID.
	 *
	 * When cix,model is absent, the firmware initializes the codec itself,
	 * so modelname stays NULL: no fixup matches and no driver-side verb init
	 * runs. The pdb power GPIO is skipped too (see probe/resume).
	 */
	if (has_model)
		to_hda_bus(bus)->modelname = sname;

	/* longname for card */
	snprintf(card->longname, sizeof(card->longname),
		 "%s at 0x%lx irq %i",
		 card->shortname, bus->addr, bus->irq);

	return 0;
}

static int cix_ipbloq_hda_create(struct cix_ipbloq_hda *hda,
				 struct snd_card *card,
				 unsigned int driver_caps)
{
	static const struct snd_device_ops ops = {
		.dev_disconnect = cix_ipbloq_hda_dev_disconnect,
		.dev_free = cix_ipbloq_hda_dev_free,
	};
	struct azx *chip;
	int err;

	chip = &hda->chip;
	chip->card = card;
	chip->ops = &cix_ipbloq_hda_ops;
	chip->driver_caps = driver_caps;
	chip->driver_type = driver_caps & 0xff;
	chip->dev_index = 0;
	chip->single_cmd = 0;
	chip->codec_probe_mask = -1;
	chip->align_buffer_size = 1;
	chip->jackpoll_interval = msecs_to_jiffies(CIX_IPBLOQ_JACKPOLL_DEFAULT_TIME_MS);
	mutex_init(&chip->open_mutex);
	INIT_LIST_HEAD(&chip->pcm_list);

	/*
	 * HD-audio controllers appear pretty inaccurate about the update-IRQ timing.
	 * The IRQ is issued before actually the data is processed. So use stream
	 * link position by default instead of dma position buffer.
	 */
	chip->get_position[0] = chip->get_position[1] = azx_get_pos_lpib;

	err = azx_bus_init(chip, NULL);
	if (err < 0) {
		dev_err(hda->dev, "failed to init bus, err = %d\n", err);
		return err;
	}

	/* RIRBSTS.RINTFL cannot be cleared, cause interrupt storm */
	chip->bus.core.polling_mode = 1;
	chip->bus.core.not_use_interrupts = 1;

	chip->bus.core.aligned_mmio = 1;
	chip->bus.core.dma_stop_delay = 100;
	if (hda->dev->of_node)
		chip->bus.core.addr_offset = (dma_addr_t)CIX_IPBLOQ_SKY1_ADDR_HOST_TO_HDAC_OFFSET;

	chip->bus.jackpoll_in_suspend = 1;

	err = snd_device_new(card, SNDRV_DEV_LOWLEVEL, chip, &ops);
	if (err < 0) {
		dev_err(card->dev, "failed to create device, err = %d\n", err);
		return err;
	}

	return 0;
}

/*
 * Parse the RSVL table from the CIX audio subsystem resource device
 * (CIXA1019) to find reserved DMA memory for the HDA controller.
 * The RSVL table format is:
 *   Package { Package { base_addr, size, flags_str, consumer_ref }, ... }
 */
#ifdef CONFIG_ACPI
static int cix_ipbloq_hda_setup_acpi_rsvmem(struct device *dev)
{
	struct acpi_device *rest_adev;
	struct acpi_buffer output = {ACPI_ALLOCATE_BUFFER, NULL};
	union acpi_object *out_obj, *entry, *el;
	acpi_status status;
	phys_addr_t base;
	resource_size_t size;
	dma_addr_t bus_addr;
	int i, ret = -ENODEV;

	/* Find the audio subsystem resource device CIXA1019 */
	rest_adev = acpi_dev_get_first_match_dev("CIXA1019", NULL, -1);
	if (!rest_adev)
		return -ENODEV;

	status = acpi_evaluate_object_typed(
		acpi_device_handle(rest_adev), "RSVL", NULL, &output,
		ACPI_TYPE_PACKAGE);
	acpi_dev_put(rest_adev);
	if (ACPI_FAILURE(status))
		return -ENODEV;

	out_obj = output.pointer;

	for (i = 0; i < out_obj->package.count; i++) {
		struct acpi_device *consumer;

		entry = &out_obj->package.elements[i];
		if (entry->type != ACPI_TYPE_PACKAGE || entry->package.count < 4)
			continue;

		/* Element 3 is the consumer device reference */
		el = &entry->package.elements[3];
		if (el->type != ACPI_TYPE_LOCAL_REFERENCE)
			continue;

		consumer = acpi_fetch_acpi_dev(el->reference.handle);
		if (!consumer || consumer != ACPI_COMPANION(dev))
			continue;

		/* Found our entry — extract base and size */
		el = &entry->package.elements[0];
		if (el->type != ACPI_TYPE_INTEGER)
			break;
		base = el->integer.value;

		el = &entry->package.elements[1];
		if (el->type != ACPI_TYPE_INTEGER)
			break;
		size = el->integer.value;

		/* Translate CPU physical to bus address */
		if (base < CIX_HDA_DMA_CPU_START ||
		    base + size > CIX_HDA_DMA_CPU_START + CIX_HDA_DMA_SIZE) {
			dev_warn(dev, "RSVL: region 0x%llx+0x%llx outside DMA window\n",
				 (u64)base, (u64)size);
			break;
		}
		bus_addr = base - CIX_HDA_DMA_CPU_START + CIX_HDA_DMA_DEV_START;

		ret = dma_declare_coherent_memory(dev, base, bus_addr, size);
		if (ret) {
			dev_warn(dev, "RSVL: failed to declare coherent mem: %d\n", ret);
			break;
		}
		dev_info(dev, "RSVL: reserved %lluMB DMA at 0x%llx (bus 0x%llx)\n",
			 (u64)size >> 20, (u64)base, (u64)bus_addr);
		ret = 0;
		break;
	}

	kfree(output.pointer);
	return ret;
}
#else
static inline int cix_ipbloq_hda_setup_acpi_rsvmem(struct device *dev)
{
	return -ENODEV;
}
#endif

static int cix_ipbloq_hda_probe(struct platform_device *pdev)
{
	const unsigned int driver_flags = AZX_DCAPS_PM_RUNTIME;
	struct cix_ipbloq_hda *hda;
	struct snd_card *card;
	struct azx *chip;
	int err;

	hda = devm_kzalloc(&pdev->dev, sizeof(*hda), GFP_KERNEL);
	if (!hda)
		return -ENOMEM;
	hda->dev = &pdev->dev;

	if (IS_ENABLED(CONFIG_ACPI) && has_acpi_companion(&pdev->dev))
		hda->reset = devm_reset_control_get(hda->dev, "hda");
	else
		hda->reset = devm_reset_control_get(hda->dev, NULL);
	if (IS_ERR(hda->reset))
		return dev_err_probe(hda->dev, PTR_ERR(hda->reset),
				     "failed to get reset, err = %ld\n", PTR_ERR(hda->reset));

	if (IS_ENABLED(CONFIG_ACPI) && has_acpi_companion(&pdev->dev))
	{
		hda->clocks[hda->nclocks++].id = "sysclk";
		hda->clocks[hda->nclocks++].id = "clk48m";
	} else {
		hda->clocks[hda->nclocks++].id = "ipg";
		hda->clocks[hda->nclocks++].id = "per";
	}
	err = devm_clk_bulk_get(hda->dev, hda->nclocks, hda->clocks);
	if (err < 0)
		return dev_err_probe(hda->dev, err, "failed to get clk, err = %d\n", err);

	dma_set_mask_and_coherent(hda->dev, DMA_BIT_MASK(32));

	/*
	 * Under ACPI, configure DMA address translation for the HDA
	 * controller. The audio subsystem bus applies a fixed offset
	 * between CPU physical addresses and the controller's bus
	 * addresses. Under DT this is handled by dma-ranges.
	 */
	if (has_acpi_companion(&pdev->dev) && !pdev->dev.dma_range_map) {
		struct bus_dma_region *map;

		/*
		 * Use kcalloc, not devm_kcalloc: the core driver model
		 * frees dma_range_map with plain kfree() in
		 * device_unbind_cleanup(), so this must be a standalone
		 * allocation.
		 */
		map = kcalloc(2, sizeof(*map), GFP_KERNEL);
		if (!map) {
			err = -ENOMEM;
			goto out_free_device;
		}
		map[0].cpu_start = CIX_HDA_DMA_CPU_START;
		map[0].dma_start = CIX_HDA_DMA_DEV_START;
		map[0].size = CIX_HDA_DMA_SIZE;
		/* map[1] is zero-initialized sentinel */
		pdev->dev.dma_range_map = map;
	}

	if (!pdev->dev.dma_mem && pdev->dev.of_node) {
	err = of_reserved_mem_device_init(hda->dev);
	if (err < 0 && err != -ENODEV) {
		dev_err(hda->dev,
			"failed to init reserved mem for DMA, err = %d\n", err);
		return err;
	}
	}

	err = snd_card_new(hda->dev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1,
			   THIS_MODULE, 0, &card);
	if (err < 0)
		return dev_err_probe(hda->dev, err, "failed to crate card, err = %d\n", err);

	err = cix_ipbloq_hda_create(hda, card, driver_flags);
	if (err < 0)
		goto out_free_card;

	chip = &hda->chip;
	card->private_data = chip;
	dev_set_drvdata(hda->dev, card);
	if (IS_ENABLED(CONFIG_ACPI) &&
	    !pdev->dev.dma_mem && has_acpi_companion(&pdev->dev)) {
		err = cix_ipbloq_hda_setup_acpi_rsvmem(&pdev->dev);
		if (err && err != -ENODEV) {
			dev_err(&pdev->dev,
				"failed to init ACPI reserved mem for DMA, err = %d\n", err);
			goto out_free_device;
		}
	}

	/*
	 * The pdb GPIO powers/resets the codec for driver-side init. Skip it
	 * entirely when the firmware owns codec init (no cix,model): the GPIO
	 * may still be described in firmware but must not be touched by us. We
	 * check cix,model directly here rather than a flag set in
	 * cix_ipbloq_hda_init(), because the codec must be powered up before
	 * init() runs.
	 */
	if (device_property_present(&pdev->dev, "cix,model")) {
		hda->pdb_gpiod = devm_gpiod_get_optional(&pdev->dev, "pdb",
							 GPIOD_OUT_HIGH);
		if (IS_ERR(hda->pdb_gpiod)) {
			err = PTR_ERR(hda->pdb_gpiod);
			dev_err(&pdev->dev, "failed to get pdb gpio, err: %d\n", err);
			goto out_free_device;
		}

		msleep(20);
	}

	pm_runtime_enable(hda->dev);
	if (!azx_has_pm_runtime(chip))
		pm_runtime_forbid(hda->dev);

	err = pm_runtime_resume_and_get(hda->dev);
	if (err < 0) {
		dev_err(hda->dev, "runtime resume and get failed, err = %d\n", err);
		goto out_free_device;
	}

	err = cix_ipbloq_hda_init(hda, chip, pdev);
	if (err < 0)
		goto out_free_device;

	err = cix_ipbloq_hda_probe_codec(hda);
	if (err < 0)
		goto out_free_device;

	pm_runtime_put(hda->dev);

	return 0;

out_free_device:
	snd_device_free(card, chip);
out_free_card:
	snd_card_free(card);

	return err;
}

static void cix_ipbloq_hda_remove(struct platform_device *pdev)
{
	struct snd_card *card = dev_get_drvdata(&pdev->dev);
	struct azx *chip = card->private_data;

	snd_device_free(card, chip);
	snd_card_free(card);

	pm_runtime_disable(&pdev->dev);
}

static void cix_ipbloq_hda_shutdown(struct platform_device *pdev)
{
	struct snd_card *card = dev_get_drvdata(&pdev->dev);
	struct azx *chip;

	if (!card)
		return;

	chip = card->private_data;
	if (chip && chip->running)
		azx_stop_chip(chip);
}

static int cix_ipbloq_hda_suspend(struct device *dev)
{
	struct snd_card *card = dev_get_drvdata(dev);
	struct azx *chip = card->private_data;
	struct cix_ipbloq_hda *hda = container_of(chip, struct cix_ipbloq_hda, chip);
	int rc;

	rc = pm_runtime_force_suspend(dev);
	if (rc < 0)
		return rc;
	snd_power_change_state(card, SNDRV_CTL_POWER_D3cold);

	/* Only power-gate the codec on suspend when the driver owns its init
	 * (cix,model present); otherwise the firmware manages codec power.
	 */
	if (device_property_present(dev, "cix,model"))
		gpiod_set_value_cansleep(hda->pdb_gpiod, 0);

	return 0;
}

static int cix_ipbloq_hda_resume(struct device *dev)
{
	struct snd_card *card = dev_get_drvdata(dev);
	struct azx *chip = card->private_data;
	struct cix_ipbloq_hda *hda = container_of(chip, struct cix_ipbloq_hda, chip);
	int rc;

	/* Re-power the codec before resume only when the driver owns its init
	 * (cix,model present); otherwise the firmware manages codec power.
	 */
	if (device_property_present(dev, "cix,model")) {
		gpiod_set_value_cansleep(hda->pdb_gpiod, 1);
		msleep(20);
	}

	rc = pm_runtime_force_resume(dev);
	if (rc < 0)
		return rc;
	snd_power_change_state(card, SNDRV_CTL_POWER_D0);

	return 0;
}

static int cix_ipbloq_hda_runtime_suspend(struct device *dev)
{
	struct snd_card *card = dev_get_drvdata(dev);
	struct azx *chip = card->private_data;
	struct cix_ipbloq_hda *hda = container_of(chip, struct cix_ipbloq_hda, chip);

	if (chip && chip->running) {
		azx_stop_chip(chip);
		azx_enter_link_reset(chip);
	}

	clk_bulk_disable_unprepare(hda->nclocks, hda->clocks);

	return 0;
}

static int cix_ipbloq_hda_runtime_resume(struct device *dev)
{
	struct snd_card *card = dev_get_drvdata(dev);
	struct azx *chip = card->private_data;
	struct cix_ipbloq_hda *hda = container_of(chip, struct cix_ipbloq_hda, chip);
	int rc;

	rc = clk_bulk_prepare_enable(hda->nclocks, hda->clocks);
	if (rc) {
		dev_err(dev, "failed to enable clk bulk, rc: %d\n", rc);
		return rc;
	}

	rc = reset_control_assert(hda->reset);
	if (rc) {
		dev_err(dev, "failed to assert reset, rc: %d\n", rc);
		return rc;
	}

	rc = reset_control_deassert(hda->reset);
	if (rc) {
		dev_err(dev, "failed to deassert reset, rc: %d\n", rc);
		return rc;
	}

	if (chip && chip->running)
		azx_init_chip(chip, 1);

	return 0;
}

static const struct dev_pm_ops cix_ipbloq_hda_pm = {
	SYSTEM_SLEEP_PM_OPS(cix_ipbloq_hda_suspend,
			    cix_ipbloq_hda_resume)
	RUNTIME_PM_OPS(cix_ipbloq_hda_runtime_suspend,
		       cix_ipbloq_hda_runtime_resume, NULL)
};

static const struct of_device_id cix_ipbloq_hda_match[] = {
	{ .compatible = "cix,sky1-ipbloq-hda" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, cix_ipbloq_hda_match);

static const struct acpi_device_id cix_ipbloq_hda_acpi_match[] = {
	{ "CIXH6020" },
	{},
};
MODULE_DEVICE_TABLE(acpi, cix_ipbloq_hda_acpi_match);

static struct platform_driver cix_ipbloq_hda_driver = {
	.driver = {
		.name = "cix-ipbloq-hda",
		.pm = pm_ptr(&cix_ipbloq_hda_pm),
		.of_match_table = cix_ipbloq_hda_match,
		.acpi_match_table = cix_ipbloq_hda_acpi_match,
	},
	.probe = cix_ipbloq_hda_probe,
	.remove = cix_ipbloq_hda_remove,
	.shutdown = cix_ipbloq_hda_shutdown,
};
module_platform_driver(cix_ipbloq_hda_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CIX IPBLOQ HDA bus driver");
MODULE_AUTHOR("Joakim Zhang <joakim.zhang@cixtech.com>");
