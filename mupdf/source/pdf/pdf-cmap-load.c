#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <string.h>

size_t
pdf_cmap_size(fz_context *ctx, pdf_cmap *cmap)
{
	if (cmap == NULL)
		return 0;
	if (cmap->storable.refs < 0)
		return 0;

	return pdf_cmap_size(ctx, cmap->usecmap) +
		cmap->rcap * sizeof *cmap->ranges +
		cmap->xcap * sizeof *cmap->xranges +
		cmap->mcap * sizeof *cmap->mranges;
}

/*
 * Load CMap stream in PDF file
 */
pdf_cmap *
pdf_load_embedded_cmap(fz_context *ctx, pdf_document *doc, pdf_obj *stmobj)
{
	fz_stream *file = NULL;
	pdf_cmap *cmap = NULL;
	pdf_cmap *usecmap = NULL;
	pdf_obj *obj;

	fz_var(file);
	fz_var(cmap);
	fz_var(usecmap);

	if ((cmap = pdf_find_item(ctx, pdf_drop_cmap_imp, stmobj)) != NULL)
		return cmap;

	fz_try(ctx)
	{
		file = pdf_open_stream(ctx, stmobj);
		cmap = pdf_load_cmap(ctx, file);

		obj = pdf_dict_get(ctx, stmobj, PDF_NAME(WMode));
		if (pdf_is_int(ctx, obj))
			pdf_set_cmap_wmode(ctx, cmap, pdf_to_int(ctx, obj));

		obj = pdf_dict_get(ctx, stmobj, PDF_NAME(UseCMap));
		if (pdf_is_name(ctx, obj))
		{
			usecmap = pdf_load_system_cmap(ctx, pdf_to_name(ctx, obj));
			pdf_set_usecmap(ctx, cmap, usecmap);
		}
		else if (pdf_is_indirect(ctx, obj))
		{
			if (pdf_mark_obj(ctx, obj))
				fz_throw(ctx, FZ_ERROR_GENERIC, "recursive CMap");
			fz_try(ctx)
				usecmap = pdf_load_embedded_cmap(ctx, doc, obj);
			fz_always(ctx)
				pdf_unmark_obj(ctx, obj);
			fz_catch(ctx)
				fz_rethrow(ctx);
			pdf_set_usecmap(ctx, cmap, usecmap);
		}

		pdf_store_item(ctx, stmobj, cmap, pdf_cmap_size(ctx, cmap));
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, file);
		pdf_drop_cmap(ctx, usecmap);
	}
	fz_catch(ctx)
	{
		pdf_drop_cmap(ctx, cmap);
		fz_rethrow(ctx);
	}

	return cmap;
}

/*
 * Create an Identity-* CMap (for both 1 and 2-byte encodings)
 */
pdf_cmap *
pdf_new_identity_cmap(fz_context *ctx, int wmode, int bytes)
{
	pdf_cmap *cmap = pdf_new_cmap(ctx);
	fz_try(ctx)
	{
		unsigned int high = (1 << (bytes * 8)) - 1;
		if (wmode)
			fz_strlcpy(cmap->cmap_name, "Identity-V", sizeof cmap->cmap_name);
		else
			fz_strlcpy(cmap->cmap_name, "Identity-H", sizeof cmap->cmap_name);
		pdf_add_codespace(ctx, cmap, 0, high, bytes);
		pdf_map_range_to_range(ctx, cmap, 0, high, 0);
		pdf_sort_cmap(ctx, cmap);
		pdf_set_cmap_wmode(ctx, cmap, wmode);
	}
	fz_catch(ctx)
	{
		pdf_drop_cmap(ctx, cmap);
		fz_rethrow(ctx);
	}
	return cmap;
}

/*
 * Load built-in CMap resource.
 */

#ifdef NO_CJK

pdf_cmap *
pdf_load_builtin_cmap(fz_context *ctx, const char *name)
{
	if (!strcmp(name, "Identity-H")) return pdf_new_identity_cmap(ctx, 0, 2);
	if (!strcmp(name, "Identity-V")) return pdf_new_identity_cmap(ctx, 1, 2);
	return NULL;
}

#else

