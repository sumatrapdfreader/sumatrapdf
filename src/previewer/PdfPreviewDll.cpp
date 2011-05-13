/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

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
        
        IInitializeWithStream *pObject = NULL;
        
        CLSID clsid;
        if (SUCCEEDED(CLSIDFromString(SZ_PDF_PREVIEW_CLSID, &clsid)) && IsEqualCLSID(m_clsid, clsid))
            pObject = new /*(std::nothrow)*/ CPdfPreview(&g_lRefCount);
        else
            return CLASS_E_CLASSNOTAVAILABLE;
        if (!pObject)
            return E_OUTOFMEMORY;
        
        HRESULT hr = pObject->QueryInterface(riid, ppv);
        pObject->Release();
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
    CClassFactory *pClassFactory = new /*(std::nothrow)*/ CClassFactory(rclsid);
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

static bool IsWow64()
{
#ifndef _WIN64
    typedef BOOL (WINAPI *IsWow64ProcessProc)(HANDLE, PBOOL);
    IsWow64ProcessProc _IsWow64Process = (IsWow64ProcessProc)LoadDllFunc(_T("kernel32.dll"), "IsWow64Process");
    BOOL isWow = FALSE;
    if (_IsWow64Process)
        _IsWow64Process(GetCurrentProcess(), &isWow);
    return isWow;
#else
    return false;
#endif
}

STDAPI DllRegisterServer()
{
    WCHAR szModuleName[MAX_PATH];

    if (!GetModuleFileNameW(g_hInstance, szModuleName, ARRAYSIZE(szModuleName)))
        return HRESULT_FROM_WIN32(GetLastError());

    const REGISTRY_ENTRY rgRegistryEntries[] = {
        { L"Software\\Classes\\CLSID\\" SZ_PDF_PREVIEW_CLSID,                                     NULL,               L"SumatraPDF Preview Handler" },
        { L"Software\\Classes\\CLSID\\" SZ_PDF_PREVIEW_CLSID L"\\InProcServer32",                 NULL,               szModuleName },
        { L"Software\\Classes\\CLSID\\" SZ_PDF_PREVIEW_CLSID L"\\InProcServer32",                 L"ThreadingModel",  L"Apartment" },
        // IThumbnailProvider
        { L"Software\\Classes\\.pdf\\shellex\\{e357fccd-a995-4576-b01f-234630154e96}",            NULL,               SZ_PDF_PREVIEW_CLSID },
        // IPreviewHandler
        { L"Software\\Classes\\.pdf\\shellex\\{8895b1c6-b41f-4c1c-a562-0d564250836f}",            NULL,               SZ_PDF_PREVIEW_CLSID },
        { L"Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers",                       SZ_PDF_PREVIEW_CLSID, L"SumatraPDF Preview Handler" },
        { L"Software\\Classes\\CLSID\\" SZ_PDF_PREVIEW_CLSID,                                     L"AppId",           IsWow64() ? L"{534A1E02-D58F-44f0-B58B-36CBED287C7C}" : L"{6d2b5079-2f0b-48dd-ab7f-97cec514d30b}" },
    };

    HRESULT hr = S_OK;

    for (int i = 0; i < ARRAYSIZE(rgRegistryEntries) && SUCCEEDED(hr); i++) {
        hr = CreateRegKeyAndSetValue(HKEY_LOCAL_MACHINE, &rgRegistryEntries[i]);
        hr = CreateRegKeyAndSetValue(HKEY_CURRENT_USER, &rgRegistryEntries[i]);
    }

    return hr;
}

STDAPI DllUnregisterServer()
{
    const LPWSTR rgpszKeys[] = {
        L"Software\\Classes\\CLSID\\" SZ_PDF_PREVIEW_CLSID,
        L"Software\\Classes\\.pdf\\shellex\\{e357fccd-a995-4576-b01f-234630154e96}",
        L"Software\\Classes\\.pdf\\shellex\\{8895b1c6-b41f-4c1c-a562-0d564250836f}",
    };

    HRESULT hr = S_OK;

    for (int i = 0; i < ARRAYSIZE(rgpszKeys) && SUCCEEDED(hr); i++) {
        DWORD dwError = SHDeleteKeyW(HKEY_LOCAL_MACHINE, rgpszKeys[i]);
        dwError = SHDeleteKeyW(HKEY_CURRENT_USER, rgpszKeys[i]);
        if (ERROR_FILE_NOT_FOUND == dwError)
            hr = S_OK;
        else
            hr = HRESULT_FROM_WIN32(dwError);
    }
    SHDeleteValueW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers", SZ_PDF_PREVIEW_CLSID);
    SHDeleteValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers", SZ_PDF_PREVIEW_CLSID);

    return hr;
}

#pragma comment(linker, "/EXPORT:DllCanUnloadNow=_DllCanUnloadNow@0,PRIVATE")
#pragma comment(linker, "/EXPORT:DllGetClassObject=_DllGetClassObject@12,PRIVATE")
#pragma comment(linker, "/EXPORT:DllRegisterServer=_DllRegisterServer@0,PRIVATE")
#pragma comment(linker, "/EXPORT:DllUnregisterServer=_DllUnregisterServer@0,PRIVATE")
