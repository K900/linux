// SPDX-License-Identifier: GPL-2.0-only
/*
 * Stack tracing support
 *
 * Copyright (C) 2025 CIX Ltd.
 */
#include <linux/syscalls.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/kmsg_dump.h>
#include <linux/pstore.h>
#include "include/rdr_ap_adapter.h"
#include "include/rdr_ap_logbuf.h"
#include "../rdr_print.h"
#include "../rdr_inner.h"

#define LOGMEM_PROP_INIT(name) PROPERTY_INIT(ap_log_##name##_size)
#define IS_PSTORE BIT(0)

/*
 * Fallback size for the kernel-log snapshot region when no
 * "ap_log_dmesg_size" property is provided (e.g. ACPI boot without _DSD
 * entry). Without a backing region the panic dmesg cannot be captured.
 */
#define AP_LOG_DMESG_DEFAULT_SIZE 0x20000

static struct bbox_mem g_logmem[PSTORE_TYPE_MAX];
static struct vfsmount *pstore_mnt;
static bool g_pstore_mounted;
static struct property_table g_logmem_prop[PSTORE_TYPE_MAX] = {
	[PSTORE_TYPE_DMESG] = LOGMEM_PROP_INIT(dmesg),
	[PSTORE_TYPE_CONSOLE] = LOGMEM_PROP_INIT(console)
};

/* Snapshot buffer filled from the kernel log ring in panic context. */
static char g_dmesg_snapshot[AP_LOG_DMESG_DEFAULT_SIZE];

void pstore_dump_mount(void)
{
	struct file_system_type *fs_type;

	g_pstore_mounted = true;
	fs_type = get_fs_type("pstore");
	if (IS_ERR_OR_NULL(fs_type)) {
		BB_ERR("pstore is not exist!\n");
		return;
	}

	pstore_mnt = kern_mount(fs_type);
	if (IS_ERR_OR_NULL(pstore_mnt))
		BB_ERR("pstore mount fail!\n");
	else
		BB_PN("pstore mount success!\n");
	put_filesystem(fs_type);
}

/*
 * Snapshot the kernel log into the reserved log region on panic. Runs in
 * atomic/panic context, so it must not sleep or allocate: it copies into a
 * preallocated static buffer and then into the reserved memory pool.
 */
static void ap_panic_kmsg_dump(struct kmsg_dumper *dumper,
			       struct kmsg_dump_detail *detail)
{
	struct kmsg_dump_iter iter;
	size_t len = 0;

	if (detail->reason != KMSG_DUMP_PANIC)
		return;

	kmsg_dump_rewind(&iter);
	if (!kmsg_dump_get_buffer(&iter, true, g_dmesg_snapshot,
				  sizeof(g_dmesg_snapshot), &len))
		return;

	logmem_add(PSTORE_TYPE_DMESG, g_dmesg_snapshot, (u32)len);

	/*
	 * Flush the reserved region to DRAM so the snapshot survives the
	 * upcoming warm reset (the region is mapped write-back cacheable).
	 */
	rdr_flush_total_mem();
}

static struct kmsg_dumper g_ap_panic_dumper = {
	.dump = ap_panic_kmsg_dump,
	.max_reason = KMSG_DUMP_PANIC,
};

int pstore_dump_init(struct platform_device *pdev,
		     struct rdr_safemem_pool *pool)
{
	int ret;

	(void)ap_prop_table_init(&pdev->dev, g_logmem_prop,
				 ARRAY_SIZE(g_logmem_prop));

	/*
	 * Fall back to a default DMESG region size when the platform does not
	 * provide "ap_log_dmesg_size" (e.g. ACPI boot), so the panic kernel
	 * log can always be captured.
	 */
	if (g_logmem_prop[PSTORE_TYPE_DMESG].size == 0)
		g_logmem_prop[PSTORE_TYPE_DMESG].size = AP_LOG_DMESG_DEFAULT_SIZE;

