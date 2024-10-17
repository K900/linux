// SPDX-License-Identifier: BSD-3-Clause-Clear
/* Copyright (C) 2020 MediaTek Inc. */

#include "mt7921.h"

extern bool mt7921_disable_pm;
extern bool mt7921_disable_deep_sleep;

static int
mt7921_reg_set(void *data, u64 val)
{
	struct mt792x_dev *dev = data;

	mt792x_mutex_acquire(dev);
	mt76_wr(dev, dev->mt76.debugfs_reg, val);
	mt792x_mutex_release(dev);

	return 0;
}

static int
mt7921_reg_get(void *data, u64 *val)
{
	struct mt792x_dev *dev = data;

	mt792x_mutex_acquire(dev);
	*val = mt76_rr(dev, dev->mt76.debugfs_reg);
	mt792x_mutex_release(dev);

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_regval, mt7921_reg_get, mt7921_reg_set,
			 "0x%08llx\n");
static int
mt7921_fw_debug_set(void *data, u64 val)
{
	struct mt792x_dev *dev = data;

	mt792x_mutex_acquire(dev);

	dev->fw_debug = (u8)val;
	mt7921_mcu_fw_log_2_host(dev, dev->fw_debug);

	mt792x_mutex_release(dev);

	return 0;
}

static int
mt7921_fw_debug_get(void *data, u64 *val)
{
	struct mt792x_dev *dev = data;

	*val = dev->fw_debug;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_fw_debug, mt7921_fw_debug_get,
			 mt7921_fw_debug_set, "%lld\n");

DEFINE_SHOW_ATTRIBUTE(mt792x_tx_stats);

static void
mt7921_seq_puts_array(struct seq_file *file, const char *str,
		      s8 *val, int len)
{
	int i;

	seq_printf(file, "%-16s:", str);
	for (i = 0; i < len; i++)
		if (val[i] == 127)
			seq_printf(file, " %6s", "N.A");
		else
			seq_printf(file, " %6d", val[i]);
	seq_puts(file, "\n");
}

#define mt7921_print_txpwr_entry(prefix, rate)				\
({									\
	mt7921_seq_puts_array(s, #prefix " (user)",			\
			      txpwr.data[TXPWR_USER].rate,		\
			      ARRAY_SIZE(txpwr.data[TXPWR_USER].rate)); \
	mt7921_seq_puts_array(s, #prefix " (eeprom)",			\
			      txpwr.data[TXPWR_EEPROM].rate,		\
			      ARRAY_SIZE(txpwr.data[TXPWR_EEPROM].rate)); \
	mt7921_seq_puts_array(s, #prefix " (tmac)",			\
			      txpwr.data[TXPWR_MAC].rate,		\
			      ARRAY_SIZE(txpwr.data[TXPWR_MAC].rate));	\
})

static int
mt7921_txpwr(struct seq_file *s, void *data)
{
	struct mt792x_dev *dev = dev_get_drvdata(s->private);
	struct mt7921_txpwr txpwr;
	int ret;

	mt792x_mutex_acquire(dev);
	ret = mt7921_get_txpwr_info(dev, &txpwr);
	mt792x_mutex_release(dev);

	if (ret)
		return ret;

	seq_printf(s, "Tx power table (channel %d)\n", txpwr.ch);
	seq_printf(s, "%-16s  %6s %6s %6s %6s\n",
		   " ", "1m", "2m", "5m", "11m");
	mt7921_print_txpwr_entry(CCK, cck);

	seq_printf(s, "%-16s  %6s %6s %6s %6s %6s %6s %6s %6s\n",
		   " ", "6m", "9m", "12m", "18m", "24m", "36m",
		   "48m", "54m");
	mt7921_print_txpwr_entry(OFDM, ofdm);

	seq_printf(s, "%-16s  %6s %6s %6s %6s %6s %6s %6s %6s\n",
		   " ", "mcs0", "mcs1", "mcs2", "mcs3", "mcs4", "mcs5",
		   "mcs6", "mcs7");
	mt7921_print_txpwr_entry(HT20, ht20);

	seq_printf(s, "%-16s  %6s %6s %6s %6s %6s %6s %6s %6s %6s\n",
		   " ", "mcs0", "mcs1", "mcs2", "mcs3", "mcs4", "mcs5",
		   "mcs6", "mcs7", "mcs32");
	mt7921_print_txpwr_entry(HT40, ht40);

	seq_printf(s, "%-16s  %6s %6s %6s %6s %6s %6s %6s %6s %6s %6s %6s %6s\n",
		   " ", "mcs0", "mcs1", "mcs2", "mcs3", "mcs4", "mcs5",
		   "mcs6", "mcs7", "mcs8", "mcs9", "mcs10", "mcs11");
	mt7921_print_txpwr_entry(VHT20, vht20);
	mt7921_print_txpwr_entry(VHT40, vht40);
	mt7921_print_txpwr_entry(VHT80, vht80);
	mt7921_print_txpwr_entry(VHT160, vht160);
	mt7921_print_txpwr_entry(HE26, he26);
	mt7921_print_txpwr_entry(HE52, he52);
	mt7921_print_txpwr_entry(HE106, he106);
	mt7921_print_txpwr_entry(HE242, he242);
	mt7921_print_txpwr_entry(HE484, he484);
	mt7921_print_txpwr_entry(HE996, he996);
	mt7921_print_txpwr_entry(HE996x2, he996x2);

	return 0;
}

