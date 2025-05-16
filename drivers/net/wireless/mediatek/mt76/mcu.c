// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (C) 2019 Lorenzo Bianconi <lorenzo.bianconi83@gmail.com>
 */

#include "mt76.h"

struct sk_buff *
__mt76_mcu_msg_alloc(struct mt76_dev *dev, const void *data,
		     int len, int data_len, gfp_t gfp)
{
	const struct mt76_mcu_ops *ops = dev->mcu_ops;
	struct sk_buff *skb;

	len = max_t(int, len, data_len);
	len = ops->headroom + len + ops->tailroom;

	skb = alloc_skb(len, gfp);
	if (!skb)
		return NULL;

	memset(skb->head, 0, len);
	skb_reserve(skb, ops->headroom);

	if (data && data_len)
		skb_put_data(skb, data, data_len);

	return skb;
}
EXPORT_SYMBOL_GPL(__mt76_mcu_msg_alloc);

struct sk_buff *mt76_mcu_get_response(struct mt76_dev *dev,
				      unsigned long expires)
{
	unsigned long timeout;

	if (!time_is_after_jiffies(expires))
		return NULL;

	timeout = expires - jiffies;
	wait_event_timeout(dev->mcu.wait,
			   (!skb_queue_empty(&dev->mcu.res_q) ||
			    test_bit(MT76_MCU_RESET, &dev->phy.state)),
			   timeout);
	return skb_dequeue(&dev->mcu.res_q);
}
EXPORT_SYMBOL_GPL(mt76_mcu_get_response);

void mt76_mcu_rx_event(struct mt76_dev *dev, struct sk_buff *skb)
{
	skb_queue_tail(&dev->mcu.res_q, skb);
	wake_up(&dev->mcu.wait);
}
EXPORT_SYMBOL_GPL(mt76_mcu_rx_event);

int mt76_mcu_send_and_get_msg(struct mt76_dev *dev, int cmd, const void *data,
			      int len, bool wait_resp, struct sk_buff **ret_skb)
{
	struct sk_buff *skb;

	if (dev->mcu_ops->mcu_send_msg)
		return dev->mcu_ops->mcu_send_msg(dev, cmd, data, len, wait_resp);

	skb = mt76_mcu_msg_alloc(dev, data, len);
	if (!skb)
		return -ENOMEM;

	return mt76_mcu_skb_send_and_get_msg(dev, skb, cmd, wait_resp, ret_skb);
}
EXPORT_SYMBOL_GPL(mt76_mcu_send_and_get_msg);

static inline void scan_skb_for(struct sk_buff *skb, u8 target, int how_many) {
	int in_a_row = 0;
	for (int i = 0; i < skb->len; i++) {
		if (skb->data[i] == target)
			in_a_row++;
		else
			in_a_row = 0;

		if (in_a_row >= how_many) {
			pr_info("Found potentially poisoned buffer (%02hhx, %d in a row)\n",
			        target, in_a_row);
			dump_stack_lvl(KERN_INFO);
			for (int j = 0; j < skb->len; j += 4) {
				switch ((skb->len - j) % 4) {
					case 1:
						pr_info("%02hhx\n", skb->data[j]);
						break;
					case 2:
						pr_info("%02hhx %02hhx\n",
						        skb->data[j], skb->data[j + 1]);
						break;
					case 3:
						pr_info("%02hhx %02hhx %02hhx\n",
						        skb->data[j], skb->data[j + 1],
							skb->data[j + 2]);
						break;
					default:
						pr_info("%02hhx %02hhx %02hhx %02hhx\n",
						        skb->data[j], skb->data[j + 1],
							skb->data[j + 2], skb->data[j + 3]);
						break;
				}
			}

			break;
		}
	}
}

int mt76_mcu_skb_send_and_get_msg(struct mt76_dev *dev, struct sk_buff *skb,
				  int cmd, bool wait_resp,
				  struct sk_buff **ret_skb)
{
	unsigned int retry = 0;
	struct sk_buff *orig_skb = NULL;
	unsigned long expires;
	int ret, seq;

