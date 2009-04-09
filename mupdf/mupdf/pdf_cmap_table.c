/*
 * :r !grep '^const.*pdf_cmap_' build/macosx-x86-debug/cmap_*.c
 */

#include "fitz.h"
#include "mupdf.h"

extern pdf_cmap pdf_cmap_Adobe_GB1_4;
extern pdf_cmap pdf_cmap_Adobe_GB1_UCS2;
extern pdf_cmap pdf_cmap_GB_EUC_H;
extern pdf_cmap pdf_cmap_GB_EUC_V;
extern pdf_cmap pdf_cmap_GBK_EUC_H;
extern pdf_cmap pdf_cmap_GBK_EUC_UCS2;
extern pdf_cmap pdf_cmap_GBK_EUC_V;
extern pdf_cmap pdf_cmap_GBK2K_H;
extern pdf_cmap pdf_cmap_GBK2K_V;
extern pdf_cmap pdf_cmap_GBKp_EUC_H;
extern pdf_cmap pdf_cmap_GBKp_EUC_V;
extern pdf_cmap pdf_cmap_GBT_EUC_H;
extern pdf_cmap pdf_cmap_GBT_EUC_V;
extern pdf_cmap pdf_cmap_GBpc_EUC_H;
extern pdf_cmap pdf_cmap_GBpc_EUC_UCS2;
extern pdf_cmap pdf_cmap_GBpc_EUC_UCS2C;
extern pdf_cmap pdf_cmap_GBpc_EUC_V;
extern pdf_cmap pdf_cmap_UniGB_UCS2_H;
extern pdf_cmap pdf_cmap_UniGB_UCS2_V;
extern pdf_cmap pdf_cmap_Adobe_CNS1_3;
extern pdf_cmap pdf_cmap_Adobe_CNS1_UCS2;
extern pdf_cmap pdf_cmap_B5pc_H;
extern pdf_cmap pdf_cmap_B5pc_UCS2;
extern pdf_cmap pdf_cmap_B5pc_UCS2C;
extern pdf_cmap pdf_cmap_B5pc_V;
extern pdf_cmap pdf_cmap_CNS_EUC_H;
extern pdf_cmap pdf_cmap_CNS_EUC_V;
extern pdf_cmap pdf_cmap_ETen_B5_H;
extern pdf_cmap pdf_cmap_ETen_B5_UCS2;
extern pdf_cmap pdf_cmap_ETen_B5_V;
extern pdf_cmap pdf_cmap_ETenms_B5_H;
extern pdf_cmap pdf_cmap_ETenms_B5_V;
extern pdf_cmap pdf_cmap_HKscs_B5_H;
extern pdf_cmap pdf_cmap_HKscs_B5_V;
extern pdf_cmap pdf_cmap_UniCNS_UCS2_H;
extern pdf_cmap pdf_cmap_UniCNS_UCS2_V;
extern pdf_cmap pdf_cmap_83pv_RKSJ_H;
extern pdf_cmap pdf_cmap_90ms_RKSJ_H;
extern pdf_cmap pdf_cmap_90ms_RKSJ_UCS2;
extern pdf_cmap pdf_cmap_90ms_RKSJ_V;
extern pdf_cmap pdf_cmap_90msp_RKSJ_H;
extern pdf_cmap pdf_cmap_90msp_RKSJ_V;
extern pdf_cmap pdf_cmap_90pv_RKSJ_H;
extern pdf_cmap pdf_cmap_90pv_RKSJ_UCS2;
extern pdf_cmap pdf_cmap_90pv_RKSJ_UCS2C;
extern pdf_cmap pdf_cmap_Add_RKSJ_H;
extern pdf_cmap pdf_cmap_Add_RKSJ_V;
extern pdf_cmap pdf_cmap_Adobe_Japan1_4;
extern pdf_cmap pdf_cmap_Adobe_Japan1_UCS2;
extern pdf_cmap pdf_cmap_EUC_H;
extern pdf_cmap pdf_cmap_EUC_V;
extern pdf_cmap pdf_cmap_Ext_RKSJ_H;
extern pdf_cmap pdf_cmap_Ext_RKSJ_V;
extern pdf_cmap pdf_cmap_H;
extern pdf_cmap pdf_cmap_UniJIS_UCS2_H;
extern pdf_cmap pdf_cmap_UniJIS_UCS2_HW_H;
extern pdf_cmap pdf_cmap_UniJIS_UCS2_HW_V;
extern pdf_cmap pdf_cmap_UniJIS_UCS2_V;
extern pdf_cmap pdf_cmap_V;
extern pdf_cmap pdf_cmap_Adobe_Korea1_2;
extern pdf_cmap pdf_cmap_Adobe_Korea1_UCS2;
extern pdf_cmap pdf_cmap_KSC_EUC_H;
extern pdf_cmap pdf_cmap_KSC_EUC_V;
extern pdf_cmap pdf_cmap_KSCms_UHC_H;
extern pdf_cmap pdf_cmap_KSCms_UHC_HW_H;
extern pdf_cmap pdf_cmap_KSCms_UHC_HW_V;
extern pdf_cmap pdf_cmap_KSCms_UHC_UCS2;
extern pdf_cmap pdf_cmap_KSCms_UHC_V;
extern pdf_cmap pdf_cmap_KSCpc_EUC_H;
extern pdf_cmap pdf_cmap_KSCpc_EUC_UCS2;
extern pdf_cmap pdf_cmap_KSCpc_EUC_UCS2C;
extern pdf_cmap pdf_cmap_UniKS_UCS2_H;
extern pdf_cmap pdf_cmap_UniKS_UCS2_V;

