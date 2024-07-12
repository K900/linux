// SPDX-License-Identifier: GPL-2.0-only
/*
 * mac80211 ethtool hooks for cfg80211
 *
 * Copied from cfg.c - originally
 * Copyright 2006-2010	Johannes Berg <johannes@sipsolutions.net>
 * Copyright 2014	Intel Corporation (Author: Johannes Berg)
 * Copyright (C) 2018, 2022-2023 Intel Corporation
 */
#include <linux/types.h>
#include <net/cfg80211.h>
#include "ieee80211_i.h"
#include "sta_info.h"
#include "driver-ops.h"
#include <asm/div64.h>

static inline __s64 mac_div(__s64 n, __u32 base)
{
	if (n < 0) {
		__u64 tmp = -n;
		do_div(tmp, base);
		/* printk("pktgen: pg_div, n: %llu  base: %d  rv: %llu\n",
		   n, base, tmp); */
		return -tmp;
	}
	else {
		__u64 tmp = n;
		do_div(tmp, base);
		/* printk("pktgen: pg_div, n: %llu  base: %d  rv: %llu\n",
		   n, base, tmp); */
		return tmp;
	}
}

static int ieee80211_set_ringparam(struct net_device *dev,
				   struct ethtool_ringparam *rp,
				   struct kernel_ethtool_ringparam *kernel_rp,
				   struct netlink_ext_ack *extack)
{
	struct ieee80211_local *local = wiphy_priv(dev->ieee80211_ptr->wiphy);

	if (rp->rx_mini_pending != 0 || rp->rx_jumbo_pending != 0)
		return -EINVAL;

	guard(wiphy)(local->hw.wiphy);

	return drv_set_ringparam(local, rp->tx_pending, rp->rx_pending);
}

static void ieee80211_get_ringparam(struct net_device *dev,
				    struct ethtool_ringparam *rp,
				    struct kernel_ethtool_ringparam *kernel_rp,
				    struct netlink_ext_ack *extack)
{
	struct ieee80211_local *local = wiphy_priv(dev->ieee80211_ptr->wiphy);

	memset(rp, 0, sizeof(*rp));

	guard(wiphy)(local->hw.wiphy);

	drv_get_ringparam(local, &rp->tx_pending, &rp->tx_max_pending,
			  &rp->rx_pending, &rp->rx_max_pending);
}

static const char ieee80211_gstrings_sta_stats[][ETH_GSTRING_LEN] = {
	/* per sdata stats len */
	"bss_color",
	"valid_links",
	"active_links",
	"dormant_links",

	/* Link 0 stats */
	"rx_packets",
	"rx_bytes",
	"rx_duplicates",
	"rx_fragments",
	"rx_dropped",
	"tx_packets",
	"tx_bytes",
	"tx_filtered",
	"tx_retry_failed",
	"tx_retries",
	"tx_handlers_drop",
	"sta_state",
	"txrate",
	"rxrate",
	"signal",
	"signal_beacon",
	"signal_chains",
	"signal_chains_avg",
	/* Add new stats here, channel and others go below */
	"channel",
	"noise",
	"ch_time",
	"ch_time_busy",
	"ch_time_ext_busy",
	"ch_time_rx",
	"ch_time_tx",

	/* Link 1 stats */
	"L1:rx_packets",
	"L1:rx_bytes",
	"L1:rx_duplicates",
	"L1:rx_fragments",
	"L1:rx_dropped",
	"L1:tx_packets",
	"L1:tx_bytes",
	"L1:tx_filtered",
	"L1:tx_retry_failed",
	"L1:tx_retries",
	"L1:tx_handlers_drop",
	"L1:sta_state",
	"L1:txrate",
	"L1:rxrate",
	"L1:signal",
	"L1:signal_beacon",
	"L1:signal_chains",
	"L1:signal_chains_avg",
	/* Add new stats here, channel and others go below */
	"L1:channel",
	"L1:noise",
	"L1:ch_time",
	"L1:ch_time_busy",
	"L1:ch_time_ext_busy",
	"L1:ch_time_rx",
	"L1:ch_time_tx",

	/* Link 2 stats */
	"L2:rx_packets",
	"L2:rx_bytes",
	"L2:rx_duplicates",
	"L2:rx_fragments",
	"L2:rx_dropped",
	"L2:tx_packets",
	"L2:tx_bytes",
	"L2:tx_filtered",
	"L2:tx_retry_failed",
	"L2:tx_retries",
	"L2:tx_handlers_drop",
	"L2:sta_state",
	"L2:txrate",
	"L2:rxrate",
	"L2:signal",
	"L2:signal_beacon",
	"L2:signal_chains",
	"L2:signal_chains_avg",
	/* Add new stats here, channel and others go below */
	"L2:channel",
	"L2:noise",
	"L2:ch_time",
	"L2:ch_time_busy",
	"L2:ch_time_ext_busy",
	"L2:ch_time_rx",
	"L2:ch_time_tx",
};
#define STA_STATS_LEN	ARRAY_SIZE(ieee80211_gstrings_sta_stats)
#define SDATA_STATS_LEN 4 /* bss color, active_links ... */
#define ETHTOOL_LINK_COUNT 3 /* we will show stats for first 3 links */
#define PER_LINK_STATS_LEN ((STA_STATS_LEN - SDATA_STATS_LEN) / ETHTOOL_LINK_COUNT)

