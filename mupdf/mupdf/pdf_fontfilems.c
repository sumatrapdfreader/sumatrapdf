#include "fitz.h"
#include "mupdf.h"

/* we use built-in fonts in addition to those installed on windows
   because the metric for Times-Roman in windows fonts seems wrong
   and we end up with over-lapping text if this font is used.
   poppler doesn't have this problem even when using windows fonts
   so maybe there's a better fix. */
#define USE_BUILTIN_FONTS 1

#ifdef USE_BUILTIN_FONTS
extern const unsigned char pdf_font_Dingbats_ttf_buf[];
extern const unsigned int  pdf_font_Dingbats_ttf_len;
extern const unsigned char pdf_font_NimbusMonL_Bold_ttf_buf[];
extern const unsigned int  pdf_font_NimbusMonL_Bold_ttf_len;
extern const unsigned char pdf_font_NimbusMonL_BoldObli_ttf_buf[];
extern const unsigned int  pdf_font_NimbusMonL_BoldObli_ttf_len;
extern const unsigned char pdf_font_NimbusMonL_Regu_ttf_buf[];
extern const unsigned int  pdf_font_NimbusMonL_Regu_ttf_len;
extern const unsigned char pdf_font_NimbusMonL_ReguObli_ttf_buf[];
extern const unsigned int  pdf_font_NimbusMonL_ReguObli_ttf_len;
extern const unsigned char pdf_font_NimbusRomNo9L_Medi_ttf_buf[];
extern const unsigned int  pdf_font_NimbusRomNo9L_Medi_ttf_len;
extern const unsigned char pdf_font_NimbusRomNo9L_MediItal_ttf_buf[];
extern const unsigned int  pdf_font_NimbusRomNo9L_MediItal_ttf_len;
extern const unsigned char pdf_font_NimbusRomNo9L_Regu_ttf_buf[];
extern const unsigned int  pdf_font_NimbusRomNo9L_Regu_ttf_len;
extern const unsigned char pdf_font_NimbusRomNo9L_ReguItal_ttf_buf[];
extern const unsigned int  pdf_font_NimbusRomNo9L_ReguItal_ttf_len;
extern const unsigned char pdf_font_NimbusSanL_Bold_ttf_buf[];
extern const unsigned int  pdf_font_NimbusSanL_Bold_ttf_len;
extern const unsigned char pdf_font_NimbusSanL_BoldItal_ttf_buf[];
extern const unsigned int  pdf_font_NimbusSanL_BoldItal_ttf_len;
extern const unsigned char pdf_font_NimbusSanL_Regu_ttf_buf[];
extern const unsigned int  pdf_font_NimbusSanL_Regu_ttf_len;
extern const unsigned char pdf_font_NimbusSanL_ReguItal_ttf_buf[];
extern const unsigned int  pdf_font_NimbusSanL_ReguItal_ttf_len;
extern const unsigned char pdf_font_StandardSymL_ttf_buf[];
extern const unsigned int  pdf_font_StandardSymL_ttf_len;
extern const unsigned char pdf_font_URWChanceryL_MediItal_ttf_buf[];
extern const unsigned int  pdf_font_URWChanceryL_MediItal_ttf_len;
#endif

#include <windows.h>

/* TODO: make this a function */
#define SAFE_FZ_READ(file, buf, size)\
	err = fz_read(&byteread, (file), (char*)(buf), (size)); \
	if (err) goto cleanup; \
	if (byteread != (size)) err = fz_throw("ioerror");\
	if (err) goto cleanup;

#define ARRAY_SIZE(a) sizeof(a) / sizeof(a[0])

#define SWAPWORD(x)		MAKEWORD(HIBYTE(x), LOBYTE(x))
#define SWAPLONG(x)		MAKELONG(SWAPWORD(HIWORD(x)), SWAPWORD(LOWORD(x)))

#define PLATFORM_UNICODE				0
#define PLATFORM_MACINTOSH				1
#define PLATFORM_ISO					2
#define PLATFORM_MICROSOFT				3

#define UNI_ENC_UNI_1					0
#define UNI_ENC_UNI_1_1					1
#define UNI_ENC_ISO						2
#define UNI_ENC_UNI_2_BMP				3
#define UNI_ENC_UNI_2_FULL_REPERTOIRE	4

#define MAC_ROMAN						0
#define MAC_JAPANESE					1
#define MAC_CHINESE_TRADITIONAL			2
#define MAC_KOREAN						3
#define MAC_CHINESE_SIMPLIFIED			25

