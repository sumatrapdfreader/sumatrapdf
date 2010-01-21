/*
 * :r !grep -h '^pdf_cmap' build/macosx-x86-debug/cmap_*.c
 * :.,'as/\(pdf_cmap.*\) =/extern \1;/
 * :.,'as/pdf_cmap \(pdf_cmap.*\) =/\&\1;/
 */

#include "fitz.h"
#include "mupdf.h"

extern pdf_cmap pdf_cmap_Adobe_CNS1_0;

extern pdf_cmap pdf_cmap_Adobe_CNS1_0;
extern pdf_cmap pdf_cmap_Adobe_CNS1_1;
extern pdf_cmap pdf_cmap_Adobe_CNS1_2;
extern pdf_cmap pdf_cmap_Adobe_CNS1_3;
extern pdf_cmap pdf_cmap_Adobe_CNS1_4;
extern pdf_cmap pdf_cmap_Adobe_CNS1_5;
extern pdf_cmap pdf_cmap_Adobe_CNS1_6;
extern pdf_cmap pdf_cmap_B5_H;
extern pdf_cmap pdf_cmap_B5_V;
extern pdf_cmap pdf_cmap_B5pc_H;
extern pdf_cmap pdf_cmap_B5pc_V;
extern pdf_cmap pdf_cmap_CNS_EUC_H;
extern pdf_cmap pdf_cmap_CNS_EUC_V;
extern pdf_cmap pdf_cmap_CNS1_H;
extern pdf_cmap pdf_cmap_CNS1_V;
extern pdf_cmap pdf_cmap_CNS2_H;
extern pdf_cmap pdf_cmap_CNS2_V;
extern pdf_cmap pdf_cmap_ETen_B5_H;
extern pdf_cmap pdf_cmap_ETen_B5_V;
extern pdf_cmap pdf_cmap_ETenms_B5_H;
extern pdf_cmap pdf_cmap_ETenms_B5_V;
extern pdf_cmap pdf_cmap_ETHK_B5_H;
extern pdf_cmap pdf_cmap_ETHK_B5_V;
extern pdf_cmap pdf_cmap_HKdla_B5_H;
extern pdf_cmap pdf_cmap_HKdla_B5_V;
extern pdf_cmap pdf_cmap_HKdlb_B5_H;
extern pdf_cmap pdf_cmap_HKdlb_B5_V;
extern pdf_cmap pdf_cmap_HKgccs_B5_H;
extern pdf_cmap pdf_cmap_HKgccs_B5_V;
extern pdf_cmap pdf_cmap_HKm314_B5_H;
extern pdf_cmap pdf_cmap_HKm314_B5_V;
extern pdf_cmap pdf_cmap_HKm471_B5_H;
extern pdf_cmap pdf_cmap_HKm471_B5_V;
extern pdf_cmap pdf_cmap_HKscs_B5_H;
extern pdf_cmap pdf_cmap_HKscs_B5_V;
extern pdf_cmap pdf_cmap_UniCNS_UCS2_H;
extern pdf_cmap pdf_cmap_UniCNS_UCS2_V;
extern pdf_cmap pdf_cmap_UniCNS_UTF16_H;
extern pdf_cmap pdf_cmap_UniCNS_UTF16_V;
extern pdf_cmap pdf_cmap_Adobe_GB1_0;
extern pdf_cmap pdf_cmap_Adobe_GB1_1;
extern pdf_cmap pdf_cmap_Adobe_GB1_2;
extern pdf_cmap pdf_cmap_Adobe_GB1_3;
extern pdf_cmap pdf_cmap_Adobe_GB1_4;
extern pdf_cmap pdf_cmap_Adobe_GB1_5;
extern pdf_cmap pdf_cmap_GB_EUC_H;
extern pdf_cmap pdf_cmap_GB_EUC_V;
extern pdf_cmap pdf_cmap_GB_H;
extern pdf_cmap pdf_cmap_GB_V;
extern pdf_cmap pdf_cmap_GBK_EUC_H;
extern pdf_cmap pdf_cmap_GBK_EUC_V;
extern pdf_cmap pdf_cmap_GBK2K_H;
extern pdf_cmap pdf_cmap_GBK2K_V;
extern pdf_cmap pdf_cmap_GBKp_EUC_H;
extern pdf_cmap pdf_cmap_GBKp_EUC_V;
extern pdf_cmap pdf_cmap_GBpc_EUC_H;
extern pdf_cmap pdf_cmap_GBpc_EUC_V;
extern pdf_cmap pdf_cmap_GBT_EUC_H;
extern pdf_cmap pdf_cmap_GBT_EUC_V;
extern pdf_cmap pdf_cmap_GBT_H;
extern pdf_cmap pdf_cmap_GBT_V;
extern pdf_cmap pdf_cmap_GBTpc_EUC_H;
extern pdf_cmap pdf_cmap_GBTpc_EUC_V;
extern pdf_cmap pdf_cmap_UniGB_UCS2_H;
extern pdf_cmap pdf_cmap_UniGB_UCS2_V;
extern pdf_cmap pdf_cmap_UniGB_UTF16_H;
extern pdf_cmap pdf_cmap_UniGB_UTF16_V;
extern pdf_cmap pdf_cmap_78_EUC_H;
extern pdf_cmap pdf_cmap_78_EUC_V;
extern pdf_cmap pdf_cmap_78_H;
extern pdf_cmap pdf_cmap_78_RKSJ_H;
extern pdf_cmap pdf_cmap_78_RKSJ_V;
extern pdf_cmap pdf_cmap_78_V;
extern pdf_cmap pdf_cmap_78ms_RKSJ_H;
extern pdf_cmap pdf_cmap_78ms_RKSJ_V;
extern pdf_cmap pdf_cmap_83pv_RKSJ_H;
extern pdf_cmap pdf_cmap_90ms_RKSJ_H;
extern pdf_cmap pdf_cmap_90ms_RKSJ_V;
extern pdf_cmap pdf_cmap_90msp_RKSJ_H;
extern pdf_cmap pdf_cmap_90msp_RKSJ_V;
extern pdf_cmap pdf_cmap_90pv_RKSJ_H;
extern pdf_cmap pdf_cmap_90pv_RKSJ_V;
extern pdf_cmap pdf_cmap_Add_H;
extern pdf_cmap pdf_cmap_Add_RKSJ_H;
extern pdf_cmap pdf_cmap_Add_RKSJ_V;
extern pdf_cmap pdf_cmap_Add_V;
extern pdf_cmap pdf_cmap_Adobe_Japan1_0;
extern pdf_cmap pdf_cmap_Adobe_Japan1_1;
extern pdf_cmap pdf_cmap_Adobe_Japan1_2;
extern pdf_cmap pdf_cmap_Adobe_Japan1_3;
extern pdf_cmap pdf_cmap_Adobe_Japan1_4;
extern pdf_cmap pdf_cmap_Adobe_Japan1_5;
extern pdf_cmap pdf_cmap_Adobe_Japan1_6;
extern pdf_cmap pdf_cmap_EUC_H;
extern pdf_cmap pdf_cmap_EUC_V;
extern pdf_cmap pdf_cmap_Ext_H;
extern pdf_cmap pdf_cmap_Ext_RKSJ_H;
extern pdf_cmap pdf_cmap_Ext_RKSJ_V;
extern pdf_cmap pdf_cmap_Ext_V;
extern pdf_cmap pdf_cmap_H;
extern pdf_cmap pdf_cmap_Hankaku;
extern pdf_cmap pdf_cmap_Hiragana;
extern pdf_cmap pdf_cmap_Katakana;
extern pdf_cmap pdf_cmap_NWP_H;
extern pdf_cmap pdf_cmap_NWP_V;
extern pdf_cmap pdf_cmap_RKSJ_H;
extern pdf_cmap pdf_cmap_RKSJ_V;
extern pdf_cmap pdf_cmap_Roman;
extern pdf_cmap pdf_cmap_UniJIS_UCS2_H;
extern pdf_cmap pdf_cmap_UniJIS_UCS2_HW_H;
extern pdf_cmap pdf_cmap_UniJIS_UCS2_HW_V;
extern pdf_cmap pdf_cmap_UniJIS_UCS2_V;
extern pdf_cmap pdf_cmap_UniJISPro_UCS2_HW_V;
extern pdf_cmap pdf_cmap_UniJISPro_UCS2_V;
extern pdf_cmap pdf_cmap_V;
extern pdf_cmap pdf_cmap_WP_Symbol;
extern pdf_cmap pdf_cmap_Adobe_Japan2_0;
extern pdf_cmap pdf_cmap_Hojo_EUC_H;
extern pdf_cmap pdf_cmap_Hojo_EUC_V;
extern pdf_cmap pdf_cmap_Hojo_H;
extern pdf_cmap pdf_cmap_Hojo_V;
extern pdf_cmap pdf_cmap_UniHojo_UCS2_H;
extern pdf_cmap pdf_cmap_UniHojo_UCS2_V;
extern pdf_cmap pdf_cmap_UniHojo_UTF16_H;
extern pdf_cmap pdf_cmap_UniHojo_UTF16_V;
extern pdf_cmap pdf_cmap_UniJIS_UTF16_H;
extern pdf_cmap pdf_cmap_UniJIS_UTF16_V;
extern pdf_cmap pdf_cmap_Adobe_Korea1_0;
extern pdf_cmap pdf_cmap_Adobe_Korea1_1;
extern pdf_cmap pdf_cmap_Adobe_Korea1_2;
extern pdf_cmap pdf_cmap_KSC_EUC_H;
extern pdf_cmap pdf_cmap_KSC_EUC_V;
extern pdf_cmap pdf_cmap_KSC_H;
extern pdf_cmap pdf_cmap_KSC_Johab_H;
extern pdf_cmap pdf_cmap_KSC_Johab_V;
extern pdf_cmap pdf_cmap_KSC_V;
extern pdf_cmap pdf_cmap_KSCms_UHC_H;
extern pdf_cmap pdf_cmap_KSCms_UHC_HW_H;
extern pdf_cmap pdf_cmap_KSCms_UHC_HW_V;
extern pdf_cmap pdf_cmap_KSCms_UHC_V;
extern pdf_cmap pdf_cmap_KSCpc_EUC_H;
extern pdf_cmap pdf_cmap_KSCpc_EUC_V;
extern pdf_cmap pdf_cmap_UniKS_UCS2_H;
extern pdf_cmap pdf_cmap_UniKS_UCS2_V;
extern pdf_cmap pdf_cmap_UniKS_UTF16_H;
extern pdf_cmap pdf_cmap_UniKS_UTF16_V;
extern pdf_cmap pdf_cmap_Adobe_CNS1_UCS2;
extern pdf_cmap pdf_cmap_Adobe_GB1_UCS2;
extern pdf_cmap pdf_cmap_Adobe_Japan1_UCS2;
extern pdf_cmap pdf_cmap_Adobe_Korea1_UCS2;