static int ieee80211_get_sset_count(struct net_device *dev, int sset)
{
	struct ieee80211_sub_if_data *sdata = IEEE80211_DEV_TO_SUB_IF(dev);
	int rv = 0;

	if (sset == ETH_SS_STATS)
		rv += STA_STATS_LEN;

	rv += drv_get_et_sset_count(sdata, sset);

	if (rv == 0)
		return -EOPNOTSUPP;
	return rv;
}

static void ieee80211_get_stats2(struct net_device *dev,
				 struct ethtool_stats *stats,
				 u64 *data, u32 level)
{
	struct ieee80211_sub_if_data *sdata = IEEE80211_DEV_TO_SUB_IF(dev);
	struct ieee80211_chanctx_conf *chanctx_conf;
	struct ieee80211_channel *channel;
	struct sta_info *sta;
	struct ieee80211_local *local = sdata->local;
	struct station_info sinfo;
	struct survey_info survey;
	struct ieee80211_link_data *link = NULL;
	int i = 0, q, start_link_i;
	int z;
#define STA_STATS_SURVEY_LEN 7

	memset(data, 0, sizeof(u64) * STA_STATS_LEN);

#define ADD_SURVEY_STATS						\
	do {								\
		/* Get survey stats for current channel */		\
		survey.filled = 0;					\
									\
		rcu_read_lock();					\
		chanctx_conf = rcu_dereference(sdata->vif.bss_conf.chanctx_conf); \
		if (link)						\
			channel = link->conf->chanreq.oper.chan;	\
		else if (chanctx_conf)					\
			channel = chanctx_conf->def.chan;		\
		else if (local->open_count > 0 &&			\
			 local->open_count == local->monitors &&	\
			 sdata->vif.type == NL80211_IFTYPE_MONITOR)	\
			channel = local->monitor_chanreq.oper.chan;	\
		else							\
			channel = NULL;					\
		rcu_read_unlock();					\
									\
		if (channel) {						\
			q = 0;						\
			do {						\
				survey.filled = 0;			\
				if (drv_get_survey(local, q, &survey) != 0) { \
					survey.filled = 0;		\
					break;				\
				}					\
				q++;					\
			} while (channel != survey.channel);		\
		}							\
									\
		if (channel) {						\
			data[i++] = channel->center_freq;		\
		} else {						\
			if (local->dflt_chandef.chan)			\
				data[i++] = local->dflt_chandef.chan->center_freq; \
			else						\
				data[i++] = 0;				\
		}							\
		if (survey.filled & SURVEY_INFO_NOISE_DBM)		\
			data[i++] = (u8)survey.noise;			\
		else							\
			data[i++] = -1LL;				\
		if (survey.filled & SURVEY_INFO_TIME)			\
			data[i++] = survey.time;			\
		else							\
			data[i++] = -1LL;				\
		if (survey.filled & SURVEY_INFO_TIME_BUSY)		\
			data[i++] = survey.time_busy;			\
		else							\
			data[i++] = -1LL;				\
		if (survey.filled & SURVEY_INFO_TIME_EXT_BUSY)		\
			data[i++] = survey.time_ext_busy;		\
		else							\
			data[i++] = -1LL;				\
		if (survey.filled & SURVEY_INFO_TIME_RX)		\
			data[i++] = survey.time_rx;			\
		else							\
			data[i++] = -1LL;				\
		if (survey.filled & SURVEY_INFO_TIME_TX)		\
			data[i++] = survey.time_tx;			\
		else							\
			data[i++] = -1LL;				\
	} while (0)

#define ADD_STA_STATS(sta)					\
	do {							\
		data[i++] += sinfo.rx_packets;			\
		data[i++] += sinfo.rx_bytes;			\
		data[i++] += (sta)->rx_stats.num_duplicates;	\
		data[i++] += (sta)->rx_stats.fragments;		\
		data[i++] += sinfo.rx_dropped_misc;		\
								\
		data[i++] += sinfo.tx_packets;			\
		data[i++] += sinfo.tx_bytes;			\
		data[i++] += (sta)->status_stats.filtered;	\
		data[i++] += sinfo.tx_failed;			\
		data[i++] += sinfo.tx_retries;			\
	} while (0)
#define STA_STATS_COUNT 10

	/* NOTE/HACK:  TX stats are not updated for anything except
	 * deflink currently.  Use those stats on the active link.
	 */
#define ADD_LINK_STA_STATS(sta, link_rx_stats, active)		\
	do {							\
		data[i++] += link_rx_stats.packets;		\
		data[i++] += link_rx_stats.bytes;		\
		data[i++] += link_rx_stats.num_duplicates;	\
		data[i++] += link_rx_stats.fragments;		\
		data[i++] += link_rx_stats.dropped;		\
								\
		if (active) {						\
			data[i++] += sinfo.tx_packets;			\
			data[i++] += sinfo.tx_bytes;			\
			data[i++] += (sta)->status_stats.filtered;	\
			data[i++] += sinfo.tx_failed;			\
			data[i++] += sinfo.tx_retries;			\
			/*
			data[i++] += _sum_acs((sta)->tx_stats.packets);	\
			data[i++] += _sum_acs((sta)->tx_stats.bytes);	\
			data[i++] += (sta)->status_stats.filtered;	\
			data[i++] += (sta)->status_stats.retry_failed;	\
			data[i++] += (sta)->status_stats.retry_count;	\
			*/						\
		} else {						\
			i += 5;	/* skip non active links */		\
		}							\
	} while (0)

	/* For Managed stations, find the single station based on BSSID
	 * and use that.  For interface types, iterate through all available
	 * stations and add stats for any station that is assigned to this
	 * network device.
	 */

	guard(wiphy)(local->hw.wiphy);

	if (sdata->vif.bss_conf.he_bss_color.enabled)
		data[i++] = sdata->vif.bss_conf.he_bss_color.color;
	else
		data[i++] = 0;
	data[i++] = sdata->vif.valid_links;
	data[i++] = sdata->vif.active_links;
	data[i++] = sdata->vif.dormant_links;

	start_link_i = i;
	if (sdata->vif.type == NL80211_IFTYPE_STATION) {
		int li;
		struct link_sta_info *link_sta;
		struct sta_info *tmp_sta;
		bool mld = ieee80211_vif_is_mld(&sdata->vif);
		struct ieee80211_sta_rx_stats link_rx_stats;
		struct ieee80211_sta_rx_stats *last_rxstats;

		// From struct sta_info *sta, I can get link_sta_info, which has the stats.
		// struct ieee80211_link_data *link

		sta = NULL;
		list_for_each_entry(tmp_sta, &local->sta_list, list) {
			/* Make sure this station belongs to the proper dev */
			if (tmp_sta->sdata->dev == dev) {
				sta = tmp_sta;
				break; /* first and only sta for IFTYPE_STATION */
			}
		}

		/* For each of the first 3 links */
		for (li = 0; li<ETHTOOL_LINK_COUNT; li++) {
			rcu_read_lock();
			link = sdata_dereference(sdata->link[li], sdata);
			if (!link) {
				/* dummy out the stats */
				i += PER_LINK_STATS_LEN;
				rcu_read_unlock();
				continue;
			}
			if (sta)
				link_sta = rcu_dereference_protected(sta->link[li],
								     lockdep_is_held(&local->hw.wiphy->mtx));
			rcu_read_unlock();

			if (!(sta && link_sta && !WARN_ON(sta->sdata->dev != dev))) {
				i += PER_LINK_STATS_LEN;
				continue;
			}

			memset(&sinfo, 0, sizeof(sinfo));
			/* sta_set_sinfo cannot hold rcu read lock since it can block
			 * calling into firmware for stats.
			 */
			sta_set_sinfo(sta, &sinfo, false);

			if (mld) {
				last_rxstats = link_sta_get_last_rx_stats(link_sta);

				link_sta_accum_rx_stats(&link_sta->rx_stats, link_sta->pcpu_rx_stats,
							&link_rx_stats);
				ADD_LINK_STA_STATS(link_sta, link_rx_stats,
						   sdata->vif.active_links & (1<<li));
			} else {
				ADD_STA_STATS(link_sta);
			}

			data[i++] = sdata->tx_handlers_drop;
			data[i++] = sta->sta_state;

			if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_TX_BITRATE))
				data[i] = 100000ULL *
					cfg80211_calculate_bitrate(&sinfo.txrate);
			i++;
			if (mld) {
				struct rate_info rxrate;
				int mn;
				u64 accum;

				link_sta_set_rate_info_rx(link_sta, &rxrate, last_rxstats);
				data[i++] = 100000ULL *
					cfg80211_calculate_bitrate(&rxrate);

				data[i++] = (u8)last_rxstats->last_signal;

				/* No beacon signal in sta_rx_stats, get something from sinfo */
				data[i++] = (u8)sinfo.rx_beacon_signal_avg;

				/* signal chains */
				mn = min_t(int, sizeof(u64), ARRAY_SIZE(last_rxstats->chain_signal_last));
				accum = (u8)last_rxstats->chain_signal_last[0];

				mn = min_t(int, mn, last_rxstats->chains);
				for (z = 1; z < mn; z++) {
					u64 csz = last_rxstats->chain_signal_last[z] & 0xFF;
					u64 cs = csz << (8 * z);

					accum |= cs;
				}
				data[i++] = accum;

				/* No chain signal avg per link, get from sinfo */
				if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_CHAIN_SIGNAL_AVG)) {
					int mn = min_t(int, sizeof(u64), ARRAY_SIZE(sinfo.chain_signal_avg));
					u64 accum = (u8)sinfo.chain_signal_avg[0];

					for (z = 1; z < mn; z++) {
						u64 csz = sinfo.chain_signal_avg[z] & 0xFF;
						u64 cs = csz << (8 * z);

						accum |= cs;
					}
					data[i] = accum;
				}
				i++;
			} else {
				if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_RX_BITRATE))
					data[i] = 100000ULL *
						cfg80211_calculate_bitrate(&sinfo.rxrate);
				i++;

				if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_SIGNAL_AVG))
					data[i] = (u8)sinfo.signal_avg;
				i++;

				if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_SIGNAL_AVG))
					data[i] = (u8)sinfo.rx_beacon_signal_avg;
				i++;

				if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_CHAIN_SIGNAL)) {
					int mn = min_t(int, sizeof(u64), ARRAY_SIZE(sinfo.chain_signal));
					u64 accum = (u8)sinfo.chain_signal[0];

					mn = min_t(int, mn, sinfo.chains);
					for (z = 1; z < mn; z++) {
						u64 csz = sinfo.chain_signal[z] & 0xFF;
						u64 cs = csz << (8 * z);

						accum |= cs;
					}
					data[i] = accum;
				}
				i++;

				if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_CHAIN_SIGNAL_AVG)) {
					int mn = min_t(int, sizeof(u64), ARRAY_SIZE(sinfo.chain_signal_avg));
					u64 accum = (u8)sinfo.chain_signal_avg[0];

					for (z = 1; z < mn; z++) {
						u64 csz = sinfo.chain_signal_avg[z] & 0xFF;
						u64 cs = csz << (8 * z);

						accum |= cs;
					}
					data[i] = accum;
				}
				i++;
			}

			ADD_SURVEY_STATS;
		} /* for first 3 links */
	} else {
		int amt_tx = 0;
		int amt_rx = 0;
		int amt_sig = 0;
		s16 amt_accum_chain[8] = {0};
		s16 amt_accum_chain_avg[8] = {0};
		s64 tx_accum = 0;
		s64 rx_accum = 0;
		s64 sig_accum = 0;
		s64 sig_accum_beacon = 0;
		s64 sig_accum_chain[8] = {0};
		s64 sig_accum_chain_avg[8] = {0};
		int start_accum_idx = start_link_i + 1 /* sta-state */ + STA_STATS_COUNT;

		list_for_each_entry(sta, &local->sta_list, list) {
			/* Make sure this station belongs to the proper dev */
			if (sta->sdata->dev != dev)
				continue;

			memset(&sinfo, 0, sizeof(sinfo));
			sta_set_sinfo(sta, &sinfo, false);
			i = start_link_i;
			ADD_STA_STATS(&sta->deflink);
			data[i++] = sdata->tx_handlers_drop;

			i++; /* skip sta state */
			if (sinfo.filled & BIT(NL80211_STA_INFO_TX_BITRATE)) {
				tx_accum += 100000ULL *
					cfg80211_calculate_bitrate(&sinfo.txrate);
				amt_tx++;
			}

			if (sinfo.filled & BIT(NL80211_STA_INFO_RX_BITRATE)) {
				rx_accum += 100000ULL *
					cfg80211_calculate_bitrate(&sinfo.rxrate);
				amt_rx++;
			}

			if (sinfo.filled & BIT(NL80211_STA_INFO_SIGNAL_AVG)) {
				sig_accum += sinfo.signal_avg;
				sig_accum_beacon += sinfo.rx_beacon_signal_avg;
				amt_sig++;
			}

			if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_CHAIN_SIGNAL)) {
				int mn = min_t(int, sizeof(u64), ARRAY_SIZE(sinfo.chain_signal));

				mn = min_t(int, mn, sinfo.chains);
				for (z = 0; z < mn; z++) {
					sig_accum_chain[z] += sinfo.chain_signal[z];
					amt_accum_chain[z]++;
				}
			}

			if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_CHAIN_SIGNAL_AVG)) {
				int mn;

				mn = min_t(int, sizeof(u64), ARRAY_SIZE(sinfo.chain_signal_avg));
				mn = min_t(int, mn, sinfo.chains);
				for (z = 0; z < mn; z++) {
					sig_accum_chain_avg[z] += sinfo.chain_signal_avg[z];
					amt_accum_chain_avg[z]++;
				}
			}
		} /* for each stations associated to AP */

		/* Do averaging */
		i = start_accum_idx;

		if (amt_tx)
			data[i] = mac_div(tx_accum, amt_tx);
		i++;

		if (amt_rx)
			data[i] = mac_div(rx_accum, amt_rx);
		i++;

		if (amt_sig) {
			data[i] = (mac_div(sig_accum, amt_sig) & 0xFF);
			data[i + 1] = (mac_div(sig_accum_beacon, amt_sig) & 0xFF);
		}
		i += 2;

		for (z = 0; z < sizeof(u64); z++) {
			if (amt_accum_chain[z]) {
				u64 val = mac_div(sig_accum_chain[z], amt_accum_chain[z]);

				val |= 0xFF;
				data[i] |= (val << (z * 8));
			}
			if (amt_accum_chain_avg[z]) {
				u64 val = mac_div(sig_accum_chain_avg[z], amt_accum_chain_avg[z]);

				val |= 0xFF;
				data[i + 1] |= (val << (z * 8));
			}
		}
		i += 2;

		ADD_SURVEY_STATS;

		/* TODO: AP doesn't support per-link stats yet */
		i += (2 * PER_LINK_STATS_LEN);
	} /* else if not STA */

	if (WARN_ON(i != STA_STATS_LEN)) {
		pr_err("mac80211 ethtool stats, i: %d  != STA_STATS_LEN: %lu\n",
		       i, STA_STATS_LEN);
		return;
	}

	drv_get_et_stats(sdata, stats, &(data[STA_STATS_LEN]), level);
}

