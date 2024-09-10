/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include <zlib.h>
#include "utils/ByteReader.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/GuessFileType.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"

#include "utils/Log.h"

Kind kindEnginePostScript = "enginePostScript";

static TempStr GetGhostscriptPathTemp() {
    const char* gsProducts[] = {
        "AFPL Ghostscript",
        "Aladdin Ghostscript",
        "GPL Ghostscript",
        "GNU Ghostscript",
    };

    // find all installed Ghostscript versions
    StrVec versions;
    REGSAM access = KEY_READ | KEY_WOW64_32KEY;
TryAgain64Bit:
    for (const char* gsProd : gsProducts) {
        HKEY hkey;
        TempStr keyName = str::JoinTemp("Software\\", gsProd);
        TempWStr keyNameW = ToWStrTemp(keyName);
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
    int nVers = versions.Size();
    for (int i = nVers; i > 0; i--) {
        for (const char* gsProd : gsProducts) {
            char* ver = versions.At(i - 1);
            TempStr keyName = str::FormatTemp("Software\\%s\\%s", gsProd, ver);
            TempStr gsDLL = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, "GS_DLL");
            if (!gsDLL) {
                continue;
            }
            TempStr dir = path::GetDirTemp(gsDLL);
            TempStr exe = path::JoinTemp(dir, "gswin32c.exe");
            if (file::Exists(exe)) {
                return exe;
            }
            exe = path::JoinTemp(dir, "gswin64c.exe");
            if (file::Exists(exe)) {
                return exe;
            }
        }
    }

    // if Ghostscript isn't found in the Registry, try finding it in the %PATH%
    DWORD size = GetEnvironmentVariableW(L"PATH", nullptr, 0);
    TempWStr envpathW = AllocArrayTemp<WCHAR>(size + 1);
    if (size == 0) {
        return nullptr;
    }
    GetEnvironmentVariableW(L"PATH", envpathW, size);
    TempStr envPath = ToUtf8Temp(envpathW);
    StrVec paths;
    Split(&paths, envPath, ";", true);
    for (char* path : paths) {
        TempStr exe = path::JoinTemp(path, "gswin32c.exe");
        if (!file::Exists(exe)) {
            exe = path::JoinTemp(path, "gswin64c.exe");
        }
        if (!file::Exists(exe)) {
            continue;
        }
        return exe;
    }
    return nullptr;
}

struct AutoDeleteFile {
    AutoFreeStr filePath;

    explicit AutoDeleteFile(const WCHAR* path) {
        filePath.Set(ToUtf8(path));
    }
    explicit AutoDeleteFile(const char* path) {
        filePath.SetCopy(path);
    }
    ~AutoDeleteFile() {
        if (filePath) {
            file::Delete(filePath);
        }
    }
};

