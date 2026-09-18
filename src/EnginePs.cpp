/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include <zlib.h>
#include "base/AutoWin.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Win.h"

#include "gui/UIModels.h"

#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"

Kind kindEnginePostScript = "enginePostScript";

static TempStr GetGhostscriptPathTemp() {
    static const Str gsProducts[] = {
        StrL("AFPL Ghostscript"),
        StrL("Aladdin Ghostscript"),
        StrL("GPL Ghostscript"),
        StrL("GNU Ghostscript"),
    };

    // find all installed Ghostscript versions
    StrVec versions;
    REGSAM access = KEY_READ | KEY_WOW64_32KEY;
TryAgain64Bit:
    for (Str gsProd : gsProducts) {
        HKEY hkey;
        TempStr keyName = str::JoinTemp(StrL("Software\\"), gsProd);
        WCHAR* keyNameW = CWStrTemp(keyName);
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyNameW, 0, access, &hkey) != ERROR_SUCCESS) {
            continue;
        }
        WCHAR subkey[32];
        for (DWORD ix = 0; RegEnumKey(hkey, ix, subkey, dimof(subkey)) == ERROR_SUCCESS; ix++) {
            TempStr ver = ToUtf8Temp(subkey);
            versions.Append(ver);
        }
        RegCloseKey(hkey);
    }
    if ((access & KEY_WOW64_32KEY)) {
        // also look for 64-bit Ghostscript versions under 64-bit Windows
        access = KEY_READ | KEY_WOW64_64KEY;
#ifndef _WIN64
        // (unless this is 32-bit Windows)
        if (IsRunningInWow64())
#endif
            goto TryAgain64Bit;
    }
    SortNatural(&versions);

    // return the path to the newest installation
    int nVers = len(versions);
    for (int i = nVers; i > 0; i--) {
        for (Str gsProd : gsProducts) {
            Str ver = versions[i - 1];
            TempStr keyName = fmt("Software\\%s\\%s", gsProd, ver);
            TempStr gsDLL = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, StrL("GS_DLL"));
            if (len(gsDLL) == 0) {
                continue;
            }
            TempStr dir = path::GetDirTemp(gsDLL);
            TempStr exe = path::JoinTemp(dir, StrL("gswin32c.exe"));
            if (file::Exists(exe)) {
                return exe;
            }
            exe = path::JoinTemp(dir, StrL("gswin64c.exe"));
            if (file::Exists(exe)) {
                return exe;
            }
        }
    }

    // if Ghostscript isn't found in the Registry, try finding it in the %PATH%
    DWORD size = GetEnvironmentVariableW(L"PATH", nullptr, 0);
    TempWStr envpathW = WStr(AllocArrayTemp<WCHAR>((int)size + 1), (int)size + 1);
    if (size == 0) {
        return {};
    }
    GetEnvironmentVariableW(L"PATH", envpathW.s, size);
    TempStr envPath = ToUtf8Temp(envpathW);
    StrVec paths;
    Split(&paths, envPath, StrL(";"), true);
    for (Str path : paths) {
        TempStr exe = path::JoinTemp(path, StrL("gswin32c.exe"));
        if (!file::Exists(exe)) {
            exe = path::JoinTemp(path, StrL("gswin64c.exe"));
        }
        if (!file::Exists(exe)) {
            continue;
        }
        return exe;
    }
    return {};
}

struct AutoDeleteFile {
    Str filePath;

    explicit AutoDeleteFile(Str path) { filePath = str::Dup(path); }
    ~AutoDeleteFile() {
        if (filePath) {
            file::Delete(filePath);
        }
        str::Free(filePath);
    }
};

