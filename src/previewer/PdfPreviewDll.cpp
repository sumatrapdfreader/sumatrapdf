/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/AutoWin.h"
#include "base/File.h"
#include "base/Win.h"

#include "gui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "RegistryPreview.h"
#include "PdfPreview.h"
#include "SumatraLog.h"

static AtomicInt g_lRefCount = 0;

class PreviewClassFactory : public IClassFactory {
  public:
    explicit PreviewClassFactory(PreviewType type) : m_lRef(1), m_type(type) { AtomicIntInc(&g_lRefCount); }

    ~PreviewClassFactory() { AtomicIntDec(&g_lRefCount); }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        log(StrL("PdfPreview: QueryInterface()\n"));
        static const QITAB qit[] = {QITABENT(PreviewClassFactory, IClassFactory), {nullptr}};
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
        log(StrL("PdfPreview: CreateInstance()\n"));

        if (!ppv) {
            return E_POINTER;
        }
        *ppv = nullptr;
        if (punkOuter) {
            return CLASS_E_NOAGGREGATION;
        }

        AutoReleaseComPtr<IInitializeWithStream> pObject;
        pObject = new PdfPreview(&g_lRefCount, m_type);

        if (!pObject) {
            return E_OUTOFMEMORY;
        }

        return pObject->QueryInterface(riid, ppv);
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
    PreviewType m_type;
};

void UnregisterPreviewWndClass();

STDAPI_(BOOL) DllMain(HINSTANCE hInstance, DWORD dwReason, void* lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        ReportIf(hInstance != GetInstance());
        gLogAppName = StrL("PdfPreview");
    } else if (dwReason == DLL_PROCESS_DETACH) {
        // lpReserved is non-null when the process is exiting; skip teardown
        // then so we don't take locks under the loader as other DLLs die.
        if (!lpReserved) {
            UnregisterPreviewWndClass();
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
    // route our log to a file if the user enabled it (no-op otherwise). Done
    // here rather than in DllMain to avoid file/registry I/O under loader lock.
    StartPdfPreviewLoggingIfEnabled();
    *ppv = nullptr;
    PreviewType type;
    if (!FindPreviewType(rclsid, type)) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    AutoReleaseComPtr<PreviewClassFactory> pClassFactory(new PreviewClassFactory(type));
    if (!pClassFactory) {
        return E_OUTOFMEMORY;
    }
    log(StrL("PdfPreview: DllGetClassObject\n"));
    return pClassFactory->QueryInterface(riid, ppv);
}

STDAPI DllRegisterServer() {
    TempStr dllPath = GetSelfExePathTemp();
    if (len(dllPath) == 0) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    logf("DllRegisterServer: dllPath=%s\n", dllPath);

    // for compat with SumatraPDF 3.3 and lower
    // in 3.4 we call this code from the installer
    // pre-3.4 we would write to both HKLM (if permissions) and HKCU
    // in 3.4+ this will only install for current user (HKCU)
    bool ok = InstallPreviewDll(dllPath, false, {kAllPreviewExts, kAllPreviewExts});
    if (!ok) {
        log(StrL("DllRegisterServer failed!\n"));
        return E_FAIL;
    }
    return S_OK;
}

STDAPI DllUnregisterServer() {
    log(StrL("DllUnregisterServer\n"));

    bool ok = UninstallPreviewDll();
    if (!ok) {
        log(StrL("DllUnregisterServer failed!\n"));
        return E_FAIL;
    }
    return S_OK;
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
