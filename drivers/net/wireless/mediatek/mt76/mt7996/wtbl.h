/* Files copied here so as to not over-clutter regs.h */

#ifndef __MT7996_WTBL_H
#define __MT7996_WTBL_H

#include <linux/kernel.h>

enum mt7996_wtbl_type {
	WTBL_TYPE_LMAC, 	/* WTBL in LMAC */
	WTBL_TYPE_UMAC, 	/* WTBL in UMAC */
	WTBL_TYPE_KEY,		/* Key Table */
	MAX_NUM_WTBL_TYPE
};

struct berse_wtbl_parse {
	u8 *name;
	u32 mask;
	u32 shift;
	u8 new_line;
};

enum muar_idx {
	MUAR_INDEX_OWN_MAC_ADDR_0 = 0,
	MUAR_INDEX_OWN_MAC_ADDR_1,
	MUAR_INDEX_OWN_MAC_ADDR_2,
	MUAR_INDEX_OWN_MAC_ADDR_3,
	MUAR_INDEX_OWN_MAC_ADDR_4,
	MUAR_INDEX_OWN_MAC_ADDR_BC_MC = 0xE,
	MUAR_INDEX_UNMATCHED = 0xF,
	MUAR_INDEX_OWN_MAC_ADDR_11 = 0x11,
	MUAR_INDEX_OWN_MAC_ADDR_12,
	MUAR_INDEX_OWN_MAC_ADDR_13,
	MUAR_INDEX_OWN_MAC_ADDR_14,
	MUAR_INDEX_OWN_MAC_ADDR_15,
	MUAR_INDEX_OWN_MAC_ADDR_16,
	MUAR_INDEX_OWN_MAC_ADDR_17,
	MUAR_INDEX_OWN_MAC_ADDR_18,
	MUAR_INDEX_OWN_MAC_ADDR_19,
	MUAR_INDEX_OWN_MAC_ADDR_1A,
	MUAR_INDEX_OWN_MAC_ADDR_1B,
	MUAR_INDEX_OWN_MAC_ADDR_1C,
	MUAR_INDEX_OWN_MAC_ADDR_1D,
	MUAR_INDEX_OWN_MAC_ADDR_1E,
	MUAR_INDEX_OWN_MAC_ADDR_1F,
	MUAR_INDEX_OWN_MAC_ADDR_20,
	MUAR_INDEX_OWN_MAC_ADDR_21,
	MUAR_INDEX_OWN_MAC_ADDR_22,
	MUAR_INDEX_OWN_MAC_ADDR_23,
	MUAR_INDEX_OWN_MAC_ADDR_24,
	MUAR_INDEX_OWN_MAC_ADDR_25,
	MUAR_INDEX_OWN_MAC_ADDR_26,
	MUAR_INDEX_OWN_MAC_ADDR_27,
	MUAR_INDEX_OWN_MAC_ADDR_28,
	MUAR_INDEX_OWN_MAC_ADDR_29,
	MUAR_INDEX_OWN_MAC_ADDR_2A,
	MUAR_INDEX_OWN_MAC_ADDR_2B,
	MUAR_INDEX_OWN_MAC_ADDR_2C,
	MUAR_INDEX_OWN_MAC_ADDR_2D,
	MUAR_INDEX_OWN_MAC_ADDR_2E,
	MUAR_INDEX_OWN_MAC_ADDR_2F
};

enum cipher_suit {
	IGTK_CIPHER_SUIT_NONE = 0,
	IGTK_CIPHER_SUIT_BIP,
	IGTK_CIPHER_SUIT_BIP_256
};

#define LWTBL_LEN_IN_DW			36
#define UWTBL_LEN_IN_DW			16

#define MT_DBG_WTBL_BASE		0x820D8000

#define MT_DBG_WTBLON_TOP_BASE		0x820d4000
#define MT_DBG_WTBLON_TOP_WDUCR_ADDR	(MT_DBG_WTBLON_TOP_BASE + 0x0370) // 4370
#define MT_DBG_WTBLON_TOP_WDUCR_GROUP	GENMASK(4, 0)

#define MT_DBG_UWTBL_TOP_BASE		0x820c4000
#define MT_DBG_UWTBL_TOP_WDUCR_ADDR	(MT_DBG_UWTBL_TOP_BASE + 0x0104) // 4104
#define MT_DBG_UWTBL_TOP_WDUCR_GROUP	GENMASK(5, 0)
#define MT_DBG_UWTBL_TOP_WDUCR_TARGET	BIT(31)

#define LWTBL_IDX2BASE_ID		GENMASK(14, 8)
#define LWTBL_IDX2BASE_DW		GENMASK(7, 2)
#define LWTBL_IDX2BASE(_id, _dw)	(MT_DBG_WTBL_BASE | \
					FIELD_PREP(LWTBL_IDX2BASE_ID, _id) | \
					FIELD_PREP(LWTBL_IDX2BASE_DW, _dw))

#define UWTBL_IDX2BASE_ID		GENMASK(12, 6)
#define UWTBL_IDX2BASE_DW		GENMASK(5, 2)
#define UWTBL_IDX2BASE(_id, _dw)	(MT_DBG_UWTBL_TOP_BASE | 0x2000 | \
					FIELD_PREP(UWTBL_IDX2BASE_ID, _id) | \
					FIELD_PREP(UWTBL_IDX2BASE_DW, _dw))

#define KEYTBL_IDX2BASE_KEY		GENMASK(12, 6)
#define KEYTBL_IDX2BASE_DW		GENMASK(5, 2)
#define KEYTBL_IDX2BASE(_key, _dw)	(MT_DBG_UWTBL_TOP_BASE | 0x2000 | \
					FIELD_PREP(KEYTBL_IDX2BASE_KEY, _key) | \
					FIELD_PREP(KEYTBL_IDX2BASE_DW, _dw))

