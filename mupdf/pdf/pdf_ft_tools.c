#include "fitz.h"
#include "mupdf.h"

/**
 * the following code has been adapted from
 * http://code.google.com/p/ezgdi/source/browse/trunk/ezgdi/ft2vert.c?r=56
 * 
 * per http://code.google.com/p/ezgdi/ the whole project is licensed both
 * under GPLv3 and the FreeType license
 */

/*
 * "ft2vert.c"
 * 
 * Converter to vertical glyph ID by handling GSUB vrt2/vert feature
 * requires FreeType-2.1.10 or latter
 *
 * (C) 2005 Nobuyuki TSUCHIMURA
 *
 * using such Lookup
 *   ScriptTag == 'kana'
 *   DefaultLangSys or LangSysTag == 'JAN '
 *   FeatureTag == 'vrt2' or 'vert'
 *
 * [reference]
 * http://partners.adobe.com/public/developer/opentype/index_table_formats1.html
 * http://partners.adobe.com/public/developer/opentype/index_table_formats.html
 * http://partners.adobe.com/public/developer/opentype/index_tag9.html#vrt2
 */

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OPENTYPE_VALIDATE_H

#define TAG_KANA FT_MAKE_TAG('k', 'a', 'n', 'a')
#define TAG_JAN  FT_MAKE_TAG('J', 'A', 'N', ' ')
#define TAG_VERT FT_MAKE_TAG('v', 'e', 'r', 't')
#define TAG_VRT2 FT_MAKE_TAG('v', 'r', 't', '2')

#define BYTE2(p) ((p) += 2, (int)(p)[-2] << 8  | (p)[-1])
#define BYTE4(p) ((p) += 4, (int)(p)[-4] << 24 | (int)(p)[-3] << 16 | (int)(p)[-2] << 8 | (p)[-1])

struct ft2vert_st
{
	int SubTableCount;
	struct SubTable_st
	{
		struct SingleSubst_st
		{
			FT_UInt SubstFormat;
			FT_UInt DeltaGlyphID; /* SubstFormat == 1 */
			int     GlyphCount;   /* SubstFormat == 2 */
			FT_UInt *Substitute;  /* SubstFormat == 2 */
		} SingleSubst;
		struct Coverage_st
		{
			FT_UInt CoverageFormat;
			int     GlyphCount;   /* CoverageFormat == 1 */
			FT_UInt *GlyphArray;  /* CoverageFormat == 1 */
			int     RangeCount;   /* CoverageFormat == 2 */
			struct  RangeRecord_st
			{
				FT_UInt Start, End;
			} *RangeRecord;       /* CoverageFormat == 2 */
		} Coverage;
	} *SubTable;

	FT_Bytes GSUB_table;
	FT_Bytes kanaFeature;
	FT_Bytes vertLookup, vrt2Lookup;
};


static int isInIndex(FT_Bytes s, int index)
{
	int i, count;

	if (s == NULL)
		return 0;
	count = BYTE2(s);
	for (i = 0; i < count; i++)
		if (index == BYTE2(s))
			return 1;
	return 0;
}

/**********  Lookup part ***************/

static void scan_Coverage(struct ft2vert_st *ret, const FT_Bytes top)
{
	int i;
	FT_Bytes s = top;
	struct Coverage_st *t;

	t = &ret->SubTable[ret->SubTableCount].Coverage;
	t->CoverageFormat = BYTE2(s);
	switch (t->CoverageFormat)
	{
	case 1: 
		t->GlyphCount = BYTE2(s);
		t->GlyphArray = fz_calloc(t->GlyphCount, sizeof(t->GlyphArray[0]));
		memset(t->GlyphArray, 0, t->GlyphCount * sizeof(t->GlyphArray[0]));
		for (i = 0; i < t->GlyphCount; i++)
			t->GlyphArray[i] = BYTE2(s);
		break;
	case 2:
		t->RangeCount = BYTE2(s);
		t->RangeRecord = fz_calloc(t->RangeCount, sizeof(t->RangeRecord[0]));
		memset(t->RangeRecord, 0, t->RangeCount * sizeof(t->RangeRecord[0]));
		for (i = 0; i < t->RangeCount; i++)
		{
			t->RangeRecord[i].Start = BYTE2(s);
			t->RangeRecord[i].End   = BYTE2(s);
			s += 2; /* drop StartCoverageIndex */
		}
		break;
	default:
		fz_warn("scan_Coverage: unknown CoverageFormat (%d).", t->CoverageFormat);
		return;
	}
	ret->SubTableCount++;
}

