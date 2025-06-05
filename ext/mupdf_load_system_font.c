// this file is compiled as part of mupdf library and ends up
// in libmupdf.dll, to avoid issues related to crossing .dll boundaries
// It implements loading of Fonts included in windows
#include "mupdf/fitz.h"
#include "mupdf/ucdn.h"
#include "mupdf/pdf.h"

#ifdef _WIN32

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <assert.h>

typedef uint32_t u32;

// TODO: Use more of FreeType for TTF parsing (for performance reasons,
//       the fonts can't be parsed completely, though)
#include <ft2build.h>
#include FT_TRUETYPE_IDS_H
#include FT_TRUETYPE_TAGS_H

#define TTC_VERSION1 0x00010000
#define TTC_VERSION2 0x00020000

#define MAX_FACENAME 256

#define MAX_FONT_FILES 1024
#define MAX_FONTS 4096

typedef struct {
    const char* file_path;
    void* data;
    size_t size;
} font_file;

typedef struct {
    const char* fontface;
    u32 index;
    u32 file_idx;
} win_font_info;

typedef struct {
    win_font_info fontmap[MAX_FONTS];
    int len;
    int cap;
} win_fonts;

typedef struct {
    font_file files[MAX_FONT_FILES];
    int len;
    int cap;
} font_files;

font_files g_font_files;

typedef struct {
    ULONG uVersion;
    USHORT uNumOfTables;
    USHORT uSearchRange;
    USHORT uEntrySelector;
    USHORT uRangeShift;
} TT_OFFSET_TABLE;

typedef struct {
    ULONG uTag;      // table name
    ULONG uCheckSum; // Check sum
    ULONG uOffset;   // Offset from beginning of file
    ULONG uLength;   // length of the table in bytes
} TT_TABLE_DIRECTORY;

typedef struct {
    USHORT uFSelector;     // format selector. Always 0
    USHORT uNRCount;       // Name Records count
    USHORT uStorageOffset; // Offset for strings storage, from start of the table
} TT_NAME_TABLE_HEADER;

typedef struct {
    USHORT uPlatformID;
    USHORT uEncodingID;
    USHORT uLanguageID;
    USHORT uNameID;
    USHORT uStringLength;
    USHORT uStringOffset; // from start of storage area
} TT_NAME_RECORD;

typedef struct {
    ULONG Tag;
    ULONG Version;
    ULONG NumFonts;
} FONT_COLLECTION;

static struct {
    const char* name;
    const char* pattern;
} baseSubstitutes[] = {
    {"Courier", "CourierNewPSMT"},
    {"Courier-Bold", "CourierNewPS-BoldMT"},
    {"Courier-Oblique", "CourierNewPS-ItalicMT"},
    {"Courier-BoldOblique", "CourierNewPS-BoldItalicMT"},
    {"Helvetica", "ArialMT"},
    {"Helvetica-Bold", "Arial-BoldMT"},
    {"Helvetica-Oblique", "Arial-ItalicMT"},
    {"Helvetica-BoldOblique", "Arial-BoldItalicMT"},
    {"Times-Roman", "TimesNewRomanPSMT"},
    {"Times-Bold", "TimesNewRomanPS-BoldMT"},
    {"Times-Italic", "TimesNewRomanPS-ItalicMT"},
    {"Times-BoldItalic", "TimesNewRomanPS-BoldItalicMT"},
    {"Symbol", "SymbolMT"},
};

static win_fonts g_win_fonts;

static int did_init = 0;
static CRITICAL_SECTION cs_fonts;

static int streq(const char* s1, const char* s2) {
    if (strcmp(s1, s2) == 0) {
        return 1;
    }
    return 0;
}

static int streqi(const char* s1, const char* s2) {
    if (_stricmp(s1, s2) == 0) {
        return 1;
    }
    return 0;
}

static inline USHORT BEtoHs(USHORT x) {
    BYTE* data = (BYTE*)&x;
    return (data[0] << 8) | data[1];
}