	for (int i = 0; i < PSTORE_TYPE_MAX; i++) {
		if (g_logmem_prop[i].size == 0)
			continue;
		if (rdr_safemem_alloc(pool, MEMID_PSTORE + i,
				      g_logmem_prop[i].size, &g_logmem[i])) {
			BB_ERR("there is no enough space for modu [%d] to dump mem!\n",
			       i);
			break;
		}
		BB_DBG("logmem_addr [0x%px] logmem_size [0x%llx]!\n",
		       g_logmem[i].vaddr, g_logmem[i].size);
	}

	/* Register a panic dumper to snapshot the kernel log into reserved mem. */
	if (!IS_ERR_OR_NULL(g_logmem[PSTORE_TYPE_DMESG].vaddr)) {
		ret = kmsg_dump_register(&g_ap_panic_dumper);
		if (ret)
			BB_ERR("register panic kmsg dumper failed: %d\n", ret);
		else
			BB_PN("panic kmsg dumper registered\n");
	}

	return 0;
}

void logmem_add(enum pstore_type_id id, void *buf, u32 size)
{
	u32 offset = sizeof(struct pstore_head);
	u32 cp_size = 0;
	void *cp_addr;
	struct pstore_head *lhead = NULL;

	if (id >= PSTORE_TYPE_MAX || IS_ERR_OR_NULL(g_logmem[id].vaddr) ||
	    g_logmem[id].size <= offset)
		return;

	cp_size = min(size, (u32)(g_logmem[id].size - offset));
	lhead = g_logmem[id].vaddr;

	if (lhead->flag == IS_PSTORE)
		return;
	lhead->flag = IS_PSTORE;
	lhead->type = id;
	lhead->size = cp_size;
	cp_addr = buf + size - cp_size;
	if (g_logmem[id].size) {
		BB_PN("add %s log ok, orign size: %d, cp_size: %d\n",
		      pstore_type_to_name(id), size, cp_size);
		memcpy(g_logmem[id].vaddr + offset, cp_addr, cp_size);
	}
}
EXPORT_SYMBOL(logmem_add);

/*Cleartext will only occur after pstore is mounted.*/
int ap_pstore_cleartext(const char *dir_path, u64 log_addr, u32 log_len)
{
	struct ap_eh_root *head = (struct ap_eh_root *)(uintptr_t)log_addr;
	bool is_last = rdr_log_save_is_last();
	struct pstore_head *lhead;
	struct bbox_mem mem;
	struct file *fp;
	ssize_t ret = 0;

	if (!g_pstore_mounted)
		return -EPERM;

	for (int i = 0; i < PSTORE_TYPE_MAX; i++) {
		if (g_logmem[i].size == 0)
			continue;

		/*
		 * On recovery (is_last), the live AP region has already been
		 * wiped by rdr_ap_register_core() before we get here, so the
		 * panic snapshot only survives in the backup copy passed via
		 * log_addr. Locate the pstore region inside that backup the
		 * same way the other AP cleartext handlers do. For the live
		 * (non-recovery) path keep reading g_logmem directly.
		 */
		if (is_last) {
			if (IS_ERR_OR_NULL(head))
				continue;
			if (rdr_safemem_get(&head->pool, MEMID_PSTORE + i, &mem))
				continue;
			lhead = get_addr_from_root(head, mem.vaddr);
		} else {
			lhead = g_logmem[i].vaddr;
		}

		if (IS_ERR_OR_NULL(lhead) || lhead->flag != IS_PSTORE)
			continue;

		fp = bbox_cleartext_get_filep(
			dir_path, (char *)pstore_type_to_name(lhead->type));
		if (IS_ERR_OR_NULL(fp))
			continue;

		ret = kernel_write(fp, (char *)lhead + sizeof(*lhead),
				   lhead->size, &(fp->f_pos));
		if (ret != lhead->size)
			BB_PN("%s write %ld bytes is not equal %u bytes\n",
			      (char *)pstore_type_to_name(lhead->type), ret,
			      lhead->size);
		bbox_cleartext_end_filep(fp);

		/* Only the live region is safe to clear. */
		if (!is_last)
			memset(g_logmem[i].vaddr, 0, g_logmem[i].size);
	}

	return 0;
}