static void scan_SubTable(struct ft2vert_st *ret, const FT_Bytes top)
{
	int i;
	FT_Bytes s = top;
	FT_Offset Coverage;
	struct SingleSubst_st *t;

	t = &ret->SubTable[ret->SubTableCount].SingleSubst;
	t->SubstFormat = BYTE2(s);
	Coverage       = BYTE2(s);
	scan_Coverage(ret, top + Coverage);
	switch (t->SubstFormat)
	{
	case 1: /* SingleSubstFormat1 */
		t->DeltaGlyphID = BYTE2(s);
		break;
	case 2: /* SingleSubstFormat2 */
		t->GlyphCount   = BYTE2(s);
		t->Substitute = fz_calloc(t->GlyphCount, sizeof(t->Substitute[0]));
		memset(t->Substitute, 0, t->GlyphCount * sizeof(t->Substitute[0]));
		for (i = 0; i < t->GlyphCount; i++)
			t->Substitute[i] = BYTE2(s);
		break;
	default:
		fz_warn("scan_SubTable: unknown SubstFormat (%d).", t->SubstFormat);
	}
}

static void scan_Lookup(struct ft2vert_st *ret, const FT_Bytes top)
{
	int i;
	FT_Bytes s = top;
	FT_UShort LookupType;
	FT_UShort LookupFlag;
	FT_UShort SubTableCount;
	FT_UShort SubTable;

	LookupType    = BYTE2(s);
	LookupFlag    = BYTE2(s);
	SubTableCount = BYTE2(s);
	SubTable      = BYTE2(s);

	ret->SubTable = fz_calloc(SubTableCount, sizeof(ret->SubTable[0]));
	memset(ret->SubTable, 0, SubTableCount * sizeof(ret->SubTable[0]));
	for (i = 0; i < SubTableCount; i++)
		scan_SubTable(ret, top + SubTable);

	if (ret->SubTableCount != SubTableCount)
		fz_warn("scan_Lookup: SubTableCount (=%d) is not expected (=%d).\n",
				ret->SubTableCount, SubTableCount);
}

static void scan_LookupList(struct ft2vert_st *ret, const FT_Bytes top)
{
	int i;
	FT_Bytes s = top;
	int LookupCount;

	LookupCount = BYTE2(s);

	for (i = 0; i < LookupCount; i++)
	{
		FT_Bytes t = top + BYTE2(s);
		if (isInIndex(ret->vertLookup, i))
			scan_Lookup(ret, t);
	}
}

/********** Feature part ****************/

static void scan_FeatureList(struct ft2vert_st *ret, const FT_Bytes top)
{
	int i;
	FT_Bytes s = top;
	int FeatureCount;

	FeatureCount = BYTE2(s);

	for (i = 0; i < FeatureCount; i++)
	{
		FT_Tag FeatureTag = BYTE4(s);
		FT_Offset Feature = BYTE2(s);
		if (isInIndex(ret->kanaFeature, i))
		{
			switch (FeatureTag)
			{
			case TAG_VERT: ret->vertLookup = top + Feature + 2; break;
			case TAG_VRT2: ret->vrt2Lookup = top + Feature + 2; break;
			}
		}
	}
}

/********** Script part ****************/

static void scan_LangSys(struct ft2vert_st *ret, const FT_Bytes top, const FT_Tag ScriptTag)
{
	if (ScriptTag == TAG_KANA && ret->kanaFeature == NULL)
		ret->kanaFeature = top + 4;
}

static void scan_Script(struct ft2vert_st *ret, const FT_Bytes top, const FT_Tag ScriptTag)
{
	int i;
	FT_Bytes s = top;
	FT_Offset DefaultLangSys;
	int LangSysCount;

	DefaultLangSys = BYTE2(s);
	if (DefaultLangSys != 0)
		scan_LangSys(ret, top + DefaultLangSys, ScriptTag);
	LangSysCount = BYTE2(s);

	for (i = 0; i < LangSysCount; i++) 
	{
		FT_Tag LangSysTag = BYTE4(s);
		FT_Bytes t = top + BYTE2(s);
		if (LangSysTag == TAG_JAN)
			scan_LangSys(ret, t, ScriptTag);
	}
}

static void scan_ScriptList(struct ft2vert_st *ret, const FT_Bytes top)
{
	int i;
	FT_Bytes s = top;
	int ScriptCount;

	ScriptCount = BYTE2(s);

	for (i = 0; i < ScriptCount; i++)
	{
		FT_Tag ScriptTag = BYTE4(s);
		FT_Bytes t = top + BYTE2(s);
		if (ScriptTag == TAG_KANA)
			scan_Script(ret, t, ScriptTag);
	}
}

