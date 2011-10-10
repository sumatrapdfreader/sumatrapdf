/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "WinUtil.h"
#include "Scopes.h"
#ifndef DEBUG
#include <new>
#define NOTHROW (std::nothrow)
#else
#define NOTHROW
#endif
#include "CPdfFilter.h"
#ifdef BUILD_TEX_IFILTER
#include "CTeXFilter.h"
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

        ScopedComPtr<IFilter> pFilter;

        CLSID clsid;
        if (SUCCEEDED(CLSIDFromTString(SZ_PDF_FILTER_CLSID, &clsid)) && IsEqualCLSID(m_clsid, clsid))
            pFilter = new NOTHROW CPdfFilter(&g_lRefCount);
#ifdef BUILD_TEX_IFILTER
        else if (SUCCEEDED(CLSIDFromTString(SZ_TEX_FILTER_CLSID, &clsid)) && IsEqualCLSID(m_clsid, clsid))
            pFilter = new NOTHROW CTeXFilter(&g_lRefCount);
#endif
        else
            return CLASS_E_CLASSNOTAVAILABLE;
        if (!pFilter)
            return E_OUTOFMEMORY;

        return pFilter->QueryInterface(riid, ppv);
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
    WCHAR path[MAX_PATH];
    if (!GetModuleFileName(g_hInstance, path, dimof(path)))
        return HRESULT_FROM_WIN32(GetLastError());

    struct {
        TCHAR *key, *value, *data;
    } regVals[] = {
        { _T("Software\\Classes\\CLSID\\") SZ_PDF_FILTER_CLSID,
                NULL,                   _T("SumatraPDF IFilter") },
        { _T("Software\\Classes\\CLSID\\") SZ_PDF_FILTER_CLSID _T("\\InProcServer32"),
                NULL,                   path },
        { _T("Software\\Classes\\CLSID\\") SZ_PDF_FILTER_CLSID _T("\\InProcServer32"),
                _T("ThreadingModel"),   _T("Both") },
        { _T("Software\\Classes\\CLSID\\") SZ_PDF_FILTER_HANDLER,
                NULL,                   _T("SumatraPDF IFilter Persistent Handler") },
        { _T("Software\\Classes\\CLSID\\") SZ_PDF_FILTER_HANDLER _T("\\PersistentAddinsRegistered"),
                NULL,                   _T("") },
        { _T("Software\\Classes\\CLSID\\") SZ_PDF_FILTER_HANDLER _T("\\PersistentAddinsRegistered\\{89BCB740-6119-101A-BCB7-00DD010655AF}"),
                NULL,                   SZ_PDF_FILTER_CLSID },
        { _T("Software\\Classes\\.pdf\\PersistentHandler"),
                NULL,                   SZ_PDF_FILTER_HANDLER },
#ifdef BUILD_TEX_IFILTER
        { _T("Software\\Classes\\CLSID\\") SZ_TEX_FILTER_CLSID,
                NULL,                   _T("SumatraPDF IFilter") },
        { _T("Software\\Classes\\CLSID\\") SZ_TEX_FILTER_CLSID _T("\\InProcServer32"),
                NULL,                   path },
        { _T("Software\\Classes\\CLSID\\") SZ_TEX_FILTER_CLSID _T("\\InProcServer32"),
                _T("ThreadingModel"),  _T("Both") },
        { _T("Software\\Classes\\CLSID\\") SZ_TEX_FILTER_HANDLER,
                NULL,                   _T("SumatraPDF LaTeX IFilter Persistent Handler") },
        { _T("Software\\Classes\\CLSID\\") SZ_TEX_FILTER_HANDLER _T("\\PersistentAddinsRegistered"),
                NULL,                   _T("") },
        { _T("Software\\Classes\\CLSID\\") SZ_TEX_FILTER_HANDLER _T("\\PersistentAddinsRegistered\\{89BCB740-6119-101A-BCB7-00DD010655AF}"),
                NULL,                   SZ_TEX_FILTER_CLSID },
        { _T("Software\\Classes\\.tex\\PersistentHandler"),
                NULL,                   SZ_TEX_FILTER_HANDLER },
#endif
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
        _T("Software\\Classes\\CLSID\\") SZ_PDF_FILTER_CLSID,
        _T("Software\\Classes\\CLSID\\") SZ_PDF_FILTER_HANDLER,
        _T("Software\\Classes\\.pdf\\PersistentHandler"),
#ifdef BUILD_TEX_IFILTER
        _T("Software\\Classes\\CLSID\\") SZ_TEX_FILTER_CLSID,
        _T("Software\\Classes\\CLSID\\") SZ_TEX_FILTER_HANDLER,
        _T("Software\\Classes\\.tex\\PersistentHandler"),
#endif
    };

    HRESULT hr = S_OK;

    for (int i = 0; i < dimof(regKeys); i++) {
        DeleteRegKey(HKEY_LOCAL_MACHINE, regKeys[i]);
        bool ok = DeleteRegKey(HKEY_CURRENT_USER, regKeys[i]);
        if (!ok)
            hr = E_FAIL;
    }

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
