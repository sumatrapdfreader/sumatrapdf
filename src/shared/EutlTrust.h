/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// the EU Trusted List as the engine sees it; lives with the app, which has http
enum class EutlKind {
    CA,
    TSA,
};
constexpr int kEutlListed = 1;
constexpr int kEutlESig = 2;
constexpr int kEutlESeal = 4;
struct EutlLookup {
    void* (*open)();
    void (*close)(void* list);
    // a certificate store with the listed certificates, for building chains
    void* (*store)(void* list);
    // kEutl* flags as of when: 0 if not listed
    int (*status)(void* list, const u8* der, int derLen, i64 when, EutlKind kind);
    // when the list was downloaded, yyyy-mm-dd; empty if it wasn't
    Str (*date)(void* list);
};

bool EutlCertIsListed(const u8* der, int derLen);
bool EutlCacheExists();
bool EutlNeedsUpdate();
TempStr EutlCacheInfoTemp();
bool EutlUpdate(Str* errOut);
void EutlRegisterLookup();

// certB64 points into the xml given to EutlParseTsl()
struct EutlParsedPeriod {
    Str certB64;
    bool isTsa = false;
    i64 start = 0;
    // 0: still in effect
    i64 end = 0;
    bool granted = false;
    bool eSig = false;
    bool eSeal = false;
};
void EutlParseTsl(Str xml, Vec<EutlParsedPeriod>& out, StrVec* tslUrls);
i64 EutlParseXmlTime(Str s);
