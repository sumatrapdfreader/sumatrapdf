/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/CmdLineArgs.h"
#include "base/Crypto.h"
#include "base/DirScan.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Timer.h"
#include "base/Win.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "DocController.h"
#include "DocProperties.h"
#include "EngineBase.h"
#include "EngineAll.h"

TempStr GetSumatraDataDirTemp();

// pdflatex / xelatex / lualatex do not read DVI. dvipdfmx from that
// install does; otherwise dvips plus Ghostscript.
Kind kindEngineDvi = "engineDvi";

constexpr i64 kDviCacheMaxAgeSec = 7LL * 24 * 60 * 60;
constexpr i64 kDviTmpMaxAgeSec = 24LL * 60 * 60;

struct DviTools {
    Str pdfmx;
    Str dvips;
    Str gs;
};

static DviTools gDviTools;
static bool gDviToolsInited = false;
// points at gDviTools.pdfmx or gDviTools.dvips; not owned
static Str gDviConvertTool;

static TempStr FindExeTemp(Str name) {
    WCHAR* nameW = CWStrTemp(name);
    WCHAR buf[4096];
    DWORD n = SearchPathW(nullptr, nameW, nullptr, dimof(buf), buf, nullptr);
    if (n == 0 || n >= dimof(buf)) {
        return {};
    }
    return ToUtf8Temp(buf);
}

static TempStr ExeInDirTemp(Str dir, Str exeName) {
    if (len(dir) == 0) {
        return {};
    }
    TempStr path = path::JoinTemp(dir, exeName);
    if (!file::Exists(path)) {
        return {};
    }
    return path;
}

// PATH first, then the directory of pdflatex / xelatex / lualatex / latex.
static TempStr FindTexExeTemp(Str exeName) {
    TempStr found = FindExeTemp(exeName);
    if (len(found) > 0) {
        return found;
    }
    Str bins[] = {StrL("pdflatex.exe"), StrL("xelatex.exe"), StrL("lualatex.exe"), StrL("latex.exe")};
    for (Str bin : bins) {
        TempStr binPath = FindExeTemp(bin);
        if (len(binPath) == 0) {
            continue;
        }
        TempStr beside = ExeInDirTemp(path::GetDirTemp(binPath), exeName);
        if (len(beside) > 0) {
            return beside;
        }
    }
    return {};
}

static void InitDviTools() {
    if (gDviToolsInited) {
        return;
    }
    gDviToolsInited = true;

    TempStr pdfmx = FindTexExeTemp(StrL("dvipdfmx.exe"));
    if (len(pdfmx) == 0) {
        pdfmx = FindTexExeTemp(StrL("xdvipdfmx.exe"));
    }
    if (len(pdfmx) > 0) {
        gDviTools.pdfmx = str::Dup(pdfmx);
    }

    TempStr dvips = FindTexExeTemp(StrL("dvips.exe"));
    if (len(dvips) > 0) {
        gDviTools.dvips = str::Dup(dvips);
    }
    TempStr gs = GetGhostscriptPathTemp();
    if (len(gs) > 0) {
        gDviTools.gs = str::Dup(gs);
    }
}

static bool DviToolsReady() {
    InitDviTools();
    if (len(gDviTools.pdfmx) > 0) {
        return true;
    }
    return len(gDviTools.dvips) > 0 && len(gDviTools.gs) > 0;
}

bool IsEngineDviAvailable() {
    return DviToolsReady();
}

bool IsEngineDviSupportedFileType(FileType kind) {
    return kind == FileType::Dvi && IsEngineDviAvailable();
}

static DWORD DviConvertTimeoutMs() {
    if (GetEnvironmentVariableW(L"SUMATRAPDF_NO_GHOSTSCRIPT_TIMEOUT", nullptr, 0)) {
        return INFINITE;
    }
    return 120000;
}

static bool RunDviCmd(Str cmdLine, Str workDir) {
    HANDLE process = LaunchProcessInDir(cmdLine, workDir, CREATE_NO_WINDOW);
    if (!process) {
        logf("dvi: CreateProcess failed: %s\n", cmdLine);
        return false;
    }
    DWORD timeoutMs = DviConvertTimeoutMs();
    DWORD wait = WaitForSingleObject(process, timeoutMs);
    DWORD exitCode = EXIT_FAILURE;
    GetExitCodeProcess(process, &exitCode);
    if (wait != WAIT_OBJECT_0 || exitCode == STILL_ACTIVE) {
        TerminateProcess(process, 1);
        CloseHandle(process);
        logf("dvi: timed out: %s\n", cmdLine);
        return false;
    }
    CloseHandle(process);
    if (exitCode != EXIT_SUCCESS) {
        logf("dvi: exit %u: %s\n", exitCode, cmdLine);
        return false;
    }
    return true;
}

