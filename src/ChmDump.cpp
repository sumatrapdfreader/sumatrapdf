/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include <chm_lib.h>

#include "utils/FileUtil.h"
#include "utils/GuessFileType.h"
#include "utils/WinUtil.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "Flags.h"
#include "EbookBase.h"
#include "ChmFile.h"

static void CliWrite(const char* s, size_t n = 0) {
    if (!s) {
        return;
    }
    if (n == 0) {
        n = str::Len(s);
    }
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h && h != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(h, s, (DWORD)n, &written, nullptr);
        return;
    }
    fwrite(s, 1, n, stdout);
}

static void CliPrint(const char* s) {
    CliWrite(s);
    CliWrite("\n", 1);
}

static void CliPrintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    AutoFreeStr s = str::FmtV(fmt, args);
    va_end(args);
    CliPrint(s.Get());
}

static const char* ChmSpaceName(int space) {
    switch (space) {
        case CHM_UNCOMPRESSED:
            return "uncompressed";
        case CHM_COMPRESSED:
            return "compressed";
    }
    return "unknown";
}

static const char* ChmEntryKind(const chmUnitInfo* ui) {
    if (ui->flags & CHM_ENUMERATE_DIRS) {
        return "dir";
    }
    if (ui->flags & CHM_ENUMERATE_FILES) {
        return "file";
    }
    return "entry";
}

static const char* ChmEntryClass(const chmUnitInfo* ui) {
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

static bool ReadChmObject(struct chmFile* h, chmUnitInfo* ui, uint64_t* readOut) {
    const int64_t kBufSize = 64 * 1024;
    uint8_t* buf = (uint8_t*)malloc(kBufSize);
    if (!buf) {
        return false;
    }
    uint64_t pos = 0;
    while (pos < ui->length) {
        uint64_t remain = ui->length - pos;
        int64_t toRead = remain > (uint64_t)kBufSize ? kBufSize : (int64_t)remain;
        int64_t got = chm_retrieve_object(h, ui, buf, pos, toRead);
        if (got != toRead) {
            free(buf);
            *readOut = pos + (got > 0 ? (uint64_t)got : 0);
            return false;
        }
        pos += (uint64_t)got;
    }
    free(buf);
    *readOut = pos;
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
    if (str::IsEmpty(ui->path)) {
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

    uint64_t bytesRead = 0;
    bool unpacked = true;
    if ((ui->flags & CHM_ENUMERATE_FILES) && ui->length > 0) {
        unpacked = ReadChmObject(h, ui, &bytesRead);
        if (!unpacked) {
            ctx->unpackFailures++;
        }
    }

    CliPrintf("%s class=%s space=%s size=%llu read=%llu status=%s path=%s", ChmEntryKind(ui), ChmEntryClass(ui),
              ChmSpaceName(ui->space), (unsigned long long)ui->length, (unsigned long long)bytesRead,
              unpacked ? "ok" : "failed", ui->path);
    return CHM_ENUMERATOR_CONTINUE;
}

class ChmDumpTocVisitor : public EbookTocVisitor {
  public:
    bool any = false;

    void Visit(const char* name, const char* url, int level) override {
        any = true;
        CliPrintf("toc level=%d name=%s url=%s", level, name ? name : "", url ? url : "");
    }
};

class ChmDumpIndexVisitor : public EbookTocVisitor {
  public:
    bool any = false;

    void Visit(const char* name, const char* url, int level) override {
        any = true;
        CliPrintf("index level=%d name=%s url=%s", level, name ? name : "", url ? url : "");
    }
};

static bool DumpChmFileRaw(const char* path) {
    ByteSlice data = file::ReadFile(path);
    if (data.empty()) {
        CliPrint("error: couldn't read file");
        return false;
    }
    struct chmFile* h = chm_open((const char*)data.data(), data.size());
    if (!h) {
        data.Free();
        CliPrint("error: couldn't open CHM");
        return false;
    }

    ChmDumpCtx ctx;
    bool ok = chm_enumerate(h, CHM_ENUMERATE_ALL, ChmDumpEntry, &ctx) != 0;
    CliPrintf("summary entries=%d files=%d dirs=%d total-size=%llu unpack-failures=%d enumerate=%s", ctx.entries,
              ctx.files, ctx.dirs, (unsigned long long)ctx.totalSize, ctx.unpackFailures, ok ? "ok" : "failed");

    chm_close(h);
    data.Free();
    return ok && ctx.unpackFailures == 0;
}

static void DumpChmFileMetadata(const char* path) {
    ChmFile* doc = ChmFile::CreateFromFile(path);
    if (!doc) {
        CliPrint("metadata: unavailable");
        return;
    }
    CliPrintf("metadata title=%s", doc->title ? doc->title.Get() : "");
    CliPrintf("metadata creator=%s", doc->creator ? doc->creator.Get() : "");
    CliPrintf("metadata home=%s", doc->homePath ? doc->homePath.Get() : "");
    CliPrintf("metadata toc=%s", doc->tocPath ? doc->tocPath.Get() : "");
    CliPrintf("metadata index=%s", doc->indexPath ? doc->indexPath.Get() : "");
    CliPrintf("metadata codepage=%u", doc->codepage);

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

static bool DumpChmFile(const char* path) {
    CliPrintf("chm path=%s", path);
    bool ok = DumpChmFileRaw(path);
    DumpChmFileMetadata(path);
    CliPrint("end");
    return ok;
}

int DumpChm(const Flags& flags) {
    if (flags.fileNames.Size() == 0) {
        CliPrint("No file specified for -dump-chm");
        return 1;
    }

    bool ok = true;
    for (char* path : flags.fileNames) {
        if (!DumpChmFile(path)) {
            ok = false;
        }
    }
    return ok ? 0 : 1;
}
