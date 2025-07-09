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

#define ETHTOOL_LINK_COUNT 3 /* we will show stats for first 3 links */
struct ieee80211_ethtool_data_link_stats {
	u64 rx_packets;
	u64 rx_bytes;
	u64 rx_duplicates;
	u64 rx_fragments;
	u64 rx_dropped;
	u64 tx_packets;
	u64 tx_bytes;
	u64 tx_filtered;
	u64 tx_retry_failed;
	u64 tx_retries;
	u64 tx_handlers_drop;
	u64 sta_state;
	u64 txrate;
	u64 rxrate;
	u64 signal;
	u64 signal_beacon;
	u64 signal_chains;
	u64 signal_chains_avg;

	/* Add new stats here, channel and others go below */
	u64 channel;
	u64 ch_center;
	u64 noise;
	u64 ch_time;
	u64 ch_time_busy;
	u64 ch_time_ext_busy;
	u64 ch_time_rx;
	u64 ch_time_tx;
} __attribute__((__packed__));

struct ieee80211_ethtool_data_sta_stats {
	u64 bss_color;
	u64 valid_links;
	u64 active_links;
	u64 dormant_links;

	/* Link stats */
	struct ieee80211_ethtool_data_link_stats link_stats[ETHTOOL_LINK_COUNT];
} __attribute__((__packed__));

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
	"ch_center",
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
	"L1:ch_center",
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
	"L2:ch_center",
	"L2:noise",
	"L2:ch_time",
	"L2:ch_time_busy",
	"L2:ch_time_ext_busy",
	"L2:ch_time_rx",
	"L2:ch_time_tx",
};
#define STA_STATS_LEN	ARRAY_SIZE(ieee80211_gstrings_sta_stats)
#define SDATA_STATS_LEN 4 /* bss color, active_links ... */
#define PER_LINK_STATS_LEN ((STA_STATS_LEN - SDATA_STATS_LEN) / ETHTOOL_LINK_COUNT)

struct ieee80211_ethtool_data_vdev_stats {
	u64 rx_packets;
	u64 rx_bytes;
	u64 rx_duplicates;
	u64 rx_fragments;
	u64 rx_dropped;
	u64 tx_packets;
	u64 tx_bytes;
	u64 tx_filtered;
	u64 tx_retry_failed;
	u64 tx_retries;
	u64 tx_handlers_drop;
	u64 sta_state;
	u64 txrate;
	u64 rxrate;
	u64 signal;
	u64 signal_beacon;
	u64 signal_chains;
	u64 signal_chains_avg;

	/* Histogram stats */
	u64 v_tx_bw_20;
	u64 v_tx_bw_40;
	u64 v_tx_bw_80;
	u64 v_tx_bw_160;
	u64 v_tx_mcs_0;
	u64 v_tx_mcs_1;
	u64 v_tx_mcs_2;
	u64 v_tx_mcs_3;
	u64 v_tx_mcs_4;
	u64 v_tx_mcs_5;
	u64 v_tx_mcs_6;
	u64 v_tx_mcs_7;
	u64 v_tx_mcs_8;
	u64 v_tx_mcs_9;

	u64 v_rx_bw_20;
	u64 v_rx_bw_40;
	u64 v_rx_bw_80;
	u64 v_rx_bw_160;
	u64 v_rx_mcs_0;
	u64 v_rx_mcs_1;
	u64 v_rx_mcs_2;
	u64 v_rx_mcs_3;
	u64 v_rx_mcs_4;
	u64 v_rx_mcs_5;
	u64 v_rx_mcs_6;
	u64 v_rx_mcs_7;
	u64 v_rx_mcs_8;
	u64 v_rx_mcs_9;

	/* Add new stats here, channel and others go below */
	u64 channel;
	u64 ch_center;
	u64 noise;
	u64 ch_time;
	u64 ch_time_busy;
	u64 ch_time_ext_busy;
	u64 ch_time_rx;
	u64 ch_time_tx;
} __attribute__((__packed__));


#ifdef CONFIG_MAC80211_DEBUG_STA_COUNTERS
/* Stations can use this by setting the NL80211_EXT_FEATURE_ETHTOOL_VDEV_STATS
 * flag. Intended for use with IEEE802.11ac and older radios.
 */
