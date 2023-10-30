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

#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"

#include "utils/Log.h"

Kind kindEnginePostScript = "enginePostScript";

static char* GetGhostscriptPath() {
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
        char* keyName = str::JoinTemp("Software\\", gsProd);
        WCHAR* keyNameW = ToWStrTemp(keyName);
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyNameW, 0, access, &hkey) != ERROR_SUCCESS) {
            continue;
        }
        WCHAR subkey[32];
        for (DWORD ix = 0; RegEnumKey(hkey, ix, subkey, dimof(subkey)) == ERROR_SUCCESS; ix++) {
            char* ver = ToUtf8Temp(subkey);
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
    versions.SortNatural();

    // return the path to the newest installation
    size_t nVers = versions.size();
    for (size_t ix = nVers; ix > 0; ix--) {
        for (const char* gsProd : gsProducts) {
            char* ver = versions.at(ix - 1);
            TempStr keyName = str::FormatTemp("Software\\%s\\%s", gsProd, ver);
            char* GS_DLL = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, "GS_DLL");
            if (!GS_DLL) {
                continue;
            }
            char* dir = path::GetDirTemp(GS_DLL);
            char* exe = path::JoinTemp(dir, "gswin32c.exe");
            if (file::Exists(exe)) {
                return str::Dup(exe);
            }
            exe = path::JoinTemp(dir, "gswin64c.exe");
            if (file::Exists(exe)) {
                return str::Dup(exe);
            }
        }
    }

    // if Ghostscript isn't found in the Registry, try finding it in the %PATH%
    DWORD size = GetEnvironmentVariableW(L"PATH", nullptr, 0);
    AutoFreeWstr envpath(AllocArray<WCHAR>(size + 1));
    if (size == 0) {
        return nullptr;
    }
    GetEnvironmentVariableW(L"PATH", envpath, size);
    StrVec paths;
    Split(paths, ToUtf8Temp(envpath), ";", true);
    for (char* path : paths) {
        char* exe = path::JoinTemp(path, "gswin32c.exe");
        if (!file::Exists(exe)) {
            exe = path::JoinTemp(path, "gswin64c.exe");
        }
        if (!file::Exists(exe)) {
            continue;
        }
        return str::Dup(exe);
    }
    return nullptr;
}

class ScopedFile {
    AutoFreeStr path;

  public:
    explicit ScopedFile(const WCHAR* pathW) {
        path.Set(ToUtf8(pathW));
    }
    explicit ScopedFile(const char* pathA) {
        path.SetCopy(pathA);
    }
    ~ScopedFile() {
        if (path) {
            file::Delete(path);
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
    AutoFreeStr shortPath = path::ShortPath(path);
    AutoFreeStr tmpFile = path::GetTempFilePath("PsE");
    ScopedFile tmpFileScope(tmpFile);
    AutoFreeStr gswin32c = GetGhostscriptPath();
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
        gswin32c.Get(), tmpFile.Get(), shortPath.Get());

    {
        const char* fileName = path::GetBaseNameTemp(__FILE__);
        char* gswin = gswin32c.Get();
        const char* tmpFileName = path::GetBaseNameTemp(tmpFile.Get());
        logf("- %s:%d: using '%s' for creating '%%TEMP%%\\%s'\n", fileName, __LINE__, gswin, tmpFileName);
    }

    // TODO: the PS-to-PDF conversion can hang the UI for several seconds
    HANDLE process = LaunchProcess(cmdLine, nullptr, CREATE_NO_WINDOW);
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

    return CreateEngineMupdfFromStream(stream, tmpFile);
}

static EngineBase* psgz2pdf(const char* fileName) {
    AutoFreeStr tmpFile = path::GetTempFilePath("PsE");
    ScopedFile tmpFileScope(tmpFile);
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
        delete pdfEngine;
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

    TempStr GetPropertyTemp(DocumentProperty prop) override {
        // omit properties created by Ghostscript
        if (!pdfEngine || DocumentProperty::CreationDate == prop || DocumentProperty::ModificationDate == prop ||
            DocumentProperty::PdfVersion == prop || DocumentProperty::PdfProducer == prop ||
            DocumentProperty::PdfFileStructure == prop) {
            return nullptr;
        }
        return pdfEngine->GetPropertyTemp(prop);
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

    static EngineBase* CreateFromFile(const char* fileName);

  protected:
    EngineBase* pdfEngine = nullptr;

    bool Load(const char* fileName) {
        pageCount = 0;
        CrashIf(FilePath() || pdfEngine);
        if (!fileName) {
            return false;
        }
        SetFilePath(fileName);
        if (file::StartsWith(fileName, "\x1F\x8B")) {
            pdfEngine = psgz2pdf(fileName);
        } else {
            pdfEngine = ps2pdf(fileName);
        }

        if (str::EndsWithI(FilePath(), ".eps")) {
            defaultExt = str::Dup(".eps");
        }

        if (!pdfEngine) {
            return false;
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

EngineBase* EnginePs::CreateFromFile(const char* fileName) {
    EnginePs* engine = new EnginePs();
    if (!engine->Load(fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

bool IsEnginePsAvailable() {
    AutoFreeStr gswin32c = GetGhostscriptPath();
    return gswin32c.Get() != nullptr;
}

bool IsEnginePsSupportedFileType(Kind kind) {
    if (!IsEnginePsAvailable()) {
        return false;
    }
    return kind == kindFilePS;
}

EngineBase* CreateEnginePsFromFile(const char* fileName) {
    return EnginePs::CreateFromFile(fileName);
}
