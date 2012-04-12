#include "fitz-internal.h"
#include "mupdf-internal.h"

#ifdef NOCJK
#define NOCJKFONT
#endif

#include "../generated/font_base14.h"

#ifndef NODROIDFONT
#include "../generated/font_droid.h"
#endif

#ifndef NOCJKFONT
#include "../generated/font_cjk.h"
#endif

unsigned char *
pdf_lookup_builtin_font(char *name, unsigned int *len)
{
	if (!strcmp("Courier", name)) {
		*len = sizeof pdf_font_NimbusMonL_Regu;
		return (unsigned char*) pdf_font_NimbusMonL_Regu;
	}
	if (!strcmp("Courier-Bold", name)) {
		*len = sizeof pdf_font_NimbusMonL_Bold;
		return (unsigned char*) pdf_font_NimbusMonL_Bold;
	}
	if (!strcmp("Courier-Oblique", name)) {
		*len = sizeof pdf_font_NimbusMonL_ReguObli;
		return (unsigned char*) pdf_font_NimbusMonL_ReguObli;
	}
	if (!strcmp("Courier-BoldOblique", name)) {
		*len = sizeof pdf_font_NimbusMonL_BoldObli;
		return (unsigned char*) pdf_font_NimbusMonL_BoldObli;
	}
	if (!strcmp("Helvetica", name)) {
		*len = sizeof pdf_font_NimbusSanL_Regu;
		return (unsigned char*) pdf_font_NimbusSanL_Regu;
	}
	if (!strcmp("Helvetica-Bold", name)) {
		*len = sizeof pdf_font_NimbusSanL_Bold;
		return (unsigned char*) pdf_font_NimbusSanL_Bold;
	}
	if (!strcmp("Helvetica-Oblique", name)) {
		*len = sizeof pdf_font_NimbusSanL_ReguItal;
		return (unsigned char*) pdf_font_NimbusSanL_ReguItal;
	}
	if (!strcmp("Helvetica-BoldOblique", name)) {
		*len = sizeof pdf_font_NimbusSanL_BoldItal;
		return (unsigned char*) pdf_font_NimbusSanL_BoldItal;
	}
	if (!strcmp("Times-Roman", name)) {
		*len = sizeof pdf_font_NimbusRomNo9L_Regu;
		return (unsigned char*) pdf_font_NimbusRomNo9L_Regu;
	}
	if (!strcmp("Times-Bold", name)) {
		*len = sizeof pdf_font_NimbusRomNo9L_Medi;
		return (unsigned char*) pdf_font_NimbusRomNo9L_Medi;
	}
	if (!strcmp("Times-Italic", name)) {
		*len = sizeof pdf_font_NimbusRomNo9L_ReguItal;
		return (unsigned char*) pdf_font_NimbusRomNo9L_ReguItal;
	}
	if (!strcmp("Times-BoldItalic", name)) {
		*len = sizeof pdf_font_NimbusRomNo9L_MediItal;
		return (unsigned char*) pdf_font_NimbusRomNo9L_MediItal;
	}
	if (!strcmp("Symbol", name)) {
		*len = sizeof pdf_font_StandardSymL;
		return (unsigned char*) pdf_font_StandardSymL;
	}
	if (!strcmp("ZapfDingbats", name)) {
		*len = sizeof pdf_font_Dingbats;
		return (unsigned char*) pdf_font_Dingbats;
	}
	*len = 0;
	return NULL;
}

unsigned char *
pdf_lookup_substitute_font(int mono, int serif, int bold, int italic, unsigned int *len)
{
#ifdef NODROIDFONT
	if (mono) {
		if (bold) {
			if (italic) return pdf_lookup_builtin_font("Courier-BoldOblique", len);
			else return pdf_lookup_builtin_font("Courier-Bold", len);
		} else {
			if (italic) return pdf_lookup_builtin_font("Courier-Oblique", len);
			else return pdf_lookup_builtin_font("Courier", len);
		}
	} else if (serif) {
		if (bold) {
			if (italic) return pdf_lookup_builtin_font("Times-BoldItalic", len);
			else return pdf_lookup_builtin_font("Times-Bold", len);
		} else {
			if (italic) return pdf_lookup_builtin_font("Times-Italic", len);
			else return pdf_lookup_builtin_font("Times-Roman", len);
		}
	} else {
		if (bold) {
			if (italic) return pdf_lookup_builtin_font("Helvetica-BoldOblique", len);
			else return pdf_lookup_builtin_font("Helvetica-Bold", len);
		} else {
			if (italic) return pdf_lookup_builtin_font("Helvetica-Oblique", len);
			else return pdf_lookup_builtin_font("Helvetica", len);
		}
	}
#else
	if (mono) {
		*len = sizeof pdf_font_DroidSansMono;
		return (unsigned char*) pdf_font_DroidSansMono;
	} else {
		*len = sizeof pdf_font_DroidSans;
		return (unsigned char*) pdf_font_DroidSans;
	}
#endif
}