struct DviTmpFile {
    TempStr path;
    ~DviTmpFile() {
        if (len(path) > 0) {
            file::Delete(path);
        }
    }
};

static bool PdfNonEmpty(Str path) {
    return file::GetSize(path) > 8;
}

static bool ConvertWithPdfmx(Str src, Str dst, Str workDir) {
    gDviConvertTool = gDviTools.pdfmx;
    TempStr cmd =
        fmt("%s -o %s %s", QuoteCmdLineArgTemp(gDviTools.pdfmx), QuoteCmdLineArgTemp(dst), QuoteCmdLineArgTemp(src));
    if (!RunDviCmd(cmd, workDir) || !PdfNonEmpty(dst)) {
        file::Delete(dst);
        return false;
    }
    return true;
}

static bool ConvertWithDvips(Str src, Str dst, Str workDir) {
    gDviConvertTool = gDviTools.dvips;
    DviTmpFile ps;
    ps.path = GetTempFilePathTemp(StrL("Dvi"));
    if (len(ps.path) == 0) {
        return false;
    }
    TempStr dvipsCmd = fmt("%s -q -o %s %s", QuoteCmdLineArgTemp(gDviTools.dvips), QuoteCmdLineArgTemp(ps.path),
                           QuoteCmdLineArgTemp(src));
    if (!RunDviCmd(dvipsCmd, workDir)) {
        return false;
    }
    TempStr gsCmd = fmt("%s -q -dSAFER -dNOPAUSE -dBATCH -sDEVICE=pdfwrite -sOutputFile=%s -f %s",
                        QuoteCmdLineArgTemp(gDviTools.gs), QuoteCmdLineArgTemp(dst), QuoteCmdLineArgTemp(ps.path));
    if (!RunDviCmd(gsCmd, workDir) || !PdfNonEmpty(dst)) {
        file::Delete(dst);
        return false;
    }
    return true;
}

static bool ConvertDviToPdf(Str src, Str dst) {
    TempStr workDir = path::GetDirTemp(src);
    if (len(gDviTools.pdfmx) > 0 && ConvertWithPdfmx(src, dst, workDir)) {
        return true;
    }
    if (len(gDviTools.dvips) > 0 && len(gDviTools.gs) > 0) {
        return ConvertWithDvips(src, dst, workDir);
    }
    return false;
}

static TempStr DviCacheDirTemp() {
    TempStr dataDir = GetSumatraDataDirTemp();
    if (len(dataDir) == 0 || path::IsOnNetworkDrive(dataDir)) {
        return {};
    }
    return path::JoinTemp(dataDir, StrL("dvi-cache"));
}

static TempStr Md5HexTemp(Str data) {
    u8 digest[16]{};
    CalcMD5Digest(data, digest);
    return str::MemToHexTemp(Str((const char*)digest, (int)sizeof(digest)));
}

// md5(path|mtime).pdf — a new mtime is a new file, so a locked older PDF
// does not have to be replaced.
static TempStr DviPdfNameTemp(Str src, FILETIME mtime) {
    TempStr key = fmt("%s|%u|%u", src, mtime.dwHighDateTime, mtime.dwLowDateTime);
    return str::JoinTemp(Md5HexTemp(key), StrL(".pdf"));
}

static TempStr DviMetaNameTemp(Str src) {
    return str::JoinTemp(Md5HexTemp(src), StrL(".meta"));
}

static bool TakeLine(Str& rest, Str& line) {
    if (len(rest) == 0) {
        return false;
    }
    int nl = -1;
    for (int i = 0; i < len(rest); i++) {
        if (rest.s[i] == '\n') {
            nl = i;
            break;
        }
    }
    if (nl < 0) {
        line = rest;
        rest = {};
        return len(line) > 0;
    }
    line = Str(rest.s, nl);
    if (len(line) > 0 && line.s[line.len - 1] == '\r') {
        line.len--;
    }
    int skip = nl + 1;
    rest.s += skip;
    rest.len -= skip;
    return true;
}