static const char ieee80211_gstrings_sta_vdev_stats[][ETH_GSTRING_LEN] = {
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

	/* Histogram stats */
	"v_tx_bw_20",
	"v_tx_bw_40",
	"v_tx_bw_80",
	"v_tx_bw_160",
	"v_tx_mcs_0",
	"v_tx_mcs_1",
	"v_tx_mcs_2",
	"v_tx_mcs_3",
	"v_tx_mcs_4",
	"v_tx_mcs_5",
	"v_tx_mcs_6",
	"v_tx_mcs_7",
	"v_tx_mcs_8",
	"v_tx_mcs_9",

	"v_rx_bw_20",
	"v_rx_bw_40",
	"v_rx_bw_80",
	"v_rx_bw_160",
	"v_rx_mcs_0",
	"v_rx_mcs_1",
	"v_rx_mcs_2",
	"v_rx_mcs_3",
	"v_rx_mcs_4",
	"v_rx_mcs_5",
	"v_rx_mcs_6",
	"v_rx_mcs_7",
	"v_rx_mcs_8",
	"v_rx_mcs_9",

	/* Add new stats here, channel and others go below */
	"channel",
	"ch_center",
	"noise",
	"ch_time",
	"ch_time_busy",
	"ch_time_ext_busy",
	"ch_time_rx",
	"ch_time_tx",
};
#define STA_VDEV_STATS_LEN ARRAY_SIZE(ieee80211_gstrings_sta_vdev_stats)
#endif

static int ieee80211_get_sset_count(struct net_device *dev, int sset)
{
	struct ieee80211_sub_if_data *sdata = IEEE80211_DEV_TO_SUB_IF(dev);
	int rv = 0;

	if (sset == ETH_SS_STATS) {
#ifdef CONFIG_MAC80211_DEBUG_STA_COUNTERS
		if (wiphy_ext_feature_isset(sdata->local->hw.wiphy,
					    NL80211_EXT_FEATURE_ETHTOOL_VDEV_STATS))
			rv += STA_VDEV_STATS_LEN;
		else
			rv += STA_STATS_LEN;
#else
		rv += STA_STATS_LEN;
#endif
	}

	rv += drv_get_et_sset_count(sdata, sset);

	if (rv == 0)
		return -EOPNOTSUPP;
	return rv;
}

/* The following macros are for the *_get_stats2 functions */
#define ADD_SURVEY_STATS(sdata, data, local)				\
	do {								\
		struct ieee80211_chanctx_conf *_chanctx_conf;		\
		struct cfg80211_chan_def *_chan_def;			\
		struct ieee80211_channel *_channel;			\
		struct survey_info _survey;				\
		int __q;						\
									\
		/* Get survey stats for current channel */		\
		_survey.filled = 0;					\
									\
		rcu_read_lock();					\
		_chanctx_conf = rcu_dereference(sdata->vif.bss_conf.chanctx_conf); \
		if ((link)) {						\
			_chan_def = &(link)->conf->chanreq.oper;	\
			_channel = _chan_def->chan;			\
		} else if (_chanctx_conf) {				\
			_chan_def = &_chanctx_conf->def;		\
			_channel = _chan_def->chan;			\
		} else if (local->open_count > 0 &&			\
			 local->open_count == local->monitors &&	\
			 sdata->vif.type == NL80211_IFTYPE_MONITOR) {	\
			_chan_def = &(local)->monitor_chanreq.oper;	\
			_channel = _chan_def->chan;			\
		} else {						\
			_chan_def = NULL;				\
			_channel = NULL;				\
		}							\
		rcu_read_unlock();					\
									\
		if (_channel) {						\
			__q = 0;					\
			do {						\
				_survey.filled = 0;			\
				if (drv_get_survey((local), __q, &_survey) != 0) { \
					_survey.filled = 0;		\
					break;				\
				}					\
				__q++;					\
			} while (_channel != _survey.channel);		\
		}							\
									\
		if (_channel) {						\
			(data)->channel = _channel->center_freq;	\
		} else {						\
			if ((local)->dflt_chandef.chan)			\
				(data)->channel = (local)->dflt_chandef.chan->center_freq; \
		}							\
		if (_chan_def)						\
			(data)->ch_center = _chan_def->center_freq1;	\
		else							\
			if ((local))					\
				(data)->ch_center = (local)->dflt_chandef.center_freq1; \
		if (_survey.filled & SURVEY_INFO_NOISE_DBM)		\
			(data)->noise = (u8)_survey.noise;		\
		else							\
			(data)->noise = -1LL;				\
		if (_survey.filled & SURVEY_INFO_TIME)			\
			(data)->ch_time = _survey.time;			\
		else							\
			(data)->ch_time = -1LL;				\
		if (_survey.filled & SURVEY_INFO_TIME_BUSY)		\
			(data)->ch_time_busy = _survey.time_busy;	\
		else							\
			(data)->ch_time_busy = -1LL;			\
		if (_survey.filled & SURVEY_INFO_TIME_EXT_BUSY)	\
			(data)->ch_time_ext_busy = _survey.time_ext_busy; \
		else							\
			(data)->ch_time_ext_busy = -1LL;		\
		if (_survey.filled & SURVEY_INFO_TIME_RX)		\
			(data)->ch_time_rx = _survey.time_rx;		\
		else							\
			(data)->ch_time_rx = -1LL;			\
		if (_survey.filled & SURVEY_INFO_TIME_TX)		\
			(data)->ch_time_tx = _survey.time_tx;		\
		else							\
			(data)->ch_time_tx = -1LL;			\
	} while (0)