unsigned char *
pdf_lookup_substitute_cjk_font(int ros, int serif, unsigned int *len)
{
#ifndef NOCJKFONT
	*len = sizeof pdf_font_DroidSansFallback;
	return (unsigned char*) pdf_font_DroidSansFallback;
#else
	*len = 0;
	return NULL;
#endif
}

/* SumatraPDF: also load fonts included with Windows */
#ifdef _WIN32

#include <windows.h>
#include <tchar.h>

// TODO: Use more of FreeType for TTF parsing (for performance reasons,
//       the fonts can't be parsed completely, though)
#include <ft2build.h>
#include FT_TRUETYPE_IDS_H
#include FT_TRUETYPE_TAGS_H

#define TTC_VERSION1	0x00010000
#define TTC_VERSION2	0x00020000

#define MAX_FACENAME	128

// Note: the font face must be the first field so that the structure
//       can be treated like a simple string for searching
typedef struct pdf_fontmapMS_s
{
	char fontface[MAX_FACENAME]; // UTF-8 encoded
	char fontpath[MAX_PATH];     // ANSI encoded
	int index;
} pdf_fontmapMS;

typedef struct pdf_fontlistMS_s
{
	pdf_fontmapMS *fontmap;
	int len;
	int cap;
} pdf_fontlistMS;

typedef struct _tagTT_OFFSET_TABLE
{
	ULONG	uVersion;
	USHORT	uNumOfTables;
	USHORT	uSearchRange;
	USHORT	uEntrySelector;
	USHORT	uRangeShift;
} TT_OFFSET_TABLE;

typedef struct _tagTT_TABLE_DIRECTORY
{
	ULONG	uTag;				//table name
	ULONG	uCheckSum;			//Check sum
	ULONG	uOffset;			//Offset from beginning of file
	ULONG	uLength;			//length of the table in bytes
} TT_TABLE_DIRECTORY;

typedef struct _tagTT_NAME_TABLE_HEADER
{
	USHORT	uFSelector;			//format selector. Always 0
	USHORT	uNRCount;			//Name Records count
	USHORT	uStorageOffset;		//Offset for strings storage, from start of the table
} TT_NAME_TABLE_HEADER;

typedef struct _tagTT_NAME_RECORD
{
	USHORT	uPlatformID;
	USHORT	uEncodingID;
	USHORT	uLanguageID;
	USHORT	uNameID;
	USHORT	uStringLength;
	USHORT	uStringOffset;	//from start of storage area
} TT_NAME_RECORD;

typedef struct _tagFONT_COLLECTION
{
	ULONG	Tag;
	ULONG	Version;
	ULONG	NumFonts;
} FONT_COLLECTION;

static struct {
	char *name;
	char *pattern;
} baseSubstitutes[] = {
	{ "Courier", "CourierNewPSMT" },
	{ "Courier-Bold", "CourierNewPS-BoldMT" },
	{ "Courier-Oblique", "CourierNewPS-ItalicMT" },
	{ "Courier-BoldOblique", "CourierNewPS-BoldItalicMT" },
	{ "Helvetica", "ArialMT" },
	{ "Helvetica-Bold", "Arial-BoldMT" },
	{ "Helvetica-Oblique", "Arial-ItalicMT" },
	{ "Helvetica-BoldOblique", "Arial-BoldItalicMT" },
	{ "Times-Roman", "TimesNewRomanPSMT" },
	{ "Times-Bold", "TimesNewRomanPS-BoldMT" },
	{ "Times-Italic", "TimesNewRomanPS-ItalicMT" },
	{ "Times-BoldItalic", "TimesNewRomanPS-BoldItalicMT" },
	{ "Symbol", "SymbolMT" },
};

