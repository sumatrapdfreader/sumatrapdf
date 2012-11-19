/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "PdfPreview.h"

#include "WinUtil.h"

HINSTANCE g_hInstance = NULL;
long g_lRefCount = 0;


class CClassFactory : public IClassFactory
{
public:
    CClassFactory(REFCLSID rclsid) : m_lRef(1), m_clsid(rclsid)
    {
        InterlockedIncrement(&g_lRefCount);
    }

    ~CClassFactory() { InterlockedDecrement(&g_lRefCount); }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        static const QITAB qit[] =
        {
            QITABENT(CClassFactory, IClassFactory),
            { 0 }
        };
        return QISearch(this, qit, riid, ppv);
    }

    IFACEMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&m_lRef);
    }

    IFACEMETHODIMP_(ULONG) Release()
    {
        long cRef = InterlockedDecrement(&m_lRef);
        if (cRef == 0)
            delete this;
        return cRef;
    }

    // IClassFactory
    IFACEMETHODIMP CreateInstance(IUnknown *punkOuter, REFIID riid, void **ppv)
    {
        *ppv = NULL;
        if (punkOuter)
            return CLASS_E_NOAGGREGATION;

        ScopedComPtr<IInitializeWithStream> pObject;

        CLSID clsid;
        if (SUCCEEDED(CLSIDFromString(SZ_PDF_PREVIEW_CLSID, &clsid)) && IsEqualCLSID(m_clsid, clsid))
            pObject = new CPdfPreview(&g_lRefCount);
#ifdef BUILD_XPS_PREVIEW
        else if (SUCCEEDED(CLSIDFromString(SZ_XPS_PREVIEW_CLSID, &clsid)) && IsEqualCLSID(m_clsid, clsid))
            pObject = new CXpsPreview(&g_lRefCount);
#endif
#ifdef BUILD_CBZ_PREVIEW
        else if (SUCCEEDED(CLSIDFromString(SZ_CBZ_PREVIEW_CLSID, &clsid)) && IsEqualCLSID(m_clsid, clsid))
            pObject = new CCbzPreview(&g_lRefCount);
#endif
#ifdef BUILD_TGA_PREVIEW
        else if (SUCCEEDED(CLSIDFromString(SZ_TGA_PREVIEW_CLSID, &clsid)) && IsEqualCLSID(m_clsid, clsid))
            pObject = new CTgaPreview(&g_lRefCount);
#endif
        else
            return E_NOINTERFACE;
        if (!pObject)
            return E_OUTOFMEMORY;

        return pObject->QueryInterface(riid, ppv);
    }

    IFACEMETHODIMP LockServer(BOOL bLock)
    {
        if (bLock)
            InterlockedIncrement(&g_lRefCount);
        else
            InterlockedDecrement(&g_lRefCount);
        return S_OK;
    }

private:
    long m_lRef;
    CLSID m_clsid;
};



STDAPI_(BOOL) DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
        g_hInstance = hInstance;

    return TRUE;
}

STDAPI DllCanUnloadNow(VOID)
{
    return g_lRefCount == 0 ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
    *ppv = NULL;
    ScopedComPtr<CClassFactory> pClassFactory(new CClassFactory(rclsid));
    if (!pClassFactory)
        return E_OUTOFMEMORY;
    return pClassFactory->QueryInterface(riid, ppv);
}

#define CLSID_I_THUMBNAIL_PROVIDER  _T("{e357fccd-a995-4576-b01f-234630154e96}")
#define CLSID_I_EXTRACT_IMAGE       _T("{bb2e617c-0920-11d1-9a0b-00c04fc2d6c1}")
#define CLSID_I_PREVIEW_HANDLER     _T("{8895b1c6-b41f-4c1c-a562-0d564250836f}")
#define APPID_PREVHOST_EXE          _T("{6d2b5079-2f0b-48dd-ab7f-97cec514d30b}")
#define APPID_PREVHOST_EXE_WOW64    _T("{534a1e02-d58f-44f0-b58b-36cbed287c7c}")