#define STA_STATS_SURVEY_LEN 8

#define ADD_STA_STATS(data, sinfo, sta)					\
	do {								\
		(data)->rx_packets += (sinfo).rx_packets;		\
		(data)->rx_bytes += (sinfo).rx_bytes;			\
		(data)->rx_duplicates += (sta)->rx_stats.num_duplicates; \
		(data)->rx_fragments += (sta)->rx_stats.fragments;	\
		(data)->rx_dropped += (sinfo).rx_dropped_misc;		\
									\
		(data)->tx_packets += (sinfo).tx_packets;		\
		(data)->tx_bytes += (sinfo).tx_bytes;			\
		(data)->tx_filtered += (sta)->status_stats.filtered;	\
		(data)->tx_retry_failed += (sinfo).tx_failed;		\
		(data)->tx_retries += (sinfo).tx_retries;		\
	} while (0)
#define STA_STATS_COUNT 10

#ifdef CONFIG_MAC80211_DEBUG_STA_COUNTERS
static void ieee80211_get_stats2_vdev(struct net_device *dev,
				      struct ethtool_stats *stats,
				      u64 *_data, u32 level)
{
	struct ieee80211_sub_if_data *sdata = IEEE80211_DEV_TO_SUB_IF(dev);
	struct sta_info *sta;
	struct ieee80211_local *local = sdata->local;
	struct station_info sinfo;
	struct ieee80211_link_data *link = NULL;
	u64 *tmp_data;
	int z;
	struct ieee80211_ethtool_data_vdev_stats *data =
		(struct ieee80211_ethtool_data_vdev_stats*)(_data);

	memset(data, 0, sizeof(*data));

#define ADD_VDEV_STATS							\
	do {								\
		data->v_tx_bw_20 += sta->deflink.tx_stats.msdu_20;	\
		data->v_tx_bw_40 += sta->deflink.tx_stats.msdu_40;	\
		data->v_tx_bw_80 += sta->deflink.tx_stats.msdu_80;	\
		data->v_tx_bw_160 += sta->deflink.tx_stats.msdu_160;	\
		tmp_data = &(data->v_tx_mcs_0);				\
		for (z = 0; z < 10; z++)				\
			tmp_data[z] += sta->deflink.tx_stats.msdu_rate_idx[z]; \
		data->v_rx_bw_20 += sta->deflink.rx_stats.msdu_20;	\
		data->v_rx_bw_40 += sta->deflink.rx_stats.msdu_40;	\
		data->v_rx_bw_80 += sta->deflink.rx_stats.msdu_80;	\
		data->v_rx_bw_160 += sta->deflink.rx_stats.msdu_160;	\
		tmp_data = &(data->v_rx_mcs_0);				\
		for (z = 0; z < 10; z++)				\
			tmp_data[z] += sta->deflink.rx_stats.msdu_rate_idx[z]; \
	} while (0)

	/* For Managed stations, find the single station based on BSSID
	 * and use that.  For interface types, iterate through all available
	 * stations and add stats for any station that is assigned to this
	 * network device.
	 */

	wiphy_lock(local->hw.wiphy);

	if (sdata->vif.type == NL80211_IFTYPE_STATION) {
		rcu_read_lock();
		sta = ieee80211_find_best_sta_link(sdata, &link);
		rcu_read_unlock();

		if (!(sta && !WARN_ON(sta->sdata->dev != dev)))
			goto do_survey;

		memset(&sinfo, 0, sizeof(sinfo));
		/* sta_set_sinfo cannot hold rcu read lock since it can block
		 * calling into firmware for stats.
		 */
		sta_set_sinfo(sta, &sinfo, false);

		ADD_STA_STATS(data, sinfo, &sta->deflink);

		data->tx_handlers_drop = sdata->tx_handlers_drop;
		data->sta_state = sta->sta_state;

		if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_TX_BITRATE))
			data->txrate = 100000ULL *
				cfg80211_calculate_bitrate(&sinfo.txrate);
		if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_RX_BITRATE))
			data->rxrate = 100000ULL *
				cfg80211_calculate_bitrate(&sinfo.rxrate);

		if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_SIGNAL_AVG))
			data->signal = (u8)sinfo.signal_avg;

		if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_SIGNAL_AVG))
			data->signal_beacon = (u8)sinfo.rx_beacon_signal_avg;

		if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_CHAIN_SIGNAL)) {
			int mn = min_t(int, sizeof(u64), ARRAY_SIZE(sinfo.chain_signal));
			u64 accum = (u8)sinfo.chain_signal[0];

			mn = min_t(int, mn, sinfo.chains);
			for (z = 1; z < mn; z++) {
				u64 csz = sinfo.chain_signal[z] & 0xFF;
				u64 cs = csz << (8 * z);

				accum |= cs;
			}
			data->signal_chains = accum;
		}

		if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_CHAIN_SIGNAL_AVG)) {
			int mn = min_t(int, sizeof(u64), ARRAY_SIZE(sinfo.chain_signal_avg));
			u64 accum = (u8)sinfo.chain_signal_avg[0];

			for (z = 1; z < mn; z++) {
				u64 csz = sinfo.chain_signal_avg[z] & 0xFF;
				u64 cs = csz << (8 * z);

				accum |= cs;
			}
			data->signal_chains_avg = accum;
		}

		ADD_VDEV_STATS;
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

		list_for_each_entry(sta, &local->sta_list, list) {
			/* Make sure this station belongs to the proper dev */
			if (sta->sdata->dev != dev)
				continue;

			memset(&sinfo, 0, sizeof(sinfo));
			sta_set_sinfo(sta, &sinfo, false);

			ADD_STA_STATS(data, sinfo, &sta->deflink);

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
		if (amt_tx)
			data->txrate = mac_div(tx_accum, amt_tx);

		if (amt_rx)
			data->rxrate = mac_div(rx_accum, amt_rx);

		if (amt_sig) {
			data->signal = (mac_div(sig_accum, amt_sig) & 0xFF);
			data->signal_beacon = (mac_div(sig_accum_beacon, amt_sig) & 0xFF);
		}

		for (z = 0; z < sizeof(u64); z++) {
			if (amt_accum_chain[z]) {
				u64 val = mac_div(sig_accum_chain[z], amt_accum_chain[z]);

				val &= 0xFF;
				data->signal_chains |= (val << (z * 8));
			}
			if (amt_accum_chain_avg[z]) {
				u64 val = mac_div(sig_accum_chain_avg[z], amt_accum_chain_avg[z]);

				val &= 0xFF;
				data->signal_chains_avg |= (val << (z * 8));
			}
		}
		ADD_VDEV_STATS;
	} /* else if not STA */

