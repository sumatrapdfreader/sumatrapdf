/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include <zlib.h>
#include "utils/ByteReader.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/GuessFileType.h"
#include "utils/WinUtil.h"

#include "Annotation.h"
#include "wingui/TreeModel.h"
#include "DisplayMode.h"
#include "Controller.h"
#include "EngineBase.h"
#include "EngineAll.h"

#include "utils/Log.h"

Kind kindEnginePostScript = "enginePostScript";

static WCHAR* GetGhostscriptPath() {
    const WCHAR* gsProducts[] = {
        L"AFPL Ghostscript",
        L"Aladdin Ghostscript",
        L"GPL Ghostscript",
        L"GNU Ghostscript",
    };

    // find all installed Ghostscript versions
    WStrVec versions;
    REGSAM access = KEY_READ | KEY_WOW64_32KEY;
TryAgain64Bit:
    for (int i = 0; i < dimof(gsProducts); i++) {
        HKEY hkey;
        AutoFreeWstr keyName(str::Join(L"Software\\", gsProducts[i]));
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyName, 0, access, &hkey) != ERROR_SUCCESS) {
            continue;
        }
        WCHAR subkey[32];
        for (DWORD ix = 0; RegEnumKey(hkey, ix, subkey, dimof(subkey)) == ERROR_SUCCESS; ix++) {
            versions.Append(str::Dup(subkey));
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
    for (size_t ix = versions.size(); ix > 0; ix--) {
        for (int i = 0; i < dimof(gsProducts); i++) {
            AutoFreeWstr keyName(str::Format(L"Software\\%s\\%s", gsProducts[i], versions.at(ix - 1)));
            AutoFreeWstr GS_DLL(ReadRegStr(HKEY_LOCAL_MACHINE, keyName, L"GS_DLL"));
            if (!GS_DLL) {
                continue;
            }
            AutoFreeWstr dir(path::GetDir(GS_DLL));
            AutoFreeWstr exe(path::Join(dir, L"gswin32c.exe"));
            if (file::Exists(exe)) {
                return exe.StealData();
            }
            exe.Set(path::Join(dir, L"gswin64c.exe"));
            if (file::Exists(exe)) {
                return exe.StealData();
            }
        }
    }

    // if Ghostscript isn't found in the Registry, try finding it in the %PATH%
    DWORD size = GetEnvironmentVariable(L"PATH", nullptr, 0);
    AutoFreeWstr envpath(AllocArray<WCHAR>(size));
    if (size == 0) {
        return nullptr;
    }
    GetEnvironmentVariable(L"PATH", envpath, size);
    WStrVec paths;
    paths.Split(envpath, L";", true);
    for (size_t ix = 0; ix < paths.size(); ix++) {
        AutoFreeWstr exe(path::Join(paths.at(ix), L"gswin32c.exe"));
        if (file::Exists(exe)) {
            return exe.StealData();
        }
        exe.Set(path::Join(paths.at(ix), L"gswin64c.exe"));
        if (file::Exists(exe)) {
            return exe.StealData();
        }
    }
    return nullptr;
}

class ScopedFile {
    AutoFreeWstr path;

  public:
    explicit ScopedFile(const WCHAR* path) : path(str::Dup(path)) {
    }
    ~ScopedFile() {
        if (path) {
            file::Delete(path);
        }
    }
};