// pdfDataOut gets the converted PDF: the temp file is gone by the time we
// return and mupdf can't re-read its stream, so it's the only copy a clone
// (or "save as PDF") can use. Caller owns it.
static EngineBase* ps2pdf(Str path, Str* pdfDataOut) {
    // TODO: read from gswin32c's stdout instead of using a TEMP file
    TempStr shortPath = path::ShortPathTemp(path);
    TempStr tmpFile = GetTempFilePathTemp(StrL("PsE"));
    AutoDeleteFile tmpFileScope(tmpFile);
    TempStr gswin32c = GetGhostscriptPathTemp();
    if (len(shortPath) == 0 || len(tmpFile) == 0 || len(gswin32c) == 0) {
        return nullptr;
    }

    // Ghostscript 9.54+ removed .setpdfwrite, so we no longer pass PageSize via
    // -c ".setpdfwrite << /PageSize ... >> setpagedevice" (see issues #1923).
    TempStr cmdLine =
        fmt("\"%s\" -q -dSAFER -dNOPAUSE -dBATCH -dEPSCrop -sOutputFile=\"%s\" -sDEVICE=pdfwrite "
            "-f \"%s\"",
            gswin32c, tmpFile, shortPath);

    {
        TempStr fileName = path::GetBaseNameTemp(StrL(__FILE__));
        TempStr tmpFileName = path::GetBaseNameTemp(tmpFile);
        logf("- %s:%d: using '%s' for creating '%%TEMP%%\\%s'\n", fileName, __LINE__, gswin32c, tmpFileName);
    }

    // TODO: the PS-to-PDF conversion can hang the UI for several seconds
    HANDLE process = LaunchProcessInDir(cmdLine, {}, CREATE_NO_WINDOW);
    if (!process) {
        return nullptr;
    }

    // TODO: should show a message box and do it in a background thread
    DWORD timeoutInMs = 40000;
    // allow to disable the timeout
    if (GetEnvironmentVariable(L"SUMATRAPDF_NO_GHOSTSCRIPT_TIMEOUT", nullptr, 0)) {
        timeoutInMs = INFINITE;
    }
    DWORD exitCode = EXIT_FAILURE;
    WaitForSingleObject(process, timeoutInMs);
    GetExitCodeProcess(process, &exitCode);
    TerminateProcess(process, 1);
    CloseHandle(process);
    if (exitCode != EXIT_SUCCESS) {
        return nullptr;
    }

    Str pdfData = file::ReadFile(tmpFile);
    if (len(pdfData) == 0) {
        return nullptr;
    }

    TempStr nameHint = str::JoinTemp(path, StrL(".pdf"));
    EngineBase* engine = CreateEngineMupdfFromData(pdfData, nameHint, nullptr);
    if (!engine) {
        str::Free(pdfData);
        return nullptr;
    }
    *pdfDataOut = pdfData;
    return engine;
}

static EngineBase* psgz2pdf(Str fileName, Str* pdfDataOut) {
    TempStr tmpFile = GetTempFilePathTemp(StrL("PsE"));
    AutoDeleteFile tmpFileScope(tmpFile);
    if (len(tmpFile) == 0) {
        return nullptr;
    }

    WCHAR* path = CWStrTemp(fileName);
    gzFile inFile = gzopen_w(path, "rb");
    if (!inFile) {
        return nullptr;
    }
    FILE* outFile = nullptr;
    WCHAR* tmpFileW = CWStrTemp(tmpFile);
    errno_t err = _wfopen_s(&outFile, tmpFileW, L"wb");
    if (err != 0 || !outFile) {
        gzclose(inFile);
        return nullptr;
    }

    constexpr i64 kMaxUncompressedPostScriptSize = 512LL * 1024 * 1024;
    i64 totalSize = 0;
    bool ok = true;
    char buffer[12 * 1024];
    for (;;) {
        int n = gzread(inFile, buffer, sizeof(buffer));
        if (n <= 0) {
            ok = n == 0;
            break;
        }
        totalSize += n;
        if (totalSize > kMaxUncompressedPostScriptSize || fwrite(buffer, 1, n, outFile) != (size_t)n) {
            ok = false;
            break;
        }
    }
    fclose(outFile);
    gzclose(inFile);
    if (!ok) {
        return nullptr;
    }

    return ps2pdf(tmpFile, pdfDataOut);
}