pdf_cmap *pdf_cmaptable[] =
{
#ifndef NOCJK
	&pdf_cmap_Adobe_CNS1_0,
	&pdf_cmap_Adobe_CNS1_1,
	&pdf_cmap_Adobe_CNS1_2,
	&pdf_cmap_Adobe_CNS1_3,
	&pdf_cmap_Adobe_CNS1_4,
	&pdf_cmap_Adobe_CNS1_5,
	&pdf_cmap_Adobe_CNS1_6,
	&pdf_cmap_B5_H,
	&pdf_cmap_B5_V,
	&pdf_cmap_B5pc_H,
	&pdf_cmap_B5pc_V,
	&pdf_cmap_CNS_EUC_H,
	&pdf_cmap_CNS_EUC_V,
	&pdf_cmap_CNS1_H,
	&pdf_cmap_CNS1_V,
	&pdf_cmap_CNS2_H,
	&pdf_cmap_CNS2_V,
	&pdf_cmap_ETen_B5_H,
	&pdf_cmap_ETen_B5_V,
	&pdf_cmap_ETenms_B5_H,
	&pdf_cmap_ETenms_B5_V,
	&pdf_cmap_ETHK_B5_H,
	&pdf_cmap_ETHK_B5_V,
	&pdf_cmap_HKdla_B5_H,
	&pdf_cmap_HKdla_B5_V,
	&pdf_cmap_HKdlb_B5_H,
	&pdf_cmap_HKdlb_B5_V,
	&pdf_cmap_HKgccs_B5_H,
	&pdf_cmap_HKgccs_B5_V,
	&pdf_cmap_HKm314_B5_H,
	&pdf_cmap_HKm314_B5_V,
	&pdf_cmap_HKm471_B5_H,
	&pdf_cmap_HKm471_B5_V,
	&pdf_cmap_HKscs_B5_H,
	&pdf_cmap_HKscs_B5_V,
	&pdf_cmap_UniCNS_UCS2_H,
	&pdf_cmap_UniCNS_UCS2_V,
	&pdf_cmap_UniCNS_UTF16_H,
	&pdf_cmap_UniCNS_UTF16_V,
	&pdf_cmap_Adobe_GB1_0,
	&pdf_cmap_Adobe_GB1_1,
	&pdf_cmap_Adobe_GB1_2,
	&pdf_cmap_Adobe_GB1_3,
	&pdf_cmap_Adobe_GB1_4,
	&pdf_cmap_Adobe_GB1_5,
	&pdf_cmap_GB_EUC_H,
	&pdf_cmap_GB_EUC_V,
	&pdf_cmap_GB_H,
	&pdf_cmap_GB_V,
	&pdf_cmap_GBK_EUC_H,
	&pdf_cmap_GBK_EUC_V,
	&pdf_cmap_GBK2K_H,
	&pdf_cmap_GBK2K_V,
	&pdf_cmap_GBKp_EUC_H,
	&pdf_cmap_GBKp_EUC_V,
	&pdf_cmap_GBpc_EUC_H,
	&pdf_cmap_GBpc_EUC_V,
	&pdf_cmap_GBT_EUC_H,
	&pdf_cmap_GBT_EUC_V,
	&pdf_cmap_GBT_H,
	&pdf_cmap_GBT_V,
	&pdf_cmap_GBTpc_EUC_H,
	&pdf_cmap_GBTpc_EUC_V,
	&pdf_cmap_UniGB_UCS2_H,
	&pdf_cmap_UniGB_UCS2_V,
	&pdf_cmap_UniGB_UTF16_H,
	&pdf_cmap_UniGB_UTF16_V,
	&pdf_cmap_78_EUC_H,
	&pdf_cmap_78_EUC_V,
	&pdf_cmap_78_H,
	&pdf_cmap_78_RKSJ_H,
	&pdf_cmap_78_RKSJ_V,
	&pdf_cmap_78_V,
	&pdf_cmap_78ms_RKSJ_H,
	&pdf_cmap_78ms_RKSJ_V,
	&pdf_cmap_83pv_RKSJ_H,
	&pdf_cmap_90ms_RKSJ_H,
	&pdf_cmap_90ms_RKSJ_V,
	&pdf_cmap_90msp_RKSJ_H,
	&pdf_cmap_90msp_RKSJ_V,
	&pdf_cmap_90pv_RKSJ_H,
	&pdf_cmap_90pv_RKSJ_V,
	&pdf_cmap_Add_H,
	&pdf_cmap_Add_RKSJ_H,
	&pdf_cmap_Add_RKSJ_V,
	&pdf_cmap_Add_V,
	&pdf_cmap_Adobe_Japan1_0,
	&pdf_cmap_Adobe_Japan1_1,
	&pdf_cmap_Adobe_Japan1_2,
	&pdf_cmap_Adobe_Japan1_3,
	&pdf_cmap_Adobe_Japan1_4,
	&pdf_cmap_Adobe_Japan1_5,
	&pdf_cmap_Adobe_Japan1_6,
	&pdf_cmap_EUC_H,
	&pdf_cmap_EUC_V,
	&pdf_cmap_Ext_H,
	&pdf_cmap_Ext_RKSJ_H,
	&pdf_cmap_Ext_RKSJ_V,
	&pdf_cmap_Ext_V,
	&pdf_cmap_H,
	&pdf_cmap_Hankaku,
	&pdf_cmap_Hiragana,
	&pdf_cmap_Katakana,
	&pdf_cmap_NWP_H,
	&pdf_cmap_NWP_V,
	&pdf_cmap_RKSJ_H,
	&pdf_cmap_RKSJ_V,
	&pdf_cmap_Roman,
	&pdf_cmap_UniJIS_UCS2_H,
	&pdf_cmap_UniJIS_UCS2_HW_H,
	&pdf_cmap_UniJIS_UCS2_HW_V,
	&pdf_cmap_UniJIS_UCS2_V,
	&pdf_cmap_UniJISPro_UCS2_HW_V,
	&pdf_cmap_UniJISPro_UCS2_V,
	&pdf_cmap_V,
	&pdf_cmap_WP_Symbol,
	&pdf_cmap_Adobe_Japan2_0,
	&pdf_cmap_Hojo_EUC_H,
	&pdf_cmap_Hojo_EUC_V,
	&pdf_cmap_Hojo_H,
	&pdf_cmap_Hojo_V,
	&pdf_cmap_UniHojo_UCS2_H,
	&pdf_cmap_UniHojo_UCS2_V,
	&pdf_cmap_UniHojo_UTF16_H,
	&pdf_cmap_UniHojo_UTF16_V,
	&pdf_cmap_UniJIS_UTF16_H,
	&pdf_cmap_UniJIS_UTF16_V,
	&pdf_cmap_Adobe_Korea1_0,
	&pdf_cmap_Adobe_Korea1_1,
	&pdf_cmap_Adobe_Korea1_2,
	&pdf_cmap_KSC_EUC_H,
	&pdf_cmap_KSC_EUC_V,
	&pdf_cmap_KSC_H,
	&pdf_cmap_KSC_Johab_H,
	&pdf_cmap_KSC_Johab_V,
	&pdf_cmap_KSC_V,
	&pdf_cmap_KSCms_UHC_H,
	&pdf_cmap_KSCms_UHC_HW_H,
	&pdf_cmap_KSCms_UHC_HW_V,
	&pdf_cmap_KSCms_UHC_V,
	&pdf_cmap_KSCpc_EUC_H,
	&pdf_cmap_KSCpc_EUC_V,
	&pdf_cmap_UniKS_UCS2_H,
	&pdf_cmap_UniKS_UCS2_V,
	&pdf_cmap_UniKS_UTF16_H,
	&pdf_cmap_UniKS_UTF16_V,
	&pdf_cmap_Adobe_CNS1_UCS2,
	&pdf_cmap_Adobe_GB1_UCS2,
	&pdf_cmap_Adobe_Japan1_UCS2,
	&pdf_cmap_Adobe_Korea1_UCS2,
#endif
	0
};