static bool ParseU32Pair(Str s, DWORD& hi, DWORD& lo) {
    const char* p = s.s;
    const char* end = s.s + s.len;
    auto readOne = [&](DWORD& out) -> bool {
        if (p >= end || *p < '0' || *p > '9') {
            return false;
        }
        u64 v = 0;
        while (p < end && *p >= '0' && *p <= '9') {
            v = (v * 10) + (u64)(*p - '0');
            if (v > 0xffffffffu) {
                return false;
            }
            p++;
        }
        out = (DWORD)v;
        return true;
    };
    if (!readOne(hi)) {
        return false;
    }
    if (p >= end || *p != ' ') {
        return false;
    }
    p++;
    if (!readOne(lo)) {
        return false;
    }
    return p == end;
}

// hex md5 plus ".pdf", nothing else, so a meta file cannot point outside the cache dir
static bool IsSafePdfName(Str name) {
    if (len(name) != 36 || !str::EndsWithI(name, StrL(".pdf"))) {
        return false;
    }
    for (int i = 0; i < len(name); i++) {
        char c = name.s[i];
        bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!hex && c != '.') {
            return false;
        }
    }
    return true;
}

// line 1: mtime as "<high> <low>"
// line 2: cached PDF file name
// line 3: original path
static bool ParseDviMeta(Str data, FILETIME& mtime, Str& pdfName, Str& src) {
    Str rest = data;
    Str line;
    if (!TakeLine(rest, line)) {
        return false;
    }
    DWORD hi = 0;
    DWORD lo = 0;
    if (!ParseU32Pair(line, hi, lo)) {
        return false;
    }
    if (!TakeLine(rest, pdfName) || !IsSafePdfName(pdfName)) {
        return false;
    }
    if (!TakeLine(rest, src) || len(src) == 0) {
        return false;
    }
    mtime.dwHighDateTime = hi;
    mtime.dwLowDateTime = lo;
    return true;
}

static bool MetaMatches(Str metaPath, Str src, FILETIME mtime, Str pdfName) {
    Str data = file::ReadFile(metaPath);
    if (len(data) == 0) {
        return false;
    }
    FILETIME got{};
    Str gotPdf;
    Str gotSrc;
    bool ok = ParseDviMeta(data, got, gotPdf, gotSrc) && FileTimeEq(got, mtime) && str::Eq(gotPdf, pdfName) &&
              str::Eq(gotSrc, src);
    str::Free(data);
    return ok;
}

static bool WriteDviMeta(Str metaPath, Str src, FILETIME mtime, Str pdfName) {
    TempStr body = fmt("%u %u\n%s\n%s\n", mtime.dwHighDateTime, mtime.dwLowDateTime, pdfName, src);
    return file::WriteFile(metaPath, body);
}

// <data>/dvi-cache/<md5(path)>.meta records the source path, its mtime, and
// the PDF file name. A hit bumps the PDF's access time so the week sweep
// keeps it. Any failure falls through to converting again.
static TempStr EnsureDviPdf(Str src) {
    if (!DviToolsReady()) {
        return {};
    }
    FILETIME mtime = file::GetModificationTime(src);
    if (mtime.dwLowDateTime == 0 && mtime.dwHighDateTime == 0) {
        return {};
    }

    TempStr dir = DviCacheDirTemp();
    if (len(dir) == 0 || !dir::CreateAll(dir)) {
        logf("dvi: cache dir unavailable\n");
        return {};
    }

    TempStr pdfName = DviPdfNameTemp(src, mtime);
    TempStr pdfPath = path::JoinTemp(dir, pdfName);
    TempStr metaPath = path::JoinTemp(dir, DviMetaNameTemp(src));
    if (file::Exists(pdfPath) && MetaMatches(metaPath, src, mtime, pdfName)) {
        FILETIME now{};
        GetSystemTimeAsFileTime(&now);
        file::SetAccessTime(pdfPath, now);
        logf("dvi cache hit pdf='%s'\n", pdfPath);
        return pdfPath;
    }

    TempStr tmpPath = str::JoinTemp(pdfPath, StrL(".tmp"));
    auto t0 = TimeGet();
    if (!ConvertDviToPdf(src, tmpPath)) {
        file::Delete(tmpPath);
        return {};
    }
    if (!file::RenameReplace(pdfPath, tmpPath)) {
        logf("dvi: rename '%s' -> '%s' failed\n", tmpPath, pdfPath);
        file::Delete(tmpPath);
        return {};
    }
    if (!WriteDviMeta(metaPath, src, mtime, pdfName)) {
        logf("dvi: write meta '%s' failed\n", metaPath);
    }
    logf("dvi convert src='%s' pdf='%s' meta='%s' tool='%s' in %.2f ms\n", src, pdfPath, metaPath, gDviConvertTool,
         TimeSinceInMs(t0));
    return pdfPath;
}