static inline ULONG BEtoHl(ULONG x) {
    BYTE* data = (BYTE*)&x;
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

/* A little bit more sophisticated name matching so that e.g. "EurostileExtended"
    matches "EurostileExtended-Roman" or "Tahoma-Bold,Bold" matches "Tahoma-Bold" */
static int cmp_font_name(const char* name1, const char* name2) {
    int len1 = strlen(name1);
    int len2 = strlen(name2);

    if (len1 != len2) {
        const char* rest = len1 > len2 ? name1 + len2 : name2 + len1;
        if (',' == *rest || streqi(rest, "-roman"))
            return _strnicmp(name1, name2, fz_mini(len1, len2));
    }

    return _stricmp(name1, name2);
}

static int font_name_eq(const char* name1, const char* name2) {
    return cmp_font_name(name1, name2) == 0;
}

static int cmp_win_font_info(const void* el1, const void* el2) {
    win_font_info* i1 = (win_font_info*)el1;
    win_font_info* i2 = (win_font_info*)el2;
    return cmp_font_name(i1->fontface, i2->fontface);
}

static win_font_info* pdf_find_windows_font_path(const char* fontname) {
    win_font_info* map = &(g_win_fonts.fontmap[0]);
    size_t n = (size_t)g_win_fonts.len;
    size_t elSize = sizeof(win_font_info);
    win_font_info el;
    el.fontface = fontname;
    el.index = 0;
    win_font_info* res = (win_font_info*)bsearch(&el, map, n, elSize, cmp_win_font_info);
    return res;
}

/* source and dest can be same */
static void decode_unicode_BE(fz_context* ctx, char* source, int sourcelen, char* dest, int destlen) {
    WCHAR* tmp;
    int converted, i;

    if (sourcelen % 2 != 0)
        fz_throw(ctx, FZ_ERROR_GENERIC, "fonterror : invalid unicode string");

    tmp = fz_malloc_array(ctx, sourcelen / 2 + 1, WCHAR);
    for (i = 0; i < sourcelen / 2; i++)
        tmp[i] = BEtoHs(((WCHAR*)source)[i]);
    tmp[sourcelen / 2] = '\0';

    converted = WideCharToMultiByte(CP_UTF8, 0, tmp, -1, dest, destlen, NULL, NULL);
    fz_free(ctx, tmp);
    if (!converted)
        fz_throw(ctx, FZ_ERROR_GENERIC, "fonterror : invalid unicode string");
}

static void decode_platform_string(fz_context* ctx, int platform, int enctype, char* source, int sourcelen, char* dest,
                                   int destlen) {
    switch (platform) {
        case TT_PLATFORM_APPLE_UNICODE:
            switch (enctype) {
                case TT_APPLE_ID_DEFAULT:
                case TT_APPLE_ID_UNICODE_2_0:
                    decode_unicode_BE(ctx, source, sourcelen, dest, destlen);
                    return;
            }
            fz_throw(ctx, FZ_ERROR_GENERIC, "fonterror : unsupported encoding (%d/%d)", platform, enctype);
        case TT_PLATFORM_MACINTOSH:
            switch (enctype) {
                case TT_MAC_ID_ROMAN:
                    if (sourcelen + 1 > destlen)
                        fz_throw(ctx, FZ_ERROR_GENERIC, "fonterror : overlong fontname: %s", source);
                    // TODO: Convert to UTF-8 from what encoding?
                    memcpy(dest, source, sourcelen);
                    dest[sourcelen] = 0;
                    return;
            }
            fz_throw(ctx, FZ_ERROR_GENERIC, "fonterror : unsupported encoding (%d/%d)", platform, enctype);
        case TT_PLATFORM_MICROSOFT:
            switch (enctype) {
                case TT_MS_ID_SYMBOL_CS:
                case TT_MS_ID_UNICODE_CS:
                case TT_MS_ID_UCS_4:
                    decode_unicode_BE(ctx, source, sourcelen, dest, destlen);
                    return;
            }
            fz_throw(ctx, FZ_ERROR_GENERIC, "fonterror : unsupported encoding (%d/%d)", platform, enctype);
        default:
            fz_throw(ctx, FZ_ERROR_GENERIC, "fonterror : unsupported encoding (%d/%d)", platform, enctype);
    }
}

// on my machine it's ~21k for facename and path
static int g_font_allocated = 0;

static int get_font_file(const char* file_path) {
    int i;
    font_file* ff;
    for (i = 0; i < g_font_files.len; i++) {
        ff = &(g_font_files.files[i]);
        if (streq(file_path, ff->file_path)) {
            return i;
        }
    }
    return -1;
}
static int get_or_append_font_file(const char* file_path) {
    font_file* ff;
    int i = get_font_file(file_path);
    if (i >= 0) {
        return i;
    }
    if (g_font_files.len >= g_font_files.cap) {
        return -1;
    }
    i = g_font_files.len;
    ff = &g_font_files.files[i];
    g_font_allocated += strlen(file_path) + 1;
    ff->file_path = strdup(file_path);
    ff->data = NULL;
    ff->size = 0;
    g_font_files.len++;
    return i;
}

static void append_mapping(fz_context* ctx, const char* facename, const char* path, int index) {
    win_fonts* fl = &g_win_fonts;
    int file_idx = get_or_append_font_file(path);
    if (file_idx < 0) {
        return;
    }
    if (fl->len >= fl->cap) {
        // fz_throw(ctx, FZ_ERROR_GENERIC, "fonterror : fontlist overflow");
        return;
    }

    win_font_info* i = &fl->fontmap[fl->len];
    g_font_allocated += strlen(facename) + 1;
    // TODO: allocate facename and path from a pool allocator
    i->fontface = strdup(facename);
    i->file_idx = (u32)file_idx;
    i->index = (u32)index;
    fl->len++;
}

static void safe_read(fz_context* ctx, fz_stream* file, int offset, char* buf, int size) {
    int n;
    fz_seek(ctx, file, offset, SEEK_SET);
    n = fz_read(ctx, file, (unsigned char*)buf, size);
    if (n != size)
        fz_throw(ctx, FZ_ERROR_GENERIC, "safe_read: read %d, expected %d", n, size);
}

static void read_ttf_string(fz_context* ctx, fz_stream* file, int offset, TT_NAME_RECORD* ttRecordBE, char* buf,
                            int size) {
    char szTemp[MAX_FACENAME * 2];
    // ignore empty and overlong strings
    int stringLength = BEtoHs(ttRecordBE->uStringLength);
    if (stringLength == 0 || stringLength >= sizeof(szTemp))
        return;

    safe_read(ctx, file, offset + BEtoHs(ttRecordBE->uStringOffset), szTemp, stringLength);
    decode_platform_string(ctx, BEtoHs(ttRecordBE->uPlatformID), BEtoHs(ttRecordBE->uEncodingID), szTemp, stringLength,
                           buf, size);
}

static void remove_spaces(char* srcDest) {
    char* dest;
    for (dest = srcDest; *srcDest; srcDest++) {
        if (*srcDest != ' ') {
            *dest++ = *srcDest;
        }
    }
    *dest = '\0';
}

static void makeFakePSName(char szName[MAX_FACENAME], const char* szStyle) {
    // append the font's subfamily, unless it's a Regular font
    if (*szStyle && !streqi(szStyle, "Regular")) {
        fz_strlcat(szName, "-", MAX_FACENAME);
        fz_strlcat(szName, szStyle, MAX_FACENAME);
    }
    remove_spaces(szName);
}

static void parseTTF(fz_context* ctx, fz_stream* file, int offset, int index, const char* path) {
    TT_OFFSET_TABLE ttOffsetTableBE;
    TT_TABLE_DIRECTORY tblDirBE;
    TT_NAME_TABLE_HEADER ttNTHeaderBE;
    TT_NAME_RECORD ttRecordBE;

    char szPSName[MAX_FACENAME] = {0};
    char szTTName[MAX_FACENAME] = {0};
    char szStyle[MAX_FACENAME] = {0};
    char szCJKName[MAX_FACENAME] = {0};
    int i, count, tblOffset;

    safe_read(ctx, file, offset, (char*)&ttOffsetTableBE, sizeof(TT_OFFSET_TABLE));

    // check if this is a TrueType font of version 1.0 or an OpenType font
    if (BEtoHl(ttOffsetTableBE.uVersion) != TTC_VERSION1 && BEtoHl(ttOffsetTableBE.uVersion) != TTAG_OTTO) {
        fz_throw(ctx, FZ_ERROR_GENERIC, "fonterror : invalid font '%s', invalid version %x", path,
                 BEtoHl(ttOffsetTableBE.uVersion));
    }

    // determine the name table's offset by iterating through the offset table
    count = BEtoHs(ttOffsetTableBE.uNumOfTables);
    for (i = 0; i < count; i++) {
        int entryOffset = offset + sizeof(TT_OFFSET_TABLE) + i * sizeof(TT_TABLE_DIRECTORY);
        safe_read(ctx, file, entryOffset, (char*)&tblDirBE, sizeof(TT_TABLE_DIRECTORY));
        if (!BEtoHl(tblDirBE.uTag) || BEtoHl(tblDirBE.uTag) == TTAG_name) {
            break;
        }
    }
    if (count == i || !BEtoHl(tblDirBE.uTag)) {
        fz_throw(ctx, FZ_ERROR_GENERIC, "fonterror : nameless font");
    }
    tblOffset = BEtoHl(tblDirBE.uOffset);

    // read the 'name' table for record count and offsets
    safe_read(ctx, file, tblOffset, (char*)&ttNTHeaderBE, sizeof(TT_NAME_TABLE_HEADER));
    offset = tblOffset + sizeof(TT_NAME_TABLE_HEADER);
    tblOffset += BEtoHs(ttNTHeaderBE.uStorageOffset);

    // read through the strings for PostScript name and font family
    count = BEtoHs(ttNTHeaderBE.uNRCount);
    for (i = 0; i < count; i++) {
        short langId, nameId;
        BOOL isCJKName;

        safe_read(ctx, file, offset + i * sizeof(TT_NAME_RECORD), (char*)&ttRecordBE, sizeof(TT_NAME_RECORD));

        langId = BEtoHs(ttRecordBE.uLanguageID);
        nameId = BEtoHs(ttRecordBE.uNameID);
        isCJKName = TT_NAME_ID_FONT_FAMILY == nameId && LANG_CHINESE == PRIMARYLANGID(langId);

        // ignore non-English strings (except for Chinese font names)
        if (langId && langId != TT_MS_LANGID_ENGLISH_UNITED_STATES && !isCJKName) {
            continue;
        }
        // ignore names other than font (sub)family and PostScript name
        fz_try(ctx) {
            if (isCJKName) {
                read_ttf_string(ctx, file, tblOffset, &ttRecordBE, szCJKName, sizeof(szCJKName));
            } else if (TT_NAME_ID_FONT_FAMILY == nameId) {
                read_ttf_string(ctx, file, tblOffset, &ttRecordBE, szTTName, sizeof(szTTName));
            } else if (TT_NAME_ID_FONT_SUBFAMILY == nameId) {
                read_ttf_string(ctx, file, tblOffset, &ttRecordBE, szStyle, sizeof(szStyle));
            } else if (TT_NAME_ID_PS_NAME == nameId) {
                read_ttf_string(ctx, file, tblOffset, &ttRecordBE, szPSName, sizeof(szPSName));
            }
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            fz_warn(ctx, "ignoring face name decoding fonterror");
        }
    }

    // try to prevent non-Arial fonts from accidentally substituting Arial
    if (streq(szPSName, "ArialMT")) {
        // cf. https://code.google.com/p/sumatrapdf/issues/detail?id=2471
        if (!streq(szTTName, "Arial")) {
            szPSName[0] = '\0';
        } else if (strstr(path, "caps") || strstr(path, "Caps")) {
            // TODO: is there a better way to distinguish Arial Caps from Arial proper?
            // cf. https://code.google.com/p/sumatrapdf/issues/detail?id=1290
            fz_throw(ctx, FZ_ERROR_GENERIC, "ignore %s, as it can't be distinguished from Arial,Regular", path);
        }
    }

    if (szPSName[0]) {
        append_mapping(ctx, szPSName, path, index);
    }
    if (szTTName[0]) {
        // derive a PostScript-like name and add it, if it's different from the font's
        // included PostScript name; cf. https://code.google.com/p/sumatrapdf/issues/detail?id=376
        // compare the two names before adding this one
        if (!font_name_eq(szTTName, szPSName)) {
            append_mapping(ctx, szTTName, path, index);
        }
    }
    if (szCJKName[0]) {
        makeFakePSName(szCJKName, szStyle);
        if (!font_name_eq(szCJKName, szPSName) && !font_name_eq(szCJKName, szTTName)) {
            append_mapping(ctx, szCJKName, path, index);
        }
    }
}

static void parseTTFs(fz_context* ctx, const char* path) {
    fz_stream* file = 0;
    fz_try(ctx) {
        file = fz_open_file(ctx, path);
        parseTTF(ctx, file, 0, 0, path);
    }
    fz_always(ctx) {
        fz_drop_stream(ctx, file);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
}

static void parseTTCs(fz_context* ctx, const char* path) {
    FONT_COLLECTION fontcollectionBE;
    ULONG i, numFonts, *offsettableBE = NULL;

    fz_stream* file = fz_open_file(ctx, path);

    fz_var(offsettableBE);

    fz_try(ctx) {
        safe_read(ctx, file, 0, (char*)&fontcollectionBE, sizeof(FONT_COLLECTION));
        if (BEtoHl(fontcollectionBE.Tag) != TTAG_ttcf) {
            fz_throw(ctx, FZ_ERROR_GENERIC, "fonterror : wrong format %x", BEtoHl(fontcollectionBE.Tag));
        }
        if (BEtoHl(fontcollectionBE.Version) != TTC_VERSION1 && BEtoHl(fontcollectionBE.Version) != TTC_VERSION2) {
            fz_throw(ctx, FZ_ERROR_GENERIC, "fonterror : invalid version %x", BEtoHl(fontcollectionBE.Version));
        }

        numFonts = BEtoHl(fontcollectionBE.NumFonts);
        offsettableBE = fz_malloc_array(ctx, numFonts, ULONG);

        int offset = (int)sizeof(FONT_COLLECTION);
        safe_read(ctx, file, offset, (char*)offsettableBE, numFonts * sizeof(ULONG));
        for (i = 0; i < numFonts; i++) {
            parseTTF(ctx, file, BEtoHl(offsettableBE[i]), i, path);
        }
    }
    fz_always(ctx) {
        fz_free(ctx, offsettableBE);
        fz_drop_stream(ctx, file);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
}

static void extend_system_font_list(fz_context* ctx, const WCHAR* path) {
    WCHAR szPath[MAX_PATH], *lpFileName;
    WIN32_FIND_DATA FileData;
    HANDLE hList;

    GetFullPathNameW(path, nelem(szPath), szPath, &lpFileName);

    hList = FindFirstFile(szPath, &FileData);
    if (hList == INVALID_HANDLE_VALUE) {
        // Don't complain about missing directories
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            return;
        }
        fz_throw(ctx, FZ_ERROR_GENERIC, "extend_system_font_list: unknown error %d", GetLastError());
    }
    do {
        if (!(FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            char szPathUtf8[MAX_PATH], *fileExt;
            int res;
            lstrcpyn(lpFileName, FileData.cFileName, szPath + MAX_PATH - lpFileName);
            res = WideCharToMultiByte(CP_UTF8, 0, szPath, -1, szPathUtf8, sizeof(szPathUtf8), NULL, NULL);
            if (!res) {
                fz_warn(ctx, "WideCharToMultiByte failed");
                continue;
            }
            fileExt = szPathUtf8 + strlen(szPathUtf8) - 4;
            fz_try(ctx) {
                if (streqi(fileExt, ".ttc")) {
                    parseTTCs(ctx, szPathUtf8);
                } else if (streqi(fileExt, ".ttf") || streqi(fileExt, ".otf")) {
                    parseTTFs(ctx, szPathUtf8);
                }
            }
            fz_catch(ctx) {
                fz_report_error(ctx);
                // ignore errors occurring while parsing a given font file
            }
        }
    } while (FindNextFile(hList, &FileData));
    FindClose(hList);
}

// cf. https://blogs.msdn.com/b/oldnewthing/archive/2004/10/25/247180.aspx
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define CURRENT_HMODULE ((HMODULE) & __ImageBase)

// clang-cl notices the mismatch in function parameters with qsort
// as _stricmp is int _stricmp(const char *string1, const char *string2);
// and qsort expects int (*compar)(const void*,const void*)).
static int stricmp_wrapper(const void* ptr1, const void* ptr2) {
    const char* string1 = (const char*)ptr1;
    const char* string2 = (const char*)ptr2;
    return _stricmp(string1, string2);
}

static void create_system_font_list(fz_context* ctx) {
    WCHAR szFontDir[MAX_PATH];
    UINT cch;

    cch = GetWindowsDirectory(szFontDir, nelem(szFontDir) - 12);
    if (0 < cch && cch < nelem(szFontDir) - 12) {
        wcscat_s(szFontDir, MAX_PATH, L"\\Fonts\\*.?t?");
        extend_system_font_list(ctx, szFontDir);
    }

    if (g_win_fonts.len == 0) {
        fz_warn(ctx, "couldn't find any usable system fonts");
    }

#ifdef NOCJKFONT
    {
        // If no CJK fallback font is builtin but one has been shipped separately (in the same
        // directory as the main executable), add it to the list of loadable system fonts
        WCHAR szFile[MAX_PATH], *lpFileName;
        szFile[0] = '\0';
        GetModuleFileName(CURRENT_HMODULE, szFontDir, MAX_PATH);
        szFontDir[nelem(szFontDir) - 1] = '\0';
        GetFullPathNameW(szFontDir, MAX_PATH, szFile, &lpFileName);
        lstrcpyn(lpFileName, L"DroidSansFallback.ttf", szFile + MAX_PATH - lpFileName);
        extend_system_font_list(ctx, szFile);
    }
#endif

    // sort the font list, so that it can be searched binarily
    void* map = (void*)&(g_win_fonts.fontmap[0]);
    size_t n = (size_t)g_win_fonts.len;
    size_t elSize = sizeof(win_font_info);
    qsort(map, n, elSize, cmp_win_font_info);

#ifdef DEBUG
    // allow to overwrite system fonts for debugging purposes
    // (either pass a full path or a search pattern such as "fonts\*.ttf")
    cch = GetEnvironmentVariable(L"MUPDF_FONTS_PATTERN", szFontDir, nelem(szFontDir));
    if (0 < cch && cch < nelem(szFontDir)) {
        int i, prev_len = g_win_fonts.len;
        extend_system_font_list(ctx, szFontDir);
        for (i = prev_len; i < g_win_fonts.len; i++) {
            win_font_info* entry = bsearch(g_win_fonts.fontmap[i].fontface, g_win_fonts.fontmap, prev_len,
                                           sizeof(win_font_info), cmp_win_font_info);
            if (entry) {
                *entry = g_win_fonts.fontmap[i];
            }
        }
        void* map = (void*)&(g_win_fonts.fontmap[0]);
        size_t n = (size_t)g_win_fonts.len;
        size_t elSize = sizeof(win_font_info);
        qsort(map, n, elSize, cmp_win_font_info);
    }
#endif
}

// TODO(port): replace the caller
static void* fz_resize_array(fz_context* ctx, void* p, unsigned int count, unsigned int size) {
    void* np = fz_realloc(ctx, p, count * size);
    if (!np)
        fz_throw(ctx, FZ_ERROR_GENERIC, "resize array (%d x %d bytes) failed", count, size);
    return np;
}

static fz_buffer* load_and_cache_font(fz_context* ctx, win_font_info* fi, const char* font_name) {
    fz_buffer* buffer = NULL;
    int file_idx = (int)fi->file_idx;
    font_file* ff;

    EnterCriticalSection(&cs_fonts);
    ff = &g_font_files.files[file_idx];
    if (ff->data) {
        buffer = fz_new_buffer_from_shared_data(ctx, ff->data, ff->size);
        fz_warn(ctx, "found cached font '%s' from '%s'", font_name, ff->file_path);
    }
    LeaveCriticalSection(&cs_fonts);
    if (buffer) {
        return buffer;
    }

    // can fz_throw so load outside of cs
    buffer = fz_read_file(ctx, ff->file_path);
    if (!buffer) {
        return NULL;
    }

    EnterCriticalSection(&cs_fonts);
    // TODO: free this data. will have to make a copy using allocator
    // not bound to ctx
    ff->size = fz_buffer_extract(ctx, buffer, (unsigned char**)&ff->data);
    buffer = fz_new_buffer_from_shared_data(ctx, ff->data, ff->size);
    LeaveCriticalSection(&cs_fonts);
    fz_warn(ctx, "loaded font '%s' from '%s'", font_name, ff->file_path);
    return buffer;
}

static int str_ends_with(const char* str, const char* end) {
    size_t len1 = strlen(str);
    size_t len2 = strlen(end);
    return len1 >= len2 && streq(str + len1 - len2, end);
}

static fz_font* load_windows_font_by_name(fz_context* ctx, const char* orig_name) {
    win_font_info* found = NULL;
    char *comma, *fontname;
    fz_font* font;
    fz_buffer* buffer;

    EnterCriticalSection(&cs_fonts);
    if (g_win_fonts.len == 0) {
        fz_try(ctx) {
            create_system_font_list(ctx);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
    }
    LeaveCriticalSection(&cs_fonts);

    if (g_win_fonts.len == 0) {
        fz_throw(ctx, FZ_ERROR_GENERIC, "fonterror: couldn't find any fonts");
    }

    // work on a normalized copy of the font name
    fontname = fz_strdup(ctx, orig_name);
    remove_spaces(fontname);

    // first, try to find the exact font name (including appended style information)
    comma = strchr(fontname, ',');
    if (comma) {
        *comma = '-';
        found = pdf_find_windows_font_path(fontname);
        if (found) {
            goto Exit;
        }
        *comma = ',';
    } else {
        // second, substitute the font name with a known PostScript name
        int i;
        for (i = 0; i < nelem(baseSubstitutes) && !found; i++)
            if (streq(fontname, baseSubstitutes[i].name)) {
                found = pdf_find_windows_font_path(baseSubstitutes[i].pattern);
                if (found) {
                    goto Exit;
                }
            }
    }
    // third, search for the font name without additional style information
    found = pdf_find_windows_font_path(fontname);
    if (found) {
        goto Exit;
    }
    // fourth, try to separate style from basename for prestyled fonts (e.g. "ArialBold")
    if (!comma && (str_ends_with(fontname, "Bold") || str_ends_with(fontname, "Italic"))) {
        int styleLen = str_ends_with(fontname, "Bold") ? 4 : str_ends_with(fontname, "BoldItalic") ? 10 : 6;
        fontname = (char*)fz_resize_array(ctx, fontname, strlen(fontname) + 2, sizeof(char));
        comma = fontname + strlen(fontname) - styleLen;
        memmove(comma + 1, comma, styleLen + 1);
        *comma = '-';
        found = pdf_find_windows_font_path(fontname);
        if (found) {
            goto Exit;
        }
        *comma = ',';
        found = pdf_find_windows_font_path(fontname);
        if (found) {
            goto Exit;
        }
    }
    // fifth, try to convert the font name from the common Chinese codepage 936
    if (fontname[0] < 0) {
        WCHAR cjkNameW[MAX_FACENAME];
        char cjkName[MAX_FACENAME];
        if (MultiByteToWideChar(936, MB_ERR_INVALID_CHARS, fontname, -1, cjkNameW, nelem(cjkNameW)) &&
            WideCharToMultiByte(CP_UTF8, 0, cjkNameW, -1, cjkName, nelem(cjkName), NULL, NULL)) {
            comma = strchr(cjkName, ',');
            if (comma) {
                *comma = '-';
                found = pdf_find_windows_font_path(cjkName);
                if (found) {
                    goto Exit;
                }
                *comma = ',';
            }
            found = pdf_find_windows_font_path(cjkName);
            if (found) {
                goto Exit;
            }
        }
    }
Exit:
    fz_free(ctx, fontname);
    if (!found) {
        fz_throw(ctx, FZ_ERROR_GENERIC, "couldn't find system font '%s'", orig_name);
    }
    buffer = load_and_cache_font(ctx, found, orig_name);
    int use_glyph_bbox = !streq(found->fontface, "DroidSansFallback");
    font = fz_new_font_from_buffer(ctx, orig_name, buffer, found->index, use_glyph_bbox);
    font->flags.ft_substitute = 1;
    return font;
}

static fz_font* load_windows_font(fz_context* ctx, const char* fontname, int bold, int italic,
                                  int needs_exact_metrics) {
    fz_font* font;
    const char* clean_name = pdf_clean_font_name(fontname);
    int is_base_14 = clean_name != fontname;

    /* metrics for Times-Roman don't match those of Windows' Times-Roman */
    /* https://code.google.com/p/sumatrapdf/issues/detail?id=2173 */
    /* https://github.com/sumatrapdfreader/sumatrapdf/issues/2108 */
    /* https://github.com/sumatrapdfreader/sumatrapdf/issues/2028 */
    /* TODO: should this always return NULL if is_base_14 is true? */
    if (is_base_14) {
        if (!strncmp(clean_name, "Times", 5)) {
            return NULL;
        }
        if (!strncmp(clean_name, "Helvetica", 9)) {
            return NULL;
        }
        if (!strncmp(clean_name, "Courier", 7)) {
            return NULL;
        }
    }

    if (needs_exact_metrics) {
        int len;
        if (fz_lookup_base14_font(ctx, fontname, &len))
            return NULL;

        if (clean_name != fontname && !strncmp(clean_name, "Times-", 6))
            return NULL;
    }

    font = load_windows_font_by_name(ctx, fontname);
    /* use the font's own metrics for base 14 fonts */
    if (is_base_14)
        font->flags.ft_substitute = 0;
    return font;
}

static fz_font* load_windows_cjk_font(fz_context* ctx, const char* fontname, int ros, int serif) {
    fz_font* font = NULL;

    /* try to find a matching system font before falling back to an approximate one */
    fz_try(ctx) {
        font = load_windows_font_by_name(ctx, fontname);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    if (font)
        return font;

    /* try to fall back to a reasonable system font */
    fz_try(ctx) {
        if (serif) {
            switch (ros) {
                case FZ_ADOBE_CNS:
                    font = load_windows_font_by_name(ctx, "MingLiU");
                    break;
                case FZ_ADOBE_GB:
                    font = load_windows_font_by_name(ctx, "SimSun");
                    break;
                case FZ_ADOBE_JAPAN:
                    font = load_windows_font_by_name(ctx, "MS-Mincho");
                    break;
                case FZ_ADOBE_KOREA:
                    font = load_windows_font_by_name(ctx, "Batang");
                    break;
                default:
                    fz_throw(ctx, FZ_ERROR_GENERIC, "invalid serif ros");
            }
        } else {
            switch (ros) {
                case FZ_ADOBE_CNS:
                    font = load_windows_font_by_name(ctx, "DFKaiShu-SB-Estd-BF");
                    break;
                case FZ_ADOBE_GB:
                    fz_try(ctx) {
                        font = load_windows_font_by_name(ctx, "KaiTi");
                    }
                    fz_catch(ctx) {
                        font = load_windows_font_by_name(ctx, "KaiTi_GB2312");
                        fz_report_error(ctx);
                    }
                    break;
                case FZ_ADOBE_JAPAN:
                    font = load_windows_font_by_name(ctx, "MS-Gothic");
                    break;
                case FZ_ADOBE_KOREA:
                    font = load_windows_font_by_name(ctx, "Gulim");
                    break;
                default:
                    fz_throw(ctx, FZ_ERROR_GENERIC, "invalid sans-serif ros");
            }
        }
    }
    fz_catch(ctx) {
#ifdef NOCJKFONT
        /* If no CJK fallback font is builtin, maybe one has been shipped separately */
        font = load_windows_font_by_name(ctx, "DroidSansFallback");
#else
        fz_rethrow(ctx);
#endif
    }

    return font;
}
#endif

/*
Segoe UI Emoji Regular
Cambria Math Regular - math symbols
Segoe UI Symbol Regular - math and other symbols
Charis SIL => Times New Roman or Georgia

https://learn.microsoft.com/en-us/windows/apps/design/globalizing/loc-international-fonts
*/
static fz_font* load_windows_fallback_font(fz_context* ctx, int script, int language, int serif, int bold, int italic) {
    fz_font* font = NULL;
    const char* font_name = NULL;

    // TODO: more scripts
    switch (script) {
        case UCDN_SCRIPT_BENGALI: // bangla
        case UCDN_SCRIPT_GURMUKHI:
        case UCDN_SCRIPT_GUJARATI:
        case UCDN_SCRIPT_KANNADA:
        case UCDN_SCRIPT_MALAYALAM:
        case UCDN_SCRIPT_SINHALA:
        case UCDN_SCRIPT_SORA_SOMPENG:
        case UCDN_SCRIPT_OL_CHIKI:
        case UCDN_SCRIPT_ORIYA: // odia
        case UCDN_SCRIPT_TAMIL:
        case UCDN_SCRIPT_TELUGU:
        case UCDN_SCRIPT_DEVANAGARI: {
            font_name = "NirmalaUI";
            if (bold) {
                font_name = "NirmalaUI-Bold";
            }
        } break;
        case UCDN_SCRIPT_HEBREW: {
            font_name = "SegoeUI";
            if (bold) {
                font_name = "SegoeUI-Bold";
            }
        } break;
        case UCDN_SCRIPT_CYRILLIC:
        case UCDN_SCRIPT_GREEK:
        case UCDN_SCRIPT_ARMENIAN:
        case UCDN_SCRIPT_GEORGIAN: {
            font_name = "Sylfaen";
        } break;
        // per chatgpt Times New Roman is closest to Noto Serif
        case UCDN_SCRIPT_LATIN:
        // case UCDN_SCRIPT_GREEK:
        // case UCDN_SCRIPT_CYRILLIC:
        case UCDN_SCRIPT_COMMON:
        case UCDN_SCRIPT_INHERITED:
        case UCDN_SCRIPT_UNKNOWN: {
            font_name = "TimesNewRomanPSMT";
            if (bold) {
                font_name = "TimesNewRomanPS-BoldMT";
                if (italic) {
                    font_name = "TimesNewRomanPS-BoldItalicMT";
                }
            } else if (italic) {
                font_name = "TimesNewRomanPS-ItalicMT";
            }
        } break;
    }

    if (!font_name) {
        fz_warn(ctx, "couldn't find windows system font for script %d, language: %d, bold: %d, italic: %d", script,
                language, (int)bold, (int)italic);
        return NULL;
    }

    /* try to find a matching system font before falling back to an approximate one */
    fz_try(ctx) {
        font = load_windows_font_by_name(ctx, font_name);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    return font;
}

void init_system_font_list(void) {
    // this should always happen on main thread
    if (did_init) {
        return;
    }
    InitializeCriticalSection(&cs_fonts);
    g_win_fonts.len = 0;
    g_win_fonts.cap = MAX_FONTS;
    g_font_files.len = 0;
    g_font_files.cap = MAX_FONT_FILES;
    did_init = 1;
}

void destroy_system_font_list(void) {
    // TODO: free names and file data
    DeleteCriticalSection(&cs_fonts);
}

void install_load_windows_font_funcs(fz_context* ctx) {
    init_system_font_list();
    fz_install_load_system_font_funcs(ctx, load_windows_font, load_windows_cjk_font, load_windows_fallback_font);
}