// EnginePs is mostly a proxy for a PdfEngine that's fed whatever
// the ps2pdf conversion from Ghostscript returns
class EnginePs : public EngineBase {
  public:
    EnginePs() {
        kind = kindEnginePostScript;
        defaultExt = str::Dup(StrL(".ps"));
    }

    ~EnginePs() override {
        if (pdfEngine) {
            pdfEngine->Release();
        }
        str::Free(pdfData);
    }

    // from our copy of the converted PDF: pdfEngine->Clone() can't, and running
    // Ghostscript again would cost seconds
    EngineBase* Clone() override {
        if (len(pdfData) == 0) {
            return {};
        }
        TempStr nameHint = str::JoinTemp(FilePath(), StrL(".pdf"));
        EngineBase* newEngine = CreateEngineMupdfFromData(pdfData, nameHint, nullptr);
        if (!newEngine) {
            return {};
        }
        EnginePs* clone = new EnginePs();
        if (FilePath()) {
            clone->SetFilePath(FilePath());
        }
        clone->pdfEngine = newEngine;
        clone->pdfData = str::Dup(pdfData);
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

    // saving as .pdf writes what Ghostscript produced; anything else is a copy
    // of the PostScript we opened
    bool SaveFileAs(Str dstPath) override {
        if (str::EndsWithI(dstPath, StrL(".pdf")) && len(pdfData) > 0) {
            return file::WriteFile(dstPath, pdfData);
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
        // omit properties created by Ghostscript
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

    // the elements above are the pdf engine's, so its images are too
    RenderedBitmap* GetImageForPageElement(IPageElement* ipel) override {
        return pdfEngine->GetImageForPageElement(ipel);
    }

    Str GetImageDataForPageElement(IPageElement* ipel) override { return pdfEngine->GetImageDataForPageElement(ipel); }

    // the base version blocks in GetElements(); the pdf engine gives up instead
    // when a render thread holds its locks
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

    // don't delete the result
    IPageElement* GetElementAtPos(int pageNo, PointF pt) override { return pdfEngine->GetElementAtPos(pageNo, pt); }

    bool HandleLink(IPageDestination* dest, ILinkHandler* lh) override { return pdfEngine->HandleLink(dest, lh); }

    // engine-owned; do not delete
    IPageDestination* GetNamedDest(Str name) override { return pdfEngine->GetNamedDest(name); }

    TocTree* GetToc() override { return pdfEngine->GetToc(); }

    EngineBase* pdfEngine = nullptr;
    // the PDF Ghostscript made, kept for Clone() and "save as PDF"
    Str pdfData;

    // the base class keeps this in plain fields, so a clone has to copy it or it
    // reports one page (EnsureChapterTable) at the default dpi
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
        ReportIf(FilePath() || pdfEngine);
        if (len(fileName) == 0) {
            return false;
        }

        SetFilePath(fileName);
        if (file::StartsWith(fileName, StrL("\x1F\x8B"))) {
            pdfEngine = psgz2pdf(fileName, &pdfData);
        } else {
            pdfEngine = ps2pdf(fileName, &pdfData);
        }

        if (!pdfEngine) {
            return false;
        }

        if (str::EndsWithI(FilePath(), StrL(".eps"))) {
            defaultExt = str::Dup(StrL(".eps"));
        }

        CopyStateFromPdfEngine();

        return true;
    }
};

EngineBase* CreateEnginePsFromFile(Str fileName) {
    EnginePs* engine = new EnginePs();
    if (!engine->Load(fileName)) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return engine;
}

/* EnginePs.cpp */
bool IsEnginePsAvailable() {
    TempStr gswin32c = GetGhostscriptPathTemp();
    return len(gswin32c) > 0;
}

bool IsEnginePsSupportedFileType(FileType kind) {
    if (!IsEnginePsAvailable()) {
        return false;
    }
    return kind == FileType::PS;
}