pdf_cmap *pdf_cmaptable[] =
{
#ifndef NOCJK
    &pdf_cmap_Adobe_GB1_4,
    &pdf_cmap_Adobe_GB1_UCS2,
    &pdf_cmap_GB_EUC_H,
    &pdf_cmap_GB_EUC_V,
    &pdf_cmap_GBK_EUC_H,
    &pdf_cmap_GBK_EUC_UCS2,
    &pdf_cmap_GBK_EUC_V,
    &pdf_cmap_GBK2K_H,
    &pdf_cmap_GBK2K_V,
    &pdf_cmap_GBKp_EUC_H,
    &pdf_cmap_GBKp_EUC_V,
    &pdf_cmap_GBT_EUC_H,
    &pdf_cmap_GBT_EUC_V,
    &pdf_cmap_GBpc_EUC_H,
    &pdf_cmap_GBpc_EUC_UCS2,
    &pdf_cmap_GBpc_EUC_UCS2C,
    &pdf_cmap_GBpc_EUC_V,
    &pdf_cmap_UniGB_UCS2_H,
    &pdf_cmap_UniGB_UCS2_V,
    &pdf_cmap_Adobe_CNS1_3,
    &pdf_cmap_Adobe_CNS1_UCS2,
    &pdf_cmap_B5pc_H,
    &pdf_cmap_B5pc_UCS2,
    &pdf_cmap_B5pc_UCS2C,
    &pdf_cmap_B5pc_V,
    &pdf_cmap_CNS_EUC_H,
    &pdf_cmap_CNS_EUC_V,
    &pdf_cmap_ETen_B5_H,
    &pdf_cmap_ETen_B5_UCS2,
    &pdf_cmap_ETen_B5_V,
    &pdf_cmap_ETenms_B5_H,
    &pdf_cmap_ETenms_B5_V,
    &pdf_cmap_HKscs_B5_H,
    &pdf_cmap_HKscs_B5_V,
    &pdf_cmap_UniCNS_UCS2_H,
    &pdf_cmap_UniCNS_UCS2_V,
    &pdf_cmap_83pv_RKSJ_H,
    &pdf_cmap_90ms_RKSJ_H,
    &pdf_cmap_90ms_RKSJ_UCS2,
    &pdf_cmap_90ms_RKSJ_V,
    &pdf_cmap_90msp_RKSJ_H,
    &pdf_cmap_90msp_RKSJ_V,
    &pdf_cmap_90pv_RKSJ_H,
    &pdf_cmap_90pv_RKSJ_UCS2,
    &pdf_cmap_90pv_RKSJ_UCS2C,
    &pdf_cmap_Add_RKSJ_H,
    &pdf_cmap_Add_RKSJ_V,
    &pdf_cmap_Adobe_Japan1_4,
    &pdf_cmap_Adobe_Japan1_UCS2,
    &pdf_cmap_EUC_H,
    &pdf_cmap_EUC_V,
    &pdf_cmap_Ext_RKSJ_H,
    &pdf_cmap_Ext_RKSJ_V,
    &pdf_cmap_H,
    &pdf_cmap_UniJIS_UCS2_H,
    &pdf_cmap_UniJIS_UCS2_HW_H,
    &pdf_cmap_UniJIS_UCS2_HW_V,
    &pdf_cmap_UniJIS_UCS2_V,
    &pdf_cmap_V,
    &pdf_cmap_Adobe_Korea1_2,
    &pdf_cmap_Adobe_Korea1_UCS2,
    &pdf_cmap_KSC_EUC_H,
    &pdf_cmap_KSC_EUC_V,
    &pdf_cmap_KSCms_UHC_H,
    &pdf_cmap_KSCms_UHC_HW_H,
    &pdf_cmap_KSCms_UHC_HW_V,
    &pdf_cmap_KSCms_UHC_UCS2,
    &pdf_cmap_KSCms_UHC_V,
    &pdf_cmap_KSCpc_EUC_H,
    &pdf_cmap_KSCpc_EUC_UCS2,
    &pdf_cmap_KSCpc_EUC_UCS2C,
    &pdf_cmap_UniKS_UCS2_H,
    &pdf_cmap_UniKS_UCS2_V,
#endif
    0
};

