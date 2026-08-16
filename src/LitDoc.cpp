/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/* Reader for Microsoft Reader .lit ebooks: parses the ITOLITLS container,
   unseals the DRM1 ("sealed") encryption present in every .lit, decompresses
   the LZX sections, reconstructs HTML / OPF from their tokenized binary form
   and repackages everything as an in-memory epub, which EngineMupdf renders.

   The format understanding comes from ConvertLIT and calibre's lit reader
   (src/calibre/ebooks/lit/reader.py); the tag code tables in LitDocMaps.h are
   generated from calibre's maps. DRM5 (books locked to a Microsoft Passport
   account) is not supported. */

#include "base/Base.h"
#include "base/GuessFileType.h"
#include "base/Zip.h"

#include "LitDoc.h"
#include "LitDocMaps.h"

#include "SumatraLog.h"

extern "C" {
// public domain d3des, in ext/msdes
#include <d3des.h>

// the LZX decompressor inside ext/chmdec/chm.c
struct LZXstate;
struct LZXstate* LZXinit(int window);
void LZXteardown(struct LZXstate* pState);
int LZXreset(struct LZXstate* pState);
int LZXdecompress(struct LZXstate* pState, u8* inpos, u8* outpos, int inlen, int outlen);
}

constexpr int kLzxOk = 0; // chm.c DECR_OK

static const char* kLzxGuid = "{0A9007C6-4076-11D3-8789-0000F8105754}";
static const char* kDesGuid = "{67F6E4A2-60BF-11D3-8540-00C04F58C3CF}";

//--- mssha1: SHA-1 as modified by Microsoft for .lit DRM key derivation:
// different initial state and a few rounds use the wrong mixing function

struct MsSha1 {
    u32 h[5];
    u64 nBytes = 0;
    u8 buf[64];
    int bufUsed = 0;
};

static u32 rol32(u32 x, int n) {
    return (x << n) | (x >> (32 - n));
}

static void MsSha1Init(MsSha1* s) {
    s->h[0] = 0x32107654;
    s->h[1] = 0x23016745;
    s->h[2] = 0xC4E680A2;
    s->h[3] = 0xDC679823;
    s->h[4] = 0xD0857A34;
    s->nBytes = 0;
    s->bufUsed = 0;
}

static void MsSha1Block(MsSha1* s, const u8* p) {
    u32 w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = ((u32)p[i * 4] << 24) | ((u32)p[i * 4 + 1] << 16) | ((u32)p[i * 4 + 2] << 8) | (u32)p[i * 4 + 3];
    }
    for (int t = 16; t < 80; t++) {
        w[t] = rol32(w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16], 1);
    }
    static const u32 k[4] = {0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6};
    u32 a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3], e = s->h[4];
    for (int t = 0; t < 80; t++) {
        // which mixing function runs at round t; MS swapped a few and rounds
        // 6 and 42 use (b + c) ^ c, which is not any of the standard three
        int fi = t / 20; // 0 = choice, 1 = parity, 2 = majority, 3 = parity
        switch (t) {
            case 3:
            case 10:
            case 15:
            case 51:
                fi = 1;
                break;
            case 26:
            case 68:
                fi = 0;
                break;
            case 31:
                fi = 2;
                break;
            case 6:
            case 42:
                fi = 4;
                break;
        }
        u32 f;
        switch (fi) {
            case 0:
                f = (b & (c ^ d)) ^ d;
                break;
            case 2:
                f = (b & c) | (b & d) | (c & d);
                break;
            case 4:
                f = (b + c) ^ c;
                break;
            default:
                f = b ^ c ^ d;
                break;
        }
        u32 tmp = rol32(a, 5) + f + e + w[t] + k[t / 20];
        e = d;
        d = c;
        c = rol32(b, 30);
        b = a;
        a = tmp;
    }
    s->h[0] += a;
    s->h[1] += b;
    s->h[2] += c;
    s->h[3] += d;
    s->h[4] += e;
}

static void MsSha1Update(MsSha1* s, const u8* d, int n) {
    s->nBytes += (u64)n;
    while (n > 0) {
        int take = std::min(n, 64 - s->bufUsed);
        memcpy(s->buf + s->bufUsed, d, (size_t)take);
        s->bufUsed += take;
        d += take;
        n -= take;
        if (s->bufUsed == 64) {
            MsSha1Block(s, s->buf);
            s->bufUsed = 0;
        }
    }
}

static void MsSha1Final(MsSha1* s, u8 digest[20]) {
    u64 bitLen = s->nBytes * 8;
    u8 pad = 0x80;
    MsSha1Update(s, &pad, 1);
    u8 zero = 0;
    while (s->bufUsed != 56) {
        MsSha1Update(s, &zero, 1);
    }
    u8 lenBuf[8];
    for (int i = 0; i < 8; i++) {
        lenBuf[i] = (u8)(bitLen >> (56 - i * 8));
    }
    MsSha1Update(s, lenBuf, 8);
    for (int i = 0; i < 5; i++) {
        digest[i * 4] = (u8)(s->h[i] >> 24);
        digest[i * 4 + 1] = (u8)(s->h[i] >> 16);
        digest[i * 4 + 2] = (u8)(s->h[i] >> 8);
        digest[i * 4 + 3] = (u8)(s->h[i]);
    }
}

//--- little-endian readers with bounds checking

static u32 LitU16(Str d, int off) {
    if (off < 0 || off + 2 > len(d)) {
        return 0;
    }
    const u8* p = (const u8*)d.s + off;
    return (u32)p[0] | ((u32)p[1] << 8);
}

