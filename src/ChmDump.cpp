/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include <chm_lib.h>
#include <wincrypt.h>

#include "base/Crypto.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Win.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "Flags.h"
#include "EbookBase.h"
#include "ChmFile.h"

static void CliWrite(Str s, int n = 0) {
    if (!s) {
        return;
    }
    if (n == 0) {
        n = s.len;
    }
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h && h != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(h, s.s, (DWORD)n, &written, nullptr);
        return;
    }
    fwrite(s.s, 1, (size_t)n, stdout);
}

static void CliPrint(Str s) {
    CliWrite(s);
    CliWrite("\n", 1);
}

static Str ChmSpaceName(int space) {
    switch (space) {
        case CHM_UNCOMPRESSED:
            return "uncompressed";
        case CHM_COMPRESSED:
            return "compressed";
    }
    return "unknown";
}

static Str ChmEntryKind(const chmUnitInfo* ui) {
    if (ui->flags & CHM_ENUMERATE_DIRS) {
        return "dir";
    }
    if (ui->flags & CHM_ENUMERATE_FILES) {
        return "file";
    }
    return "entry";
}

static Str ChmEntryClass(const chmUnitInfo* ui) {
    if (ui->flags & CHM_ENUMERATE_SPECIAL) {
        return "special";
    }
    if (ui->flags & CHM_ENUMERATE_META) {
        return "meta";
    }
    if (ui->flags & CHM_ENUMERATE_NORMAL) {
        return "normal";
    }
    return "unknown";
}

static void FormatSha1Hex(const u8 digest[20], char out[41]) {
    for (int i = 0; i < 20; i++) {
        sprintf_s(&out[i * 2], 3, "%02x", digest[i]);
    }
    out[40] = '\0';
}

struct ChmObjectReadResult {
    uint64_t bytesRead = 0;
    u8 sha1[20]{};
    bool sha1Valid = false;
};

static bool ReadChmObject(struct chmFile* h, chmUnitInfo* ui, ChmObjectReadResult* result) {
    if (ui->length == 0) {
        CalcSHA1Digest({}, result->sha1);
        result->bytesRead = 0;
        result->sha1Valid = true;
        return true;
    }

    const int64_t kBufSize = 64 * 1024;
    uint8_t* buf = (uint8_t*)malloc(kBufSize);
    if (!buf) {
        return false;
    }

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    bool hashOk = CryptAcquireContextW(&hProv, nullptr, MS_DEF_PROV, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) &&
                  CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash);

    uint64_t pos = 0;
    while (pos < ui->length) {
        uint64_t remain = ui->length - pos;
        int64_t toRead = remain > (uint64_t)kBufSize ? kBufSize : (int64_t)remain;
        int64_t got = chm_retrieve_object(h, ui, buf, pos, toRead);
        if (got != toRead) {
            if (hashOk) {
                CryptDestroyHash(hHash);
                CryptReleaseContext(hProv, 0);
            }
            free(buf);
            result->bytesRead = pos + (got > 0 ? (uint64_t)got : 0);
            result->sha1Valid = false;
            return false;
        }
        if (hashOk) {
            CryptHashData(hHash, buf, (DWORD)got, 0);
        }
        pos += (uint64_t)got;
    }

    if (hashOk) {
        DWORD hashLen = 20;
        CryptGetHashParam(hHash, HP_HASHVAL, result->sha1, &hashLen, 0);
        result->sha1Valid = true;
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
    }

    free(buf);
    result->bytesRead = pos;
    return true;
}

struct ChmDumpCtx {
    int entries = 0;
    int files = 0;
    int dirs = 0;
    int unpackFailures = 0;
    uint64_t totalSize = 0;
};

