/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "base/File.h"

#include "RegistryPreview.h"
#include "SumatraLog.h"

#define kThumbnailProviderClsid "{e357fccd-a995-4576-b01f-234630154e96}"
#define kExtractImageClsid "{bb2e617c-0920-11d1-9a0b-00c04fc2d6c1}"
#define kPreviewHandlerClsid "{8895b1c6-b41f-4c1c-a562-0d564250836f}"
#define kAppIdPrevHostExe "{6d2b5079-2f0b-48dd-ab7f-97cec514d30b}"
#define kAppIdPrevHostExeWow64 "{534a1e02-d58f-44f0-b58b-36cbed287c7c}"

#define kRegKeyPreviewHandlers "Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers"

// clang-format off
static const struct {
    PreviewType type;
    Str clsid;
    Str ext;
    Str ext2;
} gPreviewers[] = {
    {PreviewType::Pdf, StrL(kPdfPreviewClsid), StrL(".pdf")},
    {PreviewType::Cbx, StrL(kCbxPreviewClsid), StrL(".cbz"), StrL(".cbr")},
    {PreviewType::Cbx, StrL(kCbxPreviewClsid), StrL(".cb7"), StrL(".cbt")},
    {PreviewType::Tga, StrL(kTgaPreviewClsid), StrL(".tga")},
    {PreviewType::DjVu, StrL(kDjVuPreviewClsid), StrL(".djvu")},
    {PreviewType::Xps, StrL(kXpsPreviewClsid), StrL(".xps"), StrL(".oxps")},
    {PreviewType::Epub, StrL(kEpubPreviewClsid), StrL(".epub")},
    // FictionBook: plain .fb2 and common zip containers (.fb2z, .fbz, .zfb2,
    // .fb2.zip). Multi-dot .fb2.zip needs its own Classes key so Explorer does
    // not treat it as a generic .zip (issue #1677).
    {PreviewType::Fb2, StrL(kFb2PreviewClsid), StrL(".fb2"), StrL(".fb2z")},
    {PreviewType::Fb2, StrL(kFb2PreviewClsid), StrL(".fbz"), StrL(".zfb2")},
    {PreviewType::Fb2, StrL(kFb2PreviewClsid), StrL(".fb2.zip")},
    {PreviewType::Mobi, StrL(kMobiPreviewClsid), StrL(".mobi")},
    {PreviewType::Ofd, StrL(kOfdPreviewClsid), StrL(".ofd")},
};
// clang-format on

static_assert(dimof(gPreviewers) * 2 <= 32, "a PreviewHandlers mask has a bit per extension");

bool FindPreviewType(REFCLSID clsid, PreviewType& type) {
    for (const auto& previewer : gPreviewers) {
        CLSID registered;
        if (SUCCEEDED(CLSIDFromString(CWStrTemp(previewer.clsid), &registered)) && IsEqualCLSID(clsid, registered)) {
            type = previewer.type;
            return true;
        }
    }
    return false;
}

// bit of gPreviewers[i].ext is 2*i, of .ext2 2*i+1
static u32 ExtBit(int previewerIdx, int extNo) {
    return 1u << (previewerIdx * 2 + extNo);
}

// HKCR\<ext>\shellex\<handlerIid>: the merged view Explorer reads
static TempStr ShellExHandlerTemp(Str ext, Str handlerIid) {
    TempStr key = fmt("%s\\shellex\\%s", ext, handlerIid);
    return ReadRegStrTemp(HKEY_CLASSES_ROOT, key, {});
}

// a handler of another app is registered, e.g. SageThumbs for thumbnails (#6442).
// A CLSID left behind by an uninstalled app doesn't count.
static bool IsForeignHandler(Str handlerClsid, Str ourClsid) {
    if (len(handlerClsid) == 0 || str::EqI(handlerClsid, ourClsid)) {
        return false;
    }
    TempStr key = fmt("CLSID\\%s\\InProcServer32", handlerClsid);
    return RegKeyExists(HKEY_CLASSES_ROOT, key);
}

enum class HandlerOwner {
    Us,
    OtherApp,
};

// mask of extensions whose <handlerIid> is registered by <owner>
static u32 HandlersMask(Str handlerIid, HandlerOwner owner) {
    u32 mask = 0;
    for (int i = 0; i < dimofi(gPreviewers); i++) {
        auto& prev = gPreviewers[i];
        Str exts[2] = {prev.ext, prev.ext2};
        for (int extNo = 0; extNo < 2; extNo++) {
            if (len(exts[extNo]) == 0) {
                continue;
            }
            TempStr clsid = ShellExHandlerTemp(exts[extNo], handlerIid);
            bool match =
                owner == HandlerOwner::OtherApp ? IsForeignHandler(clsid, prev.clsid) : str::EqI(clsid, prev.clsid);
            if (match) {
                mask |= ExtBit(i, extNo);
            }
        }
    }
    return mask;
}