static pdf_fontlistMS fontlistMS =
{
	NULL,
	0,
	0,
};

static inline USHORT BEtoHs(USHORT x)
{
	BYTE *data = (BYTE *)&x;
	return (data[0] << 8) + data[1];
}

static inline ULONG BEtoHl(ULONG x)
{
	BYTE *data = (BYTE *)&x;
	return (data[0] << 24) + (data[1] << 16) + (data[2] << 8) + data[3];
}

/* A little bit more sophisticated name matching so that e.g. "EurostileExtended"
   matches "EurostileExtended-Roman" or "Tahoma-Bold,Bold" matches "Tahoma-Bold" */
static int
lookup_compare(const void *elem1, const void *elem2)
{
	const char *val1 = elem1;
	const char *val2 = elem2;
	int len1 = strlen(val1);
	int len2 = strlen(val2);

	if (len1 != len2)
	{
		const char *rest = len1 > len2 ? val1 + len2 : val2 + len1;
		if (',' == *rest || !_stricmp(rest, "-roman"))
			return _strnicmp(val1, val2, MIN(len1, len2));
	}

	return _stricmp(val1, val2);
}

static void
remove_spaces(char *srcDest)
{
	char *dest;

	for (dest = srcDest; *srcDest; srcDest++)
		if (*srcDest != ' ')
			*dest++ = *srcDest;
	*dest = '\0';
}

static int
strendswith(const char *str, const char *end)
{
	size_t len1 = strlen(str);
	size_t len2 = strlen(end);

	return len1 >= len2 && !strcmp(str + len1 - len2, end);
}

static pdf_fontmapMS*
pdf_find_windows_font_path(const char *fontname)
{
	return bsearch(fontname, fontlistMS.fontmap, fontlistMS.len, sizeof(pdf_fontmapMS), lookup_compare);
}

/* source and dest can be same */
static void
decode_unicode_BE(fz_context *ctx, char *source, int sourcelen, char *dest, int destlen)
{
	WCHAR *tmp;
	int converted, i;

	if (sourcelen % 2 != 0)
		fz_throw(ctx, "fonterror : invalid unicode string");

	tmp = fz_malloc_array(ctx, sourcelen / 2 + 1, sizeof(WCHAR));
	for (i = 0; i < sourcelen / 2; i++)
		tmp[i] = BEtoHs(((WCHAR *)source)[i]);
	tmp[sourcelen / 2] = '\0';

	converted = WideCharToMultiByte(CP_UTF8, 0, tmp, -1, dest, destlen, NULL, NULL);
	fz_free(ctx, tmp);
	if (!converted)
		fz_throw(ctx, "fonterror : invalid unicode string");
}

static void
decode_platform_string(fz_context *ctx, int platform, int enctype, char *source, int sourcelen, char *dest, int destlen)
{
	switch (platform)
	{
	case TT_PLATFORM_APPLE_UNICODE:
		switch (enctype)
		{
		case TT_APPLE_ID_DEFAULT:
		case TT_APPLE_ID_UNICODE_2_0:
			decode_unicode_BE(ctx, source, sourcelen, dest, destlen);
			return;
		}
		fz_throw(ctx, "fonterror : unsupported encoding (%d/%d)", platform, enctype);
	case TT_PLATFORM_MACINTOSH:
		switch(enctype)
		{
		case TT_MAC_ID_ROMAN:
			if (sourcelen + 1 > destlen)
				fz_throw(ctx, "fonterror : overlong fontname: %s", source);
			// TODO: Convert to UTF-8 from what encoding?
			memcpy(dest, source, sourcelen);
			dest[sourcelen] = 0;
			return;
		}
		fz_throw(ctx, "fonterror : unsupported encoding (%d/%d)", platform, enctype);
	case TT_PLATFORM_MICROSOFT:
		switch (enctype)
		{
		case TT_MS_ID_SYMBOL_CS:
		case TT_MS_ID_UNICODE_CS:
		case TT_MS_ID_UCS_4:
			decode_unicode_BE(ctx, source, sourcelen, dest, destlen);
			return;
		}
		fz_throw(ctx, "fonterror : unsupported encoding (%d/%d)", platform, enctype);
	default:
		fz_throw(ctx, "fonterror : unsupported encoding (%d/%d)", platform, enctype);
	}
}

