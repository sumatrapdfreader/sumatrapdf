/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "PsEngine.h"
#include "PdfEngine.h"
#include "FileUtil.h"
#include "WinUtil.h"
#include "Scopes.h"

#include <zlib.h>
extern "C" gzFile ZEXPORT gzwopen(const wchar_t *path, const char *mode);

static TCHAR *GetGhostscriptPath()
{
    TCHAR *gsProducts[] = {
        _T("AFPL Ghostscript"),
        _T("Aladdin Ghostscript"),
        _T("GPL Ghostscript"),
        _T("GNU Ghostscript"),
    };

    // find all installed Ghostscript versions
    StrVec versions;
    REGSAM access = KEY_READ | KEY_WOW64_32KEY;
TryAgain64Bit:
    for (int i = 0; i < dimof(gsProducts); i++) {
        HKEY hkey;
        ScopedMem<TCHAR> keyName(str::Join(_T("Software\\"), gsProducts[i]));
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyName, 0, access, &hkey) != ERROR_SUCCESS)
            continue;
        TCHAR subkey[32];
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
            ScopedMem<TCHAR> keyName(str::Format(_T("Software\\%s\\%s"),
                                                 gsProducts[i], versions.At(ix - 1)));
            ScopedMem<TCHAR> GS_DLL(ReadRegStr(HKEY_LOCAL_MACHINE, keyName, _T("GS_DLL")));
            if (!GS_DLL)
                continue;
            ScopedMem<TCHAR> dir(path::GetDir(GS_DLL));
            ScopedMem<TCHAR> exe(path::Join(dir, _T("gswin32c.exe")));
            if (file::Exists(exe))
                return exe.StealData();
            exe.Set(path::Join(dir, _T("gswin64c.exe")));
            if (file::Exists(exe))
                return exe.StealData();
        }
    }

    // if Ghostscript isn't found in the Registry, try finding it in the %PATH%
    DWORD size = GetEnvironmentVariable(_T("PATH"), NULL, 0);
    ScopedMem<TCHAR> envpath(SAZA(TCHAR, size));
    if (size > 0 && envpath) {
        GetEnvironmentVariable(_T("PATH"), envpath, size);
        StrVec paths;
        paths.Split(envpath, _T(";"), true);
        for (size_t ix = 0; ix < paths.Count(); ix++) {
            ScopedMem<TCHAR> exe(path::Join(paths.At(ix), _T("gswin32c.exe")));
            if (file::Exists(exe))
                return exe.StealData();
            exe.Set(path::Join(paths.At(ix), _T("gswin64c.exe")));
            if (file::Exists(exe))
                return exe.StealData();
        }
    }

    return NULL;
}

class ScopedFile {
    TCHAR *path;

public:
    ScopedFile(const TCHAR *path) : path(path ? str::Dup(path) : NULL) { }
    ~ScopedFile() {
        if (path)
            file::Delete(path);
        free(path);
    }
};

// caller must free() the result
static TCHAR *GetTempFilePath(const TCHAR *prefix=_T("PsE"))
{
    TCHAR path[MAX_PATH], shortPath[MAX_PATH];
    DWORD res = GetTempPath(MAX_PATH - 14, shortPath);
    if (!res || res >= MAX_PATH - 14 || !GetTempFileName(shortPath, prefix, 0, path))
        return NULL;

    res = GetShortPathName(path, shortPath, dimof(shortPath));
    if (!res || res >= dimof(shortPath))
        return str::Dup(path);
    return str::Dup(shortPath);
}

static PdfEngine *ps2pdf(const TCHAR *fileName)
{
    // TODO: read from gswin32c's stdout instead of using a TEMP file
    ScopedMem<TCHAR> tmpFile(GetTempFilePath());
    ScopedFile tmpFileScope(tmpFile);
    ScopedMem<TCHAR> gswin32c(GetGhostscriptPath());
    if (!tmpFile || !gswin32c)
        return NULL;
    ScopedMem<TCHAR> cmdLine(str::Format(_T("\"%s\" -q -dSAFER -dNOPAUSE -dBATCH -dEPSCrop -sOutputFile=\"%s\" -sDEVICE=pdfwrite -c .setpdfwrite -f \"%s\""), gswin32c, tmpFile, fileName));

    if (getenv("MULOG")) {
        _tprintf(_T("ps2pdf: using Ghostscript from '%s'\n"), gswin32c);
        _tprintf(_T("ps2pdf: for creating '%s'\n"), tmpFile);
    }

    // TODO: the PS-to-PDF conversion can hang the UI for several seconds
    HANDLE process = LaunchProcess(cmdLine, CREATE_NO_WINDOW);
    if (!process)
        return NULL;

    DWORD exitCode = EXIT_FAILURE;
    WaitForSingleObject(process, 10000);
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

    if (getenv("MULOG"))
        _tprintf(_T("ps2pdf: PDF conversion successful\n"));

    return PdfEngine::CreateFromStream(stream);
}

inline bool isgzipped(const TCHAR *fileName)
{
    char header[2] = { 0 };
    file::ReadAll(fileName, header, sizeof(header));
    return str::EqN(header, "\x1F\x8B", sizeof(header));
}

