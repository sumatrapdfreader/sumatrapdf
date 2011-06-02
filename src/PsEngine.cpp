/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "PsEngine.h"
#include "PdfEngine.h"
#include "FileUtil.h"
#include "WinUtil.h"

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
    for (int i = 0; i < dimof(gsProducts); i++) {
        HKEY hkey;
        ScopedMem<TCHAR> keyName(str::Join(_T("Software\\"), gsProducts[i]));
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyName, 0, KEY_READ, &hkey) != ERROR_SUCCESS)
            continue;
        TCHAR subkey[32];
        for (DWORD ix = 0; RegEnumKey(hkey, ix, subkey, dimof(subkey)) == ERROR_SUCCESS; ix++)
            versions.Append(str::Dup(subkey));
        RegCloseKey(hkey);
    }
    versions.SortNatural();

    // return the path to the newest installation
    for (size_t ix = versions.Count(); ix > 0; ix--) {
        for (int i = 0; i < dimof(gsProducts); i++) {
            ScopedMem<TCHAR> keyName(str::Format(_T("Software\\%s\\%s"), gsProducts[i], versions[ix-1]));
            ScopedMem<TCHAR> GS_DLL(ReadRegStr(HKEY_LOCAL_MACHINE, keyName, _T("GS_DLL")));
            if (!GS_DLL)
                continue;
            ScopedMem<TCHAR> dir(path::GetDir(GS_DLL));
            if (str::IsEmpty(dir.Get()))
                continue;
            ScopedMem<TCHAR> exe(path::Join(dir, _T("gswin32c.exe")));
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
            ScopedMem<TCHAR> exe(path::Join(paths[ix], _T("gswin32c.exe")));
            if (file::Exists(exe))
                return exe.StealData();
        }
    }

    return NULL;
}

class ScopedFile {
    TCHAR *path;

public:
    ScopedFile(const TCHAR *path) : path(str::Dup(path)) { }
    ~ScopedFile() {
        if (path)
            file::Delete(path);
        free(path);
    }
};

static TCHAR *GetTempFileName(const TCHAR *prefix=_T("PsE"))
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
    ScopedMem<TCHAR> tmpFile(GetTempFileName());
    ScopedFile tmpFileScope(tmpFile);
    ScopedMem<TCHAR> gswin32c(GetGhostscriptPath());
    if (!tmpFile || !gswin32c)
        return NULL;
    ScopedMem<TCHAR> cmdLine(str::Format(_T("\"%s\" -q -dSAFER -dNOPAUSE -dBATCH -sOutputFile=\"%s\" -sDEVICE=pdfwrite -c .setpdfwrite -f \"%s\""), gswin32c, tmpFile, fileName));

    // TODO: the PS-to-PDF conversion can hang the UI for several seconds
    HANDLE process = LaunchProcess(cmdLine, CREATE_NO_WINDOW);
    if (!process)
        return NULL;

    WaitForSingleObject(process, 10000);
    TerminateProcess(process, 1);
    CloseHandle(process);

    size_t len;
    ScopedMem<char> pdfData(file::ReadAll(tmpFile, &len));
    if (!pdfData)
        return NULL;

    IStream *stream = CreateStreamFromData(pdfData, len);
    if (!stream)
        return NULL;

    PdfEngine *engine = PdfEngine::CreateFromStream(stream);
    stream->Release();

    return engine;
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
    virtual const TCHAR *GetDefaultFileExt() const { return _T(".ps"); }

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
    virtual bool HasToCTree() const {
        return pdfEngine ? pdfEngine->HasToCTree() : false;
    }
    virtual DocToCItem *GetToCTree() {
        return pdfEngine ? pdfEngine->GetToCTree() : NULL;
    }

    virtual char *GetDecryptionKey() const {
        return pdfEngine ? pdfEngine->GetDecryptionKey() : NULL;
    }
    virtual void RunGC() {
        if (pdfEngine) pdfEngine->RunGC();
    }

protected:
    const TCHAR *fileName;
    PdfEngine *pdfEngine;

    bool Load(const TCHAR *fileName) {
        assert(!this->fileName && !pdfEngine);
        if (fileName)
            this->fileName = str::Dup(fileName);
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
        char header[2] = { 0 };
        file::ReadAll(fileName, header, sizeof(header));
        return str::EqN(header, "%!", sizeof(header));
    }

    return str::EndsWithI(fileName, _T(".ps"));
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