// UMAC WTBL
// DW0
#define WF_UWTBL_PEER_MLD_ADDRESS_47_32__DW                         0
#define WF_UWTBL_PEER_MLD_ADDRESS_47_32__ADDR                       0
#define WF_UWTBL_PEER_MLD_ADDRESS_47_32__MASK                       0x0000ffff // 15- 0
#define WF_UWTBL_PEER_MLD_ADDRESS_47_32__SHIFT                      0
#define WF_UWTBL_OWN_MLD_ID_DW                                      0
#define WF_UWTBL_OWN_MLD_ID_ADDR                                    0
#define WF_UWTBL_OWN_MLD_ID_MASK                                    0x003f0000 // 21-16
#define WF_UWTBL_OWN_MLD_ID_SHIFT                                   16
// DW1
#define WF_UWTBL_PEER_MLD_ADDRESS_31_0__DW                          1
#define WF_UWTBL_PEER_MLD_ADDRESS_31_0__ADDR                        4
#define WF_UWTBL_PEER_MLD_ADDRESS_31_0__MASK                        0xffffffff // 31- 0
#define WF_UWTBL_PEER_MLD_ADDRESS_31_0__SHIFT                       0
// DW2
#define WF_UWTBL_PN_31_0__DW                                        2
#define WF_UWTBL_PN_31_0__ADDR                                      8
#define WF_UWTBL_PN_31_0__MASK                                      0xffffffff // 31- 0
#define WF_UWTBL_PN_31_0__SHIFT                                     0
// DW3
#define WF_UWTBL_PN_47_32__DW                                       3
#define WF_UWTBL_PN_47_32__ADDR                                     12
#define WF_UWTBL_PN_47_32__MASK                                     0x0000ffff // 15- 0
#define WF_UWTBL_PN_47_32__SHIFT                                    0
#define WF_UWTBL_COM_SN_DW                                          3
#define WF_UWTBL_COM_SN_ADDR                                        12
#define WF_UWTBL_COM_SN_MASK                                        0x0fff0000 // 27-16
#define WF_UWTBL_COM_SN_SHIFT                                       16
// DW4
#define WF_UWTBL_TID0_SN_DW                                         4
#define WF_UWTBL_TID0_SN_ADDR                                       16
#define WF_UWTBL_TID0_SN_MASK                                       0x00000fff // 11- 0
#define WF_UWTBL_TID0_SN_SHIFT                                      0
#define WF_UWTBL_RX_BIPN_31_0__DW                                   4
#define WF_UWTBL_RX_BIPN_31_0__ADDR                                 16
#define WF_UWTBL_RX_BIPN_31_0__MASK                                 0xffffffff // 31- 0
#define WF_UWTBL_RX_BIPN_31_0__SHIFT                                0
#define WF_UWTBL_TID1_SN_DW                                         4
#define WF_UWTBL_TID1_SN_ADDR                                       16
#define WF_UWTBL_TID1_SN_MASK                                       0x00fff000 // 23-12
#define WF_UWTBL_TID1_SN_SHIFT                                      12
#define WF_UWTBL_TID2_SN_7_0__DW                                    4
#define WF_UWTBL_TID2_SN_7_0__ADDR                                  16
#define WF_UWTBL_TID2_SN_7_0__MASK                                  0xff000000 // 31-24
#define WF_UWTBL_TID2_SN_7_0__SHIFT                                 24
// DW5
#define WF_UWTBL_TID2_SN_11_8__DW                                   5
#define WF_UWTBL_TID2_SN_11_8__ADDR                                 20
#define WF_UWTBL_TID2_SN_11_8__MASK                                 0x0000000f //  3- 0
#define WF_UWTBL_TID2_SN_11_8__SHIFT                                0
#define WF_UWTBL_RX_BIPN_47_32__DW                                  5
#define WF_UWTBL_RX_BIPN_47_32__ADDR                                20
#define WF_UWTBL_RX_BIPN_47_32__MASK                                0x0000ffff // 15- 0
#define WF_UWTBL_RX_BIPN_47_32__SHIFT                               0
#define WF_UWTBL_TID3_SN_DW                                         5
#define WF_UWTBL_TID3_SN_ADDR                                       20
#define WF_UWTBL_TID3_SN_MASK                                       0x0000fff0 // 15- 4
#define WF_UWTBL_TID3_SN_SHIFT                                      4
#define WF_UWTBL_TID4_SN_DW                                         5
#define WF_UWTBL_TID4_SN_ADDR                                       20
#define WF_UWTBL_TID4_SN_MASK                                       0x0fff0000 // 27-16
#define WF_UWTBL_TID4_SN_SHIFT                                      16
#define WF_UWTBL_TID5_SN_3_0__DW                                    5
#define WF_UWTBL_TID5_SN_3_0__ADDR                                  20
#define WF_UWTBL_TID5_SN_3_0__MASK                                  0xf0000000 // 31-28
#define WF_UWTBL_TID5_SN_3_0__SHIFT                                 28
// DW6
#define WF_UWTBL_TID5_SN_11_4__DW                                   6
#define WF_UWTBL_TID5_SN_11_4__ADDR                                 24
#define WF_UWTBL_TID5_SN_11_4__MASK                                 0x000000ff //  7- 0
#define WF_UWTBL_TID5_SN_11_4__SHIFT                                0
#define WF_UWTBL_KEY_LOC2_DW                                        6
#define WF_UWTBL_KEY_LOC2_ADDR                                      24
#define WF_UWTBL_KEY_LOC2_MASK                                      0x00001fff // 12- 0
#define WF_UWTBL_KEY_LOC2_SHIFT                                     0
#define WF_UWTBL_TID6_SN_DW                                         6
#define WF_UWTBL_TID6_SN_ADDR                                       24
#define WF_UWTBL_TID6_SN_MASK                                       0x000fff00 // 19- 8
#define WF_UWTBL_TID6_SN_SHIFT                                      8
#define WF_UWTBL_TID7_SN_DW                                         6
#define WF_UWTBL_TID7_SN_ADDR                                       24
#define WF_UWTBL_TID7_SN_MASK                                       0xfff00000 // 31-20
#define WF_UWTBL_TID7_SN_SHIFT                                      20
// DW7
#define WF_UWTBL_KEY_LOC0_DW                                        7
#define WF_UWTBL_KEY_LOC0_ADDR                                      28
#define WF_UWTBL_KEY_LOC0_MASK                                      0x00001fff // 12- 0
#define WF_UWTBL_KEY_LOC0_SHIFT                                     0
#define WF_UWTBL_KEY_LOC1_DW                                        7
#define WF_UWTBL_KEY_LOC1_ADDR                                      28
#define WF_UWTBL_KEY_LOC1_MASK                                      0x1fff0000 // 28-16
#define WF_UWTBL_KEY_LOC1_SHIFT                                     16
// DW8
#define WF_UWTBL_AMSDU_CFG_DW                                       8
#define WF_UWTBL_AMSDU_CFG_ADDR                                     32
#define WF_UWTBL_AMSDU_CFG_MASK                                     0x00000fff // 11- 0
#define WF_UWTBL_AMSDU_CFG_SHIFT                                    0
#define WF_UWTBL_SEC_ADDR_MODE_DW                                   8
#define WF_UWTBL_SEC_ADDR_MODE_ADDR                                 32
#define WF_UWTBL_SEC_ADDR_MODE_MASK                                 0x00300000 // 21-20
#define WF_UWTBL_SEC_ADDR_MODE_SHIFT                                20
#define WF_UWTBL_WMM_Q_DW                                           8
#define WF_UWTBL_WMM_Q_ADDR                                         32
#define WF_UWTBL_WMM_Q_MASK                                         0x06000000 // 26-25
#define WF_UWTBL_WMM_Q_SHIFT                                        25
#define WF_UWTBL_QOS_DW                                             8
#define WF_UWTBL_QOS_ADDR                                           32
#define WF_UWTBL_QOS_MASK                                           0x08000000 // 27-27
#define WF_UWTBL_QOS_SHIFT                                          27
#define WF_UWTBL_HT_DW                                              8
#define WF_UWTBL_HT_ADDR                                            32
#define WF_UWTBL_HT_MASK                                            0x10000000 // 28-28
#define WF_UWTBL_HT_SHIFT                                           28
#define WF_UWTBL_HDRT_MODE_DW                                       8
#define WF_UWTBL_HDRT_MODE_ADDR                                     32
#define WF_UWTBL_HDRT_MODE_MASK                                     0x20000000 // 29-29
#define WF_UWTBL_HDRT_MODE_SHIFT                                    29
// DW9
#define WF_UWTBL_RELATED_IDX0_DW                                    9
#define WF_UWTBL_RELATED_IDX0_ADDR                                  36
#define WF_UWTBL_RELATED_IDX0_MASK                                  0x00000fff // 11- 0
#define WF_UWTBL_RELATED_IDX0_SHIFT                                 0
#define WF_UWTBL_RELATED_BAND0_DW                                   9
#define WF_UWTBL_RELATED_BAND0_ADDR                                 36
#define WF_UWTBL_RELATED_BAND0_MASK                                 0x00003000 // 13-12
#define WF_UWTBL_RELATED_BAND0_SHIFT                                12
#define WF_UWTBL_PRIMARY_MLD_BAND_DW                                9
#define WF_UWTBL_PRIMARY_MLD_BAND_ADDR                              36
#define WF_UWTBL_PRIMARY_MLD_BAND_MASK                              0x0000c000 // 15-14
#define WF_UWTBL_PRIMARY_MLD_BAND_SHIFT                             14
#define WF_UWTBL_RELATED_IDX1_DW                                    9
#define WF_UWTBL_RELATED_IDX1_ADDR                                  36
#define WF_UWTBL_RELATED_IDX1_MASK                                  0x0fff0000 // 27-16
#define WF_UWTBL_RELATED_IDX1_SHIFT                                 16
#define WF_UWTBL_RELATED_BAND1_DW                                   9
#define WF_UWTBL_RELATED_BAND1_ADDR                                 36
#define WF_UWTBL_RELATED_BAND1_MASK                                 0x30000000 // 29-28
#define WF_UWTBL_RELATED_BAND1_SHIFT                                28
#define WF_UWTBL_SECONDARY_MLD_BAND_DW                              9
#define WF_UWTBL_SECONDARY_MLD_BAND_ADDR                            36
#define WF_UWTBL_SECONDARY_MLD_BAND_MASK                            0xc0000000 // 31-30
#define WF_UWTBL_SECONDARY_MLD_BAND_SHIFT                           30

/* LMAC WTBL */
// DW0
#define WF_LWTBL_PEER_LINK_ADDRESS_47_32__DW                        0
#define WF_LWTBL_PEER_LINK_ADDRESS_47_32__ADDR                      0
#define WF_LWTBL_PEER_LINK_ADDRESS_47_32__MASK \
	0x0000ffff // 15- 0
#define WF_LWTBL_PEER_LINK_ADDRESS_47_32__SHIFT                     0
#define WF_LWTBL_MUAR_DW                                            0
#define WF_LWTBL_MUAR_ADDR                                          0
#define WF_LWTBL_MUAR_MASK \
	0x003f0000 // 21-16
#define WF_LWTBL_MUAR_SHIFT                                         16
#define WF_LWTBL_RCA1_DW                                            0
#define WF_LWTBL_RCA1_ADDR                                          0
#define WF_LWTBL_RCA1_MASK \
	0x00400000 // 22-22
#define WF_LWTBL_RCA1_SHIFT                                         22
#define WF_LWTBL_KID_DW                                             0
#define WF_LWTBL_KID_ADDR                                           0
#define WF_LWTBL_KID_MASK \
	0x01800000 // 24-23
#define WF_LWTBL_KID_SHIFT                                          23
#define WF_LWTBL_RCID_DW                                            0
#define WF_LWTBL_RCID_ADDR                                          0
#define WF_LWTBL_RCID_MASK \
	0x02000000 // 25-25
#define WF_LWTBL_RCID_SHIFT                                         25
#define WF_LWTBL_BAND_DW                                            0
#define WF_LWTBL_BAND_ADDR                                          0
#define WF_LWTBL_BAND_MASK \
	0x0c000000 // 27-26
#define WF_LWTBL_BAND_SHIFT                                         26
#define WF_LWTBL_RV_DW                                              0
#define WF_LWTBL_RV_ADDR                                            0
#define WF_LWTBL_RV_MASK \
	0x10000000 // 28-28
#define WF_LWTBL_RV_SHIFT                                           28
#define WF_LWTBL_RCA2_DW                                            0
#define WF_LWTBL_RCA2_ADDR                                          0
#define WF_LWTBL_RCA2_MASK \
	0x20000000 // 29-29
#define WF_LWTBL_RCA2_SHIFT                                         29
#define WF_LWTBL_WPI_FLAG_DW                                        0
#define WF_LWTBL_WPI_FLAG_ADDR                                      0
#define WF_LWTBL_WPI_FLAG_MASK \
	0x40000000 // 30-30
#define WF_LWTBL_WPI_FLAG_SHIFT                                     30
// DW1
#define WF_LWTBL_PEER_LINK_ADDRESS_31_0__DW                         1
#define WF_LWTBL_PEER_LINK_ADDRESS_31_0__ADDR                       4
#define WF_LWTBL_PEER_LINK_ADDRESS_31_0__MASK \
	0xffffffff // 31- 0
#define WF_LWTBL_PEER_LINK_ADDRESS_31_0__SHIFT                      0
// DW2
#define WF_LWTBL_AID_DW                                             2
#define WF_LWTBL_AID_ADDR                                           8
#define WF_LWTBL_AID_MASK \
	0x00000fff // 11- 0
#define WF_LWTBL_AID_SHIFT                                          0
#define WF_LWTBL_GID_SU_DW                                          2
#define WF_LWTBL_GID_SU_ADDR                                        8
#define WF_LWTBL_GID_SU_MASK \
	0x00001000 // 12-12
#define WF_LWTBL_GID_SU_SHIFT                                       12
#define WF_LWTBL_SPP_EN_DW                                          2
#define WF_LWTBL_SPP_EN_ADDR                                        8
#define WF_LWTBL_SPP_EN_MASK \
	0x00002000 // 13-13
#define WF_LWTBL_SPP_EN_SHIFT                                       13
#define WF_LWTBL_WPI_EVEN_DW                                        2
#define WF_LWTBL_WPI_EVEN_ADDR                                      8
#define WF_LWTBL_WPI_EVEN_MASK \
	0x00004000 // 14-14
#define WF_LWTBL_WPI_EVEN_SHIFT                                     14
#define WF_LWTBL_AAD_OM_DW                                          2
#define WF_LWTBL_AAD_OM_ADDR                                        8
#define WF_LWTBL_AAD_OM_MASK \
	0x00008000 // 15-15
#define WF_LWTBL_AAD_OM_SHIFT                                       15
/* kite DW2 field bit 13-14 */
#define WF_LWTBL_DUAL_PTEC_EN_DW                                    2
#define WF_LWTBL_DUAL_PTEC_EN_ADDR                                  8
#define WF_LWTBL_DUAL_PTEC_EN_MASK \
	0x00002000 // 13-13
#define WF_LWTBL_DUAL_PTEC_EN_SHIFT                                 13
#define WF_LWTBL_DUAL_CTS_CAP_DW                                    2
#define WF_LWTBL_DUAL_CTS_CAP_ADDR                                  8
#define WF_LWTBL_DUAL_CTS_CAP_MASK \
	0x00004000 // 14-14
