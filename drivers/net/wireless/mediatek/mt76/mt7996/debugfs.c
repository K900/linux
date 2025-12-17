// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#include <linux/relay.h>
#include "mt7996.h"
#include "eeprom.h"
#include "mcu.h"
#include "mac.h"
#include "wtbl.h"

#define FW_BIN_LOG_MAGIC	0x44d9c99a

#define NO_SHIFT_DEFINE 0xFFFFFFFF
#define BITS(m, n)              (~(BIT(m)-1) & ((BIT(n) - 1) | BIT(n)))

#define GET_FIELD(_field, _reg)	\
	({	\
		(((_reg) & (_field##_MASK)) >> (_field##_SHIFT));	\
	})

/* WTBL INFO */
static int
mt7996_wtbl_read_raw(struct mt7996_dev *dev, u16 idx,
		     enum mt7996_wtbl_type type, u16 start_dw,
		     u16 len, void *buf)
{
	u32 *dest_cpy = (u32 *)buf;
	u32 size_dw = len;
	u32 src = 0;

	if (!buf)
		return 0xFF;

	if (type == WTBL_TYPE_LMAC) {
		mt76_wr(dev, MT_DBG_WTBLON_TOP_WDUCR_ADDR,
			FIELD_PREP(MT_DBG_WTBLON_TOP_WDUCR_GROUP, (idx >> 7)));
		src = LWTBL_IDX2BASE(idx, start_dw);
	} else if (type == WTBL_TYPE_UMAC) {
		mt76_wr(dev,  MT_DBG_UWTBL_TOP_WDUCR_ADDR,
			FIELD_PREP(MT_DBG_UWTBL_TOP_WDUCR_GROUP, (idx >> 7)));
		src = UWTBL_IDX2BASE(idx, start_dw);
	} else if (type == WTBL_TYPE_KEY) {
		mt76_wr(dev,  MT_DBG_UWTBL_TOP_WDUCR_ADDR,
			MT_DBG_UWTBL_TOP_WDUCR_TARGET |
			FIELD_PREP(MT_DBG_UWTBL_TOP_WDUCR_GROUP, (idx >> 7)));
		src = KEYTBL_IDX2BASE(idx, start_dw);
	}

	while (size_dw--) {
		*dest_cpy++ = mt76_rr(dev, src);
		src += 4;
	};

	return 0;
}

#if 0
static int
mt7996_wtbl_write_raw(struct mt7996_dev *dev, u16 idx,
			  enum mt7996_wtbl_type type, u16 start_dw,
			  u32 val)
{
	u32 addr = 0;

	if (type == WTBL_TYPE_LMAC) {
		mt76_wr(dev, MT_DBG_WTBLON_TOP_WDUCR_ADDR,
			FIELD_PREP(MT_DBG_WTBLON_TOP_WDUCR_GROUP, (idx >> 7)));
		addr = LWTBL_IDX2BASE(idx, start_dw);
	} else if (type == WTBL_TYPE_UMAC) {
		mt76_wr(dev, MT_DBG_UWTBL_TOP_WDUCR_ADDR,
			FIELD_PREP(MT_DBG_UWTBL_TOP_WDUCR_GROUP, (idx >> 7)));
		addr = UWTBL_IDX2BASE(idx, start_dw);
	} else if (type == WTBL_TYPE_KEY) {
		mt76_wr(dev, MT_DBG_UWTBL_TOP_WDUCR_ADDR,
			MT_DBG_UWTBL_TOP_WDUCR_TARGET |
			FIELD_PREP(MT_DBG_UWTBL_TOP_WDUCR_GROUP, (idx >> 7)));
		addr = KEYTBL_IDX2BASE(idx, start_dw);
	}

	mt76_wr(dev, addr, val);

	return 0;
}
#endif

static const struct berse_wtbl_parse WTBL_LMAC_DW0[] = {
	{"MUAR_IDX",	WF_LWTBL_MUAR_MASK,	WF_LWTBL_MUAR_SHIFT,	false},
	{"RCA1",	WF_LWTBL_RCA1_MASK,	NO_SHIFT_DEFINE,	false},
	{"KID",		WF_LWTBL_KID_MASK,	WF_LWTBL_KID_SHIFT,	false},
	{"RCID",	WF_LWTBL_RCID_MASK,	NO_SHIFT_DEFINE,	false},
	{"BAND",	WF_LWTBL_BAND_MASK,	WF_LWTBL_BAND_SHIFT,	false},
	{"RV",		WF_LWTBL_RV_MASK,	NO_SHIFT_DEFINE,	false},
	{"RCA2",	WF_LWTBL_RCA2_MASK,	NO_SHIFT_DEFINE,	false},
	{"WPI_FLAG",	WF_LWTBL_WPI_FLAG_MASK,	NO_SHIFT_DEFINE,	true},
	{NULL,}
};

static void parse_fmac_lwtbl_dw0_1(struct seq_file *s, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u16 i = 0;

	seq_printf(s, "\t\n");
	seq_printf(s, "LinkAddr: %02x:%02x:%02x:%02x:%02x:%02x(D0[B0~15], D1[B0~31])\n",
		lwtbl[4], lwtbl[5], lwtbl[6], lwtbl[7], lwtbl[0], lwtbl[1]);

	/* LMAC WTBL DW 0 */
	seq_printf(s, "\t\n");
	seq_printf(s, "LWTBL DW 0/1\n");
	addr = (u32 *)&(lwtbl[WTBL_GROUP_PEER_INFO_DW_0*4]);
	dw_value = *addr;

	while (WTBL_LMAC_DW0[i].name) {

		if (WTBL_LMAC_DW0[i].shift == NO_SHIFT_DEFINE)
			seq_printf(s, "\t%s:%d\n", WTBL_LMAC_DW0[i].name,
					 (dw_value & WTBL_LMAC_DW0[i].mask) ? 1 : 0);
		else
			seq_printf(s, "\t%s:%u\n", WTBL_LMAC_DW0[i].name,
					  (dw_value & WTBL_LMAC_DW0[i].mask) >> WTBL_LMAC_DW0[i].shift);
		i++;
	}
}

static const struct berse_wtbl_parse WTBL_LMAC_DW2_7996[] = {
	{"AID",			WF_LWTBL_AID_MASK,		WF_LWTBL_AID_SHIFT,			false},
	{"GID_SU",		WF_LWTBL_GID_SU_MASK,		NO_SHIFT_DEFINE,			false},
	{"SPP_EN",		WF_LWTBL_SPP_EN_MASK,		NO_SHIFT_DEFINE,			false},
	{"WPI_EVEN",		WF_LWTBL_WPI_EVEN_MASK,		NO_SHIFT_DEFINE,			false},
	{"AAD_OM",		WF_LWTBL_AAD_OM_MASK,		NO_SHIFT_DEFINE,			false},
	{"CIPHER_PGTK",		WF_LWTBL_CIPHER_SUIT_PGTK_MASK,	WF_LWTBL_CIPHER_SUIT_PGTK_SHIFT,	true},
	{"FROM_DS",		WF_LWTBL_FD_MASK,		NO_SHIFT_DEFINE,			false},
	{"TO_DS",		WF_LWTBL_TD_MASK,		NO_SHIFT_DEFINE,			false},
	{"SW",			WF_LWTBL_SW_MASK,		NO_SHIFT_DEFINE,			false},
	{"UL",			WF_LWTBL_UL_MASK,		NO_SHIFT_DEFINE,			false},
	{"TX_POWER_SAVE",	WF_LWTBL_TX_PS_MASK,		NO_SHIFT_DEFINE,			true},
	{"QOS",			WF_LWTBL_QOS_MASK,		NO_SHIFT_DEFINE,			false},
	{"HT",			WF_LWTBL_HT_MASK,		NO_SHIFT_DEFINE,			false},
	{"VHT",			WF_LWTBL_VHT_MASK,		NO_SHIFT_DEFINE,			false},
	{"HE",			WF_LWTBL_HE_MASK,		NO_SHIFT_DEFINE,			false},
	{"EHT",			WF_LWTBL_EHT_MASK,		NO_SHIFT_DEFINE,			false},
	{"MESH",		WF_LWTBL_MESH_MASK,		NO_SHIFT_DEFINE,			true},
	{NULL,}
};
static const struct berse_wtbl_parse *WTBL_LMAC_DW2 = WTBL_LMAC_DW2_7996;

static const struct berse_wtbl_parse WTBL_LMAC_DW2_7992[] = {
	{"AID",			WF_LWTBL_AID_MASK,		WF_LWTBL_AID_SHIFT,			false},
	{"GID_SU",		WF_LWTBL_GID_SU_MASK,		NO_SHIFT_DEFINE,			false},
	{"DUAL_PTEC_EN",	WF_LWTBL_DUAL_PTEC_EN_MASK,	NO_SHIFT_DEFINE,			false},
	{"DUAL_CTS_CAP",	WF_LWTBL_DUAL_CTS_CAP_MASK,	NO_SHIFT_DEFINE,			false},
	{"CIPHER_PGTK",		WF_LWTBL_CIPHER_SUIT_PGTK_MASK,	WF_LWTBL_CIPHER_SUIT_PGTK_SHIFT,	true},
	{"FROM_DS",		WF_LWTBL_FD_MASK,		NO_SHIFT_DEFINE,			false},
	{"TO_DS",		WF_LWTBL_TD_MASK,		NO_SHIFT_DEFINE,			false},
	{"SW",			WF_LWTBL_SW_MASK,		NO_SHIFT_DEFINE,			false},
	{"UL",			WF_LWTBL_UL_MASK,		NO_SHIFT_DEFINE,			false},
	{"TX_POWER_SAVE",	WF_LWTBL_TX_PS_MASK,		NO_SHIFT_DEFINE,			true},
	{"QOS",			WF_LWTBL_QOS_MASK,		NO_SHIFT_DEFINE,			false},
	{"HT",			WF_LWTBL_HT_MASK,		NO_SHIFT_DEFINE,			false},
	{"VHT",			WF_LWTBL_VHT_MASK,		NO_SHIFT_DEFINE,			false},
	{"HE",			WF_LWTBL_HE_MASK,		NO_SHIFT_DEFINE,			false},
	{"EHT",			WF_LWTBL_EHT_MASK,		NO_SHIFT_DEFINE,			false},
	{"MESH",		WF_LWTBL_MESH_MASK,		NO_SHIFT_DEFINE,			true},
	{NULL,}
};

static void parse_fmac_lwtbl_dw2(struct seq_file *s, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u16 i = 0;

	/* LMAC WTBL DW 2 */
	seq_printf(s, "\t\n");
	seq_printf(s, "LWTBL DW 2\n");
	addr = (u32 *)&(lwtbl[WTBL_GROUP_TRX_CAP_DW_2*4]);
	dw_value = *addr;

	while (WTBL_LMAC_DW2[i].name) {

		if (WTBL_LMAC_DW2[i].shift == NO_SHIFT_DEFINE)
			seq_printf(s, "\t%s:%d\n", WTBL_LMAC_DW2[i].name,
					 (dw_value & WTBL_LMAC_DW2[i].mask) ? 1 : 0);
		else
			seq_printf(s, "\t%s:%u\n", WTBL_LMAC_DW2[i].name,
					  (dw_value & WTBL_LMAC_DW2[i].mask) >> WTBL_LMAC_DW2[i].shift);
		i++;
	}
}

static const struct berse_wtbl_parse WTBL_LMAC_DW3[] = {
	{"WMM_Q",		WF_LWTBL_WMM_Q_MASK,			WF_LWTBL_WMM_Q_SHIFT,			false},
	{"EHT_SIG_MCS",		WF_LWTBL_EHT_SIG_MCS_MASK,		WF_LWTBL_EHT_SIG_MCS_SHIFT,		false},
	{"HDRT_MODE",		WF_LWTBL_HDRT_MODE_MASK,		NO_SHIFT_DEFINE,			false},
	{"BEAM_CHG",		WF_LWTBL_BEAM_CHG_MASK,			NO_SHIFT_DEFINE,			false},
	{"EHT_LTF_SYM_NUM",	WF_LWTBL_EHT_LTF_SYM_NUM_OPT_MASK,	WF_LWTBL_EHT_LTF_SYM_NUM_OPT_SHIFT,	true},
	{"PFMU_IDX",		WF_LWTBL_PFMU_IDX_MASK,			WF_LWTBL_PFMU_IDX_SHIFT,		false},
	{"ULPF_IDX",		WF_LWTBL_ULPF_IDX_MASK,			WF_LWTBL_ULPF_IDX_SHIFT,		false},
	{"RIBF",		WF_LWTBL_RIBF_MASK,			NO_SHIFT_DEFINE,			false},
	{"ULPF",		WF_LWTBL_ULPF_MASK,			NO_SHIFT_DEFINE,			false},
	{"BYPASS_TXSMM",	WF_LWTBL_BYPASS_TXSMM_MASK,		NO_SHIFT_DEFINE,			true},
	{"TBF_HT",		WF_LWTBL_TBF_HT_MASK,			NO_SHIFT_DEFINE,			false},
	{"TBF_VHT",		WF_LWTBL_TBF_VHT_MASK,			NO_SHIFT_DEFINE,			false},
	{"TBF_HE",		WF_LWTBL_TBF_HE_MASK,			NO_SHIFT_DEFINE,			false},
	{"TBF_EHT",		WF_LWTBL_TBF_EHT_MASK,			NO_SHIFT_DEFINE,			false},
	{"IGN_FBK",		WF_LWTBL_IGN_FBK_MASK,			NO_SHIFT_DEFINE,			true},
	{NULL,}
};

static void parse_fmac_lwtbl_dw3(struct seq_file *s, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u16 i = 0;

	/* LMAC WTBL DW 3 */
	seq_printf(s, "\t\n");
	seq_printf(s, "LWTBL DW 3\n");
	addr = (u32 *)&(lwtbl[WTBL_GROUP_TRX_CAP_DW_3*4]);
	dw_value = *addr;

	while (WTBL_LMAC_DW3[i].name) {

		if (WTBL_LMAC_DW3[i].shift == NO_SHIFT_DEFINE)
			seq_printf(s, "\t%s:%d\n", WTBL_LMAC_DW3[i].name,
					 (dw_value & WTBL_LMAC_DW3[i].mask) ? 1 : 0);
		else
			seq_printf(s, "\t%s:%u\n", WTBL_LMAC_DW3[i].name,
					  (dw_value & WTBL_LMAC_DW3[i].mask) >> WTBL_LMAC_DW3[i].shift);
		i++;
	}
}

static const struct berse_wtbl_parse WTBL_LMAC_DW4[] = {
	{"NEGOTIATED_WINSIZE0",	WF_LWTBL_NEGOTIATED_WINSIZE0_MASK,	WF_LWTBL_NEGOTIATED_WINSIZE0_SHIFT,	false},
	{"WINSIZE1",		WF_LWTBL_NEGOTIATED_WINSIZE1_MASK,	WF_LWTBL_NEGOTIATED_WINSIZE1_SHIFT,	false},
	{"WINSIZE2",		WF_LWTBL_NEGOTIATED_WINSIZE2_MASK,	WF_LWTBL_NEGOTIATED_WINSIZE2_SHIFT,	false},
	{"WINSIZE3",		WF_LWTBL_NEGOTIATED_WINSIZE3_MASK,	WF_LWTBL_NEGOTIATED_WINSIZE3_SHIFT,	true},
	{"WINSIZE4",		WF_LWTBL_NEGOTIATED_WINSIZE4_MASK,	WF_LWTBL_NEGOTIATED_WINSIZE4_SHIFT,	false},
	{"WINSIZE5",		WF_LWTBL_NEGOTIATED_WINSIZE5_MASK,	WF_LWTBL_NEGOTIATED_WINSIZE5_SHIFT,	false},
	{"WINSIZE6",		WF_LWTBL_NEGOTIATED_WINSIZE6_MASK,	WF_LWTBL_NEGOTIATED_WINSIZE6_SHIFT,	false},
	{"WINSIZE7",		WF_LWTBL_NEGOTIATED_WINSIZE7_MASK,	WF_LWTBL_NEGOTIATED_WINSIZE7_SHIFT,	true},
	{"PE",			WF_LWTBL_PE_MASK,			WF_LWTBL_PE_SHIFT,			false},
	{"DIS_RHTR",		WF_LWTBL_DIS_RHTR_MASK,			NO_SHIFT_DEFINE,			false},
	{"LDPC_HT",		WF_LWTBL_LDPC_HT_MASK,			NO_SHIFT_DEFINE,			false},
	{"LDPC_VHT",		WF_LWTBL_LDPC_VHT_MASK,			NO_SHIFT_DEFINE,			false},
	{"LDPC_HE",		WF_LWTBL_LDPC_HE_MASK,			NO_SHIFT_DEFINE,			false},
	{"LDPC_EHT",		WF_LWTBL_LDPC_EHT_MASK,			NO_SHIFT_DEFINE,			true},
	{"BA_MODE",		WF_LWTBL_BA_MODE_MASK,			NO_SHIFT_DEFINE,			true},
	{NULL,}
};

static void parse_fmac_lwtbl_dw4(struct seq_file *s, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u16 i = 0;

	/* LMAC WTBL DW 4 */
	seq_printf(s, "\t\n");
	seq_printf(s, "LWTBL DW 4\n");
	addr = (u32 *)&(lwtbl[WTBL_GROUP_TRX_CAP_DW_4*4]);
	dw_value = *addr;

	while (WTBL_LMAC_DW4[i].name) {
		if (WTBL_LMAC_DW4[i].shift == NO_SHIFT_DEFINE)
			seq_printf(s, "\t%s:%d\n", WTBL_LMAC_DW4[i].name,
					 (dw_value & WTBL_LMAC_DW4[i].mask) ? 1 : 0);
		else
			seq_printf(s, "\t%s:%u\n", WTBL_LMAC_DW4[i].name,
					  (dw_value & WTBL_LMAC_DW4[i].mask) >> WTBL_LMAC_DW4[i].shift);
		i++;
	}
}

static const struct berse_wtbl_parse WTBL_LMAC_DW5_7996[] = {
	{"AF",			WF_LWTBL_AF_MASK,		WF_LWTBL_AF_SHIFT,		false},
	{"AF_HE",		WF_LWTBL_AF_HE_MASK,		WF_LWTBL_AF_HE_SHIFT,		false},
	{"RTS",			WF_LWTBL_RTS_MASK,		NO_SHIFT_DEFINE,		false},
	{"SMPS",		WF_LWTBL_SMPS_MASK,		NO_SHIFT_DEFINE,		false},
	{"DYN_BW",		WF_LWTBL_DYN_BW_MASK,		NO_SHIFT_DEFINE,		true},
	{"MMSS",		WF_LWTBL_MMSS_MASK,		WF_LWTBL_MMSS_SHIFT,		false},
	{"USR",			WF_LWTBL_USR_MASK,		NO_SHIFT_DEFINE,		false},
	{"SR_RATE",		WF_LWTBL_SR_R_MASK,		WF_LWTBL_SR_R_SHIFT,		false},
	{"SR_ABORT",		WF_LWTBL_SR_ABORT_MASK,		NO_SHIFT_DEFINE,		true},
	{"TX_POWER_OFFSET",	WF_LWTBL_TX_POWER_OFFSET_MASK,  WF_LWTBL_TX_POWER_OFFSET_SHIFT,	false},
	{"LTF_EHT",		WF_LWTBL_LTF_EHT_MASK,		WF_LWTBL_LTF_EHT_SHIFT, 	false},
	{"GI_EHT",		WF_LWTBL_GI_EHT_MASK,		WF_LWTBL_GI_EHT_SHIFT,		false},
	{"DOPPL",		WF_LWTBL_DOPPL_MASK,		NO_SHIFT_DEFINE,		false},
	{"TXOP_PS_CAP",		WF_LWTBL_TXOP_PS_CAP_MASK,	NO_SHIFT_DEFINE,		false},
	{"DONOT_UPDATE_I_PSM",	WF_LWTBL_DU_I_PSM_MASK,		NO_SHIFT_DEFINE,		true},
	{"I_PSM",		WF_LWTBL_I_PSM_MASK,		NO_SHIFT_DEFINE,		false},
	{"PSM",			WF_LWTBL_PSM_MASK,		NO_SHIFT_DEFINE,		false},
	{"SKIP_TX",		WF_LWTBL_SKIP_TX_MASK,		NO_SHIFT_DEFINE,		true},
	{NULL,}
};
static const struct berse_wtbl_parse *WTBL_LMAC_DW5 = WTBL_LMAC_DW5_7996;

static const struct berse_wtbl_parse WTBL_LMAC_DW5_7992[] = {
	{"AF",			WF_LWTBL_AF_MASK_7992,		WF_LWTBL_AF_SHIFT,		false},
	{"RTS",			WF_LWTBL_RTS_MASK,		NO_SHIFT_DEFINE,		false},
	{"SMPS",		WF_LWTBL_SMPS_MASK,		NO_SHIFT_DEFINE,		false},
	{"DYN_BW",		WF_LWTBL_DYN_BW_MASK,		NO_SHIFT_DEFINE,		true},
	{"MMSS",		WF_LWTBL_MMSS_MASK,		WF_LWTBL_MMSS_SHIFT,		false},
	{"USR",			WF_LWTBL_USR_MASK,		NO_SHIFT_DEFINE,		false},
	{"SR_RATE",		WF_LWTBL_SR_R_MASK,		WF_LWTBL_SR_R_SHIFT,		false},
	{"SR_ABORT",		WF_LWTBL_SR_ABORT_MASK,		NO_SHIFT_DEFINE,		true},
	{"TX_POWER_OFFSET",	WF_LWTBL_TX_POWER_OFFSET_MASK,	WF_LWTBL_TX_POWER_OFFSET_SHIFT,	false},
	{"LTF_EHT",		WF_LWTBL_LTF_EHT_MASK,		WF_LWTBL_LTF_EHT_SHIFT,		false},
	{"GI_EHT",		WF_LWTBL_GI_EHT_MASK,		WF_LWTBL_GI_EHT_SHIFT,		false},
	{"DOPPL",		WF_LWTBL_DOPPL_MASK,		NO_SHIFT_DEFINE,		false},
	{"TXOP_PS_CAP",		WF_LWTBL_TXOP_PS_CAP_MASK,	NO_SHIFT_DEFINE,		false},
	{"DONOT_UPDATE_I_PSM",	WF_LWTBL_DU_I_PSM_MASK,		NO_SHIFT_DEFINE,		true},
	{"I_PSM",		WF_LWTBL_I_PSM_MASK,		NO_SHIFT_DEFINE,		false},
	{"PSM",			WF_LWTBL_PSM_MASK,		NO_SHIFT_DEFINE,		false},
	{"SKIP_TX",		WF_LWTBL_SKIP_TX_MASK,		NO_SHIFT_DEFINE,		true},
	{NULL,}
};

static void parse_fmac_lwtbl_dw5(struct seq_file *s, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u16 i = 0;

	/* LMAC WTBL DW 5 */
	seq_printf(s, "\t\n");
	seq_printf(s, "LWTBL DW 5\n");
	addr = (u32 *)&(lwtbl[WTBL_GROUP_TRX_CAP_DW_5*4]);
	dw_value = *addr;

	while (WTBL_LMAC_DW5[i].name) {
		if (WTBL_LMAC_DW5[i].shift == NO_SHIFT_DEFINE)
			seq_printf(s, "\t%s:%d\n", WTBL_LMAC_DW5[i].name,
					 (dw_value & WTBL_LMAC_DW5[i].mask) ? 1 : 0);
		else
			seq_printf(s, "\t%s:%u\n", WTBL_LMAC_DW5[i].name,
					  (dw_value & WTBL_LMAC_DW5[i].mask) >> WTBL_LMAC_DW5[i].shift);
		i++;
	}
}

static const struct berse_wtbl_parse WTBL_LMAC_DW6[] = {
	{"CBRN",	WF_LWTBL_CBRN_MASK,	WF_LWTBL_CBRN_SHIFT,	false},
	{"DBNSS_EN",	WF_LWTBL_DBNSS_EN_MASK,	NO_SHIFT_DEFINE,	false},
	{"BAF_EN",	WF_LWTBL_BAF_EN_MASK,	NO_SHIFT_DEFINE,	false},
	{"RDGBA",	WF_LWTBL_RDGBA_MASK,	NO_SHIFT_DEFINE,	false},
	{"RDG",		WF_LWTBL_R_MASK,	NO_SHIFT_DEFINE,	false},
	{"SPE_IDX",	WF_LWTBL_SPE_IDX_MASK,	WF_LWTBL_SPE_IDX_SHIFT,	true},
	{"G2",		WF_LWTBL_G2_MASK,	NO_SHIFT_DEFINE,	false},
	{"G4",		WF_LWTBL_G4_MASK,	NO_SHIFT_DEFINE,	false},
	{"G8",		WF_LWTBL_G8_MASK,	NO_SHIFT_DEFINE,	false},
	{"G16",		WF_LWTBL_G16_MASK,	NO_SHIFT_DEFINE,	true},
	{"G2_LTF",	WF_LWTBL_G2_LTF_MASK,	WF_LWTBL_G2_LTF_SHIFT,	false},
	{"G4_LTF",	WF_LWTBL_G4_LTF_MASK,	WF_LWTBL_G4_LTF_SHIFT,	false},
	{"G8_LTF",	WF_LWTBL_G8_LTF_MASK,	WF_LWTBL_G8_LTF_SHIFT,	false},
	{"G16_LTF",	WF_LWTBL_G16_LTF_MASK,	WF_LWTBL_G16_LTF_SHIFT,	true},
	{"G2_HE",	WF_LWTBL_G2_HE_MASK,	WF_LWTBL_G2_HE_SHIFT,	false},
	{"G4_HE",	WF_LWTBL_G4_HE_MASK,	WF_LWTBL_G4_HE_SHIFT,	false},
	{"G8_HE",	WF_LWTBL_G8_HE_MASK,	WF_LWTBL_G8_HE_SHIFT,	false},
	{"G16_HE",	WF_LWTBL_G16_HE_MASK,	WF_LWTBL_G16_HE_SHIFT,	true},
	{NULL,}
};

static void parse_fmac_lwtbl_dw6(struct seq_file *s, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u16 i = 0;

	/* LMAC WTBL DW 6 */
	seq_printf(s, "\t\n");
	seq_printf(s, "LWTBL DW 6\n");
	addr = (u32 *)&(lwtbl[WTBL_GROUP_TRX_CAP_DW_6*4]);
	dw_value = *addr;

	while (WTBL_LMAC_DW6[i].name) {
		if (WTBL_LMAC_DW6[i].shift == NO_SHIFT_DEFINE)
			seq_printf(s, "\t%s:%d\n", WTBL_LMAC_DW6[i].name,
					 (dw_value & WTBL_LMAC_DW6[i].mask) ? 1 : 0);
		else
			seq_printf(s, "\t%s:%u\n", WTBL_LMAC_DW6[i].name,
					  (dw_value & WTBL_LMAC_DW6[i].mask) >> WTBL_LMAC_DW6[i].shift);
		i++;
	}
}

static void parse_fmac_lwtbl_dw7(struct seq_file *s, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	int i = 0;

	/* LMAC WTBL DW 7 */
	seq_printf(s, "\t\n");
	seq_printf(s, "LWTBL DW 7\n");
	addr = (u32 *)&(lwtbl[WTBL_GROUP_TRX_CAP_DW_7*4]);
	dw_value = *addr;

	for (i = 0; i < 8; i++) {
		seq_printf(s, "\tBA_WIN_SIZE%u:%lu\n", i, ((dw_value & BITS(i*4, i*4+3)) >> i*4));
	}
}

static const struct berse_wtbl_parse WTBL_LMAC_DW8[] = {
	{"RTS_FAIL_CNT_AC0",	WF_LWTBL_AC0_RTS_FAIL_CNT_MASK,	WF_LWTBL_AC0_RTS_FAIL_CNT_SHIFT,	false},
	{"AC1",			WF_LWTBL_AC1_RTS_FAIL_CNT_MASK,	WF_LWTBL_AC1_RTS_FAIL_CNT_SHIFT,	false},
	{"AC2",			WF_LWTBL_AC2_RTS_FAIL_CNT_MASK,	WF_LWTBL_AC2_RTS_FAIL_CNT_SHIFT,	false},
	{"AC3",			WF_LWTBL_AC3_RTS_FAIL_CNT_MASK,	WF_LWTBL_AC3_RTS_FAIL_CNT_SHIFT,	true},
	{"PARTIAL_AID",		WF_LWTBL_PARTIAL_AID_MASK,	WF_LWTBL_PARTIAL_AID_SHIFT,		false},
	{"CHK_PER",		WF_LWTBL_CHK_PER_MASK,		NO_SHIFT_DEFINE,			true},
	{NULL,}
};

static void parse_fmac_lwtbl_dw8(struct seq_file *s, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u16 i = 0;

	/* LMAC WTBL DW 8 */
	seq_printf(s, "\t\n");
	seq_printf(s, "LWTBL DW 8\n");
	addr = (u32 *)&(lwtbl[WTBL_GROUP_TRX_CAP_DW_8*4]);
	dw_value = *addr;

	while (WTBL_LMAC_DW8[i].name) {
		if (WTBL_LMAC_DW8[i].shift == NO_SHIFT_DEFINE)
			seq_printf(s, "\t%s:%d\n", WTBL_LMAC_DW8[i].name,
					 (dw_value & WTBL_LMAC_DW8[i].mask) ? 1 : 0);
		else
			seq_printf(s, "\t%s:%u\n", WTBL_LMAC_DW8[i].name,
					  (dw_value & WTBL_LMAC_DW8[i].mask) >> WTBL_LMAC_DW8[i].shift);
		i++;
	}
}

static const struct berse_wtbl_parse WTBL_LMAC_DW9_7996[] = {
	{"RX_AVG_MPDU_SIZE",	WF_LWTBL_RX_AVG_MPDU_SIZE_MASK,	WF_LWTBL_RX_AVG_MPDU_SIZE_SHIFT,	false},
	{"PRITX_SW_MODE",	WF_LWTBL_PRITX_SW_MODE_MASK,	NO_SHIFT_DEFINE,			false},
	{"PRITX_ERSU",		WF_LWTBL_PRITX_ERSU_MASK,	NO_SHIFT_DEFINE,			false},
	{"PRITX_PLR",		WF_LWTBL_PRITX_PLR_MASK,	NO_SHIFT_DEFINE,			true},
	{"PRITX_DCM",		WF_LWTBL_PRITX_DCM_MASK,	NO_SHIFT_DEFINE,			false},
	{"PRITX_ER106T",	WF_LWTBL_PRITX_ER106T_MASK,	NO_SHIFT_DEFINE,			true},
	/* {"FCAP(0:20 1:~40)",	WTBL_FCAP_20_TO_160_MHZ,	WTBL_FCAP_20_TO_160_MHZ_OFFSET}, */
	{"MPDU_FAIL_CNT",	WF_LWTBL_MPDU_FAIL_CNT_MASK,	WF_LWTBL_MPDU_FAIL_CNT_SHIFT,		false},
	{"MPDU_OK_CNT",		WF_LWTBL_MPDU_OK_CNT_MASK,	WF_LWTBL_MPDU_OK_CNT_SHIFT,		false},
	{"RATE_IDX",		WF_LWTBL_RATE_IDX_MASK,		WF_LWTBL_RATE_IDX_SHIFT,		true},
	{NULL,}
};
static const struct berse_wtbl_parse *WTBL_LMAC_DW9 = WTBL_LMAC_DW9_7996;

static const struct berse_wtbl_parse WTBL_LMAC_DW9_7992[] = {
	{"RX_AVG_MPDU_SIZE",	WF_LWTBL_RX_AVG_MPDU_SIZE_MASK,		WF_LWTBL_RX_AVG_MPDU_SIZE_SHIFT,	false},
	{"PRITX_SW_MODE",	WF_LWTBL_PRITX_SW_MODE_MASK_7992,	NO_SHIFT_DEFINE,			false},
	{"PRITX_ERSU",		WF_LWTBL_PRITX_ERSU_MASK_7992,		NO_SHIFT_DEFINE,			false},
	{"PRITX_PLR",		WF_LWTBL_PRITX_PLR_MASK_7992,		NO_SHIFT_DEFINE,			true},
	{"PRITX_DCM",		WF_LWTBL_PRITX_DCM_MASK,		NO_SHIFT_DEFINE,			false},
	{"PRITX_ER106T",	WF_LWTBL_PRITX_ER106T_MASK,		NO_SHIFT_DEFINE,			true},
	/* {"FCAP(0:20 1:~40)",	WTBL_FCAP_20_TO_160_MHZ,		WTBL_FCAP_20_TO_160_MHZ_OFFSET}, */
	{"MPDU_FAIL_CNT",	WF_LWTBL_MPDU_FAIL_CNT_MASK,		WF_LWTBL_MPDU_FAIL_CNT_SHIFT,		false},
	{"MPDU_OK_CNT",		WF_LWTBL_MPDU_OK_CNT_MASK,		WF_LWTBL_MPDU_OK_CNT_SHIFT,		false},
	{"RATE_IDX",		WF_LWTBL_RATE_IDX_MASK,			WF_LWTBL_RATE_IDX_SHIFT,		true},
	{NULL,}
};

char *fcap_name[] = {"20MHz", "20/40MHz", "20/40/80MHz", "20/40/80/160/80+80MHz", "20/40/80/160/80+80/320MHz"};

static void parse_fmac_lwtbl_dw9(struct seq_file *s, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u16 i = 0;

	/* LMAC WTBL DW 9 */
	seq_printf(s, "\t\n");
	seq_printf(s, "LWTBL DW 9\n");
	addr = (u32 *)&(lwtbl[WTBL_GROUP_TRX_CAP_DW_9*4]);
	dw_value = *addr;

	while (WTBL_LMAC_DW9[i].name) {
		if (WTBL_LMAC_DW9[i].shift == NO_SHIFT_DEFINE)
			seq_printf(s, "\t%s:%d\n", WTBL_LMAC_DW9[i].name,
					 (dw_value & WTBL_LMAC_DW9[i].mask) ? 1 : 0);
		else
			seq_printf(s, "\t%s:%u\n", WTBL_LMAC_DW9[i].name,
					  (dw_value & WTBL_LMAC_DW9[i].mask) >> WTBL_LMAC_DW9[i].shift);
		i++;
	}

	/* FCAP parser */
	seq_printf(s, "\t\n");
	seq_printf(s, "FCAP:%s\n", fcap_name[(dw_value & WF_LWTBL_FCAP_MASK) >> WF_LWTBL_FCAP_SHIFT]);
}

#define HW_TX_RATE_TO_MODE(_x)			(((_x) & WTBL_RATE_TX_MODE_MASK) >> WTBL_RATE_TX_MODE_OFFSET)
#define HW_TX_RATE_TO_MCS(_x, _mode)		((_x) & WTBL_RATE_TX_RATE_MASK >> WTBL_RATE_TX_RATE_OFFSET)
#define HW_TX_RATE_TO_NSS(_x)			(((_x) & WTBL_RATE_NSTS_MASK) >> WTBL_RATE_NSTS_OFFSET)
#define HW_TX_RATE_TO_STBC(_x)			(((_x) & WTBL_RATE_STBC_MASK) >> WTBL_RATE_STBC_OFFSET)

static char *HW_TX_MODE_STR[] = {"CCK", "OFDM", "HT-Mix", "HT-GF", "VHT",
				 "N/A", "N/A", "N/A",
				 "HE_SU", "HE_EXT_SU", "HE_TRIG", "HE_MU",
				 "N/A",
				 "EHT_EXT_SU", "EHT_TRIG", "EHT_MU"};
static char *HW_TX_RATE_CCK_STR[] = {"1M", "2Mlong", "5.5Mlong", "11Mlong", "N/A", "2Mshort", "5.5Mshort", "11Mshort", "N/A"};
static char *HW_TX_RATE_OFDM_STR[] = {"6M", "9M", "12M", "18M", "24M", "36M", "48M", "54M", "N/A"};

static char *hw_rate_ofdm_str(uint16_t ofdm_idx)
{
	switch (ofdm_idx) {
	case 11: /* 6M */
		return HW_TX_RATE_OFDM_STR[0];

	case 15: /* 9M */
		return HW_TX_RATE_OFDM_STR[1];

	case 10: /* 12M */
		return HW_TX_RATE_OFDM_STR[2];

	case 14: /* 18M */
		return HW_TX_RATE_OFDM_STR[3];

	case 9: /* 24M */
		return HW_TX_RATE_OFDM_STR[4];

	case 13: /* 36M */
		return HW_TX_RATE_OFDM_STR[5];

	case 8: /* 48M */
		return HW_TX_RATE_OFDM_STR[6];

	case 12: /* 54M */
		return HW_TX_RATE_OFDM_STR[7];

	default:
		return HW_TX_RATE_OFDM_STR[8];
	}
}

static char *hw_rate_str(u8 mode, uint16_t rate_idx)
{
	if (mode == 0)
		return rate_idx < 8 ? HW_TX_RATE_CCK_STR[rate_idx] : HW_TX_RATE_CCK_STR[8];
	else if (mode == 1)
		return hw_rate_ofdm_str(rate_idx);
	else
		return "MCS";
}

static void
parse_rate(struct seq_file *s, uint16_t rate_idx, uint16_t txrate)
{
	uint16_t txmode, mcs, nss, stbc;

	txmode = HW_TX_RATE_TO_MODE(txrate);
	mcs = HW_TX_RATE_TO_MCS(txrate, txmode);
	nss = HW_TX_RATE_TO_NSS(txrate);
	stbc = HW_TX_RATE_TO_STBC(txrate);

	seq_printf(s, "\tRate%d(0x%x):TxMode=%d(%s), TxRate=%d(%s), Nsts=%d, STBC=%d\n",
			  rate_idx + 1, txrate,
			  txmode, HW_TX_MODE_STR[txmode],
			  mcs, hw_rate_str(txmode, mcs), nss, stbc);
}


static const struct berse_wtbl_parse WTBL_LMAC_DW10[] = {
	{"RATE1",	WF_LWTBL_RATE1_MASK,	WF_LWTBL_RATE1_SHIFT},
	{"RATE2",	WF_LWTBL_RATE2_MASK,	WF_LWTBL_RATE2_SHIFT},
	{NULL,}
};

static void parse_fmac_lwtbl_dw10(struct seq_file *s, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u16 i = 0;

	/* LMAC WTBL DW 10 */
	seq_printf(s, "\t\n");
	seq_printf(s, "LWTBL DW 10\n");
	addr = (u32 *)&(lwtbl[WTBL_GROUP_AUTO_RATE_1_2*4]);
	dw_value = *addr;

	while (WTBL_LMAC_DW10[i].name) {
		parse_rate(s, i, (dw_value & WTBL_LMAC_DW10[i].mask) >> WTBL_LMAC_DW10[i].shift);
		i++;
	}
}

static const struct berse_wtbl_parse WTBL_LMAC_DW11[] = {
	{"RATE3",	WF_LWTBL_RATE3_MASK,	WF_LWTBL_RATE3_SHIFT},
	{"RATE4",	WF_LWTBL_RATE4_MASK,	WF_LWTBL_RATE4_SHIFT},
	{NULL,}
};

static void parse_fmac_lwtbl_dw11(struct seq_file *s, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u16 i = 0;

	/* LMAC WTBL DW 11 */
	seq_printf(s, "\t\n");
	seq_printf(s, "LWTBL DW 11\n");
	addr = (u32 *)&(lwtbl[WTBL_GROUP_AUTO_RATE_3_4*4]);
	dw_value = *addr;

	while (WTBL_LMAC_DW11[i].name) {
		parse_rate(s, i+2, (dw_value & WTBL_LMAC_DW11[i].mask) >> WTBL_LMAC_DW11[i].shift);
		i++;
	}
}

static const struct berse_wtbl_parse WTBL_LMAC_DW12[] = {
	{"RATE5",	WF_LWTBL_RATE5_MASK,	WF_LWTBL_RATE5_SHIFT},
	{"RATE6",	WF_LWTBL_RATE6_MASK,	WF_LWTBL_RATE6_SHIFT},
	{NULL,}
};

static void parse_fmac_lwtbl_dw12(struct seq_file *s, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u16 i = 0;

	/* LMAC WTBL DW 12 */
	seq_printf(s, "\t\n");
	seq_printf(s, "LWTBL DW 12\n");
	addr = (u32 *)&(lwtbl[WTBL_GROUP_AUTO_RATE_5_6*4]);
	dw_value = *addr;

	while (WTBL_LMAC_DW12[i].name) {
		parse_rate(s, i+4, (dw_value & WTBL_LMAC_DW12[i].mask) >> WTBL_LMAC_DW12[i].shift);
		i++;
	}
}

static const struct berse_wtbl_parse WTBL_LMAC_DW13[] = {
	{"RATE7",	WF_LWTBL_RATE7_MASK,	WF_LWTBL_RATE7_SHIFT},
	{"RATE8",	WF_LWTBL_RATE8_MASK,	WF_LWTBL_RATE8_SHIFT},
	{NULL,}
};

static void parse_fmac_lwtbl_dw13(struct seq_file *s, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u16 i = 0;

	/* LMAC WTBL DW 13 */
	seq_printf(s, "\t\n");
	seq_printf(s, "LWTBL DW 13\n");
	addr = (u32 *)&(lwtbl[WTBL_GROUP_AUTO_RATE_7_8*4]);
	dw_value = *addr;

	while (WTBL_LMAC_DW13[i].name) {
		parse_rate(s, i+6, (dw_value & WTBL_LMAC_DW13[i].mask) >> WTBL_LMAC_DW13[i].shift);
		i++;
	}
}

static const struct berse_wtbl_parse WTBL_LMAC_DW14_BMC[] = {
	{"CIPHER_IGTK",		WF_LWTBL_CIPHER_SUIT_IGTK_MASK,		WF_LWTBL_CIPHER_SUIT_IGTK_SHIFT,	false},
	{"CIPHER_BIGTK",	WF_LWTBL_CIPHER_SUIT_BIGTK_MASK,	WF_LWTBL_CIPHER_SUIT_BIGTK_SHIFT,	true},
	{NULL,}
};

static const struct berse_wtbl_parse WTBL_LMAC_DW14[] = {
	{"RATE1_TX_CNT",	WF_LWTBL_RATE1_TX_CNT_MASK,	WF_LWTBL_RATE1_TX_CNT_SHIFT,	false},
	{"RATE1_FAIL_CNT",	WF_LWTBL_RATE1_FAIL_CNT_MASK,	WF_LWTBL_RATE1_FAIL_CNT_SHIFT,	true},
	{NULL,}
};

static void parse_fmac_lwtbl_dw14(struct seq_file *s, u8 *lwtbl)
{
	u32 *addr, *muar_addr = 0;
	u32 dw_value, muar_dw_value = 0;
	u16 i = 0;

	/* DUMP DW14 for BMC entry only */
	muar_addr = (u32 *)&(lwtbl[WF_LWTBL_MUAR_DW*4]);
	muar_dw_value = *muar_addr;
	if (((muar_dw_value & WF_LWTBL_MUAR_MASK) >> WF_LWTBL_MUAR_SHIFT)
		== MUAR_INDEX_OWN_MAC_ADDR_BC_MC) {
		/* LMAC WTBL DW 14 */
		seq_printf(s, "\t\n");
		seq_printf(s, "LWTBL DW 14\n");
		addr = (u32 *)&(lwtbl[WF_LWTBL_CIPHER_SUIT_IGTK_DW*4]);
		dw_value = *addr;

		while (WTBL_LMAC_DW14_BMC[i].name) {
			if (WTBL_LMAC_DW14_BMC[i].shift == NO_SHIFT_DEFINE)
				seq_printf(s, "\t%s:%d\n", WTBL_LMAC_DW14_BMC[i].name,
					(dw_value & WTBL_LMAC_DW14_BMC[i].mask) ? 1 : 0);
			else
				seq_printf(s, "\t%s:%u\n", WTBL_LMAC_DW14_BMC[i].name,
					(dw_value & WTBL_LMAC_DW14_BMC[i].mask) >> WTBL_LMAC_DW14_BMC[i].shift);
			i++;
		}
	} else {
		seq_printf(s, "\t\n");
		seq_printf(s, "LWTBL DW 14\n");
		addr = (u32 *)&(lwtbl[WF_LWTBL_CIPHER_SUIT_IGTK_DW*4]);
		dw_value = *addr;

		while (WTBL_LMAC_DW14[i].name) {
			if (WTBL_LMAC_DW14[i].shift == NO_SHIFT_DEFINE)
				seq_printf(s, "\t%s:%d\n", WTBL_LMAC_DW14[i].name,
					(dw_value & WTBL_LMAC_DW14[i].mask) ? 1 : 0);
			else
				seq_printf(s, "\t%s:%u\n", WTBL_LMAC_DW14[i].name,
					(dw_value & WTBL_LMAC_DW14[i].mask) >> WTBL_LMAC_DW14[i].shift);
			i++;
		}
	}
}

static const struct berse_wtbl_parse WTBL_LMAC_DW28[] = {
	{"RELATED_IDX0",	WF_LWTBL_RELATED_IDX0_MASK,		WF_LWTBL_RELATED_IDX0_SHIFT,		false},
	{"RELATED_BAND0",	WF_LWTBL_RELATED_BAND0_MASK,		WF_LWTBL_RELATED_BAND0_SHIFT,		false},
	{"PRI_MLD_BAND",	WF_LWTBL_PRIMARY_MLD_BAND_MASK,		WF_LWTBL_PRIMARY_MLD_BAND_SHIFT,	true},
	{"RELATED_IDX1",	WF_LWTBL_RELATED_IDX1_MASK,		WF_LWTBL_RELATED_IDX1_SHIFT,		false},
	{"RELATED_BAND1",	WF_LWTBL_RELATED_BAND1_MASK,		WF_LWTBL_RELATED_BAND1_SHIFT,		false},
	{"SEC_MLD_BAND",	WF_LWTBL_SECONDARY_MLD_BAND_MASK,	WF_LWTBL_SECONDARY_MLD_BAND_SHIFT,	true},
	{NULL,}
};

static void parse_fmac_lwtbl_dw28(struct seq_file *s, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u16 i = 0;

	/* LMAC WTBL DW 28 */
	seq_printf(s, "\t\n");
	seq_printf(s, "LWTBL DW 28\n");
	addr = (u32 *)&(lwtbl[WTBL_GROUP_MLO_INFO_LINE_1*4]);
	dw_value = *addr;

	while (WTBL_LMAC_DW28[i].name) {
		if (WTBL_LMAC_DW28[i].shift == NO_SHIFT_DEFINE)
			seq_printf(s, "\t%s:%d\n", WTBL_LMAC_DW28[i].name,
				(dw_value & WTBL_LMAC_DW28[i].mask) ? 1 : 0);
		else
			seq_printf(s, "\t%s:%u\n", WTBL_LMAC_DW28[i].name,
				(dw_value & WTBL_LMAC_DW28[i].mask) >>
					WTBL_LMAC_DW28[i].shift);
		i++;
	}
}

static const struct berse_wtbl_parse WTBL_LMAC_DW29[] = {
	{"DISPATCH_POLICY_MLD_TID0",	WF_LWTBL_DISPATCH_POLICY0_MASK,		WF_LWTBL_DISPATCH_POLICY0_SHIFT,	false},
	{"MLD_TID1",			WF_LWTBL_DISPATCH_POLICY1_MASK,		WF_LWTBL_DISPATCH_POLICY1_SHIFT,	false},
	{"MLD_TID2",			WF_LWTBL_DISPATCH_POLICY2_MASK,		WF_LWTBL_DISPATCH_POLICY2_SHIFT,	false},
	{"MLD_TID3",			WF_LWTBL_DISPATCH_POLICY3_MASK,		WF_LWTBL_DISPATCH_POLICY3_SHIFT,	true},
	{"MLD_TID4",			WF_LWTBL_DISPATCH_POLICY4_MASK,		WF_LWTBL_DISPATCH_POLICY4_SHIFT,	false},
	{"MLD_TID5",			WF_LWTBL_DISPATCH_POLICY5_MASK,		WF_LWTBL_DISPATCH_POLICY5_SHIFT,	false},
	{"MLD_TID6",			WF_LWTBL_DISPATCH_POLICY6_MASK,		WF_LWTBL_DISPATCH_POLICY6_SHIFT,	false},
	{"MLD_TID7",			WF_LWTBL_DISPATCH_POLICY7_MASK,		WF_LWTBL_DISPATCH_POLICY7_SHIFT,	true},
	{"OMLD_ID",			WF_LWTBL_OWN_MLD_ID_MASK,		WF_LWTBL_OWN_MLD_ID_SHIFT,		false},
	{"EMLSR0",			WF_LWTBL_EMLSR0_MASK,			NO_SHIFT_DEFINE,			false},
	{"EMLMR0",			WF_LWTBL_EMLMR0_MASK,			NO_SHIFT_DEFINE,			false},
	{"EMLSR1",			WF_LWTBL_EMLSR1_MASK,			NO_SHIFT_DEFINE,			false},
	{"EMLMR1",			WF_LWTBL_EMLMR1_MASK,			NO_SHIFT_DEFINE,			true},
	{"EMLSR2",			WF_LWTBL_EMLSR2_MASK,			NO_SHIFT_DEFINE,			false},
	{"EMLMR2",			WF_LWTBL_EMLMR2_MASK,			NO_SHIFT_DEFINE,			false},
	{"STR_BITMAP",			WF_LWTBL_STR_BITMAP_MASK,		WF_LWTBL_STR_BITMAP_SHIFT,		true},
	{NULL,}
};

static void parse_fmac_lwtbl_dw29(struct seq_file *s, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u16 i = 0;

	/* LMAC WTBL DW 29 */
	seq_printf(s, "\t\n");
	seq_printf(s, "LWTBL DW 29\n");
	addr = (u32 *)&(lwtbl[WTBL_GROUP_MLO_INFO_LINE_2*4]);
	dw_value = *addr;

	while (WTBL_LMAC_DW29[i].name) {
		if (WTBL_LMAC_DW29[i].shift == NO_SHIFT_DEFINE)
			seq_printf(s, "\t%s:%d\n", WTBL_LMAC_DW29[i].name,
				(dw_value & WTBL_LMAC_DW29[i].mask) ? 1 : 0);
		else
			seq_printf(s, "\t%s:%u\n", WTBL_LMAC_DW29[i].name,
				(dw_value & WTBL_LMAC_DW29[i].mask) >>
					WTBL_LMAC_DW29[i].shift);
		i++;
	}
}

static const struct berse_wtbl_parse WTBL_LMAC_DW30[] = {
	{"DISPATCH_ORDER",	WF_LWTBL_DISPATCH_ORDER_MASK,	WF_LWTBL_DISPATCH_ORDER_SHIFT,	false},
	{"DISPATCH_RATIO",	WF_LWTBL_DISPATCH_RATIO_MASK,	WF_LWTBL_DISPATCH_RATIO_SHIFT,	false},
	{"LINK_MGF",		WF_LWTBL_LINK_MGF_MASK,		WF_LWTBL_LINK_MGF_SHIFT,	true},
	{NULL,}
};

static void parse_fmac_lwtbl_dw30(struct seq_file *s, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u16 i = 0;

	/* LMAC WTBL DW 30 */
	seq_printf(s, "\t\n");
	seq_printf(s, "LWTBL DW 30\n");
	addr = (u32 *)&(lwtbl[WTBL_GROUP_MLO_INFO_LINE_3*4]);
	dw_value = *addr;


	while (WTBL_LMAC_DW30[i].name) {
		if (WTBL_LMAC_DW30[i].shift == NO_SHIFT_DEFINE)
			seq_printf(s, "\t%s:%d\n", WTBL_LMAC_DW30[i].name,
				(dw_value & WTBL_LMAC_DW30[i].mask) ? 1 : 0);
		else
			seq_printf(s, "\t%s:%u\n", WTBL_LMAC_DW30[i].name,
				(dw_value & WTBL_LMAC_DW30[i].mask) >> WTBL_LMAC_DW30[i].shift);
		i++;
	}
}

static const struct berse_wtbl_parse WTBL_LMAC_DW31[] = {
	{"BFTX_TB",		WF_LWTBL_BFTX_TB_MASK,		NO_SHIFT_DEFINE,		false},
	{"DROP",		WF_LWTBL_DROP_MASK,		NO_SHIFT_DEFINE,		false},
	{"CASCAD",		WF_LWTBL_CASCAD_MASK,		NO_SHIFT_DEFINE,		false},
	{"ALL_ACK",		WF_LWTBL_ALL_ACK_MASK,		NO_SHIFT_DEFINE,		false},
	{"MPDU_SIZE",		WF_LWTBL_MPDU_SIZE_MASK,	WF_LWTBL_MPDU_SIZE_SHIFT,	false},
	{"RXD_DUP_MODE",	WF_LWTBL_RXD_DUP_MODE_MASK,	WF_LWTBL_RXD_DUP_MODE_SHIFT,	true},
	{"ACK_EN",		WF_LWTBL_ACK_EN_MASK,		NO_SHIFT_DEFINE,		true},
	{NULL,}
};

static void parse_fmac_lwtbl_dw31(struct seq_file *s, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u16 i = 0;

	/* LMAC WTBL DW 31 */
	seq_printf(s, "\t\n");
	seq_printf(s, "LWTBL DW 31\n");
	addr = (u32 *)&(lwtbl[WTBL_GROUP_RESP_INFO_DW_31*4]);
	dw_value = *addr;

	while (WTBL_LMAC_DW31[i].name) {
		if (WTBL_LMAC_DW31[i].shift == NO_SHIFT_DEFINE)
			seq_printf(s, "\t%s:%d\n", WTBL_LMAC_DW31[i].name,
				(dw_value & WTBL_LMAC_DW31[i].mask) ? 1 : 0);
		else
			seq_printf(s, "\t%s:%u\n", WTBL_LMAC_DW31[i].name,
				(dw_value & WTBL_LMAC_DW31[i].mask) >>
					WTBL_LMAC_DW31[i].shift);
		i++;
	}
}

static const struct berse_wtbl_parse WTBL_LMAC_DW32[] = {
	{"OM_INFO",			WF_LWTBL_OM_INFO_MASK,			WF_LWTBL_OM_INFO_SHIFT,		false},
	{"OM_INFO_EHT",         WF_LWTBL_OM_INFO_EHT_MASK,         WF_LWTBL_OM_INFO_EHT_SHIFT,  false},
	{"RXD_DUP_FOR_OM_CHG",		WF_LWTBL_RXD_DUP_FOR_OM_CHG_MASK,	NO_SHIFT_DEFINE,		false},
	{"RXD_DUP_WHITE_LIST",	WF_LWTBL_RXD_DUP_WHITE_LIST_MASK,	WF_LWTBL_RXD_DUP_WHITE_LIST_SHIFT,	false},
	{NULL,}
};

static void parse_fmac_lwtbl_dw32(struct seq_file *s, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u16 i = 0;

	/* LMAC WTBL DW 32 */
	seq_printf(s, "\t\n");
	seq_printf(s, "LWTBL DW 32\n");
	addr = (u32 *)&(lwtbl[WTBL_GROUP_RX_DUP_INFO_DW_32*4]);
	dw_value = *addr;

	while (WTBL_LMAC_DW32[i].name) {
		if (WTBL_LMAC_DW32[i].shift == NO_SHIFT_DEFINE)
			seq_printf(s, "\t%s:%d\n", WTBL_LMAC_DW32[i].name,
				(dw_value & WTBL_LMAC_DW32[i].mask) ? 1 : 0);
		else
			seq_printf(s, "\t%s:%u\n", WTBL_LMAC_DW32[i].name,
				(dw_value & WTBL_LMAC_DW32[i].mask) >>
					WTBL_LMAC_DW32[i].shift);
		i++;
	}
}

static const struct berse_wtbl_parse WTBL_LMAC_DW33[] = {
	{"USER_RSSI",			WF_LWTBL_USER_RSSI_MASK,		WF_LWTBL_USER_RSSI_SHIFT,		false},
	{"USER_SNR",			WF_LWTBL_USER_SNR_MASK,			WF_LWTBL_USER_SNR_SHIFT,		false},
	{"RAPID_REACTION_RATE",		WF_LWTBL_RAPID_REACTION_RATE_MASK,	WF_LWTBL_RAPID_REACTION_RATE_SHIFT,	true},
	{"HT_AMSDU(Read Only)",		WF_LWTBL_HT_AMSDU_MASK,			NO_SHIFT_DEFINE,			false},
	{"AMSDU_CROSS_LG(Read Only)",	WF_LWTBL_AMSDU_CROSS_LG_MASK,		NO_SHIFT_DEFINE,			true},
	{NULL,}
};

static void parse_fmac_lwtbl_dw33(struct seq_file *s, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u16 i = 0;

	/* LMAC WTBL DW 33 */
	seq_printf(s, "\t\n");
	seq_printf(s, "LWTBL DW 33\n");
	addr = (u32 *)&(lwtbl[WTBL_GROUP_RX_STAT_CNT_LINE_1*4]);
	dw_value = *addr;

	while (WTBL_LMAC_DW33[i].name) {
		if (WTBL_LMAC_DW33[i].shift == NO_SHIFT_DEFINE)
			seq_printf(s, "\t%s:%d\n", WTBL_LMAC_DW33[i].name,
				(dw_value & WTBL_LMAC_DW33[i].mask) ? 1 : 0);
		else
			seq_printf(s, "\t%s:%u\n", WTBL_LMAC_DW33[i].name,
				(dw_value & WTBL_LMAC_DW33[i].mask) >>
					WTBL_LMAC_DW33[i].shift);
		i++;
	}
}

static const struct berse_wtbl_parse WTBL_LMAC_DW34[] = {
	{"RESP_RCPI0",	WF_LWTBL_RESP_RCPI0_MASK,	WF_LWTBL_RESP_RCPI0_SHIFT,	false},
	{"RCPI1",	WF_LWTBL_RESP_RCPI1_MASK,	WF_LWTBL_RESP_RCPI1_SHIFT,	false},
	{"RCPI2",	WF_LWTBL_RESP_RCPI2_MASK,	WF_LWTBL_RESP_RCPI2_SHIFT,	false},
	{"RCPI3",	WF_LWTBL_RESP_RCPI3_MASK,	WF_LWTBL_RESP_RCPI3_SHIFT,	true},
	{NULL,}
};

static void parse_fmac_lwtbl_dw34(struct seq_file *s, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u16 i = 0;

	/* LMAC WTBL DW 34 */
	seq_printf(s, "\t\n");
	seq_printf(s, "LWTBL DW 34\n");
	addr = (u32 *)&(lwtbl[WTBL_GROUP_RX_STAT_CNT_LINE_2*4]);
	dw_value = *addr;


	while (WTBL_LMAC_DW34[i].name) {
		if (WTBL_LMAC_DW34[i].shift == NO_SHIFT_DEFINE)
			seq_printf(s, "\t%s:%d\n", WTBL_LMAC_DW34[i].name,
				(dw_value & WTBL_LMAC_DW34[i].mask) ? 1 : 0);
		else
			seq_printf(s, "\t%s:%u\n", WTBL_LMAC_DW34[i].name,
				(dw_value & WTBL_LMAC_DW34[i].mask) >>
					WTBL_LMAC_DW34[i].shift);
		i++;
	}
}

static const struct berse_wtbl_parse WTBL_LMAC_DW35[] = {
	{"SNR 0",	WF_LWTBL_SNR_RX0_MASK,		WF_LWTBL_SNR_RX0_SHIFT,		false},
	{"SNR 1",	WF_LWTBL_SNR_RX1_MASK,		WF_LWTBL_SNR_RX1_SHIFT,		false},
	{"SNR 2",	WF_LWTBL_SNR_RX2_MASK,		WF_LWTBL_SNR_RX2_SHIFT,		false},
	{"SNR 3",	WF_LWTBL_SNR_RX3_MASK,		WF_LWTBL_SNR_RX3_SHIFT,		true},
	{NULL,}
};

static void parse_fmac_lwtbl_dw35(struct seq_file *s, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u16 i = 0;

	/* LMAC WTBL DW 35 */
	seq_printf(s, "\t\n");
	seq_printf(s, "LWTBL DW 35\n");
	addr = (u32 *)&(lwtbl[WTBL_GROUP_RX_STAT_CNT_LINE_3*4]);
	dw_value = *addr;


	while (WTBL_LMAC_DW35[i].name) {
		if (WTBL_LMAC_DW35[i].shift == NO_SHIFT_DEFINE)
			seq_printf(s, "\t%s:%d\n", WTBL_LMAC_DW35[i].name,
				(dw_value & WTBL_LMAC_DW35[i].mask) ? 1 : 0);
		else
			seq_printf(s, "\t%s:%u\n", WTBL_LMAC_DW35[i].name,
				(dw_value & WTBL_LMAC_DW35[i].mask) >>
					WTBL_LMAC_DW35[i].shift);
		i++;
	}
}

static void parse_fmac_lwtbl_rx_stats(struct seq_file *s, u8 *lwtbl)
{
	parse_fmac_lwtbl_dw33(s, lwtbl);
	parse_fmac_lwtbl_dw34(s, lwtbl);
	parse_fmac_lwtbl_dw35(s, lwtbl);
}

static void parse_fmac_lwtbl_mlo_info(struct seq_file *s, u8 *lwtbl)
{
	parse_fmac_lwtbl_dw28(s, lwtbl);
	parse_fmac_lwtbl_dw29(s, lwtbl);
	parse_fmac_lwtbl_dw30(s, lwtbl);
}

static const struct berse_wtbl_parse WTBL_UMAC_DW9[] = {
	{"RELATED_IDX0",	WF_UWTBL_RELATED_IDX0_MASK,		WF_UWTBL_RELATED_IDX0_SHIFT,		false},
	{"RELATED_BAND0",	WF_UWTBL_RELATED_BAND0_MASK,		WF_UWTBL_RELATED_BAND0_SHIFT,		false},
	{"PRI_MLD_BAND",	WF_UWTBL_PRIMARY_MLD_BAND_MASK,		WF_UWTBL_PRIMARY_MLD_BAND_SHIFT,	true},
	{"RELATED_IDX1",	WF_UWTBL_RELATED_IDX1_MASK,		WF_UWTBL_RELATED_IDX1_SHIFT,		false},
	{"RELATED_BAND1",	WF_UWTBL_RELATED_BAND1_MASK,		WF_UWTBL_RELATED_BAND1_SHIFT,		false},
	{"SEC_MLD_BAND",	WF_UWTBL_SECONDARY_MLD_BAND_MASK,	WF_UWTBL_SECONDARY_MLD_BAND_SHIFT,	true},
	{NULL,}
};

static void parse_fmac_uwtbl_mlo_info(struct seq_file *s, u8 *uwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u16 i = 0;

	seq_printf(s, "\t\n");
	seq_printf(s, "MldAddr: %02x:%02x:%02x:%02x:%02x:%02x(D0[B0~15], D1[B0~31])\n",
		uwtbl[4], uwtbl[5], uwtbl[6], uwtbl[7], uwtbl[0], uwtbl[1]);

	/* UMAC WTBL DW 0 */
	seq_printf(s, "\t\n");
	seq_printf(s, "UWTBL DW 0\n");
	addr = (u32 *)&(uwtbl[WF_UWTBL_OWN_MLD_ID_DW*4]);
	dw_value = *addr;

	seq_printf(s, "\t%s:%u\n", "OMLD_ID",
		(dw_value & WF_UWTBL_OWN_MLD_ID_MASK) >> WF_UWTBL_OWN_MLD_ID_SHIFT);

	/* UMAC WTBL DW 9 */
	seq_printf(s, "\t\n");
	seq_printf(s, "UWTBL DW 9\n");
	addr = (u32 *)&(uwtbl[WF_UWTBL_RELATED_IDX0_DW*4]);
	dw_value = *addr;

	while (WTBL_UMAC_DW9[i].name) {

		if (WTBL_UMAC_DW9[i].shift == NO_SHIFT_DEFINE)
			seq_printf(s, "\t%s:%d\n", WTBL_UMAC_DW9[i].name,
				(dw_value & WTBL_UMAC_DW9[i].mask) ? 1 : 0);
		else
			seq_printf(s, "\t%s:%u\n", WTBL_UMAC_DW9[i].name,
				 (dw_value & WTBL_UMAC_DW9[i].mask) >>
					WTBL_UMAC_DW9[i].shift);
		i++;
	}
}

static bool
is_wtbl_bigtk_exist(u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;

	addr = (u32 *)&(lwtbl[WF_LWTBL_MUAR_DW*4]);
	dw_value = *addr;
	if (((dw_value & WF_LWTBL_MUAR_MASK) >> WF_LWTBL_MUAR_SHIFT) ==
					MUAR_INDEX_OWN_MAC_ADDR_BC_MC) {
		addr = (u32 *)&(lwtbl[WF_LWTBL_CIPHER_SUIT_BIGTK_DW*4]);
		dw_value = *addr;
		if (((dw_value & WF_LWTBL_CIPHER_SUIT_BIGTK_MASK) >>
			WF_LWTBL_CIPHER_SUIT_BIGTK_SHIFT) != IGTK_CIPHER_SUIT_NONE)
			return true;
	}

	return false;
}

static const struct berse_wtbl_parse WTBL_UMAC_DW2[] = {
	{"PN0",		WTBL_PN0_MASK,		WTBL_PN0_OFFSET,	false},
	{"PN1",		WTBL_PN1_MASK,		WTBL_PN1_OFFSET,	false},
	{"PN2",		WTBL_PN2_MASK,		WTBL_PN2_OFFSET,	true},
	{"PN3",		WTBL_PN3_MASK,		WTBL_PN3_OFFSET,	false},
	{NULL,}
};

static const struct berse_wtbl_parse WTBL_UMAC_DW3[] = {
	{"PN4",		WTBL_PN4_MASK,		WTBL_PN4_OFFSET,	false},
	{"PN5",		WTBL_PN5_MASK,		WTBL_PN5_OFFSET,	true},
	{"COM_SN",	WF_UWTBL_COM_SN_MASK,	WF_UWTBL_COM_SN_SHIFT,	true},
	{NULL,}
};

static const struct berse_wtbl_parse WTBL_UMAC_DW4_BIPN[] = {
	{"BIPN0",	WTBL_BIPN0_MASK,	WTBL_BIPN0_OFFSET,	false},
	{"BIPN1",	WTBL_BIPN1_MASK,	WTBL_BIPN1_OFFSET,	false},
	{"BIPN2",	WTBL_BIPN2_MASK,	WTBL_BIPN2_OFFSET,	true},
	{"BIPN3",	WTBL_BIPN3_MASK,	WTBL_BIPN3_OFFSET,	false},
	{NULL,}
};

static const struct berse_wtbl_parse WTBL_UMAC_DW5_BIPN[] = {
	{"BIPN4",	WTBL_BIPN4_MASK,	WTBL_BIPN4_OFFSET,	false},
	{"BIPN5",	WTBL_BIPN5_MASK,	WTBL_BIPN5_OFFSET,	true},
	{NULL,}
};

static void parse_fmac_uwtbl_pn(struct seq_file *s, u8 *uwtbl, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u16 i = 0;

	seq_printf(s, "\t\n");
	seq_printf(s, "UWTBL PN\n");

	/* UMAC WTBL DW 2/3 */
	addr = (u32 *)&(uwtbl[WF_UWTBL_PN_31_0__DW*4]);
	dw_value = *addr;

	while (WTBL_UMAC_DW2[i].name) {
		seq_printf(s, "\t%s:%u\n", WTBL_UMAC_DW2[i].name,
			(dw_value & WTBL_UMAC_DW2[i].mask) >>
				WTBL_UMAC_DW2[i].shift);
		i++;
	}

	i = 0;
	addr = (u32 *)&(uwtbl[WF_UWTBL_PN_47_32__DW*4]);
	dw_value = *addr;

	while (WTBL_UMAC_DW3[i].name) {
		seq_printf(s, "\t%s:%u\n", WTBL_UMAC_DW3[i].name,
			 (dw_value & WTBL_UMAC_DW3[i].mask) >>
			WTBL_UMAC_DW3[i].shift);
		i++;
	}


	/* UMAC WTBL DW 4/5 for BIGTK */
	if (is_wtbl_bigtk_exist(lwtbl) == true) {
		i = 0;
		addr = (u32 *)&(uwtbl[WF_UWTBL_RX_BIPN_31_0__DW*4]);
		dw_value = *addr;

		while (WTBL_UMAC_DW4_BIPN[i].name) {
			seq_printf(s, "\t%s:%u\n", WTBL_UMAC_DW4_BIPN[i].name,
				(dw_value & WTBL_UMAC_DW4_BIPN[i].mask) >>
					WTBL_UMAC_DW4_BIPN[i].shift);
			i++;
		}

		i = 0;
		addr = (u32 *)&(uwtbl[WF_UWTBL_RX_BIPN_47_32__DW*4]);
		dw_value = *addr;

		while (WTBL_UMAC_DW5_BIPN[i].name) {
			seq_printf(s, "\t%s:%u\n", WTBL_UMAC_DW5_BIPN[i].name,
				(dw_value & WTBL_UMAC_DW5_BIPN[i].mask) >>
				WTBL_UMAC_DW5_BIPN[i].shift);
			i++;
		}
	}
}

static void parse_fmac_uwtbl_sn(struct seq_file *s, u8 *uwtbl)
{
	u32 *addr = 0;
	u32 u2SN = 0;

	/* UMAC WTBL DW SN part */
	seq_printf(s, "\t\n");
	seq_printf(s, "UWTBL SN\n");

	addr = (u32 *)&(uwtbl[WF_UWTBL_TID0_SN_DW*4]);
	u2SN = ((*addr) & WF_UWTBL_TID0_SN_MASK) >> WF_UWTBL_TID0_SN_SHIFT;
	seq_printf(s, "\t%s:%u\n", "TID0_AC0_SN", u2SN);

	addr = (u32 *)&(uwtbl[WF_UWTBL_TID1_SN_DW*4]);
	u2SN = ((*addr) & WF_UWTBL_TID1_SN_MASK) >> WF_UWTBL_TID1_SN_SHIFT;
	seq_printf(s, "\t%s:%u\n", "TID1_AC1_SN", u2SN);

	addr = (u32 *)&(uwtbl[WF_UWTBL_TID2_SN_7_0__DW*4]);
	u2SN = ((*addr) & WF_UWTBL_TID2_SN_7_0__MASK) >>
				WF_UWTBL_TID2_SN_7_0__SHIFT;
	addr = (u32 *)&(uwtbl[WF_UWTBL_TID2_SN_11_8__DW*4]);
	u2SN |= (((*addr) & WF_UWTBL_TID2_SN_11_8__MASK) >>
			WF_UWTBL_TID2_SN_11_8__SHIFT) << 8;
	seq_printf(s, "\t%s:%u\n", "TID2_AC2_SN", u2SN);

	addr = (u32 *)&(uwtbl[WF_UWTBL_TID3_SN_DW*4]);
	u2SN = ((*addr) & WF_UWTBL_TID3_SN_MASK) >> WF_UWTBL_TID3_SN_SHIFT;
	seq_printf(s, "\t%s:%u\n", "TID3_AC3_SN", u2SN);

	addr = (u32 *)&(uwtbl[WF_UWTBL_TID4_SN_DW*4]);
	u2SN = ((*addr) & WF_UWTBL_TID4_SN_MASK) >> WF_UWTBL_TID4_SN_SHIFT;
	seq_printf(s, "\t%s:%u\n", "TID4_SN", u2SN);

	addr = (u32 *)&(uwtbl[WF_UWTBL_TID5_SN_3_0__DW*4]);
	u2SN = ((*addr) & WF_UWTBL_TID5_SN_3_0__MASK) >>
				WF_UWTBL_TID5_SN_3_0__SHIFT;
	addr = (u32 *)&(uwtbl[WF_UWTBL_TID5_SN_11_4__DW*4]);
	u2SN |= (((*addr) & WF_UWTBL_TID5_SN_11_4__MASK) >>
				WF_UWTBL_TID5_SN_11_4__SHIFT) << 4;
	seq_printf(s, "\t%s:%u\n", "TID5_SN", u2SN);

	addr = (u32 *)&(uwtbl[WF_UWTBL_TID6_SN_DW*4]);
	u2SN = ((*addr) & WF_UWTBL_TID6_SN_MASK) >> WF_UWTBL_TID6_SN_SHIFT;
	seq_printf(s, "\t%s:%u\n", "TID6_SN", u2SN);

	addr = (u32 *)&(uwtbl[WF_UWTBL_TID7_SN_DW*4]);
	u2SN = ((*addr) & WF_UWTBL_TID7_SN_MASK) >> WF_UWTBL_TID7_SN_SHIFT;
	seq_printf(s, "\t%s:%u\n", "TID7_SN", u2SN);

	addr = (u32 *)&(uwtbl[WF_UWTBL_COM_SN_DW*4]);
	u2SN = ((*addr) & WF_UWTBL_COM_SN_MASK) >> WF_UWTBL_COM_SN_SHIFT;
	seq_printf(s, "\t%s:%u\n", "COM_SN", u2SN);
}

static void dump_key_table(
	struct seq_file *s,
	struct mt7996_dev *dev,
	uint16_t keyloc0,
	uint16_t keyloc1,
	uint16_t keyloc2
)
{
#define ONE_KEY_ENTRY_LEN_IN_DW                8
	u8 keytbl[ONE_KEY_ENTRY_LEN_IN_DW*4] = {0};
	uint16_t x;

	seq_printf(s, "\t\n");
	seq_printf(s, "\t%s:%d\n", "keyloc0", keyloc0);
	if (keyloc0 != INVALID_KEY_ENTRY) {

		/* Don't swap below two lines, halWtblReadRaw will
		* write new value WF_WTBLON_TOP_WDUCR_ADDR
		*/
		mt7996_wtbl_read_raw(dev, keyloc0,
			WTBL_TYPE_KEY, 0, ONE_KEY_ENTRY_LEN_IN_DW, keytbl);
		seq_printf(s, "\t\tKEY WTBL Addr: group:0x%x=0x%x addr: 0x%lx\n",
			MT_DBG_UWTBL_TOP_WDUCR_ADDR,
			mt76_rr(dev, MT_DBG_UWTBL_TOP_WDUCR_ADDR),
			KEYTBL_IDX2BASE(keyloc0, 0));
		for (x = 0; x < ONE_KEY_ENTRY_LEN_IN_DW; x++) {
			seq_printf(s, "\t\tDW%02d: %02x %02x %02x %02x\n",
				x,
				keytbl[x * 4 + 3],
				keytbl[x * 4 + 2],
				keytbl[x * 4 + 1],
				keytbl[x * 4]);
		}
	}

	seq_printf(s, "\t%s:%d\n", "keyloc1", keyloc1);
	if (keyloc1 != INVALID_KEY_ENTRY) {
		/* Don't swap below two lines, halWtblReadRaw will
		* write new value WF_WTBLON_TOP_WDUCR_ADDR
		*/
		mt7996_wtbl_read_raw(dev, keyloc1,
			WTBL_TYPE_KEY, 0, ONE_KEY_ENTRY_LEN_IN_DW, keytbl);
		seq_printf(s, "\t\tKEY WTBL Addr: group:0x%x=0x%x addr: 0x%lx\n",
			MT_DBG_UWTBL_TOP_WDUCR_ADDR,
			mt76_rr(dev, MT_DBG_UWTBL_TOP_WDUCR_ADDR),
			KEYTBL_IDX2BASE(keyloc1, 0));
		for (x = 0; x < ONE_KEY_ENTRY_LEN_IN_DW; x++) {
			seq_printf(s, "\t\tDW%02d: %02x %02x %02x %02x\n",
				x,
				keytbl[x * 4 + 3],
				keytbl[x * 4 + 2],
				keytbl[x * 4 + 1],
				keytbl[x * 4]);
		}
	}

	seq_printf(s, "\t%s:%d\n", "keyloc2", keyloc2);
	if (keyloc2 != INVALID_KEY_ENTRY) {
		/* Don't swap below two lines, halWtblReadRaw will
		* write new value WF_WTBLON_TOP_WDUCR_ADDR
		*/
		mt7996_wtbl_read_raw(dev, keyloc2,
			WTBL_TYPE_KEY, 0, ONE_KEY_ENTRY_LEN_IN_DW, keytbl);
		seq_printf(s, "\t\tKEY WTBL Addr: group:0x%x=0x%x addr: 0x%lx\n",
			MT_DBG_UWTBL_TOP_WDUCR_ADDR,
			mt76_rr(dev, MT_DBG_UWTBL_TOP_WDUCR_ADDR),
			KEYTBL_IDX2BASE(keyloc2, 0));
		for (x = 0; x < ONE_KEY_ENTRY_LEN_IN_DW; x++) {
			seq_printf(s, "\t\tDW%02d: %02x %02x %02x %02x\n",
				x,
				keytbl[x * 4 + 3],
				keytbl[x * 4 + 2],
				keytbl[x * 4 + 1],
				keytbl[x * 4]);
		}
	}
}

static void parse_fmac_uwtbl_key_info(struct seq_file *s, struct mt7996_dev *dev,
				      u8 *uwtbl, u8 *lwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	uint16_t keyloc0 = INVALID_KEY_ENTRY;
	uint16_t keyloc1 = INVALID_KEY_ENTRY;
	uint16_t keyloc2 = INVALID_KEY_ENTRY;

	/* UMAC WTBL DW 7 */
	seq_printf(s, "\t\n");
	seq_printf(s, "UWTBL key info\n");

	addr = (u32 *)&(uwtbl[WF_UWTBL_KEY_LOC0_DW*4]);
	dw_value = *addr;
	keyloc0 = (dw_value & WF_UWTBL_KEY_LOC0_MASK) >> WF_UWTBL_KEY_LOC0_SHIFT;
	keyloc1 = (dw_value & WF_UWTBL_KEY_LOC1_MASK) >> WF_UWTBL_KEY_LOC1_SHIFT;

	seq_printf(s, "\t%s:%u/%u\n", "Key Loc 0/1", keyloc0, keyloc1);

	/* UMAC WTBL DW 6 for BIGTK */
	if (is_wtbl_bigtk_exist(lwtbl) == true) {
		addr = (u32 *)&(uwtbl[WF_UWTBL_KEY_LOC2_DW*4]);
		dw_value = *addr;
		keyloc2 = (dw_value & WF_UWTBL_KEY_LOC2_MASK) >>
			WF_UWTBL_KEY_LOC2_SHIFT;
		seq_printf(s, "\t%s:%u\n", "Key Loc 2", keyloc2);
	}

	/* Parse KEY link */
	dump_key_table(s, dev, keyloc0, keyloc1, keyloc2);
}

static const struct berse_wtbl_parse WTBL_UMAC_DW8[] = {
	{"UWTBL_WMM_Q",		WF_UWTBL_WMM_Q_MASK,		WF_UWTBL_WMM_Q_SHIFT,	false},
	{"UWTBL_QOS",		WF_UWTBL_QOS_MASK,		NO_SHIFT_DEFINE,	false},
	{"UWTBL_HT_VHT_HE",	WF_UWTBL_HT_MASK,		NO_SHIFT_DEFINE,	false},
	{"UWTBL_HDRT_MODE",	WF_UWTBL_HDRT_MODE_MASK,	NO_SHIFT_DEFINE,	true},
	{NULL,}
};

static void parse_fmac_uwtbl_msdu_info(struct seq_file *s, u8 *uwtbl)
{
	u32 *addr = 0;
	u32 dw_value = 0;
	u32 amsdu_len = 0;
	u16 i = 0;

	/* UMAC WTBL DW 8 */
	seq_printf(s, "\t\n");
	seq_printf(s, "UWTBL DW8\n");

	addr = (u32 *)&(uwtbl[WF_UWTBL_AMSDU_CFG_DW*4]);
	dw_value = *addr;

	while (WTBL_UMAC_DW8[i].name) {

		if (WTBL_UMAC_DW8[i].shift == NO_SHIFT_DEFINE)
			seq_printf(s, "\t%s:%d\n", WTBL_UMAC_DW8[i].name,
				(dw_value & WTBL_UMAC_DW8[i].mask) ? 1 : 0);
		else
			seq_printf(s, "\t%s:%u\n", WTBL_UMAC_DW8[i].name,
				(dw_value & WTBL_UMAC_DW8[i].mask) >>
					WTBL_UMAC_DW8[i].shift);
		i++;
	}

	/* UMAC WTBL DW 8 - SEC_ADDR_MODE */
	addr = (u32 *)&(uwtbl[WF_UWTBL_SEC_ADDR_MODE_DW*4]);
	dw_value = *addr;
	seq_printf(s, "\t%s:%lu\n", "SEC_ADDR_MODE",
		(dw_value & WTBL_SEC_ADDR_MODE_MASK) >> WTBL_SEC_ADDR_MODE_OFFSET);

	/* UMAC WTBL DW 8 - AMSDU_CFG */
	seq_printf(s, "\t%s:%d\n", "HW AMSDU Enable",
				(dw_value & WTBL_AMSDU_EN_MASK) ? 1 : 0);

	amsdu_len = (dw_value & WTBL_AMSDU_LEN_MASK) >> WTBL_AMSDU_LEN_OFFSET;
	if (amsdu_len == 0)
		seq_printf(s, "\t%s:invalid (WTBL value=0x%x)\n", "HW AMSDU Len",
			amsdu_len);
	else if (amsdu_len == 1)
		seq_printf(s, "\t%s:%d~%d (WTBL value=0x%x)\n", "HW AMSDU Len",
			1,
			255,
			amsdu_len);
	else if (amsdu_len == 2)
		seq_printf(s, "\t%s:%d~%d (WTBL value=0x%x)\n", "HW AMSDU Len",
			256,
			511,
			amsdu_len);
	else if (amsdu_len == 3)
		seq_printf(s, "\t%s:%d~%d (WTBL value=0x%x)\n", "HW AMSDU Len",
			512,
			767,
			amsdu_len);
	else
		seq_printf(s, "\t%s:%d~%d (WTBL value=0x%x)\n", "HW AMSDU Len",
			256 * (amsdu_len - 1),
			256 * (amsdu_len - 1) + 255,
			amsdu_len);

	seq_printf(s, "\t%s:%lu (WTBL value=0x%lx)\n", "HW AMSDU Num",
		((dw_value & WTBL_AMSDU_NUM_MASK) >> WTBL_AMSDU_NUM_OFFSET) + 1,
		(dw_value & WTBL_AMSDU_NUM_MASK) >> WTBL_AMSDU_NUM_OFFSET);
}

static void
mt7996_wtbl_dump(struct seq_file *s, struct mt7996_dev *dev, u16 idx)
{
	u8 lwtbl[LWTBL_LEN_IN_DW * 4] = {0};
	u8 uwtbl[UWTBL_LEN_IN_DW * 4] = {0};
	int x;

	mt7996_wtbl_read_raw(dev, idx, WTBL_TYPE_LMAC, 0,
				 LWTBL_LEN_IN_DW, lwtbl);
	seq_printf(s, "Dump WTBL info of WLAN_IDX:%d\n", idx);
	seq_printf(s, "LMAC WTBL Addr: group:0x%x=0x%x addr: 0x%lx\n",
		   MT_DBG_WTBLON_TOP_WDUCR_ADDR,
		   mt76_rr(dev, MT_DBG_WTBLON_TOP_WDUCR_ADDR),
		   LWTBL_IDX2BASE(idx, 0));
	for (x = 0; x < LWTBL_LEN_IN_DW; x++) {
		seq_printf(s, "DW%02d: %02x %02x %02x %02x\n",
			   x,
			   lwtbl[x * 4 + 3],
			   lwtbl[x * 4 + 2],
			   lwtbl[x * 4 + 1],
			   lwtbl[x * 4]);
	}

	/* Parse LWTBL */
	parse_fmac_lwtbl_dw0_1(s, lwtbl);
	parse_fmac_lwtbl_dw2(s, lwtbl);
	parse_fmac_lwtbl_dw3(s, lwtbl);
	parse_fmac_lwtbl_dw4(s, lwtbl);
	parse_fmac_lwtbl_dw5(s, lwtbl);
	parse_fmac_lwtbl_dw6(s, lwtbl);
	parse_fmac_lwtbl_dw7(s, lwtbl);
	parse_fmac_lwtbl_dw8(s, lwtbl);
	parse_fmac_lwtbl_dw9(s, lwtbl);
	parse_fmac_lwtbl_dw10(s, lwtbl);
	parse_fmac_lwtbl_dw11(s, lwtbl);
	parse_fmac_lwtbl_dw12(s, lwtbl);
	parse_fmac_lwtbl_dw13(s, lwtbl);
	parse_fmac_lwtbl_dw14(s, lwtbl);
	parse_fmac_lwtbl_mlo_info(s, lwtbl);
	parse_fmac_lwtbl_dw31(s, lwtbl);
	parse_fmac_lwtbl_dw32(s, lwtbl);
	parse_fmac_lwtbl_rx_stats(s, lwtbl);

	mt7996_wtbl_read_raw(dev, idx, WTBL_TYPE_UMAC, 0,
				 UWTBL_LEN_IN_DW, uwtbl);
	seq_printf(s, "Dump WTBL info of WLAN_IDX:%d\n", idx);
	seq_printf(s, "UMAC WTBL Addr: group:0x%x=0x%x addr: 0x%lx\n",
		   MT_DBG_UWTBL_TOP_WDUCR_ADDR,
		   mt76_rr(dev, MT_DBG_UWTBL_TOP_WDUCR_ADDR),
		   UWTBL_IDX2BASE(idx, 0));
	for (x = 0; x < UWTBL_LEN_IN_DW; x++) {
		seq_printf(s, "DW%02d: %02x %02x %02x %02x\n",
			   x,
			   uwtbl[x * 4 + 3],
			   uwtbl[x * 4 + 2],
			   uwtbl[x * 4 + 1],
			   uwtbl[x * 4]);
	}

	/* Parse UWTBL */
	parse_fmac_uwtbl_mlo_info(s, uwtbl);
	parse_fmac_uwtbl_pn(s, uwtbl, lwtbl);
	parse_fmac_uwtbl_sn(s, uwtbl);
	parse_fmac_uwtbl_key_info(s, dev, uwtbl, lwtbl);
	parse_fmac_uwtbl_msdu_info(s, uwtbl);
}

/** global debugfs **/

struct hw_queue_map {
	const char *name;
	u8 index;
	u8 pid;
	u8 qid;
};

static int
mt7996_implicit_txbf_set(void *data, u64 val)
{
	struct mt7996_dev *dev = data;

	/* The existing connected stations shall reconnect to apply
	 * new implicit txbf configuration.
	 */
	dev->ibf = !!val;

	return mt7996_mcu_set_txbf(dev, BF_HW_EN_UPDATE);
}

static int
mt7996_implicit_txbf_get(void *data, u64 *val)
{
	struct mt7996_dev *dev = data;

	*val = dev->ibf;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_implicit_txbf, mt7996_implicit_txbf_get,
			 mt7996_implicit_txbf_set, "%lld\n");

/* test knob of system error recovery */
static ssize_t
mt7996_sys_recovery_set(struct file *file, const char __user *user_buf,
			size_t count, loff_t *ppos)
{
	struct mt7996_phy *phy = file->private_data;
	struct mt7996_dev *dev = phy->dev;
	bool band = phy->mt76->band_idx;
	char buf[16];
	int ret = 0;
	u16 val;

	if (count >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	if (count && buf[count - 1] == '\n')
		buf[count - 1] = '\0';
	else
		buf[count] = '\0';

	if (kstrtou16(buf, 0, &val))
		return -EINVAL;

	switch (val) {
	/*
	 * 0: grab firmware current SER state.
	 * 1: trigger & enable system error L1 recovery.
	 * 2: trigger & enable system error L2 recovery.
	 * 3: trigger & enable system error L3 rx abort.
	 * 4: trigger & enable system error L3 tx abort
	 * 5: trigger & enable system error L3 tx disable.
	 * 6: trigger & enable system error L3 bf recovery.
	 * 7: trigger & enable system error L4 mdp recovery.
	 * 8: trigger & enable system error full recovery.
	 * 9: trigger firmware crash.
	 * 10: trigger grab wa firmware coredump.
	 * 11: trigger grab wm firmware coredump.
	 * 12: hw bit detect only.
	 */
	case UNI_CMD_SER_QUERY:
		ret = mt7996_mcu_set_ser(dev, UNI_CMD_SER_QUERY, 0, band);
		break;
	case UNI_CMD_SER_SET_RECOVER_L1:
	case UNI_CMD_SER_SET_RECOVER_L2:
	case UNI_CMD_SER_SET_RECOVER_L3_RX_ABORT:
	case UNI_CMD_SER_SET_RECOVER_L3_TX_ABORT:
	case UNI_CMD_SER_SET_RECOVER_L3_TX_DISABLE:
	case UNI_CMD_SER_SET_RECOVER_L3_BF:
	case UNI_CMD_SER_SET_RECOVER_L4_MDP:
		ret = mt7996_mcu_set_ser(dev, UNI_CMD_SER_SET, BIT(val), band);
		if (ret)
			return ret;

		ret = mt7996_mcu_set_ser(dev, UNI_CMD_SER_TRIGGER, val, band);
		break;

	/* enable full chip reset */
	case UNI_CMD_SER_SET_RECOVER_FULL:
		mt76_set(dev, MT_WFDMA0_MCU_HOST_INT_ENA, MT_MCU_CMD_WDT_MASK);
		dev->recovery.state |= MT_MCU_CMD_WM_WDT;
		mt7996_reset(dev);
		break;

	/* WARNING: trigger firmware crash */
	case UNI_CMD_SER_SET_SYSTEM_ASSERT:
		// trigger wm assert exception
		mt76_wr(dev, 0x89018108, 0x20);
		mt76_wr(dev, 0x89018118, 0x20);
		// trigger wa assert exception
		if (mt7996_has_wa(dev)) {
			mt76_wr(dev, 0x89098108, 0x20);
			mt76_wr(dev, 0x89098118, 0x20);
		}
		break;
	case UNI_CMD_SER_FW_COREDUMP_WA:
		if (mt7996_has_wa(dev))
			mt7996_coredump(dev, MT7996_COREDUMP_MANUAL_WA);
		break;
	case UNI_CMD_SER_FW_COREDUMP_WM:
		mt7996_coredump(dev, MT7996_COREDUMP_MANUAL_WM);
		break;
	case UNI_CMD_SER_SET_HW_BIT_DETECT_ONLY:
		ret = mt7996_mcu_set_ser(dev, UNI_CMD_SER_SET, BIT(0), band);
		if (ret)
			return ret;
		break;
	default:
		break;
	}

	return ret ? ret : count;
}

static ssize_t
mt7996_sys_recovery_get(struct file *file, char __user *user_buf,
			size_t count, loff_t *ppos)
{
	struct mt7996_phy *phy = file->private_data;
	struct mt7996_dev *dev = phy->dev;
	char *buff;
	int desc = 0;
	ssize_t ret;
	static const size_t bufsz = 1536;

	buff = kmalloc(bufsz, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	/* HELP */
	desc += scnprintf(buff + desc, bufsz - desc,
			  "Please echo the correct value ...\n");
	desc += scnprintf(buff + desc, bufsz - desc,
			  "%2d: grab firmware transient SER state\n",
			  UNI_CMD_SER_QUERY);
	desc += scnprintf(buff + desc, bufsz - desc,
			  "%2d: trigger system error L1 recovery\n",
			  UNI_CMD_SER_SET_RECOVER_L1);
	desc += scnprintf(buff + desc, bufsz - desc,
			  "%2d: trigger system error L2 recovery\n",
			  UNI_CMD_SER_SET_RECOVER_L2);
	desc += scnprintf(buff + desc, bufsz - desc,
			  "%2d: trigger system error L3 rx abort\n",
			  UNI_CMD_SER_SET_RECOVER_L3_RX_ABORT);
	desc += scnprintf(buff + desc, bufsz - desc,
			  "%2d: trigger system error L3 tx abort\n",
			  UNI_CMD_SER_SET_RECOVER_L3_TX_ABORT);
	desc += scnprintf(buff + desc, bufsz - desc,
			  "%2d: trigger system error L3 tx disable\n",
			  UNI_CMD_SER_SET_RECOVER_L3_TX_DISABLE);
	desc += scnprintf(buff + desc, bufsz - desc,
			  "%2d: trigger system error L3 bf recovery\n",
			  UNI_CMD_SER_SET_RECOVER_L3_BF);
	desc += scnprintf(buff + desc, bufsz - desc,
			  "%2d: trigger system error L4 mdp recovery\n",
			  UNI_CMD_SER_SET_RECOVER_L4_MDP);
	desc += scnprintf(buff + desc, bufsz - desc,
			  "%2d: trigger system error full recovery\n",
			  UNI_CMD_SER_SET_RECOVER_FULL);
	desc += scnprintf(buff + desc, bufsz - desc,
			  "%2d: trigger firmware crash\n",
			  UNI_CMD_SER_SET_SYSTEM_ASSERT);
	desc += scnprintf(buff + desc, bufsz - desc,
			  "%2d: trigger grab wa firmware coredump\n",
			  UNI_CMD_SER_FW_COREDUMP_WA);
	desc += scnprintf(buff + desc, bufsz - desc,
			  "%2d: trigger grab wm firmware coredump\n",
			  UNI_CMD_SER_FW_COREDUMP_WM);
	desc += scnprintf(buff + desc, bufsz - desc,
			  "%2d: hw bit detect only\n",
			  UNI_CMD_SER_SET_HW_BIT_DETECT_ONLY);
	/* SER statistics */
	desc += scnprintf(buff + desc, bufsz - desc,
			  "\nlet's dump firmware SER statistics...\n");
	desc += scnprintf(buff + desc, bufsz - desc,
			  "::E  R , SER_STATUS        = 0x%08x\n",
			  mt76_rr(dev, MT_SWDEF_SER_STATS));
	desc += scnprintf(buff + desc, bufsz - desc,
			  "::E  R , SER_PLE_ERR       = 0x%08x\n",
			  mt76_rr(dev, MT_SWDEF_PLE_STATS));
	desc += scnprintf(buff + desc, bufsz - desc,
			  "::E  R , SER_PLE_ERR_1     = 0x%08x\n",
			  mt76_rr(dev, MT_SWDEF_PLE1_STATS));
	desc += scnprintf(buff + desc, bufsz - desc,
			  "::E  R , SER_PLE_ERR_AMSDU = 0x%08x\n",
			  mt76_rr(dev, MT_SWDEF_PLE_AMSDU_STATS));
	desc += scnprintf(buff + desc, bufsz - desc,
			  "::E  R , SER_PSE_ERR       = 0x%08x\n",
			  mt76_rr(dev, MT_SWDEF_PSE_STATS));
	desc += scnprintf(buff + desc, bufsz - desc,
			  "::E  R , SER_PSE_ERR_1     = 0x%08x\n",
			  mt76_rr(dev, MT_SWDEF_PSE1_STATS));
	desc += scnprintf(buff + desc, bufsz - desc,
			  "::E  R , SER_LMAC_WISR6_B0 = 0x%08x\n",
			  mt76_rr(dev, MT_SWDEF_LAMC_WISR6_BN0_STATS));
	desc += scnprintf(buff + desc, bufsz - desc,
			  "::E  R , SER_LMAC_WISR6_B1 = 0x%08x\n",
			  mt76_rr(dev, MT_SWDEF_LAMC_WISR6_BN1_STATS));
	desc += scnprintf(buff + desc, bufsz - desc,
			  "::E  R , SER_LMAC_WISR6_B2 = 0x%08x\n",
			  mt76_rr(dev, MT_SWDEF_LAMC_WISR6_BN2_STATS));
	desc += scnprintf(buff + desc, bufsz - desc,
			  "::E  R , SER_LMAC_WISR7_B0 = 0x%08x\n",
			  mt76_rr(dev, MT_SWDEF_LAMC_WISR7_BN0_STATS));
	desc += scnprintf(buff + desc, bufsz - desc,
			  "::E  R , SER_LMAC_WISR7_B1 = 0x%08x\n",
			  mt76_rr(dev, MT_SWDEF_LAMC_WISR7_BN1_STATS));
	desc += scnprintf(buff + desc, bufsz - desc,
			  "::E  R , SER_LMAC_WISR7_B2 = 0x%08x\n",
			  mt76_rr(dev, MT_SWDEF_LAMC_WISR7_BN2_STATS));
	desc += scnprintf(buff + desc, bufsz - desc,
			  "\nSYS_RESET_COUNT: WM %d, WA %d\n",
			  dev->recovery.wm_reset_count,
			  dev->recovery.wa_reset_count);

	ret = simple_read_from_buffer(user_buf, count, ppos, buff, desc);
	kfree(buff);
	return ret;
}

static const struct file_operations mt7996_sys_recovery_ops = {
	.write = mt7996_sys_recovery_set,
	.read = mt7996_sys_recovery_get,
	.open = simple_open,
	.llseek = default_llseek,
};

static int
mt7996_radar_trigger(void *data, u64 val)
{
#define RADAR_MAIN_CHAIN	1
#define RADAR_BACKGROUND	2
	struct mt7996_dev *dev = data;
	struct mt7996_phy *phy = mt7996_band_phy(dev, NL80211_BAND_5GHZ);
	struct cfg80211_chan_def *chandef;
	int rdd_idx, ret;

	if (!phy || !val || val > RADAR_BACKGROUND)
		return -EINVAL;

	if (test_bit(MT76_SCANNING, &phy->mt76->state))
		return -EBUSY;

	if (val == RADAR_BACKGROUND) {
		if (!dev->rdd2_phy || !cfg80211_chandef_valid(&dev->rdd2_chandef)) {
			dev_err(dev->mt76.dev, "Background radar is not enabled\n");
			return -EINVAL;
		}
		chandef = &dev->rdd2_chandef;
	} else {
		chandef = &phy->mt76->chandef;
	}

	rdd_idx = mt7996_get_rdd_idx(phy, val == RADAR_BACKGROUND);
	if (rdd_idx < 0) {
		dev_err(dev->mt76.dev, "No RDD found\n");
		return -EINVAL;
	}

	ret = cfg80211_chandef_dfs_required(dev->mt76.hw->wiphy, chandef,
					    NL80211_IFTYPE_AP);
	if (ret <= 0)
		return ret;

	return mt7996_mcu_rdd_cmd(dev, RDD_RADAR_EMULATE, rdd_idx, 0);
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_radar_trigger, NULL,
			 mt7996_radar_trigger, "%lld\n");

static int
mt7996_rdd_monitor(struct seq_file *s, void *data)
{
	struct mt7996_dev *dev = dev_get_drvdata(s->private);
	struct cfg80211_chan_def *chandef = &dev->rdd2_chandef;
	const char *bw;
	int ret = 0;

	mutex_lock(&dev->mt76.mutex);

	if (!cfg80211_chandef_valid(chandef)) {
		ret = -EINVAL;
		goto out;
	}

	if (!dev->rdd2_phy) {
		seq_puts(s, "not running\n");
		goto out;
	}

	switch (chandef->width) {
	case NL80211_CHAN_WIDTH_40:
		bw = "40";
		break;
	case NL80211_CHAN_WIDTH_80:
		bw = "80";
		break;
	case NL80211_CHAN_WIDTH_160:
		bw = "160";
		break;
	case NL80211_CHAN_WIDTH_80P80:
		bw = "80P80";
		break;
	default:
		bw = "20";
		break;
	}

	seq_printf(s, "channel %d (%d MHz) width %s MHz center1: %d MHz\n",
		   chandef->chan->hw_value, chandef->chan->center_freq,
		   bw, chandef->center_freq1);
out:
	mutex_unlock(&dev->mt76.mutex);

	return ret;
}

static int
mt7996_fw_debug_wm_set(void *data, u64 val)
{
	struct mt7996_dev *dev = data;
	enum {
		DEBUG_TXCMD = 62,
		DEBUG_CMD_RPT_TX,
		DEBUG_CMD_RPT_TRIG,
		DEBUG_SPL,
		DEBUG_RPT_RX,
		DEBUG_IDS_SND = 84,
		DEBUG_IDS_BSRP,
		DEBUG_IDS_TPUT_MON,
		DEBUG_IDS_PP = 93,
		DEBUG_IDS_RA,
		DEBUG_IDS_BF,
		DEBUG_IDS_SR,
		DEBUG_IDS_RU,
		DEBUG_IDS_MUMIMO,
		DEBUG_IDS_MLO = 100,
		DEBUG_IDS_ERR_LOG,
	};
	u8 debug_category[] = {
		DEBUG_TXCMD,
		DEBUG_CMD_RPT_TX,
		DEBUG_CMD_RPT_TRIG,
		DEBUG_SPL,
		DEBUG_RPT_RX,
		DEBUG_IDS_SND,
		DEBUG_IDS_BSRP,
		DEBUG_IDS_TPUT_MON,
		DEBUG_IDS_PP,
		DEBUG_IDS_RA,
		DEBUG_IDS_BF,
		DEBUG_IDS_SR,
		DEBUG_IDS_RU,
		DEBUG_IDS_MUMIMO,
		DEBUG_IDS_MLO,
		DEBUG_IDS_ERR_LOG,
	};
	bool tx, rx, en;
	int ret;
	u8 i;

	dev->fw_debug_wm = val;

	if (dev->fw_debug_bin)
		val = MCU_FW_LOG_RELAY;
	else
		val = dev->fw_debug_wm;

	tx = dev->fw_debug_wm || (dev->fw_debug_bin & BIT(1));
	rx = dev->fw_debug_wm || (dev->fw_debug_bin & BIT(2));
	en = dev->fw_debug_wm || (dev->fw_debug_bin & BIT(0));

	ret = mt7996_mcu_fw_log_2_host(dev, MCU_FW_LOG_WM, val);
	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(debug_category); i++) {
		if (debug_category[i] == DEBUG_RPT_RX)
			val = en && rx;
		else
			val = en && tx;

		ret = mt7996_mcu_fw_dbg_ctrl(dev, debug_category[i], val);
		if (ret)
			return ret;

		if ((debug_category[i] == DEBUG_TXCMD ||
		     debug_category[i] == DEBUG_IDS_SND) && en) {
			ret = mt7996_mcu_fw_dbg_ctrl(dev, debug_category[i], 2);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int
mt7996_fw_debug_wm_get(void *data, u64 *val)
{
	struct mt7996_dev *dev = data;

	*val = dev->fw_debug_wm;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_fw_debug_wm, mt7996_fw_debug_wm_get,
			 mt7996_fw_debug_wm_set, "%lld\n");

static int
mt7996_fw_debug_wa_set(void *data, u64 val)
{
	struct mt7996_dev *dev = data;
	int ret;

	dev->fw_debug_wa = val ? MCU_FW_LOG_TO_HOST : 0;

	ret = mt7996_mcu_fw_log_2_host(dev, MCU_FW_LOG_WA, dev->fw_debug_wa);
	if (ret)
		return ret;

	return mt7996_mcu_wa_cmd(dev, MCU_WA_PARAM_CMD(SET), MCU_WA_PARAM_PDMA_RX,
				 !!dev->fw_debug_wa, 0);
}

static int
mt7996_fw_debug_wa_get(void *data, u64 *val)
{
	struct mt7996_dev *dev = data;

	*val = dev->fw_debug_wa;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_fw_debug_wa, mt7996_fw_debug_wa_get,
			 mt7996_fw_debug_wa_set, "%lld\n");

static struct dentry *
create_buf_file_cb(const char *filename, struct dentry *parent, umode_t mode,
		   struct rchan_buf *buf, int *is_global)
{
	struct dentry *f;

	f = debugfs_create_file("fwlog_data", mode, parent, buf,
				&relay_file_operations);
	if (IS_ERR(f))
		return NULL;

	*is_global = 1;

	return f;
}

static int
remove_buf_file_cb(struct dentry *f)
{
	debugfs_remove(f);

	return 0;
}

static int
mt7996_fw_debug_bin_set(void *data, u64 val)
{
	static struct rchan_callbacks relay_cb = {
		.create_buf_file = create_buf_file_cb,
		.remove_buf_file = remove_buf_file_cb,
	};
	struct mt7996_dev *dev = data;

	if (!dev->relay_fwlog)
		dev->relay_fwlog = relay_open("fwlog_data", dev->debugfs_dir,
					      1500, 512, &relay_cb, NULL);
	if (!dev->relay_fwlog)
		return -ENOMEM;

	dev->fw_debug_bin = val;

	relay_reset(dev->relay_fwlog);

	return mt7996_fw_debug_wm_set(dev, dev->fw_debug_wm);
}

static int
mt7996_fw_debug_bin_get(void *data, u64 *val)
{
	struct mt7996_dev *dev = data;

	*val = dev->fw_debug_bin;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_fw_debug_bin, mt7996_fw_debug_bin_get,
			 mt7996_fw_debug_bin_set, "%lld\n");

static int
mt7996_fw_util_wa_show(struct seq_file *file, void *data)
{
	struct mt7996_dev *dev = file->private;

	if (dev->fw_debug_wa)
		return mt7996_mcu_wa_cmd(dev, MCU_WA_PARAM_CMD(QUERY),
					 MCU_WA_PARAM_CPU_UTIL, 0, 0);

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(mt7996_fw_util_wa);

static void
mt7996_ampdu_stat_read_phy(struct mt7996_phy *phy, struct seq_file *file)
{
	struct mt7996_dev *dev = phy->dev;
	int bound[15], range[8], i;
	u8 band_idx = phy->mt76->band_idx;

	/* Tx ampdu stat */
	for (i = 0; i < ARRAY_SIZE(range); i++)
		range[i] = mt76_rr(dev, MT_MIB_ARNG(band_idx, i));

	for (i = 0; i < ARRAY_SIZE(bound); i++)
		bound[i] = MT_MIB_ARNCR_RANGE(range[i / 2], i % 2) + 1;

	seq_printf(file, "\nPhy %s, Phy band %d\n",
		   wiphy_name(phy->mt76->hw->wiphy), band_idx);

	seq_printf(file, "Length: %8d | ", bound[0]);
	for (i = 0; i < ARRAY_SIZE(bound) - 1; i++)
		seq_printf(file, "%3d -%3d | ",
			   bound[i] + 1, bound[i + 1]);

	seq_puts(file, "\nCount:  ");
	for (i = 0; i < ARRAY_SIZE(bound); i++)
		seq_printf(file, "%8d | ", phy->mt76->aggr_stats[i]);
	seq_puts(file, "\n");

	seq_printf(file, "BA miss count: %d\n", phy->mib.ba_miss_cnt);
}

static void
mt7996_txbf_stat_read_phy(struct mt7996_phy *phy, struct seq_file *s)
{
	struct mt76_mib_stats *mib = &phy->mib;
	static const char * const bw[] = {
		"BW20", "BW40", "BW80", "BW160", "BW320"
	};

	/* Tx Beamformer monitor */
	seq_puts(s, "\nTx Beamformer applied PPDU counts: ");

	seq_printf(s, "iBF: %d, eBF: %d\n",
		   mib->tx_bf_ibf_ppdu_cnt,
		   mib->tx_bf_ebf_ppdu_cnt);

	/* Tx Beamformer Rx feedback monitor */
	seq_puts(s, "Tx Beamformer Rx feedback statistics: ");

	seq_printf(s, "All: %d, EHT: %d, HE: %d, VHT: %d, HT: %d, ",
		   mib->tx_bf_rx_fb_all_cnt,
		   mib->tx_bf_rx_fb_eht_cnt,
		   mib->tx_bf_rx_fb_he_cnt,
		   mib->tx_bf_rx_fb_vht_cnt,
		   mib->tx_bf_rx_fb_ht_cnt);

	seq_printf(s, "%s, NC: %d, NR: %d\n",
		   bw[mib->tx_bf_rx_fb_bw],
		   mib->tx_bf_rx_fb_nc_cnt,
		   mib->tx_bf_rx_fb_nr_cnt);

	/* Tx Beamformee Rx NDPA & Tx feedback report */
	seq_printf(s, "Tx Beamformee successful feedback frames: %d\n",
		   mib->tx_bf_fb_cpl_cnt);
	seq_printf(s, "Tx Beamformee feedback triggered counts: %d\n",
		   mib->tx_bf_fb_trig_cnt);

	/* Tx SU & MU counters */
	seq_printf(s, "Tx multi-user Beamforming counts: %d\n",
		   mib->tx_mu_bf_cnt);
	seq_printf(s, "Tx multi-user MPDU counts: %d\n", mib->tx_mu_mpdu_cnt);
	seq_printf(s, "Tx multi-user successful MPDU counts: %d\n",
		   mib->tx_mu_acked_mpdu_cnt);
	seq_printf(s, "Tx single-user successful MPDU counts: %d\n",
		   mib->tx_su_acked_mpdu_cnt);

	seq_puts(s, "\n");
}

static void
mt7996_tx_stats_show_phy(struct seq_file *file, struct mt7996_phy *phy)
{
	struct mt76_mib_stats *mib = &phy->mib;
	u32 attempts, success, per;
	int i;

	mt7996_mac_update_stats(phy);
	mt7996_ampdu_stat_read_phy(phy, file);

	attempts = mib->tx_mpdu_attempts_cnt;
	success = mib->tx_mpdu_success_cnt;
	per = attempts ? 100 - success * 100 / attempts : 100;
	seq_printf(file, "Tx attempts: %8u (MPDUs)\n", attempts);
	seq_printf(file, "Tx success: %8u (MPDUs)\n", success);
	seq_printf(file, "Tx PER: %u%%\n", per);

	mt7996_txbf_stat_read_phy(phy, file);

	/* Tx amsdu info */
	seq_puts(file, "Tx MSDU statistics:\n");
	for (i = 0; i < ARRAY_SIZE(mib->tx_amsdu); i++) {
		seq_printf(file, "AMSDU pack count of %d MSDU in TXD: %8d ",
			   i + 1, mib->tx_amsdu[i]);
		if (mib->tx_amsdu_cnt)
			seq_printf(file, "(%3d%%)\n",
				   mib->tx_amsdu[i] * 100 / mib->tx_amsdu_cnt);
		else
			seq_puts(file, "\n");
	}
}

static int
mt7996_tx_stats_show(struct seq_file *file, void *data)
{
	struct mt7996_phy *phy = file->private;
	struct mt7996_dev *dev = phy->dev;

	mutex_lock(&dev->mt76.mutex);

	mt7996_tx_stats_show_phy(file, phy);

	mutex_unlock(&dev->mt76.mutex);

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(mt7996_tx_stats);

static int
mt7996_rxfilter_show(struct seq_file *file, void *data)
{
	struct mt7996_phy *phy = file->private;
	struct mt7996_dev *dev = phy->dev;
	u32 cr, cr1;

	mutex_lock(&phy->dev->mt76.mutex);

	cr = mt76_rr(dev, MT_WF_RFCR(phy->mt76->band_idx));
	cr1 = mt76_rr(dev, MT_WF_RFCR1(phy->mt76->band_idx));

#define __MT7996_RXFILTER_PRINT(reg, flag) do {		\
		if ((reg) & (flag))			\
			seq_printf(file, #flag "\n");	\
	} while (0)
#define MT7996_RFCR_PRINT(flag) __MT7996_RXFILTER_PRINT(cr, MT_WF_RFCR_##flag)
#define MT7996_RFCR1_PRINT(flag) __MT7996_RXFILTER_PRINT(cr1, MT_WF_RFCR1_##flag)

	seq_printf(file, "CR: 0x%08x (configured: 0x%08x)\n", cr, phy->rxfilter.cr);
	MT7996_RFCR_PRINT(DROP_STBC_MULTI);
	MT7996_RFCR_PRINT(DROP_FCSFAIL);
	MT7996_RFCR_PRINT(DROP_PROBEREQ);
	MT7996_RFCR_PRINT(DROP_MCAST);
	MT7996_RFCR_PRINT(DROP_BCAST);
	MT7996_RFCR_PRINT(DROP_MCAST_FILTERED);
	MT7996_RFCR_PRINT(DROP_A3_MAC);
	MT7996_RFCR_PRINT(DROP_A3_BSSID);
	MT7996_RFCR_PRINT(DROP_A2_BSSID);
	MT7996_RFCR_PRINT(DROP_OTHER_BEACON);
	MT7996_RFCR_PRINT(DROP_FRAME_REPORT);
	MT7996_RFCR_PRINT(DROP_CTL_RSV);
	MT7996_RFCR_PRINT(DROP_CTS);
	MT7996_RFCR_PRINT(DROP_RTS);
	MT7996_RFCR_PRINT(DROP_DUPLICATE);
	MT7996_RFCR_PRINT(DROP_OTHER_BSS);
	MT7996_RFCR_PRINT(DROP_OTHER_UC);
	MT7996_RFCR_PRINT(DROP_OTHER_TIM);
	MT7996_RFCR_PRINT(DROP_NDPA);
	MT7996_RFCR_PRINT(DROP_UNWANTED_CTL);

	seq_printf(file, "\nCR1: 0x%08x (configured: 0x%08x)\n", cr1, phy->rxfilter.cr1);
	MT7996_RFCR1_PRINT(DROP_ACK);
	MT7996_RFCR1_PRINT(DROP_BF_POLL);
	MT7996_RFCR1_PRINT(DROP_BA);
	MT7996_RFCR1_PRINT(DROP_CFEND);
	MT7996_RFCR1_PRINT(DROP_CFACK);

	mutex_unlock(&phy->dev->mt76.mutex);

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(mt7996_rxfilter);

static int
mt7996_rmac_table_show(struct seq_file *s, void *data)
{
	struct mt7996_phy *phy = s->private;
	struct mt7996_dev *dev = phy->dev;
	unsigned long usage_bitmap[2] = {0};
	char buf[20];
	int i, j;
	u8 band = phy->mt76->band_idx;

	usage_bitmap[0] = (unsigned long)mt76_rr(dev, MT_WF_RMAC_SRAM_BITMAP0(band));
	usage_bitmap[1] = (unsigned long)mt76_rr(dev, MT_WF_RMAC_SRAM_BITMAP1(band));

	for (i = 0; i < 2; i++) {
		for_each_set_bit(j, &usage_bitmap[i], 32) {
			u32 om_addr_lo, bssid_lo;
			u16 om_addr_hi, bssid_hi;
			bool om_en, bss_en, mbss_en, color_en, pcolor_en;
			u8 mode, mbss_msk, color;
			int idx = i * 32 + j;
			u32 req = MT_WF_RMAC_MEM_CTRL_TRIG |
				  u32_encode_bits(i * 32 + j, MT_WF_RMAC_MEM_CTRL_TDX);
			u32 dw[4];

			mt76_wr(dev, MT_WF_RMAC_MEM_CTRL(band), req);
			dw[0] = mt76_rr(dev, MT_WF_RMAC_SRAM_DATA0(band));
			dw[1] = mt76_rr(dev, MT_WF_RMAC_SRAM_DATA1(band));
			dw[2] = mt76_rr(dev, MT_WF_RMAC_SRAM_DATA2(band));
			dw[3] = mt76_rr(dev, MT_WF_RMAC_SRAM_DATA3(band));

			snprintf(buf, sizeof(buf), "[%d]", idx);

			seq_printf(s, "%-4s ", buf);

			om_en = FIELD_GET(MT_WF_RMAC_SRAM_OMAC_EN, dw[1]);
			if (om_en) {
				om_addr_lo = dw[0];
				om_addr_hi = FIELD_GET(MT_WF_RMAC_SRAM_OMAC_HI, dw[1]);

				snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
					 om_addr_lo & 0xff, om_addr_lo >> 8 & 0xff,
					 om_addr_lo >> 16 & 0xff, om_addr_lo >> 24,
					 om_addr_hi & 0xff, om_addr_hi >> 8);
			}
			seq_printf(s, "OwnMAC Addr: %-17s  ", om_en ? buf : "None");

			bss_en = FIELD_GET(MT_WF_RMAC_SRAM_BSSID_EN, dw[3]);

			if (!bss_en) {
				seq_puts(s, "No BSS\n");
				continue;
			}

			bssid_lo = dw[2];
			bssid_hi = FIELD_GET(MT_WF_RMAC_SRAM_BSSID_HI, dw[3]);

			mode = FIELD_GET(MT_WF_RMAC_SRAM_MODE, dw[3]);

			seq_printf(s, "BSSID: %02x:%02x:%02x:%02x:%02x:%02x  Mode %d  ",
				   bssid_lo & 0xff, bssid_lo >> 8 & 0xff,
				   bssid_lo >> 16 & 0xff, bssid_lo >> 24,
				   bssid_hi & 0xff, bssid_hi >> 8, mode);

			mbss_en = FIELD_GET(MT_WF_RMAC_SRAM_MBSSID_EN, dw[3]);

			if (mbss_en) {
				mbss_msk = FIELD_GET(MT_WF_RMAC_SRAM_MBSSID_MASK, dw[3]);
				snprintf(buf, sizeof(buf), "0x%x", mbss_msk);
			}

			seq_printf(s, "MBSS Mask: %-4s  ", mbss_en ? buf : "None");

			pcolor_en = FIELD_GET(MT_WF_RMAC_SRAM_BSS_PCOLOR_EN, dw[3]);
			color_en = FIELD_GET(MT_WF_RMAC_SRAM_BSS_COLOR_EN, dw[3]) || pcolor_en;

			if (color_en) {
				color = FIELD_GET(MT_WF_RMAC_SRAM_BSS_COLOR, dw[3]);
				snprintf(buf, sizeof(buf), "%-2d%s", color,
				         pcolor_en ? " (partial)" : "");
			}
			seq_printf(s, "Color: %s\n", color_en ? buf : "None");
		}
	}

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(mt7996_rmac_table);

static int
mt7996_phy_info_show(struct seq_file *file, void *data)
{
	struct mt7996_dev *dev = file->private;
	struct mt7996_phy *phy;

	mutex_lock(&dev->mt76.mutex);

	mt7996_for_each_phy(dev, phy) {
		seq_printf(file, "MAC: %pM\n", phy->mt76->macaddr);
		seq_printf(file, "Band: %d\n", phy->mt76->band_idx);
	}

	mutex_unlock(&dev->mt76.mutex);

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(mt7996_phy_info);

struct mt7996_txo_worker_info {
	char *buf;
	int sofar;
	int size;
};

// TODO:  Set txo per link instead of assuming it is in deflink
//  but maybe first, see if mtk firmware has what we need for txo
//  as it may not ever support what we want. --Ben
static void mt7996_txo_worker(void *wi_data, struct ieee80211_sta *sta)
{
	struct mt7996_txo_worker_info *wi = wi_data;
	struct mt7996_sta *msta = (struct mt7996_sta *)sta->drv_priv;
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

static ssize_t mt7996_read_set_rate_override(struct file *file,
					     char __user *user_buf,
					     size_t count, loff_t *ppos)
{
	struct mt7996_dev *dev = file->private_data;
	struct ieee80211_hw *hw = dev->mphy.hw;
	char *buf2;
	int size = 8000;
	int rv, sofar;
	struct mt7996_txo_worker_info wi;
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

	ieee80211_iterate_stations_atomic(hw, mt7996_txo_worker, &wi);

	rv = simple_read_from_buffer(user_buf, count, ppos, buf2, wi.sofar);
	kfree(buf2);
	return rv;
}

/* Set the rates for specific types of traffic.
 */
static ssize_t mt7996_write_set_rate_override(struct file *file,
					      const char __user *user_buf,
					      size_t count, loff_t *ppos)
{
	struct mt7996_dev *dev = file->private_data;
	struct mt7996_sta *msta;
	struct ieee80211_vif *vif;
	struct mt76_testmode_data *td = NULL;
	struct wireless_dev *wdev;
	struct mt76_wcid *wcid;
	struct mt7996_sta_link *link;
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

#define MT7996_PARSE_LTOK(a, b)						\
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
					 "mt7996: set-rate-override: " #a \
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

			link = container_of(wcid, struct mt7996_sta_link, wcid);
			msta = link->sta;

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
				td = &msta->deflink.test;
				bufptr = buf + strlen(dev_name_match) - 1;

				MT7996_PARSE_LTOK(tpc, tx_power[0]);
				MT7996_PARSE_LTOK(sgi, tx_rate_sgi);
				MT7996_PARSE_LTOK(mcs, tx_rate_idx);
				MT7996_PARSE_LTOK(nss, tx_rate_nss);
				MT7996_PARSE_LTOK(pream, tx_rate_mode);
				MT7996_PARSE_LTOK(retries, tx_xmit_count);
				MT7996_PARSE_LTOK(dynbw, tx_dynbw);
				MT7996_PARSE_LTOK(bw, txbw);
				MT7996_PARSE_LTOK(active, txo_active);
				MT7996_PARSE_LTOK(ldpc, tx_rate_ldpc);
				MT7996_PARSE_LTOK(stbc, tx_rate_stbc);

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
					 "mt7996: set-rate-overrides, vdev %i(%s) active=%d tpc=%d sgi=%d ltf=%d mcs=%d"
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
				 "mt7996: set-rate-override, unknown netdev name: %s\n", buf);
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
	.read = mt7996_read_set_rate_override,
	.write = mt7996_write_set_rate_override,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static int
mt7996_sr_pp_enable_get(void *data, u64 *val)
{
	struct mt7996_dev *dev = data;

	*val = dev->sr_pp_enable;

	return 0;
}

static int
mt7996_sr_pp_enable_set(void *data, u64 val)
{
	struct mt7996_dev *dev = data;
	int ret;
	bool en = !!val;

	if (en == dev->sr_pp_enable)
		return 0;

	ret = mt7996_mcu_set_sr_pp_en(dev, en);
	if (ret)
		return ret;

	dev->sr_pp_enable = en;

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(fops_sr_pp_enable, mt7996_sr_pp_enable_get,
			 mt7996_sr_pp_enable_set, "%lld\n");

static int
mt7996_uba_enable_get(void *data, u64 *val)
{
	struct mt7996_dev *dev = data;

	*val = dev->uba_enable;

	return 0;
}

static int
mt7996_uba_enable_set(void *data, u64 val)
{
	struct mt7996_dev *dev = data;
	int ret;
	bool en = !!val;

	if (en == dev->uba_enable)
		return 0;

	ret = mt7996_mcu_set_uba_en(dev, en);
	if (ret)
		return ret;

	dev->uba_enable = en;

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(fops_uba_enable, mt7996_uba_enable_get,
			 mt7996_uba_enable_set, "%lld\n");

static int
mt7996_mru_probe_enable_get(void *data, u64 *val)
{
	struct mt7996_phy *phy = data;

	*val = phy->mru_probe_enable;

	return 0;
}

static int
mt7996_mru_probe_enable_set(void *data, u64 val)
{
#define MRU_PROBE_ENABLE 1
	struct mt7996_phy *phy = data;
	int ret;
	bool en = !!val;

	if (en == phy->mru_probe_enable)
		return 0;

	if (en != MRU_PROBE_ENABLE)
		return 0;

	ret = mt7996_mcu_set_mru_probe_en(phy);
	if (ret)
		return ret;

	phy->mru_probe_enable = en;
	/* When enabling MRU probe, PP would also enter FW mode */
	phy->pp_mode = PP_FW_MODE;

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(fops_mru_probe_enable, mt7996_mru_probe_enable_get,
			 mt7996_mru_probe_enable_set, "%lld\n");

static int
mt7996_rx_group_5_enable_set(void *data, u64 val)
{
	struct mt7996_dev *dev = data;

	mutex_lock(&dev->mt76.mutex);

	dev->rx_group_5_enable = !!val;

	/* Enabled if we requested enabled OR if monitor mode is enabled. */
	mt76_rmw_field(dev, MT_DMA_DCR0(0), MT_DMA_DCR0_RXD_G5_EN,
		       dev->rx_group_5_enable);
	mt76_testmode_reset(dev->phy.mt76, true);

	mutex_unlock(&dev->mt76.mutex);
	return 0;
}

static int
mt7996_rx_group_5_enable_get(void *data, u64 *val)
{
	struct mt7996_dev *dev = data;

	*val = dev->rx_group_5_enable;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_rx_group_5_enable, mt7996_rx_group_5_enable_get,
			 mt7996_rx_group_5_enable_set, "%lld\n");

static void
mt7996_hw_queue_read(struct seq_file *s, u32 size,
		     const struct hw_queue_map *map)
{
	struct mt7996_phy *phy = s->private;
	struct mt7996_dev *dev = phy->dev;
	u32 i, val;

	val = mt76_rr(dev, MT_FL_Q_EMPTY);
	for (i = 0; i < size; i++) {
		u32 ctrl, head, tail, queued;

		if (val & BIT(map[i].index))
			continue;

		ctrl = BIT(31) | (map[i].pid << 10) | ((u32)map[i].qid << 24);
		mt76_wr(dev, MT_FL_Q0_CTRL, ctrl);

		head = mt76_get_field(dev, MT_FL_Q2_CTRL,
				      GENMASK(11, 0));
		tail = mt76_get_field(dev, MT_FL_Q2_CTRL,
				      GENMASK(27, 16));
		queued = mt76_get_field(dev, MT_FL_Q3_CTRL,
					GENMASK(11, 0));

		seq_printf(s, "\t%s: ", map[i].name);
		seq_printf(s, "queued:0x%03x head:0x%03x tail:0x%03x\n",
			   queued, head, tail);
	}
}

static void
mt7996_sta_hw_queue_read(void *data, struct ieee80211_sta *sta)
{
	struct mt7996_sta *msta = (struct mt7996_sta *)sta->drv_priv;
	struct mt7996_vif *mvif = msta->vif;
	struct mt7996_phy *phy = mt7996_vif_link_phy(&mvif->deflink);
	struct ieee80211_link_sta *link_sta;
	struct seq_file *s = data;
	struct ieee80211_vif *vif;
	struct mt7996_dev *dev;
	unsigned int link_id;

	if (!phy)
		return;

	vif = container_of((void *)mvif, struct ieee80211_vif, drv_priv);
	dev = phy->dev;

	rcu_read_lock();

	for_each_sta_active_link(vif, sta, link_sta, link_id) {
		struct mt7996_sta_link *msta_link;
		struct mt76_vif_link *mlink;
		u8 ac;

		mlink = rcu_dereference(mvif->mt76.link[link_id]);
		if (!mlink)
			continue;

		msta_link = mt7996_sta_link(msta, link_id);
		if (!msta_link)
			continue;

		for (ac = 0; ac < 4; ac++) {
			u32 idx = msta_link->wcid.idx >> 5, qlen, ctrl, val;
			u8 offs = msta_link->wcid.idx & GENMASK(4, 0);

			ctrl = BIT(31) | BIT(11) | (ac << 24);
			val = mt76_rr(dev, MT_PLE_AC_QEMPTY(ac, idx));

			if (val & BIT(offs))
				continue;

			mt76_wr(dev,
				MT_FL_Q0_CTRL, ctrl | msta_link->wcid.idx);
			qlen = mt76_get_field(dev, MT_FL_Q3_CTRL,
					      GENMASK(11, 0));
			seq_printf(s, "\tSTA %pM wcid %d: AC%d%d queued:%d\n",
				   sta->addr, msta_link->wcid.idx,
				   mlink->wmm_idx, ac, qlen);
		}
	}

	rcu_read_unlock();
}

static int
mt7996_hw_queues_show(struct seq_file *file, void *data)
{
	struct mt7996_phy *phy = file->private;
	struct mt7996_dev *dev = phy->dev;
	static const struct hw_queue_map ple_queue_map[] = {
		{ "CPU_Q0",  0,  1, MT_CTX0	      },
		{ "CPU_Q1",  1,  1, MT_CTX0 + 1	      },
		{ "CPU_Q2",  2,  1, MT_CTX0 + 2	      },
		{ "CPU_Q3",  3,  1, MT_CTX0 + 3	      },
		{ "ALTX_Q0", 8,  2, MT_LMAC_ALTX0     },
		{ "BMC_Q0",  9,  2, MT_LMAC_BMC0      },
		{ "BCN_Q0",  10, 2, MT_LMAC_BCN0      },
		{ "PSMP_Q0", 11, 2, MT_LMAC_PSMP0     },
		{ "ALTX_Q1", 12, 2, MT_LMAC_ALTX0 + 4 },
		{ "BMC_Q1",  13, 2, MT_LMAC_BMC0  + 4 },
		{ "BCN_Q1",  14, 2, MT_LMAC_BCN0  + 4 },
		{ "PSMP_Q1", 15, 2, MT_LMAC_PSMP0 + 4 },
	};
	static const struct hw_queue_map pse_queue_map[] = {
		{ "CPU Q0",  0,  1, MT_CTX0	      },
		{ "CPU Q1",  1,  1, MT_CTX0 + 1	      },
		{ "CPU Q2",  2,  1, MT_CTX0 + 2	      },
		{ "CPU Q3",  3,  1, MT_CTX0 + 3	      },
		{ "HIF_Q0",  8,  0, MT_HIF0	      },
		{ "HIF_Q1",  9,  0, MT_HIF0 + 1	      },
		{ "HIF_Q2",  10, 0, MT_HIF0 + 2	      },
		{ "HIF_Q3",  11, 0, MT_HIF0 + 3	      },
		{ "HIF_Q4",  12, 0, MT_HIF0 + 4	      },
		{ "HIF_Q5",  13, 0, MT_HIF0 + 5	      },
		{ "LMAC_Q",  16, 2, 0		      },
		{ "MDP_TXQ", 17, 2, 1		      },
		{ "MDP_RXQ", 18, 2, 2		      },
		{ "SEC_TXQ", 19, 2, 3		      },
		{ "SEC_RXQ", 20, 2, 4		      },
	};
	u32 val, head, tail;

	/* ple queue */
	val = mt76_rr(dev, MT_PLE_FREEPG_CNT);
	head = mt76_get_field(dev, MT_PLE_FREEPG_HEAD_TAIL, GENMASK(11, 0));
	tail = mt76_get_field(dev, MT_PLE_FREEPG_HEAD_TAIL, GENMASK(27, 16));
	seq_puts(file, "PLE page info:\n");
	seq_printf(file,
		   "\tTotal free page: 0x%08x head: 0x%03x tail: 0x%03x\n",
		   val, head, tail);

	val = mt76_rr(dev, MT_PLE_PG_HIF_GROUP);
	head = mt76_get_field(dev, MT_PLE_HIF_PG_INFO, GENMASK(11, 0));
	tail = mt76_get_field(dev, MT_PLE_HIF_PG_INFO, GENMASK(27, 16));
	seq_printf(file, "\tHIF free page: 0x%03x res: 0x%03x used: 0x%03x\n",
		   val, head, tail);

	seq_puts(file, "PLE non-empty queue info:\n");
	mt7996_hw_queue_read(file, ARRAY_SIZE(ple_queue_map),
			     &ple_queue_map[0]);

	/* iterate per-sta ple queue */
	ieee80211_iterate_stations_atomic(phy->mt76->hw,
					  mt7996_sta_hw_queue_read, file);
	phy = mt7996_phy2(dev);
	if (phy)
		ieee80211_iterate_stations_atomic(phy->mt76->hw,
						  mt7996_sta_hw_queue_read, file);
	phy = mt7996_phy3(dev);
	if (phy)
		ieee80211_iterate_stations_atomic(phy->mt76->hw,
						  mt7996_sta_hw_queue_read, file);

	/* pse queue */
	seq_puts(file, "PSE non-empty queue info:\n");
	mt7996_hw_queue_read(file, ARRAY_SIZE(pse_queue_map),
			     &pse_queue_map[0]);

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(mt7996_hw_queues);

static int
mt7996_twt_stats(struct seq_file *s, void *data)
{
	struct mt7996_dev *dev = dev_get_drvdata(s->private);
	struct mt7996_twt_flow *iter;

	rcu_read_lock();

	seq_puts(s, "     wcid |       id |    flags |      exp | mantissa");
	seq_puts(s, " | duration |            tsf |\n");
	list_for_each_entry_rcu(iter, &dev->twt_list, list)
		seq_printf(s,
			   "%9d | %8d | %5c%c%c%c | %8d | %8d | %8d | %14lld |\n",
			   iter->wcid, iter->id,
			   iter->sched ? 's' : 'u',
			   iter->protection ? 'p' : '-',
			   iter->trigger ? 't' : '-',
			   iter->flowtype ? '-' : 'a',
			   iter->exp, iter->mantissa,
			   iter->duration, iter->tsf);

	rcu_read_unlock();

	return 0;
}

/* The index of RF registers use the generic regidx, combined with two parts:
 * WF selection [31:24] and offset [23:0].
 */
static int
mt7996_rf_regval_get(void *data, u64 *val)
{
	struct mt7996_dev *dev = data;
	u32 regval;
	int ret;

	ret = mt7996_mcu_rf_regval(dev, dev->mt76.debugfs_reg, &regval, false);
	if (ret)
		return ret;

	*val = regval;

	return 0;
}

static int
mt7996_rf_regval_set(void *data, u64 val)
{
	struct mt7996_dev *dev = data;
	u32 val32 = val;

	return mt7996_mcu_rf_regval(dev, dev->mt76.debugfs_reg, &val32, true);
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_rf_regval, mt7996_rf_regval_get,
			 mt7996_rf_regval_set, "0x%08llx\n");

static int
mt7996_txpower_level_set(void *data, u64 val)
{
	struct mt7996_dev *dev = data;
	struct mt7996_phy *phy = &dev->phy;
	int ret;

	if (val > 100)
		return -EINVAL;

	ret = mt7996_mcu_set_tx_power_ctrl(phy, UNI_TXPOWER_PERCENTAGE_CTRL, !!val);
	if (ret)
		return ret;

	return mt7996_mcu_set_tx_power_ctrl(phy, UNI_TXPOWER_PERCENTAGE_DROP_CTRL, val);
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_txpower_level, NULL,
			 mt7996_txpower_level_set, "%lld\n");

static int
mt7996_scs_enable_set(void *data, u64 val)
{
	struct mt7996_phy *phy = data;
	return mt7996_mcu_set_scs(phy, (u8) val);
}
DEFINE_DEBUGFS_ATTRIBUTE(fops_scs_enable, NULL,
			 mt7996_scs_enable_set, "%lld\n");

static ssize_t
mt7996_get_txpower_info(struct file *file, char __user *user_buf,
			size_t count, loff_t *ppos)
{
	struct mt7996_dev *dev = file->private_data;
	struct mt7996_phy *phy = &dev->phy;
	struct mt7996_mcu_txpower_event *event;
	struct txpower_basic_info *basic_info;
	struct mt76_phy *mphy = phy->mt76;
	struct ieee80211_hw *hw = mphy->hw;
	static const size_t size = 2048;
	int len = 0;
	ssize_t ret;
	char *buf;
	s8 single_nss_txpower;

	buf = kzalloc(size, GFP_KERNEL);
	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (!buf || !event) {
		ret = -ENOMEM;
		goto out;
	}

	ret = mt7996_mcu_get_tx_power_info(phy, BASIC_INFO, event);
	if (ret ||
	    le32_to_cpu(event->basic_info.category) != UNI_TXPOWER_BASIC_INFO)
		goto out;

	basic_info = &event->basic_info;

	len += scnprintf(buf + len, size - len,
			 "======================== BASIC INFO ========================\n");
	len += scnprintf(buf + len, size - len, "    Band Index: %d, Channel Band: %d\n",
			 basic_info->band_idx, basic_info->band);
	len += scnprintf(buf + len, size - len, "    PA Type: %s\n",
			 basic_info->is_epa ? "ePA" : "iPA");
	len += scnprintf(buf + len, size - len, "    LNA Type: %s\n",
			 basic_info->is_elna ? "eLNA" : "iLNA");

	len += scnprintf(buf + len, size - len,
			 "------------------------------------------------------------\n");
	len += scnprintf(buf + len, size - len, "    SKU: %s\n",
			 basic_info->sku_enable ? "enable" : "disable");
	len += scnprintf(buf + len, size - len, "    Percentage Control: %s\n",
			 basic_info->percentage_ctrl_enable ? "enable" : "disable");
	len += scnprintf(buf + len, size - len, "    Power Drop: %d [dBm]\n",
			 basic_info->power_drop_level >> 1);
	len += scnprintf(buf + len, size - len, "    Backoff: %s\n",
			 basic_info->bf_backoff_enable ? "enable" : "disable");
	len += scnprintf(buf + len, size - len, "    TX Front-end Loss:  %d, %d, %d, %d\n",
			 basic_info->front_end_loss_tx[0], basic_info->front_end_loss_tx[1],
			 basic_info->front_end_loss_tx[2], basic_info->front_end_loss_tx[3]);
	len += scnprintf(buf + len, size - len, "    RX Front-end Loss:  %d, %d, %d, %d\n",
			 basic_info->front_end_loss_rx[0], basic_info->front_end_loss_rx[1],
			 basic_info->front_end_loss_rx[2], basic_info->front_end_loss_rx[3]);
	len += scnprintf(buf + len, size - len,
			 "    MU TX Power Mode:  %s\n",
			 basic_info->mu_tx_power_manual_enable ? "manual" : "auto");
	len += scnprintf(buf + len, size - len,
			 "    MU TX Power (Auto / Manual): %d / %d [0.5 dBm]\n",
			 basic_info->mu_tx_power_auto, basic_info->mu_tx_power_manual);
	len += scnprintf(buf + len, size - len,
			 "    Thermal Compensation:  %s\n",
			 basic_info->thermal_compensate_enable ? "enable" : "disable");
	len += scnprintf(buf + len, size - len,
			 "    Thermal Compensation Value: %d\n",
			 basic_info->thermal_compensate_value);

	len += scnprintf(buf + len, size - len,
			 "    PHY Power Bound: %d\n",
			 mt7996_get_power_bound(mphy, hw->conf.power_level, &single_nss_txpower));
	len += scnprintf(buf + len, size - len,
			 "    HW Conf Power Level: %d\n",
			 hw->conf.power_level);
	len += scnprintf(buf + len, size - len,
			 "    Per-Chain TX-Power Cur: %d 1/2dB\n",
			 mphy->txpower_cur);
	len += scnprintf(buf + len, size - len,
			 "    PHY tx-front-end-loss: %d\n",
			 phy->tx_front_end_loss);
	len += scnprintf(buf + len, size - len,
			 "    PHY tx-front-end-loss-acquired: %d\n",
			 phy->tx_front_end_loss_acquired);

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);

out:
	kfree(buf);
	kfree(event);
	return ret;
}

static const struct file_operations mt7996_txpower_info_fops = {
	.read = mt7996_get_txpower_info,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

#define mt7996_txpower_puts(rate)							\
({											\
	len += scnprintf(buf + len, size - len, "%-21s:", #rate " (TMAC)");		\
	for (i = 0; i < mt7996_sku_group_len[SKU_##rate]; i++, offs++)			\
		len += scnprintf(buf + len, size - len, " %6d",				\
				 event->phy_rate_info.frame_power[offs][band_idx]);	\
	len += scnprintf(buf + len, size - len, "\n");					\
})

static ssize_t
__mt7996_get_txpower_sku(struct file *file, char __user *user_buf,
			 size_t count, loff_t *ppos, struct mt7996_mcu_txpower_event *event,
			 char* buf, size_t size)
{
	struct mt7996_dev *dev = file->private_data;
	struct mt7996_phy *phy = &dev->phy;
	u8 band_idx = phy->mt76->band_idx;
	int i, offs = 0, len = 0;
	ssize_t ret;
	u32 reg;

	len += scnprintf(buf + len, size - len,
			 "\nPhy %d TX Power Table (Channel %d)\n",
			 band_idx, phy->mt76->chandef.chan->hw_value);
	len += scnprintf(buf + len, size - len, "%-21s  %6s %6s %6s %6s\n",
			 " ", "1m", "2m", "5m", "11m");
	mt7996_txpower_puts(CCK);

	len += scnprintf(buf + len, size - len,
			 "%-21s  %6s %6s %6s %6s %6s %6s %6s %6s\n",
			 " ", "6m", "9m", "12m", "18m", "24m", "36m", "48m",
			 "54m");
	mt7996_txpower_puts(OFDM);

	len += scnprintf(buf + len, size - len,
			 "%-21s  %6s %6s %6s %6s %6s %6s %6s %6s\n",
			 " ", "mcs0", "mcs1", "mcs2", "mcs3", "mcs4",
			 "mcs5", "mcs6", "mcs7");
	mt7996_txpower_puts(HT20);

	len += scnprintf(buf + len, size - len,
			 "%-21s  %6s %6s %6s %6s %6s %6s %6s %6s %6s\n",
			 " ", "mcs0", "mcs1", "mcs2", "mcs3", "mcs4", "mcs5",
			 "mcs6", "mcs7", "mcs32");
	mt7996_txpower_puts(HT40);

	len += scnprintf(buf + len, size - len,
			 "%-21s  %6s %6s %6s %6s %6s %6s %6s %6s %6s %6s %6s %6s\n",
			 " ", "mcs0", "mcs1", "mcs2", "mcs3", "mcs4", "mcs5",
			 "mcs6", "mcs7", "mcs8", "mcs9", "mcs10", "mcs11");
	mt7996_txpower_puts(VHT20);
	mt7996_txpower_puts(VHT40);
	mt7996_txpower_puts(VHT80);
	mt7996_txpower_puts(VHT160);
	mt7996_txpower_puts(HE26);
	mt7996_txpower_puts(HE52);
	mt7996_txpower_puts(HE106);
	mt7996_txpower_puts(HE242);
	mt7996_txpower_puts(HE484);
	mt7996_txpower_puts(HE996);
	mt7996_txpower_puts(HE2x996);

	len += scnprintf(buf + len, size - len,
			 "%-21s  %6s %6s %6s %6s %6s %6s %6s %6s ",
			 " ", "mcs0", "mcs1", "mcs2", "mcs3", "mcs4", "mcs5", "mcs6", "mcs7");
	len += scnprintf(buf + len, size - len,
			 "%6s %6s %6s %6s %6s %6s %6s %6s\n",
			 "mcs8", "mcs9", "mcs10", "mcs11", "mcs12", "mcs13", "mcs14", "mcs15");
	mt7996_txpower_puts(EHT26);
	mt7996_txpower_puts(EHT52);
	mt7996_txpower_puts(EHT106);
	mt7996_txpower_puts(EHT242);
	mt7996_txpower_puts(EHT484);
	mt7996_txpower_puts(EHT996);
	mt7996_txpower_puts(EHT2x996);
	mt7996_txpower_puts(EHT4x996);
	mt7996_txpower_puts(EHT26_52);
	mt7996_txpower_puts(EHT26_106);
	mt7996_txpower_puts(EHT484_242);
	mt7996_txpower_puts(EHT996_484);
	mt7996_txpower_puts(EHT996_484_242);
	mt7996_txpower_puts(EHT2x996_484);
	mt7996_txpower_puts(EHT3x996);
	mt7996_txpower_puts(EHT3x996_484);

	len += scnprintf(buf + len, size - len, "\nePA Gain: %d\n",
			 event->phy_rate_info.epa_gain);
	len += scnprintf(buf + len, size - len, "Max Power Bound: %d\n",
			 event->phy_rate_info.max_power_bound);
	len += scnprintf(buf + len, size - len, "Min Power Bound: %d\n",
			 event->phy_rate_info.min_power_bound);

	reg = MT_WF_PHYDFE_BAND_TPC_CTRL_STAT0(band_idx);
	len += scnprintf(buf + len, size - len,
			 "BBP TX Power (target power from TMAC)  : %6ld [0.5 dBm]\n",
			 mt76_get_field(dev, reg, MT_WF_PHY_TPC_POWER_TMAC));
	len += scnprintf(buf + len, size - len,
			 "BBP TX Power (target power from RMAC)  : %6ld [0.5 dBm]\n",
			 mt76_get_field(dev, reg, MT_WF_PHY_TPC_POWER_RMAC));
	len += scnprintf(buf + len, size - len,
			 "BBP TX Power (TSSI module power input)  : %6ld [0.5 dBm]\n",
			 mt76_get_field(dev, reg, MT_WF_PHY_TPC_POWER_TSSI));

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);

	return ret;
}

static ssize_t
mt7996_get_txpower_sku(struct file *file, char __user *user_buf,
		       size_t count, loff_t *ppos)
{
	struct mt7996_phy *phy = file->private_data;
	struct mt7996_mcu_txpower_event *event;
	static const size_t size = 5120;
	char *buf;
	int ret;

	buf = kzalloc(size, GFP_KERNEL);
	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (!buf || !event) {
		ret = -ENOMEM;
		goto out;
	}

	ret = mt7996_mcu_get_tx_power_info(phy, PHY_RATE_INFO, event);
	if (ret ||
	    le32_to_cpu(event->phy_rate_info.category) != UNI_TXPOWER_PHY_RATE_INFO)
		goto out;

	ret = __mt7996_get_txpower_sku(file, user_buf, count, ppos, event, buf, size);

out:
	kfree(buf);
	kfree(event);

	return ret;
}

static const struct file_operations mt7996_txpower_sku_fops = {
	.read = mt7996_get_txpower_sku,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t
mt7996_get_txpower_default(struct file *file, char __user *user_buf,
			   size_t count, loff_t *ppos)
{
	struct mt7996_dev *dev = file->private_data;
	struct mt7996_phy *phy = &dev->phy;
	static const size_t size = 5120;
	char *buf;
	int ret;
	int len = 0;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		goto out;
	}

	if (phy->default_txpower) {
		ret = __mt7996_get_txpower_sku(file, user_buf, count, ppos, phy->default_txpower, buf, size);
	}
	else {
		len += scnprintf(buf + len, size - len, "ERROR:  default_txpower is NULL\n");
		ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	}

out:
	kfree(buf);

	return ret;
}

static const struct file_operations mt7996_txpower_default_fops = {
	.read = mt7996_get_txpower_default,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

#define mt7996_txpower_path_puts(rate, arr_length)					\
({											\
	len += scnprintf(buf + len, size - len, "%-23s:", #rate " (TMAC)");		\
	for (i = 0; i < arr_length; i++, offs++)					\
		len += scnprintf(buf + len, size - len, " %4d",				\
				 event->backoff_table_info.frame_power[offs]);		\
	len += scnprintf(buf + len, size - len, "\n");					\
})

static ssize_t
mt7996_get_txpower_path(struct file *file, char __user *user_buf,
			size_t count, loff_t *ppos)
{
	struct mt7996_dev *dev = file->private_data;
	struct mt7996_phy *phy = &dev->phy;
	struct mt7996_mcu_txpower_event *event;
	static const size_t size = 5120;
	int i, offs = 0, len = 0;
	ssize_t ret;
	char *buf;

	buf = kzalloc(size, GFP_KERNEL);
	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (!buf || !event) {
		ret = -ENOMEM;
		goto out;
	}

	ret = mt7996_mcu_get_tx_power_info(phy, BACKOFF_TABLE_INFO, event);
	if (ret ||
	    le32_to_cpu(event->phy_rate_info.category) != UNI_TXPOWER_BACKOFF_TABLE_SHOW_INFO)
		goto out;

	len += scnprintf(buf + len, size - len, "\n%*c", 25, ' ');
	len += scnprintf(buf + len, size - len, "1T1S/2T1S/3T1S/4T1S/5T1S/2T2S/3T2S/4T2S/5T2S/"
			 "3T3S/4T3S/5T3S/4T4S/5T4S/5T5S\n");

	mt7996_txpower_path_puts(CCK, 5);
	mt7996_txpower_path_puts(OFDM, 5);
	mt7996_txpower_path_puts(BF-OFDM, 4);

	mt7996_txpower_path_puts(RU26, 15);
	mt7996_txpower_path_puts(BF-RU26, 15);
	mt7996_txpower_path_puts(RU52, 15);
	mt7996_txpower_path_puts(BF-RU52, 15);
	mt7996_txpower_path_puts(RU26_52, 15);
	mt7996_txpower_path_puts(BF-RU26_52, 15);
	mt7996_txpower_path_puts(RU106, 15);
	mt7996_txpower_path_puts(BF-RU106, 15);
	mt7996_txpower_path_puts(RU106_52, 15);
	mt7996_txpower_path_puts(BF-RU106_52, 15);

	mt7996_txpower_path_puts(BW20/RU242, 15);
	mt7996_txpower_path_puts(BF-BW20/RU242, 15);
	mt7996_txpower_path_puts(BW40/RU484, 15);
	mt7996_txpower_path_puts(BF-BW40/RU484, 15);
	mt7996_txpower_path_puts(RU242_484, 15);
	mt7996_txpower_path_puts(BF-RU242_484, 15);
	mt7996_txpower_path_puts(BW80/RU996, 15);
	mt7996_txpower_path_puts(BF-BW80/RU996, 15);
	mt7996_txpower_path_puts(RU484_996, 15);
	mt7996_txpower_path_puts(BF-RU484_996, 15);
	mt7996_txpower_path_puts(RU242_484_996, 15);
	mt7996_txpower_path_puts(BF-RU242_484_996, 15);
	mt7996_txpower_path_puts(BW160/RU996x2, 15);
	mt7996_txpower_path_puts(BF-BW160/RU996x2, 15);
	mt7996_txpower_path_puts(RU484_996x2, 15);
	mt7996_txpower_path_puts(BF-RU484_996x2, 15);
	mt7996_txpower_path_puts(RU996x3, 15);
	mt7996_txpower_path_puts(BF-RU996x3, 15);
	mt7996_txpower_path_puts(RU484_996x3, 15);
	mt7996_txpower_path_puts(BF-RU484_996x3, 15);
	mt7996_txpower_path_puts(BW320/RU996x4, 15);
	mt7996_txpower_path_puts(BF-BW320/RU996x4, 15);

	len += scnprintf(buf + len, size - len, "\nBackoff table: %s\n",
			 event->backoff_table_info.backoff_en ? "enable" : "disable");

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);

out:
	kfree(buf);
	kfree(event);
	return ret;
}

static const struct file_operations mt7996_txpower_path_fops = {
	.read = mt7996_get_txpower_path,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static int
mt7996_sr_enable_get(void *data, u64 *val)
{
	struct mt7996_phy *phy = data;

	*val = phy->sr_enable;

	return 0;
}

static int
mt7996_sr_enable_set(void *data, u64 val)
{
	struct mt7996_phy *phy = data;
	int ret;

	if (!!val == phy->sr_enable)
		return 0;

	ret = mt7996_mcu_set_sr_enable(phy, UNI_CMD_SR_CFG_SR_ENABLE, val, true);
	if (ret)
		return ret;

	return mt7996_mcu_set_sr_enable(phy, UNI_CMD_SR_CFG_SR_ENABLE, 0, false);
}
DEFINE_DEBUGFS_ATTRIBUTE(fops_sr_enable, mt7996_sr_enable_get,
			 mt7996_sr_enable_set, "%lld\n");

static int
mt7996_adjust_txp_by_loss_get(void *data, u64 *val)
{
	struct mt7996_dev *dev = data;
	struct mt7996_phy *phy = &dev->phy;

	*val = phy->adjust_txp_by_loss;

	return 0;
}

static int
mt7996_adjust_txp_by_loss_set(void *data, u64 val)
{
	struct mt7996_dev *dev = data;
	struct mt7996_phy *phy = &dev->phy;

	if (!!val == phy->adjust_txp_by_loss)
		return 0;

	phy->adjust_txp_by_loss = val;
	return mt7996_mcu_set_txpower_sku(phy);
}
DEFINE_DEBUGFS_ATTRIBUTE(fops_adjust_txp_by_loss, mt7996_adjust_txp_by_loss_get,
			 mt7996_adjust_txp_by_loss_set, "%lld\n");

static int
mt7996_sr_enhanced_enable_get(void *data, u64 *val)
{
	struct mt7996_phy *phy = data;

	*val = phy->enhanced_sr_enable;

	return 0;
}

static int
mt7996_sr_enhanced_enable_set(void *data, u64 val)
{
	struct mt7996_phy *phy = data;
	int ret;

	if (!!val == phy->enhanced_sr_enable)
		return 0;

	ret = mt7996_mcu_set_sr_enable(phy, UNI_CMD_SR_HW_ENHANCE_SR_ENABLE, val, true);
	if (ret)
		return ret;

	return mt7996_mcu_set_sr_enable(phy, UNI_CMD_SR_HW_ENHANCE_SR_ENABLE, 0, false);
}
DEFINE_DEBUGFS_ATTRIBUTE(fops_sr_enhanced_enable, mt7996_sr_enhanced_enable_get,
			 mt7996_sr_enhanced_enable_set, "%lld\n");

static int
mt7996_sr_stats_show(struct seq_file *file, void *data)
{
	struct mt7996_phy *phy = file->private;

	mt7996_mcu_set_sr_enable(phy, UNI_CMD_SR_HW_IND, 0, false);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(mt7996_sr_stats);

static int
mt7996_sr_scene_cond_show(struct seq_file *file, void *data)
{
	struct mt7996_phy *phy = file->private;

	return mt7996_mcu_set_sr_enable(phy, UNI_CMD_SR_SW_SD, 0, false);
}
DEFINE_SHOW_ATTRIBUTE(mt7996_sr_scene_cond);

static int mt7996_pp_alg_show(struct seq_file *s, void *data)
{
	struct mt7996_phy *phy = s->private;
	struct mt7996_dev *dev = phy->dev;

	dev_info(dev->mt76.dev, "pp_mode = %d\n", phy->pp_mode);
	mt7996_mcu_set_pp_alg_ctrl(phy, PP_ALG_GET_STATISTICS);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(mt7996_pp_alg);

int mt7996_init_band_debugfs(struct mt7996_phy *phy)
{
	struct mt7996_dev *dev = phy->dev;
	struct dentry *dir;
	char dir_name[10];

	if (!dev->debugfs_dir)
		return -EINVAL;

	snprintf(dir_name, sizeof(dir_name), "band%d", phy->mt76->band_idx);

	dir = debugfs_create_dir(dir_name, dev->debugfs_dir);
	if (!dir)
		return -ENOMEM;

	debugfs_create_file("hw-queues", 0400, dir, phy,
			    &mt7996_hw_queues_fops);
	mt76_debugfs_add_tx_queues(phy->mt76, dir);
	debugfs_create_file("sys_recovery", 0600, dir, phy,
			    &mt7996_sys_recovery_ops);
	debugfs_create_file("tx_stats", 0400, dir, phy, &mt7996_tx_stats_fops);
	debugfs_create_file("rxfilter", 0400, dir, phy, &mt7996_rxfilter_fops);
	debugfs_create_file("sr_enable", 0600, dir, phy, &fops_sr_enable);
	debugfs_create_file("sr_stats", 0400, dir, phy, &mt7996_sr_stats_fops);
	debugfs_create_file("sr_enhanced_enable", 0600, dir, phy, &fops_sr_enhanced_enable);
	debugfs_create_file("sr_scene_cond", 0400, dir, phy, &mt7996_sr_scene_cond_fops);
	debugfs_create_file("rmac_table", 0400, dir, phy, &mt7996_rmac_table_fops);

#ifdef CONFIG_MTK_DEBUG
	mt7996_mtk_init_band_debugfs(phy, dir);
	mt7996_mtk_init_band_debugfs_internal(phy, dir);
#endif
	return 0;
}

int mt7996_init_dev_debugfs(struct mt7996_phy *phy)
{
	struct mt7996_dev *dev = phy->dev;
	struct dentry *dir;

	dir = mt76_register_debugfs_fops(phy->mt76, NULL);
	if (!dir)
		return -ENOMEM;
	debugfs_create_file("fw_debug_wm", 0600, dir, dev, &fops_fw_debug_wm);
	debugfs_create_file("fw_debug_wa", 0600, dir, dev, &fops_fw_debug_wa);
	debugfs_create_file("fw_debug_bin", 0600, dir, dev, &fops_fw_debug_bin);

	if (is_mt7992(&dev->mt76)) {
		debugfs_create_file("sr_pp_enable", 0600, dir, dev,
				    &fops_sr_pp_enable);
		debugfs_create_file("uba_enable", 0600, dir, dev, &fops_uba_enable);
		debugfs_create_file("mru_probe_enable", 0600, dir, phy,
				    &fops_mru_probe_enable);
	}
	/* TODO: wm fw cpu utilization */
	debugfs_create_file("fw_util_wa", 0400, dir, dev,
			    &mt7996_fw_util_wa_fops);
	debugfs_create_file("rx_group_5_enable", 0600, dir, dev, &fops_rx_group_5_enable);
	debugfs_create_file("implicit_txbf", 0600, dir, dev,
			    &fops_implicit_txbf);
	debugfs_create_devm_seqfile(dev->mt76.dev, "twt_stats", dir,
				    mt7996_twt_stats);
	mt76_debugfs_add_mcu_queues(&dev->mt76, dir);
	debugfs_create_file("rf_regval", 0600, dir, dev, &fops_rf_regval);
	debugfs_create_u32("ignore_radar", 0600, dir,
			   &dev->ignore_radar);
	debugfs_create_file("set_rate_override", 0600, dir,
			    dev, &fops_set_rate_override);

	debugfs_create_file("phy_info", 0400, dir, dev, &mt7996_phy_info_fops);

	debugfs_create_file("txpower_level", 0600, dir, dev, &fops_txpower_level);
	debugfs_create_file("txpower_info", 0600, dir, dev, &mt7996_txpower_info_fops);
	debugfs_create_file("txpower_sku", 0600, dir, dev, &mt7996_txpower_sku_fops);
	debugfs_create_file("txpower_default", 0600, dir, dev, &mt7996_txpower_default_fops);
	debugfs_create_file("txpower_path", 0600, dir, dev, &mt7996_txpower_path_fops);
	debugfs_create_file("adjust_txp_by_loss", 0600, dir, dev, &fops_adjust_txp_by_loss);

	debugfs_create_bool("mgmt_pwr_enhance", 0600, dir, &dev->mt76.mgmt_pwr_enhance);
	debugfs_create_file("scs_enable", 0200, dir, phy, &fops_scs_enable);

	debugfs_create_file("pp_alg", 0200, dir, phy, &mt7996_pp_alg_fops);

	debugfs_create_u32("dfs_hw_pattern", 0400, dir, &dev->hw_pattern);
	debugfs_create_file("radar_trigger", 0200, dir, dev,
			    &fops_radar_trigger);
	debugfs_create_devm_seqfile(dev->mt76.dev, "rdd_monitor", dir,
				    mt7996_rdd_monitor);

	if (phy == &dev->phy) {
		dev->debugfs_dir = dir;
#ifdef CONFIG_MTK_DEBUG
		mt7996_mtk_init_dev_debugfs_internal(phy, dir);
#endif
	}
#ifdef CONFIG_MTK_DEBUG
	debugfs_create_u16("wlan_idx", 0600, dir, &dev->wlan_idx);
	mt7996_mtk_init_dev_debugfs(dev, dir);
#endif

	return 0;
}

static void
mt7996_debugfs_write_fwlog(struct mt7996_dev *dev, const void *hdr, int hdrlen,
			   const void *data, int len)
{
	static DEFINE_SPINLOCK(lock);
	unsigned long flags;
	void *dest;

	if (!dev->relay_fwlog)
		return;

	spin_lock_irqsave(&lock, flags);
	dest = relay_reserve(dev->relay_fwlog, hdrlen + len + 4);
	if (dest) {
		*(u32 *)dest = hdrlen + len;
		dest += 4;

		if (hdrlen) {
			memcpy(dest, hdr, hdrlen);
			dest += hdrlen;
		}

		memcpy(dest, data, len);
		relay_flush(dev->relay_fwlog);
	}
	spin_unlock_irqrestore(&lock, flags);
}

void mt7996_debugfs_rx_fw_monitor(struct mt7996_dev *dev, const void *data, int len)
{
	struct {
		__le32 magic;
		u8 version;
		u8 _rsv;
		__le16 serial_id;
		__le32 timestamp;
		__le16 msg_type;
		__le16 len;
	} hdr = {
		.version = 0x1,
		.magic = cpu_to_le32(FW_BIN_LOG_MAGIC),
		.msg_type = cpu_to_le16(PKT_TYPE_RX_FW_MONITOR),
	};

	if (!dev->relay_fwlog)
		return;

	hdr.serial_id = cpu_to_le16(dev->fw_debug_seq++);
	hdr.timestamp = cpu_to_le32(mt76_rr(dev, MT_LPON_FRCR(0)));
	hdr.len = *(__le16 *)data;
	mt7996_debugfs_write_fwlog(dev, &hdr, sizeof(hdr), data, len);
}

bool mt7996_debugfs_rx_log(struct mt7996_dev *dev, const void *data, int len)
{
	if (get_unaligned_le32(data) != FW_BIN_LOG_MAGIC)
		return false;

	if (dev->relay_fwlog)
		mt7996_debugfs_write_fwlog(dev, NULL, 0, data, len);

	return true;
}

#ifdef CONFIG_MAC80211_DEBUGFS

static int
mt7996_link_wtbl_show(struct seq_file *file, void *data)
{
	struct ieee80211_bss_conf *conf = file->private;
	struct mt7996_vif *mvif = NULL;
	struct mt7996_dev *dev = NULL;
	struct mt76_vif_link *mlink = NULL;
	int ret = 0;

	if (!conf || !conf->vif)
		return -ENOENT;

	mvif = (struct mt7996_vif *)conf->vif->drv_priv;

	if (!mt7996_vif_link_phy(&mvif->deflink))
		return -ENOENT;

	dev = mt7996_vif_link_phy(&mvif->deflink)->dev;

	if (!dev)
		return -ENOENT;

	mutex_lock(&dev->mt76.mutex);

	mlink = mt76_dereference(mvif->mt76.link[conf->link_id], &dev->mt76);

	/* WLAN 0 is reserved for control frames, and isn't attached to a link.
	 * 0 here likely means uninitialized.
	 */
	if (!mlink || !mlink->wcid || mlink->wcid->idx == 0) {
		ret = -ENOENT;
		goto out;
	}

	mt7996_wtbl_dump(file, dev, mlink->wcid->idx);

out:
	mutex_unlock(&dev->mt76.mutex);

	return ret;
}
DEFINE_SHOW_ATTRIBUTE(mt7996_link_wtbl);


void mt7996_link_add_debugfs(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			     struct ieee80211_bss_conf *link_conf, struct dentry *dir)
{
	debugfs_create_file("wtbl_info", 0600, dir, link_conf, &mt7996_link_wtbl_fops);
}

static int
mt7996_queues_show(struct seq_file *s, void *data)
{
	struct ieee80211_sta *sta = s->private;

	mt7996_sta_hw_queue_read(s, sta);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(mt7996_queues);

static int
mt7996_sta_links_info_show(struct seq_file *s, void *data)
{
	struct ieee80211_sta *sta = s->private;
	struct mt7996_sta *msta = (struct mt7996_sta *)sta->drv_priv;
	u64 tx_cnt = 0, tx_fails = 0, tx_retries = 0, rx_cnt = 0;
	struct mt7996_dev *dev = mt7996_vif_link_phy(&msta->vif->deflink)->dev;
	unsigned long valid_links;
	u8 link_id;

	seq_printf(s, "primary link, link ID = %d\n", msta->deflink_id);
	seq_printf(s, "valid links = 0x%x\n", sta->valid_links);

	mutex_lock(&dev->mt76.mutex);
	valid_links = sta->valid_links ?: BIT(0);
	for_each_set_bit(link_id, &valid_links, IEEE80211_MLD_MAX_NUM_LINKS) {
		struct mt7996_sta_link *msta_link =
			mt76_dereference(msta->link[link_id], &dev->mt76);
		struct mt76_wcid *wcid;

		if (!msta_link)
			continue;

		wcid = &msta_link->wcid;

		tx_cnt += wcid->stats.tx_attempts;
		tx_fails += wcid->stats.tx_failed;
		tx_retries += wcid->stats.tx_retries;
		rx_cnt += wcid->stats.rx_packets;

		seq_printf(s, "link%d: wcid=%d, phy=%d, link_valid=%d\n",
			    wcid->link_id, wcid->idx, wcid->phy_idx, wcid->link_valid);
	}
	mutex_unlock(&dev->mt76.mutex);

	/* PER may be imprecise, because MSDU total and failed counts
	 * are updated at different times.
	 */
	seq_printf(s, "TX MSDU Count: %llu\n", tx_cnt);
	seq_printf(s, "TX MSDU Fails: %llu (PER: %llu.%llu%%)\n", tx_fails,
		   tx_cnt ? tx_fails * 1000 / tx_cnt / 10 : 0,
		   tx_cnt ? tx_fails * 1000 / tx_cnt % 10 : 0);
	seq_printf(s, "TX MSDU Retries: %llu\n", tx_retries);
	seq_printf(s, "RX MSDU Count: %llu\n", rx_cnt);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(mt7996_sta_links_info);

void mt7996_sta_add_debugfs(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			    struct ieee80211_sta *sta, struct dentry *dir)
{
	debugfs_create_file("hw-queues", 0400, dir, sta, &mt7996_queues_fops);
	debugfs_create_file("mt76_links_info", 0400, dir, sta,
			    &mt7996_sta_links_info_fops);
}

static ssize_t mt7996_link_sta_fixed_rate_set(struct file *file,
					      const char __user *user_buf,
					      size_t count, loff_t *ppos)
{
#define SHORT_PREAMBLE 0
#define LONG_PREAMBLE 1
	struct ieee80211_link_sta *link_sta = file->private_data;
	struct mt7996_sta *msta = (struct mt7996_sta *)link_sta->sta->drv_priv;
	struct mt7996_phy *link_phy = mt7996_vif_link_phy(&msta->vif->deflink);
	struct mt7996_sta_link *msta_link;
	struct ra_rate phy = {};
	struct mt7996_dev *dev;
	char buf[100];
	int ret;
	u16 gi, ltf;

	if (!link_phy)
		return -EINVAL;

	if (count >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	if (count && buf[count - 1] == '\n')
		buf[count - 1] = '\0';
	else
		buf[count] = '\0';

	/* mode - cck: 0, ofdm: 1, ht: 2, gf: 3, vht: 4, he_su: 8, he_er: 9 EHT: 15
	 * bw - bw20: 0, bw40: 1, bw80: 2, bw160: 3, BW320: 4
	 * mcs - cck: 0~4, ofdm: 0~7, ht: 0~32, vht: 0~9, he_su: 0~11, he_er: 0~2, eht: 0~13
	 * nss - vht: 1~4, he: 1~4, eht: 1~4, others: ignore
	 * gi - (ht/vht) lgi: 0, sgi: 1; (he) 0.8us: 0, 1.6us: 1, 3.2us: 2
	 * preamble - short: 1, long: 0
	 * spe - off: 0, on: 1
	 * ltf - 1xltf: 0, 2xltf: 1, 4xltf: 2
	 */
	dev = link_phy->dev;
	if (sscanf(buf, "%hhu %hhu %hhu %hhu %hu %hhu %hhu %hhu %hhu %hu",
		   &phy.mode, &phy.bw, &phy.mcs, &phy.nss, &gi,
		   &phy.preamble, &phy.stbc, &phy.ldpc, &phy.spe, &ltf) != 10) {
		dev_warn(dev->mt76.dev,
			 "format: Mode BW MCS NSS GI Preamble STBC LDPC SPE ltf\n");
		return -EINVAL;
	}

	mutex_lock(&dev->mt76.mutex);

	msta_link = mt7996_sta_link_protected(dev, msta, link_sta->link_id);
	if (!msta_link) {
		ret = -EINVAL;
		goto out;
	}
	phy.wlan_idx = cpu_to_le16(msta_link->wcid.idx);
	phy.gi = cpu_to_le16(gi);
	phy.ltf = cpu_to_le16(ltf);
	phy.ldpc = phy.ldpc ? 7 : 0;
	phy.preamble = phy.preamble ? SHORT_PREAMBLE : LONG_PREAMBLE;

	ret = mt7996_mcu_set_fixed_rate_ctrl(dev, &phy, 0);
	if (ret)
		goto out;

	ret = count;
out:
	mutex_unlock(&dev->mt76.mutex);
	return ret;
}

static const struct file_operations fops_fixed_rate = {
	.write = mt7996_link_sta_fixed_rate_set,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static int
mt7996_link_sta_wtbl_show(struct seq_file *file, void *data)
{
	struct ieee80211_link_sta *link_sta = file->private;
	struct mt7996_sta *msta = NULL;
	struct mt7996_dev *dev = NULL;
	struct mt7996_sta_link *msta_link = NULL;
	int ret = 0;

	if (!link_sta || !link_sta->sta)
		return -ENOENT;

	msta = (struct mt7996_sta *)link_sta->sta->drv_priv;

	if (!msta || !msta->vif || !mt7996_vif_link_phy(&msta->vif->deflink))
		return -ENOENT;

	dev = mt7996_vif_link_phy(&msta->vif->deflink)->dev;

	if (!dev)
		return -ENOENT;

	mutex_lock(&dev->mt76.mutex);

	msta_link = mt76_dereference(msta->link[link_sta->link_id], &dev->mt76);

	/* WLAN 0 is reserved for control frames, and isn't attached to a link.
	 * 0 here likely means uninitialized.
	 */
	if (!msta_link || msta_link->wcid.idx == 0) {
		ret = -ENOENT;
		goto out;
	}

	mt7996_wtbl_dump(file, dev, msta_link->wcid.idx);

out:
	mutex_unlock(&dev->mt76.mutex);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(mt7996_link_sta_wtbl);

void mt7996_link_sta_add_debugfs(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
				 struct ieee80211_link_sta *link_sta,
				 struct dentry *dir)
{
	debugfs_create_file("fixed_rate", 0600, dir, link_sta, &fops_fixed_rate);
	debugfs_create_file("wtbl_info", 0600, dir, link_sta, &mt7996_link_sta_wtbl_fops);
}

#endif
