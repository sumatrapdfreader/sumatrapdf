/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Crypto.h"
#include "base/File.h"
#include "base/Http.h"

#if OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>
#endif

#include "EutlTrust.h"

TempStr GetSumatraDataDirTemp();
#if OS_WIN
void SetEutlLookupFn(bool (*fn)(const u8* der, int derLen));
#endif

constexpr const char* kLotlUrl = "https://ec.europa.eu/tools/lotl/eu-lotl.xml";
constexpr int kMaxTslLists = 40;
constexpr int kMaxXmlBytes = 8 * 1024 * 1024;

static TempStr EutlCachePathTemp() {
    TempStr dir = GetSumatraDataDirTemp();
    if (len(dir) == 0) {
        return {};
    }
    return path::JoinTemp(dir, StrL("eutl-certs.sha256"));
}

static TempStr EutlStampPathTemp() {
    TempStr dir = GetSumatraDataDirTemp();
    if (len(dir) == 0) {
        return {};
    }
    return path::JoinTemp(dir, StrL("eutl-certs.txt"));
}

static void CollectTagContents(Str xml, Str openNeedle, Str closeNeedle, StrVec& out) {
    Str rest = xml;
    while (len(rest) > 0) {
        int open = str::IndexOfI(rest, openNeedle);
        if (open < 0) {
            break;
        }
        Str after = Str(rest.s + open, rest.len - open);
        int gt = str::IndexOfChar(after, '>');
        if (gt < 0) {
            break;
        }
        Str body = Str(after.s + gt + 1, after.len - gt - 1);
        int close = str::IndexOfI(body, closeNeedle);
        if (close < 0) {
            break;
        }
        Str val(body.s, close);
        str::TrimWsBoth(val);
        out.AppendNonEmpty(val);
        rest = Str(body.s + close, body.len - close);
    }
}

#if OS_WIN
static Str DecodeBase64Owned(Str b64) {
    DWORD n = 0;
    if (!CryptStringToBinaryA(CStrTemp(b64), (DWORD)len(b64), CRYPT_STRING_BASE64, nullptr, &n, nullptr, nullptr) ||
        n == 0) {
        return {};
    }
    u8* buf = AllocArray<u8>((int)n);
    if (!CryptStringToBinaryA(CStrTemp(b64), (DWORD)len(b64), CRYPT_STRING_BASE64, buf, &n, nullptr, nullptr)) {
        free(buf);
        return {};
    }
    return Str((char*)buf, (int)n);
}
#else
static Str DecodeBase64Owned(Str) {
    return {};
}
#endif

static void AddCertFingerprint(Str der, StrVec& fps) {
    if (len(der) == 0) {
        return;
    }
    u8 digest[32]{};
    CalcSHA2Digest(der, digest);
    TempStr hex = str::MemToHexTemp(Str((const char*)digest, 32));
    if (hex && fps.Find(hex) < 0) {
        fps.Append(hex);
    }
}

// LOTL TSLLocation lists XML TSLs and PDF copies. Fetch XML only
// (.xml, .xtsl, or names like TSLDK_v6xml).
static bool IsXmlTslUrl(Str url) {
    int cut = str::IndexOfChar(url, '?');
    if (cut < 0) {
        cut = str::IndexOfChar(url, '#');
    }
    if (cut >= 0) {
        url = Str(url.s, cut);
    }
    return str::EndsWithI(url, StrL("xml")) || str::EndsWithI(url, StrL("xtsl"));
}

static bool HttpGetBounded(Str url, str::Builder& out) {
    HttpRsp rsp;
    if (!HttpGet(url, &rsp) || !IsHttpRspOk(&rsp)) {
        return false;
    }
    Str d = ToStr(rsp.data);
    if (len(d) == 0 || len(d) > kMaxXmlBytes) {
        return false;
    }
    out.Reset();
    out.Append(d);
    return true;
}

static void HarvestXml(Str xml, StrVec& fps, StrVec& tslUrls) {
    StrVec certs;
    CollectTagContents(xml, StrL("X509Certificate"), StrL("</"), certs);
    for (Str c : certs) {
        Str der = DecodeBase64Owned(c);
        AddCertFingerprint(der, fps);
        str::Free(der);
    }
    CollectTagContents(xml, StrL("TSLLocation"), StrL("</"), tslUrls);
}

bool EutlCertIsEuTrusted(const u8* der, int derLen) {
    if (!der || derLen <= 0) {
        return false;
    }
    TempStr path = EutlCachePathTemp();
    if (len(path) == 0 || !file::Exists(path)) {
        return false;
    }
    Str cache = file::ReadFile(path);
    if (len(cache) == 0) {
        return false;
    }
    u8 digest[32]{};
    CalcSHA2Digest(Str((const char*)der, derLen), digest);
    TempStr hex = str::MemToHexTemp(Str((const char*)digest, 32));
    bool found = hex && str::ContainsI(cache, hex);
    str::Free(cache);
    return found;
}

bool EutlCacheExists() {
    TempStr path = EutlCachePathTemp();
    return path && file::Exists(path);
}

TempStr EutlCacheInfoTemp() {
    TempStr stamp = EutlStampPathTemp();
    if (len(stamp) == 0 || !file::Exists(stamp)) {
        return StrL("EU trusted list not downloaded");
    }
    Str s = file::ReadFile(stamp);
    TempStr out = str::DupTemp(s);
    str::Free(s);
    return out ? out : StrL("EU trusted list present");
}

bool EutlUpdate(Str* errOut) {
    str::Builder lotl;
    if (!HttpGetBounded(Str(kLotlUrl), lotl)) {
        if (errOut) {
            *errOut = str::Dup(StrL("failed to download the EU LOTL"));
        }
        return false;
    }
    StrVec fps;
    StrVec tslUrls;
    HarvestXml(ToStr(lotl), fps, tslUrls);

    int fetched = 0;
    for (int i = 0; i < len(tslUrls) && fetched < kMaxTslLists; i++) {
        Str url = tslUrls[i];
        if (!str::StartsWithI(url, StrL("http")) || !IsXmlTslUrl(url)) {
            continue;
        }
        str::Builder tsl;
        if (!HttpGetBounded(url, tsl)) {
            logf("EutlUpdate: skip TSL '%s'\n", url);
            continue;
        }
        StrVec ignore;
        HarvestXml(ToStr(tsl), fps, ignore);
        fetched++;
    }

    if (len(fps) == 0) {
        if (errOut) {
            *errOut = str::Dup(StrL("EU LOTL contained no certificates"));
        }
        return false;
    }

    str::Builder cache;
    for (Str fp : fps) {
        cache.Append(fp);
        cache.AppendChar('\n');
    }
    TempStr cachePath = EutlCachePathTemp();
    TempStr stampPath = EutlStampPathTemp();
    if (len(cachePath) == 0 || !dir::CreateForFile(cachePath)) {
        if (errOut) {
            *errOut = str::Dup(StrL("could not write the EUTL cache"));
        }
        return false;
    }
    if (!file::WriteFile(cachePath, ToStr(cache))) {
        if (errOut) {
            *errOut = str::Dup(StrL("could not write the EUTL cache"));
        }
        return false;
    }
    TempStr info = fmt("Updated, %d certificates from %d national lists", len(fps), fetched);
    file::WriteFile(stampPath, info);
    logf("EutlUpdate: %s\n", info);
    return true;
}

static bool EutlLookupThunk(const u8* der, int derLen) {
    return EutlCertIsEuTrusted(der, derLen);
}

void EutlRegisterLookup() {
#if OS_WIN
    SetEutlLookupFn(EutlLookupThunk);
#endif
}