static u32 LitU32(Str d, int off) {
    if (off < 0 || off + 4 > len(d)) {
        return 0;
    }
    const u8* p = (const u8*)d.s + off;
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static i64 LitU64(Str d, int off) {
    return (i64)LitU32(d, off) | ((i64)LitU32(d, off + 4) << 32);
}

// 7-bit groups, high bit set = continue; big-endian group order. Values too
// large for an int saturate at INT_MAX (the root "/" entry stores junk that
// overflows; its value is never used, but the bytes must be consumed)
static int LitEncInt(Str d, int* pos) {
    u64 v = 0;
    while (*pos < len(d)) {
        u8 b = (u8)d.s[(*pos)++];
        v = (v << 7) | (b & 0x7f);
        if (v > (u64)INT_MAX) {
            v = (u64)INT_MAX;
        }
        if (!(b & 0x80)) {
            return (int)v;
        }
    }
    return -1;
}

// one utf8-encoded value (the tokenized format uses utf8 for tag / attr
// codes, so values like 0x8000 span multiple bytes); -1 on malformed input
static int LitUtf8Char(Str d, int* pos) {
    if (*pos >= len(d)) {
        return -1;
    }
    u32 c = (u8)d.s[(*pos)++];
    if (c < 0x80) {
        return (int)c;
    }
    int mask = 0x80;
    int size = 0;
    while (c & mask) {
        mask >>= 1;
        size++;
    }
    // the tokenized format can use overlong encodings, up to 6 bytes
    if (size < 2 || size > 6) {
        return -1;
    }
    c &= mask - 1;
    for (int i = 1; i < size; i++) {
        if (*pos >= len(d)) {
            return -1;
        }
        u8 b = (u8)d.s[(*pos)++];
        if ((b & 0xC0) != 0x80) {
            return -1;
        }
        c = (c << 6) | (b & 0x3f);
    }
    return (int)c;
}

static void LitAppendUtf8(str::Builder& out, int c) {
    if (c < 0x80) {
        out.AppendChar((char)c);
    } else if (c < 0x800) {
        out.AppendChar((char)(0xC0 | (c >> 6)));
        out.AppendChar((char)(0x80 | (c & 0x3f)));
    } else if (c < 0x10000) {
        out.AppendChar((char)(0xE0 | (c >> 12)));
        out.AppendChar((char)(0x80 | ((c >> 6) & 0x3f)));
        out.AppendChar((char)(0x80 | (c & 0x3f)));
    } else {
        out.AppendChar((char)(0xF0 | (c >> 18)));
        out.AppendChar((char)(0x80 | ((c >> 12) & 0x3f)));
        out.AppendChar((char)(0x80 | ((c >> 6) & 0x3f)));
        out.AppendChar((char)(0x80 | (c & 0x3f)));
    }
}

// length-prefixed utf8 string: first utf8 char is the length in characters
static TempStr LitSizedStringTemp(Str d, int* pos, bool zpad) {
    int nChars = LitUtf8Char(d, pos);
    if (nChars < 0) {
        return {};
    }
    str::Builder out;
    for (int i = 0; i < nChars; i++) {
        int c = LitUtf8Char(d, pos);
        if (c < 0) {
            return {};
        }
        LitAppendUtf8(out, c);
    }
    if (zpad && *pos < len(d) && d.s[*pos] == 0) {
        (*pos)++;
    }
    return str::DupTemp(ToStrTemp(out));
}

//--- the .lit container

struct LitEntry {
    Str name; // points into the directory piece
    int section = 0;
    int offset = 0;
    int size = 0;
};

struct LitManifestItem {
    TempStr internal{};
    TempStr original{};
    TempStr mime{};
    TempStr path{}; // normalized path, also the path inside the epub
    bool isSpine = false;
};

constexpr int kLitMaxSections = 16;

struct LitFile {
    Str d; // the whole file
    int contentOffset = 0;
    u32 entryChunkLen = 0;
    u32 entryUnknown = 0;
    Vec<LitEntry> entries;
    StrVec sectionNames;
    Str sectionData[kLitMaxSections]; // decoded caches, owned
    Vec<LitManifestItem> manifest;
    int drmLevel = 0;
    u8 bookKey[8] = {};

    ~LitFile() {
        for (Str& s : sectionData) {
            str::Free(s);
        }
    }

    LitEntry* FindEntry(Str name);
    Str GetFile(Str name); // temp or view; copy if kept
    Str GetSection(int section);
};

LitEntry* LitFile::FindEntry(Str name) {
    for (LitEntry& e : entries) {
        if (str::Eq(e.name, name)) {
            return &e;
        }
    }
    return nullptr;
}

static bool LitParseHeader(LitFile* lit) {
    Str d = lit->d;
    if (len(d) < 0x28 || !str::StartsWith(d, StrL("ITOLITLS"))) {
        return false;
    }
    if (LitU32(d, 8) != 1) {
        logf("LitDoc: unknown version %d\n", (int)LitU32(d, 8));
        return false;
    }
    int hdrLen = (int)LitU32(d, 12);
    int nPieces = (int)LitU32(d, 16);
    int secHdrLen = (int)LitU32(d, 20);
    if (hdrLen < 0x28 || nPieces < 5 || nPieces > 16) {
        return false;
    }

    // secondary header: CAOL / ITSF blocks
    {
        int off = hdrLen + nPieces * 16;
        Str sec(d.s + off, std::min(secHdrLen, len(d) - off));
        int pos = (int)LitU32(sec, 4);
        bool haveContentOffset = false;
        while (pos >= 0 && pos + 8 <= len(sec)) {
            Str blockTag(sec.s + pos, 4);
            u32 ver = LitU32(sec, pos + 4);
            if (str::Eq(blockTag, StrL("CAOL"))) {
                if (ver != 2) {
                    return false;
                }
                lit->entryChunkLen = LitU32(sec, pos + 20);
                lit->entryUnknown = LitU32(sec, pos + 28);
                pos += 48;
            } else if (str::Eq(blockTag, StrL("ITSF"))) {
                if (ver != 4 || LitU32(sec, pos + 20) != 0) {
                    return false;
                }
                lit->contentOffset = (int)LitU32(sec, pos + 16);
                haveContentOffset = true;
                pos += 48;
            } else {
                break;
            }
        }
        if (!haveContentOffset) {
            return false;
        }
    }

    // header piece 1 is the directory
    i64 dirOff64 = LitU64(d, hdrLen + 16);
    i64 dirLen64 = LitU64(d, hdrLen + 16 + 8);
    if (dirOff64 <= 0 || dirLen64 <= 32 || dirOff64 + dirLen64 > len(d)) {
        return false;
    }
    Str dir(d.s + (int)dirOff64, (int)dirLen64);
    if (!str::StartsWith(dir, StrL("IFCM"))) {
        return false;
    }
    int chunkSize = (int)LitU32(dir, 8);
    int nChunks = (int)LitU32(dir, 24);
    if (chunkSize <= 48 || nChunks <= 0 || 32 + (i64)nChunks * chunkSize != dirLen64) {
        return false;
    }
    if (lit->entryChunkLen && (u32)chunkSize != lit->entryChunkLen) {
        return false;
    }
    for (int i = 0; i < nChunks; i++) {
        int chunkOff = 32 + i * chunkSize;
        Str chunk(dir.s + chunkOff, chunkSize);
        if (!str::StartsWith(chunk, StrL("AOLL"))) {
            continue;
        }
        int freeSpace = (int)LitU32(chunk, 4);
        if (freeSpace < 0 || freeSpace >= chunkSize) {
            return false;
        }
        int dataEnd = chunkSize - freeSpace - 2; // last 2 bytes: entry count
        int nEntries = (int)LitU16(chunk, chunkSize - 2);
        if (nEntries == 0) {
            nEntries = 0xffff;
        }
        int pos = 48;
        for (int j = 0; j < nEntries && pos < dataEnd; j++) {
            int nameLen = LitEncInt(chunk, &pos);
            if (nameLen <= 0 || pos + nameLen > dataEnd) {
                break;
            }
            LitEntry e;
            e.name = Str(chunk.s + pos, nameLen);
            pos += nameLen;
            e.section = LitEncInt(chunk, &pos);
            e.offset = LitEncInt(chunk, &pos);
            e.size = LitEncInt(chunk, &pos);
            if (e.size < 0) {
                break; // ran off the end of the chunk
            }
            lit->entries.Append(e);
        }
    }
    return len(lit->entries) > 0;
}

Str LitFile::GetFile(Str name) {
    LitEntry* e = FindEntry(name);
    if (!e) {
        return {};
    }
    if (e->section == 0) {
        i64 off = (i64)contentOffset + e->offset;
        if (off + e->size > len(d)) {
            return {};
        }
        return Str(d.s + (int)off, e->size);
    }
    Str sec = GetSection(e->section);
    if (e->offset + e->size > len(sec)) {
        return {};
    }
    return Str(sec.s + e->offset, e->size);
}

static bool LitParseSectionNames(LitFile* lit) {
    Str raw = lit->GetFile("::DataSpace/NameList");
    if (len(raw) < 4) {
        return false;
    }
    int nSections = (int)LitU16(raw, 2);
    if (nSections <= 0 || nSections > kLitMaxSections) {
        return false;
    }
    int pos = 4;
    for (int i = 0; i < nSections; i++) {
        int nChars = (int)LitU16(raw, pos);
        pos += 2;
        if (pos + nChars * 2 + 2 > len(raw)) {
            return false;
        }
        WStr ws((const WCHAR*)(raw.s + pos), nChars);
        lit->sectionNames.Append(ToUtf8Temp(ws));
        pos += nChars * 2 + 2;
    }
    return true;
}

static void LitDesDecrypt(u8* dst, const u8* src, int n, const u8 key[8]) {
    u8 k[8];
    memcpy(k, key, 8);
    deskey(k, DE1);
    for (int off = 0; off + 8 <= n; off += 8) {
        u8 in[8];
        memcpy(in, src + off, 8);
        des(in, dst + off);
    }
}

// DRM1 key: fold the mssha1 hash of (2 NUL bytes + /meta, zero-padded to a
// 64-byte multiple) + (/DRMStorage/DRMSource, same padding)
static bool LitReadDrm(LitFile* lit) {
    if (lit->FindEntry("/DRMStorage/Licenses/EUL")) {
        lit->drmLevel = 5;
        logf("LitDoc: DRM5 (user-locked) book, cannot decrypt\n");
        return false;
    }
    if (lit->FindEntry("/DRMStorage/DRMBookplate")) {
        lit->drmLevel = 3;
    } else if (lit->FindEntry("/DRMStorage/DRMSealed")) {
        lit->drmLevel = 1;
    } else {
        return true; // no DRM
    }
    MsSha1 sha;
    MsSha1Init(&sha);
    const char* hashFiles[3] = {"/meta", "/DRMStorage/DRMSource", nullptr};
    if (lit->drmLevel == 3) {
        hashFiles[2] = "/DRMStorage/DRMBookplate";
    }
    u8 zeros[64] = {};
    int prepad = 2; // the first hashed file gets 2 leading NUL bytes
    for (const char* name : hashFiles) {
        if (!name) {
            continue;
        }
        Str data = lit->GetFile(name);
        if (str::IsNull(data)) {
            return false;
        }
        if (prepad > 0) {
            MsSha1Update(&sha, zeros, prepad);
        }
        MsSha1Update(&sha, (const u8*)data.s, len(data));
        int pad = 64 - ((len(data) + prepad) % 64);
        if (pad < 64) {
            MsSha1Update(&sha, zeros, pad);
        }
        prepad = 0;
    }
    u8 digest[20];
    MsSha1Final(&sha, digest);
    u8 key[8] = {};
    for (int i = 0; i < 20; i++) {
        key[i % 8] ^= digest[i];
    }

    Str sealed = lit->GetFile("/DRMStorage/DRMSealed");
    if (len(sealed) < 16) {
        return false;
    }
    u8 unsealed[16];
    LitDesDecrypt(unsealed, (const u8*)sealed.s, 16, key);
    if (unsealed[0] != 0) {
        logf("LitDoc: failed to unseal DRM key\n");
        return false;
    }
    memcpy(lit->bookKey, unsealed + 1, 8);
    return true;
}

static Str LitLzxDecompress(Str content, Str control, Str resetTable) {
    if (len(control) < 32 || !str::Eq(Str(control.s + 4, 4), StrL("LZXC"))) {
        return {};
    }
    if (len(resetTable) < 40) {
        return {};
    }
    int windowSize = 14;
    u32 u = LitU32(control, 12);
    while (u > 0) {
        u >>= 1;
        windowSize++;
    }
    if (windowSize < 15 || windowSize > 21) {
        return {};
    }
    struct LZXstate* lzx = LZXinit(windowSize);
    if (!lzx) {
        return {};
    }

    int ofsEntry = (int)LitU32(resetTable, 12) + 8;
    int ucLength = (int)LitU32(resetTable, 16);
    if (LitU32(resetTable, 20) != 0) {
        LZXteardown(lzx);
        return {};
    }
    int interval = (int)LitU32(resetTable, 32);
    int bytesRemaining = ucLength;
    if (interval <= 0) {
        LZXteardown(lzx);
        return {};
    }

    // the reset table stores a compressed offset at every `interval` (block_size)
    // uncompressed bytes. The LZX decoder is reset only at window boundaries;
    // the finer reset-table granularity is for random seeking. Decode one
    // interval at a time (feeding the exact compressed slice for that interval),
    // resetting only when a new window begins.
    int windowBytes = 1 << windowSize;
    int intervalsPerWindow = (windowBytes >= interval) ? (windowBytes / interval) : 1;
    str::Builder out;
    u8* obuf = AllocArray<u8>(interval);
    bool ok = true;
    int base = 0;
    int idx = 0;
    while (bytesRemaining > 0 && ofsEntry + 8 <= len(resetTable)) {
        int size = (int)LitU32(resetTable, ofsEntry);
        if (LitU32(resetTable, ofsEntry + 4) != 0 || size > len(content) || size < base) {
            ok = false;
            break;
        }
        int outThis = std::min(interval, bytesRemaining);
        if (idx % intervalsPerWindow == 0) {
            LZXreset(lzx);
        }
        int res = LZXdecompress(lzx, (u8*)content.s + base, obuf, size - base, outThis);
        if (res != kLzxOk) {
            ok = false;
            break;
        }
        out.Append(Str((char*)obuf, outThis));
        bytesRemaining -= outThis;
        base = size;
        ofsEntry += 8;
        idx++;
    }
    // last (partial) interval extends to the end of the content. A well-formed
    // file's remainder is < interval (obuf's size); anything larger means the
    // reset table ran out early, i.e. the file is malformed
    if (ok && bytesRemaining > 0 && bytesRemaining <= interval) {
        if (idx % intervalsPerWindow == 0) {
            LZXreset(lzx);
        }
        int res = LZXdecompress(lzx, (u8*)content.s + base, obuf, len(content) - base, bytesRemaining);
        if (res == kLzxOk) {
            out.Append(Str((char*)obuf, bytesRemaining));
            bytesRemaining = 0;
        }
    }
    Free(nullptr, obuf);
    LZXteardown(lzx);
    if (!ok || bytesRemaining != 0) {
        logf("LitDoc: LZX decompression failed\n");
        return {};
    }
    return out.TakeStr();
}

static TempStr LitGuidTemp(Str d) {
    if (len(d) < 16) {
        return {};
    }
    const u8* p = (const u8*)d.s;
    u32 a = LitU32(d, 0);
    u32 b = LitU16(d, 4);
    u32 c = LitU16(d, 6);
    return fmt("{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", a, b, c, p[8], p[9], p[10], p[11], p[12], p[13],
               p[14], p[15]);
}

Str LitFile::GetSection(int section) {
    if (section <= 0 || section >= kLitMaxSections || section >= len(sectionNames)) {
        return {};
    }
    if (!str::IsNull(sectionData[section])) {
        return sectionData[section];
    }
    Str name = sectionNames.At(section);
    TempStr base = fmt("::DataSpace/Storage/%s", name);
    Str transform = GetFile(fmt("%s/Transform/List", Str(base)));
    Str content = GetFile(fmt("%s/Content", Str(base)));
    Str control = GetFile(fmt("%s/ControlData", Str(base)));

    Str cur = {};       // owned copy of the data as transforms get applied
    bool owned = false; // content starts as a view into d
    Str view = content;
    while (len(transform) >= 16) {
        int csize = ((int)LitU32(control, 0) + 1) * 4;
        if (csize <= 0 || csize > len(control)) {
            break;
        }
        TempStr guid = LitGuidTemp(transform);
        if (str::Eq(guid, Str(kDesGuid))) {
            if (drmLevel == 0 || drmLevel == 5) {
                if (owned) {
                    str::Free(cur);
                }
                return {};
            }
            int n = len(view);
            int nPadded = RoundUp(n, 8);
            u8* dec = AllocArray<u8>(nPadded);
            u8* src = AllocArray<u8>(nPadded);
            memcpy(src, view.s, (size_t)n);
            LitDesDecrypt(dec, src, nPadded, bookKey);
            Free(nullptr, src);
            if (owned) {
                str::Free(cur);
            }
            cur = Str((char*)dec, n);
            view = cur;
            owned = true;
        } else if (str::Eq(guid, Str(kLzxGuid))) {
            Str resetTable = GetFile(fmt("%s/Transform/%s/InstanceData/ResetTable", Str(base), Str(kLzxGuid)));
            Str dec = LitLzxDecompress(view, control, resetTable);
            if (owned) {
                str::Free(cur);
            }
            if (str::IsNull(dec)) {
                return {};
            }
            cur = dec;
            view = cur;
            owned = true;
        } else {
            logf("LitDoc: unknown transform %s\n", guid);
            if (owned) {
                str::Free(cur);
            }
            return {};
        }
        control = Str(control.s + csize, len(control) - csize);
        transform = Str(transform.s + 16, len(transform) - 16);
    }
    if (!owned) {
        cur = str::Dup(view);
    }
    sectionData[section] = cur;
    return cur;
}

//--- manifest

// resolve ".." / "." components; a path that escapes the root keeps getting
// its leading "../" stripped, like calibre does
static TempStr LitNormPathTemp(Str path) {
    StrVec parts;
    Split(&parts, path, "/", true);
    StrVec out;
    for (int i = 0; i < len(parts); i++) {
        Str p = parts.At(i);
        if (str::Eq(p, StrL("."))) {
            continue;
        }
        if (str::Eq(p, StrL("..")) && !out.IsEmpty() && !str::Eq(out.At(len(out) - 1), StrL(".."))) {
            out.RemoveAt(len(out) - 1);
            continue;
        }
        out.Append(p);
    }
    int skip = 0;
    while (skip < len(out) && str::Eq(out.At(skip), StrL(".."))) {
        skip++;
    }
    str::Builder res;
    for (int i = skip; i < len(out); i++) {
        if (i > skip) {
            res.Append("/");
        }
        res.Append(out.At(i));
    }
    return str::DupTemp(ToStrTemp(res));
}

static bool LitParseManifest(LitFile* lit) {
    Str raw = lit->GetFile("/manifest");
    if (str::IsNull(raw)) {
        return false;
    }
    int pos = 0;
    while (pos < len(raw)) {
        int slen = (u8)raw.s[pos++];
        if (slen == 0 || pos + slen > len(raw)) {
            break;
        }
        pos += slen; // root name, unused
        for (int state = 0; state < 4; state++) {
            // 0 = spine, 1 = not spine, 2 = css, 3 = images
            int nFiles = (int)LitU32(raw, pos);
            pos += 4;
            if (nFiles <= 0) {
                continue;
            }
            for (int i = 0; i < nFiles; i++) {
                if (pos + 5 > len(raw)) {
                    return len(lit->manifest) > 0;
                }
                pos += 4; // offset, unused
                LitManifestItem item;
                item.internal = LitSizedStringTemp(raw, &pos, false);
                item.original = LitSizedStringTemp(raw, &pos, false);
                item.mime = LitSizedStringTemp(raw, &pos, true);
                if (!item.internal || !item.original) {
                    return len(lit->manifest) > 0;
                }
                item.isSpine = (state == 0);
                // normalize the original path: windows separators, drive
                // letters, stray ".." (all seen in the wild per calibre)
                TempStr path = str::DupTemp(item.original);
                str::TransCharsInPlace(path, "\\", "/");
                if (len(path) > 2 && path.s[1] == ':' && path.s[2] == '/') {
                    path = str::DupTemp(Str(path.s + 3, len(path) - 3));
                }
                item.path = LitNormPathTemp(path);
                lit->manifest.Append(item);
            }
        }
    }
    // strip the path prefix shared by all items
    if (len(lit->manifest) > 1) {
        for (;;) {
            Str first = lit->manifest[0].path;
            int slash = -1;
            for (int i = 0; i < len(first); i++) {
                if (first.s[i] == '/') {
                    slash = i;
                    break;
                }
            }
            if (slash < 0) {
                break;
            }
            Str prefix(first.s, slash + 1);
            bool all = true;
            for (LitManifestItem& it : lit->manifest) {
                if (!str::StartsWith(it.path, prefix)) {
                    all = false;
                    break;
                }
            }
            if (!all) {
                break;
            }
            for (LitManifestItem& it : lit->manifest) {
                it.path = str::DupTemp(Str(it.path.s + len(prefix), len(it.path) - len(prefix)));
            }
        }
    }
    for (LitManifestItem& it : lit->manifest) {
        if (len(it.path) == 0) {
            it.path = str::DupTemp(it.internal);
        }
    }
    return len(lit->manifest) > 0;
}

static LitManifestItem* LitFindManifest(LitFile* lit, Str internal) {
    for (LitManifestItem& it : lit->manifest) {
        if (str::Eq(it.internal, internal)) {
            return &it;
        }
    }
    return nullptr;
}

//--- UnBinary: reconstruct HTML / OPF text from the tokenized binary form

struct LitAtoms {
    StrVec tags;  // 1-based: tags[i - 1]
    StrVec attrs; // 1-based
};

// /data/{internal}/atom: custom tag / attribute names referenced by FLAG_ATOM
static void LitParseAtoms(LitFile* lit, Str internal, LitAtoms* atoms) {
    Str data = lit->GetFile(fmt("/data/%s/atom", internal));
    if (len(data) < 4) {
        return;
    }
    int n = (int)LitU32(data, 0);
    int pos = 4;
    for (int i = 0; i < n; i++) {
        if (pos + 1 > len(data)) {
            return;
        }
        int size = (u8)data.s[pos++];
        if (size == 0 || pos + size > len(data)) {
            return;
        }
        atoms->tags.Append(Str(data.s + pos, size));
        pos += size;
    }
    if (pos + 4 > len(data)) {
        return;
    }
    n = (int)LitU32(data, pos);
    pos += 4;
    for (int i = 0; i < n; i++) {
        if (pos + 4 > len(data)) {
            return;
        }
        int size = (int)LitU32(data, pos);
        pos += 4;
        if (size <= 0 || pos + size > len(data)) {
            return;
        }
        atoms->attrs.Append(Str(data.s + pos, size));
        pos += size;
    }
}

constexpr int kLitFlagOpening = 1 << 0;
constexpr int kLitFlagClosing = 1 << 1;
constexpr int kLitFlagAtom = 1 << 4;

struct UnBinaryCtx {
    Str bin;
    int pos = 0;
    str::Builder out;
    LitFile* lit = nullptr;
    TempStr dir{}; // directory of the file being reconstructed
    bool isHtml = true;
    LitAtoms* atoms = nullptr;
};

static const char* LitTagName(UnBinaryCtx* ctx, int tag) {
    if (tag < 0) {
        return nullptr;
    }
    if (ctx->isHtml) {
        // gLitHtmlTags is a SeqStrings indexed by tag code; a code with no tag
        // is stored as the "\x01" sentinel (empty isn't representable mid-list)
        Str name = SeqStrByIndex(gLitHtmlTags, tag);
        if (len(name) > 0 && name.s[0] != '\x01') {
            return name.s; // points into the static literal
        }
        return nullptr;
    }
    if (tag < dimofi(gLitOpfTags)) {
        return gLitOpfTags[tag];
    }
    return nullptr;
}

static const char* LitAttrInList(const LitAttrCode* attrs, int n, int code) {
    for (int i = 0; i < n; i++) {
        if ((int)attrs[i].code == code) {
            return attrs[i].name;
        }
    }
    return nullptr;
}

static const char* LitAttrName(UnBinaryCtx* ctx, int tag, bool tagIsAtom, int code) {
    if (tagIsAtom && ctx->atoms && code >= 1 && code <= len(ctx->atoms->attrs)) {
        Str s = ctx->atoms->attrs.At(code - 1);
        if (len(s) > 0) {
            return s.s; // page strings are 0-terminated
        }
    }
    const char* res = nullptr;
    if (!tagIsAtom && ctx->isHtml && tag >= 0 && tag < dimofi(gLitHtmlTagAttrs)) {
        res = LitAttrInList(gLitHtmlTagAttrs[tag].attrs, gLitHtmlTagAttrs[tag].n, code);
    }
    if (!res) {
        res = ctx->isHtml ? LitAttrInList(gLitHtmlAttrs, dimofi(gLitHtmlAttrs), code)
                          : LitAttrInList(gLitOpfAttrs, dimofi(gLitOpfAttrs), code);
    }
    return res;
}

// href/src values start with '/' and reference a manifest internal id;
// translate to the item's path, relative to the current file's directory
static TempStr LitResolveHrefTemp(UnBinaryCtx* ctx, Str href) {
    // the first character is a flag byte (not part of the path); drop it
    if (len(href) > 0) {
        href = Str(href.s + 1, len(href) - 1);
    }
    Str doc = href;
    Str frag = {};
    for (int i = 0; i < len(href); i++) {
        if (href.s[i] == '#') {
            doc = Str(href.s, i);
            frag = Str(href.s + i, len(href) - i); // includes '#'
            break;
        }
    }
    TempStr path = str::DupTemp(doc);
    LitManifestItem* item = ctx->lit ? LitFindManifest(ctx->lit, doc) : nullptr;
    if (item) {
        // make relative to ctx->dir
        Str target = item->path;
        Str base = ctx->dir;
        // strip common leading directories
        for (;;) {
            int slash = -1;
            for (int i = 0; i < std::min(len(target), len(base)); i++) {
                if (target.s[i] != base.s[i]) {
                    break;
                }
                if (target.s[i] == '/') {
                    slash = i;
                }
            }
            if (slash < 0) {
                break;
            }
            target = Str(target.s + slash + 1, len(target) - slash - 1);
            base = Str(base.s + slash + 1, len(base) - slash - 1);
        }
        int nUp = 0;
        for (int i = 0; i < len(base); i++) {
            if (base.s[i] == '/') {
                nUp++;
            }
        }
        if (len(base) > 0) {
            nUp++; // base is a dir path without trailing slash
        }
        str::Builder rel;
        for (int i = 0; i < nUp; i++) {
            rel.Append("../");
        }
        rel.Append(target);
        path = str::DupTemp(ToStrTemp(rel));
    }
    if (len(frag) > 0) {
        path = str::JoinTemp(Str(path), frag);
    }
    return path;
}

// emit one output character: ASCII verbatim, everything else as a numeric
// character reference. Matches calibre and sidesteps any charset ambiguity in
// the reconstructed XHTML (mupdf would otherwise read raw UTF-8 as latin-1)
static void LitEmitChar(str::Builder& out, int c) {
    if (c < 0x80) {
        out.AppendChar((char)c);
    } else {
        out.Append(fmt("&#%d;", c));
    }
}

// appends c to out, escaping markup like the tokenized form expects: literal
// '<' / '>' in text are doubled here and fixed up in LitEscapeReserved()
static void LitEmitTextChar(str::Builder& out, int c) {
    if (c == '\v') {
        out.AppendChar('\n');
    } else if (c == '>') {
        out.Append(">>");
    } else if (c == '<') {
        out.Append("<<");
    } else {
        LitEmitChar(out, c);
    }
}

// port of calibre's UnBinary.binary_to_text state machine
static bool LitBinaryToText(UnBinaryCtx* ctx, int depth) {
    if (depth > 128) {
        return false;
    }
    str::Builder& out = ctx->out;
    Str bin = ctx->bin;
    int state = 0; // 0 text, 1 flags, 2 tag, 3 attr, 4 value-len, 5 value,
                   // 6 custom-len, 7 custom, 8 attr-len, 9 custom-attr,
                   // 10 href-len, 11 href
    int flags = 0;
    int count = 0;
    bool inCensorship = false;
    bool isGoingdown = false;
    bool tagIsAtom = false;
    int tag = 0;
    TempStr tagName = nullptr;
    str::Builder custom;
    str::Builder href;

    while (ctx->pos < len(bin)) {
        int c = LitUtf8Char(bin, &ctx->pos);
        if (c < 0) {
            return false;
        }
        switch (state) {
            case 0: // text
                if (c == 0) {
                    state = 1;
                } else {
                    LitEmitTextChar(out, c);
                }
                break;
            case 1: // get flags
                if (c == 0) {
                    state = 0;
                    break;
                }
                flags = c;
                state = 2;
                break;
            case 2: // get tag
                state = (c == 0) ? 0 : 3;
                if (flags & kLitFlagOpening) {
                    tag = c;
                    out.Append("<");
                    isGoingdown = !(flags & kLitFlagClosing);
                    if (tag == 0x8000) {
                        state = 6;
                        break;
                    }
                    tagIsAtom = false;
                    const char* name = nullptr;
                    if (flags & kLitFlagAtom) {
                        if (ctx->atoms && tag >= 1 && tag <= len(ctx->atoms->tags)) {
                            name = ctx->atoms->tags.At(tag - 1).s;
                            tagIsAtom = true;
                        } else {
                            return false;
                        }
                    } else {
                        name = LitTagName(ctx, tag);
                    }
                    if (name) {
                        tagName = str::DupTemp(Str(name));
                    } else {
                        tagName = fmt("x-lit-tag-%d", tag);
                    }
                    out.Append(Str(tagName));
                } else if (flags & kLitFlagClosing) {
                    if (depth == 0) {
                        return false;
                    }
                    return true; // parent writes the close tag
                }
                break;
            case 3: // get attr
                inCensorship = false;
                if (c == 0) {
                    state = 0;
                    if (!isGoingdown) {
                        tagName = nullptr;
                        out.Append(" />");
                    } else {
                        out.Append(">");
                        // recursively emit children until the closing token
                        if (!LitBinaryToText(ctx, depth + 1)) {
                            return false;
                        }
                        if (!tagName) {
                            return false;
                        }
                        out.Append("</");
                        out.Append(Str(tagName));
                        out.Append(">");
                        tagName = nullptr;
                        isGoingdown = false;
                    }
                } else {
                    if (c == 0x8000) {
                        state = 8;
                        break;
                    }
                    const char* attr = LitAttrName(ctx, tag, tagIsAtom, c);
                    if (!attr) {
                        return false;
                    }
                    if (attr[0] == '%') {
                        inCensorship = true;
                        state = 4;
                        break;
                    }
                    out.Append(" ");
                    out.Append(Str(attr));
                    out.Append("=");
                    if (str::Eq(Str(attr), StrL("href")) || str::Eq(Str(attr), StrL("src"))) {
                        state = 10;
                    } else {
                        state = 4;
                    }
                }
                break;
            case 4: // get value length
                if (!inCensorship) {
                    out.Append("\"");
                }
                count = c - 1;
                if (count == 0) {
                    if (!inCensorship) {
                        out.Append("\"");
                    }
                    inCensorship = false;
                    state = 3;
                    break;
                }
                state = 5;
                if (c == 0xffff) {
                    break; // numeric value follows as one char
                }
                if (count < 0 || count > len(bin) - ctx->pos) {
                    return false;
                }
                break;
            case 5: // get value
                if (count == 0xfffe) {
                    if (!inCensorship) {
                        out.Append(fmt("%d\"", c - 1));
                    }
                    inCensorship = false;
                    state = 3;
                } else if (count > 0) {
                    if (!inCensorship) {
                        if (c == '"') {
                            out.Append("&quot;");
                        } else if (c == '<') {
                            out.Append("&lt;");
                        } else {
                            LitEmitChar(out, c);
                        }
                    }
                    count--;
                }
                if (count == 0 && state == 5) {
                    if (!inCensorship) {
                        out.Append("\"");
                    }
                    inCensorship = false;
                    state = 3;
                }
                break;
            case 6: // custom tag name length
                count = c - 1;
                if (count <= 0 || count > len(bin) - ctx->pos) {
                    return false;
                }
                custom.Reset();
                state = 7;
                break;
            case 7: // custom tag name
                LitAppendUtf8(custom, c);
                if (--count == 0) {
                    tagName = str::DupTemp(ToStrTemp(custom));
                    out.Append(Str(tagName));
                    state = 3;
                }
                break;
            case 8: // custom attr name length
                count = c - 1;
                if (count <= 0 || count > len(bin) - ctx->pos) {
                    return false;
                }
                out.Append(" ");
                state = 9;
                break;
            case 9: // custom attr name
                LitEmitChar(out, c);
                if (--count == 0) {
                    out.Append("=");
                    state = 4;
                }
                break;
            case 10: // href length
                count = c - 1;
                if (count <= 0 || count > len(bin) - ctx->pos) {
                    return false;
                }
                href.Reset();
                state = 11;
                break;
            case 11: // href
                LitAppendUtf8(href, c);
                if (--count == 0) {
                    TempStr path = LitResolveHrefTemp(ctx, ToStrTemp(href));
                    out.Append("\"");
                    out.Append(Str(path));
                    out.Append("\"");
                    state = 3;
                }
                break;
        }
    }
    // EOF with unclosed tags is tolerated: parents still write their close tags
    return true;
}

static bool LitIsEntityStart(Str s, int pos) {
    // "&#123;", "&#x1f;" or "&name;"
    int i = pos + 1;
    if (i < len(s) && s.s[i] == '#') {
        i++;
        if (i < len(s) && (s.s[i] == 'x' || s.s[i] == 'X')) {
            i++;
        }
        int nDigits = 0;
        while (i < len(s) &&
               (str::IsDigit(s.s[i]) || (s.s[i] >= 'a' && s.s[i] <= 'f') || (s.s[i] >= 'A' && s.s[i] <= 'F'))) {
            i++;
            nDigits++;
        }
        return nDigits > 0 && i < len(s) && s.s[i] == ';';
    }
    int nChars = 0;
    while (i < len(s) && (str::IsAlNum(s.s[i]) || s.s[i] == '_' || s.s[i] == ':' || s.s[i] == '.' || s.s[i] == '-')) {
        i++;
        nChars++;
    }
    return nChars > 0 && i < len(s) && s.s[i] == ';';
}

// literal '&' => &amp;; '<<' / '>>' pairs written for literal angle brackets
// become &lt; / &gt;
static Str LitEscapeReserved(Str s) {
    str::Builder out;
    int i = 0;
    while (i < len(s)) {
        char c = s.s[i];
        if (c == '&') {
            if (LitIsEntityStart(s, i)) {
                out.AppendChar('&');
            } else {
                out.Append("&amp;");
            }
            i++;
            continue;
        }
        if (c == '<' && i + 1 < len(s) && s.s[i + 1] == '<') {
            // "<<" => &lt; except "<<!--" which stays a comment opener
            if (i + 4 < len(s) && str::Eq(Str(s.s + i + 2, 3), StrL("!--"))) {
                out.AppendChar('<');
                i += 2;
            } else {
                out.Append("&lt;");
                i += 2;
            }
            continue;
        }
        if (c == '>' && i + 1 < len(s) && s.s[i + 1] == '>') {
            // "-->>" keeps one '>' for the comment closer
            if (i >= 2 && s.s[i - 1] == '-' && s.s[i - 2] == '-') {
                out.AppendChar('>');
                i += 2;
            } else {
                out.Append("&gt;");
                i += 2;
            }
            continue;
        }
        out.AppendChar(c);
        i++;
    }
    return out.TakeStr();
}

// binary tokenized content -> markup text; returns {} on failure
static Str LitUnBinary(LitFile* lit, Str bin, Str path, bool isHtml, LitAtoms* atoms) {
    UnBinaryCtx ctx;
    ctx.bin = bin;
    ctx.lit = lit;
    ctx.isHtml = isHtml;
    ctx.atoms = atoms;
    TempStr dir = str::DupTemp(path);
    int lastSlash = -1;
    for (int i = 0; i < len(dir); i++) {
        if (dir.s[i] == '/') {
            lastSlash = i;
        }
    }
    ctx.dir = lastSlash >= 0 ? str::DupTemp(Str(dir.s, lastSlash)) : str::DupTemp(Str(""));
    if (!LitBinaryToText(&ctx, 0)) {
        return {};
    }
    Str raw = ToStrTemp(ctx.out);
    // strip leading whitespace
    while (len(raw) > 0 && str::IsWs(raw.s[0])) {
        raw.s++;
        raw.len--;
    }
    return LitEscapeReserved(raw);
}

//--- epub packaging

static const char* kOpfDecl =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
    "<!DOCTYPE package\n"
    "  PUBLIC \"+//ISBN 0-9673008-1-9//DTD OEB 1.0.1 Package//EN\"\n"
    "  \"http://openebook.org/dtds/oeb-1.0.1/oebpkg101.dtd\">\n";

static const char* kHtmlDecl =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
    "<!DOCTYPE html PUBLIC\n"
    " \"+//ISBN 0-9673008-1-9//DTD OEB 1.0.1 Document//EN\"\n"
    " \"http://openebook.org/dtds/oeb-1.0.1/oebdoc101.dtd\">\n";

static const char* kContainerXml =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">\n"
    "  <rootfiles>\n"
    "    <rootfile full-path=\"content.opf\" media-type=\"application/oebps-package+xml\"/>\n"
    "  </rootfiles>\n"
    "</container>\n";

Str LitToEpubConvert(Str litData) {
    LitFile lit;
    lit.d = litData;
    if (!LitParseHeader(&lit)) {
        logf("LitDoc: failed to parse header/directory\n");
        return {};
    }
    if (!LitParseSectionNames(&lit)) {
        logf("LitDoc: failed to parse section names\n");
        return {};
    }
    if (!LitParseManifest(&lit)) {
        logf("LitDoc: failed to parse manifest\n");
        return {};
    }
    if (!LitReadDrm(&lit)) {
        return {};
    }

    Str meta = lit.GetFile("/meta");
    if (str::IsNull(meta)) {
        logf("LitDoc: no /meta\n");
        return {};
    }
    Str opf = LitUnBinary(&lit, meta, "content.opf", false, nullptr);
    if (str::IsNull(opf)) {
        logf("LitDoc: failed to reconstruct OPF\n");
        return {};
    }

    str::Builder zipData;
    ZipCreator zc(zipData);
    bool ok = zc.AddFileData("mimetype", "application/epub+zip");
    ok &= zc.AddFileData("META-INF/container.xml", Str(kContainerXml));
    {
        TempStr full = str::JoinTemp(Str(kOpfDecl), opf);
        ok &= zc.AddFileData("content.opf", Str(full));
    }
    str::Free(opf);

    int nAdded = 0;
    for (LitManifestItem& item : lit.manifest) {
        if (!ok) {
            break;
        }
        if (item.isSpine) {
            Str raw = lit.GetFile(fmt("/data/%s/content", item.internal));
            if (str::IsNull(raw)) {
                logf("LitDoc: no content for spine item '%s'\n", item.internal);
                continue;
            }
            LitAtoms atoms;
            LitParseAtoms(&lit, item.internal, &atoms);
            Str html = LitUnBinary(&lit, raw, item.path, true, &atoms);
            if (str::IsNull(html)) {
                logf("LitDoc: failed to reconstruct '%s'\n", item.path);
                continue;
            }
            TempStr full = str::JoinTemp(Str(kHtmlDecl), html);
            ok &= zc.AddFileData(item.path, Str(full));
            str::Free(html);
            nAdded++;
        } else {
            Str data = lit.GetFile(fmt("/data/%s", item.internal));
            if (str::IsNull(data)) {
                continue;
            }
            ok &= zc.AddFileData(item.path, data);
            nAdded++;
        }
    }
    if (!ok || nAdded == 0) {
        logf("LitDoc: no files packaged\n");
        return {};
    }
    if (!zc.Finish()) {
        return {};
    }
    return zipData.TakeStr();
}