/* To regenerate this list: :r !bash scripts/runcmapdump.sh */

#include "cmaps/83pv-RKSJ-H.h"
#include "cmaps/90ms-RKSJ-H.h"
#include "cmaps/90ms-RKSJ-V.h"
#include "cmaps/90msp-RKSJ-H.h"
#include "cmaps/90msp-RKSJ-V.h"
#include "cmaps/90pv-RKSJ-H.h"
#include "cmaps/Add-RKSJ-H.h"
#include "cmaps/Add-RKSJ-V.h"
#include "cmaps/Adobe-CNS1-UCS2.h"
#include "cmaps/Adobe-GB1-UCS2.h"
#include "cmaps/Adobe-Japan1-UCS2.h"
#include "cmaps/Adobe-Korea1-UCS2.h"
#include "cmaps/B5pc-H.h"
#include "cmaps/B5pc-V.h"
#include "cmaps/CNS-EUC-H.h"
#include "cmaps/CNS-EUC-V.h"
#include "cmaps/ETen-B5-H.h"
#include "cmaps/ETen-B5-V.h"
#include "cmaps/ETenms-B5-H.h"
#include "cmaps/ETenms-B5-V.h"
#include "cmaps/EUC-H.h"
#include "cmaps/EUC-V.h"
#include "cmaps/Ext-RKSJ-H.h"
#include "cmaps/Ext-RKSJ-V.h"
#include "cmaps/GB-EUC-H.h"
#include "cmaps/GB-EUC-V.h"
#include "cmaps/GBK-EUC-H.h"
#include "cmaps/GBK-EUC-V.h"
#include "cmaps/GBK-X.h"
#include "cmaps/GBK2K-H.h"
#include "cmaps/GBK2K-V.h"
#include "cmaps/GBKp-EUC-H.h"
#include "cmaps/GBKp-EUC-V.h"
#include "cmaps/GBpc-EUC-H.h"
#include "cmaps/GBpc-EUC-V.h"
#include "cmaps/H.h"
#include "cmaps/HKscs-B5-H.h"
#include "cmaps/HKscs-B5-V.h"
#include "cmaps/Identity-H.h"
#include "cmaps/Identity-V.h"
#include "cmaps/KSC-EUC-H.h"
#include "cmaps/KSC-EUC-V.h"
#include "cmaps/KSCms-UHC-H.h"
#include "cmaps/KSCms-UHC-HW-H.h"
#include "cmaps/KSCms-UHC-HW-V.h"
#include "cmaps/KSCms-UHC-V.h"
#include "cmaps/KSCpc-EUC-H.h"
#include "cmaps/UniCNS-UCS2-H.h"
#include "cmaps/UniCNS-UCS2-V.h"
#include "cmaps/UniCNS-UTF16-H.h"
#include "cmaps/UniCNS-UTF16-V.h"
#include "cmaps/UniCNS-X.h"
#include "cmaps/UniGB-UCS2-H.h"
#include "cmaps/UniGB-UCS2-V.h"
#include "cmaps/UniGB-UTF16-H.h"
#include "cmaps/UniGB-UTF16-V.h"
#include "cmaps/UniGB-X.h"
#include "cmaps/UniJIS-UCS2-H.h"
#include "cmaps/UniJIS-UCS2-HW-H.h"
#include "cmaps/UniJIS-UCS2-HW-V.h"
#include "cmaps/UniJIS-UCS2-V.h"
#include "cmaps/UniJIS-UTF16-H.h"
#include "cmaps/UniJIS-UTF16-V.h"
#include "cmaps/UniJIS-X.h"
#include "cmaps/UniKS-UCS2-H.h"
#include "cmaps/UniKS-UCS2-V.h"
#include "cmaps/UniKS-UTF16-H.h"
#include "cmaps/UniKS-UTF16-V.h"
#include "cmaps/UniKS-X.h"
#include "cmaps/V.h"