static int
mt7921_pm_set(void *data, u64 val)
{
	struct mt792x_dev *dev = data;
	struct mt76_connac_pm *pm = &dev->pm;

	if (mt76_is_usb(&dev->mt76))
		return -EOPNOTSUPP;

	mutex_lock(&dev->mt76.mutex);

	if (val == pm->enable_user && val == pm->enable)
		goto out;

	if (!pm->enable_user) {
		pm->stats.last_wake_event = jiffies;
		pm->stats.last_doze_event = jiffies;
	}
	/* make sure the chip is awake here and ps_work is scheduled
	 * just at end of the this routine.
	 */
	pm->enable = false;
	mt76_connac_pm_wake(&dev->mphy, pm);

	pm->enable_user = val;
	mt7921_set_runtime_pm(dev);
	mt76_connac_power_save_sched(&dev->mphy, pm);
out:
	mutex_unlock(&dev->mt76.mutex);

	return 0;
}

static int
mt7921_pm_get(void *data, u64 *val)
{
	struct mt792x_dev *dev = data;

	*val = dev->pm.enable_user;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_pm, mt7921_pm_get, mt7921_pm_set, "%lld\n");

static int
mt7921_deep_sleep_set(void *data, u64 val)
{
	struct mt792x_dev *dev = data;
	struct mt76_connac_pm *pm = &dev->pm;
	bool monitor = !!(dev->mphy.hw->conf.flags & IEEE80211_CONF_MONITOR);
	bool enable = !!val;

	if (mt76_is_usb(&dev->mt76))
		return -EOPNOTSUPP;

	mt792x_mutex_acquire(dev);
	if (pm->ds_enable_user == enable && pm->ds_enable == enable)
		goto out;

	pm->ds_enable_user = enable;
	pm->ds_enable = enable && !monitor && !mt7921_disable_deep_sleep;
	mt76_connac_mcu_set_deep_sleep(&dev->mt76, pm->ds_enable);
out:
	mt792x_mutex_release(dev);

	return 0;
}

static int
mt7921_deep_sleep_get(void *data, u64 *val)
{
	struct mt792x_dev *dev = data;

	*val = dev->pm.ds_enable_user;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_ds, mt7921_deep_sleep_get,
			 mt7921_deep_sleep_set, "%lld\n");

DEFINE_DEBUGFS_ATTRIBUTE(fops_pm_idle_timeout, mt792x_pm_idle_timeout_get,
			 mt792x_pm_idle_timeout_set, "%lld\n");

static int mt7921_chip_reset(void *data, u64 val)
{
	struct mt792x_dev *dev = data;
	int ret = 0;

	switch (val) {
	case 1:
		/* Reset wifisys directly. */
		mt792x_reset(&dev->mt76);
		break;
	default:
		/* Collect the core dump before reset wifisys. */
		mt792x_mutex_acquire(dev);
		ret = mt76_connac_mcu_chip_config(&dev->mt76);
		mt792x_mutex_release(dev);
		break;
	}

	return ret;
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_reset, NULL, mt7921_chip_reset, "%lld\n");

