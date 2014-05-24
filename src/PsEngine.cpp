/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "PsEngine.h"

#include "ByteReader.h"
#include "FileUtil.h"
#include "PdfEngine.h"
#include "WinUtil.h"

#include <zlib.h>

static WCHAR *GetGhostscriptPath()
{
    WCHAR *gsProducts[] = {
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
        ScopedMem<WCHAR> keyName(str::Join(L"Software\\", gsProducts[i]));
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
    for (size_t ix = versions.Count(); ix > 0; ix--) {
        for (int i = 0; i < dimof(gsProducts); i++) {
            ScopedMem<WCHAR> keyName(str::Format(L"Software\\%s\\%s",
                                                 gsProducts[i], versions.At(ix - 1)));
            ScopedMem<WCHAR> GS_DLL(ReadRegStr(HKEY_LOCAL_MACHINE, keyName, L"GS_DLL"));
            if (!GS_DLL)
                continue;
            ScopedMem<WCHAR> dir(path::GetDir(GS_DLL));
            ScopedMem<WCHAR> exe(path::Join(dir, L"gswin32c.exe"));
            if (file::Exists(exe))
                return exe.StealData();
            exe.Set(path::Join(dir, L"gswin64c.exe"));
            if (file::Exists(exe))
                return exe.StealData();
        }
    }

    // if Ghostscript isn't found in the Registry, try finding it in the %PATH%
    DWORD size = GetEnvironmentVariable(L"PATH", NULL, 0);
    ScopedMem<WCHAR> envpath(AllocArray<WCHAR>(size));
    if (size > 0 && envpath) {
        GetEnvironmentVariable(L"PATH", envpath, size);
        WStrVec paths;
        paths.Split(envpath, L";", true);
        for (size_t ix = 0; ix < paths.Count(); ix++) {
            ScopedMem<WCHAR> exe(path::Join(paths.At(ix), L"gswin32c.exe"));
            if (file::Exists(exe))
                return exe.StealData();
            exe.Set(path::Join(paths.At(ix), L"gswin64c.exe"));
            if (file::Exists(exe))
                return exe.StealData();
        }
    }

    return NULL;
}

class ScopedFile {
    ScopedMem<WCHAR> path;

public:
    explicit ScopedFile(const WCHAR *path) : path(str::Dup(path)) { }
    ~ScopedFile() {
        if (path)
            file::Delete(path);
    }
};

static RectI ExtractDSCPageSize(const WCHAR *fileName)
{
    char header[1024] = { 0 };
    file::ReadN(fileName, header, sizeof(header) - 1);
    if (!str::StartsWith(header, "%!PS-Adobe-"))
        return RectI();

    // PostScript creators are supposed to set the page size
    // e.g. through a setpagedevice call in PostScript code,
    // some creators however fail to do so and only indicate
    // the page size in a DSC BoundingBox comment.
    char *nl = header;
    geomutil::RectT<float> bbox;
    while ((nl = strchr(nl + 1, '\n')) != NULL && '%' == nl[1]) {
        if (str::StartsWith(nl + 1, "%%BoundingBox:") &&
            str::Parse(nl + 1, "%%%%BoundingBox: 0 0 %f %f% ", &bbox.dx, &bbox.dy)) {
            return bbox.Convert<int>();
        }
    }

    return RectI();
}

static BaseEngine *ps2pdf(const WCHAR *fileName)
{
    // TODO: read from gswin32c's stdout instead of using a TEMP file
    ScopedMem<WCHAR> shortPath(path::ShortPath(fileName));
    ScopedMem<WCHAR> tmpFile(path::GetTempPath(L"PsE"));
    ScopedFile tmpFileScope(tmpFile);
    ScopedMem<WCHAR> gswin32c(GetGhostscriptPath());
    if (!shortPath || !tmpFile || !gswin32c)
        return NULL;

    // try to help Ghostscript determine the intended page size
    ScopedMem<WCHAR> psSetup;
    RectI page = ExtractDSCPageSize(fileName);
    if (!page.IsEmpty())
        psSetup.Set(str::Format(L" << /PageSize [%i %i] >> setpagedevice", page.dx, page.dy));

    ScopedMem<WCHAR> cmdLine(str::Format(
        L"\"%s\" -q -dSAFER -dNOPAUSE -dBATCH -dEPSCrop -sOutputFile=\"%s\" -sDEVICE=pdfwrite -c \".setpdfwrite%s\" -f \"%s\"",
        gswin32c, tmpFile, psSetup ? psSetup : L"", shortPath));
    fprintf(stderr, "- %s:%d: using '%ls' for creating '%%TEMP%%\\%ls'\n", path::GetBaseName(__FILE__), __LINE__, gswin32c.Get(), path::GetBaseName(tmpFile));

    // TODO: the PS-to-PDF conversion can hang the UI for several seconds
    HANDLE process = LaunchProcess(cmdLine, NULL, CREATE_NO_WINDOW);
    if (!process)
        return NULL;

    DWORD timeout = 10000;
#ifdef DEBUG
    // allow to disable the timeout for debugging purposes
    if (GetEnvironmentVariable(L"SUMATRAPDF_NO_GHOSTSCRIPT_TIMEOUT", NULL, 0))
        timeout = INFINITE;
#endif
    DWORD exitCode = EXIT_FAILURE;
    WaitForSingleObject(process, timeout);
    GetExitCodeProcess(process, &exitCode);
    TerminateProcess(process, 1);
    CloseHandle(process);
    if (exitCode != EXIT_SUCCESS)
        return NULL;

    size_t len;
    ScopedMem<char> pdfData(file::ReadAll(tmpFile, &len));
    if (!pdfData)
        return NULL;

    ScopedComPtr<IStream> stream(CreateStreamFromData(pdfData, len));
    if (!stream)
        return NULL;

    return PdfEngine::CreateFromStream(stream);
}

static BaseEngine *psgz2pdf(const WCHAR *fileName)
{
    ScopedMem<WCHAR> tmpFile(path::GetTempPath(L"PsE"));
    ScopedFile tmpFileScope(tmpFile);
    if (!tmpFile)
        return NULL;

    gzFile inFile = gzopen_w(fileName, "rb");
    if (!inFile)
        return NULL;
    FILE *outFile = NULL;
    errno_t err = _wfopen_s(&outFile, tmpFile, L"wb");
    if (err != 0 || !outFile) {
        gzclose(inFile);
        return NULL;
    }

    char buffer[12*1024];
    for (;;) {
        int len = gzread(inFile, buffer, sizeof(buffer));
        if (len <= 0)
            break;
        fwrite(buffer, 1, len, outFile);
    }
    fclose(outFile);
    gzclose(inFile);

    return ps2pdf(tmpFile);
}

// PsEngineImpl is mostly a proxy for a PdfEngine that's fed whatever
// the ps2pdf conversion from Ghostscript returns
class PsEngineImpl : public PsEngine {
    friend PsEngine;

public:
    PsEngineImpl() : fileName(NULL), pdfEngine(NULL) { }
    virtual ~PsEngineImpl() {
        free(fileName);
        delete pdfEngine;
    }
    virtual PsEngineImpl *Clone() {
        if (!pdfEngine)
            return NULL;
        BaseEngine *newEngine = pdfEngine->Clone();
        if (!newEngine)
            return NULL;
        PsEngineImpl *clone = new PsEngineImpl();
        if (fileName) clone->fileName = str::Dup(fileName);
        clone->pdfEngine = newEngine;
        return clone;
    }

    virtual const WCHAR *FileName() const { return fileName; };
    virtual int PageCount() const {
        return pdfEngine ? pdfEngine->PageCount() : 0;
    }

    virtual RectD PageMediabox(int pageNo) {
        return pdfEngine ? pdfEngine->PageMediabox(pageNo) : RectD();
    }
    virtual RectD PageContentBox(int pageNo, RenderTarget target=Target_View) {
        return pdfEngine ? pdfEngine->PageContentBox(pageNo, target) : RectD();
    }

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View, AbortCookie **cookie_out=NULL) {
        return pdfEngine ? pdfEngine->RenderBitmap(pageNo, zoom, rotation, pageRect, target, cookie_out) : NULL;
    }
    virtual bool RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, RenderTarget target=Target_View, AbortCookie **cookie_out=NULL) {
        return pdfEngine ? pdfEngine->RenderPage(hDC, screenRect, pageNo, zoom, rotation, pageRect, target, cookie_out) : false;
    }

    virtual PointD Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse=false) {
        return pdfEngine ? pdfEngine->Transform(pt, pageNo, zoom, rotation, inverse) : pt;
    }
    virtual RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse=false) {
        return pdfEngine ? pdfEngine->Transform(rect, pageNo, zoom, rotation, inverse) : rect;
    }

    virtual unsigned char *GetFileData(size_t *cbCount) {
        return fileName ? (unsigned char *)file::ReadAll(fileName, cbCount) : NULL;
    }
    virtual bool SaveFileAs(const WCHAR *copyFileName) {
        return fileName ? CopyFile(fileName, copyFileName, FALSE) : false;
    }
    virtual WCHAR * ExtractPageText(int pageNo, WCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View) {
        return pdfEngine ? pdfEngine->ExtractPageText(pageNo, lineSep, coords_out, target) : NULL;
    }
    virtual bool HasClipOptimizations(int pageNo) {
        return pdfEngine ? pdfEngine->HasClipOptimizations(pageNo) : true;
    }
    virtual PageLayoutType PreferredLayout() {
        return pdfEngine ? pdfEngine->PreferredLayout() : Layout_Single;
    }
    virtual WCHAR *GetProperty(DocumentProperty prop) {
        // omit properties created by Ghostscript
        if (!pdfEngine || Prop_CreationDate == prop || Prop_ModificationDate == prop ||
            Prop_PdfVersion == prop || Prop_PdfProducer == prop || Prop_PdfFileStructure == prop) {
            return NULL;
        }
        return pdfEngine->GetProperty(prop);
    }

    virtual bool SupportsAnnotation(bool forSaving=false) const {
        return !forSaving && pdfEngine && pdfEngine->SupportsAnnotation();
    }
    virtual void UpdateUserAnnotations(Vec<PageAnnotation> *list) {
        if (pdfEngine) pdfEngine->UpdateUserAnnotations(list);
    }

    virtual bool AllowsPrinting() const {
        return pdfEngine ? pdfEngine->AllowsPrinting() : true;
    }
    virtual bool AllowsCopyingText() const {
        return pdfEngine ? pdfEngine->AllowsCopyingText() : true;
    }

    virtual float GetFileDPI() const {
        return pdfEngine ? pdfEngine->GetFileDPI() : 72.0f;
    }
    virtual const WCHAR *GetDefaultFileExt() const {
        return !fileName || !str::EndsWithI(fileName, L".eps") ? L".ps" : L".eps";
    }

    virtual bool BenchLoadPage(int pageNo) {
        return pdfEngine ? pdfEngine->BenchLoadPage(pageNo) : false;
    }

    virtual Vec<PageElement *> *GetElements(int pageNo) {
        return pdfEngine ? pdfEngine->GetElements(pageNo) : NULL;
    }
    virtual PageElement *GetElementAtPos(int pageNo, PointD pt) {
        return pdfEngine ? pdfEngine->GetElementAtPos(pageNo, pt) : NULL;
    }

    virtual PageDestination *GetNamedDest(const WCHAR *name) {
        return pdfEngine ? pdfEngine->GetNamedDest(name) : NULL;
    }
    virtual bool HasTocTree() const {
        return pdfEngine ? pdfEngine->HasTocTree() : false;
    }
    virtual DocTocItem *GetTocTree() {
        return pdfEngine ? pdfEngine->GetTocTree() : NULL;
    }

    virtual char *GetDecryptionKey() const {
        return pdfEngine ? pdfEngine->GetDecryptionKey() : NULL;
    }

    virtual bool SaveFileAsPDF(const WCHAR *copyFileName) {
        return pdfEngine->SaveFileAs(copyFileName);
    }

protected:
    WCHAR *fileName;
    BaseEngine *pdfEngine;

    bool Load(const WCHAR *fileName) {
        assert(!this->fileName && !pdfEngine);
        if (!fileName)
            return false;
        this->fileName = str::Dup(fileName);
        if (file::StartsWith(fileName, "\x1F\x8B"))
            pdfEngine = psgz2pdf(fileName);
        else
            pdfEngine = ps2pdf(fileName);
        return pdfEngine != NULL;
    }
};

bool PsEngine::IsAvailable()
{
    ScopedMem<WCHAR> gswin32c(GetGhostscriptPath());
    return gswin32c.Get() != NULL;
}

bool PsEngine::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    if (!IsAvailable())
        return false;

    if (sniff) {
        char header[2048] = { 0 };
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

    return str::EndsWithI(fileName, L".ps") ||
           str::EndsWithI(fileName, L".ps.gz") ||
           str::EndsWithI(fileName, L".eps");
}

PsEngine *PsEngine::CreateFromFile(const WCHAR *fileName)
{
    PsEngineImpl *engine = new PsEngineImpl();
    if (!engine->Load(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}
