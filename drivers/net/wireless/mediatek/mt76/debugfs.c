// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (C) 2016 Felix Fietkau <nbd@nbd.name>
 */
#include "mt76.h"
#include "mt76_connac.h"
#include <linux/version.h>

static int
mt76_reg_set(void *data, u64 val)
{
	struct mt76_dev *dev = data;

	__mt76_wr(dev, dev->debugfs_reg, val);
	return 0;
}

static int
mt76_reg_get(void *data, u64 *val)
{
	struct mt76_dev *dev = data;

	*val = __mt76_rr(dev, dev->debugfs_reg);
	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_regval, mt76_reg_get, mt76_reg_set,
			 "0x%08llx\n");

static int
mt76_napi_threaded_set(void *data, u64 val)
{
	struct mt76_dev *dev = data;

	if (!mt76_is_mmio(dev))
		return -EOPNOTSUPP;

	if (dev->napi_dev->threaded != val)
		return dev_set_threaded(dev->napi_dev, val);

	return 0;
}

static int
mt76_napi_threaded_get(void *data, u64 *val)
{
	struct mt76_dev *dev = data;

	*val = dev->napi_dev->threaded;
	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_napi_threaded, mt76_napi_threaded_get,
			 mt76_napi_threaded_set, "%llu\n");

static const char *TXQ_NAMES[__MT_TXQ_MAX] = {
	[MT_TXQ_VO]     = "VO",
	[MT_TXQ_VI]     = "VI",
	[MT_TXQ_BE]     = "BE",
	[MT_TXQ_BK]     = "BK",
	[MT_TXQ_PSD]    = "PSD",
	[MT_TXQ_BEACON] = "BEACON",
	[MT_TXQ_CAB]    = "CAB",
};

static char *MCUQ_NAMES[__MT_MCUQ_MAX] = {
	[MT_MCUQ_WM]   = "WM",
	[MT_MCUQ_WA]   = "WA",
	[MT_MCUQ_FWDL] = "FWDL",
};

