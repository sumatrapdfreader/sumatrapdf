/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "PdfPreview.h"
#include "PdfPreviewBase.h"

long g_lRefCount = 0;

class CClassFactory : public IClassFactory {
  public:
    CClassFactory(REFCLSID rclsid) : m_lRef(1), m_clsid(rclsid) { InterlockedIncrement(&g_lRefCount); }

    ~CClassFactory() { InterlockedDecrement(&g_lRefCount); }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        static const QITAB qit[] = {QITABENT(CClassFactory, IClassFactory), {0}};
        return QISearch(this, qit, riid, ppv);
    }

    IFACEMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_lRef); }

    IFACEMETHODIMP_(ULONG) Release() {
        long cRef = InterlockedDecrement(&m_lRef);
        if (cRef == 0)
            delete this;
        return cRef;
    }

    // IClassFactory
    IFACEMETHODIMP CreateInstance(IUnknown* punkOuter, REFIID riid, void** ppv) {
        *ppv = nullptr;
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
#ifdef BUILD_DJVU_PREVIEW
        else if (SUCCEEDED(CLSIDFromString(SZ_DJVU_PREVIEW_CLSID, &clsid)) && IsEqualCLSID(m_clsid, clsid))
            pObject = new CDjVuPreview(&g_lRefCount);
#endif
#ifdef BUILD_EPUB_PREVIEW
        else if (SUCCEEDED(CLSIDFromString(SZ_EPUB_PREVIEW_CLSID, &clsid)) && IsEqualCLSID(m_clsid, clsid))
            pObject = new CEpubPreview(&g_lRefCount);
#endif
#ifdef BUILD_FB2_PREVIEW
        else if (SUCCEEDED(CLSIDFromString(SZ_FB2_PREVIEW_CLSID, &clsid)) && IsEqualCLSID(m_clsid, clsid))
            pObject = new CFb2Preview(&g_lRefCount);
#endif
#ifdef BUILD_MOBI_PREVIEW
        else if (SUCCEEDED(CLSIDFromString(SZ_MOBI_PREVIEW_CLSID, &clsid)) && IsEqualCLSID(m_clsid, clsid))
            pObject = new CMobiPreview(&g_lRefCount);
#endif
#if defined(BUILD_CBZ_PREVIEW) || defined(BUILD_CBR_PREVIEW) || defined(BUILD_CB7_PREVIEW) || defined(BUILD_CBT_PREVIEW)
        else if (SUCCEEDED(CLSIDFromString(SZ_CBX_PREVIEW_CLSID, &clsid)) && IsEqualCLSID(m_clsid, clsid))
            pObject = new CCbxPreview(&g_lRefCount);
#endif
#ifdef BUILD_TGA_PREVIEW
        else if (SUCCEEDED(CLSIDFromString(SZ_TGA_PREVIEW_CLSID, &clsid)) && IsEqualCLSID(m_clsid, clsid))
            pObject = new CTgaPreview(&g_lRefCount);
#endif
        else
            return E_NOINTERFACE;

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

STDAPI_(BOOL) DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved) {
    UNUSED(lpReserved);
    if (dwReason == DLL_PROCESS_ATTACH) {
        CrashIf(hInstance != GetInstance());
    }

    return TRUE;
}

STDAPI DllCanUnloadNow(VOID) {
    return g_lRefCount == 0 ? S_OK : S_FALSE;
}

// disable warning C6387 which is wrongly issued due to a compiler bug; cf.
// http://connect.microsoft.com/VisualStudio/feedback/details/498862/c6387-warning-on-stock-dllgetclassobject-code-with-static-analyser
#pragma warning(push)
#pragma warning(disable : 6387) /* '*ppv' might be '0': this does not adhere to the specification for the function \
                                   'DllGetClassObject' */

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    *ppv = nullptr;
    ScopedComPtr<CClassFactory> pClassFactory(new CClassFactory(rclsid));
    if (!pClassFactory) {
        return E_OUTOFMEMORY;
    }
    return pClassFactory->QueryInterface(riid, ppv);
}