static pdf_cmap *table[] = {
	&cmap_83pv_RKSJ_H,
	&cmap_90ms_RKSJ_H,
	&cmap_90ms_RKSJ_V,
	&cmap_90msp_RKSJ_H,
	&cmap_90msp_RKSJ_V,
	&cmap_90pv_RKSJ_H,
	&cmap_Add_RKSJ_H,
	&cmap_Add_RKSJ_V,
	&cmap_Adobe_CNS1_UCS2,
	&cmap_Adobe_GB1_UCS2,
	&cmap_Adobe_Japan1_UCS2,
	&cmap_Adobe_Korea1_UCS2,
	&cmap_B5pc_H,
	&cmap_B5pc_V,
	&cmap_CNS_EUC_H,
	&cmap_CNS_EUC_V,
	&cmap_ETen_B5_H,
	&cmap_ETen_B5_V,
	&cmap_ETenms_B5_H,
	&cmap_ETenms_B5_V,
	&cmap_EUC_H,
	&cmap_EUC_V,
	&cmap_Ext_RKSJ_H,
	&cmap_Ext_RKSJ_V,
	&cmap_GB_EUC_H,
	&cmap_GB_EUC_V,
	&cmap_GBK_EUC_H,
	&cmap_GBK_EUC_V,
	&cmap_GBK_X,
	&cmap_GBK2K_H,
	&cmap_GBK2K_V,
	&cmap_GBKp_EUC_H,
	&cmap_GBKp_EUC_V,
	&cmap_GBpc_EUC_H,
	&cmap_GBpc_EUC_V,
	&cmap_H,
	&cmap_HKscs_B5_H,
	&cmap_HKscs_B5_V,
	&cmap_Identity_H,
	&cmap_Identity_V,
	&cmap_KSC_EUC_H,
	&cmap_KSC_EUC_V,
	&cmap_KSCms_UHC_H,
	&cmap_KSCms_UHC_HW_H,
	&cmap_KSCms_UHC_HW_V,
	&cmap_KSCms_UHC_V,
	&cmap_KSCpc_EUC_H,
	&cmap_UniCNS_UCS2_H,
	&cmap_UniCNS_UCS2_V,
	&cmap_UniCNS_UTF16_H,
	&cmap_UniCNS_UTF16_V,
	&cmap_UniCNS_X,
	&cmap_UniGB_UCS2_H,
	&cmap_UniGB_UCS2_V,
	&cmap_UniGB_UTF16_H,
	&cmap_UniGB_UTF16_V,
	&cmap_UniGB_X,
	&cmap_UniJIS_UCS2_H,
	&cmap_UniJIS_UCS2_HW_H,
	&cmap_UniJIS_UCS2_HW_V,
	&cmap_UniJIS_UCS2_V,
	&cmap_UniJIS_UTF16_H,
	&cmap_UniJIS_UTF16_V,
	&cmap_UniJIS_X,
	&cmap_UniKS_UCS2_H,
	&cmap_UniKS_UCS2_V,
	&cmap_UniKS_UTF16_H,
	&cmap_UniKS_UTF16_V,
	&cmap_UniKS_X,
	&cmap_V,
};

pdf_cmap *
pdf_load_builtin_cmap(fz_context *ctx, const char *name)
{
	int r = nelem(table)-1;
	int l = 0;
	while (l <= r)
	{
		int m = (l + r) >> 1;
		int c = strcmp(name, table[m]->cmap_name);
		if (c < 0)
			r = m - 1;
		else if (c > 0)
			l = m + 1;
		else
			return table[m];
	}
	return NULL;
}

#endif

/*
 * Load predefined CMap from system.
 */
pdf_cmap *
pdf_load_system_cmap(fz_context *ctx, const char *cmap_name)
{
	pdf_cmap *usecmap;
	pdf_cmap *cmap;

	cmap = pdf_load_builtin_cmap(ctx, cmap_name);
	if (!cmap)
		fz_throw(ctx, FZ_ERROR_GENERIC, "no builtin cmap file: %s", cmap_name);

	if (cmap->usecmap_name[0] && !cmap->usecmap)
	{
		usecmap = pdf_load_system_cmap(ctx, cmap->usecmap_name);
		if (!usecmap)
			fz_throw(ctx, FZ_ERROR_GENERIC, "no builtin cmap file: %s", cmap->usecmap_name);
		pdf_set_usecmap(ctx, cmap, usecmap);
	}

	return cmap;
}