#define MS_ENC_SYMBOL					0
#define MS_ENC_UNI_BMP					1
#define MS_ENC_SHIFTJIS					2
#define MS_ENC_PRC						3
#define MS_ENC_BIG5						4
#define MS_ENC_WANSUNG					5
#define MS_ENC_JOHAB					6
#define MS_ENC_UNI_FULL_REPETOIRE		10

#define TTC_VERSION1	0x00010000
#define TTC_VERSION2	0x00020000

typedef struct pdf_fontmapMS_s pdf_fontmapMS;
typedef struct pdf_fontlistMS_s pdf_fontlistMS;

struct pdf_fontmapMS_s
{
	char fontface[128];
	char fontpath[MAX_PATH+1];
	int index;
};

struct pdf_fontlistMS_s
{
	pdf_fontmapMS *fontmap;
	int len;
	int cap;
};

typedef struct _tagTT_OFFSET_TABLE
{
	USHORT	uMajorVersion;
	USHORT	uMinorVersion;
	USHORT	uNumOfTables;
	USHORT	uSearchRange;
	USHORT	uEntrySelector;
	USHORT	uRangeShift;
} TT_OFFSET_TABLE;

typedef struct _tagTT_TABLE_DIRECTORY
{
	char	szTag[4];			//table name
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
	char	Tag[4];
	ULONG	Version;
	ULONG	NumFonts;
}FONT_COLLECTION;

static char *basenames[13] =
{
	"Courier",
	"Courier-Bold",
	"Courier-Oblique",
	"Courier-BoldOblique",
	"Helvetica",
	"Helvetica-Bold",
	"Helvetica-Oblique",
	"Helvetica-BoldOblique",
	"Times-Roman",
	"Times-Bold",
	"Times-Italic",
	"Times-BoldItalic",
	"Symbol",
};

static char *basepatterns[13] =
{
	"CourierNewPSMT",
	"CourierNewPS-BoldMT",
	"CourierNewPS-ItalicMT",
	"CourierNewPS-BoldItalicMT",
	"ArialMT",
	"Arial-BoldMT",
	"Arial-ItalicMT",
	"Arial-BoldItalicMT",
	"TimesNewRomanPSMT",
	"TimesNewRomanPS-BoldMT",
	"TimesNewRomanPS-ItalicMT",
	"TimesNewRomanPS-BoldItalicMT",
	"SymbolMT"
};

static pdf_fontlistMS fontlistMS =
{
	NULL,
	0,
	0,
};

struct pdf_fontmapMS_s defaultSubstitute;

/* copied from pdf_fontfile.c */
enum
{
	FD_FIXED = 1 << 0,
	FD_SERIF = 1 << 1,
	FD_SYMBOLIC = 1 << 2,
	FD_SCRIPT = 1 << 3,
	FD_NONSYMBOLIC = 1 << 5,
	FD_ITALIC = 1 << 6,
	FD_ALLCAP = 1 << 16,
	FD_SMALLCAP = 1 << 17,
	FD_FORCEBOLD = 1 << 18
};

enum { CNS, GB, Japan, Korea };
enum { MINCHO, GOTHIC };

static int streq(const char *s1, const char *s2)
{
	return strcmp(s1,s2) == 0;
}

/* A little bit more sophisticated name matching so that e.g. "EurostileExtended"
   matches "EurostileExtended-Roman" or "Tahoma-Bold,Bold" matches "Tahoma-Bold" */
static int
fontnamematches(const char *s1, const char *s2)
{
	const char *rest;
	int c1, c2;
	while (*s1 && *s2)
	{
		c1 = tolower(*s1++);
		c2 = tolower(*s2++);
		if (c1 != c2)
			return 0;
	}
	if (!*s1 && !*s2)
		return 1;
	rest = s2;
	if (*s1)
		rest = s1;

	if (',' == *rest || 0 == stricmp(rest, "-roman"))
		return 1;
	return 0;
}

static int
lookupcompare(const void *elem1, const void *elem2)
{
	pdf_fontmapMS *val1 = (pdf_fontmapMS *)elem1;
	pdf_fontmapMS *val2 = (pdf_fontmapMS *)elem2;

	if (val1->fontface[0] == 0)
		return 1;
	if (val2->fontface[0] == 0)
		return -1;

	if (fontnamematches(val1->fontface, val2->fontface))
		return 0;

	return stricmp(val1->fontface, val2->fontface);
}

static int
sortcompare(const void *elem1, const void *elem2)
{
	pdf_fontmapMS *val1 = (pdf_fontmapMS *)elem1;
	pdf_fontmapMS *val2 = (pdf_fontmapMS *)elem2;

	if (val1->fontface[0] == 0)
		return 1;
	if (val2->fontface[0] == 0)
		return -1;

	return stricmp(val1->fontface, val2->fontface);
}

