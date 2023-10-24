/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/FileUtil.h"

#include "RegistryPreview.h"

#include "utils/Log.h"

#define kThumbnailProviderClsid "{e357fccd-a995-4576-b01f-234630154e96}"
#define kExtractImageClsid "{bb2e617c-0920-11d1-9a0b-00c04fc2d6c1}"
#define kPreviewHandlerClsid "{8895b1c6-b41f-4c1c-a562-0d564250836f}"
#define kAppIdPrevHostExe "{6d2b5079-2f0b-48dd-ab7f-97cec514d30b}"
#define kAppIdPrevHostExeWow64 "{534a1e02-d58f-44f0-b58b-36cbed287c7c}"

#define kRegKeyPreviewHandlers "Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers"

// clang-format off
static struct {
    const char* clsid = nullptr;
    const char *ext = nullptr, *ext2 = nullptr;
    bool skip = false;
} gPreviewers[] = {
    {kPdfPreviewClsid, ".pdf"},
    {kCbxPreviewClsid, ".cbz"},
    {kCbxPreviewClsid, ".cbr"},
    {kCbxPreviewClsid, ".cb7"},
    {kCbxPreviewClsid, ".cbt"},
    {kTgaPreviewClsid, ".tga"},
    {kDjVuPreviewClsid, ".djvu"},
#ifdef BUILD_XPS_PREVIEW
    {kXpsPreviewClsid, ".xps", ".oxps"},
#endif
#ifdef BUILD_EPUB_PREVIEW
    {kEpubPreviewClsid, ".epub"},
#endif
#ifdef BUILD_FB2_PREVIEW
    {kFb2PreviewClsid, ".fb2", ".fb2z"},
#endif
#ifdef BUILD_MOBI_PREVIEW
    {kMobiPreviewClsid, ".mobi"},
#endif
};
// clang-format on

bool InstallPreviewDll(const char* dllPath, bool allUsers) {
    HKEY hkey = allUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    bool ok;

    for (auto& prev : gPreviewers) {
        if (prev.skip) {
            continue;
        }
        const char* clsid = prev.clsid;
        const char* ext = prev.ext;
        const char* ext2 = prev.ext2;
        ok = true;

        TempStr displayName = str::FormatTemp("SumatraPDF Preview (*%s)", ext);
        // register class
        TempStr key = str::FormatTemp("Software\\Classes\\CLSID\\%s", clsid);
        ok &= LoggedWriteRegStr(hkey, key, nullptr, displayName);
        ok &= LoggedWriteRegStr(hkey, key, "AppId", IsRunningInWow64() ? kAppIdPrevHostExeWow64 : kAppIdPrevHostExe);
        ok &= LoggedWriteRegStr(hkey, key, "DisplayName", displayName);
        key = str::FormatTemp("Software\\Classes\\CLSID\\%s\\InProcServer32", clsid);
        ok &= LoggedWriteRegStr(hkey, key, nullptr, dllPath);
        ok &= LoggedWriteRegStr(hkey, key, "ThreadingModel", "Apartment");
        // IThumbnailProvider
        key = str::FormatTemp("Software\\Classes\\%s\\shellex\\" kThumbnailProviderClsid, ext);
        ok &= LoggedWriteRegStr(hkey, key, nullptr, clsid);
        if (ext2) {
            key = str::FormatTemp("Software\\Classes\\%s\\shellex\\" kThumbnailProviderClsid, ext2);
            ok &= LoggedWriteRegStr(hkey, key, nullptr, clsid);
        }
        // IPreviewHandler
        key = str::FormatTemp("Software\\Classes\\%s\\shellex\\" kPreviewHandlerClsid, ext);
        ok &= LoggedWriteRegStr(hkey, key, nullptr, clsid);
        if (ext2) {
            key = str::FormatTemp("Software\\Classes\\%s\\shellex\\" kPreviewHandlerClsid, ext2);
            ok &= LoggedWriteRegStr(hkey, key, nullptr, clsid);
        }
        ok &= LoggedWriteRegStr(hkey, kRegKeyPreviewHandlers, clsid, displayName);
        if (!ok) {
            return false;
        }
    }

    return true;
}

static void DeleteOrFail(const char* key, HRESULT* hr) {
    LoggedDeleteRegKey(HKEY_LOCAL_MACHINE, key);
    if (!LoggedDeleteRegKey(HKEY_CURRENT_USER, key)) {
        *hr = E_FAIL;
    }
}

// we delete from HKLM and HKCU for compat with pre-3.4
bool UninstallPreviewDll() {
    HRESULT hr = S_OK;

    TempStr key;
    for (auto& prev : gPreviewers) {
        if (prev.skip) {
            logf("UninstallPreviewDll: skipping '%s'\n", prev.ext);
            continue;
        }
        const char* clsid = prev.clsid;
        const char* ext = prev.ext;
        const char* ext2 = prev.ext2;

        // unregister preview handler
        DeleteRegValue(HKEY_LOCAL_MACHINE, kRegKeyPreviewHandlers, clsid);
        DeleteRegValue(HKEY_CURRENT_USER, kRegKeyPreviewHandlers, clsid);
        // remove class data
        key = str::FormatTemp("Software\\Classes\\CLSID\\%s", clsid);
        DeleteOrFail(key, &hr);
        // IThumbnailProvider
        key = str::FormatTemp("Software\\Classes\\%s\\shellex\\" kThumbnailProviderClsid, ext);
        DeleteOrFail(key, &hr);
        if (ext2) {
            key = str::FormatTemp("Software\\Classes\\%s\\shellex\\" kThumbnailProviderClsid, ext2);
            DeleteOrFail(key, &hr);
        }
        // IExtractImage (for Windows XP)
        key = str::FormatTemp("Software\\Classes\\%s\\shellex\\" kExtractImageClsid, ext);
        DeleteOrFail(key, &hr);
        if (ext2) {
            key = str::FormatTemp("Software\\Classes\\%s\\shellex\\" kExtractImageClsid, ext2);
            DeleteOrFail(key, &hr);
        }
        // IPreviewHandler
        key = str::FormatTemp("Software\\Classes\\%s\\shellex\\" kPreviewHandlerClsid, ext);
        DeleteOrFail(key, &hr);
        if (ext2) {
            key = str::FormatTemp("Software\\Classes\\%s\\shellex\\" kPreviewHandlerClsid, ext2);
            DeleteOrFail(key, &hr);
        }
        logf("UninstallPreviewDll: removed '%s'\n", prev.ext);
    }
    return hr == S_OK ? true : false;
}

// TODO: is anyone using this functionality?
void DisablePreviewInstallExts(const char* cmdLine) {
    // allows installing only a subset of available preview handlers
    if (str::StartsWithI(cmdLine, "exts:")) {
        TempStr extsList = str::DupTemp(cmdLine + 5);
        str::ToLowerInPlace(extsList);
        str::TransCharsInPlace(extsList, ";. :", ",,,\0");
        StrVec exts;
        Split(exts, extsList, ",", true);
        for (auto& p : gPreviewers) {
            p.skip = !exts.Contains(p.ext + 1);
        }
    }
}

bool IsPreviewInstalled() {
    const char* key = ".pdf\\shellex\\{8895b1c6-b41f-4c1c-a562-0d564250836f}";
    char* iid = LoggedReadRegStrTemp(HKEY_CLASSES_ROOT, key, nullptr);
    bool isInstalled = str::EqI(iid, kPdfPreviewClsid);
    logf("IsPreviewInstalled() isInstalled=%d\n", (int)isInstalled);
    return isInstalled;
}
