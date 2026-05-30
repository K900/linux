// SPDX-License-Identifier: GPL-2.0-only
/*
 * rdr_bootcheck.c
 *
 * rdr startup abnormal monitoring
 *
 * Copyright (c) 2001-2019 Huawei Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/pid.h>
#include <linux/fs.h>
#include <linux/fs_struct.h>
#include <linux/namei.h>
#include <linux/path.h>
#include <linux/string.h>
#include <linux/jiffies.h>
#include <linux/soc/cix/boot_postcode.h>
#include <linux/soc/cix/dst_reboot_reason.h>
#include "rdr_field.h"
#include "rdr_print.h"

#define RDR_NEED_SAVE_MEM 1
#define RDR_DONTNEED_SAVE_MEM 0
#define PSTORE_PATH "/sys/fs/pstore"

/* Max time to wait for userspace init (PID 1) to switch to the real rootfs */
#define RDR_ADOPT_ROOT_TIMEOUT_MS 30000
#define RDR_ADOPT_ROOT_POLL_MS 200

/* Max time to wait for the rootfs to be remounted read-write */
#define RDR_RW_WAIT_TIMEOUT_MS 60000
#define RDR_RW_WAIT_POLL_MS 200

/*
 * This thread is created during late_initcall_sync, before the kernel execs
 * userspace init. On systems that boot via an initramfs, the kthread inherits
 * the early (initramfs) rootfs as its filesystem root. When userspace later
 * does switch_root to the real rootfs, the kthread keeps the OLD root, so any
 * files it writes (e.g. /var/log/cix/bbox/...) land in the discarded initramfs
 * and are invisible to userspace.
 *
 * The initramfs root is always a memory-backed filesystem (rootfs/ramfs/tmpfs);
 * after switch_root PID 1's root is backed by a real filesystem (ext4/erofs/...).
 * Detect the switch by checking whether PID 1's root has left the ramfs type,
 * then give this kthread a private fs_struct and adopt PID 1's root. After this,
 * absolute paths resolve against the real rootfs. If there is no initramfs (our
 * own root is already non-ramfs), the fast path returns immediately.
 */
static bool path_is_ramfs(const struct path *root)
{
	struct super_block *sb = root->dentry->d_sb;
	const char *t = sb->s_type->name;

	return !strcmp(t, "rootfs") || !strcmp(t, "ramfs") || !strcmp(t, "tmpfs");
}

static int rdr_adopt_init_root(void)
{
	struct path early = { .mnt = NULL, .dentry = NULL };
	struct path root = { .mnt = NULL, .dentry = NULL };
	struct task_struct *initp;
	unsigned long deadline;
	int ret = -ENOENT;

	get_fs_root(current->fs, &early);

	/* Fast path: no initramfs — our root is already the real rootfs. */
	if (!path_is_ramfs(&early)) {
		BB_PN("root not ramfs, no initramfs switch needed\n");
		path_put(&early);
		return 0;
	}

	deadline = jiffies + msecs_to_jiffies(RDR_ADOPT_ROOT_TIMEOUT_MS);

	do {
		rcu_read_lock();
		initp = pid_task(find_vpid(1), PIDTYPE_PID);
		if (initp)
			get_task_struct(initp);
		rcu_read_unlock();

		if (initp) {
			task_lock(initp);
			if (initp->fs)
				get_fs_root(initp->fs, &root);
			task_unlock(initp);
			put_task_struct(initp);
		}

		/* PID 1 has left the initramfs once its root is no longer ramfs. */
		if (root.dentry && !path_is_ramfs(&root)) {
			ret = unshare_fs_struct();
			if (!ret) {
				set_fs_root(current->fs, &root);
				set_fs_pwd(current->fs, &root);
				BB_PN("adopted PID 1 root for bbox bootcheck\n");
			} else {
				BB_ERR("unshare_fs_struct failed: %d\n", ret);
			}
			path_put(&root);
			break;
		}

		if (root.dentry) {
			path_put(&root);
			root.dentry = NULL;
			root.mnt = NULL;
		}
		msleep(RDR_ADOPT_ROOT_POLL_MS);
	} while (time_before(jiffies, deadline));

	if (ret)
		BB_PN("kept original root (no rootfs switch detected)\n");

	path_put(&early);
	return ret;
}