static void *
localbsearch (const void *key, const void *base, size_t num,
		 size_t width, int (*compare)(const void *, const void *))
{
	char *lo = (char *)base;
	char *hi = (char *)base + (num - 1) * width;
	char *mid;
	unsigned int half;
	int result;

	while (lo <= hi)
		if (half = num / 2)
		{
			mid = lo + (num & 1 ? half : (half - 1)) * width;
			if (!(result = (*compare)(key,mid)))
				return(mid);
			else if (result < 0)
			{
				hi = mid - width;
				num = num & 1 ? half : half-1;
			}
			else	{
				lo = mid + width;
				num = half;
			}
		}
		else if (num)
			return((*compare)(key,lo) ? 0 : lo);
		else
			break;

		return(0);
}

static void
removeredundancy(pdf_fontlistMS *fl)
{
	int i;
	int roffset = 0;
	int redundancy_count = 0;

	qsort(fl->fontmap, fl->len, sizeof(pdf_fontmapMS), sortcompare);
	for (i = 0; i < fl->len - 1; ++i)
	{
		if (strcmp(fl->fontmap[i].fontface,fl->fontmap[i+1].fontface) == 0)
		{
			fl->fontmap[i].fontface[0] = 0;
			++redundancy_count;
		}
	}
	qsort(fl->fontmap, fl->len, sizeof(pdf_fontmapMS), sortcompare);
	fl->len -= redundancy_count;
#if 0
	for (i = 0; i < fl->len; ++i)
		fprintf(stdout,"%s , %s , %d\n",fl->fontmap[i].fontface,
			fl->fontmap[i].fontpath,fl->fontmap[i].index);
#endif
}

static void
finddefaultsubstitutes(pdf_fontlistMS *fl)
{
	int i;
	for (i = 0; i < fl->len; ++i)
	{
		char *face = fl->fontmap[i].fontface;
#if defined(_MSC_VER) && 0
		OutputDebugStringA(face);
		OutputDebugStringA("\n");
#endif
		if (0 == strcmp(face, "TimesNewRomanPSMT"))
		{
			defaultSubstitute = fl->fontmap[i];
		}
	}
}

static fz_error
swapword(char* pbyte, int nLen)
{
	int i;
	char tmp;
	int nMax;

	if (nLen % 2)
		return fz_throw("fonterror");

	nMax = nLen / 2;
	for (i = 0; i < nLen; ++i) {
		tmp = pbyte[i*2];
		pbyte[i*2] = pbyte[i*2+1];
		pbyte[i*2+1] = tmp;
	}
	return fz_okay;
}

/* pSouce and PDest can be same */
static fz_error
decodeunicodeBMP(char* source, int sourcelen,char* dest, int destlen)
{
	wchar_t tmp[1024*2];
	int converted;
	memset(tmp,0,sizeof(tmp));
	memcpy(tmp,source,sourcelen);
	swapword((char*)tmp,sourcelen);

	converted = WideCharToMultiByte(CP_ACP, 0, tmp,
		-1, dest, destlen, NULL, NULL);

	if (converted == 0)
		return fz_throw("fonterror");

	return fz_okay;
}

static fz_error
decodeunicodeplatform(char* source, int sourcelen,char* dest, int destlen, int enctype)
{
	fz_error err = fz_okay;
	switch(enctype)
	{
	case UNI_ENC_UNI_1:
	case UNI_ENC_UNI_2_BMP:
		err = decodeunicodeBMP(source,sourcelen,dest,destlen);
		break;
	case UNI_ENC_UNI_2_FULL_REPERTOIRE:
	case UNI_ENC_UNI_1_1:
	case UNI_ENC_ISO:
	default:
		err = fz_throw("fonterror : unsupported encoding");
		break;
	}
	return err;
}

static fz_error
decodemacintoshplatform(char* source, int sourcelen,char* dest, int destlen, int enctype)
{
	fz_error err = fz_okay;
	switch(enctype)
	{
	case MAC_ROMAN:
		if (sourcelen + 1 > destlen)
			err = fz_throw("fonterror : short buf lenth");
		else
		{
			memcpy(source,dest,sourcelen);
			dest[sourcelen] = 0;
		}
		break;
	default:
		err = fz_throw("fonterror : unsupported encoding");
		break;
	}
	return err;
}

static fz_error
decodemicrosoftplatform(char* source, int sourcelen,char* dest, int destlen, int enctype)
{
	fz_error err = fz_okay;
	switch(enctype)
	{
	case MS_ENC_SYMBOL:
	case MS_ENC_UNI_BMP:
	case MS_ENC_UNI_FULL_REPETOIRE:
		err = decodeunicodeBMP(source,sourcelen,dest,destlen);
		break;
	default:
		err = fz_throw("fonterror : unsupported encoding");
		break;
	}
	return err;
}

