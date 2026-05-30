/* SPDX-License-Identifier: GPL-2.0-only */
// Copyright 2025 Cix Technology Group Co., Ltd.
#ifndef __HW_BREAKPOINT_UNTIL_H
#define __HW_BREAKPOINT_UNTIL_H

#include <linux/vmalloc.h>
#include <linux/soc/cix/plat_hw_breakpoint.h>
#include "../dst_print.h"

typedef struct iophys_info {
	struct list_head list;
	struct vm_struct area;
	u64 virt_addr;
} iophys_info;

void process_cmd_string(char *pBuf, int *pArgc, char *pArgv[]);
/*iophy to virt func*/
iophys_info *get_iophys_info(u64 addr);
void free_iophys_info(iophys_info *info);

void hw_manage_lock(void);
void hw_manage_unlock(void);
struct list_head *hw_get_rules(u64 addr);
void hw_bp_show_all(void);
int hw_proc_init(void);
void hw_proc_exit(void);
void hw_del_all_contion(u64 addr);
bool hw_check_contion(struct list_head *head, const struct pt_regs *regs,
		      const hw_bp_value *value, u64 access);
void hw_show_all_contion(u64 addr);
void hw_until_init(void);

#endif