/*
 * The kernel command line mounts the rootfs read-only ("ro"); userspace
 * (systemd-remount-fs) only flips it to read-write a bit later in boot. This
 * kthread can reach the saving stage before that happens, in which case every
 * file/dir creation fails with -EROFS. Wait until the filesystem backing our
 * root is writable before attempting to write the crash logs.
 */
static int rdr_wait_rootfs_writable(void)
{
	unsigned long deadline;
	struct path root;
	bool ro = true;

	deadline = jiffies + msecs_to_jiffies(RDR_RW_WAIT_TIMEOUT_MS);

	do {
		if (kern_path("/", LOOKUP_FOLLOW | LOOKUP_DIRECTORY,
			      &root) == 0) {
			ro = sb_rdonly(root.dentry->d_sb);
			path_put(&root);
			if (!ro) {
				BB_PN("rootfs is writable\n");
				return 0;
			}
		}
		msleep(RDR_RW_WAIT_POLL_MS);
	} while (time_before(jiffies, deadline));

	BB_ERR("rootfs still read-only after %dms, log saving may fail\n",
	       RDR_RW_WAIT_TIMEOUT_MS);
	return -EROFS;
}

/*
 * check status of last reboot.
 * return
 * 0 dont need save.
 * 1 need save log.
 */
static int rdr_check_exceptionboot(struct rdr_exception_info_s *info)
{
	u32 temp_reboot_type;
	u32 hw_reboot_type;
	struct rdr_base_info_s *base = NULL;
	struct rdr_struct_s *tmpbb = NULL;

	if (!info) {
		BB_PN();
		return RDR_DONTNEED_SAVE_MEM;
	}

	temp_reboot_type = rdr_get_reboot_type();
	hw_reboot_type = get_hw_reboot_reason();
	BB_PN("reboot_type = 0x%x, hw_reboot_type = 0x%x\n", temp_reboot_type, hw_reboot_type);
	/* If the exception type is normal, do not need to save log */
	if (temp_reboot_type < REBOOT_REASON_LABEL1 ||
	    (temp_reboot_type >= REBOOT_REASON_LABEL4)) {
		return RDR_DONTNEED_SAVE_MEM;
	}

	/* Save the default value of the log after reset */
	info->e_modid = RDR_MODID_AP_ABNORMAL_REBOOT;
	info->e_from_core = RDR_AP;
	info->e_notify_core_mask = RDR_AP;
	info->e_exce_type = temp_reboot_type;
	info->e_exce_subtype = rdr_get_exec_subtype_value();

	tmpbb = rdr_get_head(true);
	if (!tmpbb) {
		return RDR_DONTNEED_SAVE_MEM;
	}
	/* Get the bbox header struct */
	base = &(tmpbb->base_info);

	/* If the log is not saved before resetting, you need to save it again after the reset is started */
	if (base->start_flag != RDR_PROC_EXEC_DONE ||
	    base->savefile_flag != RDR_DUMP_LOG_DONE) {
		BB_ERR("ap error:start[%x],save done[%x]\n", base->start_flag,
		       base->savefile_flag);
		info->e_modid = BBOX_MODID_LAST_SAVE_NOT_DONE;
	} else
		return RDR_DONTNEED_SAVE_MEM;

	BB_ERR("reboot reason: %s-%s\n",
	       rdr_get_exception_type_name(info->e_exce_type),
	       rdr_get_exception_subtype_name(info->e_exce_type,
					      info->e_exce_subtype));
	return RDR_NEED_SAVE_MEM;
}

static void rdr_bootcheck_notify_dump(char *path,
				      struct rdr_exception_info_s *info)
{
	u64 result;

	if (!path || !info) {
		BB_ERR("paramtar is NULL\n");
		return;
	}

