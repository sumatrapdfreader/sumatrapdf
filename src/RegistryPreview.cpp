/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "base/File.h"
#include "base/Crypto.h"

#include "RegistryPreview.h"
#include "SumatraLog.h"

#define kThumbnailProviderClsid "{e357fccd-a995-4576-b01f-234630154e96}"
#define kExtractImageClsid "{bb2e617c-0920-11d1-9a0b-00c04fc2d6c1}"
#define kPreviewHandlerClsid "{8895b1c6-b41f-4c1c-a562-0d564250836f}"
#define kAppIdPrevHostExe "{6d2b5079-2f0b-48dd-ab7f-97cec514d30b}"
#define kAppIdPrevHostExeWow64 "{534a1e02-d58f-44f0-b58b-36cbed287c7c}"

#define kRegKeyPreviewHandlers "Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers"

// clang-format off
static struct {
    Str clsid;
    Str ext;
    Str ext2;
    bool skip = false;
} gPreviewers[] = {
    {kPdfPreviewClsid, ".pdf"},
    {kCbxPreviewClsid, ".cbz", ".cbr"},
    {kCbxPreviewClsid, ".cb7", ".cbt"},
    {kTgaPreviewClsid, ".tga"},
    {kDjVuPreviewClsid, ".djvu"},
    {kXpsPreviewClsid, ".xps", ".oxps"},
    {kEpubPreviewClsid, ".epub"},
    {kFb2PreviewClsid, ".fb2", ".fb2z"},
    {kMobiPreviewClsid, ".mobi"},
};
// clang-format on

bool InstallPreviewDll(Str dllPath, bool allUsers) {
    HKEY hkey = allUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    bool ok;

    for (auto& prev : gPreviewers) {
        if (prev.skip) {
            continue;
        }
        Str clsid = prev.clsid;
        Str ext = prev.ext;
        Str ext2 = prev.ext2;
        ok = true;

        TempStr displayName = fmt("SumatraPDF Preview (*%s)", ext);
        // register class
        TempStr key = fmt("Software\\Classes\\CLSID\\%s", clsid);
        ok &= LoggedWriteRegStr(hkey, key, nullptr, displayName);
        ok &= LoggedWriteRegStr(hkey, key, "AppId", IsRunningInWow64() ? kAppIdPrevHostExeWow64 : kAppIdPrevHostExe);
        ok &= LoggedWriteRegStr(hkey, key, "DisplayName", displayName);
        key = fmt("Software\\Classes\\CLSID\\%s\\InProcServer32", clsid);
        ok &= LoggedWriteRegStr(hkey, key, nullptr, dllPath);
        ok &= LoggedWriteRegStr(hkey, key, "ThreadingModel", "Apartment");
        // IThumbnailProvider
        key = fmt("Software\\Classes\\%s\\shellex\\" kThumbnailProviderClsid, ext);
        ok &= LoggedWriteRegStr(hkey, key, nullptr, clsid);
        if (ext2) {
            key = fmt("Software\\Classes\\%s\\shellex\\" kThumbnailProviderClsid, ext2);
            ok &= LoggedWriteRegStr(hkey, key, nullptr, clsid);
        }
        // IPreviewHandler
        key = fmt("Software\\Classes\\%s\\shellex\\" kPreviewHandlerClsid, ext);
        ok &= LoggedWriteRegStr(hkey, key, nullptr, clsid);
        if (ext2) {
            key = fmt("Software\\Classes\\%s\\shellex\\" kPreviewHandlerClsid, ext2);
            ok &= LoggedWriteRegStr(hkey, key, nullptr, clsid);
        }
        ok &= LoggedWriteRegStr(hkey, kRegKeyPreviewHandlers, clsid, displayName);
        if (!ok) {
            return false;
        }
    }

    return true;
}

static void DeleteOrFail(Str key, HRESULT* hr) {
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
        Str clsid = prev.clsid;
        Str ext = prev.ext;
        Str ext2 = prev.ext2;

        // unregister preview handler
        DeleteRegValue(HKEY_LOCAL_MACHINE, kRegKeyPreviewHandlers, clsid);
        DeleteRegValue(HKEY_CURRENT_USER, kRegKeyPreviewHandlers, clsid);
        // remove class data
        key = fmt("Software\\Classes\\CLSID\\%s", clsid);
        DeleteOrFail(key, &hr);
        // IThumbnailProvider
        key = fmt("Software\\Classes\\%s\\shellex\\" kThumbnailProviderClsid, ext);
        DeleteOrFail(key, &hr);
        if (ext2) {
            key = fmt("Software\\Classes\\%s\\shellex\\" kThumbnailProviderClsid, ext2);
            DeleteOrFail(key, &hr);
        }
        // IExtractImage (for Windows XP)
        key = fmt("Software\\Classes\\%s\\shellex\\" kExtractImageClsid, ext);
        DeleteOrFail(key, &hr);
        if (ext2) {
            key = fmt("Software\\Classes\\%s\\shellex\\" kExtractImageClsid, ext2);
            DeleteOrFail(key, &hr);
        }
        // IPreviewHandler
        key = fmt("Software\\Classes\\%s\\shellex\\" kPreviewHandlerClsid, ext);
        DeleteOrFail(key, &hr);
        if (ext2) {
            key = fmt("Software\\Classes\\%s\\shellex\\" kPreviewHandlerClsid, ext2);
            DeleteOrFail(key, &hr);
        }
        logf("UninstallPreviewDll: removed '%s'\n", prev.ext);
    }
    return hr == S_OK ? true : false;
}

