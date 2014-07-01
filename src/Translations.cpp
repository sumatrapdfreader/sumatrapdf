/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "Translations.h"

// set to 0 for not compressed. Must match trans_gen.py (gen_c_code_for_dir).
// Also, in Sumatra, when using compressed, UNINSTALLER_OBJS need to include $(ZLIB_OBJS)
// in makefile.msvc
#define COMPRESSED 0
// set to 0 for not compressed.

#if COMPRESSED == 1

// local copy of uncompr.c
//#define ZLIB_INTERNAL
#include "zlib.h"

int uncompress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen)
{
    z_stream stream;
    int err;

    stream.next_in = (Bytef*)source;
    stream.avail_in = (uInt)sourceLen;
    /* Check for source > 64K on 16-bit machine: */
    if ((uLong)stream.avail_in != sourceLen) return Z_BUF_ERROR;

    stream.next_out = dest;
    stream.avail_out = (uInt)*destLen;
    if ((uLong)stream.avail_out != *destLen) return Z_BUF_ERROR;

    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;

    err = inflateInit(&stream);
    if (err != Z_OK) return err;

    err = inflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        inflateEnd(&stream);
        if (err == Z_NEED_DICT || (err == Z_BUF_ERROR && stream.avail_in == 0))
            return Z_DATA_ERROR;
        return err;
    }
    *destLen = stream.total_out;

    err = inflateEnd(&stream);
    return err;
}
#endif // COMPRESSED == 1

/*
TODO:
 - could use bzip2 for compression for additional ~7k savings
*/

// Note: this code is intentionally optimized for (small) size, not speed