	BB_PN("create dump file path:[%s]\n", path);
	while (!rdr_module_is_register(info->e_notify_core_mask)) {
		BB_PN("wait module register. need[0x%llx]\n",
		      info->e_notify_core_mask);
		msleep(1000);
	}

	result = rdr_notify_module_dump(0, info->e_modid, info, path);
	BB_PN("rdr: notify [0x%llx] core dump data done\n", result);
}

static int rdr_save_history_log_back(void)
{
	struct rdr_exception_info_s temp;

	temp.e_from_core = RDR_AP;
	temp.e_reset_core_mask = RDR_AP;
	temp.e_exce_type = rdr_get_reboot_type();
	temp.e_exce_subtype = rdr_get_exec_subtype_value();

	return rdr_save_history_log(&temp, rdr_get_logdir_date(true),
				    DATATIME_MAXLEN, false,
				    get_last_bootup_postcode());
}

static int rdr_bootcheck_thread_body(void *arg)
{
	int cur_reboot_times;
	int ret;
	char *path;
	struct rdr_exception_info_s info;
	struct rdr_syserr_param_s p;
	struct rdr_struct_s *temp_pbb = NULL;
	unsigned int max_reboot_times = rdr_get_reboot_times();

	BB_PR_START();

	(void)rdr_dump_init();

	/*
	 * Make sure our file writes land in the real userspace rootfs, not the
	 * early initramfs root this kthread was created with.
	 */
	(void)rdr_adopt_init_root();

	BB_PN("============wait for fs ready start =============\n");
	while (rdr_wait_partition(PSTORE_PATH, RDR_WAIT_PARTITION_TIME,
				  (S_IFDIR | S_IRUSR)) != 0) {
	}
	BB_PN("============wait for fs ready e n d =============\n");

	/* Rootfs starts read-only ("ro" in cmdline); wait for the rw remount */
	(void)rdr_wait_rootfs_writable();

	if (rdr_check_exceptionboot(&info) != RDR_NEED_SAVE_MEM) {
		BB_PN("need not save dump file when boot\n");
		goto end;
	}

	temp_pbb = rdr_get_head(true);
	if (temp_pbb->base_info.reserve == RDR_UNEXPECTED_REBOOT_MARK_ADDR) {
		cur_reboot_times = rdr_record_reboot_times2file();
		BB_PN("ap has reboot %d times\n", cur_reboot_times);
		if (max_reboot_times < (unsigned int)cur_reboot_times)
			rdr_reset_reboot_times(); /* reset the file of reboot_times */
	} else {
		rdr_reset_reboot_times();
	}

	p.modid = info.e_modid;
	p.arg1 = info.e_from_core;
	p.arg2 = info.e_exce_type;

	ret = rdr_saving_start(true);
	if (ret == -1) {
		BB_ERR("failed to create epath!\n");
		goto end;
	}

	path = rdr_get_logdir_path(true);
	if (IS_ERR_OR_NULL(path))
		goto end;

	rdr_bootcheck_notify_dump(path, &info);
	rdr_save_baseinfo(path, true);
	rdr_save_ramlog(path, RDR_SAVE_RAMLOG, true);
	rdr_save_cleartext(true);

	/* Create a new DONE file under the exception directory, indicating that the exception log is saved */
	bbox_save_done(path, BBOX_SAVE_STEP_DONE);
	rdr_save_history_log_back();
	/* File system sync to ensure read and write tasks are completed */
	rdr_sys_sync();

	BB_PN("saving data done\n");
	rdr_saving_end(true);

end:
	rdr_clear_last_head();
	BB_PR_END();
	return 0;
}

static int __init rdr_bootcheck_init(void)
{
	struct task_struct *rdr_bootcheck;

	rdr_bootcheck =
		kthread_run(rdr_bootcheck_thread_body, NULL, "bbox_bootcheck");
	if (rdr_bootcheck == NULL)
		BB_ERR("create thread rdr_bootcheck_thread faild\n");
	return 0;
}

late_initcall_sync(rdr_bootcheck_init);