// TODO: is anyone using this functionality?
void DisablePreviewInstallExts(Str cmdLine) {
    // allows installing only a subset of available preview handlers
    if (str::StartsWithI(cmdLine, "exts:")) {
        TempStr extsList = str::DupTemp(Str(cmdLine.s + 5, cmdLine.len - 5));
        str::ToLowerInPlace(extsList);
        str::TransCharsInPlace(extsList, StrL(";. :"), StrL(",,,\0"));
        StrVec exts;
        Split(&exts, extsList, ",", true);
        for (auto& p : gPreviewers) {
            Str extNoDot = Str(p.ext.s + 1, p.ext.len - 1);
            p.skip = !exts.Contains(extNoDot);
        }
    }
}

bool IsPreviewInstalled() {
    Str key = ".pdf\\shellex\\{8895b1c6-b41f-4c1c-a562-0d564250836f}";
    TempStr iid = LoggedReadRegStrTemp(HKEY_CLASSES_ROOT, key, nullptr);
    bool isInstalled = str::EqI(iid, kPdfPreviewClsid);
    logf("IsPreviewInstalled() isInstalled=%d\n", (int)isInstalled);
    return isInstalled;
}

// --- opt-in PdfPreview.dll file logging ---------------------------------------

#define kRegKeySumatra "Software\\SumatraPDF"
#define kRegValLogPdfPreview "LogPdfPreview"

bool IsPdfPreviewLoggingEnabled() {
    DWORD val = 0;
    if (!ReadRegDWORD(HKEY_CURRENT_USER, kRegKeySumatra, kRegValLogPdfPreview, val)) {
        return false;
    }
    return val != 0;
}

void SetPdfPreviewLoggingEnabled(bool enable) {
    WriteRegDWORD(HKEY_CURRENT_USER, kRegKeySumatra, kRegValLogPdfPreview, enable ? 1 : 0);
    logf("SetPdfPreviewLoggingEnabled: %d\n", (int)enable);
}

// Per-build data dir, keyed on the sha1 of the SumatraPDF.exe sitting next to
// this module: in SumatraPDF.exe that's the running exe, in PdfPreview.dll it's
// the sibling exe -- either way it resolves to the same directory SumatraPDF.exe
// uses (see GetBuildDirNameTemp), so logs land next to its crashinfo/logs.
TempStr GetPdfPreviewLogDirTemp() {
    TempStr exeDir = GetSelfExeDirTemp();
    if (!exeDir) {
        return {};
    }
    TempStr exePath = path::JoinTemp(exeDir, StrL("SumatraPDF.exe"));
    Str d = file::ReadFile(exePath);
    if (len(d) == 0) {
        return {};
    }
    u8 sha1[20]{};
    CalcSHA1Digest(d, sha1);
    str::Free(d);
    char id[7];
    for (int i = 0; i < 3; i++) { // first 6 hex chars (3 bytes), matches GetBuildDirNameTemp
        sprintf_s(&id[2 * i], 3, "%02x", sha1[i]);
    }
    TempStr local = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, false);
    if (!local) {
        return {};
    }
    TempStr dir = path::JoinTemp(local, StrL("SumatraPDF-data"));
    return path::JoinTemp(dir, id);
}

// pdfpreview.log.<month>-<day>.<hour>-<minute>.<unique>.txt
static TempStr GetNewPdfPreviewLogFilePathTemp() {
    TempStr dir = GetPdfPreviewLogDirTemp();
    if (!dir) {
        return {};
    }
    SYSTEMTIME st{};
    GetLocalTime(&st);
    // unique part: pid plus low bits of tick, so concurrent preview hosts that
    // start in the same minute don't collide
    DWORD uniq = (GetCurrentProcessId() << 16) ^ (DWORD)(GetTickCount64() & 0xffff);
    TempStr name = fmt("%s%02d-%02d.%02d-%02d.%08x.txt", Str(kPdfPreviewLogPrefix), (int)st.wMonth, (int)st.wDay,
                       (int)st.wHour, (int)st.wMinute, uniq);
    return path::JoinTemp(dir.s, name.s);
}

void StartPdfPreviewLoggingIfEnabled() {
    static bool started = false;
    if (started || !IsPdfPreviewLoggingEnabled()) {
        return;
    }
    started = true;
    TempStr path = GetNewPdfPreviewLogFilePathTemp();
    if (!path) {
        return;
    }
    // WriteCurrentLogToFile creates the directory and flushes whatever we've
    // already buffered (e.g. DllMain); StartLogToFile appends subsequent lines.
    WriteCurrentLogToFile(path);
    StartLogToFile(path, false);
    logf("PdfPreview: logging to '%s'\n", path);
}
