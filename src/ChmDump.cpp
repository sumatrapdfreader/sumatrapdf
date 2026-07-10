/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include <chm.h>

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

static Str ChmCompressionName(bool isCompressed) {
    return isCompressed ? "compressed" : "uncompressed";
}

static Str ChmEntryKind(const chm_entry* e) {
    if (e->is_dir) {
        return "dir";
    }
    if (e->is_file) {
        return "file";
    }
    return "entry";
}

static Str ChmEntryClass(const chm_entry* e) {
    if (e->is_special) {
        return "special";
    }
    if (e->is_meta) {
        return "meta";
    }
    if (e->is_normal) {
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

static bool ReadChmObject(chm_ctx* ctx, chm_entry* e, ChmObjectReadResult* result) {
    if (e->length == 0) {
        CalcSHA1Digest({}, result->sha1);
        result->bytesRead = 0;
        result->sha1Valid = true;
        return true;
    }
    if (e->length > 512 * 1024 * 1024) {
        return false; // sanity cap for the dump tool
    }

    size_t n = (size_t)e->length;
    uint8_t* buf = (uint8_t*)malloc(n);
    if (!buf) {
        return false;
    }
    int64_t got = chm_read_entry(ctx, e, buf);
    if (got != (int64_t)e->length) {
        free(buf);
        result->bytesRead = got > 0 ? (uint64_t)got : 0;
        result->sha1Valid = false;
        return false;
    }
    CalcSHA1Digest(Str((char*)buf, (int)n), result->sha1);
    result->sha1Valid = true;
    result->bytesRead = (uint64_t)got;
    free(buf);
    return true;
}

struct ChmDumpCtx {
    int entries = 0;
    int files = 0;
    int dirs = 0;
    int unpackFailures = 0;
    uint64_t totalSize = 0;
};

static void ChmDumpEntry(chm_ctx* h, chm_entry* e, ChmDumpCtx* ctx) {
    if (!e->path || e->path[0] == 0) {
        return;
    }

    ctx->entries++;
    if (e->is_file) {
        ctx->files++;
        ctx->totalSize += e->length;
    }
    if (e->is_dir) {
        ctx->dirs++;
    }

    ChmObjectReadResult readResult;
    bool unpacked = true;
    if (e->is_file) {
        unpacked = ReadChmObject(h, e, &readResult);
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

    CliPrint(fmt("%s class=%s space=%s size=%llu read=%llu sha1=%s status=%s path=%s", Str(ChmEntryKind(e)),
                 Str(ChmEntryClass(e)), Str(ChmCompressionName(e->is_compressed)), (unsigned long long)e->length,
                 (unsigned long long)readResult.bytesRead, sha1Str, Str(unpacked ? "ok" : "failed"), Str(e->path)));
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
    chm_ctx* h = chm_ctx_new(nullptr, nullptr, nullptr, nullptr);
    if (!h || !chm_open(h, (const uint8_t*)data.s, (size_t)data.len)) {
        chm_ctx_free(h);
        str::Free(data);
        CliPrint("error: couldn't open CHM");
        return false;
    }

    chm_entry** entries = nullptr;
    int nEntries = chm_get_entries(h, &entries);

    ChmDumpCtx ctx;
    for (int i = 0; i < nEntries; i++) {
        ChmDumpEntry(h, entries[i], &ctx);
    }
    bool ok = nEntries > 0;
    CliPrint(fmt("summary entries=%d files=%d dirs=%d total-size=%llu unpack-failures=%d enumerate=%s", ctx.entries,
                 ctx.files, ctx.dirs, (unsigned long long)ctx.totalSize, ctx.unpackFailures,
                 Str(ok ? "ok" : "failed")));

    chm_ctx_free(h);
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