#define WF_LWTBL_DUAL_CTS_CAP_SHIFT                                 14
#define WF_LWTBL_CIPHER_SUIT_PGTK_DW                                2
#define WF_LWTBL_CIPHER_SUIT_PGTK_ADDR                              8
#define WF_LWTBL_CIPHER_SUIT_PGTK_MASK \
	0x001f0000 // 20-16
#define WF_LWTBL_CIPHER_SUIT_PGTK_SHIFT                             16
#define WF_LWTBL_FD_DW                                              2
#define WF_LWTBL_FD_ADDR                                            8
#define WF_LWTBL_FD_MASK \
	0x00200000 // 21-21
#define WF_LWTBL_FD_SHIFT                                           21
#define WF_LWTBL_TD_DW                                              2
#define WF_LWTBL_TD_ADDR                                            8
#define WF_LWTBL_TD_MASK \
	0x00400000 // 22-22
#define WF_LWTBL_TD_SHIFT                                           22
#define WF_LWTBL_SW_DW                                              2
#define WF_LWTBL_SW_ADDR                                            8
#define WF_LWTBL_SW_MASK \
	0x00800000 // 23-23
#define WF_LWTBL_SW_SHIFT                                           23
#define WF_LWTBL_UL_DW                                              2
#define WF_LWTBL_UL_ADDR                                            8
#define WF_LWTBL_UL_MASK \
	0x01000000 // 24-24
#define WF_LWTBL_UL_SHIFT                                           24
#define WF_LWTBL_TX_PS_DW                                           2
#define WF_LWTBL_TX_PS_ADDR                                         8
#define WF_LWTBL_TX_PS_MASK \
	0x02000000 // 25-25
#define WF_LWTBL_TX_PS_SHIFT                                        25
#define WF_LWTBL_QOS_DW                                             2
#define WF_LWTBL_QOS_ADDR                                           8
#define WF_LWTBL_QOS_MASK \
	0x04000000 // 26-26
#define WF_LWTBL_QOS_SHIFT                                          26
#define WF_LWTBL_HT_DW                                              2
#define WF_LWTBL_HT_ADDR                                            8
#define WF_LWTBL_HT_MASK \
	0x08000000 // 27-27
#define WF_LWTBL_HT_SHIFT                                           27
#define WF_LWTBL_VHT_DW                                             2
#define WF_LWTBL_VHT_ADDR                                           8
#define WF_LWTBL_VHT_MASK \
	0x10000000 // 28-28
#define WF_LWTBL_VHT_SHIFT                                          28
#define WF_LWTBL_HE_DW                                              2
#define WF_LWTBL_HE_ADDR                                            8
#define WF_LWTBL_HE_MASK \
	0x20000000 // 29-29
#define WF_LWTBL_HE_SHIFT                                           29
#define WF_LWTBL_EHT_DW                                             2
#define WF_LWTBL_EHT_ADDR                                           8
#define WF_LWTBL_EHT_MASK \
	0x40000000 // 30-30
#define WF_LWTBL_EHT_SHIFT                                          30
#define WF_LWTBL_MESH_DW                                            2
#define WF_LWTBL_MESH_ADDR                                          8
#define WF_LWTBL_MESH_MASK \
	0x80000000 // 31-31
#define WF_LWTBL_MESH_SHIFT                                         31
// DW3
#define WF_LWTBL_WMM_Q_DW                                           3
#define WF_LWTBL_WMM_Q_ADDR                                         12
#define WF_LWTBL_WMM_Q_MASK \
	0x00000003 // 1- 0
#define WF_LWTBL_WMM_Q_SHIFT                                        0
#define WF_LWTBL_EHT_SIG_MCS_DW                                     3
#define WF_LWTBL_EHT_SIG_MCS_ADDR                                   12
#define WF_LWTBL_EHT_SIG_MCS_MASK \
	0x0000000c // 3- 2
#define WF_LWTBL_EHT_SIG_MCS_SHIFT                                  2
#define WF_LWTBL_HDRT_MODE_DW                                       3
#define WF_LWTBL_HDRT_MODE_ADDR                                     12
#define WF_LWTBL_HDRT_MODE_MASK \
	0x00000010 // 4- 4
#define WF_LWTBL_HDRT_MODE_SHIFT                                    4
#define WF_LWTBL_BEAM_CHG_DW                                        3
#define WF_LWTBL_BEAM_CHG_ADDR                                      12
#define WF_LWTBL_BEAM_CHG_MASK \
	0x00000020 // 5- 5
#define WF_LWTBL_BEAM_CHG_SHIFT                                     5
#define WF_LWTBL_EHT_LTF_SYM_NUM_OPT_DW                             3
#define WF_LWTBL_EHT_LTF_SYM_NUM_OPT_ADDR                           12
#define WF_LWTBL_EHT_LTF_SYM_NUM_OPT_MASK \
	0x000000c0 // 7- 6
#define WF_LWTBL_EHT_LTF_SYM_NUM_OPT_SHIFT                          6
#define WF_LWTBL_PFMU_IDX_DW                                        3
#define WF_LWTBL_PFMU_IDX_ADDR                                      12
#define WF_LWTBL_PFMU_IDX_MASK \
	0x0000ff00 // 15- 8
#define WF_LWTBL_PFMU_IDX_SHIFT                                     8
#define WF_LWTBL_ULPF_IDX_DW                                        3
#define WF_LWTBL_ULPF_IDX_ADDR                                      12
#define WF_LWTBL_ULPF_IDX_MASK \
	0x00ff0000 // 23-16
#define WF_LWTBL_ULPF_IDX_SHIFT                                     16
#define WF_LWTBL_RIBF_DW                                            3
#define WF_LWTBL_RIBF_ADDR                                          12
#define WF_LWTBL_RIBF_MASK \
	0x01000000 // 24-24
#define WF_LWTBL_RIBF_SHIFT                                         24
#define WF_LWTBL_ULPF_DW                                            3
#define WF_LWTBL_ULPF_ADDR                                          12
#define WF_LWTBL_ULPF_MASK \
	0x02000000 // 25-25
#define WF_LWTBL_ULPF_SHIFT                                         25
#define WF_LWTBL_BYPASS_TXSMM_DW                                    3
#define WF_LWTBL_BYPASS_TXSMM_ADDR                                  12
#define WF_LWTBL_BYPASS_TXSMM_MASK \
	0x04000000 // 26-26
#define WF_LWTBL_BYPASS_TXSMM_SHIFT                                 26
#define WF_LWTBL_TBF_HT_DW                                          3
#define WF_LWTBL_TBF_HT_ADDR                                        12
#define WF_LWTBL_TBF_HT_MASK \
	0x08000000 // 27-27
#define WF_LWTBL_TBF_HT_SHIFT                                       27
#define WF_LWTBL_TBF_VHT_DW                                         3
#define WF_LWTBL_TBF_VHT_ADDR                                       12
#define WF_LWTBL_TBF_VHT_MASK \
	0x10000000 // 28-28
#define WF_LWTBL_TBF_VHT_SHIFT                                      28
#define WF_LWTBL_TBF_HE_DW                                          3
#define WF_LWTBL_TBF_HE_ADDR                                        12
#define WF_LWTBL_TBF_HE_MASK \
	0x20000000 // 29-29
#define WF_LWTBL_TBF_HE_SHIFT                                       29
#define WF_LWTBL_TBF_EHT_DW                                         3
#define WF_LWTBL_TBF_EHT_ADDR                                       12
#define WF_LWTBL_TBF_EHT_MASK \
	0x40000000 // 30-30
#define WF_LWTBL_TBF_EHT_SHIFT                                      30
#define WF_LWTBL_IGN_FBK_DW                                         3
#define WF_LWTBL_IGN_FBK_ADDR                                       12
#define WF_LWTBL_IGN_FBK_MASK \
	0x80000000 // 31-31
#define WF_LWTBL_IGN_FBK_SHIFT                                      31
// DW4
#define WF_LWTBL_NEGOTIATED_WINSIZE0_DW                             4
#define WF_LWTBL_NEGOTIATED_WINSIZE0_ADDR                           16
#define WF_LWTBL_NEGOTIATED_WINSIZE0_MASK \
	0x00000007 // 2- 0
#define WF_LWTBL_NEGOTIATED_WINSIZE0_SHIFT                          0
#define WF_LWTBL_NEGOTIATED_WINSIZE1_DW                             4
#define WF_LWTBL_NEGOTIATED_WINSIZE1_ADDR                           16
#define WF_LWTBL_NEGOTIATED_WINSIZE1_MASK \
	0x00000038 // 5- 3
#define WF_LWTBL_NEGOTIATED_WINSIZE1_SHIFT                          3
#define WF_LWTBL_NEGOTIATED_WINSIZE2_DW                             4
#define WF_LWTBL_NEGOTIATED_WINSIZE2_ADDR                           16
#define WF_LWTBL_NEGOTIATED_WINSIZE2_MASK \
	0x000001c0 // 8- 6
#define WF_LWTBL_NEGOTIATED_WINSIZE2_SHIFT                          6
#define WF_LWTBL_NEGOTIATED_WINSIZE3_DW                             4
#define WF_LWTBL_NEGOTIATED_WINSIZE3_ADDR                           16
#define WF_LWTBL_NEGOTIATED_WINSIZE3_MASK \
	0x00000e00 // 11- 9
#define WF_LWTBL_NEGOTIATED_WINSIZE3_SHIFT                          9
#define WF_LWTBL_NEGOTIATED_WINSIZE4_DW                             4
#define WF_LWTBL_NEGOTIATED_WINSIZE4_ADDR                           16
#define WF_LWTBL_NEGOTIATED_WINSIZE4_MASK \
	0x00007000 // 14-12
#define WF_LWTBL_NEGOTIATED_WINSIZE4_SHIFT                          12
#define WF_LWTBL_NEGOTIATED_WINSIZE5_DW                             4
#define WF_LWTBL_NEGOTIATED_WINSIZE5_ADDR                           16
#define WF_LWTBL_NEGOTIATED_WINSIZE5_MASK \
	0x00038000 // 17-15
#define WF_LWTBL_NEGOTIATED_WINSIZE5_SHIFT                          15
#define WF_LWTBL_NEGOTIATED_WINSIZE6_DW                             4
#define WF_LWTBL_NEGOTIATED_WINSIZE6_ADDR                           16
#define WF_LWTBL_NEGOTIATED_WINSIZE6_MASK \
	0x001c0000 // 20-18