static fz_error
growfontlist(pdf_fontlistMS *fl)
{
	int newcap;
	pdf_fontmapMS *newitems;

	if (fl->cap == 0)
		newcap = 1024;
	else
		newcap = fl->cap * 2;

	newitems = fz_realloc(fl->fontmap, sizeof(pdf_fontmapMS) * newcap);
	if (!newitems)
		return fz_rethrow(-1, "out of memory");;

	memset(newitems + fl->cap, 0,
		sizeof(struct fz_keyval_s) * (newcap - fl->cap));

	fl->fontmap = newitems;
	fl->cap = newcap;

	return fz_okay;
}

static fz_error
insertmapping(pdf_fontlistMS *fl, char *facename, char *path, int index)
{
	fz_error err;

	if (fl->len == fl->cap) {
		err = growfontlist(fl);
		if (err) return err;
	}

	if (fl->len >= fl->cap)
		return fz_throw("fonterror : fontlist overflow");

	strlcpy(fl->fontmap[fl->len].fontface, facename,
		sizeof(fl->fontmap[0].fontface));
	strlcpy(fl->fontmap[fl->len].fontpath, path,
		sizeof(fl->fontmap[0].fontpath));
	fl->fontmap[fl->len].index = index;

	++fl->len;

	return fz_okay;
}

static fz_error
parseTTF(fz_stream *file, int offset, int index, char *path)
{
	fz_error err = fz_okay;
	int byteread;

	TT_OFFSET_TABLE ttOffsetTable;
	TT_TABLE_DIRECTORY tblDir;
	TT_NAME_TABLE_HEADER ttNTHeader;
	TT_NAME_RECORD ttRecord;

	char szTemp[4096];
	int found;
	int i;

	fz_seek(file,offset,0);
	SAFE_FZ_READ(file, &ttOffsetTable, sizeof(TT_OFFSET_TABLE));

	ttOffsetTable.uNumOfTables = SWAPWORD(ttOffsetTable.uNumOfTables);
	ttOffsetTable.uMajorVersion = SWAPWORD(ttOffsetTable.uMajorVersion);
	ttOffsetTable.uMinorVersion = SWAPWORD(ttOffsetTable.uMinorVersion);

	//check is this is a true type font and the version is 1.0
	if (ttOffsetTable.uMajorVersion != 1 || ttOffsetTable.uMinorVersion != 0)
		return fz_throw("fonterror : invalid font version");

	found = 0;

	for (i = 0; i< ttOffsetTable.uNumOfTables; i++)
	{
		SAFE_FZ_READ(file,&tblDir,sizeof(TT_TABLE_DIRECTORY));

		memcpy(szTemp, tblDir.szTag, 4);
		szTemp[4] = 0;

		if (stricmp(szTemp, "name") == 0)
		{
			found = 1;
			tblDir.uLength = SWAPLONG(tblDir.uLength);
			tblDir.uOffset = SWAPLONG(tblDir.uOffset);
			break;
		}
		else if (szTemp[0] == 0)
		{
			break;
		}
	}

	if (!found)
		return err;

	fz_seek(file, tblDir.uOffset,0);

	SAFE_FZ_READ(file,&ttNTHeader,sizeof(TT_NAME_TABLE_HEADER));

	ttNTHeader.uNRCount = SWAPWORD(ttNTHeader.uNRCount);
	ttNTHeader.uStorageOffset = SWAPWORD(ttNTHeader.uStorageOffset);

	offset = tblDir.uOffset + sizeof(TT_NAME_TABLE_HEADER);

	for (i = 0; i < ttNTHeader.uNRCount && err == fz_okay; ++i)
	{
		fz_seek(file, offset + sizeof(TT_NAME_RECORD)*i, 0);
		SAFE_FZ_READ(file,&ttRecord,sizeof(TT_NAME_RECORD));

		ttRecord.uNameID = SWAPWORD(ttRecord.uNameID);
		ttRecord.uLanguageID = SWAPWORD(ttRecord.uLanguageID);

		// Full Name
		if (ttRecord.uNameID == 6)
		{
			ttRecord.uPlatformID = SWAPWORD(ttRecord.uPlatformID);
			ttRecord.uEncodingID = SWAPWORD(ttRecord.uEncodingID);
			ttRecord.uStringLength = SWAPWORD(ttRecord.uStringLength);
			ttRecord.uStringOffset = SWAPWORD(ttRecord.uStringOffset);

			fz_seek(file, tblDir.uOffset + ttRecord.uStringOffset + ttNTHeader.uStorageOffset, 0);
			SAFE_FZ_READ(file, szTemp, ttRecord.uStringLength);

			switch(ttRecord.uPlatformID)
			{
			case PLATFORM_UNICODE:
				err = decodeunicodeplatform(szTemp, ttRecord.uStringLength,
					szTemp, sizeof(szTemp), ttRecord.uEncodingID);
				break;
			case PLATFORM_MACINTOSH:
				err = decodemacintoshplatform(szTemp, ttRecord.uStringLength,
					szTemp, sizeof(szTemp), ttRecord.uEncodingID);
				break;
			case PLATFORM_ISO:
				err = fz_throw("fonterror : unsupported platform");
				break;
			case PLATFORM_MICROSOFT:
				err = decodemicrosoftplatform(szTemp, ttRecord.uStringLength,
					szTemp, sizeof(szTemp), ttRecord.uEncodingID);
				break;
			}

			if (err == fz_okay)
				err = insertmapping(&fontlistMS, szTemp, path, index);
		}
	}

cleanup:
	return err;
}

