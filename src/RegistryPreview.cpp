/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/FileUtil.h"

#include "RegistryPreview.h"

#define CLSID_I_THUMBNAIL_PROVIDER L"{e357fccd-a995-4576-b01f-234630154e96}"
#define CLSID_I_EXTRACT_IMAGE L"{bb2e617c-0920-11d1-9a0b-00c04fc2d6c1}"
#define CLSID_I_PREVIEW_HANDLER L"{8895b1c6-b41f-4c1c-a562-0d564250836f}"
#define APPID_PREVHOST_EXE L"{6d2b5079-2f0b-48dd-ab7f-97cec514d30b}"
#define APPID_PREVHOST_EXE_WOW64 L"{534a1e02-d58f-44f0-b58b-36cbed287c7c}"

#define REG_KEY_PREVIEW_HANDLERS L"Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers"

// clang-format off
static struct {
    const WCHAR* clsid;
    const WCHAR *ext, *ext2;
    bool skip;
} gPreviewers[] = {
    {SZ_PDF_PREVIEW_CLSID, L".pdf"},
    {SZ_CBX_PREVIEW_CLSID, L".cbz"},
    {SZ_CBX_PREVIEW_CLSID, L".cbr"},
    {SZ_CBX_PREVIEW_CLSID, L".cb7"},
    {SZ_CBX_PREVIEW_CLSID, L".cbt"},
    {SZ_TGA_PREVIEW_CLSID, L".tga"},
    {SZ_DJVU_PREVIEW_CLSID, L".djvu"},
#ifdef BUILD_XPS_PREVIEW
    {SZ_XPS_PREVIEW_CLSID, L".xps", L".oxps"},
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
};
// clang-format on

bool InstallPreviewDll(const WCHAR* dllPath, bool allUsers) {
    HKEY hkey = allUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    bool ok;

    for (int i = 0; i < dimof(gPreviewers); i++) {
        if (gPreviewers[i].skip) {
            continue;
        }
        const WCHAR* clsid = gPreviewers[i].clsid;
        const WCHAR* ext = gPreviewers[i].ext;
        const WCHAR* ext2 = gPreviewers[i].ext2;
        ok = true;

        AutoFreeWstr displayName = str::Format(L"SumatraPDF Preview (*%s)", ext);
        // register class
        AutoFreeWstr key = str::Format(L"Software\\Classes\\CLSID\\%s", clsid);
        ok &= LoggedWriteRegStr(hkey, key, nullptr, displayName);
        ok &=
            LoggedWriteRegStr(hkey, key, L"AppId", IsRunningInWow64() ? APPID_PREVHOST_EXE_WOW64 : APPID_PREVHOST_EXE);
        ok &= LoggedWriteRegStr(hkey, key, L"DisplayName", displayName);
        key.Set(str::Format(L"Software\\Classes\\CLSID\\%s\\InProcServer32", clsid));
        ok &= LoggedWriteRegStr(hkey, key, nullptr, dllPath);
        ok &= LoggedWriteRegStr(hkey, key, L"ThreadingModel", L"Apartment");
        // IThumbnailProvider
        key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_THUMBNAIL_PROVIDER, ext));
        ok &= LoggedWriteRegStr(hkey, key, nullptr, clsid);
        if (ext2) {
            key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_THUMBNAIL_PROVIDER, ext2));
            ok &= LoggedWriteRegStr(hkey, key, nullptr, clsid);
        }
        // IPreviewHandler
        key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_PREVIEW_HANDLER, ext));
        ok &= LoggedWriteRegStr(hkey, key, nullptr, clsid);
        if (ext2) {
            key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_PREVIEW_HANDLER, ext2));
            ok &= LoggedWriteRegStr(hkey, key, nullptr, clsid);
        }
        ok &= LoggedWriteRegStr(hkey, REG_KEY_PREVIEW_HANDLERS, clsid, displayName);
        if (!ok) {
            return false;
        }
    }

    return true;
}

static void DeleteOrFail(const WCHAR* key, HRESULT* hr) {
    LoggedDeleteRegKey(HKEY_LOCAL_MACHINE, key);
    if (!LoggedDeleteRegKey(HKEY_CURRENT_USER, key)) {
        *hr = E_FAIL;
    }
}

// we delete from HKLM and HKCU for compat with pre-3.4
bool UninstallPreviewDll() {
    HRESULT hr = S_OK;

    for (int i = 0; i < dimof(gPreviewers); i++) {
        if (gPreviewers[i].skip) {
            continue;
        }
        const WCHAR* clsid = gPreviewers[i].clsid;
        const WCHAR* ext = gPreviewers[i].ext;
        const WCHAR* ext2 = gPreviewers[i].ext2;

        // unregister preview handler
        DeleteRegValue(HKEY_LOCAL_MACHINE, REG_KEY_PREVIEW_HANDLERS, clsid);
        DeleteRegValue(HKEY_CURRENT_USER, REG_KEY_PREVIEW_HANDLERS, clsid);
        // remove class data
        AutoFreeWstr key(str::Format(L"Software\\Classes\\CLSID\\%s", clsid));
        DeleteOrFail(key, &hr);
        // IThumbnailProvider
        key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_THUMBNAIL_PROVIDER, ext));
        DeleteOrFail(key, &hr);
        if (ext2) {
            key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_THUMBNAIL_PROVIDER, ext2));
            DeleteOrFail(key, &hr);
        }
        // IExtractImage (for Windows XP)
        key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_EXTRACT_IMAGE, ext));
        DeleteOrFail(key, &hr);
        if (ext2) {
            key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_EXTRACT_IMAGE, ext2));
            DeleteOrFail(key, &hr);
        }
        // IPreviewHandler
        key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_PREVIEW_HANDLER, ext));
        DeleteOrFail(key, &hr);
        if (ext2) {
            key.Set(str::Format(L"Software\\Classes\\%s\\shellex\\" CLSID_I_PREVIEW_HANDLER, ext2));
            DeleteOrFail(key, &hr);
        }
    }
    return hr == S_OK ? true : false;
}

// TODO: is anyone using this functionality?
void DisablePreviewInstallExts(const WCHAR* cmdLine) {
    // allows installing only a subset of available preview handlers
    if (str::StartsWithI(cmdLine, L"exts:")) {
        AutoFreeWstr extsList = str::Dup(cmdLine + 5);
        str::ToLowerInPlace(extsList);
        str::TransCharsInPlace(extsList, L";. :", L",,,\0");
        WStrVec exts;
        exts.Split(extsList, L",", true);
        for (int i = 0; i < dimof(gPreviewers); i++) {
            gPreviewers[i].skip = !exts.Contains(gPreviewers[i].ext + 1);
        }
    }
}