#define WF_LWTBL_NEGOTIATED_WINSIZE6_SHIFT                          18
#define WF_LWTBL_NEGOTIATED_WINSIZE7_DW                             4
#define WF_LWTBL_NEGOTIATED_WINSIZE7_ADDR                           16
#define WF_LWTBL_NEGOTIATED_WINSIZE7_MASK \
	0x00e00000 // 23-21
#define WF_LWTBL_NEGOTIATED_WINSIZE7_SHIFT                          21
#define WF_LWTBL_PE_DW                                              4
#define WF_LWTBL_PE_ADDR                                            16
#define WF_LWTBL_PE_MASK \
	0x03000000 // 25-24
#define WF_LWTBL_PE_SHIFT                                           24
#define WF_LWTBL_DIS_RHTR_DW                                        4
#define WF_LWTBL_DIS_RHTR_ADDR                                      16
#define WF_LWTBL_DIS_RHTR_MASK \
	0x04000000 // 26-26
#define WF_LWTBL_DIS_RHTR_SHIFT                                     26
#define WF_LWTBL_LDPC_HT_DW                                         4
#define WF_LWTBL_LDPC_HT_ADDR                                       16
#define WF_LWTBL_LDPC_HT_MASK \
	0x08000000 // 27-27
#define WF_LWTBL_LDPC_HT_SHIFT                                      27
#define WF_LWTBL_LDPC_VHT_DW                                        4
#define WF_LWTBL_LDPC_VHT_ADDR                                      16
#define WF_LWTBL_LDPC_VHT_MASK \
	0x10000000 // 28-28
#define WF_LWTBL_LDPC_VHT_SHIFT                                     28
#define WF_LWTBL_LDPC_HE_DW                                         4
#define WF_LWTBL_LDPC_HE_ADDR                                       16
#define WF_LWTBL_LDPC_HE_MASK \
	0x20000000 // 29-29
#define WF_LWTBL_LDPC_HE_SHIFT                                      29
#define WF_LWTBL_LDPC_EHT_DW                                        4
#define WF_LWTBL_LDPC_EHT_ADDR                                      16
#define WF_LWTBL_LDPC_EHT_MASK \
	0x40000000 // 30-30
#define WF_LWTBL_LDPC_EHT_SHIFT                                     30
#define WF_LWTBL_BA_MODE_DW                                         4
#define WF_LWTBL_BA_MODE_ADDR                                       16
#define WF_LWTBL_BA_MODE_MASK \
	0x80000000 // 31-31
#define WF_LWTBL_BA_MODE_SHIFT                                      31
// DW5
#define WF_LWTBL_AF_DW                                              5
#define WF_LWTBL_AF_ADDR                                            20
#define WF_LWTBL_AF_MASK \
	0x00000007 // 2- 0
#define WF_LWTBL_AF_MASK_7992 \
	0x0000000f // 3- 0
#define WF_LWTBL_AF_SHIFT                                           0
#define WF_LWTBL_AF_HE_DW                                           5
#define WF_LWTBL_AF_HE_ADDR                                         20
#define WF_LWTBL_AF_HE_MASK \
	0x00000018 // 4- 3
#define WF_LWTBL_AF_HE_SHIFT                                        3
#define WF_LWTBL_RTS_DW                                             5
#define WF_LWTBL_RTS_ADDR                                           20
#define WF_LWTBL_RTS_MASK \
	0x00000020 // 5- 5
#define WF_LWTBL_RTS_SHIFT                                          5
#define WF_LWTBL_SMPS_DW                                            5
#define WF_LWTBL_SMPS_ADDR                                          20
#define WF_LWTBL_SMPS_MASK \
	0x00000040 // 6- 6
#define WF_LWTBL_SMPS_SHIFT                                         6
#define WF_LWTBL_DYN_BW_DW                                          5
#define WF_LWTBL_DYN_BW_ADDR                                        20
#define WF_LWTBL_DYN_BW_MASK \
	0x00000080 // 7- 7
#define WF_LWTBL_DYN_BW_SHIFT                                       7
#define WF_LWTBL_MMSS_DW                                            5
#define WF_LWTBL_MMSS_ADDR                                          20
#define WF_LWTBL_MMSS_MASK \
	0x00000700 // 10- 8
#define WF_LWTBL_MMSS_SHIFT                                         8
#define WF_LWTBL_USR_DW                                             5
#define WF_LWTBL_USR_ADDR                                           20
#define WF_LWTBL_USR_MASK \
	0x00000800 // 11-11
#define WF_LWTBL_USR_SHIFT                                          11
#define WF_LWTBL_SR_R_DW                                            5
#define WF_LWTBL_SR_R_ADDR                                          20
#define WF_LWTBL_SR_R_MASK \
	0x00007000 // 14-12
#define WF_LWTBL_SR_R_SHIFT                                         12
#define WF_LWTBL_SR_ABORT_DW                                        5
#define WF_LWTBL_SR_ABORT_ADDR                                      20
#define WF_LWTBL_SR_ABORT_MASK \
	0x00008000 // 15-15
#define WF_LWTBL_SR_ABORT_SHIFT                                     15
#define WF_LWTBL_TX_POWER_OFFSET_DW                                 5
#define WF_LWTBL_TX_POWER_OFFSET_ADDR                               20
#define WF_LWTBL_TX_POWER_OFFSET_MASK \
	0x003f0000 // 21-16
#define WF_LWTBL_TX_POWER_OFFSET_SHIFT                              16
#define WF_LWTBL_LTF_EHT_DW                                         5
#define WF_LWTBL_LTF_EHT_ADDR                                       20
#define WF_LWTBL_LTF_EHT_MASK \
	0x00c00000 // 23-22
#define WF_LWTBL_LTF_EHT_SHIFT                                      22
#define WF_LWTBL_GI_EHT_DW                                          5
#define WF_LWTBL_GI_EHT_ADDR                                        20
#define WF_LWTBL_GI_EHT_MASK \
	0x03000000 // 25-24
#define WF_LWTBL_GI_EHT_SHIFT                                       24
#define WF_LWTBL_DOPPL_DW                                           5
#define WF_LWTBL_DOPPL_ADDR                                         20
#define WF_LWTBL_DOPPL_MASK \
	0x04000000 // 26-26
#define WF_LWTBL_DOPPL_SHIFT                                        26
#define WF_LWTBL_TXOP_PS_CAP_DW                                     5
#define WF_LWTBL_TXOP_PS_CAP_ADDR                                   20
#define WF_LWTBL_TXOP_PS_CAP_MASK \
	0x08000000 // 27-27
#define WF_LWTBL_TXOP_PS_CAP_SHIFT                                  27
#define WF_LWTBL_DU_I_PSM_DW                                        5
#define WF_LWTBL_DU_I_PSM_ADDR                                      20
#define WF_LWTBL_DU_I_PSM_MASK \
	0x10000000 // 28-28
#define WF_LWTBL_DU_I_PSM_SHIFT                                     28
#define WF_LWTBL_I_PSM_DW                                           5
#define WF_LWTBL_I_PSM_ADDR                                         20
#define WF_LWTBL_I_PSM_MASK \
	0x20000000 // 29-29
#define WF_LWTBL_I_PSM_SHIFT                                        29
#define WF_LWTBL_PSM_DW                                             5
#define WF_LWTBL_PSM_ADDR                                           20
#define WF_LWTBL_PSM_MASK \
	0x40000000 // 30-30
#define WF_LWTBL_PSM_SHIFT                                          30
#define WF_LWTBL_SKIP_TX_DW                                         5
#define WF_LWTBL_SKIP_TX_ADDR                                       20
#define WF_LWTBL_SKIP_TX_MASK \
	0x80000000 // 31-31
#define WF_LWTBL_SKIP_TX_SHIFT                                      31
// DW6
#define WF_LWTBL_CBRN_DW                                            6
#define WF_LWTBL_CBRN_ADDR                                          24
#define WF_LWTBL_CBRN_MASK \
	0x00000007 // 2- 0
#define WF_LWTBL_CBRN_SHIFT                                         0
#define WF_LWTBL_DBNSS_EN_DW                                        6
#define WF_LWTBL_DBNSS_EN_ADDR                                      24
#define WF_LWTBL_DBNSS_EN_MASK \
	0x00000008 // 3- 3
#define WF_LWTBL_DBNSS_EN_SHIFT                                     3
#define WF_LWTBL_BAF_EN_DW                                          6
#define WF_LWTBL_BAF_EN_ADDR                                        24
#define WF_LWTBL_BAF_EN_MASK \
	0x00000010 // 4- 4
#define WF_LWTBL_BAF_EN_SHIFT                                       4
#define WF_LWTBL_RDGBA_DW                                           6
#define WF_LWTBL_RDGBA_ADDR                                         24
#define WF_LWTBL_RDGBA_MASK \
	0x00000020 // 5- 5
#define WF_LWTBL_RDGBA_SHIFT                                        5
#define WF_LWTBL_R_DW                                               6
#define WF_LWTBL_R_ADDR                                             24
#define WF_LWTBL_R_MASK \
	0x00000040 // 6- 6
#define WF_LWTBL_R_SHIFT                                            6
#define WF_LWTBL_SPE_IDX_DW                                         6
#define WF_LWTBL_SPE_IDX_ADDR                                       24
#define WF_LWTBL_SPE_IDX_MASK \
	0x00000f80 // 11- 7
#define WF_LWTBL_SPE_IDX_SHIFT                                      7
#define WF_LWTBL_G2_DW                                              6
#define WF_LWTBL_G2_ADDR                                            24
#define WF_LWTBL_G2_MASK \
	0x00001000 // 12-12
#define WF_LWTBL_G2_SHIFT                                           12
#define WF_LWTBL_G4_DW                                              6
#define WF_LWTBL_G4_ADDR                                            24
#define WF_LWTBL_G4_MASK \
	0x00002000 // 13-13
#define WF_LWTBL_G4_SHIFT                                           13
#define WF_LWTBL_G8_DW                                              6
#define WF_LWTBL_G8_ADDR                                            24
#define WF_LWTBL_G8_MASK \
	0x00004000 // 14-14
#define WF_LWTBL_G8_SHIFT                                           14
#define WF_LWTBL_G16_DW                                             6
#define WF_LWTBL_G16_ADDR                                           24
#define WF_LWTBL_G16_MASK \
	0x00008000 // 15-15
#define WF_LWTBL_G16_SHIFT                                          15
#define WF_LWTBL_G2_LTF_DW                                          6
#define WF_LWTBL_G2_LTF_ADDR                                        24
#define WF_LWTBL_G2_LTF_MASK \
	0x00030000 // 17-16
