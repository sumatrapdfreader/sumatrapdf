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

#include "Permissions.h"
#include "SumatraConfig.h"
#include "EutlTrust.h"

TempStr GetAppDataDirTemp();
bool HasPermission(Perm);
void SetEutlLookup(const EutlLookup* lookup);

constexpr const char* kLotlUrl = "https://ec.europa.eu/tools/lotl/eu-lotl.xml";
constexpr int kMaxTslLists = 64;
constexpr int kMaxXmlBytes = 64 * 1024 * 1024;
constexpr i64 kUpdateAfterSecs = 7 * 24 * 60 * 60;

// ---- parsing the Trust Service Lists (ETSI TS 119 612) ---------------------

// next element called local, whatever its namespace prefix. Doesn't handle an
// element nested in one of the same name, which these lists don't have.
static bool XmlNextElement(Str& rest, Str local, Str& body) {
    const char* s = rest.s;
    int n = len(rest);
    for (int i = 0; i + 1 < n; i++) {
        if (s[i] != '<' || s[i + 1] == '/' || s[i + 1] == '?' || s[i + 1] == '!') {
            continue;
        }
        int nameStart = i + 1;
        int nameEnd = nameStart;
        while (nameEnd < n && s[nameEnd] != '>' && s[nameEnd] != '/' && s[nameEnd] != ' ' && s[nameEnd] != '\t' &&
               s[nameEnd] != '\r' && s[nameEnd] != '\n') {
            nameEnd++;
        }
        int localStart = nameStart;
        for (int k = nameStart; k < nameEnd; k++) {
            if (s[k] == ':') {
                localStart = k + 1;
            }
        }
        if (!str::Eq(Str(s + localStart, nameEnd - localStart), local)) {
            continue;
        }
        int gt = nameEnd;
        while (gt < n && s[gt] != '>') {
            gt++;
        }
        if (gt >= n) {
            return false;
        }
        if (s[gt - 1] == '/') {
            body = Str(s + gt, 0);
            rest = Str(s + gt + 1, n - gt - 1);
            return true;
        }
        for (int k = gt + 1; k + 2 < n; k++) {
            if (s[k] != '<' || s[k + 1] != '/') {
                continue;
            }
            int closeEnd = k + 2;
            while (closeEnd < n && s[closeEnd] != '>') {
                closeEnd++;
            }
            int closeLocal = k + 2;
            for (int m = k + 2; m < closeEnd; m++) {
                if (s[m] == ':') {
                    closeLocal = m + 1;
                }
            }
            if (closeEnd < n && str::Eq(Str(s + closeLocal, closeEnd - closeLocal), local)) {
                body = Str(s + gt + 1, k - gt - 1);
                rest = Str(s + closeEnd + 1, n - closeEnd - 1);
                return true;
            }
        }
        return false;
    }
    return false;
}

static Str XmlFirstText(Str xml, Str local) {
    Str body;
    if (!XmlNextElement(xml, local, body)) {
        return {};
    }
    str::TrimWsBoth(body);
    return body;
}

// days from 1970-01-01 of a proleptic Gregorian date
static i64 DaysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    i64 era = (y >= 0 ? y : y - 399) / 400;
    int yoe = (int)(y - era * 400);
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

// 2016-06-30T22:00:00Z
i64 EutlParseXmlTime(Str s) {
    TempStr z = str::DupTemp(s);
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, sec = 0;
    if (sscanf(z.s, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &sec) != 6) {
        return 0;
    }
    return DaysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + sec;
}

static bool IsGrantedStatus(Str status) {
    // since eIDAS only "granted"; the rest are the names from before it
    static const char* kGranted[] = {"/Svcstatus/granted", "/Svcstatus/undersupervision", "/Svcstatus/accredited",
                                     "/Svcstatus/supervisionincessation", "/Svcstatus/setbynationallaw"};
    for (const char* g : kGranted) {
        if (str::EndsWithI(status, Str(g))) {
            return true;
        }
    }
    return false;
}

struct ServiceInstance {
    bool wanted = false;
    bool isTsa = false;
    bool granted = false;
    bool eSig = false;
    bool eSeal = false;
    i64 start = 0;
};