static int
mt7921s_sched_quota_read(struct seq_file *s, void *data)
{
	struct mt792x_dev *dev = dev_get_drvdata(s->private);
	struct mt76_sdio *sdio = &dev->mt76.sdio;

	seq_printf(s, "pse_data_quota\t%d\n", sdio->sched.pse_data_quota);
	seq_printf(s, "ple_data_quota\t%d\n", sdio->sched.ple_data_quota);
	seq_printf(s, "pse_mcu_quota\t%d\n", sdio->sched.pse_mcu_quota);
	seq_printf(s, "sched_deficit\t%d\n", sdio->sched.deficit);

	return 0;
}

struct mt7921_txo_worker_info {
	char* buf;
	int sofar;
	int size;
};

static void mt7921_txo_worker(void *wi_data, struct ieee80211_sta *sta)
{
	struct mt7921_txo_worker_info *wi = wi_data;
	struct mt792x_sta *msta = (struct mt792x_sta *)sta->drv_priv;
	struct mt76_testmode_data *td = &msta->deflink.test;
	struct ieee80211_vif *vif;
	struct wireless_dev *wdev;

	if (wi->sofar >= wi->size)
		return; /* buffer is full */

	vif = container_of((void *)msta->vif, struct ieee80211_vif, drv_priv);
	wdev = ieee80211_vif_to_wdev(vif);

	wi->sofar += scnprintf(wi->buf + wi->sofar, wi->size - wi->sofar,
			       "vdev (%s) active=%d tpc=%d sgi=%d mcs=%d nss=%d"
			       " pream=%d retries=%d dynbw=%d bw=%d stbc=%d ldpc=%d\n",
			       wdev->netdev->name,
			       td->txo_active, td->tx_power[0],
			       td->tx_rate_sgi, td->tx_rate_idx,
			       td->tx_rate_nss, td->tx_rate_mode,
			       td->tx_xmit_count, td->tx_dynbw,
			       td->txbw, td->tx_rate_stbc, td->tx_rate_ldpc);
}

static ssize_t mt7921_read_set_rate_override(struct file *file,
					     char __user *user_buf,
					     size_t count, loff_t *ppos)
{
	struct mt792x_dev *dev = file->private_data;
        struct ieee80211_hw *hw = dev->mphy.hw;
	char *buf2;
	int size = 8000;
	int rv, sofar;
	struct mt7921_txo_worker_info wi;
	const char buf[] =
		"This allows specify specif tx rate parameters for all DATA"
		" frames on a vdev\n"
		"To set a value, you specify the dev-name and key-value pairs:\n"
		"tpc=10 sgi=1 mcs=x nss=x pream=x retries=x dynbw=0|1 bw=x enable=0|1\n"
		"pream: 0=cck, 1=ofdm, 2=HT, 3=VHT, 4=HE_SU\n"
		"cck-mcs: 0=1Mbps, 1=2Mbps, 3=5.5Mbps, 3=11Mbps\n"
		"ofdm-mcs: 0=6Mbps, 1=9Mbps, 2=12Mbps, 3=18Mbps, 4=24Mbps, 5=36Mbps,"
		" 6=48Mbps, 7=54Mbps\n"
		"sgi: HT/VHT: 0 | 1, HE 0: 1xLTF+0.8us, 1: 2xLTF+0.8us, 2: 2xLTF+1.6us, 3: 4xLTF+3.2us, 4: 4xLTF+0.8us\n"
		"tpc: adjust power from defaults, in 1/2 db units 0 - 31, 16 is default\n"
		"bw is 0-3 for 20-160\n"
		"stbc: 0 off, 1 on\n"
		"ldpc: 0 off, 1 on\n"
		" For example, wlan0:\n"
		"echo \"wlan0 tpc=255 sgi=1 mcs=0 nss=1 pream=3 retries=1 dynbw=0 bw=0"
		" active=1\" > ...mt76/set_rate_override\n";

	buf2 = kzalloc(size, GFP_KERNEL);
	if (!buf2)
		return -ENOMEM;
	strcpy(buf2, buf);
	sofar = strlen(buf2);

	wi.sofar = sofar;
	wi.buf = buf2;
	wi.size = size;

	ieee80211_iterate_stations_atomic(hw, mt7921_txo_worker, &wi);

	rv = simple_read_from_buffer(user_buf, count, ppos, buf2, wi.sofar);
	kfree(buf2);
	return rv;
}