#pragma warning(pop)

#define CLSID_I_THUMBNAIL_PROVIDER L"{e357fccd-a995-4576-b01f-234630154e96}"
#define CLSID_I_EXTRACT_IMAGE L"{bb2e617c-0920-11d1-9a0b-00c04fc2d6c1}"
#define CLSID_I_PREVIEW_HANDLER L"{8895b1c6-b41f-4c1c-a562-0d564250836f}"
#define APPID_PREVHOST_EXE L"{6d2b5079-2f0b-48dd-ab7f-97cec514d30b}"
#define APPID_PREVHOST_EXE_WOW64 L"{534a1e02-d58f-44f0-b58b-36cbed287c7c}"

#define REG_KEY_PREVIEW_HANDLERS L"Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers"

static struct {
    const WCHAR* clsid;
    const WCHAR *ext, *ext2;
    bool skip;
} gPreviewers[] = {
    {SZ_PDF_PREVIEW_CLSID, L".pdf"},
#ifdef BUILD_XPS_PREVIEW
    {SZ_XPS_PREVIEW_CLSID, L".xps", L".oxps"},
#endif
#ifdef BUILD_DJVU_PREVIEW
    {SZ_DJVU_PREVIEW_CLSID, L".djvu"},
#endif
#ifdef BUILD_EPUB_PREVIEW
    {SZ_EPUB_PREVIEW_CLSID, L".epub"},
#endif
#ifdef BUILD_FB2_PREVIEW
    {SZ_FB2_PREVIEW_CLSID, L".fb2", L".fb2z"},
#endif
#ifdef BUILD_MOBI_PREVIEW
    {SZ_MOBI_PREVIEW_CLSID, L".mobi"},
#endif
#ifdef BUILD_CBZ_PREVIEW
    {SZ_CBX_PREVIEW_CLSID, L".cbz"},
#endif
#ifdef BUILD_CBR_PREVIEW
    {SZ_CBX_PREVIEW_CLSID, L".cbr"},
#endif
#ifdef BUILD_CB7_PREVIEW
    {SZ_CBX_PREVIEW_CLSID, L".cb7"},
#endif
#ifdef BUILD_CBT_PREVIEW
    {SZ_CBX_PREVIEW_CLSID, L".cbt"},
#endif
#ifdef BUILD_TGA_PREVIEW
    {SZ_TGA_PREVIEW_CLSID, L".tga"},
#endif
};

STDAPI DllRegisterServer() {
    AutoFreeWstr dllPath = path::GetPathOfFileInAppDir();
    if (!dllPath) {
        return HRESULT_FROM_WIN32(GetLastError());        
    }

#define WriteOrFail_(key, value, data)                     \
    WriteRegStr(HKEY_LOCAL_MACHINE, key, value, data);     \
    if (!WriteRegStr(HKEY_CURRENT_USER, key, value, data)) \
    return E_FAIL

    for (int i = 0; i < dimof(gPreviewers); i++) {
        if (gPreviewers[i].skip) {
            continue;
        }
        const WCHAR* clsid = gPreviewers[i].clsid;
        const WCHAR* ext = gPreviewers[i].ext;
        const WCHAR* ext2 = gPreviewers[i].ext2;

        AutoFreeWstr displayName = str::Format(L"SumatraPDF Preview (*%s)", ext);
        // register class
        AutoFreeWstr key = str::Format(L"Software\\Classes\\CLSID\\%s", clsid);
        WriteOrFail_(key, nullptr, displayName);
        WriteOrFail_(key, L"AppId", IsRunningInWow64() ? APPID_PREVHOST_EXE_WOW64 : APPID_PREVHOST_EXE);
        WriteOrFail_(key, L"DisplayName", displayName);
        key.Set(str::Format(L"Software\\Classes\\CLSID\\%s\\InProcServer32", clsid));
        WriteOrFail_(key, nullptr, dllPath);
        WriteOrFail_(key, L"ThreadingModel", L"Apartment");
        // IThumbnailProvider
        key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_THUMBNAIL_PROVIDER, ext));
        WriteOrFail_(key, nullptr, clsid);
        if (ext2) {
            key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_THUMBNAIL_PROVIDER, ext2));
            WriteOrFail_(key, nullptr, clsid);
        }
        // IExtractImage (for Windows XP)
        if (!IsVistaOrGreater()) {
            // don't register for IExtractImage on systems which accept IThumbnailProvider
            // (because it doesn't offer anything beyond what IThumbnailProvider does)
            key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_EXTRACT_IMAGE, ext));
            WriteOrFail_(key, nullptr, clsid);
            if (ext2) {
                key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_EXTRACT_IMAGE, ext2));
                WriteOrFail_(key, nullptr, clsid);
            }
        }
        // IPreviewHandler
        key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_PREVIEW_HANDLER, ext));
        WriteOrFail_(key, nullptr, clsid);
        if (ext2) {
            key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_PREVIEW_HANDLER, ext2));
            WriteOrFail_(key, nullptr, clsid);
        }
        WriteOrFail_(REG_KEY_PREVIEW_HANDLERS, clsid, displayName);
    }