PreviewHandlers GetOurPreviewHandlers() {
    return {HandlersMask(StrL(kPreviewHandlerClsid), HandlerOwner::Us),
            HandlersMask(StrL(kThumbnailProviderClsid), HandlerOwner::Us)};
}

PreviewHandlers GetForeignPreviewHandlers() {
    return {HandlersMask(StrL(kPreviewHandlerClsid), HandlerOwner::OtherApp),
            HandlersMask(StrL(kThumbnailProviderClsid), HandlerOwner::OtherApp)};
}

// registers the handlers in <which>: preview pane and / or thumbnails, per extension
bool InstallPreviewDll(Str dllPath, bool allUsers, PreviewHandlers which) {
    HKEY hkey = allUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    logf("InstallPreviewDll: preview=0x%x thumbnails=0x%x\n", which.preview, which.thumbnails);

    for (int i = 0; i < dimofi(gPreviewers); i++) {
        auto& prev = gPreviewers[i];
        u32 bits = ExtBit(i, 0) | ExtBit(i, 1);
        bool withPreview = which.preview & bits;
        bool withThumbnails = which.thumbnails & bits;
        if (!withPreview && !withThumbnails) {
            continue;
        }
        Str clsid = prev.clsid;
        Str exts[2] = {prev.ext, prev.ext2};
        bool ok = true;

        TempStr displayName = fmt("SumatraPDF Preview (*%s)", prev.ext);
        // register class
        TempStr key = fmt("Software\\Classes\\CLSID\\%s", clsid);
        ok &= LoggedWriteRegStr(hkey, key, {}, displayName);
        ok &= LoggedWriteRegStr(hkey, key, StrL("AppId"),
                                IsRunningInWow64() ? StrL(kAppIdPrevHostExeWow64) : StrL(kAppIdPrevHostExe));
        ok &= LoggedWriteRegStr(hkey, key, StrL("DisplayName"), displayName);
        key = fmt("Software\\Classes\\CLSID\\%s\\InProcServer32", clsid);
        ok &= LoggedWriteRegStr(hkey, key, {}, dllPath);
        ok &= LoggedWriteRegStr(hkey, key, StrL("ThreadingModel"), StrL("Apartment"));

        for (int extNo = 0; extNo < 2; extNo++) {
            Str ext = exts[extNo];
            if (len(ext) == 0) {
                continue;
            }
            u32 bit = ExtBit(i, extNo);
            if (which.thumbnails & bit) {
                key = fmt("Software\\Classes\\%s\\shellex\\" kThumbnailProviderClsid, ext);
                ok &= LoggedWriteRegStr(hkey, key, {}, clsid);
            }
            if (which.preview & bit) {
                key = fmt("Software\\Classes\\%s\\shellex\\" kPreviewHandlerClsid, ext);
                ok &= LoggedWriteRegStr(hkey, key, {}, clsid);
            }
        }
        if (withPreview) {
            ok &= LoggedWriteRegStr(hkey, StrL(kRegKeyPreviewHandlers), clsid, displayName);
        }
        if (!ok) {
            return false;
        }
    }

    return true;
}

// deletes HKLM and HKCU <key> if its default value is <clsid>: another app may
// have registered its own handler there since
static void DeleteIfOurs(Str key, Str clsid, HRESULT* hr) {
    HKEY hkeys[2] = {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER};
    for (HKEY hkey : hkeys) {
        TempStr val = ReadRegStrTemp(hkey, key, {});
        if (!str::EqI(val, clsid)) {
            continue;
        }
        if (!LoggedDeleteRegKey(hkey, key) && hkey == HKEY_CURRENT_USER) {
            *hr = E_FAIL;
        }
    }
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
    for (const auto& prev : gPreviewers) {
        Str clsid = prev.clsid;
        Str exts[2] = {prev.ext, prev.ext2};

        // unregister preview handler
        DeleteRegValue(HKEY_LOCAL_MACHINE, StrL(kRegKeyPreviewHandlers), clsid);
        DeleteRegValue(HKEY_CURRENT_USER, StrL(kRegKeyPreviewHandlers), clsid);
        // remove class data
        key = fmt("Software\\Classes\\CLSID\\%s", clsid);
        DeleteOrFail(key, &hr);
        for (Str ext : exts) {
            if (len(ext) == 0) {
                continue;
            }
            // IThumbnailProvider, IExtractImage (for Windows XP), IPreviewHandler
            key = fmt("Software\\Classes\\%s\\shellex\\" kThumbnailProviderClsid, ext);
            DeleteIfOurs(key, clsid, &hr);
            key = fmt("Software\\Classes\\%s\\shellex\\" kExtractImageClsid, ext);
            DeleteIfOurs(key, clsid, &hr);
            key = fmt("Software\\Classes\\%s\\shellex\\" kPreviewHandlerClsid, ext);
            DeleteIfOurs(key, clsid, &hr);
        }
        logf("UninstallPreviewDll: removed '%s'\n", prev.ext);
    }
    return hr == S_OK ? true : false;
}