do_survey:
	ADD_SURVEY_STATS(sdata, data, local);

	if (WARN_ON(sizeof(*data) != STA_VDEV_STATS_LEN*8)) {
		pr_err("mac80211 ethtool stats, data-size: %lu  != STA_STATS_LENx8: %lu\n",
		       sizeof(*data), STA_VDEV_STATS_LEN*8);
		wiphy_unlock(local->hw.wiphy);
		return;
	}

	drv_get_et_stats(sdata, stats, &_data[STA_VDEV_STATS_LEN], level);
	wiphy_unlock(local->hw.wiphy);
}
#endif

static void ieee80211_add_link_sta_stats(u32 li, struct link_sta_info *link_sta,
					 struct ieee80211_sta_rx_stats *link_rx_stats,
					 u32 active, struct station_info *sinfo,
					 struct ieee80211_ethtool_data_sta_stats *data)
{

	data->link_stats[li].rx_packets += link_rx_stats->packets;
	data->link_stats[li].rx_bytes += u64_stats_read(&link_rx_stats->bytes);
	data->link_stats[li].rx_duplicates += link_rx_stats->num_duplicates;
	data->link_stats[li].rx_fragments += link_rx_stats->fragments;
	data->link_stats[li].rx_dropped += link_rx_stats->dropped;