static void
grow_system_font_list(fz_context *ctx, pdf_fontlistMS *fl)
{
	int newcap;
	pdf_fontmapMS *newitems;

	if (fl->cap == 0)
		newcap = 1024;
	else
		newcap = fl->cap * 2;

	// use realloc/free for the fontmap, since the list can
	// remain in memory even with all fz_contexts destroyed
	newitems = realloc(fl->fontmap, newcap * sizeof(pdf_fontmapMS));
	if (!newitems)
		fz_throw(ctx, "OOM in grow_system_font_list");
	memset(newitems + fl->cap, 0, sizeof(pdf_fontmapMS) * (newcap - fl->cap));

	fl->fontmap = newitems;
	fl->cap = newcap;
}

static void
insert_mapping(fz_context *ctx, pdf_fontlistMS *fl, char *facename, char *path, int index)
{
	if (fl->len == fl->cap)
		grow_system_font_list(ctx, fl);

	if (fl->len >= fl->cap)
		fz_throw(ctx, "fonterror : fontlist overflow");

	fz_strlcpy(fl->fontmap[fl->len].fontface, facename, sizeof(fl->fontmap[0].fontface));
	fz_strlcpy(fl->fontmap[fl->len].fontpath, path, sizeof(fl->fontmap[0].fontpath));
	fl->fontmap[fl->len].index = index;

	++fl->len;
}

static void
safe_read(fz_stream *file, char *buf, int size)
{
	int n = fz_read(file, buf, size);
	if (n != size) /* covers n < 0 case */
		fz_throw(file->ctx, "ioerror : read %d, expected %d", n, size);
}

static void
read_ttf_string(fz_stream *file, int offset, TT_NAME_RECORD *ttRecordBE, char *buf, int size)
{
	char szTemp[MAX_FACENAME * 2];
	// ignore empty and overlong strings
	int stringLength = BEtoHs(ttRecordBE->uStringLength);
	if (stringLength == 0 || stringLength >= sizeof(szTemp))
		return;

	fz_seek(file, offset + BEtoHs(ttRecordBE->uStringOffset), 0);
	safe_read(file, szTemp, stringLength);
	decode_platform_string(file->ctx, BEtoHs(ttRecordBE->uPlatformID),
		BEtoHs(ttRecordBE->uEncodingID), szTemp, stringLength, buf, size);
}

