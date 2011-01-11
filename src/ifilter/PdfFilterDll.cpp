/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include <windows.h>
#include <new>
#include <shlwapi.h>
#include <tchar.h>
#include "CPdfFilter.h"

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
        
        CLSID clsid;
        if (!SUCCEEDED(CLSIDFromString(SZ_PDF_FILTER_CLSID, &clsid)) ||
            !IsEqualCLSID(m_clsid, clsid))
            return CLASS_E_CLASSNOTAVAILABLE;
            
        IFilter *pFilter = new (std::nothrow) CPdfFilter(&g_lRefCount);
        if (!pFilter)
            return E_OUTOFMEMORY;
        
        HRESULT hr = pFilter->QueryInterface(riid, ppv);
        pFilter->Release();
        return hr;
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
    HRESULT hr = E_OUTOFMEMORY;
    *ppv = NULL;
    CClassFactory *pClassFactory = new (std::nothrow) CClassFactory(rclsid);
    if (pClassFactory) {
        hr = pClassFactory->QueryInterface(riid, ppv);
        pClassFactory->Release();
    }
    return hr;
}

struct REGISTRY_ENTRY {
    LPWSTR pszKeyName;
    LPWSTR pszValueName;
    LPWSTR pszData;
};

HRESULT CreateRegKeyAndSetValue(HKEY hKeyRoot, const REGISTRY_ENTRY *pRegistryEntry)
{
    HRESULT hr;
    HKEY hKey;

    LONG lRet = RegCreateKeyExW(hKeyRoot, pRegistryEntry->pszKeyName,
                                0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL);
    if (lRet != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(lRet);

    lRet = RegSetValueExW(hKey, pRegistryEntry->pszValueName, 0, REG_SZ,
                          (LPBYTE)pRegistryEntry->pszData,
                          ((DWORD)lstrlenW(pRegistryEntry->pszData) + 1) * sizeof(WCHAR));

    hr = HRESULT_FROM_WIN32(lRet);
    RegCloseKey(hKey);

    return hr;
}

STDAPI DllRegisterServer()
{
    HRESULT hr;

    WCHAR szModuleName[MAX_PATH];

    if (!GetModuleFileNameW(g_hInstance, szModuleName, ARRAYSIZE(szModuleName)))
        return HRESULT_FROM_WIN32(GetLastError());

    const REGISTRY_ENTRY rgRegistryEntries[] = {
        { L"Software\\Classes\\CLSID\\" SZ_PDF_FILTER_CLSID,                                     NULL,               L"SumatraPDF IFilter"},
        { L"Software\\Classes\\CLSID\\" SZ_PDF_FILTER_CLSID L"\\InProcServer32",                 NULL,               szModuleName},
        { L"Software\\Classes\\CLSID\\" SZ_PDF_FILTER_CLSID L"\\InProcServer32",                 L"ThreadingModel",  L"Both"},
        { L"Software\\Classes\\CLSID\\" SZ_PDF_FILTER_HANDLER,                                   NULL,               L"SumatraPDF IFilter Persistent Handler"},
        { L"Software\\Classes\\CLSID\\" SZ_PDF_FILTER_HANDLER L"\\PersistentAddinsRegistered",   NULL,               L""},
        { L"Software\\Classes\\CLSID\\" SZ_PDF_FILTER_HANDLER L"\\PersistentAddinsRegistered\\{89BCB740-6119-101A-BCB7-00DD010655AF}", NULL, SZ_PDF_FILTER_CLSID},
        { L"Software\\Classes\\.pdf\\PersistentHandler",                                         NULL,               SZ_PDF_FILTER_HANDLER},
    };

    hr = S_OK;

    for (int i = 0; i < ARRAYSIZE(rgRegistryEntries) && SUCCEEDED(hr); i++)
    {
        hr = CreateRegKeyAndSetValue(HKEY_LOCAL_MACHINE, &rgRegistryEntries[i]);
        hr = CreateRegKeyAndSetValue(HKEY_CURRENT_USER, &rgRegistryEntries[i]);
    }

    return hr;
}

STDAPI DllUnregisterServer()
{
    HRESULT hr = S_OK;

    const LPWSTR rgpszKeys[] = {
        L"Software\\Classes\\CLSID\\" SZ_PDF_FILTER_CLSID,
        L"Software\\Classes\\CLSID\\" SZ_PDF_FILTER_HANDLER,
        L"Software\\Classes\\.pdf\\PersistentHandler"
    };

    for (int i = 0; i < ARRAYSIZE(rgpszKeys) && SUCCEEDED(hr); i++) {
        DWORD dwError = SHDeleteKeyW(HKEY_LOCAL_MACHINE, rgpszKeys[i]);
        dwError = SHDeleteKeyW(HKEY_CURRENT_USER, rgpszKeys[i]);
        if (ERROR_FILE_NOT_FOUND == dwError)
            hr = S_OK;
        else
            hr = HRESULT_FROM_WIN32(dwError);
    }
    return hr;
}

#pragma comment(linker, "/EXPORT:DllCanUnloadNow=_DllCanUnloadNow@0,PRIVATE")
#pragma comment(linker, "/EXPORT:DllGetClassObject=_DllGetClassObject@12,PRIVATE")
#pragma comment(linker, "/EXPORT:DllRegisterServer=_DllRegisterServer@0,PRIVATE")
#pragma comment(linker, "/EXPORT:DllUnregisterServer=_DllUnregisterServer@0,PRIVATE")
