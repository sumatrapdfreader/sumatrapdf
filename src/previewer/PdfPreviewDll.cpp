/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "PdfPreview.h"

#include "WinUtil.h"

// cf. http://blogs.msdn.com/b/oldnewthing/archive/2004/10/25/247180.aspx
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define CURRENT_HMODULE ((HMODULE)&__ImageBase)

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
        if (SUCCEEDED(CLSIDFromString(SZ_PDF_PREVIEW_CLSID, &clsid)) && IsEqualCLSID(m_clsid, clsid))
            pObject = new CPdfPreview(&g_lRefCount);
#ifdef BUILD_XPS_PREVIEW
        else if (SUCCEEDED(CLSIDFromString(SZ_XPS_PREVIEW_CLSID, &clsid)) && IsEqualCLSID(m_clsid, clsid))
            pObject = new CXpsPreview(&g_lRefCount);
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
#if defined(BUILD_CBZ_PREVIEW) || defined(BUILD_CBR_PREVIEW)
        else if (SUCCEEDED(CLSIDFromString(SZ_CBX_PREVIEW_CLSID, &clsid)) && IsEqualCLSID(m_clsid, clsid))
            pObject = new CCbxPreview(&g_lRefCount);
#endif
#ifdef BUILD_TGA_PREVIEW
        else if (SUCCEEDED(CLSIDFromString(SZ_TGA_PREVIEW_CLSID, &clsid)) && IsEqualCLSID(m_clsid, clsid))
            pObject = new CTgaPreview(&g_lRefCount);
#endif
        else
            return E_NOINTERFACE;
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
    if (dwReason == DLL_PROCESS_ATTACH) {
        g_hInstance = hInstance;
        CrashIf(g_hInstance != CURRENT_HMODULE);
    }

    return TRUE;
}

STDAPI DllCanUnloadNow(VOID)
{
    return g_lRefCount == 0 ? S_OK : S_FALSE;
}

// disable warning C6387 which is wrongly issued due to a compiler bug; cf.
// http://connect.microsoft.com/VisualStudio/feedback/details/498862/c6387-warning-on-stock-dllgetclassobject-code-with-static-analyser
#pragma warning(push)
#pragma warning(disable: 6387) /* '*ppv' might be '0': this does not adhere to the specification for the function 'DllGetClassObject' */

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
    *ppv = NULL;
    ScopedComPtr<CClassFactory> pClassFactory(new CClassFactory(rclsid));
    if (!pClassFactory)
        return E_OUTOFMEMORY;
    return pClassFactory->QueryInterface(riid, ppv);
}

#pragma warning(pop)

#define CLSID_I_THUMBNAIL_PROVIDER  L"{e357fccd-a995-4576-b01f-234630154e96}"
#define CLSID_I_EXTRACT_IMAGE       L"{bb2e617c-0920-11d1-9a0b-00c04fc2d6c1}"
#define CLSID_I_PREVIEW_HANDLER     L"{8895b1c6-b41f-4c1c-a562-0d564250836f}"
#define APPID_PREVHOST_EXE          L"{6d2b5079-2f0b-48dd-ab7f-97cec514d30b}"
#define APPID_PREVHOST_EXE_WOW64    L"{534a1e02-d58f-44f0-b58b-36cbed287c7c}"

#define REG_KEY_PREVIEW_HANDLERS    L"Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers"

static struct {
    const WCHAR *clsid;
    const WCHAR *ext;
    bool skip;
} gPreviewers[] = {
    { SZ_PDF_PREVIEW_CLSID, L".pdf" },
#ifdef BUILD_XPS_PREVIEW
    { SZ_XPS_PREVIEW_CLSID, L".xps" },
    { SZ_XPS_PREVIEW_CLSID, L".oxps" },
#endif
#ifdef BUILD_EPUB_PREVIEW
    { SZ_EPUB_PREVIEW_CLSID,L".epub" },
#endif
#ifdef BUILD_FB2_PREVIEW
    { SZ_FB2_PREVIEW_CLSID, L".fb2" },
    { SZ_FB2_PREVIEW_CLSID, L".fb2z" },
#endif
#ifdef BUILD_MOBI_PREVIEW
    { SZ_MOBI_PREVIEW_CLSID,L".mobi" },
#endif
#ifdef BUILD_CBZ_PREVIEW
    { SZ_CBX_PREVIEW_CLSID, L".cbz" },
#endif
#ifdef BUILD_CBZ_PREVIEW
    { SZ_CBX_PREVIEW_CLSID, L".cbr" },
#endif
#ifdef BUILD_TGA_PREVIEW
    { SZ_TGA_PREVIEW_CLSID, L".tga" },
#endif
};