#define WF_LWTBL_G2_LTF_SHIFT                                       16
#define WF_LWTBL_G4_LTF_DW                                          6
#define WF_LWTBL_G4_LTF_ADDR                                        24
#define WF_LWTBL_G4_LTF_MASK \
	0x000c0000 // 19-18
#define WF_LWTBL_G4_LTF_SHIFT                                       18
#define WF_LWTBL_G8_LTF_DW                                          6
#define WF_LWTBL_G8_LTF_ADDR                                        24
#define WF_LWTBL_G8_LTF_MASK \
	0x00300000 // 21-20
#define WF_LWTBL_G8_LTF_SHIFT                                       20
#define WF_LWTBL_G16_LTF_DW                                         6
#define WF_LWTBL_G16_LTF_ADDR                                       24
#define WF_LWTBL_G16_LTF_MASK \
	0x00c00000 // 23-22
#define WF_LWTBL_G16_LTF_SHIFT                                      22
#define WF_LWTBL_G2_HE_DW                                           6
#define WF_LWTBL_G2_HE_ADDR                                         24
#define WF_LWTBL_G2_HE_MASK \
	0x03000000 // 25-24
#define WF_LWTBL_G2_HE_SHIFT                                        24
#define WF_LWTBL_G4_HE_DW                                           6
#define WF_LWTBL_G4_HE_ADDR                                         24
#define WF_LWTBL_G4_HE_MASK \
	0x0c000000 // 27-26
#define WF_LWTBL_G4_HE_SHIFT                                        26
#define WF_LWTBL_G8_HE_DW                                           6
#define WF_LWTBL_G8_HE_ADDR                                         24
#define WF_LWTBL_G8_HE_MASK \
	0x30000000 // 29-28
#define WF_LWTBL_G8_HE_SHIFT                                        28
#define WF_LWTBL_G16_HE_DW                                          6
#define WF_LWTBL_G16_HE_ADDR                                        24
#define WF_LWTBL_G16_HE_MASK \
	0xc0000000 // 31-30
#define WF_LWTBL_G16_HE_SHIFT                                       30
// DW7
#define WF_LWTBL_BA_WIN_SIZE0_DW                                    7
#define WF_LWTBL_BA_WIN_SIZE0_ADDR                                  28
#define WF_LWTBL_BA_WIN_SIZE0_MASK \
	0x0000000f // 3- 0
#define WF_LWTBL_BA_WIN_SIZE0_SHIFT                                 0
#define WF_LWTBL_BA_WIN_SIZE1_DW                                    7
#define WF_LWTBL_BA_WIN_SIZE1_ADDR                                  28
#define WF_LWTBL_BA_WIN_SIZE1_MASK \
	0x000000f0 // 7- 4
#define WF_LWTBL_BA_WIN_SIZE1_SHIFT                                 4
#define WF_LWTBL_BA_WIN_SIZE2_DW                                    7
#define WF_LWTBL_BA_WIN_SIZE2_ADDR                                  28
#define WF_LWTBL_BA_WIN_SIZE2_MASK \
	0x00000f00 // 11- 8
#define WF_LWTBL_BA_WIN_SIZE2_SHIFT                                 8
#define WF_LWTBL_BA_WIN_SIZE3_DW                                    7
#define WF_LWTBL_BA_WIN_SIZE3_ADDR                                  28
#define WF_LWTBL_BA_WIN_SIZE3_MASK \
	0x0000f000 // 15-12
#define WF_LWTBL_BA_WIN_SIZE3_SHIFT                                 12
#define WF_LWTBL_BA_WIN_SIZE4_DW                                    7
#define WF_LWTBL_BA_WIN_SIZE4_ADDR                                  28
#define WF_LWTBL_BA_WIN_SIZE4_MASK \
	0x000f0000 // 19-16
#define WF_LWTBL_BA_WIN_SIZE4_SHIFT                                 16
#define WF_LWTBL_BA_WIN_SIZE5_DW                                    7
#define WF_LWTBL_BA_WIN_SIZE5_ADDR                                  28
#define WF_LWTBL_BA_WIN_SIZE5_MASK \
	0x00f00000 // 23-20
#define WF_LWTBL_BA_WIN_SIZE5_SHIFT                                 20
#define WF_LWTBL_BA_WIN_SIZE6_DW                                    7
#define WF_LWTBL_BA_WIN_SIZE6_ADDR                                  28
#define WF_LWTBL_BA_WIN_SIZE6_MASK \
	0x0f000000 // 27-24
#define WF_LWTBL_BA_WIN_SIZE6_SHIFT                                 24
#define WF_LWTBL_BA_WIN_SIZE7_DW                                    7
#define WF_LWTBL_BA_WIN_SIZE7_ADDR                                  28
#define WF_LWTBL_BA_WIN_SIZE7_MASK \
	0xf0000000 // 31-28
#define WF_LWTBL_BA_WIN_SIZE7_SHIFT                                 28
// DW8
#define WF_LWTBL_AC0_RTS_FAIL_CNT_DW                                8
#define WF_LWTBL_AC0_RTS_FAIL_CNT_ADDR                              32
#define WF_LWTBL_AC0_RTS_FAIL_CNT_MASK \
	0x0000001f // 4- 0
#define WF_LWTBL_AC0_RTS_FAIL_CNT_SHIFT                             0
#define WF_LWTBL_AC1_RTS_FAIL_CNT_DW                                8
#define WF_LWTBL_AC1_RTS_FAIL_CNT_ADDR                              32
#define WF_LWTBL_AC1_RTS_FAIL_CNT_MASK \
	0x000003e0 // 9- 5
#define WF_LWTBL_AC1_RTS_FAIL_CNT_SHIFT                             5
#define WF_LWTBL_AC2_RTS_FAIL_CNT_DW                                8
#define WF_LWTBL_AC2_RTS_FAIL_CNT_ADDR                              32
#define WF_LWTBL_AC2_RTS_FAIL_CNT_MASK \
	0x00007c00 // 14-10
#define WF_LWTBL_AC2_RTS_FAIL_CNT_SHIFT                             10
#define WF_LWTBL_AC3_RTS_FAIL_CNT_DW                                8
#define WF_LWTBL_AC3_RTS_FAIL_CNT_ADDR                              32
#define WF_LWTBL_AC3_RTS_FAIL_CNT_MASK \
	0x000f8000 // 19-15
#define WF_LWTBL_AC3_RTS_FAIL_CNT_SHIFT                             15
#define WF_LWTBL_PARTIAL_AID_DW                                     8
#define WF_LWTBL_PARTIAL_AID_ADDR                                   32
#define WF_LWTBL_PARTIAL_AID_MASK \
	0x1ff00000 // 28-20
#define WF_LWTBL_PARTIAL_AID_SHIFT                                  20
#define WF_LWTBL_CHK_PER_DW                                         8
#define WF_LWTBL_CHK_PER_ADDR                                       32
#define WF_LWTBL_CHK_PER_MASK \
	0x80000000 // 31-31
#define WF_LWTBL_CHK_PER_SHIFT                                      31
// DW9
#define WF_LWTBL_RX_AVG_MPDU_SIZE_DW                                9
#define WF_LWTBL_RX_AVG_MPDU_SIZE_ADDR                              36
#define WF_LWTBL_RX_AVG_MPDU_SIZE_MASK \
	0x00003fff // 13- 0
#define WF_LWTBL_RX_AVG_MPDU_SIZE_SHIFT                             0
#define WF_LWTBL_PRITX_SW_MODE_DW                                   9
#define WF_LWTBL_PRITX_SW_MODE_ADDR                                 36
#define WF_LWTBL_PRITX_SW_MODE_MASK \
	0x00008000 // 15-15
#define WF_LWTBL_PRITX_SW_MODE_SHIFT                                15
#define WF_LWTBL_PRITX_SW_MODE_MASK_7992 \
	0x00004000 // 14-14
#define WF_LWTBL_PRITX_SW_MODE_SHIFT_7992                           14
#define WF_LWTBL_PRITX_ERSU_DW                                      9
#define WF_LWTBL_PRITX_ERSU_ADDR                                    36
#define WF_LWTBL_PRITX_ERSU_MASK \
	0x00010000 // 16-16
#define WF_LWTBL_PRITX_ERSU_SHIFT                                   16
#define WF_LWTBL_PRITX_ERSU_MASK_7992 \
	0x00008000 // 15-15
#define WF_LWTBL_PRITX_ERSU_SHIFT_7992                              15
#define WF_LWTBL_PRITX_PLR_DW                                       9
#define WF_LWTBL_PRITX_PLR_ADDR                                     36
#define WF_LWTBL_PRITX_PLR_MASK \
	0x00020000 // 17-17
#define WF_LWTBL_PRITX_PLR_SHIFT                                    17
#define WF_LWTBL_PRITX_PLR_MASK_7992 \
	0x00030000 // 17-16
#define WF_LWTBL_PRITX_PLR_SHIFT_7992                               16
#define WF_LWTBL_PRITX_DCM_DW                                       9
#define WF_LWTBL_PRITX_DCM_ADDR                                     36
#define WF_LWTBL_PRITX_DCM_MASK \
	0x00040000 // 18-18
#define WF_LWTBL_PRITX_DCM_SHIFT                                    18
#define WF_LWTBL_PRITX_ER106T_DW                                    9
#define WF_LWTBL_PRITX_ER106T_ADDR                                  36
#define WF_LWTBL_PRITX_ER106T_MASK \
	0x00080000 // 19-19
#define WF_LWTBL_PRITX_ER106T_SHIFT                                 19
#define WF_LWTBL_FCAP_DW                                            9
#define WF_LWTBL_FCAP_ADDR                                          36
#define WF_LWTBL_FCAP_MASK \
	0x00700000 // 22-20
#define WF_LWTBL_FCAP_SHIFT                                         20
#define WF_LWTBL_MPDU_FAIL_CNT_DW                                   9
#define WF_LWTBL_MPDU_FAIL_CNT_ADDR                                 36
#define WF_LWTBL_MPDU_FAIL_CNT_MASK \
	0x03800000 // 25-23
#define WF_LWTBL_MPDU_FAIL_CNT_SHIFT                                23
#define WF_LWTBL_MPDU_OK_CNT_DW                                     9
#define WF_LWTBL_MPDU_OK_CNT_ADDR                                   36
#define WF_LWTBL_MPDU_OK_CNT_MASK \
	0x1c000000 // 28-26
#define WF_LWTBL_MPDU_OK_CNT_SHIFT                                  26
#define WF_LWTBL_RATE_IDX_DW                                        9
#define WF_LWTBL_RATE_IDX_ADDR                                      36
#define WF_LWTBL_RATE_IDX_MASK \
	0xe0000000 // 31-29