#if 0
static Rect ExtractDSCPageSize(const WCHAR* path) {
    char header[1024]{};
    file::ReadN(path, header, sizeof(header) - 1);
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

static EngineBase* ps2pdf(const char* path) {
    // TODO: read from gswin32c's stdout instead of using a TEMP file
    TempStr shortPath = path::ShortPathTemp(path);
    TempStr tmpFile = GetTempFilePathTemp("PsE");
    AutoDeleteFile tmpFileScope(tmpFile);
    TempStr gswin32c = GetGhostscriptPathTemp();
    if (!shortPath || !tmpFile || !gswin32c) {
        return nullptr;
    }

    // TODO: before gs 9.54 we would call:
    // Rect page = ExtractDSCPageSize(path);
    // and use that to add "-c ".setpdfwrite << /PageSize [$dx $dy] >> setpagedevice"
    // to cmd-line. In 9.54 .setpdfwrite was removed and using it causes
    // conversion to fail
    // So we removed use of -c .setpdfwrite completely. Not sure if there's an alternative
    // way to do it
    // https://github.com/GravityMedia/Ghostscript/issues/6
    // https://github.com/sumatrapdfreader/sumatrapdf/issues/1923
    TempStr cmdLine = str::FormatTemp(
        "\"%s\" -q -dSAFER -dNOPAUSE -dBATCH -dEPSCrop -sOutputFile=\"%s\" -sDEVICE=pdfwrite "
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

    ByteSlice pdfData = file::ReadFile(tmpFile);
    if (pdfData.empty()) {
        return nullptr;
    }

    IStream* strm = CreateStreamFromData(pdfData);
    pdfData.Free();
    ScopedComPtr<IStream> stream(strm);
    if (!stream) {
        return nullptr;
    }

    TempStr nameHint = str::Join(path, ".pdf");
    return CreateEngineMupdfFromStream(stream, nameHint);
}

static EngineBase* psgz2pdf(const char* fileName) {
    TempStr tmpFile = GetTempFilePathTemp("PsE");
    AutoDeleteFile tmpFileScope(tmpFile);
    if (!tmpFile) {
        return nullptr;
    }

    WCHAR* path = ToWStrTemp(fileName);
    gzFile inFile = gzopen_w(path, "rb");
    if (!inFile) {
        return nullptr;
    }
    FILE* outFile = nullptr;
    WCHAR* tmpFileW = ToWStrTemp(tmpFile);
    errno_t err = _wfopen_s(&outFile, tmpFileW, L"wb");
    if (err != 0 || !outFile) {
        gzclose(inFile);
        return nullptr;
    }

    char buffer[12 * 1024];
    for (;;) {
        int len = gzread(inFile, buffer, sizeof(buffer));
        if (len <= 0) {
            break;
        }
        fwrite(buffer, 1, len, outFile);
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
        defaultExt = str::Dup(".ps");
    }

    ~EnginePs() override {
        if (pdfEngine) {
            pdfEngine->Release();
        }
    }

    EngineBase* Clone() override {
        EngineBase* newEngine = pdfEngine->Clone();
        if (!newEngine) {
            return nullptr;
        }
        EnginePs* clone = new EnginePs();
        if (FilePath()) {
            clone->SetFilePath(FilePath());
        }
        clone->pdfEngine = newEngine;
        return clone;
    }

    RectF PageMediabox(int pageNo) override {
        return pdfEngine->PageMediabox(pageNo);
    }

    RectF PageContentBox(int pageNo, RenderTarget target = RenderTarget::View) override {
        return pdfEngine->PageContentBox(pageNo, target);
    }

    RenderedBitmap* RenderPage(RenderPageArgs& args) override {
        return pdfEngine->RenderPage(args);
    }

    RectF Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse = false) override {
        return pdfEngine->Transform(rect, pageNo, zoom, rotation, inverse);
    }

    ByteSlice GetFileData() override {
        const char* path = FilePath();
        return file::ReadFile(path);
    }

    bool SaveFileAs(const char* dstPath) override {
        const char* srcPath = FilePath();
        if (!srcPath) {
            return false;
        }
        return file::Copy(dstPath, srcPath, false);
    }

    PageText ExtractPageText(int pageNo) override {
        return pdfEngine->ExtractPageText(pageNo);
    }

    bool HasClipOptimizations(int pageNo) override {
        return pdfEngine->HasClipOptimizations(pageNo);
    }

    TempStr GetPropertyTemp(const char* name) override {
        // omit properties created by Ghostscript
        if (!pdfEngine) {
            return nullptr;
        }
        static const char* toOmit[] = {kPropCreationDate, kPropModificationDate, kPropPdfVersion,
                                       kPropPdfProducer,  kPropPdfFileStructure, nullptr};

        for (const char** ptr = toOmit; *ptr; ptr++) {
            const char* s = *ptr;
            if (str::Eq(s, name)) {
                return nullptr;
            }
        }
        return pdfEngine->GetPropertyTemp(name);
    }

    bool BenchLoadPage(int pageNo) override {
        return pdfEngine->BenchLoadPage(pageNo);
    }

    Vec<IPageElement*> GetElements(int pageNo) override {
        return pdfEngine->GetElements(pageNo);
    }

    // don't delete the result
    IPageElement* GetElementAtPos(int pageNo, PointF pt) override {
        return pdfEngine->GetElementAtPos(pageNo, pt);
    }

    bool HandleLink(IPageDestination* dest, ILinkHandler* lh) override {
        return pdfEngine->HandleLink(dest, lh);
    }

    IPageDestination* GetNamedDest(const char* name) override {
        return pdfEngine->GetNamedDest(name);
    }

    TocTree* GetToc() override {
        return pdfEngine->GetToc();
    }

    EngineBase* pdfEngine = nullptr;

    bool Load(const char* fileName) {
        pageCount = 0;
        ReportIf(FilePath() || pdfEngine);
        if (!fileName) {
            return false;
        }

        SetFilePath(fileName);
        if (file::StartsWith(fileName, "\x1F\x8B")) {
            pdfEngine = psgz2pdf(fileName);
        } else {
            pdfEngine = ps2pdf(fileName);
        }

        if (!pdfEngine) {
            return false;
        }

        if (str::EndsWithI(FilePath(), ".eps")) {
            defaultExt = str::Dup(".eps");
        }

        preferredLayout = pdfEngine->preferredLayout;
        fileDPI = pdfEngine->GetFileDPI();
        allowsPrinting = pdfEngine->AllowsPrinting();
        allowsCopyingText = pdfEngine->AllowsCopyingText();
        decryptionKey = pdfEngine->decryptionKey;
        pageCount = pdfEngine->PageCount();

        return true;
    }
};

EngineBase* CreateEnginePsFromFile(const char* fileName) {
    EnginePs* engine = new EnginePs();
    if (!engine->Load(fileName)) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return engine;
}

bool IsEnginePsAvailable() {
    TempStr gswin32c = GetGhostscriptPathTemp();
    return gswin32c != nullptr;
}

bool IsEnginePsSupportedFileType(Kind kind) {
    if (!IsEnginePsAvailable()) {
        return false;
    }
    return kind == kindFilePS;
}