#undef WriteOrFail_

    return S_OK;
}

STDAPI DllUnregisterServer() {
    HRESULT hr = S_OK;

#define DeleteOrFail_(key)                     \
    DeleteRegKey(HKEY_LOCAL_MACHINE, key);     \
    if (!DeleteRegKey(HKEY_CURRENT_USER, key)) \
    hr = E_FAIL

    for (int i = 0; i < dimof(gPreviewers); i++) {
        if (gPreviewers[i].skip) {
            continue;
        }
        const WCHAR* clsid = gPreviewers[i].clsid;
        const WCHAR* ext = gPreviewers[i].ext;
        const WCHAR* ext2 = gPreviewers[i].ext2;

        // unregister preview handler
        SHDeleteValue(HKEY_LOCAL_MACHINE, REG_KEY_PREVIEW_HANDLERS, clsid);
        SHDeleteValue(HKEY_CURRENT_USER, REG_KEY_PREVIEW_HANDLERS, clsid);
        // remove class data
        AutoFreeWstr key(str::Format(L"Software\\Classes\\CLSID\\%s", clsid));
        DeleteOrFail_(key);
        // IThumbnailProvider
        key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_THUMBNAIL_PROVIDER, ext));
        DeleteOrFail_(key);
        if (ext2) {
            key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_THUMBNAIL_PROVIDER, ext2));
            DeleteOrFail_(key);
        }
        // IExtractImage (for Windows XP)
        key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_EXTRACT_IMAGE, ext));
        DeleteOrFail_(key);
        if (ext2) {
            key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_EXTRACT_IMAGE, ext2));
            DeleteOrFail_(key);
        }
        // IPreviewHandler
        key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_PREVIEW_HANDLER, ext));
        DeleteOrFail_(key);
        if (ext2) {
            key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_PREVIEW_HANDLER, ext2));
            DeleteOrFail_(key);
        }
    }
#undef DeleteOrFail_

    return hr;
}

STDAPI DllInstall(BOOL bInstall, LPCWSTR pszCmdLine) {
    // allows installing only a subset of available preview handlers
    if (str::StartsWithI(pszCmdLine, L"exts:")) {
        AutoFreeWstr extsList(str::Dup(pszCmdLine + 5));
        str::ToLowerInPlace(extsList);
        str::TransChars(extsList, L";. :", L",,,\0");
        WStrVec exts;
        exts.Split(extsList, L",", true);
        for (int i = 0; i < dimof(gPreviewers); i++) {
            gPreviewers[i].skip = !exts.Contains(gPreviewers[i].ext + 1);
        }
    }

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