#define WF_LWTBL_RATE_IDX_SHIFT                                     29
// DW10
#define WF_LWTBL_RATE1_DW                                           10
#define WF_LWTBL_RATE1_ADDR                                         40
#define WF_LWTBL_RATE1_MASK \
	0x00007fff // 14- 0
#define WF_LWTBL_RATE1_SHIFT                                        0
#define WF_LWTBL_RATE2_DW                                           10
#define WF_LWTBL_RATE2_ADDR                                         40
#define WF_LWTBL_RATE2_MASK \
	0x7fff0000 // 30-16
#define WF_LWTBL_RATE2_SHIFT                                        16
// DW11
#define WF_LWTBL_RATE3_DW                                           11
#define WF_LWTBL_RATE3_ADDR                                         44
#define WF_LWTBL_RATE3_MASK \
	0x00007fff // 14- 0
#define WF_LWTBL_RATE3_SHIFT                                        0
#define WF_LWTBL_RATE4_DW                                           11
#define WF_LWTBL_RATE4_ADDR                                         44
#define WF_LWTBL_RATE4_MASK \
	0x7fff0000 // 30-16
#define WF_LWTBL_RATE4_SHIFT                                        16
// DW12
#define WF_LWTBL_RATE5_DW                                           12
#define WF_LWTBL_RATE5_ADDR                                         48
#define WF_LWTBL_RATE5_MASK \
	0x00007fff // 14- 0
#define WF_LWTBL_RATE5_SHIFT                                        0
#define WF_LWTBL_RATE6_DW                                           12
#define WF_LWTBL_RATE6_ADDR                                         48
#define WF_LWTBL_RATE6_MASK \
	0x7fff0000 // 30-16
#define WF_LWTBL_RATE6_SHIFT                                        16
// DW13
#define WF_LWTBL_RATE7_DW                                           13
#define WF_LWTBL_RATE7_ADDR                                         52
#define WF_LWTBL_RATE7_MASK \
	0x00007fff // 14- 0
#define WF_LWTBL_RATE7_SHIFT                                        0
#define WF_LWTBL_RATE8_DW                                           13
#define WF_LWTBL_RATE8_ADDR                                         52
#define WF_LWTBL_RATE8_MASK \
	0x7fff0000 // 30-16
#define WF_LWTBL_RATE8_SHIFT                                        16
// DW14
#define WF_LWTBL_RATE1_TX_CNT_DW                                    14
#define WF_LWTBL_RATE1_TX_CNT_ADDR                                  56
#define WF_LWTBL_RATE1_TX_CNT_MASK \
	0x0000ffff // 15- 0
#define WF_LWTBL_RATE1_TX_CNT_SHIFT                                 0
#define WF_LWTBL_CIPHER_SUIT_IGTK_DW                                14
#define WF_LWTBL_CIPHER_SUIT_IGTK_ADDR                              56
#define WF_LWTBL_CIPHER_SUIT_IGTK_MASK \
	0x00003000 // 13-12
#define WF_LWTBL_CIPHER_SUIT_IGTK_SHIFT                             12
#define WF_LWTBL_CIPHER_SUIT_BIGTK_DW                               14
#define WF_LWTBL_CIPHER_SUIT_BIGTK_ADDR                             56
#define WF_LWTBL_CIPHER_SUIT_BIGTK_MASK \
	0x0000c000 // 15-14
#define WF_LWTBL_CIPHER_SUIT_BIGTK_SHIFT                            14
#define WF_LWTBL_RATE1_FAIL_CNT_DW                                  14
#define WF_LWTBL_RATE1_FAIL_CNT_ADDR                                56
#define WF_LWTBL_RATE1_FAIL_CNT_MASK \
	0xffff0000 // 31-16
#define WF_LWTBL_RATE1_FAIL_CNT_SHIFT                               16
// DW15
#define WF_LWTBL_RATE2_OK_CNT_DW                                    15
#define WF_LWTBL_RATE2_OK_CNT_ADDR                                  60
#define WF_LWTBL_RATE2_OK_CNT_MASK \
	0x0000ffff // 15- 0
#define WF_LWTBL_RATE2_OK_CNT_SHIFT                                 0
#define WF_LWTBL_RATE3_OK_CNT_DW                                    15
#define WF_LWTBL_RATE3_OK_CNT_ADDR                                  60
#define WF_LWTBL_RATE3_OK_CNT_MASK \
	0xffff0000 // 31-16
#define WF_LWTBL_RATE3_OK_CNT_SHIFT                                 16
// DW16
#define WF_LWTBL_CURRENT_BW_TX_CNT_DW                               16
#define WF_LWTBL_CURRENT_BW_TX_CNT_ADDR                             64
#define WF_LWTBL_CURRENT_BW_TX_CNT_MASK \
	0x0000ffff // 15- 0
#define WF_LWTBL_CURRENT_BW_TX_CNT_SHIFT                            0
#define WF_LWTBL_CURRENT_BW_FAIL_CNT_DW                             16
#define WF_LWTBL_CURRENT_BW_FAIL_CNT_ADDR                           64
#define WF_LWTBL_CURRENT_BW_FAIL_CNT_MASK \
	0xffff0000 // 31-16
#define WF_LWTBL_CURRENT_BW_FAIL_CNT_SHIFT                          16
// DW17
#define WF_LWTBL_OTHER_BW_TX_CNT_DW                                 17
#define WF_LWTBL_OTHER_BW_TX_CNT_ADDR                               68
#define WF_LWTBL_OTHER_BW_TX_CNT_MASK \
	0x0000ffff // 15- 0
#define WF_LWTBL_OTHER_BW_TX_CNT_SHIFT                              0
#define WF_LWTBL_OTHER_BW_FAIL_CNT_DW                               17
#define WF_LWTBL_OTHER_BW_FAIL_CNT_ADDR                             68
#define WF_LWTBL_OTHER_BW_FAIL_CNT_MASK \
	0xffff0000 // 31-16
#define WF_LWTBL_OTHER_BW_FAIL_CNT_SHIFT                            16
// DW18
#define WF_LWTBL_RTS_OK_CNT_DW                                      18
#define WF_LWTBL_RTS_OK_CNT_ADDR                                    72
#define WF_LWTBL_RTS_OK_CNT_MASK \
	0x0000ffff // 15- 0
#define WF_LWTBL_RTS_OK_CNT_SHIFT                                   0
#define WF_LWTBL_RTS_FAIL_CNT_DW                                    18
#define WF_LWTBL_RTS_FAIL_CNT_ADDR                                  72
#define WF_LWTBL_RTS_FAIL_CNT_MASK \
	0xffff0000 // 31-16
#define WF_LWTBL_RTS_FAIL_CNT_SHIFT                                 16
// DW19
#define WF_LWTBL_DATA_RETRY_CNT_DW                                  19
#define WF_LWTBL_DATA_RETRY_CNT_ADDR                                76
#define WF_LWTBL_DATA_RETRY_CNT_MASK \
	0x0000ffff // 15- 0
#define WF_LWTBL_DATA_RETRY_CNT_SHIFT                               0
#define WF_LWTBL_MGNT_RETRY_CNT_DW                                  19
#define WF_LWTBL_MGNT_RETRY_CNT_ADDR                                76
#define WF_LWTBL_MGNT_RETRY_CNT_MASK \
	0xffff0000 // 31-16
#define WF_LWTBL_MGNT_RETRY_CNT_SHIFT                               16
// DW20
#define WF_LWTBL_AC0_CTT_CDT_CRB_DW                                 20
#define WF_LWTBL_AC0_CTT_CDT_CRB_ADDR                               80
#define WF_LWTBL_AC0_CTT_CDT_CRB_MASK \
	0xffffffff // 31- 0
#define WF_LWTBL_AC0_CTT_CDT_CRB_SHIFT                              0
// DW21
// DO NOT process repeat field(adm[0])
// DW22
#define WF_LWTBL_AC1_CTT_CDT_CRB_DW                                 22
#define WF_LWTBL_AC1_CTT_CDT_CRB_ADDR                               88
#define WF_LWTBL_AC1_CTT_CDT_CRB_MASK \
	0xffffffff // 31- 0
#define WF_LWTBL_AC1_CTT_CDT_CRB_SHIFT                              0
// DW23
// DO NOT process repeat field(adm[1])
// DW24
#define WF_LWTBL_AC2_CTT_CDT_CRB_DW                                 24
#define WF_LWTBL_AC2_CTT_CDT_CRB_ADDR                               96
#define WF_LWTBL_AC2_CTT_CDT_CRB_MASK \
	0xffffffff // 31- 0
#define WF_LWTBL_AC2_CTT_CDT_CRB_SHIFT                              0
// DW25
// DO NOT process repeat field(adm[2])
// DW26
#define WF_LWTBL_AC3_CTT_CDT_CRB_DW                                 26
#define WF_LWTBL_AC3_CTT_CDT_CRB_ADDR                               104
#define WF_LWTBL_AC3_CTT_CDT_CRB_MASK \
	0xffffffff // 31- 0
#define WF_LWTBL_AC3_CTT_CDT_CRB_SHIFT                              0
// DW27
// DO NOT process repeat field(adm[3])
// DW28
#define WF_LWTBL_RELATED_IDX0_DW                                    28
#define WF_LWTBL_RELATED_IDX0_ADDR                                  112
#define WF_LWTBL_RELATED_IDX0_MASK \
	0x00000fff // 11- 0
#define WF_LWTBL_RELATED_IDX0_SHIFT                                 0
#define WF_LWTBL_RELATED_BAND0_DW                                   28
#define WF_LWTBL_RELATED_BAND0_ADDR                                 112
#define WF_LWTBL_RELATED_BAND0_MASK \
	0x00003000 // 13-12
#define WF_LWTBL_RELATED_BAND0_SHIFT                                12
#define WF_LWTBL_PRIMARY_MLD_BAND_DW                                28
#define WF_LWTBL_PRIMARY_MLD_BAND_ADDR                              112
#define WF_LWTBL_PRIMARY_MLD_BAND_MASK \
	0x0000c000 // 15-14
#define WF_LWTBL_PRIMARY_MLD_BAND_SHIFT                             14
#define WF_LWTBL_RELATED_IDX1_DW                                    28
#define WF_LWTBL_RELATED_IDX1_ADDR                                  112
#define WF_LWTBL_RELATED_IDX1_MASK \
	0x0fff0000 // 27-16