static i64 FileAgeSec(FILETIME ft) {
    FILETIME nowFt{};
    GetSystemTimeAsFileTime(&nowFt);
    ULARGE_INTEGER now{};
    now.LowPart = nowFt.dwLowDateTime;
    now.HighPart = nowFt.dwHighDateTime;
    ULARGE_INTEGER t{};
    t.LowPart = ft.dwLowDateTime;
    t.HighPart = ft.dwHighDateTime;
    if (t.QuadPart == 0 || t.QuadPart > now.QuadPart) {
        return 0;
    }
    return (i64)((now.QuadPart - t.QuadPart) / 10000000ULL);
}

static void DeleteAged(Str path, i64 ageSec, i64 maxAgeSec) {
    if (ageSec < maxAgeSec) {
        return;
    }
    bool ok = file::Delete(path);
    logf("DeleteStaleDviCache: delete '%s' (age %lld days) -> %d\n", path, (long long)(ageSec / (24LL * 60 * 60)),
         (int)ok);
}

void DeleteStaleDviCache() {
    TempStr dir = DviCacheDirTemp();
    if (len(dir) == 0 || path::GetType(dir) != path::Type::Dir) {
        return;
    }

    DirIter di{dir};
    di.includeFiles = true;
    di.includeDirs = false;
    for (DirIterEntry* de : di) {
        bool isTmp = str::EndsWithI(de->name, StrL(".pdf.tmp"));
        bool isPdf = !isTmp && str::EndsWithI(de->name, StrL(".pdf"));
        if (!isPdf && !isTmp) {
            continue;
        }
        i64 ageSec = FileAgeSec(de->accessTime);
        DeleteAged(de->filePath, ageSec, isTmp ? kDviTmpMaxAgeSec : kDviCacheMaxAgeSec);
    }

    DirIter metas{dir};
    metas.includeFiles = true;
    metas.includeDirs = false;
    for (DirIterEntry* de : metas) {
        if (!str::EndsWithI(de->name, StrL(".meta"))) {
            continue;
        }
        Str data = file::ReadFile(de->filePath);
        FILETIME mtime{};
        Str pdfName;
        Str src;
        bool parsed = len(data) > 0 && ParseDviMeta(data, mtime, pdfName, src);
        bool pdfGone = true;
        if (parsed) {
            TempStr pdfPath = path::JoinTemp(dir, pdfName);
            pdfGone = !file::Exists(pdfPath);
        }
        str::Free(data);
        if (!pdfGone) {
            continue;
        }
        bool ok = file::Delete(de->filePath);
        logf("DeleteStaleDviCache: delete meta '%s' -> %d\n", de->filePath, (int)ok);
    }
}

class EngineDvi : public EngineBase {
  public:
    EngineDvi() {
        kind = kindEngineDvi;
        defaultExt = str::Dup(StrL(".dvi"));
    }

    ~EngineDvi() override {
        if (pdfEngine) {
            pdfEngine->Release();
        }
        str::Free(cachedPdf);
    }

    EngineBase* Clone() override {
        if (len(cachedPdf) == 0 || !file::Exists(cachedPdf)) {
            return nullptr;
        }
        EngineBase* inner = CreateEngineMupdfFromFile(cachedPdf, FileType::PDF, DpiGet(), nullptr);
        if (!inner) {
            return nullptr;
        }
        EngineDvi* clone = new EngineDvi();
        if (FilePath()) {
            clone->SetFilePath(FilePath());
        }
        clone->pdfEngine = inner;
        clone->cachedPdf = str::Dup(cachedPdf);
        clone->CopyStateFromPdfEngine();
        return clone;
    }

    RectF PageMediabox(int pageNo) override { return pdfEngine->PageMediabox(pageNo); }

    RectF PageContentBox(int pageNo, RenderTarget target = RenderTarget::View) override {
        return pdfEngine->PageContentBox(pageNo, target);
    }

    Pixmap* RenderPage(RenderPageArgs& args) override { return pdfEngine->RenderPage(args); }