static fz_error
parseTTFs(char *path)
{
	fz_error err = fz_okay;
	fz_stream *file = nil;

	err = fz_openrfile(&file, path);
	if (err)
		goto cleanup;

	err = parseTTF(file,0,0,path);
	if (err)
		goto cleanup;

cleanup:
	if (file)
		fz_dropstream(file);

	return err;
}

static fz_error
parseTTCs(char *path)
{
	fz_error err = fz_okay;
	int byteread;
	fz_stream *file = nil;
	FONT_COLLECTION fontcollectioin;
	ULONG i;

	err = fz_openrfile(&file, path);
	if (err)
		goto cleanup;

	SAFE_FZ_READ(file, &fontcollectioin, sizeof(FONT_COLLECTION));
	if (memcmp(fontcollectioin.Tag,"ttcf",sizeof(fontcollectioin.Tag)) == 0)
	{
		fontcollectioin.Version = SWAPLONG(fontcollectioin.Version);
		fontcollectioin.NumFonts = SWAPLONG(fontcollectioin.NumFonts);
		if (fontcollectioin.Version == TTC_VERSION1 ||
			fontcollectioin.Version == TTC_VERSION2 )
		{
			ULONG *offsettable = fz_malloc(sizeof(ULONG)*fontcollectioin.NumFonts);
			if (offsettable == nil)
			{
				err = fz_throw("out of memory");
				goto cleanup;
			}

			SAFE_FZ_READ(file, offsettable, sizeof(ULONG)*fontcollectioin.NumFonts);
			for (i = 0; i < fontcollectioin.NumFonts; ++i)
			{
				offsettable[i] = SWAPLONG(offsettable[i]);
				parseTTF(file,offsettable[i],i,path);
			}
			fz_free(offsettable);
		}
		else
		{
			err = fz_throw("fonterror : invalid version");
			goto cleanup;
		}
	}
	else
	{
		err = fz_throw("fonterror: wrong format");
		goto cleanup;
	}


cleanup:
	if (file)
		fz_dropstream(file);

	return err;
}

