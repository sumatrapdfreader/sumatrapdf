/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include <chm.h>

#include "base/Crypto.h"
#include "base/File.h"
#include "base/GuessFileType.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "Flags.h"
#include "EbookBase.h"
#include "ChmFile.h"
#include "ChmDump.h"

static void CliPrint(Str s) {
    WriteStdout(s);
    WriteStdout(StrL("\n"));
}

static Str ChmEntryKind(const chm_entry* e) {
    if (e->is_dir) {
        return StrL("dir");
    }
    if (e->is_file) {
        return StrL("file");
    }
    return StrL("entry");
}

static Str ChmEntryClass(const chm_entry* e) {
    if (e->is_special) {
        return StrL("special");
    }
    if (e->is_meta) {
        return StrL("meta");
    }
    if (e->is_normal) {
        return StrL("normal");
    }
    return StrL("unknown");
}

struct ChmObjectReadResult {
    uint64_t bytesRead = 0;
    u8 sha1[20]{};
};

static bool ReadChmObject(chm_ctx* ctx, chm_entry* e, ChmObjectReadResult* result) {
    if (e->length == 0) {
        CalcSHA1Digest({}, result->sha1);
        result->bytesRead = 0;
        return true;
    }
    if (e->length > 512ULL * 1024 * 1024) {
        return false; // sanity cap for the dump tool
    }

    size_t n = (size_t)e->length;
    AutoFree<uint8_t> buf((uint8_t*)malloc(n));
    if (!buf) {
        return false;
    }
    int64_t got = chm_read_entry(ctx, e, buf);
    if (got != (int64_t)e->length) {
        result->bytesRead = got > 0 ? (uint64_t)got : 0;
        return false;
    }
    CalcSHA1Digest(Str((char*)buf.Get(), (int)n), result->sha1);
    result->bytesRead = (uint64_t)got;
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

    Str sha1Str = StrL("-");
    if (e->is_file && unpacked) {
        sha1Str = str::MemToHexTemp(Str((char*)readResult.sha1, sizeofi(readResult.sha1)));
    }

    Str compression = e->is_compressed ? StrL("compressed") : StrL("uncompressed");
    CliPrint(fmt("%s class=%s space=%s size=%llu read=%llu sha1=%s status=%s path=%s", ChmEntryKind(e),
                 ChmEntryClass(e), compression, (unsigned long long)e->length, (unsigned long long)readResult.bytesRead,
                 sha1Str, Str(unpacked ? "ok" : "failed"), Str(e->path)));
}

struct ChmDumpTocVisitor : EbookTocVisitor {
    Str section;
    bool any = false;

    explicit ChmDumpTocVisitor(Str section) : section(section) {}

    void Visit(Str name, Str url, int level) override {
        any = true;
        CliPrint(fmt("%s level=%d name=%s url=%s", section, level, name, url));
    }
};

static bool DumpChmFileRaw(Str path) {
    Str data = file::ReadFile(path);
    AutoFree<char> freeData(data.s);
    if (len(data) == 0) {
        CliPrint(StrL("error: couldn't read file"));
        return false;
    }
    chm_ctx* h = chm_ctx_new(nullptr, nullptr, nullptr, nullptr);
    if (!h || !chm_open(h, (const uint8_t*)data.s, (size_t)data.len)) {
        chm_ctx_free(h);
        CliPrint(StrL("error: couldn't open CHM"));
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
    return ok && ctx.unpackFailures == 0;
}

static void DumpChmFileMetadata(Str path) {
    AutoDelete<ChmFile> doc(ChmFile::CreateFromFile(path));
    if (!doc) {
        CliPrint(StrL("metadata: unavailable"));
        return;
    }
    CliPrint(fmt("metadata title=%s", doc->title));
    CliPrint(fmt("metadata creator=%s", doc->creator));
    CliPrint(fmt("metadata home=%s", doc->homePath));
    CliPrint(fmt("metadata toc=%s", doc->tocPath));
    CliPrint(fmt("metadata index=%s", doc->indexPath));
    CliPrint(fmt("metadata codepage=%u", doc->codepage));

    ChmDumpTocVisitor toc(StrL("toc"));
    if (!doc->ParseToc(&toc) || !toc.any) {
        CliPrint(StrL("toc: none"));
    }
    ChmDumpTocVisitor index(StrL("index"));
    if (!doc->ParseIndex(&index) || !index.any) {
        CliPrint(StrL("index: none"));
    }
}

// Dump CHM metadata, file table, and TOC/index information to stdout.
// Returns 0 if every requested CHM opened, enumerated, and unpacked successfully.
int DumpChm(const Flags& flags) {
    if (len(flags.fileNames) == 0) {
        CliPrint(StrL("No file specified for -dump-chm"));
        return 1;
    }

    bool ok = true;
    for (Str path : flags.fileNames) {
        CliPrint(fmt("chm path=%s", path));
        ok &= DumpChmFileRaw(path);
        DumpChmFileMetadata(path);
        CliPrint(StrL("end"));
    }
    return ok ? 0 : 1;
}
