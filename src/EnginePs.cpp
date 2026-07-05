/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include <zlib.h>
#include "base/ScopedWin.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Win.h"

#include "wingui/UIModels.h"

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
            TempStr gsDLL = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, "GS_DLL");
            if (!gsDLL) {
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
    TempWStr envpathW = WStr(AllocArrayTemp<WCHAR>(size + 1), (int)size + 1);
    if (size == 0) {
        return {};
    }
    GetEnvironmentVariableW(L"PATH", envpathW.s, size);
    TempStr envPath = ToUtf8Temp(envpathW);
    StrVec paths;
    Split(&paths, envPath, ";", true);
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

#if 0
static Rect ExtractDSCPageSize(const WCHAR* path) {
    char header[1024]{};
    file::ReadN(path, (u8*)header, sizeof(header) - 1);
    if (!str::StartsWith((char*)header, "%!PS-Adobe-")) {
        return {};
    }

    // PostScript creators are supposed to set the page size
    // e.g. through a setpagedevice call in PostScript code,
    // some creators however fail to do so and only indicate
    // the page size in a DSC BoundingBox comment.
    char* nl = (char*)header;
    RectF bbox;
    while ((nl = strchr(nl + 1, '\n')) != nullptr && '%' == nl[1]) {
        if (str::StartsWith(nl + 1, "%%BoundingBox:") &&
            str::Parse(nl + 1, "%%%%BoundingBox: 0 0 %f %f% ", &bbox.dx, &bbox.dy)) {
            return ToRect(bbox);
        }
    }

    return {};
}
#endif

static EngineBase* ps2pdf(Str path) {
    // TODO: read from gswin32c's stdout instead of using a TEMP file
    TempStr shortPath = path::ShortPathTemp(path);
    TempStr tmpFile = GetTempFilePathTemp("PsE");
    AutoDeleteFile tmpFileScope(tmpFile);
    TempStr gswin32c = GetGhostscriptPathTemp();
    if (!shortPath || !tmpFile || !gswin32c) {
        return nullptr;
    }

    // Ghostscript 9.54+ removed .setpdfwrite, so we no longer pass PageSize via
    // -c ".setpdfwrite << /PageSize ... >> setpagedevice" (see issues #1923).
    TempStr cmdLine =
        fmt("\"%s\" -q -dSAFER -dNOPAUSE -dBATCH -dEPSCrop -sOutputFile=\"%s\" -sDEVICE=pdfwrite "
            "-f \"%s\"",
            gswin32c, tmpFile, shortPath);

    {
        TempStr fileName = path::GetBaseNameTemp(__FILE__);
        TempStr tmpFileName = path::GetBaseNameTemp(tmpFile);
        logf("- %s:%d: using '%s' for creating '%%TEMP%%\\%s'\n", fileName, __LINE__, gswin32c, tmpFileName);
    }

    // TODO: the PS-to-PDF conversion can hang the UI for several seconds
    HANDLE process = LaunchProcessInDir(cmdLine, nullptr, CREATE_NO_WINDOW);
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

    IStream* strm = CreateStreamFromData(pdfData);
    str::Free(pdfData);
    ScopedComPtr<IStream> stream(strm);
    if (!stream) {
        return nullptr;
    }

    TempStr nameHint = str::JoinTemp(path, StrL(".pdf"));
    return CreateEngineMupdfFromStream(stream, nameHint);
}

static EngineBase* psgz2pdf(Str fileName) {
    TempStr tmpFile = GetTempFilePathTemp("PsE");
    AutoDeleteFile tmpFileScope(tmpFile);
    if (!tmpFile) {
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

    char buffer[12 * 1024];
    for (;;) {
        int n = gzread(inFile, buffer, sizeof(buffer));
        if (n <= 0) {
            break;
        }
        fwrite(buffer, 1, n, outFile);
    }
    fclose(outFile);
    gzclose(inFile);

    return ps2pdf(tmpFile);
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
    }

    EngineBase* Clone() override {
        EngineBase* newEngine = pdfEngine->Clone();
        if (!newEngine) {
            return {};
        }
        EnginePs* clone = new EnginePs();
        if (FilePath()) {
            clone->SetFilePath(FilePath());
        }
        clone->pdfEngine = newEngine;
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

    bool SaveFileAs(Str dstPath) override {
        Str srcPath = FilePath();
        if (!srcPath) {
            return false;
        }
        return file::Copy(dstPath, srcPath, false);
    }

    PageText ExtractPageText(int pageNo) override { return pdfEngine->ExtractPageText(pageNo); }

    bool HasClipOptimizations(int pageNo) override { return pdfEngine->HasClipOptimizations(pageNo); }

    TempStr GetPropertyTemp(Str name) override {
        // omit properties created by Ghostscript
        if (!pdfEngine) {
            return {};
        }
        static const Str toOmit[] = {kPropCreationDate, kPropModificationDate, kPropPdfVersion,
                                     kPropPdfProducer,  kPropPdfFileStructure, Str()};

        for (Str omit : toOmit) {
            if (!omit) {
                break;
            }
            if (str::Eq(omit, name)) {
                return {};
            }
        }
        return pdfEngine->GetPropertyTemp(name);
    }

    bool BenchLoadPage(int pageNo) override { return pdfEngine->BenchLoadPage(pageNo); }

    Vec<IPageElement*> GetElements(int pageNo) override { return pdfEngine->GetElements(pageNo); }

    // don't delete the result
    IPageElement* GetElementAtPos(int pageNo, PointF pt) override { return pdfEngine->GetElementAtPos(pageNo, pt); }

    bool HandleLink(IPageDestination* dest, ILinkHandler* lh) override { return pdfEngine->HandleLink(dest, lh); }

    IPageDestination* GetNamedDest(Str name) override { return pdfEngine->GetNamedDest(name); }

    TocTree* GetToc() override { return pdfEngine->GetToc(); }

    EngineBase* pdfEngine = nullptr;

    bool Load(Str fileName) {
        pageCount = 0;
        ReportIf(FilePath() || pdfEngine);
        if (!fileName) {
            return false;
        }

        SetFilePath(fileName);
        if (file::StartsWith(fileName, StrL("\x1F\x8B"))) {
            pdfEngine = psgz2pdf(fileName);
        } else {
            pdfEngine = ps2pdf(fileName);
        }

        if (!pdfEngine) {
            return false;
        }

        if (str::EndsWithI(FilePath(), ".eps")) {
            defaultExt = str::Dup(StrL(".eps"));
        }

        preferredLayout = pdfEngine->preferredLayout;
        fileDPI = pdfEngine->GetFileDPI();
        allowsPrinting = pdfEngine->AllowsPrinting();
        allowsCopyingText = pdfEngine->AllowsCopyingText();
        decryptionKey = str::Dup(arena, pdfEngine->decryptionKey);
        pageCount = pdfEngine->PageCount();

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

bool IsEnginePsAvailable() {
    TempStr gswin32c = GetGhostscriptPathTemp();
    return len(gswin32c) > 0;
}

bool IsEnginePsSupportedFileType(Kind kind) {
    if (!IsEnginePsAvailable()) {
        return false;
    }
    return kind == kindFilePS;
}