STDAPI DllRegisterServer()
{
    TCHAR path[MAX_PATH];
    if (!GetModuleFileName(g_hInstance, path, dimof(path)))
        return HRESULT_FROM_WIN32(GetLastError());

    struct {
        TCHAR *key, *value, *data;
    } regVals[] = {
        { _T("Software\\Classes\\CLSID\\") SZ_PDF_PREVIEW_CLSID,
                NULL,                   _T("SumatraPDF Preview Handler") },
        { _T("Software\\Classes\\CLSID\\") SZ_PDF_PREVIEW_CLSID _T("\\InProcServer32"),
                NULL,                   path },
        { _T("Software\\Classes\\CLSID\\") SZ_PDF_PREVIEW_CLSID _T("\\InProcServer32"),
                _T("ThreadingModel"),   _T("Apartment") },
        // IThumbnailProvider
        { _T("Software\\Classes\\.pdf\\shellex\\") CLSID_I_THUMBNAIL_PROVIDER,
                NULL,                   SZ_PDF_PREVIEW_CLSID },
        // IExtractImage (for Windows XP)
        { _T("Software\\Classes\\.pdf\\shellex\\") CLSID_I_EXTRACT_IMAGE,
                NULL,                   SZ_PDF_PREVIEW_CLSID },
        // IPreviewHandler
        { _T("Software\\Classes\\.pdf\\shellex\\") CLSID_I_PREVIEW_HANDLER,
                NULL,                   SZ_PDF_PREVIEW_CLSID },
        { _T("Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers"),
                SZ_PDF_PREVIEW_CLSID,   _T("SumatraPDF Preview Handler") },
        { _T("Software\\Classes\\CLSID\\") SZ_PDF_PREVIEW_CLSID,
                _T("AppId"),            IsRunningInWow64() ? APPID_PREVHOST_EXE_WOW64 : APPID_PREVHOST_EXE },
#ifdef BUILD_XPS_PREVIEW
        { _T("Software\\Classes\\CLSID\\") SZ_XPS_PREVIEW_CLSID,
                NULL,                   _T("SumatraPDF XPS Preview Handler") },
        { _T("Software\\Classes\\CLSID\\") SZ_XPS_PREVIEW_CLSID _T("\\InProcServer32"),
                NULL,                   path },
        { _T("Software\\Classes\\CLSID\\") SZ_XPS_PREVIEW_CLSID _T("\\InProcServer32"),
                _T("ThreadingModel"),   _T("Apartment") },
        // IThumbnailProvider
        { _T("Software\\Classes\\.xps\\shellex\\") CLSID_I_THUMBNAIL_PROVIDER,
                NULL,                   SZ_XPS_PREVIEW_CLSID },
        // IExtractImage (for Windows XP)
        { _T("Software\\Classes\\.xps\\shellex\\") CLSID_I_EXTRACT_IMAGE,
                NULL,                   SZ_XPS_PREVIEW_CLSID },
        // IPreviewHandler
        { _T("Software\\Classes\\.xps\\shellex\\") CLSID_I_PREVIEW_HANDLER,
                NULL,                   SZ_XPS_PREVIEW_CLSID },
        { _T("Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers"),
                SZ_XPS_PREVIEW_CLSID,   _T("SumatraPDF XPS Preview Handler") },
        { _T("Software\\Classes\\CLSID\\") SZ_XPS_PREVIEW_CLSID,
                _T("AppId"),            IsRunningInWow64() ? APPID_PREVHOST_EXE_WOW64 : APPID_PREVHOST_EXE },
#endif
#ifdef BUILD_CBZ_PREVIEW
        { _T("Software\\Classes\\CLSID\\") SZ_CBZ_PREVIEW_CLSID,
                NULL,                   _T("SumatraPDF CBZ Preview Handler") },
        { _T("Software\\Classes\\CLSID\\") SZ_CBZ_PREVIEW_CLSID _T("\\InProcServer32"),
                NULL,                   path },
        { _T("Software\\Classes\\CLSID\\") SZ_CBZ_PREVIEW_CLSID _T("\\InProcServer32"),
                _T("ThreadingModel"),   _T("Apartment") },
        // IThumbnailProvider
        { _T("Software\\Classes\\.cbz\\shellex\\") CLSID_I_THUMBNAIL_PROVIDER,
                NULL,                   SZ_CBZ_PREVIEW_CLSID },
        // IExtractImage (for Windows XP)
        { _T("Software\\Classes\\.cbz\\shellex\\") CLSID_I_EXTRACT_IMAGE,
                NULL,                   SZ_CBZ_PREVIEW_CLSID },
        // IPreviewHandler
        { _T("Software\\Classes\\.cbz\\shellex\\") CLSID_I_PREVIEW_HANDLER,
                NULL,                   SZ_CBZ_PREVIEW_CLSID },
        { _T("Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers"),
                SZ_CBZ_PREVIEW_CLSID,   _T("SumatraPDF CBZ Preview Handler") },
        { _T("Software\\Classes\\CLSID\\") SZ_CBZ_PREVIEW_CLSID,
                _T("AppId"),            IsRunningInWow64() ? APPID_PREVHOST_EXE_WOW64 : APPID_PREVHOST_EXE },
#endif
#ifdef BUILD_TGA_PREVIEW
        { _T("Software\\Classes\\CLSID\\") SZ_TGA_PREVIEW_CLSID,
                NULL,                   _T("SumatraPDF TGA Preview Handler") },
        { _T("Software\\Classes\\CLSID\\") SZ_TGA_PREVIEW_CLSID _T("\\InProcServer32"),
                NULL,                   path },
        { _T("Software\\Classes\\CLSID\\") SZ_TGA_PREVIEW_CLSID _T("\\InProcServer32"),
                _T("ThreadingModel"),   _T("Apartment") },
        // IThumbnailProvider
        { _T("Software\\Classes\\.tga\\shellex\\") CLSID_I_THUMBNAIL_PROVIDER,
                NULL,                   SZ_TGA_PREVIEW_CLSID },
        // IExtractImage (for Windows XP)
        { _T("Software\\Classes\\.tga\\shellex\\") CLSID_I_EXTRACT_IMAGE,
                NULL,                   SZ_TGA_PREVIEW_CLSID },
        // IPreviewHandler
        { _T("Software\\Classes\\.tga\\shellex\\") CLSID_I_PREVIEW_HANDLER,
                NULL,                   SZ_TGA_PREVIEW_CLSID },
        { _T("Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers"),
                SZ_TGA_PREVIEW_CLSID,   _T("SumatraPDF TGA Preview Handler") },
        { _T("Software\\Classes\\CLSID\\") SZ_TGA_PREVIEW_CLSID,
                _T("AppId"),            IsRunningInWow64() ? APPID_PREVHOST_EXE_WOW64 : APPID_PREVHOST_EXE },
#endif
    };

    for (int i = 0; i < dimof(regVals); i++) {
        // don't register for IExtractImage on systems which accept IThumbnailProvider
        // (because it doesn't offer anything beyond what IThumbnailProvider does)
        if (WindowsVerVistaOrGreater() && str::EndsWith(regVals[i].key, CLSID_I_EXTRACT_IMAGE))
            continue;
        WriteRegStr(HKEY_LOCAL_MACHINE, regVals[i].key, regVals[i].value, regVals[i].data);
        bool ok = WriteRegStr(HKEY_CURRENT_USER, regVals[i].key, regVals[i].value, regVals[i].data);
        if (!ok)
            return E_FAIL;
    }

    return S_OK;
}

