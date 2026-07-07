/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/File.h"
#include "base/Win.h"

#include "wingui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "RegistryPreview.h"
#include "PdfPreview.h"
#include "SumatraLog.h"

long g_lRefCount = 0;

class PreviewClassFactory : public IClassFactory {
  public:
    explicit PreviewClassFactory(REFCLSID rclsid) : m_lRef(1), m_clsid(rclsid) { InterlockedIncrement(&g_lRefCount); }

    ~PreviewClassFactory() { InterlockedDecrement(&g_lRefCount); }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        log("PdfPreview: QueryInterface()\n");
        static const QITAB qit[] = {QITABENT(PreviewClassFactory, IClassFactory), {nullptr}};
        return QISearch(this, qit, riid, ppv);
    }

    IFACEMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_lRef); }

    IFACEMETHODIMP_(ULONG) Release() {
        long cRef = InterlockedDecrement(&m_lRef);
        if (cRef == 0) {
            delete this;
        }
        return cRef;
    }

    bool IsClsid(Str s) {
        CLSID clsid;
        WCHAR* ws = CWStrTemp(s);
        return SUCCEEDED(CLSIDFromString(ws, &clsid)) && IsEqualCLSID(m_clsid, clsid);
    }

    // IClassFactory
    IFACEMETHODIMP CreateInstance(IUnknown* punkOuter, REFIID riid, void** ppv) {
        log("PdfPreview: CreateInstance()\n");

        *ppv = nullptr;
        if (punkOuter) {
            return CLASS_E_NOAGGREGATION;
        }

        PreviewType type;
        if (IsClsid(kPdfPreviewClsid)) {
            type = PreviewType::Pdf;
        } else if (IsClsid(kXpsPreviewClsid)) {
            type = PreviewType::Xps;
        } else if (IsClsid(kDjVuPreviewClsid)) {
            type = PreviewType::DjVu;
        } else if (IsClsid(kEpubPreviewClsid)) {
            type = PreviewType::Epub;
        } else if (IsClsid(kFb2PreviewClsid)) {
            type = PreviewType::Fb2;
        } else if (IsClsid(kMobiPreviewClsid)) {
            type = PreviewType::Mobi;
        } else if (IsClsid(kCbxPreviewClsid)) {
            type = PreviewType::Cbx;
        } else if (IsClsid(kTgaPreviewClsid)) {
            type = PreviewType::Tga;
        } else {
            return E_NOINTERFACE;
        }

        ScopedComPtr<IInitializeWithStream> pObject;
        pObject = new PdfPreview(&g_lRefCount, type);

        if (!pObject) {
            return E_OUTOFMEMORY;
        }

        return pObject->QueryInterface(riid, ppv);
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

static Str GetReason(DWORD dwReason) {
    switch (dwReason) {
        case DLL_PROCESS_ATTACH:
            return "DLL_PROCESS_ATTACH";
        case DLL_THREAD_ATTACH:
            return "DLL_THREAD_ATTACH";
        case DLL_THREAD_DETACH:
            return "DLL_THREAD_DETACH";
        case DLL_PROCESS_DETACH:
            return "DLL_PROCESS_DETACH";
    }
    return "Unknown reason";
}

STDAPI_(BOOL) DllMain(HINSTANCE hInstance, DWORD dwReason, void*) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        ReportIf(hInstance != GetInstance());
    }
    gLogAppName = StrL("PdfPreview");
    logf("PdfPreview: DllMain %s\n", GetReason(dwReason));
    return TRUE;
}

STDAPI DllCanUnloadNow(VOID) {
    return g_lRefCount == 0 ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    // route our log to a file if the user enabled it (no-op otherwise). Done
    // here rather than in DllMain to avoid file/registry I/O under loader lock.
    StartPdfPreviewLoggingIfEnabled();
    *ppv = nullptr;
    ScopedComPtr<PreviewClassFactory> pClassFactory(new PreviewClassFactory(rclsid));
    if (!pClassFactory) {
        return E_OUTOFMEMORY;
    }
    log("PdfPreview: DllGetClassObject\n");
    return pClassFactory->QueryInterface(riid, ppv);
}

STDAPI DllRegisterServer() {
    TempStr dllPath = GetSelfExePathTemp();
    if (!dllPath) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    logf("DllRegisterServer: dllPath=%s\n", dllPath);

    // for compat with SumatraPDF 3.3 and lower
    // in 3.4 we call this code from the installer
    // pre-3.4 we would write to both HKLM (if permissions) and HKCU
    // in 3.4+ this will only install for current user (HKCU)
    bool ok = InstallPreviewDll(dllPath, false);
    if (!ok) {
        log("DllRegisterServer failed!\n");
        return E_FAIL;
    }
    return S_OK;
}

STDAPI DllUnregisterServer() {
    log("DllUnregisterServer\n");

    bool ok = UninstallPreviewDll();
    if (!ok) {
        log("DllUnregisterServer failed!\n");
        return E_FAIL;
    }
    return S_OK;
}

// TODO: maybe remove, is anyone using this functionality?
STDAPI DllInstall(BOOL bInstall, const WCHAR* pszCmdLine) {
    TempStr s = ToUtf8Temp(pszCmdLine);
    DisablePreviewInstallExts(s);
    if (!bInstall) {
        return DllUnregisterServer();
    }
    return DllRegisterServer();
}

#ifdef _WIN64
#pragma comment(linker, "/EXPORT:DllCanUnloadNow=DllCanUnloadNow,PRIVATE")
#pragma comment(linker, "/EXPORT:DllGetClassObject=DllGetClassObject,PRIVATE")
#pragma comment(linker, "/EXPORT:DllRegisterServer=DllRegisterServer,PRIVATE")
#pragma comment(linker, "/EXPORT:DllUnregisterServer=DllUnregisterServer,PRIVATE")
#pragma comment(linker, "/EXPORT:DllInstall=DllInstall,PRIVATE")
#else
#pragma comment(linker, "/EXPORT:DllCanUnloadNow=_DllCanUnloadNow@0,PRIVATE")
#pragma comment(linker, "/EXPORT:DllGetClassObject=_DllGetClassObject@12,PRIVATE")
#pragma comment(linker, "/EXPORT:DllRegisterServer=_DllRegisterServer@0,PRIVATE")
#pragma comment(linker, "/EXPORT:DllUnregisterServer=_DllUnregisterServer@0,PRIVATE")
#pragma comment(linker, "/EXPORT:DllInstall=_DllInstall@8,PRIVATE")
#endif