	if (mt76_is_sdio(dev))
		if (test_bit(MT76_RESET, &dev->phy.state) && atomic_read(&dev->bus_hung))
			return -EIO;

	if (ret_skb)
		*ret_skb = NULL;

	mutex_lock(&dev->mcu.mutex);

	if (dev->mcu_ops->mcu_skb_prepare_msg) {
		orig_skb = skb;
		ret = dev->mcu_ops->mcu_skb_prepare_msg(dev, skb, cmd, &seq);
		if (ret < 0)
			goto out;
	}

retry:
	/* orig skb might be needed for retry, mcu_skb_send_msg consumes it */
	if (orig_skb)
		skb_get(orig_skb);
	ret = dev->mcu_ops->mcu_skb_send_msg(dev, skb, cmd, wait_resp ? &seq : NULL);
	if (ret < 0)
		goto out;

	if (dev->phy.mcu_poison)
		scan_skb_for(skb, dev->phy.mcu_poison, 1);

	if (!wait_resp) {
		ret = 0;
		goto out;
	}

	expires = jiffies + dev->mcu.timeout;

	do {
		skb = mt76_mcu_get_response(dev, expires);
		if (!skb && !test_bit(MT76_MCU_RESET, &dev->phy.state) &&
		    orig_skb && retry++ < dev->mcu_ops->max_retry) {
			dev_err(dev->dev, "Retry message %08x (seq %d)\n",
				cmd, seq);
			skb = orig_skb;
			goto retry;
		}

		ret = dev->mcu_ops->mcu_parse_response(dev, cmd, skb, seq);
		if (!ret && ret_skb)
			*ret_skb = skb;
		else
			dev_kfree_skb(skb);
	} while (ret == -EAGAIN);

	if (ret == -ETIMEDOUT)
		mt76_dump_mcu_debug_buf(&dev->phy);

out:
	dev_kfree_skb(orig_skb);
	mutex_unlock(&dev->mcu.mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(mt76_mcu_skb_send_and_get_msg);

int __mt76_mcu_send_firmware(struct mt76_dev *dev, int cmd, const void *data,
			     int len, int max_len)
{
	int err, cur_len;

	while (len > 0) {
		cur_len = min_t(int, max_len, len);

		err = mt76_mcu_send_msg(dev, cmd, data, cur_len, false);
		if (err)
			return err;

		data += cur_len;
		len -= cur_len;

		if (dev->queue_ops->tx_cleanup)
			dev->queue_ops->tx_cleanup(dev,
						   dev->q_mcu[MT_MCUQ_FWDL],
						   false);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(__mt76_mcu_send_firmware);

void mt76_dump_mcu_debug_buf(struct mt76_phy *phy) {
	struct mt76_circ_buf *mcu_debug = &phy->mcu_debug;

	mtk_dbg(phy->dev, MCU_DUMP, "Dumping last %d messages sent to firmware:\n",
	        CIRC_CNT(mcu_debug->head, mcu_debug->tail, MT76_MCU_DEBUG_N_MSG));

	for (int i = mcu_debug->tail; i != mcu_debug->head; i = (i + 1) % MT76_MCU_DEBUG_N_MSG) {
		struct mt76_mcu_buf *mcu_buf = &mcu_debug->buf[i];
		char msg_out_buf[MT76_MCU_DEBUG_BUF_SIZE * 3 + 1];
		int j;

		for (j = 0; j < mcu_buf->size; j++) {
			snprintf(msg_out_buf + (3 * j), 4, "%02hhx ", mcu_buf->message[j]);
		}

		msg_out_buf[j * 3] = '\0';

		mtk_dbg(phy->dev, MCU_DUMP, "cmd: %08x  size: %d  msg (hex, LE): %s\n",
		        mcu_buf->command, mcu_buf->size, msg_out_buf);
	}
}
EXPORT_SYMBOL_GPL(mt76_dump_mcu_debug_buf);