#define WF_LWTBL_RELATED_IDX1_SHIFT                                 16
#define WF_LWTBL_RELATED_BAND1_DW                                   28
#define WF_LWTBL_RELATED_BAND1_ADDR                                 112
#define WF_LWTBL_RELATED_BAND1_MASK \
	0x30000000 // 29-28
#define WF_LWTBL_RELATED_BAND1_SHIFT                                28
#define WF_LWTBL_SECONDARY_MLD_BAND_DW                              28
#define WF_LWTBL_SECONDARY_MLD_BAND_ADDR                            112
#define WF_LWTBL_SECONDARY_MLD_BAND_MASK \
	0xc0000000 // 31-30
#define WF_LWTBL_SECONDARY_MLD_BAND_SHIFT                           30
// DW29
#define WF_LWTBL_DISPATCH_POLICY0_DW                                29
#define WF_LWTBL_DISPATCH_POLICY0_ADDR                              116
#define WF_LWTBL_DISPATCH_POLICY0_MASK \
	0x00000003 // 1- 0
#define WF_LWTBL_DISPATCH_POLICY0_SHIFT                             0
#define WF_LWTBL_DISPATCH_POLICY1_DW                                29
#define WF_LWTBL_DISPATCH_POLICY1_ADDR                              116
#define WF_LWTBL_DISPATCH_POLICY1_MASK \
	0x0000000c // 3- 2
#define WF_LWTBL_DISPATCH_POLICY1_SHIFT                             2
#define WF_LWTBL_DISPATCH_POLICY2_DW                                29
#define WF_LWTBL_DISPATCH_POLICY2_ADDR                              116
#define WF_LWTBL_DISPATCH_POLICY2_MASK \
	0x00000030 // 5- 4
#define WF_LWTBL_DISPATCH_POLICY2_SHIFT                             4
#define WF_LWTBL_DISPATCH_POLICY3_DW                                29
#define WF_LWTBL_DISPATCH_POLICY3_ADDR                              116
#define WF_LWTBL_DISPATCH_POLICY3_MASK \
	0x000000c0 // 7- 6
#define WF_LWTBL_DISPATCH_POLICY3_SHIFT                             6
#define WF_LWTBL_DISPATCH_POLICY4_DW                                29
#define WF_LWTBL_DISPATCH_POLICY4_ADDR                              116
#define WF_LWTBL_DISPATCH_POLICY4_MASK \
	0x00000300 // 9- 8
#define WF_LWTBL_DISPATCH_POLICY4_SHIFT                             8
#define WF_LWTBL_DISPATCH_POLICY5_DW                                29
#define WF_LWTBL_DISPATCH_POLICY5_ADDR                              116
#define WF_LWTBL_DISPATCH_POLICY5_MASK \
	0x00000c00 // 11-10
#define WF_LWTBL_DISPATCH_POLICY5_SHIFT                             10
#define WF_LWTBL_DISPATCH_POLICY6_DW                                29
#define WF_LWTBL_DISPATCH_POLICY6_ADDR                              116
#define WF_LWTBL_DISPATCH_POLICY6_MASK \
	0x00003000 // 13-12
#define WF_LWTBL_DISPATCH_POLICY6_SHIFT                             12
#define WF_LWTBL_DISPATCH_POLICY7_DW                                29
#define WF_LWTBL_DISPATCH_POLICY7_ADDR                              116
#define WF_LWTBL_DISPATCH_POLICY7_MASK \
	0x0000c000 // 15-14
#define WF_LWTBL_DISPATCH_POLICY7_SHIFT                             14
#define WF_LWTBL_OWN_MLD_ID_DW                                      29
#define WF_LWTBL_OWN_MLD_ID_ADDR                                    116
#define WF_LWTBL_OWN_MLD_ID_MASK \
	0x003f0000 // 21-16
#define WF_LWTBL_OWN_MLD_ID_SHIFT                                   16
#define WF_LWTBL_EMLSR0_DW                                          29
#define WF_LWTBL_EMLSR0_ADDR                                        116
#define WF_LWTBL_EMLSR0_MASK \
	0x00400000 // 22-22
#define WF_LWTBL_EMLSR0_SHIFT                                       22
#define WF_LWTBL_EMLMR0_DW                                          29
#define WF_LWTBL_EMLMR0_ADDR                                        116
#define WF_LWTBL_EMLMR0_MASK \
	0x00800000 // 23-23
#define WF_LWTBL_EMLMR0_SHIFT                                       23
#define WF_LWTBL_EMLSR1_DW                                          29
#define WF_LWTBL_EMLSR1_ADDR                                        116
#define WF_LWTBL_EMLSR1_MASK \
	0x01000000 // 24-24
#define WF_LWTBL_EMLSR1_SHIFT                                       24
#define WF_LWTBL_EMLMR1_DW                                          29
#define WF_LWTBL_EMLMR1_ADDR                                        116
#define WF_LWTBL_EMLMR1_MASK \
	0x02000000 // 25-25
#define WF_LWTBL_EMLMR1_SHIFT                                       25
#define WF_LWTBL_EMLSR2_DW                                          29
#define WF_LWTBL_EMLSR2_ADDR                                        116
#define WF_LWTBL_EMLSR2_MASK \
	0x04000000 // 26-26
#define WF_LWTBL_EMLSR2_SHIFT                                       26
#define WF_LWTBL_EMLMR2_DW                                          29
#define WF_LWTBL_EMLMR2_ADDR                                        116
#define WF_LWTBL_EMLMR2_MASK \
	0x08000000 // 27-27
#define WF_LWTBL_EMLMR2_SHIFT                                       27
#define WF_LWTBL_STR_BITMAP_DW                                      29
#define WF_LWTBL_STR_BITMAP_ADDR                                    116
#define WF_LWTBL_STR_BITMAP_MASK \
	0xe0000000 // 31-29
#define WF_LWTBL_STR_BITMAP_SHIFT                                   29
// DW30
#define WF_LWTBL_DISPATCH_ORDER_DW                                  30
#define WF_LWTBL_DISPATCH_ORDER_ADDR                                120
#define WF_LWTBL_DISPATCH_ORDER_MASK \
	0x0000007f // 6- 0
#define WF_LWTBL_DISPATCH_ORDER_SHIFT                               0
#define WF_LWTBL_DISPATCH_RATIO_DW                                  30
#define WF_LWTBL_DISPATCH_RATIO_ADDR                                120
#define WF_LWTBL_DISPATCH_RATIO_MASK \
	0x00003f80 // 13- 7
#define WF_LWTBL_DISPATCH_RATIO_SHIFT                               7
#define WF_LWTBL_LINK_MGF_DW                                        30
#define WF_LWTBL_LINK_MGF_ADDR                                      120
#define WF_LWTBL_LINK_MGF_MASK \
	0xffff0000 // 31-16
#define WF_LWTBL_LINK_MGF_SHIFT                                     16
// DW31
#define WF_LWTBL_BFTX_TB_DW                                         31
#define WF_LWTBL_BFTX_TB_ADDR                                       124
#define WF_LWTBL_BFTX_TB_MASK \
	0x00800000 // 23-23
#define WF_LWTBL_DROP_DW                                            31
#define WF_LWTBL_DROP_ADDR                                          124
#define WF_LWTBL_DROP_MASK \
	0x01000000 // 24-24
#define WF_LWTBL_DROP_SHIFT                                         24
#define WF_LWTBL_CASCAD_DW                                          31
#define WF_LWTBL_CASCAD_ADDR                                        124
#define WF_LWTBL_CASCAD_MASK \
	0x02000000 // 25-25
#define WF_LWTBL_CASCAD_SHIFT                                       25
#define WF_LWTBL_ALL_ACK_DW                                         31
#define WF_LWTBL_ALL_ACK_ADDR                                       124
#define WF_LWTBL_ALL_ACK_MASK \
	0x04000000 // 26-26
#define WF_LWTBL_ALL_ACK_SHIFT                                      26
#define WF_LWTBL_MPDU_SIZE_DW                                       31
#define WF_LWTBL_MPDU_SIZE_ADDR                                     124
#define WF_LWTBL_MPDU_SIZE_MASK \
	0x18000000 // 28-27
#define WF_LWTBL_MPDU_SIZE_SHIFT                                    27
#define WF_LWTBL_RXD_DUP_MODE_DW                                    31
#define WF_LWTBL_RXD_DUP_MODE_ADDR                                  124
#define WF_LWTBL_RXD_DUP_MODE_MASK \
	0x60000000 // 30-29
#define WF_LWTBL_RXD_DUP_MODE_SHIFT                                 29
#define WF_LWTBL_ACK_EN_DW                                          31
#define WF_LWTBL_ACK_EN_ADDR                                        128
#define WF_LWTBL_ACK_EN_MASK \
	0x80000000 // 31-31
#define WF_LWTBL_ACK_EN_SHIFT                                       31
// DW32
#define WF_LWTBL_OM_INFO_DW                                         32
#define WF_LWTBL_OM_INFO_ADDR                                       128
#define WF_LWTBL_OM_INFO_MASK \
	0x00000fff // 11- 0
#define WF_LWTBL_OM_INFO_SHIFT                                      0
#define WF_LWTBL_OM_INFO_EHT_DW                                     32
#define WF_LWTBL_OM_INFO_EHT_ADDR                                   128
#define WF_LWTBL_OM_INFO_EHT_MASK \
	0x0000f000 // 15-12
#define WF_LWTBL_OM_INFO_EHT_SHIFT                                  12
#define WF_LWTBL_RXD_DUP_FOR_OM_CHG_DW                              32
#define WF_LWTBL_RXD_DUP_FOR_OM_CHG_ADDR                            128
#define WF_LWTBL_RXD_DUP_FOR_OM_CHG_MASK \
	0x00010000 // 16-16
#define WF_LWTBL_RXD_DUP_FOR_OM_CHG_SHIFT                           16
#define WF_LWTBL_RXD_DUP_WHITE_LIST_DW                              32
#define WF_LWTBL_RXD_DUP_WHITE_LIST_ADDR                            128
#define WF_LWTBL_RXD_DUP_WHITE_LIST_MASK \
	0x1ffe0000 // 28-17
#define WF_LWTBL_RXD_DUP_WHITE_LIST_SHIFT                           17
// DW33
#define WF_LWTBL_USER_RSSI_DW                                       33
#define WF_LWTBL_USER_RSSI_ADDR                                     132
#define WF_LWTBL_USER_RSSI_MASK \
	0x000001ff // 8- 0
#define WF_LWTBL_USER_RSSI_SHIFT                                    0
#define WF_LWTBL_USER_SNR_DW                                        33
#define WF_LWTBL_USER_SNR_ADDR                                      132
#define WF_LWTBL_USER_SNR_MASK \
	0x00007e00 // 14- 9