    RectF Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse = false) override {
        return pdfEngine->Transform(rect, pageNo, zoom, rotation, inverse);
    }

    Str GetFileData() override { return file::ReadFile(FilePath()); }

    // saving as .pdf writes the cached conversion; anything else copies the DVI
    bool SaveFileAs(Str dstPath) override {
        if (str::EndsWithI(dstPath, StrL(".pdf")) && len(cachedPdf) > 0) {
            return file::Copy(dstPath, cachedPdf, false);
        }
        Str srcPath = FilePath();
        if (len(srcPath) == 0) {
            return false;
        }
        return file::Copy(dstPath, srcPath, false);
    }

    PageText ExtractPageText(int pageNo) override { return pdfEngine->ExtractPageText(pageNo); }

    bool HasClipOptimizations(int pageNo) override { return pdfEngine->HasClipOptimizations(pageNo); }

    TempStr GetPropertyTemp(DocProp prop) override {
        if (!pdfEngine) {
            return {};
        }
        static const DocProp toOmit[] = {DocProp::CreationDate, DocProp::ModificationDate, DocProp::PdfVersion,
                                         DocProp::PdfProducer,  DocProp::PdfFileStructure, DocProp::None};
        for (DocProp omit : toOmit) {
            if (omit == DocProp::None) {
                break;
            }
            if (omit == prop) {
                return {};
            }
        }
        return pdfEngine->GetPropertyTemp(prop);
    }

    bool BenchLoadPage(int pageNo) override { return pdfEngine->BenchLoadPage(pageNo); }

    Vec<IPageElement*> GetElements(int pageNo) override { return pdfEngine->GetElements(pageNo); }

    RenderedBitmap* GetImageForPageElement(IPageElement* ipel) override {
        return pdfEngine->GetImageForPageElement(ipel);
    }

    Str GetImageDataForPageElement(IPageElement* ipel) override { return pdfEngine->GetImageDataForPageElement(ipel); }

    bool TryGetElements(int pageNo, Vec<IPageElement*>* out) override { return pdfEngine->TryGetElements(pageNo, out); }

    bool TryExtractPageText(int pageNo, PageText* out) override { return pdfEngine->TryExtractPageText(pageNo, out); }

    void ReleaseTextExtractionThreadContext() override { pdfEngine->ReleaseTextExtractionThreadContext(); }

    void GetPdfPageBoxes(int pageNo, Vec<PdfPageBox>& out) override { pdfEngine->GetPdfPageBoxes(pageNo, out); }

    int GetOpenActionPageNo() override { return pdfEngine->GetOpenActionPageNo(); }

    Location ResolveDest(IPageDestination* dest) override { return pdfEngine->ResolveDest(dest); }

    TempStr GetPageLabeTemp(int pageNo) const override { return pdfEngine->GetPageLabeTemp(pageNo); }

    int GetPageByLabel(Str label) const override { return pdfEngine->GetPageByLabel(label); }

    void GetBitmapRecolorSkipRects(int pageNo, float zoom, int rotation, const RectF& renderPageRect, Size bmpSize,
                                   Vec<Rect>& skipRects) override {
        pdfEngine->GetBitmapRecolorSkipRects(pageNo, zoom, rotation, renderPageRect, bmpSize, skipRects);
    }

    IPageElement* GetElementAtPos(int pageNo, PointF pt) override { return pdfEngine->GetElementAtPos(pageNo, pt); }

    bool HandleLink(IPageDestination* dest, ILinkHandler* lh) override { return pdfEngine->HandleLink(dest, lh); }

    IPageDestination* GetNamedDest(Str name) override { return pdfEngine->GetNamedDest(name); }

    TocTree* GetToc() override { return pdfEngine->GetToc(); }

    EngineBase* pdfEngine = nullptr;
    Str cachedPdf;

    void CopyStateFromPdfEngine() {
        preferredLayout = pdfEngine->preferredLayout;
        fileDPI = pdfEngine->GetFileDPI();
        allowsPrinting = pdfEngine->AllowsPrinting();
        allowsCopyingText = pdfEngine->AllowsCopyingText();
        decryptionKey = str::Dup(arena, pdfEngine->decryptionKey);
        pageCount = pdfEngine->PageCount();
        hasPageLabels = pdfEngine->HasPageLabels();
        logicalPageCount = pdfEngine->LogicalPageCount();
    }

    bool Load(Str fileName) {
        pageCount = 0;
        if (len(fileName) == 0) {
            return false;
        }
        SetFilePath(fileName);
        TempStr pdf = EnsureDviPdf(fileName);
        if (len(pdf) == 0) {
            return false;
        }
        cachedPdf = str::Dup(pdf);
        pdfEngine = CreateEngineMupdfFromFile(cachedPdf, FileType::PDF, DpiGet(), nullptr);
        if (!pdfEngine) {
            return false;
        }
        CopyStateFromPdfEngine();
        return true;
    }
};

EngineBase* CreateEngineDviFromFile(Str fileName) {
    EngineDvi* engine = new EngineDvi();
    if (!engine->Load(fileName)) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return engine;
}