#if 0
static Rect ExtractDSCPageSize(const WCHAR* path) {
    char header[1024] = {0};
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

static EngineBase* ps2pdf(const WCHAR* path) {
    // TODO: read from gswin32c's stdout instead of using a TEMP file
    AutoFreeWstr shortPath(path::ShortPath(path));
    AutoFreeWstr tmpFile(path::GetTempFilePath(L"PsE"));
    ScopedFile tmpFileScope(tmpFile);
    AutoFreeWstr gswin32c(GetGhostscriptPath());
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
    AutoFreeWstr cmdLine = str::Format(
        L"\"%s\" -q -dSAFER -dNOPAUSE -dBATCH -dEPSCrop -sOutputFile=\"%s\" -sDEVICE=pdfwrite "
        L"-f \"%s\"",
        gswin32c.Get(), tmpFile.Get(), shortPath.Get());

    {
        const char* fileName = path::GetBaseNameTemp(__FILE__);
        auto gswin = ToUtf8Temp(gswin32c.Get());
        auto tmpFileName = ToUtf8Temp(path::GetBaseNameTemp(tmpFile));
        logf("- %s:%d: using '%s' for creating '%%TEMP%%\\%s'\n", fileName, __LINE__, gswin.Get(), tmpFileName.Get());
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

    AutoFree pdfData = file::ReadFile(tmpFile);
    if (pdfData.empty()) {
        return nullptr;
    }

    auto strm = CreateStreamFromData(pdfData.AsSpan());
    ScopedComPtr<IStream> stream(strm);
    if (!stream) {
        return nullptr;
    }

    return CreateEngineMupdfFromStream(stream, ToUtf8Temp(tmpFile).Get());
}

static EngineBase* psgz2pdf(const WCHAR* fileName) {
    AutoFreeWstr tmpFile(path::GetTempFilePath(L"PsE"));
    ScopedFile tmpFileScope(tmpFile);
    if (!tmpFile) {
        return nullptr;
    }

    gzFile inFile = gzopen_w(fileName, "rb");
    if (!inFile) {
        return nullptr;
    }
    FILE* outFile = nullptr;
    errno_t err = _wfopen_s(&outFile, tmpFile, L"wb");
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
        defaultExt = L".ps";
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
        if (FileName()) {
            clone->SetFileName(FileName());
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
        const WCHAR* fileName = FileName();
        return file::ReadFile(fileName);
    }

    bool SaveFileAs(const char* copyFileName) override {
        if (!FileName()) {
            return false;
        }
        auto dstPath = ToWstrTemp(copyFileName);
        return file::Copy(dstPath, FileName(), false);
    }

    bool SaveFileAsPDF(const char* pdfFileName) override {
        return pdfEngine->SaveFileAs(pdfFileName);
    }

    PageText ExtractPageText(int pageNo) override {
        return pdfEngine->ExtractPageText(pageNo);
    }

    bool HasClipOptimizations(int pageNo) override {
        return pdfEngine->HasClipOptimizations(pageNo);
    }

    WCHAR* GetProperty(DocumentProperty prop) override {
        // omit properties created by Ghostscript
        if (!pdfEngine || DocumentProperty::CreationDate == prop || DocumentProperty::ModificationDate == prop ||
            DocumentProperty::PdfVersion == prop || DocumentProperty::PdfProducer == prop ||
            DocumentProperty::PdfFileStructure == prop) {
            return nullptr;
        }
        return pdfEngine->GetProperty(prop);
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

    IPageDestination* GetNamedDest(const WCHAR* name) override {
        return pdfEngine->GetNamedDest(name);
    }

    TocTree* GetToc() override {
        return pdfEngine->GetToc();
    }

    static EngineBase* CreateFromFile(const WCHAR* fileName);

  protected:
    EngineBase* pdfEngine = nullptr;

    bool Load(const WCHAR* fileName) {
        pageCount = 0;
        CrashIf(FileName() || pdfEngine);
        if (!fileName) {
            return false;
        }
        SetFileName(fileName);
        if (file::StartsWith(fileName, "\x1F\x8B")) {
            pdfEngine = psgz2pdf(fileName);
        } else {
            pdfEngine = ps2pdf(fileName);
        }

        if (str::EndsWithI(FileName(), L".eps")) {
            defaultExt = L".eps";
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

EngineBase* EnginePs::CreateFromFile(const WCHAR* fileName) {
    EnginePs* engine = new EnginePs();
    if (!engine->Load(fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

bool IsEnginePsAvailable() {
    AutoFreeWstr gswin32c(GetGhostscriptPath());
    return gswin32c.Get() != nullptr;
}

bool IsEnginePsSupportedFileType(Kind kind) {
    if (!IsEnginePsAvailable()) {
        return false;
    }
    return kind == kindFilePS;
}

EngineBase* CreateEnginePsFromFile(const WCHAR* fileName) {
    return EnginePs::CreateFromFile(fileName);
}