// any of our preview pane or thumbnail handlers is registered
bool IsPreviewInstalled() {
    PreviewHandlers ours = GetOurPreviewHandlers();
    bool isInstalled = ours.preview != 0 || ours.thumbnails != 0;
    logf("IsPreviewInstalled() preview=0x%x thumbnails=0x%x\n", ours.preview, ours.thumbnails);
    return isInstalled;
}

// --- opt-in PdfPreview.dll file logging ---------------------------------------

#define kRegKeySumatra "Software\\SumatraPDF"
#define kRegValLogPdfPreview "LogPdfPreview"

bool IsPdfPreviewLoggingEnabled() {
    DWORD val = 0;
    if (!ReadRegDWORD(HKEY_CURRENT_USER, StrL(kRegKeySumatra), StrL(kRegValLogPdfPreview), val)) {
        return false;
    }
    return val != 0;
}

void SetPdfPreviewLoggingEnabled(bool enable) {
    WriteRegDWORD(HKEY_CURRENT_USER, StrL(kRegKeySumatra), StrL(kRegValLogPdfPreview), enable ? 1 : 0);
    logf("SetPdfPreviewLoggingEnabled: %d\n", (int)enable);
}

// Per-build data dir of the SumatraPDF.exe next to this module, the same one
// GetSumatraBuildSpecificDirTemp picks (...\SumatraPDF-data\<build id prefix>),
// so logs land next to its crashinfo/logs.
TempStr GetPdfPreviewLogDirTemp() {
    TempStr exeDir = GetSelfExeDirTemp();
    if (len(exeDir) == 0) {
        return {};
    }
    TempStr exePath = path::JoinTemp(exeDir, StrL("SumatraPDF.exe"));
    TempStr buildId = GetPeBuildIdTemp(exePath);
    if (len(buildId) == 0) {
        return {};
    }
    char id[7] = "000000";
    str::BufSet(Str(id, dimof(id)), buildId);
    TempStr local = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, false);
    if (len(local) == 0) {
        return {};
    }
    TempStr dir = path::JoinTemp(local, StrL("SumatraPDF-data"));
    return path::JoinTemp(dir, Str(id));
}

// pdfpreview.log.<month>-<day>.<hour>-<minute>.<unique>.txt
static TempStr GetNewPdfPreviewLogFilePathTemp() {
    TempStr dir = GetPdfPreviewLogDirTemp();
    if (len(dir) == 0) {
        return {};
    }
    SYSTEMTIME st{};
    GetLocalTime(&st);
    // unique part: pid plus low bits of tick, so concurrent preview hosts that
    // start in the same minute don't collide
    DWORD uniq = (GetCurrentProcessId() << 16) ^ (DWORD)(GetTickCount64() & 0xffff);
    TempStr name = fmt("%s%02d-%02d.%02d-%02d.%08x.txt", Str(kPdfPreviewLogPrefix), (int)st.wMonth, (int)st.wDay,
                       (int)st.wHour, (int)st.wMinute, uniq);
    return path::JoinTemp(dir, name);
}

// if logging is enabled, route this module's log to a fresh unique file
void StartPdfPreviewLoggingIfEnabled() {
    static bool started = false;
    if (started || !IsPdfPreviewLoggingEnabled()) {
        return;
    }
    started = true;
    TempStr path = GetNewPdfPreviewLogFilePathTemp();
    if (len(path) == 0) {
        return;
    }
    // WriteCurrentLogToFile creates the directory and flushes whatever we've
    // already buffered (e.g. DllMain); StartLogToFile appends subsequent lines.
    WriteCurrentLogToFile(path);
    StartLogToFile(path, false);
    logf("PdfPreview: logging to '%s'\n", path);
}