static void ieee80211_get_stats(struct net_device *dev,
				struct ethtool_stats *stats,
				u64 *data)
{
	ieee80211_get_stats2(dev, stats, data, 0);
}

static void ieee80211_get_strings(struct net_device *dev, u32 sset, u8 *data)
{
	struct ieee80211_sub_if_data *sdata = IEEE80211_DEV_TO_SUB_IF(dev);
	int sz_sta_stats = 0;

	if (sset == ETH_SS_STATS) {
		sz_sta_stats = sizeof(ieee80211_gstrings_sta_stats);
		memcpy(data, ieee80211_gstrings_sta_stats, sz_sta_stats);
	}
	drv_get_et_strings(sdata, sset, &(data[sz_sta_stats]));
}

static int ieee80211_get_regs_len(struct net_device *dev)
{
	return 0;
}

static void ieee80211_get_regs(struct net_device *dev,
			       struct ethtool_regs *regs,
			       void *data)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;

	regs->version = wdev->wiphy->hw_version;
	regs->len = 0;
}

const struct ethtool_ops ieee80211_ethtool_ops = {
	.get_drvinfo = cfg80211_get_drvinfo,
	.get_regs_len = ieee80211_get_regs_len,
	.get_regs = ieee80211_get_regs,
	.get_link = ethtool_op_get_link,
	.get_ringparam = ieee80211_get_ringparam,
	.set_ringparam = ieee80211_set_ringparam,
	.get_strings = ieee80211_get_strings,
	.get_ethtool_stats = ieee80211_get_stats,
#ifdef HAS_ETHTOOL_STATS2
	.get_ethtool_stats2 = ieee80211_get_stats2,
#endif
	.get_sset_count = ieee80211_get_sset_count,
};