static void
parseTTF(fz_stream *file, int offset, int index, char *path)
{
	TT_OFFSET_TABLE ttOffsetTableBE;
	TT_TABLE_DIRECTORY tblDirBE;
	TT_NAME_TABLE_HEADER ttNTHeaderBE;
	TT_NAME_RECORD ttRecordBE;

	char szPSName[MAX_FACENAME] = { 0 }, szTTName[MAX_FACENAME] = { 0 }, szStyle[MAX_FACENAME] = { 0 };
	int i, count, tblOffset;

	fz_seek(file, offset, 0);
	safe_read(file, (char *)&ttOffsetTableBE, sizeof(TT_OFFSET_TABLE));

	// check if this is a TrueType font of version 1.0 or an OpenType font
	if (BEtoHl(ttOffsetTableBE.uVersion) != TTC_VERSION1 &&
		BEtoHl(ttOffsetTableBE.uVersion) != TTAG_OTTO)
	{
		fz_throw(file->ctx, "fonterror : invalid font version %x", BEtoHl(ttOffsetTableBE.uVersion));
	}

	// determine the name table's offset by iterating through the offset table
	count = BEtoHs(ttOffsetTableBE.uNumOfTables);
	for (i = 0; i < count; i++)
	{
		safe_read(file, (char *)&tblDirBE, sizeof(TT_TABLE_DIRECTORY));
		if (!BEtoHl(tblDirBE.uTag) || BEtoHl(tblDirBE.uTag) == TTAG_name)
			break;
	}
	if (count == i || !BEtoHl(tblDirBE.uTag))
		fz_throw(file->ctx, "fonterror : nameless font");
	tblOffset = BEtoHl(tblDirBE.uOffset);

	// read the 'name' table for record count and offsets
	fz_seek(file, tblOffset, 0);
	safe_read(file, (char *)&ttNTHeaderBE, sizeof(TT_NAME_TABLE_HEADER));
	offset = tblOffset + sizeof(TT_NAME_TABLE_HEADER);
	tblOffset += BEtoHs(ttNTHeaderBE.uStorageOffset);

	// read through the strings for PostScript name and font family
	count = BEtoHs(ttNTHeaderBE.uNRCount);
	for (i = 0; i < count; i++)
	{
		short nameId;

		fz_seek(file, offset + i * sizeof(TT_NAME_RECORD), 0);
		safe_read(file, (char *)&ttRecordBE, sizeof(TT_NAME_RECORD));

		// ignore non-English strings
		if (BEtoHs(ttRecordBE.uLanguageID) && BEtoHs(ttRecordBE.uLanguageID) != TT_MS_LANGID_ENGLISH_UNITED_STATES)
			continue;
		// ignore names other than font (sub)family and PostScript name
		nameId = BEtoHs(ttRecordBE.uNameID);
		fz_try(file->ctx)
		{
			if (TT_NAME_ID_FONT_FAMILY == nameId)
				read_ttf_string(file, tblOffset, &ttRecordBE, szTTName, MAX_FACENAME);
			else if (TT_NAME_ID_FONT_SUBFAMILY == nameId)
				read_ttf_string(file, tblOffset, &ttRecordBE, szStyle, MAX_FACENAME);
			else if (TT_NAME_ID_PS_NAME == nameId)
				read_ttf_string(file, tblOffset, &ttRecordBE, szPSName, MAX_FACENAME);
		}
		fz_catch(file->ctx)
		{
			fz_warn(file->ctx, "ignoring face name decoding fonterror");
		}
	}

	// TODO: is there a better way to distinguish Arial Caps from Arial proper?
	// cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1290
	if (!strcmp(szPSName, "ArialMT") && (strstr(path, "caps") || strstr(path, "Caps")))
		fz_throw(file->ctx, "ignore %s, as it can't be distinguished from Arial,Regular", path);

	if (szPSName[0])
		insert_mapping(file->ctx, &fontlistMS, szPSName, path, index);
	if (szTTName[0])
	{
		// derive a PostScript-like name and add it, if it's different from the font's
		// included PostScript name; cf. http://code.google.com/p/sumatrapdf/issues/detail?id=376

		// append the font's subfamily, unless it's a Regular font
		if (szStyle[0] && _stricmp(szStyle, "Regular") != 0)
		{
			fz_strlcat(szTTName, "-", MAX_FACENAME);
			fz_strlcat(szTTName, szStyle, MAX_FACENAME);
		}
		remove_spaces(szTTName);
		// compare the two names before adding this one
		if (lookup_compare(szTTName, szPSName))
			insert_mapping(file->ctx, &fontlistMS, szTTName, path, index);
	}
}

