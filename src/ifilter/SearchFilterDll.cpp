/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"

#include "FilterBase.h"
#include "RegistrySearchFilter.h"
#include "PdfFilter.h"
#ifdef BUILD_TEX_IFILTER
#include "TeXFilter.h"
#endif
#ifdef BUILD_EPUB_IFILTER
#include "EpubFilter.h"
#endif

#include "utils/Log.h"

long g_lRefCount = 0;

class FilterClassFactory : public IClassFactory {
  public:
    explicit FilterClassFactory(REFCLSID rclsid) : m_lRef(1), m_clsid(rclsid) {
        InterlockedIncrement(&g_lRefCount);
    }

    ~FilterClassFactory() {
        InterlockedDecrement(&g_lRefCount);
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        static const QITAB qit[] = {QITABENT(FilterClassFactory, IClassFactory), {nullptr}};
        return QISearch(this, qit, riid, ppv);
    }

    IFACEMETHODIMP_(ULONG) AddRef() {
        return InterlockedIncrement(&m_lRef);
    }

    IFACEMETHODIMP_(ULONG) Release() {
        long cRef = InterlockedDecrement(&m_lRef);
        if (cRef == 0) {
            delete this;
        }
        return cRef;
    }

    // IClassFactory
    IFACEMETHODIMP CreateInstance(IUnknown* punkOuter, REFIID riid, void** ppv) {
        log("FilterClassFactory::CreateInstance()\n");

        *ppv = nullptr;
        if (punkOuter) {
            return CLASS_E_NOAGGREGATION;
        }

        ScopedComPtr<IFilter> pFilter;

        CLSID clsid;
        if (SUCCEEDED(CLSIDFromString(kPdfFilterClsid, &clsid)) && IsEqualCLSID(m_clsid, clsid)) {
            pFilter = new PdfFilter(&g_lRefCount);
#ifdef BUILD_TEX_IFILTER
        } else if (SUCCEEDED(CLSIDFromString(kTexFilterClsid, &clsid)) && IsEqualCLSID(m_clsid, clsid)) {
            pFilter = new TeXFilter(&g_lRefCount);
#endif
#ifdef BUILD_EPUB_IFILTER
        } else if (SUCCEEDED(CLSIDFromString(kEpubFilterClsid, &clsid)) && IsEqualCLSID(m_clsid, clsid)) {
            pFilter = new EpubFilter(&g_lRefCount);
#endif
        } else {
            return E_NOINTERFACE;
        }
        if (!pFilter) {
            return E_OUTOFMEMORY;
        }

        return pFilter->QueryInterface(riid, ppv);
    }

    IFACEMETHODIMP LockServer(BOOL bLock) {
        if (bLock) {
            InterlockedIncrement(&g_lRefCount);
        } else {
            InterlockedDecrement(&g_lRefCount);
        }
        return S_OK;
    }

  private:
    long m_lRef;
    CLSID m_clsid;
};

STDAPI_(BOOL) DllMain(__unused HINSTANCE hInstance, DWORD dwReason, __unused LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        CrashIf(hInstance != GetInstance());
    }
    gLogAppName = "PdfFilter";
    gLogToConsole = false;
    log("DllMain\n");
    return TRUE;
}

STDAPI DllCanUnloadNow(VOID) {
    return g_lRefCount == 0 ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    *ppv = nullptr;
    ScopedComPtr<FilterClassFactory> pClassFactory(new FilterClassFactory(rclsid));
    if (!pClassFactory) {
        return E_OUTOFMEMORY;
    }
    return pClassFactory->QueryInterface(riid, ppv);
}

STDAPI DllRegisterServer() {
    log("DllRegisterServer\n");
    TempStr dllPath = path::GetPathOfFileInAppDirTemp((const char*)nullptr);
    if (!dllPath) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    bool ok = InstallSearchFiler(dllPath, false);
    return ok ? S_OK : E_FAIL;
}

STDAPI DllUnregisterServer() {
    log("DllUnregisterServer\n");
    bool ok = UninstallSearchFilter();
    if (!ok) {
        log("DllUnregisterServer failed\n");
    }
    return ok ? S_OK : E_FAIL;
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
