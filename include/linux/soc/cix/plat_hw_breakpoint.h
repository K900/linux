#ifndef __PLAT_HW_BREAKPOINT_H
#define __PLAT_HW_BREAKPOINT_H

#include <linux/sched.h>
#include <asm/ptrace.h>
#include <uapi/linux/perf_event.h>

struct step_hook {
	int (*fn)(struct pt_regs *regs, unsigned long esr);
	struct list_head node;
};

void register_kernel_step_hook(struct step_hook *hook);
void unregister_kernel_step_hook(struct step_hook *hook);

/*struct of get info*/
typedef struct hw_bp_report {
	u32 type; /*bp type*/
	u64 addr; /*The addr of the bp expected to be monitored*/
	u64 len; /*The length of the bp expected to be monitored*/
	u32 mask;
	hw_trigger_times times; /*trigger times*/
} hw_bp_report;

typedef struct hw_bp_info_list {
	struct list_head list; /*list*/
	hw_bp_report *attr; /*bp attr. attr[cpu_id]*/
	int cpu_mask; /*success install of cpu*/
	int cpu_num; /*total cpu num*/
} hw_bp_info_list;

typedef struct hw_bp_ctrl_reg {
	u32 reserved2 : 3, //29~31bit,
		mask : 5, //24~28bit, addr mask，mask=0b11111: (mask2^0b11111 the low bit addr), support 8~2G range
		reserved1 : 3, //21~23bit,
		wt : 1, //20bit, watchpoint type, Unlinked(0)/linked(1) data address match.
		lbn : 4, //16~19bit, WT is only required to be set when setting, which is related to link breakpoints
		ssc : 2, //14,15bit, Security state control, which controls what state will listen for breakpoint events
		hmc : 1, //13bit, Use in conjunction with the above fields
		len : 8, //5~12bit, LBN of len, Each bit represents 1 byte and a maximum of 8 bytes
		type : 2, //3~4bit， bp type wp/bp
		privilege : 2, //1~2bit, The EL level at the time of the last breakpoint setting is used with SSC and HMC
		enabled : 1; //0bit, bp enable
} hw_bp_ctrl_reg;

typedef struct hw_bp_vc {
	u64 address;
	hw_bp_ctrl_reg ctrl;
	u64 trigger;
	u8 access_type;
} hw_bp_vc;

typedef enum hw_bp_condition_type {
	CONDITION_TYPE_NONE = 0,
	CONDITION_TYPE_EQUAL,
	CONDITION_TYPE_NOT_EQUAL,
	CONDITION_TYPE_LESS_THAN,
	CONDITION_TYPE_LESS_THAN_EQUAL,
	CONDITION_TYPE_GREATER_THAN,
	CONDITION_TYPE_GREATER_THAN_EQUAL,
	CONDITION_TYPE_RANGE,
	CONDITION_TYPE_NOT_RANGE,
	CONDITION_TYPE_MAX,
} hw_bp_condition_type;

typedef enum hw_bp_value_type {
	VALUE_TYPE_OLD = 0,
	VALUE_TYPE_NEW,
	VALUE_TYPE_PC,
	VALUE_TYPE_SP,
	VALUE_TYPE_X30,
	VALUE_TYPE_X29,
	VALUE_TYPE_X28,
	VALUE_TYPE_X27,
	VALUE_TYPE_X26,
	VALUE_TYPE_X25,
	VALUE_TYPE_X24,
	VALUE_TYPE_X23,
	VALUE_TYPE_X22,
	VALUE_TYPE_X21,
	VALUE_TYPE_X20,
	VALUE_TYPE_X19,
	VALUE_TYPE_X18,
	VALUE_TYPE_X17,
	VALUE_TYPE_X16,
	VALUE_TYPE_X15,
	VALUE_TYPE_X14,
	VALUE_TYPE_X13,
	VALUE_TYPE_X12,
	VALUE_TYPE_X11,
	VALUE_TYPE_X10,
	VALUE_TYPE_X9,
	VALUE_TYPE_X8,
	VALUE_TYPE_X7,
	VALUE_TYPE_X6,
	VALUE_TYPE_X5,
	VALUE_TYPE_X4,
	VALUE_TYPE_X3,
	VALUE_TYPE_X2,
	VALUE_TYPE_X1,
	VALUE_TYPE_X0,
	VALUE_TYPE_MAX,
} hw_bp_value_type;

typedef struct hw_bp_condition {
	u64 mask;
	u64 condition_value[2];
	hw_bp_condition_type condition;
	hw_bp_value_type value_type;
	u64 bp_type;
} hw_bp_condition;

typedef struct hw_bp_trigger {
	struct list_head list;
	hw_bp_condition rule;
	int magic;
} hw_bp_trigger;

/*install/uninstall*/
int hw_bp_install_from_addr(u64 addr, int len, int type,
			    hw_bp_callback handler);
void hw_bp_uninstall_from_addr(u64 addr);
int hw_bp_install_from_symbol(char *name, int len, int type,
			      hw_bp_callback handler);
void hw_bp_uninstall_from_symbol(char *name);
/*get install bp info*/
hw_bp_info_list *hw_get_bp_infos(void);
void hw_free_bp_infos(hw_bp_info_list *info);
/*add trigger rule*/
int hw_add_contion(u64 addr, hw_bp_condition *cond);
void hw_del_contion(u64 addr, int magic);

#endif
