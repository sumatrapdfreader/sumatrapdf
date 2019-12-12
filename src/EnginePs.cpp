/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include <zlib.h>
#include "utils/ByteReader.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "EnginePdf.h"
#include "EnginePs.h"

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
        AutoFreeW keyName(str::Join(L"Software\\", gsProducts[i]));
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyName, 0, access, &hkey) != ERROR_SUCCESS)
            continue;
        WCHAR subkey[32];
        for (DWORD ix = 0; RegEnumKey(hkey, ix, subkey, dimof(subkey)) == ERROR_SUCCESS; ix++)
            versions.Append(str::Dup(subkey));
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
            AutoFreeW keyName(str::Format(L"Software\\%s\\%s", gsProducts[i], versions.at(ix - 1)));
            AutoFreeW GS_DLL(ReadRegStr(HKEY_LOCAL_MACHINE, keyName, L"GS_DLL"));
            if (!GS_DLL)
                continue;
            AutoFreeW dir(path::GetDir(GS_DLL));
            AutoFreeW exe(path::Join(dir, L"gswin32c.exe"));
            if (file::Exists(exe))
                return exe.StealData();
            exe.Set(path::Join(dir, L"gswin64c.exe"));
            if (file::Exists(exe))
                return exe.StealData();
        }
    }

    // if Ghostscript isn't found in the Registry, try finding it in the %PATH%
    DWORD size = GetEnvironmentVariable(L"PATH", nullptr, 0);
    AutoFreeW envpath(AllocArray<WCHAR>(size));
    if (size > 0 && envpath) {
        GetEnvironmentVariable(L"PATH", envpath, size);
        WStrVec paths;
        paths.Split(envpath, L";", true);
        for (size_t ix = 0; ix < paths.size(); ix++) {
            AutoFreeW exe(path::Join(paths.at(ix), L"gswin32c.exe"));
            if (file::Exists(exe))
                return exe.StealData();
            exe.Set(path::Join(paths.at(ix), L"gswin64c.exe"));
            if (file::Exists(exe))
                return exe.StealData();
        }
    }

    return nullptr;
}

class ScopedFile {
    AutoFreeW path;

  public:
    explicit ScopedFile(const WCHAR* path) : path(str::Dup(path)) {
    }
    ~ScopedFile() {
        if (path)
            file::Delete(path);
    }
};

static RectI ExtractDSCPageSize(const WCHAR* fileName) {
    char header[1024] = {0};
    file::ReadN(fileName, header, sizeof(header) - 1);
    if (!str::StartsWith(header, "%!PS-Adobe-"))
        return RectI();

    // PostScript creators are supposed to set the page size
    // e.g. through a setpagedevice call in PostScript code,
    // some creators however fail to do so and only indicate
    // the page size in a DSC BoundingBox comment.
    char* nl = header;
    geomutil::RectT<float> bbox;
    while ((nl = strchr(nl + 1, '\n')) != nullptr && '%' == nl[1]) {
        if (str::StartsWith(nl + 1, "%%BoundingBox:") &&
            str::Parse(nl + 1, "%%%%BoundingBox: 0 0 %f %f% ", &bbox.dx, &bbox.dy)) {
            return bbox.ToInt();
        }
    }

    return RectI();
}

