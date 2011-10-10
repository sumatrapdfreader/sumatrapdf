/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "PdfPreview.h"
#include "WinUtil.h"
#ifndef DEBUG
#include <new>
#define NOTHROW (std::nothrow)
#else
#define NOTHROW
#endif

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
#ifdef UNICODE
        HRESULT hr = CLSIDFromString(SZ_PDF_PREVIEW_CLSID, &clsid);
#else
        HRESULT hr = CLSIDFromString(ScopedMem<WCHAR>(str::conv::ToWStr(SZ_PDF_PREVIEW_CLSID)), &clsid);
#endif
        if (SUCCEEDED(hr) && IsEqualCLSID(m_clsid, clsid))
            pObject = new NOTHROW CPdfPreview(&g_lRefCount);
        else
            return CLASS_E_CLASSNOTAVAILABLE;
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
    ScopedComPtr<CClassFactory> pClassFactory(new NOTHROW CClassFactory(rclsid));
    if (!pClassFactory)
        return E_OUTOFMEMORY;
    return pClassFactory->QueryInterface(riid, ppv);
}

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
        { _T("Software\\Classes\\.pdf\\shellex\\{e357fccd-a995-4576-b01f-234630154e96}"),
                NULL,                   SZ_PDF_PREVIEW_CLSID },
        // IPreviewHandler
        { _T("Software\\Classes\\.pdf\\shellex\\{8895b1c6-b41f-4c1c-a562-0d564250836f}"),
                NULL,                   SZ_PDF_PREVIEW_CLSID },
        { _T("Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers"),
                SZ_PDF_PREVIEW_CLSID,   _T("SumatraPDF Preview Handler") },
        { _T("Software\\Classes\\CLSID\\") SZ_PDF_PREVIEW_CLSID,
                _T("AppId"),            IsRunningInWow64() ? _T("{534A1E02-D58F-44f0-B58B-36CBED287C7C}") : _T("{6d2b5079-2f0b-48dd-ab7f-97cec514d30b}") },
    };

    for (int i = 0; i < dimof(regVals); i++) {
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
        _T("Software\\Classes\\.pdf\\shellex\\{e357fccd-a995-4576-b01f-234630154e96}"),
        _T("Software\\Classes\\.pdf\\shellex\\{8895b1c6-b41f-4c1c-a562-0d564250836f}"),
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