/********** header part *****************/

static void scan_GSUB_Header(struct ft2vert_st *ret, const FT_Bytes top)
{
	FT_Bytes s = top;
	FT_Fixed  Version;
	FT_Offset ScriptList;
	FT_Offset FeatureList;
	FT_Offset LookupList;

	Version     = BYTE4(s);
	ScriptList  = BYTE2(s);
	FeatureList = BYTE2(s);
	LookupList  = BYTE2(s);

	if (Version != 0x00010000)
		fz_warn("GSUB Version (=%.1f) is not 1.0\n", (double)Version / 0x10000);

	scan_ScriptList(ret, top + ScriptList);
	scan_FeatureList(ret, top + FeatureList);
	/* vrt2 has higher priority over vert */
	if (ret->vrt2Lookup != NULL)
		ret->vertLookup = ret->vrt2Lookup;
	scan_LookupList(ret, top + LookupList);
}

static struct ft2vert_st *ft2vert_init(FT_Face face)
{
	struct ft2vert_st *ret;
	int ft_error;
	FT_Bytes base, gdef, gpos, jstf;

	ret = fz_malloc(sizeof(ret[0]));
	memset(ret, 0, sizeof(ret[0]));

	ft_error = FT_OpenType_Validate(face, FT_VALIDATE_GSUB, &base, &gdef, &gpos, &ret->GSUB_table, &jstf);
	assert(ft_error != FT_Err_Unimplemented_Feature); // make sure to enable the otvalid module
	if (ft_error != 0 || ret->GSUB_table == 0)
	{
		fz_warn("%s has no GSUB table.\n", face->family_name);
		return ret;
	}
	scan_GSUB_Header(ret, ret->GSUB_table);
	if (ret->SubTableCount == 0)
		fz_warn("%s has no vrt2/vert feature.\n", face->family_name);

	return ret;
}

static void ft2vert_final(FT_Face face, struct ft2vert_st *vert){
	int j;
	for (j = 0; j < vert->SubTableCount; j++)
	{
		fz_free(vert->SubTable[j].SingleSubst.Substitute);
		fz_free(vert->SubTable[j].Coverage.GlyphArray);
		fz_free(vert->SubTable[j].Coverage.RangeRecord);
	}
	fz_free(vert->SubTable);
	FT_OpenType_Free(face, vert->GSUB_table);
	fz_free(vert);
}

/********** converting part *****************/

static FT_UInt get_vert_nth_gid(struct SubTable_st *t, FT_UInt gid, int n)
{
	switch (t->SingleSubst.SubstFormat)
	{
	case 1: return gid + t->SingleSubst.DeltaGlyphID;
	case 2: return t->SingleSubst.Substitute[n];
	default: fz_warn("get_vert_nth_gid: internal error");
	}
	return 0;
}

static FT_UInt ft2gsub_get_gid(const struct ft2vert_st *ft2vert, const FT_UInt gid)
{
	int i, k;
	int j = 0; /* StartCoverageIndex */

	for (k = 0; k < ft2vert->SubTableCount; k++)
	{
		struct SubTable_st *t = &ft2vert->SubTable[k];
		switch (t->Coverage.CoverageFormat)
		{
		case 1:
			for (i = 0; i < t->Coverage.GlyphCount; i++)
				if (t->Coverage.GlyphArray[i] == gid)
					return get_vert_nth_gid(t, gid, i);
			break;
		case 2:
			for (i = 0; i < t->Coverage.RangeCount; i++)
			{
				struct RangeRecord_st *r = &t->Coverage.RangeRecord[i];
				if (r->Start <= gid && gid <= r->End)
					return get_vert_nth_gid(t, gid, gid - r->Start + j);
				j += r->End - r->Start + 1;
			}
			break;
		default:
			fz_warn("ft2gsub_get_gid: internal error");
		}
	}

	return 0;
}


int pdf_ft_get_vgid(pdf_font_desc *fontdesc, int gid)
{
	int vgid = 0;
	if (!fontdesc->_vsubst)
		fontdesc->_vsubst = ft2vert_init(fontdesc->font->ft_face);
	vgid = ft2gsub_get_gid(fontdesc->_vsubst, gid);
	return vgid ? vgid : gid;
}

void pdf_ft_free_vsubst(pdf_font_desc *fontdesc)
{
	if (fontdesc && fontdesc->_vsubst)
		ft2vert_final(fontdesc->font->ft_face, fontdesc->_vsubst);
}
