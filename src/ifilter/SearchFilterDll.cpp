/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/AutoWin.h"
#include "base/File.h"
#include "base/Win.h"

#include "FilterBase.h" // IWYU pragma: keep
#include "RegistrySearchFilter.h"
#include "PdfFilter.h"
#include "TeXFilter.h"
#include "EpubFilter.h"
#include "SumatraLog.h"

static AtomicInt g_lRefCount = 0;

enum class FilterKind {
    None,
    Pdf,
    Tex,
    Epub
};

static bool IsClsid(REFCLSID clsid, Str s) {
    CLSID c;
    return SUCCEEDED(CLSIDFromString(s, &c)) && IsEqualCLSID(clsid, c);
}

static FilterKind FilterKindFromClsid(REFCLSID clsid) {
    if (IsClsid(clsid, StrL(kPdfFilterClsid))) {
        return FilterKind::Pdf;
    }
    if (IsClsid(clsid, StrL(kTexFilterClsid))) {
        return FilterKind::Tex;
    }
    if (IsClsid(clsid, StrL(kEpubFilterClsid))) {
        return FilterKind::Epub;
    }
    return FilterKind::None;
}

class FilterClassFactory : public IClassFactory {
  public:
    explicit FilterClassFactory(REFCLSID rclsid) : m_lRef(1), m_clsid(rclsid) { AtomicIntInc(&g_lRefCount); }

    ~FilterClassFactory() { AtomicIntDec(&g_lRefCount); }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        static const QITAB qit[] = {QITABENT(FilterClassFactory, IClassFactory), {nullptr}};
        return QISearch(this, qit, riid, ppv);
    }

    IFACEMETHODIMP_(ULONG) AddRef() { return AtomicIntInc(&m_lRef); }

    IFACEMETHODIMP_(ULONG) Release() {
        int cRef = AtomicIntDec(&m_lRef);
        if (cRef == 0) {
            delete this;
        }
        return cRef;
    }

    // IClassFactory
    IFACEMETHODIMP CreateInstance(IUnknown* punkOuter, REFIID riid, void** ppv) {
        log(StrL("FilterClassFactory::CreateInstance()\n"));

        if (!ppv) {
            return E_POINTER;
        }
        *ppv = nullptr;
        if (punkOuter) {
            return CLASS_E_NOAGGREGATION;
        }

        AutoReleaseComPtr<IFilter> pFilter;

        switch (FilterKindFromClsid(m_clsid)) {
            case FilterKind::Pdf:
                pFilter = new PdfFilter(&g_lRefCount);
                break;
            case FilterKind::Tex:
                pFilter = new TeXFilter(&g_lRefCount);
                break;
            case FilterKind::Epub:
                pFilter = new EpubFilter(&g_lRefCount);
                break;
            default:
                return E_NOINTERFACE;
        }
        if (!pFilter) {
            return E_OUTOFMEMORY;
        }

        return pFilter->QueryInterface(riid, ppv);
    }

    IFACEMETHODIMP LockServer(BOOL bLock) {
        if (bLock) {
            AtomicIntInc(&g_lRefCount);
        } else {
            AtomicIntDec(&g_lRefCount);
        }
        return S_OK;
    }

  private:
    AtomicInt m_lRef;
    CLSID m_clsid;
};

STDAPI_(BOOL) DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        ReportIf(hInstance != GetInstance());
        gLogAppName = StrL("PdfFilter");
        gLogToConsole = false;
    } else if (dwReason == DLL_PROCESS_DETACH) {
        // lpReserved is non-null when the process is exiting; skip teardown
        // then so we don't take locks under the loader as other DLLs die.
        // FreeLibrary unload (ifilttst, SearchFilterHost) is lpReserved == 0
        // and that's the leak the CRT dump sees (#4859).
        if (!lpReserved) {
            DestroyLogging();
            DestroyTempArena();
            DestroyPermArena();
        }
    }
    return TRUE;
}

STDAPI DllCanUnloadNow(VOID) {
    return AtomicIntGet(&g_lRefCount) == 0 ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    if (!ppv) {
        return E_POINTER;
    }
    *ppv = nullptr;
    if (FilterKindFromClsid(rclsid) == FilterKind::None) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    AutoReleaseComPtr<FilterClassFactory> pClassFactory(new FilterClassFactory(rclsid));
    if (!pClassFactory) {
        return E_OUTOFMEMORY;
    }
    return pClassFactory->QueryInterface(riid, ppv);
}

STDAPI DllRegisterServer() {
    log(StrL("DllRegisterServer\n"));
    TempStr dllPath = GetSelfExePathTemp();
    if (len(dllPath) == 0) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    bool ok = InstallSearchFilter(dllPath, false);
    return ok ? S_OK : E_FAIL;
}

STDAPI DllUnregisterServer() {
    log(StrL("DllUnregisterServer\n"));
    bool ok = UninstallSearchFilter();
    if (!ok) {
        log(StrL("DllUnregisterServer failed\n"));
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