STDAPI DllRegisterServer()
{
    WCHAR dllPath[MAX_PATH];
    if (!GetModuleFileName(g_hInstance, dllPath, dimof(dllPath)))
        return HRESULT_FROM_WIN32(GetLastError());
    dllPath[dimof(dllPath) - 1] = '\0';

#define WriteOrFail_(key, value, data) WriteRegStr(HKEY_LOCAL_MACHINE, key, value, data); \
    if (!WriteRegStr(HKEY_CURRENT_USER, key, value, data)) return E_FAIL

    for (int i = 0; i < dimof(gPreviewers); i++) {
        if (gPreviewers[i].skip)
            continue;
        // register class
        ScopedMem<WCHAR> key(str::Format(L"Software\\Classes\\CLSID\\%s", gPreviewers[i].clsid));
        WriteOrFail_(key, NULL, L"SumatraPDF Preview Handler");
        WriteOrFail_(key, L"AppId", IsRunningInWow64() ? APPID_PREVHOST_EXE_WOW64 : APPID_PREVHOST_EXE);
        key.Set(str::Format(L"Software\\Classes\\CLSID\\%s\\InProcServer32", gPreviewers[i].clsid));
        WriteOrFail_(key, NULL, dllPath);
        WriteOrFail_(key, L"ThreadingModel", L"Apartment");
        // IThumbnailProvider
        key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_THUMBNAIL_PROVIDER, gPreviewers[i].ext));
        WriteOrFail_(key, NULL, gPreviewers[i].clsid);
        // IExtractImage (for Windows XP)
        if (!IsVistaOrGreater()) {
            // don't register for IExtractImage on systems which accept IThumbnailProvider
            // (because it doesn't offer anything beyond what IThumbnailProvider does)
            key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_EXTRACT_IMAGE, gPreviewers[i].ext));
            WriteOrFail_(key, NULL, gPreviewers[i].clsid);
        }
        // IPreviewHandler
        key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_PREVIEW_HANDLER, gPreviewers[i].ext));
        WriteOrFail_(key, NULL, gPreviewers[i].clsid);
        WriteOrFail_(REG_KEY_PREVIEW_HANDLERS, gPreviewers[i].clsid, L"SumatraPDF Preview Handler");

    }
#undef WriteOrFail_

    return S_OK;
}

STDAPI DllUnregisterServer()
{
    HRESULT hr = S_OK;

#define DeleteOrFail_(key) DeleteRegKey(HKEY_LOCAL_MACHINE, key); \
    if (!DeleteRegKey(HKEY_CURRENT_USER, key)) hr = E_FAIL

    for (int i = 0; i < dimof(gPreviewers); i++) {
        if (gPreviewers[i].skip)
            continue;
        // unregister preview handler
        SHDeleteValue(HKEY_LOCAL_MACHINE, REG_KEY_PREVIEW_HANDLERS, gPreviewers[i].clsid);
        SHDeleteValue(HKEY_CURRENT_USER, REG_KEY_PREVIEW_HANDLERS, gPreviewers[i].clsid);
        // remove class data
        ScopedMem<WCHAR> key(str::Format(L"Software\\Classes\\CLSID\\%s", gPreviewers[i].clsid));
        DeleteOrFail_(key);
        // IThumbnailProvider
        key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_THUMBNAIL_PROVIDER, gPreviewers[i].ext));
        DeleteOrFail_(key);
        // IExtractImage (for Windows XP)
        key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_EXTRACT_IMAGE, gPreviewers[i].ext));
        DeleteOrFail_(key);
        // IPreviewHandler
        key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_PREVIEW_HANDLER, gPreviewers[i].ext));
        DeleteOrFail_(key);
    }
#undef DeleteOrFail_

    return hr;
}

STDAPI DllInstall(BOOL bInstall, LPCWSTR pszCmdLine)
{
    // allows installing only a subset of available preview handlers
    if (str::StartsWithI(pszCmdLine, L"exts:")) {
        ScopedMem<WCHAR> extsList(str::Dup(pszCmdLine + 5));
        str::ToLower(extsList);
        str::TransChars(extsList, L";. :", L",,,\0");
        WStrVec exts;
        exts.Split(extsList, L",", true);
        for (int i = 0; i < dimof(gPreviewers); i++) {
            gPreviewers[i].skip = !exts.Contains(gPreviewers[i].ext + 1);
        }
    }

    if (!bInstall)
        return DllUnregisterServer();
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