static int
mt76_tx_queues_show(struct seq_file *file, void *data)
{
	struct mt76_phy *phy = file->private;
	int i;

	seq_puts(file, "     queue | hw-queued |      head |      tail |\n");
	for (i = 0; i < ARRAY_SIZE(phy->q_tx); i++) {
		struct mt76_queue *q = phy->q_tx[i];
		const char *name = TXQ_NAMES[i] ?: "";

		if (!q)
			continue;

		seq_printf(file, "%2d %7s | %9d | %9d | %9d |\n",
			   i, name, q->queued, q->head, q->tail);
	}

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(mt76_tx_queues);

static int
mt76_mcu_queues_show(struct seq_file *file, void *data)
{
	struct mt76_dev *dev = file->private;
	int i;

	seq_puts(file, "     queue | hw-queued |      head |      tail |\n");
	for (i = 0; i < ARRAY_SIZE(dev->q_mcu); i++) {
		struct mt76_queue *q = dev->q_mcu[i];
		const char *name = MCUQ_NAMES[i] ?: "";

		if (!q)
			continue;

		seq_printf(file, "%2d %7s | %9d | %9d | %9d |\n",
			   i, name, q->queued, q->head, q->tail);
	}

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(mt76_mcu_queues);

void mt76_debugfs_add_tx_queues(struct mt76_phy *phy, struct dentry *dir)
{
	debugfs_create_file("tx-queues", 0400, dir, phy, &mt76_tx_queues_fops);
}
EXPORT_SYMBOL_GPL(mt76_debugfs_add_tx_queues);

void mt76_debugfs_add_mcu_queues(struct mt76_dev *dev, struct dentry *dir)
{
	debugfs_create_file("mcu-queues", 0400, dir, dev, &mt76_mcu_queues_fops);
}
EXPORT_SYMBOL_GPL(mt76_debugfs_add_mcu_queues);

int mt76_queues_read(struct seq_file *s, void *data)
{
	struct mt76_dev *dev = dev_get_drvdata(s->private);
	int i;

	seq_puts(s, "     queue | hw-queued |      head |      tail |\n");
	for (i = 0; i < ARRAY_SIZE(dev->phy.q_tx); i++) {
		struct mt76_queue *q = dev->phy.q_tx[i];

		if (!q)
			continue;

		seq_printf(s, " %9d | %9d | %9d | %9d |\n",
			   i, q->queued, q->head, q->tail);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mt76_queues_read);

static int mt76_rx_queues_read(struct seq_file *s, void *data)
{
	struct mt76_dev *dev = dev_get_drvdata(s->private);
	int i, queued;

	seq_puts(s, "     queue | hw-queued |      head |      tail |\n");
	mt76_for_each_q_rx(dev, i) {
		struct mt76_queue *q = &dev->q_rx[i];

		queued = mt76_is_usb(dev) ? q->ndesc - q->queued : q->queued;
		seq_printf(s, " %9d | %9d | %9d | %9d |\n",
			   i, queued, q->head, q->tail);
	}

	return 0;
}

static int
mt7915_txs_for_no_skb_set(void *data, u64 val)
{
	struct mt76_dev *dev = data;

	dev->txs_for_no_skb_enabled = !!val;

	return 0;
}

static int
mt7915_txs_for_no_skb_get(void *data, u64 *val)
{
	struct mt76_dev *dev = data;

	*val = dev->txs_for_no_skb_enabled;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_txs_for_no_skb, mt7915_txs_for_no_skb_get,
			 mt7915_txs_for_no_skb_set, "%lld\n");

static int
mt76_txs_for_all_set(void *data, u64 val)
{
	struct mt76_dev *dev = data;

	dev->txs_for_all_enabled = !!val;

	return 0;
}

static int
mt76_txs_for_all_get(void *data, u64 *val)
{
	struct mt76_dev *dev = data;

	*val = dev->txs_for_all_enabled;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_txs_for_all, mt76_txs_for_all_get,
			 mt76_txs_for_all_set, "%lld\n");

static int
mt76_debug_lvl_set(void *data, u64 val)
{
	struct mt76_dev *dev = data;

	*dev->debug_lvl = val;

	return 0;
}

static int
mt76_debug_lvl_get(void *data, u64 *val)
{
	struct mt76_dev *dev = data;

	*val = dev->debug_lvl ? *dev->debug_lvl : 0;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_debug_lvl, mt76_debug_lvl_get,
			 mt76_debug_lvl_set, "%lld\n");

static int
mt76_version(struct seq_file *s, void *data)
{
	struct mt76_dev *dev = dev_get_drvdata(s->private);

	seq_printf(s, "chipset:       ");
	if (is_mt7922(dev))
		seq_printf(s, "7922\n");
	else if (is_mt7921k(dev))
		seq_printf(s, "7921k\n");
	/* Add specific connac2 radios here */
	else if (is_connac2(dev))
		seq_printf(s, "7921\n");
	else if (is_mt7925(dev))
		seq_printf(s, "7925\n");
	else if (is_mt7663(dev))
		seq_printf(s, "7663\n");
	else if (is_mt7915(dev))
		seq_printf(s, "7915\n");
	else if (is_mt7916(dev))
		seq_printf(s, "7916\n");
	else if (is_mt7986(dev))
		seq_printf(s, "7986\n");
	else if (is_mt7996(dev))
		seq_printf(s, "7996\n");
	else if (is_mt7622(dev))
		seq_printf(s, "7622\n");
	else if (is_mt7615(dev))
		seq_printf(s, "7615\n");
	else if (is_mt7996(dev))
		seq_printf(s, "7996\n");
	else if (is_mt7611(dev))
		seq_printf(s, "7611\n");
	else
		seq_printf(s, "UNKNOWN\n");

	seq_printf(s, "ASIC-Revision: 0x%x\n", dev->rev);
	seq_printf(s, "hw_sw_ver:     0x%x\n", dev->fw.hw_sw_ver);
	seq_printf(s, "build_date:    %s\n", dev->fw.build_date);
	seq_printf(s, "bus:           %s\n", dev_name(dev->dev));
	seq_printf(s, "fwcfg:         fwcfg-%s-%s.txt\n", mt76_bus_str(dev->bus->type), dev_name(dev->dev));
	seq_printf(s, "WM-hw_sw_ver:  %s\n", dev->fw.wm_fw_ver);
	seq_printf(s, "WM-build_date: %s\n", dev->fw.wm_build_date);
	seq_printf(s, "WA-hw_sw_ver:  %s\n", dev->fw.wa_fw_ver);
	seq_printf(s, "WA-build_date: %s\n", dev->fw.wa_build_date);

	return 0;
}

static int mt76_dfs_info_read(struct seq_file *s, void *data)
{
	struct mt76_dev *dev = dev_get_drvdata(s->private);
	int i;

	seq_puts(s, "     index |  ctrl/cmd |    rx-sel |       val |        rv\n");
	for (i = 0; i<ARRAY_SIZE(dev->debug_mcu_rdd_cmd_rv); i++) {
		seq_printf(s, " %9d | %9d | %9d | %9d | %09d |\n",
			   i, dev->debug_mcu_rdd_cmd[i].ctrl,
			   dev->debug_mcu_rdd_cmd[i].rdd_rx_sel,
			   dev->debug_mcu_rdd_cmd[i].val,
			   dev->debug_mcu_rdd_cmd_rv[i]
			);
	}

	return 0;
}

void mt76_seq_puts_array(struct seq_file *file, const char *str,
			 s8 *val, int len)
{
	int i;

	seq_printf(file, "%16s:", str);
	for (i = 0; i < len; i++)
		seq_printf(file, " %4d", val[i]);
	seq_puts(file, "\n");
}
EXPORT_SYMBOL_GPL(mt76_seq_puts_array);

struct dentry *
mt76_register_debugfs_fops(struct mt76_phy *phy,
			   const struct file_operations *ops)
{
	const struct file_operations *fops = ops ? ops : &fops_regval;
	struct mt76_dev *dev = phy->dev;
	struct dentry *dir;

	dir = debugfs_create_dir("mt76", phy->hw->wiphy->debugfsdir);

	debugfs_create_u8("block_traffic", 0600, dir, &dev->block_traffic);
	debugfs_create_u8("led_pin", 0600, dir, &phy->leds.pin);
	debugfs_create_u32("regidx", 0600, dir, &dev->debugfs_reg);
	debugfs_create_u32("stale_skb_status_check", 0600, dir, &dev->stale_skb_status_check);
	debugfs_create_u32("stale_skb_status_timeout", 0600, dir, &dev->stale_skb_status_timeout);
	debugfs_create_file_unsafe("regval", 0600, dir, dev, fops);
	debugfs_create_file_unsafe("napi_threaded", 0600, dir, dev,
				   &fops_napi_threaded);
	debugfs_create_file("force_txs", 0600, dir, dev, &fops_txs_for_no_skb);
	debugfs_create_file("force_txs_all_skb", 0600, dir, dev, &fops_txs_for_all);
	debugfs_create_file("debug_lvl", 0600, dir, dev, &fops_debug_lvl);
	debugfs_create_blob("eeprom", 0400, dir, &dev->eeprom);
	debugfs_create_devm_seqfile(dev->dev, "version", dir, mt76_version);
	if (dev->otp.data)
		debugfs_create_blob("otp", 0400, dir, &dev->otp);
	debugfs_create_devm_seqfile(dev->dev, "rx-queues", dir,
				    mt76_rx_queues_read);
	debugfs_create_devm_seqfile(dev->dev, "dfs-info", dir,
				    mt76_dfs_info_read);

	return dir;
}
EXPORT_SYMBOL_GPL(mt76_register_debugfs_fops);