static fz_error
pdf_createfontlistMS()
{
	char szFontDir[MAX_PATH*2];
	char szSearch[MAX_PATH*2];
	char szFile[MAX_PATH*2];
	BOOL fFinished;
	HANDLE hList;
	WIN32_FIND_DATAA FileData;
	fz_error err;

	if (fontlistMS.len != 0)
		return fz_okay;

	GetWindowsDirectoryA(szFontDir, sizeof(szFontDir));

	// Get the proper directory path
	strcat(szFontDir,"\\Fonts\\");
	sprintf(szSearch,"%s*.tt?",szFontDir);
	// Get the first file
	hList = FindFirstFileA(szSearch, &FileData);
	if (hList == INVALID_HANDLE_VALUE)
	{
		/* Don't complain about missing directories */
		if (errno == ENOENT)
			return fz_throw("fonterror : can't find system fonts dir");
		return fz_throw("ioerror");
	}
	// Traverse through the directory structure
	fFinished = FALSE;
	while (!fFinished)
	{
		if (!(FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			// Get the full path for sub directory
			sprintf(szFile,"%s%s", szFontDir, FileData.cFileName);
			if (szFile[strlen(szFile)-1] == 'c' || szFile[strlen(szFile)-1] == 'C')
			{
				err = parseTTCs(szFile);
				// ignore error parsing a given font file
			}
			else if (szFile[strlen(szFile)-1] == 'f'|| szFile[strlen(szFile)-1] == 'F')
			{
				err = parseTTFs(szFile);
				// ignore error parsing a given font file
			}
		}

		if (!FindNextFileA(hList, &FileData))
		{
			if (GetLastError() == ERROR_NO_MORE_FILES)
			{
				fFinished = TRUE;
			}
		}
	}
	// Let go of find handle
	FindClose(hList);

	removeredundancy(&fontlistMS);
	finddefaultsubstitutes(&fontlistMS);

	return fz_okay;
}

void
pdf_destoryfontlistMS()
{
	if (fontlistMS.fontmap != nil)
		fz_free(fontlistMS.fontmap);

	fontlistMS.len = 0;
	fontlistMS.cap = 0;
}

#ifdef USE_BUILTIN_FONTS
/* based on (and duplicates lots of) pdf_fontfile.c */
static const struct
{
	const char *name;
	const unsigned char *ttf;
	const unsigned int *len;
} basefonts[] = {
	{ "Courier",
	pdf_font_NimbusMonL_Regu_ttf_buf,
	&pdf_font_NimbusMonL_Regu_ttf_len },
	{ "Courier-Bold",
	pdf_font_NimbusMonL_Bold_ttf_buf,
	&pdf_font_NimbusMonL_Bold_ttf_len },
	{ "Courier-Oblique",
	pdf_font_NimbusMonL_ReguObli_ttf_buf,
	&pdf_font_NimbusMonL_ReguObli_ttf_len },
	{ "Courier-BoldOblique",
	pdf_font_NimbusMonL_BoldObli_ttf_buf,
	&pdf_font_NimbusMonL_BoldObli_ttf_len },
	{ "Helvetica",
	pdf_font_NimbusSanL_Regu_ttf_buf,
	&pdf_font_NimbusSanL_Regu_ttf_len },
	{ "Helvetica-Bold",
	pdf_font_NimbusSanL_Bold_ttf_buf,
	&pdf_font_NimbusSanL_Bold_ttf_len },
	{ "Helvetica-Oblique",
	pdf_font_NimbusSanL_ReguItal_ttf_buf,
	&pdf_font_NimbusSanL_ReguItal_ttf_len },
	{ "Helvetica-BoldOblique",
	pdf_font_NimbusSanL_BoldItal_ttf_buf,
	&pdf_font_NimbusSanL_BoldItal_ttf_len },
	{ "Times-Roman",
	pdf_font_NimbusRomNo9L_Regu_ttf_buf,
	&pdf_font_NimbusRomNo9L_Regu_ttf_len },
	{ "Times-Bold",
	pdf_font_NimbusRomNo9L_Medi_ttf_buf,
	&pdf_font_NimbusRomNo9L_Medi_ttf_len },
	{ "Times-Italic",
	pdf_font_NimbusRomNo9L_ReguItal_ttf_buf,
	&pdf_font_NimbusRomNo9L_ReguItal_ttf_len },
	{ "Times-BoldItalic",
	pdf_font_NimbusRomNo9L_MediItal_ttf_buf,
	&pdf_font_NimbusRomNo9L_MediItal_ttf_len },
	{ "Symbol",
	pdf_font_StandardSymL_ttf_buf,
	&pdf_font_StandardSymL_ttf_len },
	{ "ZapfDingbats",
	pdf_font_Dingbats_ttf_buf,
	&pdf_font_Dingbats_ttf_len },
	{ "Chancery",
	pdf_font_URWChanceryL_MediItal_ttf_buf,
	&pdf_font_URWChanceryL_MediItal_ttf_len },
    { 0, 0, 0 }
};

/* TODO: this is copy of pdf_loadbuiltinfont() from pdf_fontfile.c */
fz_error
pdf_loadbuiltinfont2(pdf_fontdesc *font, char *fontname)
{
	fz_error error;
	unsigned char *data;
	unsigned int len;
	int i;

	for (i = 0; basefonts[i].name; i++)
		if (!strcmp(fontname, basefonts[i].name))
			goto found;

	return fz_throw("cannot find font: %s", fontname);

found:
	pdf_logfont("load builtin font %s\n", fontname);

	data = (unsigned char *) basefonts[i].ttf;
	len = *basefonts[i].len;

	error = fz_newfontfrombuffer(&font->font, data, len, 0);
	if (error)
		return fz_rethrow(error, "cannot load freetype font from buffer");

	return fz_okay;
}
#endif

static fz_error
lookupwindowsfont(char *fontname, char **fontpath, int *index)
{
	pdf_fontmapMS fontmap;
	pdf_fontmapMS *found;
	char *pattern;
	int i;

	pdf_createfontlistMS();
	if (fontlistMS.len == 0)
		return fz_throw("fonterror : no fonts in the system");

	pattern = fontname;
	for (i = 0; i < ARRAY_SIZE(basenames); i++)
	{
		if (0 == strcmp(fontname, basenames[i]))
		{
			pattern = basepatterns[i];
			break;
		}
	}

	strlcpy(fontmap.fontface,pattern, sizeof(fontmap.fontface));
	found = localbsearch(&fontmap, fontlistMS.fontmap, fontlistMS.len, sizeof(pdf_fontmapMS), lookupcompare);

#if 0
	if (!found)
		found = findlinear(&fontlistMS, &fontmap);
#endif

	if (found)
	{
		*fontpath = found->fontpath;
		*index = found->index;
		return fz_okay;
	}
	return 1;
}

static fz_error
lookupcidfont(pdf_fontdesc *font, int csi, int kind, char **fontpath, int *index)
{
	if (MINCHO == kind) {
		switch (csi) {
			case CNS:
				return lookupwindowsfont("MingLiU", fontpath, index);
			case GB:
				return lookupwindowsfont("SimSun", fontpath, index);
			case Japan:
				return lookupwindowsfont("MS-Mincho", fontpath, index);
			case Korea:
				return lookupwindowsfont("Batang", fontpath, index);
		}
	}

	if (GOTHIC == kind) {
		switch (csi) {
			case CNS:
				return lookupwindowsfont("DFKaiShu-SB-Estd-BF", fontpath, index);
			case GB:
				if (fz_okay == lookupwindowsfont("KaiTi", fontpath, index))
					return fz_okay;
				return lookupwindowsfont("KaiTi_GB2312", fontpath, index);
			case Japan:
				return lookupwindowsfont("MS-Gothic", fontpath, index);
			case Korea:
				return lookupwindowsfont("Gulim", fontpath, index);
		}
	}

	return fz_throw("Unknown cid kind %d", kind);
}

#if 0
/* TODO: those rules conflict with lookupsubstitutefont() logic. */
static fz_error
lookupjapansubstitute(pdf_fontdesc *font, char *fontname, char **fontpath, int *index)
{
	if (streq(fontname, "GothicBBB-Medium"))
		return lookupwindowsfont("MS-Gothic", fontpath, index);

	if (streq(fontname, "Ryumin-Light"))
		return lookupwindowsfont("MS-Mincho", fontpath, index);

	if (font->flags & FD_FIXED)
	{
		if (font->flags & FD_SERIF)
			return lookupwindowsfont("MS-Mincho", fontpath, index);

		return lookupwindowsfont("MS-Gothic", fontpath, index);
	}

	if (font->flags & FD_SERIF)
			return lookupwindowsfont("MS-PMincho", fontpath, index);

	return lookupwindowsfont("MS-PGothic", fontpath, index);
}
#endif

/* based on pdf_loadsystemfont() from pdf_fontfile.c */
static fz_error
lookupsystemsubstitutefont(pdf_fontdesc *font, char *fontname, char *collection, char **fontpath, int *index)
{
	int isbold = 0;
	int isitalic = 0;
	int isserif = 0;
	int isscript = 0;
	int isfixed = 0;

	if (strstr(fontname, "Bold"))
		isbold = 1;
	if (strstr(fontname, "Italic"))
		isitalic = 1;
	if (strstr(fontname, "Oblique"))
		isitalic = 1;

	if (font->flags & FD_FIXED)
		isfixed = 1;
	if (font->flags & FD_SERIF)
		isserif = 1;
	if (font->flags & FD_ITALIC)
		isitalic = 1;
	if (font->flags & FD_SCRIPT)
		isscript = 1;
	if (font->flags & FD_FORCEBOLD)
		isbold = 1;

	pdf_logfont("fixed-%d serif-%d italic-%d script-%d bold-%d\n",
			isfixed, isserif, isitalic, isscript, isbold);	
	
	if (collection)
	{
		int kind;

		if (isserif)
			kind = MINCHO;
		else
			kind = GOTHIC;

		if (streq(collection, "Adobe-CNS1"))
			return lookupcidfont(font, CNS, kind, fontpath, index);
		else if (streq(collection, "Adobe-GB1"))
			return lookupcidfont(font, GB, kind, fontpath, index);
		else if (streq(collection, "Adobe-Japan1"))
			return lookupcidfont(font, Japan, kind, fontpath, index);
		else if (streq(collection, "Adobe-Japan2"))
			return lookupcidfont(font, Japan, kind, fontpath, index);
		else if (streq(collection, "Adobe-Korea1"))
			return lookupcidfont(font, Korea, kind, fontpath, index);

		fz_warn("unknown cid collection: %s", collection);
	}

	return fz_throw("no system substitute font");
}

/* based on pdf_loadsystemfont() from pdf_fontfile.c */
static fz_error
loadbuiltinsubstitutefont(pdf_fontdesc *font, char *fontname)
{
	fz_error error;
	char *name;

	int isbold = 0;
	int isitalic = 0;
	int isserif = 0;
	int isscript = 0;
	int isfixed = 0;

	if (strstr(fontname, "Bold"))
		isbold = 1;
	if (strstr(fontname, "Italic"))
		isitalic = 1;
	if (strstr(fontname, "Oblique"))
		isitalic = 1;

	if (font->flags & FD_FIXED)
		isfixed = 1;
	if (font->flags & FD_SERIF)
		isserif = 1;
	if (font->flags & FD_ITALIC)
		isitalic = 1;
	if (font->flags & FD_SCRIPT)
		isscript = 1;
	if (font->flags & FD_FORCEBOLD)
		isbold = 1;

	pdf_logfont("fixed-%d serif-%d italic-%d script-%d bold-%d\n",
			isfixed, isserif, isitalic, isscript, isbold);	

	if (isscript)
		name = "Chancery";

	else if (isfixed)
	{
		if (isitalic) {
			if (isbold) name = "Courier-BoldOblique";
			else name = "Courier-Oblique";
		}
		else {
			if (isbold) name = "Courier-Bold";
			else name = "Courier";
		}
	}

	else if (isserif)
	{
		if (isitalic) {
			if (isbold) name = "Times-BoldItalic";
			else name = "Times-Italic";
		}
		else {
			if (isbold) name = "Times-Bold";
			else name = "Times-Roman";
		}
	}

	else
	{
		if (isitalic) {
			if (isbold) name = "Helvetica-BoldOblique";
			else name = "Helvetica-Oblique";
		}
		else {
			if (isbold) name = "Helvetica-Bold";
			else name = "Helvetica";
		}
	}

	error = pdf_loadbuiltinfont(font, name);
	if (error)
		return fz_throw("cannot load builtin substitute font: %s", name);

	/* it's a substitute font: override the metrics */
	font->font->ftsubstitute = 1;

	return fz_okay;
}

static fz_error loadsubstitutefont(pdf_fontdesc *font, char *fontname, char *collection)
{
	fz_error error;
	char *file;
	int index;

	error = lookupsystemsubstitutefont(font, fontname, collection, &file, &index);
	if (fz_okay == error) {
		error = fz_newfontfromfile(&font->font, file, index);
		if (error)
			return fz_rethrow(error, "cannot load freetype font from a file %s", file);
		font->font->ftsubstitute = 1;
		return fz_okay;
	}

	return loadbuiltinsubstitutefont(font, fontname);
}

static fz_error loadwindowsfont(pdf_fontdesc *font, char *fontname)
{
	fz_error error;
	char *file;
	int index;

	error = lookupwindowsfont(fontname, &file, &index);
	if (error)
		return error;

	error = fz_newfontfromfile(&font->font, file, index);
	if (error)
		return fz_rethrow(error, "cannot load freetype font from a file %s", file);
	return fz_okay;
}

fz_error
pdf_loadbuiltinfont(pdf_fontdesc *font, char *basefont)
{
#ifdef USE_BUILTIN_FONTS
	fz_error error = pdf_loadbuiltinfont2(font, basefont);
	if (fz_okay == error)
		return fz_okay;
#endif

	return loadwindowsfont(font, basefont);
}

fz_error
pdf_loadsystemfont(pdf_fontdesc *font, char *basefont, char *collection)
{
	fz_error error;
	error = loadwindowsfont(font, basefont);
	if (fz_okay == error)
		return fz_okay;

	error = loadsubstitutefont(font, basefont, collection);
	if (error)
		return fz_throw("cannot load system font %s from collection %s", basefont, collection);
	return fz_okay;
}

/* TODO: copied from pdf_fontfile.c */
fz_error
pdf_loadembeddedfont(pdf_fontdesc *font, pdf_xref *xref, fz_obj *stmref)
{
	fz_error error;
	fz_buffer *buf;

	pdf_logfont("load embedded font\n");

	error = pdf_loadstream(&buf, xref, fz_tonum(stmref), fz_togen(stmref));
	if (error)
		return fz_rethrow(error, "cannot load font stream");

	error = fz_newfontfrombuffer(&font->font, buf->rp, buf->wp - buf->rp, 0);
	if (error)
	{
		fz_dropbuffer(buf);
		return fz_rethrow(error, "cannot load embedded font (%d %d R)", fz_tonum(stmref), fz_togen(stmref));
	}

	font->buffer = buf->rp; /* save the buffer so we can free it later */
	fz_free(buf); /* only free the fz_buffer struct, not the contained data */

	font->isembedded = 1;

	return fz_okay;
}