// ServiceInformation or ServiceHistoryInstance: the same fields
static ServiceInstance ParseServiceInstance(Str xml) {
    ServiceInstance res;
    Str type = XmlFirstText(xml, StrL("ServiceTypeIdentifier"));
    bool isCaQc = str::EndsWithI(type, StrL("/Svctype/CA/QC"));
    res.isTsa = str::EndsWithI(type, StrL("/Svctype/TSA/QTST"));
    res.wanted = isCaQc || res.isTsa;
    res.granted = IsGrantedStatus(XmlFirstText(xml, StrL("ServiceStatus")));
    res.start = EutlParseXmlTime(XmlFirstText(xml, StrL("StatusStartingTime")));
    // no such marker predates them: then it covers everything
    bool sig = str::ContainsI(xml, StrL("SvcInfoExt/ForeSignatures"));
    bool seal = str::ContainsI(xml, StrL("SvcInfoExt/ForeSeals"));
    bool web = str::ContainsI(xml, StrL("SvcInfoExt/ForWebSiteAuthentication"));
    bool anyMarker = sig || seal || web;
    res.eSig = sig || !anyMarker;
    res.eSeal = seal || !anyMarker;
    return res;
}

void EutlParseTsl(Str xml, Vec<EutlParsedPeriod>& out, StrVec* tslUrls) {
    Str rest = xml;
    Str service;
    while (XmlNextElement(rest, StrL("TSPService"), service)) {
        Str infoBody;
        Str scan = service;
        if (!XmlNextElement(scan, StrL("ServiceInformation"), infoBody)) {
            continue;
        }
        // views into xml, which outlives what we return
        Vec<Str> certs;
        Str identity;
        Str identityScan = infoBody;
        if (XmlNextElement(identityScan, StrL("ServiceDigitalIdentity"), identity)) {
            Str cert;
            while (XmlNextElement(identity, StrL("X509Certificate"), cert)) {
                str::TrimWsBoth(cert);
                if (len(cert) > 0) {
                    VecAppend(certs, cert);
                }
            }
        }

        // newest first, so each instance ends when the one before it started
        Vec<ServiceInstance> instances;
        VecAppend(instances, ParseServiceInstance(infoBody));
        Str history;
        Str historyScan = service;
        if (XmlNextElement(historyScan, StrL("ServiceHistory"), history)) {
            Str inst;
            while (XmlNextElement(history, StrL("ServiceHistoryInstance"), inst)) {
                VecAppend(instances, ParseServiceInstance(inst));
            }
        }
        i64 end = 0;
        for (const ServiceInstance& inst : instances) {
            if (inst.wanted) {
                for (Str c : certs) {
                    EutlParsedPeriod p;
                    p.certB64 = c;
                    p.isTsa = inst.isTsa;
                    p.start = inst.start;
                    p.end = end;
                    p.granted = inst.granted;
                    p.eSig = inst.eSig;
                    p.eSeal = inst.eSeal;
                    VecAppend(out, p);
                }
            }
            end = inst.start;
        }
    }
    if (!tslUrls) {
        return;
    }
    Str locRest = xml;
    Str loc;
    while (XmlNextElement(locRest, StrL("TSLLocation"), loc)) {
        str::TrimWsBoth(loc);
        tslUrls->AppendNonEmpty(loc);
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

static TempStr FingerprintHexTemp(Str der) {
    u8 digest[32]{};
    CalcSHA2Digest(der, digest);
    return str::MemToHexTemp(Str((const char*)digest, 32));
}

static TempStr EutlCachePathTemp() {
    TempStr dir = GetAppDataDirTemp();
    if (len(dir) == 0) {
        return {};
    }
    return path::JoinTemp(dir, StrL("eutl-services.txt"));
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

// ---- the downloaded list ---------------------------------------------------

struct ListPeriod {
    Str hash;
    EutlKind kind = EutlKind::CA;
    i64 start = 0;
    i64 end = 0;
    bool eSig = false;
    bool eSeal = false;
};

struct EutlList {
    Str text;
    Str date;
    StrVec listed;
    Vec<ListPeriod> periods;
#if OS_WIN
    HCERTSTORE store = nullptr;
#endif
};

static void* EutlListOpen() {
    TempStr path = EutlCachePathTemp();
    if (len(path) == 0 || !file::Exists(path)) {
        return nullptr;
    }
    Str text = file::ReadFile(path);
    if (len(text) == 0) {
        return nullptr;
    }
    auto* list = new EutlList;
    list->text = text;
#if OS_WIN
    list->store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, nullptr);
#endif
    StrVec lines;
    Split(&lines, text, StrL("\n"), true);
    for (Str line : lines) {
        StrVec f;
        Split(&f, line, StrL(" "), true);
        if (len(f) >= 5 && str::Eq(f[0], StrL("#"))) {
            list->date = str::Dup(f[4]);
        } else if (len(f) == 3 && str::Eq(f[0], StrL("C"))) {
            list->listed.Append(f[1]);
#if OS_WIN
            Str der = DecodeBase64Owned(f[2]);
            if (list->store && len(der) > 0) {
                CertAddEncodedCertificateToStore(list->store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                                 (const BYTE*)der.s, (DWORD)len(der), CERT_STORE_ADD_ALWAYS, nullptr);
            }
            str::Free(der);
#endif
        } else if (len(f) == 7 && str::Eq(f[0], StrL("S"))) {
            ListPeriod p;
            p.hash = str::Dup(f[1]);
            p.kind = str::Eq(f[2], StrL("TSA")) ? EutlKind::TSA : EutlKind::CA;
            p.start = ParseInt(f[3]);
            p.end = ParseInt(f[4]);
            p.eSig = str::Eq(f[5], StrL("1"));
            p.eSeal = str::Eq(f[6], StrL("1"));
            VecAppend(list->periods, p);
        }
    }
    return list;
}

static void EutlListClose(void* listPtr) {
    auto* list = (EutlList*)listPtr;
    if (!list) {
        return;
    }
#if OS_WIN
    if (list->store) {
        CertCloseStore(list->store, 0);
    }
#endif
    for (ListPeriod& p : list->periods) {
        str::Free(p.hash);
    }
    str::Free(list->date);
    str::Free(list->text);
    delete list;
}

static void* EutlListStore(void* listPtr) {
#if OS_WIN
    return ((EutlList*)listPtr)->store;
#else
    (void)listPtr;
    return nullptr;
#endif
}

static int EutlListStatus(void* listPtr, const u8* der, int derLen, i64 when, EutlKind kind) {
    auto* list = (EutlList*)listPtr;
    TempStr hex = FingerprintHexTemp(Str((const char*)der, derLen));
    if (len(hex) == 0 || list->listed.Find(hex) < 0) {
        return 0;
    }
    int res = kEutlListed;
    for (const ListPeriod& p : list->periods) {
        if (p.kind != kind || !str::Eq(p.hash, hex) || when < p.start || (p.end != 0 && when >= p.end)) {
            continue;
        }
        res |= (p.eSig ? kEutlESig : 0) | (p.eSeal ? kEutlESeal : 0);
    }
    return res;
}

static Str EutlListDate(void* listPtr) {
    return ((EutlList*)listPtr)->date;
}

bool EutlCertIsListed(const u8* der, int derLen) {
    if (!der || derLen <= 0) {
        return false;
    }
    void* list = EutlListOpen();
    if (!list) {
        return false;
    }
    TempStr hex = FingerprintHexTemp(Str((const char*)der, derLen));
    bool found = hex && ((EutlList*)list)->listed.Find(hex) >= 0;
    EutlListClose(list);
    return found;
}

bool EutlCacheExists() {
    TempStr path = EutlCachePathTemp();
    return path && file::Exists(path);
}

// downloads by itself, from the EU's server only, when a signature needs it
bool EutlNeedsUpdate() {
    if (!HasPermission(Perm::InternetAccess) || gForTesting) {
        return false;
    }
    TempStr path = EutlCachePathTemp();
    if (len(path) == 0 || !file::Exists(path)) {
        return true;
    }
#if OS_WIN
    FILETIME ft = file::GetModificationTime(path);
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    const u64 kEpochDiffSecs = 11644473600ULL;
    i64 modified = (i64)(u.QuadPart / 10000000ULL - kEpochDiffSecs);
    return time(nullptr) - modified > kUpdateAfterSecs;
#else
    return false;
#endif
}

TempStr EutlCacheInfoTemp() {
    void* list = EutlListOpen();
    if (!list) {
        return StrL("EU trusted list not downloaded");
    }
    auto* l = (EutlList*)list;
    TempStr res = fmt("EU trusted list from %s: %d certificates", l->date, len(l->listed));
    EutlListClose(list);
    return res;
}

// TLS is what vouches for these downloads: we don't verify the lists' own XML
// signatures, so a national list that isn't on https is left out.
bool EutlUpdate(Str* errOut) {
    auto fail = [errOut](Str msg) {
        if (errOut) {
            *errOut = str::Dup(msg);
        }
        return false;
    };
    str::Builder lotl;
    if (!HttpGetBounded(Str(kLotlUrl), lotl)) {
        return fail(StrL("failed to download the EU LOTL"));
    }
    Vec<EutlParsedPeriod> periods;
    StrVec tslUrls;
    EutlParseTsl(ToStr(lotl), periods, &tslUrls);

    Vec<str::Builder*> lists;
    defer {
        for (str::Builder* b : lists) {
            delete b;
        }
    };
    int fetched = 0;
    for (int i = 0; i < len(tslUrls) && fetched < kMaxTslLists; i++) {
        Str url = tslUrls[i];
        if (!str::StartsWithI(url, StrL("https://")) || !IsXmlTslUrl(url)) {
            logf("EutlUpdate: skip TSL '%s'\n", url);
            continue;
        }
        auto* tsl = new str::Builder();
        VecAppend(lists, tsl);
        if (!HttpGetBounded(url, *tsl)) {
            logf("EutlUpdate: failed to download TSL '%s'\n", url);
            continue;
        }
        EutlParseTsl(ToStr(*tsl), periods, nullptr);
        fetched++;
    }
    if (len(periods) == 0) {
        return fail(StrL("the EU lists contained no qualified services"));
    }

    str::Builder cache;
    StrVec seen;
    time_t now = time(nullptr);
    char date[32];
    strftime(date, sizeof(date), "%Y-%m-%d", gmtime(&now));
    cache.Append(fmt("# sumatrapdf eutl 2 %s\n", Str(date)));
    int nCerts = 0;
    for (const EutlParsedPeriod& p : periods) {
        Str der = DecodeBase64Owned(p.certB64);
        if (len(der) == 0) {
            continue;
        }
        TempStr hex = FingerprintHexTemp(der);
        str::Free(der);
        if (seen.Find(hex) < 0) {
            seen.Append(hex);
            // a line per certificate: base64 may be wrapped in the XML
            str::Builder b64;
            for (int i = 0; i < len(p.certB64); i++) {
                char c = p.certB64.s[i];
                if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                    b64.AppendChar(c);
                }
            }
            cache.Append(fmt("C %s %s\n", hex, ToStr(b64)));
            nCerts++;
        }
        if (p.granted) {
            cache.Append(fmt("S %s %s %d %d %d %d\n", hex, p.isTsa ? StrL("TSA") : StrL("CA"), p.start, p.end,
                             p.eSig ? 1 : 0, p.eSeal ? 1 : 0));
        }
    }
    TempStr cachePath = EutlCachePathTemp();
    if (len(cachePath) == 0 || !dir::CreateForFile(cachePath) || !file::WriteFile(cachePath, ToStr(cache))) {
        return fail(StrL("could not write the EUTL cache"));
    }
    logf("EutlUpdate: %d certificates from %d national lists\n", nCerts, fetched);
    return true;
}

void EutlRegisterLookup() {
#if OS_WIN
    static const EutlLookup kLookup = {EutlListOpen, EutlListClose, EutlListStore, EutlListStatus, EutlListDate};
    SetEutlLookup(&kLookup);
#endif
}