static PdfEngine *psgz2pdf(const TCHAR *fileName)
{
    ScopedMem<TCHAR> tmpFile(GetTempFilePath());
    ScopedFile tmpFileScope(tmpFile);
    if (!tmpFile)
        return NULL;

#ifdef UNICODE
    gzFile inFile = gzwopen(fileName, "rb");
#else
    gzFile inFile = gzopen(fileName, "rb");
#endif
    if (!inFile)
        return NULL;
    FILE *outFile = _tfopen(tmpFile, _T("wb"));
    if (!outFile) {
        gzclose(inFile);
        return NULL;
    }

    char buffer[1 << 14];
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

// CPsEngine is mostly a proxy for a PdfEngine that's fed whatever
// the ps2pdf conversion from Ghostscript returns
class CPsEngine : public PsEngine {
    friend PsEngine;

public:
    CPsEngine() : fileName(NULL), pdfEngine(NULL) { }
    virtual ~CPsEngine() {
        free((void *)fileName);
        delete pdfEngine;
    }
    virtual CPsEngine *Clone() {
        CPsEngine *clone = new CPsEngine();
        if (fileName)  clone->fileName = str::Dup(fileName);
        if (pdfEngine) clone->pdfEngine = static_cast<PdfEngine *>(pdfEngine->Clone());
        return clone;
    }

    virtual const TCHAR *FileName() const { return fileName; };
    virtual int PageCount() const {
        return pdfEngine ? pdfEngine->PageCount() : 0;
    }

    virtual int PageRotation(int pageNo) {
        return pdfEngine ? pdfEngine->PageRotation(pageNo) : 0;
    }
    virtual RectD PageMediabox(int pageNo) {
        return pdfEngine ? pdfEngine->PageMediabox(pageNo) : RectD();
    }
    virtual RectD PageContentBox(int pageNo, RenderTarget target=Target_View) {
        return pdfEngine ? pdfEngine->PageContentBox(pageNo, target) : RectD();
    }

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View) {
        return pdfEngine ? pdfEngine->RenderBitmap(pageNo, zoom, rotation, pageRect, target) : NULL;
    }
    virtual bool RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, RenderTarget target=Target_View) {
        return pdfEngine ? pdfEngine->RenderPage(hDC, screenRect, pageNo, zoom, rotation, pageRect, target) : false;
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
    virtual TCHAR * ExtractPageText(int pageNo, TCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View) {
        return pdfEngine ? pdfEngine->ExtractPageText(pageNo, lineSep, coords_out, target) : NULL;
    }
    virtual bool IsImagePage(int pageNo) {
        return pdfEngine ? pdfEngine->IsImagePage(pageNo) : false;
    }
    virtual PageLayoutType PreferredLayout() {
        return pdfEngine ? pdfEngine->PreferredLayout() : Layout_Single;
    }
    virtual TCHAR *GetProperty(char *name) { return NULL; }

    virtual bool IsPrintingAllowed() {
        return pdfEngine ? pdfEngine->IsPrintingAllowed() : true;
    }
    virtual bool IsCopyingTextAllowed() {
        return pdfEngine ? pdfEngine->IsCopyingTextAllowed() : true;
    }

    virtual float GetFileDPI() const {
        return pdfEngine ? pdfEngine->GetFileDPI() : 72.0f;
    }
    virtual const TCHAR *GetDefaultFileExt() const {
        return !fileName || !str::EndsWithI(fileName, _T(".eps")) ? _T(".ps") : _T(".eps");
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

    virtual PageDestination *GetNamedDest(const TCHAR *name) {
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
    virtual void RunGC() {
        if (pdfEngine) pdfEngine->RunGC();
    }

    virtual unsigned char *GetPDFData(size_t *cbCount) {
        return pdfEngine->GetFileData(cbCount);
    }

protected:
    const TCHAR *fileName;
    PdfEngine *pdfEngine;

    bool Load(const TCHAR *fileName) {
        assert(!this->fileName && !pdfEngine);
        if (!fileName)
            return false;
        this->fileName = str::Dup(fileName);
        if (isgzipped(fileName))
            pdfEngine = psgz2pdf(fileName);
        else
            pdfEngine = ps2pdf(fileName);
        return pdfEngine != NULL;
    }
};

bool PsEngine::IsAvailable()
{
    ScopedMem<TCHAR> gswin32c(GetGhostscriptPath());
    return gswin32c.Get() != NULL;
}

bool PsEngine::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    if (!IsAvailable())
        return false;

    if (sniff) {
        char header[1024];
        ZeroMemory(header, sizeof(header));
        file::ReadAll(fileName, header, sizeof(header) - 1);
        if (str::StartsWith(header, "\xC5\xD0\xD3\xC6")) {
            // Windows-format EPS file - cf. http://partners.adobe.com/public/developer/en/ps/5002.EPSF_Spec.pdf
            DWORD psStart = *(DWORD *)(header + 4);
            return psStart >= sizeof(header) - 12 || str::StartsWith(header + psStart, "%!PS-Adobe-");
        }
        return str::StartsWith(header, "%!") ||
               // also sniff PJL (Printer Job Language) files containing Postscript data
               str::StartsWith(header, "\x1B%-12345X@PJL") && str::Find(header, "\n%!PS-Adobe-");
    }

    return str::EndsWithI(fileName, _T(".ps")) ||
           str::EndsWithI(fileName, _T(".ps.gz")) ||
           str::EndsWithI(fileName, _T(".eps"));
}

PsEngine *PsEngine::CreateFromFileName(const TCHAR *fileName)
{
    CPsEngine *engine = new CPsEngine();
    if (!engine->Load(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;    
}