static EngineBase* ps2pdf(const WCHAR* fileName) {
    // TODO: read from gswin32c's stdout instead of using a TEMP file
    AutoFreeW shortPath(path::ShortPath(fileName));
    AutoFreeW tmpFile(path::GetTempPath(L"PsE"));
    ScopedFile tmpFileScope(tmpFile);
    AutoFreeW gswin32c(GetGhostscriptPath());
    if (!shortPath || !tmpFile || !gswin32c)
        return nullptr;

    // try to help Ghostscript determine the intended page size
    AutoFreeW psSetup;
    RectI page = ExtractDSCPageSize(fileName);
    if (!page.IsEmpty())
        psSetup.Set(str::Format(L" << /PageSize [%i %i] >> setpagedevice", page.dx, page.dy));

    AutoFreeW cmdLine(
        str::Format(L"\"%s\" -q -dSAFER -dNOPAUSE -dBATCH -dEPSCrop -sOutputFile=\"%s\" -sDEVICE=pdfwrite -c "
                    L"\".setpdfwrite%s\" -f \"%s\"",
                    gswin32c.Get(), tmpFile.Get(), psSetup ? psSetup.Get() : L"", shortPath.Get()));
    fprintf(stderr, "- %s:%d: using '%ls' for creating '%%TEMP%%\\%ls'\n", path::GetBaseNameNoFree(__FILE__), __LINE__,
            gswin32c.Get(), path::GetBaseNameNoFree(tmpFile));

    // TODO: the PS-to-PDF conversion can hang the UI for several seconds
    HANDLE process = LaunchProcess(cmdLine, nullptr, CREATE_NO_WINDOW);
    if (!process)
        return nullptr;

    DWORD timeout = 10000;
#ifdef DEBUG
    // allow to disable the timeout for debugging purposes
    if (GetEnvironmentVariable(L"SUMATRAPDF_NO_GHOSTSCRIPT_TIMEOUT", nullptr, 0))
        timeout = INFINITE;
#endif
    DWORD exitCode = EXIT_FAILURE;
    WaitForSingleObject(process, timeout);
    GetExitCodeProcess(process, &exitCode);
    TerminateProcess(process, 1);
    CloseHandle(process);
    if (exitCode != EXIT_SUCCESS) {
        return nullptr;
    }

    OwnedData pdfData(file::ReadFile(tmpFile));
    if (!pdfData.data) {
        return nullptr;
    }

    ScopedComPtr<IStream> stream(CreateStreamFromData(pdfData.data, pdfData.size));
    if (!stream) {
        return nullptr;
    }

    return CreateEnginePdfFromStream(stream);
}