	if (active) {
		/* Driver reported values take precedence, for cases like
		 * mtk7996 that cannot properly report tx-link-id on a per
		 * packet basis. */

		if (sinfo->links[li]->filled &
		    BIT_ULL(NL80211_STA_INFO_TX_PACKETS))
			data->link_stats[li].tx_packets += sinfo->links[li]->tx_packets;
		else
			data->link_stats[li].tx_packets += link_sta->tx_stats.rep_packets;
		if (sinfo->links[li]->filled &
		    BIT_ULL(NL80211_STA_INFO_TX_BYTES64))
			data->link_stats[li].tx_bytes += sinfo->links[li]->tx_bytes;
		else
			data->link_stats[li].tx_bytes += link_sta->tx_stats.rep_bytes;
		data->link_stats[li].tx_filtered += link_sta->status_stats.filtered;
		if (sinfo->links[li]->filled &
		    BIT_ULL(NL80211_STA_INFO_TX_FAILED))
			data->link_stats[li].tx_retry_failed += sinfo->links[li]->tx_failed;
		else
			data->link_stats[li].tx_retry_failed += link_sta->status_stats.retry_failed;
		if (sinfo->links[li]->filled &
		    BIT_ULL(NL80211_STA_INFO_TX_RETRIES))
			data->link_stats[li].tx_retries += sinfo->links[li]->tx_retries;
		else
			data->link_stats[li].tx_retries += link_sta->status_stats.retry_count;
	}
}