STDAPI DllUnregisterServer()
{
    const TCHAR *regKeys[] = {
        _T("Software\\Classes\\CLSID\\") SZ_PDF_PREVIEW_CLSID,
        _T("Software\\Classes\\.pdf\\shellex\\") CLSID_I_THUMBNAIL_PROVIDER,
        _T("Software\\Classes\\.pdf\\shellex\\") CLSID_I_EXTRACT_IMAGE,
        _T("Software\\Classes\\.pdf\\shellex\\") CLSID_I_PREVIEW_HANDLER,
#ifdef BUILD_XPS_PREVIEW
        _T("Software\\Classes\\CLSID\\") SZ_XPS_PREVIEW_CLSID,
        _T("Software\\Classes\\.xps\\shellex\\") CLSID_I_THUMBNAIL_PROVIDER,
        _T("Software\\Classes\\.xps\\shellex\\") CLSID_I_EXTRACT_IMAGE,
        _T("Software\\Classes\\.xps\\shellex\\") CLSID_I_PREVIEW_HANDLER,
#endif
#ifdef BUILD_CBZ_PREVIEW
        _T("Software\\Classes\\CLSID\\") SZ_CBZ_PREVIEW_CLSID,
        _T("Software\\Classes\\.cbz\\shellex\\") CLSID_I_THUMBNAIL_PROVIDER,
        _T("Software\\Classes\\.cbz\\shellex\\") CLSID_I_EXTRACT_IMAGE,
        _T("Software\\Classes\\.cbz\\shellex\\") CLSID_I_PREVIEW_HANDLER,
#endif
#ifdef BUILD_TGA_PREVIEW
        _T("Software\\Classes\\CLSID\\") SZ_TGA_PREVIEW_CLSID,
        _T("Software\\Classes\\.tga\\shellex\\") CLSID_I_THUMBNAIL_PROVIDER,
        _T("Software\\Classes\\.tga\\shellex\\") CLSID_I_EXTRACT_IMAGE,
        _T("Software\\Classes\\.tga\\shellex\\") CLSID_I_PREVIEW_HANDLER,
#endif
    };

    HRESULT hr = S_OK;

    for (int i = 0; i < dimof(regKeys); i++) {
        DeleteRegKey(HKEY_LOCAL_MACHINE, regKeys[i]);
        bool ok = DeleteRegKey(HKEY_CURRENT_USER, regKeys[i]);
        if (!ok)
            hr = E_FAIL;
    }
    SHDeleteValue(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers"), SZ_PDF_PREVIEW_CLSID);
    SHDeleteValue(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers"), SZ_PDF_PREVIEW_CLSID);
#ifdef BUILD_XPS_PREVIEW
    SHDeleteValue(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers"), SZ_XPS_PREVIEW_CLSID);
    SHDeleteValue(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers"), SZ_XPS_PREVIEW_CLSID);
#endif
#ifdef BUILD_CBZ_PREVIEW
    SHDeleteValue(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers"), SZ_CBZ_PREVIEW_CLSID);
    SHDeleteValue(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers"), SZ_CBZ_PREVIEW_CLSID);
#endif
#ifdef BUILD_TGA_PREVIEW
    SHDeleteValue(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers"), SZ_TGA_PREVIEW_CLSID);
    SHDeleteValue(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers"), SZ_TGA_PREVIEW_CLSID);
#endif

    return hr;
}

#ifdef _WIN64
#pragma comment(linker, "/EXPORT:DllCanUnloadNow=DllCanUnloadNow,PRIVATE")
#pragma comment(linker, "/EXPORT:DllGetClassObject=DllGetClassObject,PRIVATE")
#pragma comment(linker, "/EXPORT:DllRegisterServer=DllRegisterServer,PRIVATE")
#pragma comment(linker, "/EXPORT:DllUnregisterServer=DllUnregisterServer,PRIVATE")
#else
#pragma comment(linker, "/EXPORT:DllCanUnloadNow=_DllCanUnloadNow@0,PRIVATE")
#pragma comment(linker, "/EXPORT:DllGetClassObject=_DllGetClassObject@12,PRIVATE")
#pragma comment(linker, "/EXPORT:DllRegisterServer=_DllRegisterServer@0,PRIVATE")
#pragma comment(linker, "/EXPORT:DllUnregisterServer=_DllUnregisterServer@0,PRIVATE")
#endif