static EngineBase* psgz2pdf(const WCHAR* fileName) {
    AutoFreeW tmpFile(path::GetTempPath(L"PsE"));
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

// PsEngineImpl is mostly a proxy for a PdfEngine that's fed whatever
// the ps2pdf conversion from Ghostscript returns
class PsEngineImpl : public EngineBase {
  public:
    PsEngineImpl() {
        kind = kindEnginePostScript;
        defaultFileExt = L".ps";
    }

    virtual ~PsEngineImpl() {
        delete pdfEngine;
    }

    EngineBase* Clone() override {
        EngineBase* newEngine = pdfEngine->Clone();
        if (!newEngine)
            return nullptr;
        PsEngineImpl* clone = new PsEngineImpl();
        if (FileName())
            clone->SetFileName(FileName());
        clone->pdfEngine = newEngine;
        return clone;
    }

    int PageCount() const override {
        return pdfEngine->PageCount();
    }

    RectD PageMediabox(int pageNo) override {
        return pdfEngine->PageMediabox(pageNo);
    }

    RectD PageContentBox(int pageNo, RenderTarget target = RenderTarget::View) override {
        return pdfEngine->PageContentBox(pageNo, target);
    }

    RenderedBitmap* RenderBitmap(int pageNo, float zoom, int rotation,
                                 RectD* pageRect = nullptr, /* if nullptr: defaults to the page's mediabox */
                                 RenderTarget target = RenderTarget::View,
                                 AbortCookie** cookie_out = nullptr) override {
        return pdfEngine->RenderBitmap(pageNo, zoom, rotation, pageRect, target, cookie_out);
    }

    PointD Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse = false) override {
        return pdfEngine->Transform(pt, pageNo, zoom, rotation, inverse);
    }

    RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse = false) override {
        return pdfEngine->Transform(rect, pageNo, zoom, rotation, inverse);
    }

    std::tuple<char*, size_t> GetFileData() override {
        return file::ReadFile(fileName);
    }

    bool SaveFileAs(const char* copyFileName, bool includeUserAnnots = false) override {
        UNUSED(includeUserAnnots);
        if (!FileName()) {
            return false;
        }
        AutoFreeW dstPath(str::conv::FromUtf8(copyFileName));
        return CopyFileW(FileName(), dstPath, FALSE);
    }

    bool SaveFileAsPDF(const char* pdfFileName, bool includeUserAnnots = false) override {
        return pdfEngine->SaveFileAs(pdfFileName, includeUserAnnots);
    }

    WCHAR* ExtractPageText(int pageNo, RectI** coordsOut = nullptr) override {
        return pdfEngine->ExtractPageText(pageNo, coordsOut);
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

    bool SupportsAnnotation(bool forSaving = false) const override {
        return !forSaving && pdfEngine->SupportsAnnotation();
    }

    void UpdateUserAnnotations(Vec<PageAnnotation>* list) override {
        pdfEngine->UpdateUserAnnotations(list);
    }

    bool BenchLoadPage(int pageNo) override {
        return pdfEngine->BenchLoadPage(pageNo);
    }

    Vec<PageElement*>* GetElements(int pageNo) override {
        return pdfEngine->GetElements(pageNo);
    }

    PageElement* GetElementAtPos(int pageNo, PointD pt) override {
        return pdfEngine->GetElementAtPos(pageNo, pt);
    }

    PageDestination* GetNamedDest(const WCHAR* name) override {
        return pdfEngine->GetNamedDest(name);
    }

    DocTocTree* GetTocTree() override {
        return pdfEngine->GetTocTree();
    }

    static EngineBase* CreateFromFile(const WCHAR* fileName);

  protected:
    EngineBase* pdfEngine = nullptr;

    bool Load(const WCHAR* fileName) {
        AssertCrash(!FileName() && !pdfEngine);
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
            defaultFileExt = L".eps";
        }

        if (!pdfEngine) {
            return false;
        }

        preferredLayout = pdfEngine->preferredLayout;
        fileDPI = pdfEngine->GetFileDPI();
        allowsPrinting = pdfEngine->AllowsPrinting();
        allowsCopyingText = pdfEngine->AllowsCopyingText();
        decryptionKey = pdfEngine->decryptionKey;

        return true;
    }
};

EngineBase* PsEngineImpl::CreateFromFile(const WCHAR* fileName) {
    PsEngineImpl* engine = new PsEngineImpl();
    if (!engine->Load(fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

bool IsPsEngineAvailable() {
    AutoFreeW gswin32c(GetGhostscriptPath());
    return gswin32c.Get() != nullptr;
}

bool IsPsEngineSupportedFile(const WCHAR* fileName, bool sniff) {
    if (!IsPsEngineAvailable()) {
        return false;
    }

    if (sniff) {
        char header[2048] = {0};
        file::ReadN(fileName, header, sizeof(header) - 1);
        if (str::StartsWith(header, "\xC5\xD0\xD3\xC6")) {
            // Windows-format EPS file - cf. http://partners.adobe.com/public/developer/en/ps/5002.EPSF_Spec.pdf
            DWORD psStart = ByteReader(header, sizeof(header)).DWordLE(4);
            return psStart >= sizeof(header) - 12 || str::StartsWith(header + psStart, "%!PS-Adobe-");
        }
        return str::StartsWith(header, "%!") ||
               // also sniff PJL (Printer Job Language) files containing Postscript data
               str::StartsWith(header, "\x1B%-12345X@PJL") && str::Find(header, "\n%!PS-Adobe-");
    }

    return str::EndsWithI(fileName, L".ps") || str::EndsWithI(fileName, L".ps.gz") || str::EndsWithI(fileName, L".eps");
}

EngineBase* CreatePsEngineFromFile(const WCHAR* fileName) {
    return PsEngineImpl::CreateFromFile(fileName);
}
