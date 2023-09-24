/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "RegistryPreview.h"
#include "PdfPreviewBase.h"

#include "utils/Log.h"

long g_lRefCount = 0;

#ifdef BUILD_XPS_PREVIEW
static bool gBuildXpsPreview = true;
#else
static bool gBuildXpsPreview = false;
#endif

static bool gBuildDjVuPreview = true;

#ifdef BUILD_EPUB_PREVIEW
static bool gBuildEpubPreview = true;
#else
static bool gBuildEpubPreview = false;
#endif

#ifdef BUILD_FB2_PREVIEW
static bool gBuildFb2Preview = true;
#else
static bool gBuildFb2Preview = false;
#endif

#ifdef BUILD_MOBI_PREVIEW
static bool gBuildMobiPreview = true;
#else
static bool gBuildMobiPreview = false;
#endif

static bool gBuildCbxPreview = true;
static bool gBuildTgaPreview = true;

class PreviewClassFactory : public IClassFactory {
  public:
    explicit PreviewClassFactory(REFCLSID rclsid) : m_lRef(1), m_clsid(rclsid) {
        InterlockedIncrement(&g_lRefCount);
    }

    ~PreviewClassFactory() {
        InterlockedDecrement(&g_lRefCount);
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        log("PdfPreview: QueryInterface()\n");
        static const QITAB qit[] = {QITABENT(PreviewClassFactory, IClassFactory), {nullptr}};
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
        log("PdfPreview: CreateInstance()\n");

        *ppv = nullptr;
        if (punkOuter) {
            return CLASS_E_NOAGGREGATION;
        }

        ScopedComPtr<IInitializeWithStream> pObject;

        CLSID clsid;
        if (SUCCEEDED(CLSIDFromString(kPdfPreviewClsid, &clsid)) && IsEqualCLSID(m_clsid, clsid)) {
            pObject = new PdfPreview(&g_lRefCount);
        }
#if 0
        else if (gBuildXpsPreview && SUCCEEDED(CLSIDFromString(kXpsPreviewClsid, &clsid)) &&
                   IsEqualCLSID(m_clsid, clsid)) {
            pObject = new XpsPreview(&g_lRefCount);
        }
#endif
        else if (gBuildDjVuPreview && SUCCEEDED(CLSIDFromString(kDjVuPreviewClsid, &clsid)) &&
                 IsEqualCLSID(m_clsid, clsid)) {
            pObject = new DjVuPreview(&g_lRefCount);
        } else if (gBuildEpubPreview && SUCCEEDED(CLSIDFromString(kEpubPreviewClsid, &clsid)) &&
                   IsEqualCLSID(m_clsid, clsid)) {
            pObject = new EpubPreview(&g_lRefCount);
        } else if (gBuildFb2Preview && SUCCEEDED(CLSIDFromString(kFb2PreviewClsid, &clsid)) &&
                   IsEqualCLSID(m_clsid, clsid)) {
            pObject = new Fb2Preview(&g_lRefCount);
        } else if (gBuildMobiPreview && SUCCEEDED(CLSIDFromString(kMobiPreviewClsid, &clsid)) &&
                   IsEqualCLSID(m_clsid, clsid)) {
            pObject = new MobiPreview(&g_lRefCount);
        } else if (gBuildCbxPreview && SUCCEEDED(CLSIDFromString(kCbxPreviewClsid, &clsid)) &&
                   IsEqualCLSID(m_clsid, clsid)) {
            pObject = new CbxPreview(&g_lRefCount);
        } else if (gBuildTgaPreview && SUCCEEDED(CLSIDFromString(kTgaPreviewClsid, &clsid)) &&
                   IsEqualCLSID(m_clsid, clsid)) {
            pObject = new TgaPreview(&g_lRefCount);
        } else {
            return E_NOINTERFACE;
        }

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

static const char* GetReason(DWORD dwReason) {
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
        CrashIf(hInstance != GetInstance());
    }
    gLogAppName = "PdfPreview";
    logf("PdfPreview: DllMain %s\n", GetReason(dwReason));
    return TRUE;
}

STDAPI DllCanUnloadNow(VOID) {
    return g_lRefCount == 0 ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    *ppv = nullptr;
    ScopedComPtr<PreviewClassFactory> pClassFactory(new PreviewClassFactory(rclsid));
    if (!pClassFactory) {
        return E_OUTOFMEMORY;
    }
    log("PdfPreview: DllGetClassObject\n");
    return pClassFactory->QueryInterface(riid, ppv);
}

STDAPI DllRegisterServer() {
    AutoFreeStr dllPath = path::GetPathOfFileInAppDir((const char*)nullptr);
    if (!dllPath) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    logf("DllRegisterServer: dllPath=%s\n", dllPath.Get());

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
    char* s = ToUtf8Temp(pszCmdLine);
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