static int ChmDumpEntry(struct chmFile* h, struct chmUnitInfo* ui, void* data) {
    ChmDumpCtx* ctx = (ChmDumpCtx*)data;
    if (len(ui->path) == 0) {
        return CHM_ENUMERATOR_CONTINUE;
    }

    ctx->entries++;
    if (ui->flags & CHM_ENUMERATE_FILES) {
        ctx->files++;
        ctx->totalSize += ui->length;
    }
    if (ui->flags & CHM_ENUMERATE_DIRS) {
        ctx->dirs++;
    }

    ChmObjectReadResult readResult;
    bool unpacked = true;
    if (ui->flags & CHM_ENUMERATE_FILES) {
        unpacked = ReadChmObject(h, ui, &readResult);
        if (!unpacked) {
            ctx->unpackFailures++;
        }
    }

    char sha1Hex[41]{};
    Str sha1Str = "-";
    if (readResult.sha1Valid) {
        FormatSha1Hex(readResult.sha1, sha1Hex);
        sha1Str = Str(sha1Hex, 40);
    }

    CliPrint(fmt("%s class=%s space=%s size=%llu read=%llu sha1=%s status=%s path=%s", Str(ChmEntryKind(ui)),
                 Str(ChmEntryClass(ui)), Str(ChmSpaceName(ui->space)), (unsigned long long)ui->length,
                 (unsigned long long)readResult.bytesRead, sha1Str, Str(unpacked ? "ok" : "failed"), Str(ui->path)));
    return CHM_ENUMERATOR_CONTINUE;
}

struct ChmDumpTocVisitor : EbookTocVisitor {
  public:
    bool any = false;

    void Visit(Str name, Str url, int level) override {
        any = true;
        CliPrint(fmt("toc level=%d name=%s url=%s", level, name, url));
    }
};

struct ChmDumpIndexVisitor : EbookTocVisitor {
  public:
    bool any = false;

    void Visit(Str name, Str url, int level) override {
        any = true;
        CliPrint(fmt("index level=%d name=%s url=%s", level, name, url));
    }
};

static bool DumpChmFileRaw(Str path) {
    Str data = file::ReadFile(path);
    if (len(data) == 0) {
        CliPrint("error: couldn't read file");
        return false;
    }
    struct chmFile* h = chm_open(data.s, (size_t)data.len);
    if (!h) {
        str::Free(data);
        CliPrint("error: couldn't open CHM");
        return false;
    }

    ChmDumpCtx ctx;
    bool ok = chm_enumerate(h, CHM_ENUMERATE_ALL, ChmDumpEntry, &ctx) != 0;
    CliPrint(fmt("summary entries=%d files=%d dirs=%d total-size=%llu unpack-failures=%d enumerate=%s", ctx.entries,
                 ctx.files, ctx.dirs, (unsigned long long)ctx.totalSize, ctx.unpackFailures,
                 Str(ok ? "ok" : "failed")));

    chm_close(h);
    str::Free(data);
    return ok && ctx.unpackFailures == 0;
}

static void DumpChmFileMetadata(Str path) {
    ChmFile* doc = ChmFile::CreateFromFile(path);
    if (!doc) {
        CliPrint("metadata: unavailable");
        return;
    }
    CliPrint(fmt("metadata title=%s", Str(len(doc->title) > 0 ? doc->title.s : "")));
    CliPrint(fmt("metadata creator=%s", Str(len(doc->creator) > 0 ? doc->creator.s : "")));
    CliPrint(fmt("metadata home=%s", Str(len(doc->homePath) > 0 ? doc->homePath.s : "")));
    CliPrint(fmt("metadata toc=%s", Str(len(doc->tocPath) > 0 ? doc->tocPath.s : "")));
    CliPrint(fmt("metadata index=%s", Str(len(doc->indexPath) > 0 ? doc->indexPath.s : "")));
    CliPrint(fmt("metadata codepage=%u", doc->codepage));

    ChmDumpTocVisitor toc;
    if (!doc->ParseToc(&toc) || !toc.any) {
        CliPrint("toc: none");
    }
    ChmDumpIndexVisitor index;
    if (!doc->ParseIndex(&index) || !index.any) {
        CliPrint("index: none");
    }

    delete doc;
}

static bool DumpChmFile(Str path) {
    CliPrint(fmt("chm path=%s", path));
    bool ok = DumpChmFileRaw(path);
    DumpChmFileMetadata(path);
    CliPrint("end");
    return ok;
}

int DumpChm(const Flags& flags) {
    if (len(flags.fileNames) == 0) {
        CliPrint("No file specified for -dump-chm");
        return 1;
    }

    bool ok = true;
    for (Str path : flags.fileNames) {
        if (!DumpChmFile(path)) {
            ok = false;
        }
    }
    return ok ? 0 : 1;
}