static void ieee80211_get_stats2(struct net_device *dev,
				 struct ethtool_stats *stats,
				 u64 *_data, u32 level)
{
	struct ieee80211_sub_if_data *sdata = IEEE80211_DEV_TO_SUB_IF(dev);
	struct sta_info *sta;
	struct ieee80211_local *local = sdata->local;
	struct station_info sinfo;
	struct ieee80211_link_data *link = NULL;
	int z;
	struct ieee80211_ethtool_data_sta_stats *data;

#ifdef CONFIG_MAC80211_DEBUG_STA_COUNTERS
	/* If the driver needs to get vdev stats from here...*/
	if (wiphy_ext_feature_isset(sdata->local->hw.wiphy,
				    NL80211_EXT_FEATURE_ETHTOOL_VDEV_STATS)) {
		ieee80211_get_stats2_vdev(dev, stats, _data, level);
		return;
	}
#endif

	data = (struct ieee80211_ethtool_data_sta_stats*)(_data);
	memset(data, 0, sizeof(*data));

	/* For Managed stations, find the single station based on BSSID
	 * and use that.  For interface types, iterate through all available
	 * stations and add stats for any station that is assigned to this
	 * network device.
	 */

	guard(wiphy)(local->hw.wiphy);

	if (sdata->vif.bss_conf.he_bss_color.enabled)
		data->bss_color = sdata->vif.bss_conf.he_bss_color.color;
	data->valid_links = sdata->vif.valid_links;
	data->active_links = sdata->vif.active_links;
	data->dormant_links = sdata->vif.dormant_links;

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

		memset(&sinfo, 0, sizeof(sinfo));
		/* sta_set_sinfo cannot hold rcu read lock since it can block
		 * calling into firmware for stats.
		 */
		if (sta)
			sta_set_sinfo(sta, &sinfo, false);

		/* For each of the first 3 links */
		for (li = 0; li<ETHTOOL_LINK_COUNT; li++) {
			rcu_read_lock();
			link = sdata_dereference(sdata->link[li], sdata);
			if (!link) {
				rcu_read_unlock();
				continue;
			}
			if (sta)
				link_sta = rcu_dereference_protected(sta->link[li],
								     lockdep_is_held(&local->hw.wiphy->mtx));
			rcu_read_unlock();

			if (!(sta && link_sta && !WARN_ON(sta->sdata->dev != dev))) {
				continue;
			}

			if (mld) {
				last_rxstats = link_sta_get_last_rx_stats(link_sta);

				link_sta_accum_rx_stats(&link_sta->rx_stats, link_sta->pcpu_rx_stats,
							&link_rx_stats);
				ieee80211_add_link_sta_stats(li, link_sta, &link_rx_stats,
							     sdata->vif.active_links & (1<<li),
							     &sinfo, data);
			} else {
				ADD_STA_STATS(&(data->link_stats[li]), sinfo, link_sta);
			}

			data->link_stats[li].tx_handlers_drop = sdata->tx_handlers_drop;
			data->link_stats[li].sta_state = sta->sta_state;

			if (mld) {
				struct rate_info txrxrate = {};
				int mn;
				u64 accum;

				if (sinfo.links[li]->filled &
				    BIT_ULL(NL80211_STA_INFO_TX_BITRATE)) {
					data->link_stats[li].txrate = 100000ULL *
						cfg80211_calculate_bitrate(&sinfo.links[li]->txrate);
				} else {
					/* Get it from sinfo then, better than nothing I guess */
					if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_TX_BITRATE))
						data->link_stats[li].txrate = 100000ULL *
							cfg80211_calculate_bitrate(&sinfo.txrate);
				}

				link_sta_set_rate_info_rx(link_sta, &txrxrate, last_rxstats);
				data->link_stats[li].rxrate = 100000ULL *
					cfg80211_calculate_bitrate(&txrxrate);

				data->link_stats[li].signal = (u8)last_rxstats->last_signal;

				if (sinfo.links[li]->filled &
				    BIT_ULL(NL80211_STA_INFO_BEACON_SIGNAL_AVG))
					data->link_stats[li].signal_beacon
						= (u8)(sinfo.links[li]->rx_beacon_signal_avg);
				else
					/* No beacon signal in sta_rx_stats, get something from sinfo */
					data->link_stats[li].signal_beacon = (u8)sinfo.rx_beacon_signal_avg;

				/* signal chains */
				mn = min_t(int, sizeof(u64), ARRAY_SIZE(last_rxstats->chain_signal_last));
				accum = (u8)last_rxstats->chain_signal_last[0];

				mn = min_t(int, mn, last_rxstats->chains);
				for (z = 1; z < mn; z++) {
					u64 csz = last_rxstats->chain_signal_last[z] & 0xFF;
					u64 cs = csz << (8 * z);

					accum |= cs;
				}
				data->link_stats[li].signal_chains = accum;

				/* No chain signal avg per link, get from sinfo */
				if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_CHAIN_SIGNAL_AVG)) {
					int mn = min_t(int, sizeof(u64), ARRAY_SIZE(sinfo.chain_signal_avg));
					u64 accum = (u8)sinfo.chain_signal_avg[0];

					for (z = 1; z < mn; z++) {
						u64 csz = sinfo.chain_signal_avg[z] & 0xFF;
						u64 cs = csz << (8 * z);

						accum |= cs;
					}
					data->link_stats[li].signal_chains_avg = accum;
				}
			} else {
				if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_TX_BITRATE))
					data->link_stats[li].txrate = 100000ULL *
						cfg80211_calculate_bitrate(&sinfo.txrate);

				if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_RX_BITRATE))
					data->link_stats[li].rxrate = 100000ULL *
						cfg80211_calculate_bitrate(&sinfo.rxrate);

				if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_SIGNAL_AVG))
					data->link_stats[li].signal = (u8)sinfo.signal_avg;

				if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_SIGNAL_AVG))
					data->link_stats[li].signal_beacon = (u8)sinfo.rx_beacon_signal_avg;

				if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_CHAIN_SIGNAL)) {
					int mn = min_t(int, sizeof(u64), ARRAY_SIZE(sinfo.chain_signal));
					u64 accum = (u8)sinfo.chain_signal[0];

					mn = min_t(int, mn, sinfo.chains);
					for (z = 1; z < mn; z++) {
						u64 csz = sinfo.chain_signal[z] & 0xFF;
						u64 cs = csz << (8 * z);

						accum |= cs;
					}
					data->link_stats[li].signal_chains = accum;
				}

				if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_CHAIN_SIGNAL_AVG)) {
					int mn = min_t(int, sizeof(u64), ARRAY_SIZE(sinfo.chain_signal_avg));
					u64 accum = (u8)sinfo.chain_signal_avg[0];

					for (z = 1; z < mn; z++) {
						u64 csz = sinfo.chain_signal_avg[z] & 0xFF;
						u64 cs = csz << (8 * z);

						accum |= cs;
					}
					data->link_stats[li].signal_chains_avg = accum;
				}
			}

			ADD_SURVEY_STATS(sdata, &(data->link_stats[li]), local);
		} /* for first 3 links */
	} else {
		/* else not type STATION, ie AP or something */
		bool mld = ieee80211_vif_is_mld(&sdata->vif);
		struct ieee80211_sta_rx_stats link_rx_stats;
		struct ieee80211_sta_rx_stats *last_rxstats;
		int li;
		struct per_link_accum {
			int amt_tx;
			int amt_rx;
			int amt_sig;
			s16 amt_accum_chain[8];
			s16 amt_accum_chain_avg[8];
			s64 tx_accum;
			s64 rx_accum;
			s64 sig_accum;
			s64 sig_accum_beacon;
			s64 sig_accum_chain[8];
			s64 sig_accum_chain_avg[8];
		};
		struct per_link_accum accums[ETHTOOL_LINK_COUNT] = {};

		list_for_each_entry(sta, &local->sta_list, list) {
			/* Make sure this station belongs to the proper dev */
			if (sta->sdata->dev != dev)
				continue;

			memset(&sinfo, 0, sizeof(sinfo));
			sta_set_sinfo(sta, &sinfo, false);

			/* For each of the first 3 links */
			for (li = 0; li<ETHTOOL_LINK_COUNT; li++) {
				struct link_sta_info *link_sta;

				rcu_read_lock();
				link = sdata_dereference(sdata->link[li], sdata);
				if (!link) {
					rcu_read_unlock();
					continue;
				}
				if (sta)
					link_sta = rcu_dereference_protected(sta->link[li],
									     lockdep_is_held(&local->hw.wiphy->mtx));
				rcu_read_unlock();

				if (!(sta && link_sta && !WARN_ON(sta->sdata->dev != dev))) {
					continue;
				}

				if (mld) {
					last_rxstats = link_sta_get_last_rx_stats(link_sta);

					link_sta_accum_rx_stats(&link_sta->rx_stats, link_sta->pcpu_rx_stats,
								&link_rx_stats);
					ieee80211_add_link_sta_stats(li, link_sta, &link_rx_stats,
								     sdata->vif.active_links & (1<<li),
								     &sinfo, data);
				} else {
					ADD_STA_STATS(&(data->link_stats[li]), sinfo, link_sta);
				}

				if (sinfo.filled & BIT(NL80211_STA_INFO_TX_BITRATE)) {
					accums[li].tx_accum += 100000ULL *
						cfg80211_calculate_bitrate(&sinfo.txrate);
					accums[li].amt_tx++;
				}

				if (sinfo.filled & BIT(NL80211_STA_INFO_RX_BITRATE)) {
					accums[li].rx_accum += 100000ULL *
						cfg80211_calculate_bitrate(&sinfo.rxrate);
					accums[li].amt_rx++;
				}

				if (sinfo.filled & BIT(NL80211_STA_INFO_SIGNAL_AVG)) {
					accums[li].sig_accum += sinfo.signal_avg;
					accums[li].sig_accum_beacon += sinfo.rx_beacon_signal_avg;
					accums[li].amt_sig++;
				}

				if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_CHAIN_SIGNAL)) {
					int mn = min_t(int, sizeof(u64), ARRAY_SIZE(sinfo.chain_signal));

					mn = min_t(int, mn, sinfo.chains);
					for (z = 0; z < mn; z++) {
						accums[li].sig_accum_chain[z] += sinfo.chain_signal[z];
						accums[li].amt_accum_chain[z]++;
					}
				}

				if (sinfo.filled & BIT_ULL(NL80211_STA_INFO_CHAIN_SIGNAL_AVG)) {
					int mn;

					mn = min_t(int, sizeof(u64), ARRAY_SIZE(sinfo.chain_signal_avg));
					mn = min_t(int, mn, sinfo.chains);
					for (z = 0; z < mn; z++) {
						accums[li].sig_accum_chain_avg[z] += sinfo.chain_signal_avg[z];
						accums[li].amt_accum_chain_avg[z]++;
					}
				}
				
				data->link_stats[li].tx_handlers_drop = sdata->tx_handlers_drop;
			} /* for each of 3 links */
		} /* for each stations associated to AP */


		/* Do averaging */
		for (li = 0; li<ETHTOOL_LINK_COUNT; li++) {
			if (accums[li].amt_tx)
				data->link_stats[li].txrate = mac_div(accums[li].tx_accum, accums[li].amt_tx);

			if (accums[li].amt_rx)
				data->link_stats[li].rxrate = mac_div(accums[li].rx_accum, accums[li].amt_rx);

			if (accums[li].amt_sig) {
				data->link_stats[li].signal = (mac_div(accums[li].sig_accum, accums[li].amt_sig) & 0xFF);
				data->link_stats[li].signal_beacon = (mac_div(accums[li].sig_accum_beacon, accums[li].amt_sig) & 0xFF);
			}

			for (z = 0; z < sizeof(u64); z++) {
				if (accums[li].amt_accum_chain[z]) {
					u64 val = mac_div(accums[li].sig_accum_chain[z], accums[li].amt_accum_chain[z]);

					val &= 0xFF;
					data->link_stats[li].signal_chains |= (val << (z * 8));
				}
				if (accums[li].amt_accum_chain_avg[z]) {
					u64 val = mac_div(accums[li].sig_accum_chain_avg[z], accums[li].amt_accum_chain_avg[z]);

					val &= 0xFF;
					data->link_stats[li].signal_chains_avg |= (val << (z * 8));
				}
			}

			ADD_SURVEY_STATS(sdata, (&(data->link_stats[li])), local);
		} /* for each of 3 links */
	} /* else if not STA */

	if (WARN_ON(sizeof(*data) != STA_STATS_LEN*8)) {
		pr_err("mac80211 ethtool stats, data-size: %lu  != STA_STATS_LENx8: %lu\n",
		       sizeof(*data), STA_STATS_LEN*8);
		return;
	}

	drv_get_et_stats(sdata, stats, &(_data[STA_STATS_LEN]), level);
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
#ifdef CONFIG_MAC80211_DEBUG_STA_COUNTERS
		if (wiphy_ext_feature_isset(sdata->local->hw.wiphy,
					    NL80211_EXT_FEATURE_ETHTOOL_VDEV_STATS)) {
			sz_sta_stats = sizeof(ieee80211_gstrings_sta_vdev_stats);
			memcpy(data, ieee80211_gstrings_sta_vdev_stats, sz_sta_stats);
		} else {
			sz_sta_stats = sizeof(ieee80211_gstrings_sta_stats);
			memcpy(data, ieee80211_gstrings_sta_stats, sz_sta_stats);
		}
#else
		sz_sta_stats = sizeof(ieee80211_gstrings_sta_stats);
		memcpy(data, ieee80211_gstrings_sta_stats, sz_sta_stats);
#endif
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
