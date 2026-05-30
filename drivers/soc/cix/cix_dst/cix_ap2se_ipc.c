// SPDX-License-Identifier: GPL-2.0
/* Copyright 2024 Cix Technology Group Co., Ltd.*/
/**
 * SoC: CIX SKY1 platform
 * AP to SE IPC interface
 */

#include <linux/soc/cix/cix_ap2se_ipc.h>
#include <linux/printk.h>

int cix_ap2se_ipc_send(uint32_t cmd_id, char *data, size_t len, bool need_reply)
{
	/* Stub implementation - replace with actual IPC mechanism */
	pr_debug("cix_ap2se_ipc_send: cmd_id=0x%x len=%zu need_reply=%d\n",
		 cmd_id, len, need_reply);
	return 0;
}
EXPORT_SYMBOL(cix_ap2se_ipc_send);