static void
parseTTFs(fz_context *ctx, char *path)
{
	fz_stream *file = fz_open_file(ctx, path);
	/* "fonterror : %s not found", path */
	fz_try(ctx)
	{
		parseTTF(file, 0, 0, path);
	}
	fz_always(ctx)
	{
		fz_close(file);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
parseTTCs(fz_context *ctx, char *path)
{
	FONT_COLLECTION fontcollectionBE;
	ULONG i, numFonts, *offsettableBE = NULL;

	fz_stream *file = fz_open_file(ctx, path);
	/* "fonterror : %s not found", path */

	fz_var(offsettableBE);

	fz_try(ctx)
	{
		safe_read(file, (char *)&fontcollectionBE, sizeof(FONT_COLLECTION));
		if (BEtoHl(fontcollectionBE.Tag) != TTAG_ttcf)
			fz_throw(ctx, "fonterror : wrong format %x", BEtoHl(fontcollectionBE.Tag));
		if (BEtoHl(fontcollectionBE.Version) != TTC_VERSION1 &&
			BEtoHl(fontcollectionBE.Version) != TTC_VERSION2)
		{
			fz_throw(ctx, "fonterror : invalid version %x", BEtoHl(fontcollectionBE.Version));
		}

		numFonts = BEtoHl(fontcollectionBE.NumFonts);
		offsettableBE = fz_malloc_array(ctx, numFonts, sizeof(ULONG));

		safe_read(file, (char *)offsettableBE, numFonts * sizeof(ULONG));
		for (i = 0; i < numFonts; i++)
			parseTTF(file, BEtoHl(offsettableBE[i]), i, path);
	}
	fz_always(ctx)
	{
		fz_free(ctx, offsettableBE);
		fz_close(file);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
extend_system_font_list(fz_context *ctx, const TCHAR *path)
{
	TCHAR szPath[MAX_PATH], *lpFileName;
	WIN32_FIND_DATA FileData;
	HANDLE hList;

	GetFullPathName(path, nelem(szPath), szPath, &lpFileName);

	hList = FindFirstFile(szPath, &FileData);
	if (hList == INVALID_HANDLE_VALUE)
	{
		// Don't complain about missing directories
		if (GetLastError() == ERROR_FILE_NOT_FOUND)
			return;
		fz_throw(ctx, "extend_system_font_list: unknown error %d", GetLastError());
	}
	do
	{
		if (!(FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			char szPathAnsi[MAX_PATH], *fileExt;
			BOOL isNonAnsiPath = FALSE;
			lstrcpyn(lpFileName, FileData.cFileName, szPath + MAX_PATH - lpFileName);
#ifdef _UNICODE
			// FreeType uses fopen and thus requires the path to be in the ANSI code page
			WideCharToMultiByte(CP_ACP, 0, szPath, -1, szPathAnsi, sizeof(szPathAnsi), NULL, &isNonAnsiPath);
#else
			strcpy(szPathAnsi, szPath);
			isNonAnsiPath = strchr(szPathAnsi, '?') != NULL;
#endif
			fileExt = szPathAnsi + strlen(szPathAnsi) - 4;
			fz_try(ctx)
			{
				if (isNonAnsiPath)
					fz_warn(ctx, "ignoring font with non-ANSI filename: %s", szPathAnsi);
				else if (!_stricmp(fileExt, ".ttc"))
					parseTTCs(ctx, szPathAnsi);
				else if (!_stricmp(fileExt, ".ttf") || !_stricmp(fileExt, ".otf"))
					parseTTFs(ctx, szPathAnsi);
			}
			fz_catch(ctx)
			{
				// ignore errors occurring while parsing a given font file
			}
		}
	} while (FindNextFile(hList, &FileData));
	FindClose(hList);
}

static void
destroy_system_font_list(void)
{
	free(fontlistMS.fontmap);
	memset(&fontlistMS, 0, sizeof(fontlistMS));
}

static void
create_system_font_list(fz_context *ctx)
{
	TCHAR szFontDir[MAX_PATH];

	GetWindowsDirectory(szFontDir, nelem(szFontDir) - 12);
	_tcscat_s(szFontDir, MAX_PATH, _T("\\Fonts\\*.?t?"));
	extend_system_font_list(ctx, szFontDir);

	if (fontlistMS.len == 0)
		fz_warn(ctx, "couldn't find any usable system fonts");

#ifdef NOCJKFONT
	{
		// If no CJK fallback font is builtin but one has been shipped separately (in the same
		// directory as the main executable), add it to the list of loadable system fonts
		TCHAR szFile[MAX_PATH], *lpFileName;
		GetModuleFileName(0, szFontDir, MAX_PATH);
		GetFullPathName(szFontDir, MAX_PATH, szFile, &lpFileName);
		lstrcpyn(lpFileName, _T("DroidSansFallback.ttf"), szFile + MAX_PATH - lpFileName);
		extend_system_font_list(ctx, szFile);
	}
#endif

	// sort the font list, so that it can be searched binarily
	qsort(fontlistMS.fontmap, fontlistMS.len, sizeof(pdf_fontmapMS), _stricmp);

	// make sure to clean up after ourselves
	atexit(destroy_system_font_list);
}

void
pdf_load_windows_font(fz_context *ctx, pdf_font_desc *font, char *fontname)
{
	pdf_fontmapMS *found = NULL;
	char *comma, *orig_name = fontname;

	fz_synchronize_begin();
	if (fontlistMS.len == 0)
	{
		fz_try(ctx)
		{
			create_system_font_list(ctx);
		}
		fz_catch(ctx) { }
	}
	fz_synchronize_end();
	if (fontlistMS.len == 0)
		fz_throw(ctx, "fonterror: couldn't find any fonts");

	if (GetEnvironmentVariable(_T("MULOG"), NULL, 0))
		printf("pdf_load_windows_font: looking for font '%s'\n", fontname);

	// work on a normalized copy of the font name
	fontname = fz_strdup(ctx, fontname);
	remove_spaces(fontname);

	// first, try to find the exact font name (including appended style information)
	comma = strchr(fontname, ',');
	if (comma)
	{
		*comma = '-';
		found = pdf_find_windows_font_path(fontname);
		*comma = ',';
	}
	// second, substitute the font name with a known PostScript name
	else
	{
		int i;
		for (i = 0; i < nelem(baseSubstitutes) && !found; i++)
			if (!strcmp(fontname, baseSubstitutes[i].name))
				found = pdf_find_windows_font_path(baseSubstitutes[i].pattern);
	}
	// third, search for the font name without additional style information
	if (!found)
		found = pdf_find_windows_font_path(fontname);
	// fourth, try to separate style from basename for prestyled fonts (e.g. "ArialBold")
	if (!found && !comma && (strendswith(fontname, "Bold") || strendswith(fontname, "Italic")))
	{
		int styleLen = strendswith(fontname, "Bold") ? 4 : strendswith(fontname, "BoldItalic") ? 10 : 6;
		fontname = fz_resize_array(ctx, fontname, strlen(fontname) + 2, sizeof(char));
		comma = fontname + strlen(fontname) - styleLen;
		memmove(comma + 1, comma, styleLen + 1);
		*comma = '-';
		found = pdf_find_windows_font_path(fontname);
		*comma = ',';
		if (!found)
			found = pdf_find_windows_font_path(fontname);
	}

	if (found && (!strcmp(fontname, "Symbol") || !strcmp(fontname, "ZapfDingbats")))
		font->flags |= PDF_FD_SYMBOLIC;

	fz_free(ctx, fontname);
	if (!found)
		fz_throw(ctx, "couldn't find system font '%s'", orig_name);

	if (GetEnvironmentVariable(_T("MULOG"), NULL, 0))
		printf("pdf_load_windows_font: loading font from '%s'\n", found->fontpath);

	font->font = fz_new_font_from_file(ctx, found->fontpath, found->index,
		strcmp(found->fontface, "DroidSansFallback") != 0);
	/* "cannot load freetype font from file %s", found->fontpath */
	font->font->ft_file = fz_strdup(ctx, found->fontpath);
}

void
pdf_load_similar_cjk_font(fz_context *ctx, pdf_font_desc *fontdesc, int ros, int serif)
{
	if (serif)
	{
		switch (ros)
		{
		case PDF_ROS_CNS: pdf_load_windows_font(ctx, fontdesc, "MingLiU"); break;
		case PDF_ROS_GB: pdf_load_windows_font(ctx, fontdesc, "SimSun"); break;
		case PDF_ROS_JAPAN: pdf_load_windows_font(ctx, fontdesc, "MS-Mincho"); break;
		case PDF_ROS_KOREA: pdf_load_windows_font(ctx, fontdesc, "Batang"); break;
		default: fz_throw(ctx, "invalid serif ros");
		}
	}
	else
	{
		switch (ros)
		{
		case PDF_ROS_CNS: pdf_load_windows_font(ctx, fontdesc, "DFKaiShu-SB-Estd-BF"); break;
		case PDF_ROS_GB:
			fz_try(ctx)
			{
				pdf_load_windows_font(ctx, fontdesc, "KaiTi");
			}
			fz_catch(ctx)
			{
				pdf_load_windows_font(ctx, fontdesc, "KaiTi_GB2312");
			}
			break;
		case PDF_ROS_JAPAN: pdf_load_windows_font(ctx, fontdesc, "MS-Gothic"); break;
		case PDF_ROS_KOREA: pdf_load_windows_font(ctx, fontdesc, "Gulim"); break;
		default: fz_throw(ctx, "invalid sans-serif ros");
		}
	}
}

#endif