#define WF_LWTBL_USER_SNR_SHIFT                                     9
#define WF_LWTBL_RAPID_REACTION_RATE_DW                             33
#define WF_LWTBL_RAPID_REACTION_RATE_ADDR                           132
#define WF_LWTBL_RAPID_REACTION_RATE_MASK \
	0x0fff0000 // 27-16
#define WF_LWTBL_RAPID_REACTION_RATE_SHIFT                          16
#define WF_LWTBL_HT_AMSDU_DW                                        33
#define WF_LWTBL_HT_AMSDU_ADDR                                      132
#define WF_LWTBL_HT_AMSDU_MASK \
	0x40000000 // 30-30
#define WF_LWTBL_HT_AMSDU_SHIFT                                     30
#define WF_LWTBL_AMSDU_CROSS_LG_DW                                  33
#define WF_LWTBL_AMSDU_CROSS_LG_ADDR                                132
#define WF_LWTBL_AMSDU_CROSS_LG_MASK \
	0x80000000 // 31-31
#define WF_LWTBL_AMSDU_CROSS_LG_SHIFT                               31
// DW34
#define WF_LWTBL_RESP_RCPI0_DW                                      34
#define WF_LWTBL_RESP_RCPI0_ADDR                                    136
#define WF_LWTBL_RESP_RCPI0_MASK \
	0x000000ff // 7- 0
#define WF_LWTBL_RESP_RCPI0_SHIFT                                   0
#define WF_LWTBL_RESP_RCPI1_DW                                      34
#define WF_LWTBL_RESP_RCPI1_ADDR                                    136
#define WF_LWTBL_RESP_RCPI1_MASK \
	0x0000ff00 // 15- 8
#define WF_LWTBL_RESP_RCPI1_SHIFT                                   8
#define WF_LWTBL_RESP_RCPI2_DW                                      34
#define WF_LWTBL_RESP_RCPI2_ADDR                                    136
#define WF_LWTBL_RESP_RCPI2_MASK \
	0x00ff0000 // 23-16
#define WF_LWTBL_RESP_RCPI2_SHIFT                                   16
#define WF_LWTBL_RESP_RCPI3_DW                                      34
#define WF_LWTBL_RESP_RCPI3_ADDR                                    136
#define WF_LWTBL_RESP_RCPI3_MASK \
	0xff000000 // 31-24
#define WF_LWTBL_RESP_RCPI3_SHIFT                                   24
// DW35
#define WF_LWTBL_SNR_RX0_DW                                         35
#define WF_LWTBL_SNR_RX0_ADDR                                       140
#define WF_LWTBL_SNR_RX0_MASK \
	0x0000003f // 5- 0
#define WF_LWTBL_SNR_RX0_SHIFT                                      0
#define WF_LWTBL_SNR_RX1_DW                                         35
#define WF_LWTBL_SNR_RX1_ADDR                                       140
#define WF_LWTBL_SNR_RX1_MASK \
	0x00000fc0 // 11- 6
#define WF_LWTBL_SNR_RX1_SHIFT                                      6
#define WF_LWTBL_SNR_RX2_DW                                         35
#define WF_LWTBL_SNR_RX2_ADDR                                       140
#define WF_LWTBL_SNR_RX2_MASK \
	0x0003f000 // 17-12
#define WF_LWTBL_SNR_RX2_SHIFT                                      12
#define WF_LWTBL_SNR_RX3_DW                                         35
#define WF_LWTBL_SNR_RX3_ADDR                                       140
#define WF_LWTBL_SNR_RX3_MASK \
	0x00fc0000 // 23-18
#define WF_LWTBL_SNR_RX3_SHIFT                                      18

/* WTBL Group - Packet Number */
/* DW 2 */
#define WTBL_PN0_MASK                   BITS(0, 7)
#define WTBL_PN0_OFFSET                 0
#define WTBL_PN1_MASK                   BITS(8, 15)
#define WTBL_PN1_OFFSET                 8
#define WTBL_PN2_MASK                   BITS(16, 23)
#define WTBL_PN2_OFFSET                 16
#define WTBL_PN3_MASK                   BITS(24, 31)
#define WTBL_PN3_OFFSET                 24

/* DW 3 */
#define WTBL_PN4_MASK                   BITS(0, 7)
#define WTBL_PN4_OFFSET                 0
#define WTBL_PN5_MASK                   BITS(8, 15)
#define WTBL_PN5_OFFSET                 8

/* DW 4 */
#define WTBL_BIPN0_MASK                 BITS(0, 7)
#define WTBL_BIPN0_OFFSET               0
#define WTBL_BIPN1_MASK                 BITS(8, 15)
#define WTBL_BIPN1_OFFSET               8
#define WTBL_BIPN2_MASK                 BITS(16, 23)
#define WTBL_BIPN2_OFFSET               16
#define WTBL_BIPN3_MASK                 BITS(24, 31)
#define WTBL_BIPN3_OFFSET               24

/* DW 5 */
#define WTBL_BIPN4_MASK                 BITS(0, 7)
#define WTBL_BIPN4_OFFSET               0
#define WTBL_BIPN5_MASK                 BITS(8, 15)
#define WTBL_BIPN5_OFFSET               8

/* UWTBL DW 6 */
#define WTBL_AMSDU_LEN_MASK             BITS(0, 5)
#define WTBL_AMSDU_LEN_OFFSET           0
#define WTBL_AMSDU_NUM_MASK             BITS(6, 10)
#define WTBL_AMSDU_NUM_OFFSET           6
#define WTBL_AMSDU_EN_MASK              BIT(11)
#define WTBL_AMSDU_EN_OFFSET            11

/* UWTBL DW 8 */
#define WTBL_SEC_ADDR_MODE_MASK		BITS(20, 21)
#define WTBL_SEC_ADDR_MODE_OFFSET	20

/* LWTBL Rate field */
#define WTBL_RATE_TX_RATE_MASK          BITS(0, 5)
#define WTBL_RATE_TX_RATE_OFFSET        0
#define WTBL_RATE_TX_MODE_MASK          BITS(6, 9)
#define WTBL_RATE_TX_MODE_OFFSET        6
#define WTBL_RATE_NSTS_MASK             BITS(10, 13)
#define WTBL_RATE_NSTS_OFFSET           10
#define WTBL_RATE_STBC_MASK             BIT(14)
#define WTBL_RATE_STBC_OFFSET           14

/***** WTBL(LMAC) DW Offset *****/
/* LMAC WTBL Group - Peer Unique Information */
#define WTBL_GROUP_PEER_INFO_DW_0               0
#define WTBL_GROUP_PEER_INFO_DW_1               1

/* WTBL Group - TxRx Capability/Information */
#define WTBL_GROUP_TRX_CAP_DW_2                 2
#define WTBL_GROUP_TRX_CAP_DW_3                 3
#define WTBL_GROUP_TRX_CAP_DW_4                 4
#define WTBL_GROUP_TRX_CAP_DW_5                 5
#define WTBL_GROUP_TRX_CAP_DW_6                 6
#define WTBL_GROUP_TRX_CAP_DW_7                 7
#define WTBL_GROUP_TRX_CAP_DW_8                 8
#define WTBL_GROUP_TRX_CAP_DW_9                 9

/* WTBL Group - Auto Rate Table*/
#define WTBL_GROUP_AUTO_RATE_1_2                10
#define WTBL_GROUP_AUTO_RATE_3_4                11
#define WTBL_GROUP_AUTO_RATE_5_6                12
#define WTBL_GROUP_AUTO_RATE_7_8                13

/* WTBL Group - Tx Counter */
#define WTBL_GROUP_TX_CNT_LINE_1                14
#define WTBL_GROUP_TX_CNT_LINE_2                15
#define WTBL_GROUP_TX_CNT_LINE_3                16
#define WTBL_GROUP_TX_CNT_LINE_4                17
#define WTBL_GROUP_TX_CNT_LINE_5                18
#define WTBL_GROUP_TX_CNT_LINE_6                19

/* WTBL Group - Admission Control Counter */
#define WTBL_GROUP_ADM_CNT_LINE_1               20
#define WTBL_GROUP_ADM_CNT_LINE_2               21
#define WTBL_GROUP_ADM_CNT_LINE_3               22
#define WTBL_GROUP_ADM_CNT_LINE_4               23
#define WTBL_GROUP_ADM_CNT_LINE_5               24
#define WTBL_GROUP_ADM_CNT_LINE_6               25
#define WTBL_GROUP_ADM_CNT_LINE_7               26
#define WTBL_GROUP_ADM_CNT_LINE_8               27

/* WTBL Group -MLO Info */
#define WTBL_GROUP_MLO_INFO_LINE_1              28
#define WTBL_GROUP_MLO_INFO_LINE_2              29
#define WTBL_GROUP_MLO_INFO_LINE_3              30

/* WTBL Group -RESP Info */
#define WTBL_GROUP_RESP_INFO_DW_31              31

/* WTBL Group -RX DUP Info */
#define WTBL_GROUP_RX_DUP_INFO_DW_32            32

/* WTBL Group - Rx Statistics Counter */
#define WTBL_GROUP_RX_STAT_CNT_LINE_1           33
#define WTBL_GROUP_RX_STAT_CNT_LINE_2           34
#define WTBL_GROUP_RX_STAT_CNT_LINE_3           35

/* UWTBL Group - HW AMSDU */
#define UWTBL_HW_AMSDU_DW                       WF_UWTBL_AMSDU_CFG_DW

/* LWTBL DW 4 */
#define WTBL_DIS_RHTR                           WF_LWTBL_DIS_RHTR_MASK

/* UWTBL DW 5 */
#define WTBL_KEY_LINK_DW_KEY_LOC0_MASK          BITS(0, 10)
#define WTBL_PSM				WF_LWTBL_PSM_MASK

/* Need to sync with FW define */
#define INVALID_KEY_ENTRY                       WTBL_KEY_LINK_DW_KEY_LOC0_MASK

// RATE
#define WTBL_RATE_TX_RATE_MASK          	BITS(0, 5)
#define WTBL_RATE_TX_RATE_OFFSET        	0
#define WTBL_RATE_TX_MODE_MASK          	BITS(6, 9)
#define WTBL_RATE_TX_MODE_OFFSET        	6
#define WTBL_RATE_NSTS_MASK             	BITS(10, 13)
#define WTBL_RATE_NSTS_OFFSET           	10
#define WTBL_RATE_STBC_MASK            	 	BIT(14)
#define WTBL_RATE_STBC_OFFSET          	 	14

#endif