namespace trans {

// defined in Trans*_txt.cpp
extern int              gLangsCount;
extern int              gStringsCount;
extern const char *     gLangNames;
extern const char *     gLangCodes;
const LANGID *          GetLangIds();
bool                    IsLangRtl(int langIdx);
const char **           GetOriginalStrings();

// used locally, gCurrLangCode points into gLangCodes
static const char *     gCurrLangCode = NULL;
static int              gCurrLangIdx = 0;

// Note: we don't have access to STRINGS_COUNT and LANGS_COUNT
// hence foo[] => *foo here
// const char *gCurrLangStrings[STRINGS_COUNT];
const char **           gCurrLangStrings = NULL;
// WCHAR ** gLangsTransCache[LANGS_COUNT];
WCHAR ***               gLangsTransCache = NULL;

#if COMPRESSED == 1
// const char *gLangsStringsUncompressed[LANGS_COUNT];
const unsigned char *   GetTranslationsForLang(int langIdx, uint32_t *uncompressedSizeOut, uint32_t *compressedSizeOut);
const char **           gLangsStringsUncompressed = NULL;
#else
const char *   GetTranslationsForLang(int langIdx);
#endif

/* In general, after adding new _TR() strings, one has to re-generate Translations_txt.cpp, but
that also requires uploading new strings to the server, for which one needs access.

To support adding new strings without re-generating Translatiosn_txt.cpp, we have a concept
of missing translations. */

struct MissingTranslation {
    const char *s;
    const WCHAR *translation;
};

// number of missing translations should be small
static MissingTranslation  gMissingTranslations[64];
static int                 gMissingTranslationsCount = 0;

static void FreeMissingTranslations()
{
    for (int i=0; i < gMissingTranslationsCount; i++) {
        free((void*)gMissingTranslations[i].translation);
    }
    gMissingTranslationsCount = 0;
}

static const WCHAR *FindOrAddMissingTranslation(const char *s)
{
    for (int i = 0; i < gMissingTranslationsCount; i++) {
        if (s == gMissingTranslations[i].s) {
            return gMissingTranslations[i].translation;
        }
    }
    if (gMissingTranslationsCount >= dimof(gMissingTranslations))
        return L"missing translation";

    gMissingTranslations[gMissingTranslationsCount].s = s;
    const WCHAR *res = str::conv::FromUtf8(s);
    gMissingTranslations[gMissingTranslationsCount].translation = res;
    gMissingTranslationsCount++;
    return res;
}

int GetLangsCount()
{
    return gLangsCount;
}

const char *GetCurrentLangCode()
{
    return gCurrLangCode;
}

static WCHAR **GetTransCacheForLang(int langIdx)
{
    if (!gLangsTransCache[langIdx])
        gLangsTransCache[langIdx] = AllocArray<WCHAR *>(gStringsCount);
    return gLangsTransCache[langIdx];
}

static void FreeTransCache()
{
    for (int langIdx = 0; langIdx < gLangsCount; langIdx++) {
        WCHAR **transCache = gLangsTransCache[langIdx];
        for (int i = 0; transCache && i < gStringsCount; i++) {
            free(transCache[i]);
        }
        free(transCache);
#if COMPRESSED == 1
        if (gLangsStringsUncompressed[langIdx])
            free((void*)gLangsStringsUncompressed[langIdx]);
#endif
    }
    free(gLangsTransCache);
    free(gCurrLangStrings);
#if COMPRESSED == 1
    free(gLangsStringsUncompressed);
#endif
}

static void BuildStringsIndexForLang(int langIdx)
{
    if (0 == gCurrLangIdx) {
        const char **origStrings = GetOriginalStrings();
        for (int idx = 0; idx < gStringsCount; idx++) {
            gCurrLangStrings[idx] = origStrings[idx];
            CrashIf(!gCurrLangStrings[idx]);
        }
        return;
    }

#if COMPRESSED == 1
    const char *s =  gLangsStringsUncompressed[langIdx];
    if (NULL == s) {
        uint32_t uncompressedSize, compressedSize;
        const unsigned char *compressed = GetTranslationsForLang(langIdx, &uncompressedSize, &compressedSize);
        void *uncompressed = malloc(uncompressedSize);
        uLongf uncompressedSizeReal;
        int res = uncompress((Bytef*)uncompressed, &uncompressedSizeReal, compressed, compressedSize);
        CrashAlwaysIf(Z_OK != res);
        CrashAlwaysIf(uncompressedSize != uncompressedSizeReal);
        gLangsStringsUncompressed[langIdx] = (const char *)uncompressed;
        s = gLangsStringsUncompressed[langIdx];
    }
#else
    const char *s = GetTranslationsForLang(langIdx);
#endif
    for (int i = 0; i < gStringsCount; i++) {
        if (0 == *s)
            gCurrLangStrings[i] = NULL;
        else
            gCurrLangStrings[i] = s;
        // advance to the next string
        while (*s) {
            ++s;
        }
        ++s;
    }
}

void SetCurrentLangByCode(const char *langCode)
{
    if (!gCurrLangStrings) {
        gCurrLangStrings = AllocArray<const char*>(gStringsCount);
        gLangsTransCache = AllocArray<WCHAR **>(gLangsCount);
#if COMPRESSED == 1
        gLangsStringsUncompressed = AllocArray<const char*>(gLangsCount);
#endif
    }

    if (str::Eq(langCode, gCurrLangCode))
        return;

    int idx = seqstrings::StrToIdx(gLangCodes, langCode);
    CrashIf(-1 == idx);
    gCurrLangIdx = idx;
    gCurrLangCode = GetLangCodeByIdx(idx);
    BuildStringsIndexForLang(gCurrLangIdx);
}

const char *ValidateLangCode(const char *langCode)
{
    int idx = seqstrings::StrToIdx(gLangCodes, langCode);
    if (-1 == idx)
        return NULL;
    return GetLangCodeByIdx(idx);
}

const char *GetLangCodeByIdx(int idx)
{
    return seqstrings::IdxToStr(gLangCodes, idx);
}

const char *GetLangNameByIdx(int idx)
{
    return seqstrings::IdxToStr(gLangNames, idx);
}

bool IsCurrLangRtl()
{
    return IsLangRtl(gCurrLangIdx);
}

const char *DetectUserLang()
{
    const LANGID *langIds = GetLangIds();
    LANGID langId = GetUserDefaultUILanguage();
    // try the exact match
    for (int i = 0; i < gLangsCount; i++) {
        if (langId == langIds[i])
            return GetLangCodeByIdx(i);
    }

    // see if we have a translation in a language that has the same
    // primary id as user's language and neutral sublang
    LANGID userLangIdNeutral = MAKELANGID(PRIMARYLANGID(langId), SUBLANG_NEUTRAL);
    for (int i = 0; i < gLangsCount; i++) {
        if (userLangIdNeutral == langIds[i])
            return GetLangCodeByIdx(i);
    }

    return "en";
}

static int GetEnglishStringIndex(const char* txt)
{
    const char **origStrings = GetOriginalStrings();
    for (int idx = 0; idx < gStringsCount; idx++) {
        const char *s = origStrings[idx];
        if (str::Eq(s, txt))
            return idx;
    }
    return -1;
}

const WCHAR *GetTranslation(const char *s)
{
    if (NULL == gCurrLangCode)
        SetCurrentLangByCode("en");

    int idx = GetEnglishStringIndex(s);
    if (-1 == idx)
        return FindOrAddMissingTranslation(s);

    const char *trans = gCurrLangStrings[idx];
    // fall back to English if the language doesn't have a translations for this string
    if (!trans)
        trans = s;

    WCHAR **transCache = GetTransCacheForLang(gCurrLangIdx);
    if (!transCache[idx])
        transCache[idx] = str::conv::FromUtf8(trans);
    return transCache[idx];
}

void Destroy()
{
    if (!gCurrLangCode) {
        // no need for clean-up if translations were never initialized
        return;
    }

    FreeTransCache();
    FreeMissingTranslations();
}

} // namespace trans