/* Set the rates for specific types of traffic.
 */
static ssize_t mt7921_write_set_rate_override(struct file *file,
					      const char __user *user_buf,
					      size_t count, loff_t *ppos)
{
	struct mt792x_dev *dev = file->private_data;
	struct mt792x_link_sta *link_sta;
	struct mt792x_sta *msta;
	struct ieee80211_vif *vif;
	struct mt76_testmode_data *td = NULL;
	struct wireless_dev *wdev;
	struct mt76_wcid *wcid;
	struct mt76_phy *mphy = &dev->mt76.phy;
	char buf[180];
	char tmp[20];
	char *tok;
	int ret, i, j;
	unsigned int vdev_id = 0xFFFF;
	char *bufptr = buf;
	long rc;
	char dev_name_match[IFNAMSIZ + 2];

	memset(buf, 0, sizeof(buf));

	simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);

	/* make sure that buf is null terminated */
	buf[sizeof(buf) - 1] = 0;

#define MT7921_PARSE_LTOK(a, b)						\
	do {								\
		tok = strstr(bufptr, " " #a "=");			\
		if (tok) {						\
			char *tspace;					\
			tok += 1; /* move past initial space */		\
			strscpy(tmp, tok + strlen(#a "="), sizeof(tmp) - 1); \
			tmp[sizeof(tmp) - 1] = 0;			\
			tspace = strstr(tmp, " ");			\
			if (tspace)					\
				*tspace = 0;				\
			if (kstrtol(tmp, 0, &rc) != 0)			\
				dev_info(dev->mt76.dev,			\
					 "mt7921: set-rate-override: " #a \
					 "= could not be parsed, tmp: %s\n", \
					 tmp);				\
			else						\
				td->b = rc;				\
		}							\
	} while (0)

	/* drop the possible '\n' from the end */
	if (buf[count - 1] == '\n')
		buf[count - 1] = 0;

	mutex_lock(&mphy->dev->mutex);

	/* Ignore empty lines, 'echo' appends them sometimes at least. */
	if (buf[0] == 0) {
		ret = count;
		goto exit;
	}

	/* String starts with vdev name, ie 'wlan0'  Find the proper vif that
	 * matches the name.
	 */
	for (i = 0; i < ARRAY_SIZE(dev->mt76.wcid_mask); i++) {
		u32 mask = dev->mt76.wcid_mask[i];

		if (!mask)
			continue;

		for (j = i * 32; mask; j++, mask >>= 1) {
			if (!(mask & 1))
				continue;

			rcu_read_lock();
			wcid = rcu_dereference(dev->mt76.wcid[j]);
			if (!wcid) {
				rcu_read_unlock();
				continue;
			}

			link_sta = container_of(wcid, struct mt792x_link_sta, wcid);
			msta = link_sta->sta;

			if (!msta) {
				rcu_read_unlock();
				continue;
			}

			// TOOD:  Support setting specific link, not just first one we fine.
			if (!msta->vif) {
				rcu_read_unlock();
				continue;
			}

			vif = container_of((void *)msta->vif, struct ieee80211_vif, drv_priv);

			wdev = ieee80211_vif_to_wdev(vif);

			if (!wdev || !wdev->netdev) {
				rcu_read_unlock();
				continue;
			}

			snprintf(dev_name_match, sizeof(dev_name_match) - 1, "%s ",
				 wdev->netdev->name);

			if (strncmp(dev_name_match, buf, strlen(dev_name_match)) == 0) {
				vdev_id = j;
				td = &link_sta->test;
				bufptr = buf + strlen(dev_name_match) - 1;

				MT7921_PARSE_LTOK(tpc, tx_power[0]);
				MT7921_PARSE_LTOK(sgi, tx_rate_sgi);
				MT7921_PARSE_LTOK(mcs, tx_rate_idx);
				MT7921_PARSE_LTOK(nss, tx_rate_nss);
				MT7921_PARSE_LTOK(pream, tx_rate_mode);
				MT7921_PARSE_LTOK(retries, tx_xmit_count);
				MT7921_PARSE_LTOK(dynbw, tx_dynbw);
				MT7921_PARSE_LTOK(ldpc, tx_rate_ldpc);
				MT7921_PARSE_LTOK(stbc, tx_rate_stbc);
				MT7921_PARSE_LTOK(bw, txbw);
				MT7921_PARSE_LTOK(active, txo_active);

				/* To match Intel's API
				 * HE 0: 1xLTF+0.8us, 1: 2xLTF+0.8us, 2: 2xLTF+1.6us, 3: 4xLTF+3.2us, 4: 4xLTF+0.8us
				 */
				if (td->tx_rate_mode >= 4) {
					if (td->tx_rate_sgi == 0) {
						td->tx_rate_sgi = 0;
						td->tx_ltf = 0;
					} else if (td->tx_rate_sgi == 1) {
						td->tx_rate_sgi = 0;
						td->tx_ltf = 1;
					} else if (td->tx_rate_sgi == 2) {
						td->tx_rate_sgi = 1;
						td->tx_ltf = 1;
					} else if (td->tx_rate_sgi == 3) {
						td->tx_rate_sgi = 2;
						td->tx_ltf = 2;
					}
					else {
						td->tx_rate_sgi = 0;
						td->tx_ltf = 2;
					}
				}
				//td->tx_ltf = 1; /* 0: HTLTF 3.2us, 1: HELTF, 6.4us, 2 HELTF 12,8us */

				dev_info(dev->mt76.dev,
					 "mt7921: set-rate-overrides, vdev %i(%s) active=%d tpc=%d sgi=%d ltf=%d mcs=%d"
					 " nss=%d pream=%d retries=%d dynbw=%d bw=%d ldpc=%d stbc=%d\n",
					 vdev_id, dev_name_match,
					 td->txo_active, td->tx_power[0], td->tx_rate_sgi, td->tx_ltf, td->tx_rate_idx,
					 td->tx_rate_nss, td->tx_rate_mode, td->tx_xmit_count, td->tx_dynbw,
					 td->txbw, td->tx_rate_ldpc, td->tx_rate_stbc);

			}

			rcu_read_unlock();
		}
	}

	if (vdev_id == 0xFFFF) {
		if (strstr(buf, "active=0")) {
			/* Ignore, we are disabling it anyway */
			ret = count;
			goto exit;
		} else {
			dev_info(dev->mt76.dev,
				 "mt7921: set-rate-override, unknown netdev name: %s\n", buf);
		}
		ret = -EINVAL;
		goto exit;
	}

	ret = count;

exit:
	mutex_unlock(&mphy->dev->mutex);
	return ret;
}

static const struct file_operations fops_set_rate_override = {
	.read = mt7921_read_set_rate_override,
	.write = mt7921_write_set_rate_override,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

int mt7921_init_debugfs(struct mt792x_dev *dev)
{
	struct dentry *dir;

	dir = mt76_register_debugfs_fops(&dev->mphy, &fops_regval);

	if (mt76_is_mmio(&dev->mt76))
		debugfs_create_devm_seqfile(dev->mt76.dev, "xmit-queues",
					    dir, mt792x_queues_read);
	else
		debugfs_create_devm_seqfile(dev->mt76.dev, "xmit-queues",
					    dir, mt76_queues_read);

	debugfs_create_devm_seqfile(dev->mt76.dev, "acq", dir,
				    mt792x_queues_acq);
	debugfs_create_devm_seqfile(dev->mt76.dev, "txpower_sku", dir,
				    mt7921_txpwr);
	debugfs_create_file("tx_stats", 0400, dir, dev, &mt792x_tx_stats_fops);
	debugfs_create_file("fw_debug", 0600, dir, dev, &fops_fw_debug);
	debugfs_create_file("runtime-pm", 0600, dir, dev, &fops_pm);
	debugfs_create_file("idle-timeout", 0600, dir, dev,
			    &fops_pm_idle_timeout);
	debugfs_create_file("chip_reset", 0600, dir, dev, &fops_reset);
	debugfs_create_devm_seqfile(dev->mt76.dev, "runtime_pm_stats", dir,
				    mt792x_pm_stats);
	debugfs_create_file("deep-sleep", 0600, dir, dev, &fops_ds);
	if (mt76_is_sdio(&dev->mt76))
		debugfs_create_devm_seqfile(dev->mt76.dev, "sched-quota", dir,
					    mt7921s_sched_quota_read);
	debugfs_create_file("set_rate_override", 0600, dir,
			    dev, &fops_set_rate_override);
	/* TODO:  Add rate_txpower_show */
	return 0;
}
