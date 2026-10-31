/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/CmdLineArgs.h"
#include "base/Crypto.h"
#include "base/DirScan.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Http.h"
#include "base/JsonParser.h"
#include "base/Timer.h"
#include "base/Win.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "DocController.h"
#include "DocProperties.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "Permissions.h"

Kind kindEngineCaj = "engineCaj";

// CAJ, KDH and NH are converted to a cached PDF by caj2pdf, an exe we publish
// (hack/sumatrapdf/caj2pdf/release.ts) and download when the user agrees to it.
//
//   files.sumatrapdfreader.org/software/caj2pdf/rel/x64/latestver.json
//                                                      /<ver>/caj2pdf.exe
//
//   <data dir>/caj2pdf/latestver.json   describes the exe we have
//                     /<ver>/caj2pdf.exe
//                     /cache/<md5>.pdf
#if OS_WIN

bool HasPermission(Perm permission);
TempStr GetSumatraDataDirTemp();

constexpr const char* kCaj2pdfBaseUrl = "https://files.sumatrapdfreader.org/software/caj2pdf/rel/x64";
constexpr i64 kMaxCaj2pdfSize = 256LL * 1024 * 1024;
constexpr i64 kUpdateCheckAgeSec = 24LL * 60 * 60;
constexpr i64 kCajCacheMaxAgeSec = 7LL * 24 * 60 * 60;
constexpr i64 kCajTmpMaxAgeSec = 24LL * 60 * 60;
constexpr DWORD kConvertTimeoutMs = 5 * 60 * 1000;

// serializes installing a new caj2pdf.exe
static Mutex gCaj2pdfUpdateLock;

struct Caj2pdfVer {
    Str ver;
    Str sha1;
    i64 size = 0;
};

// tests serve their own converter and keep it away from the user's data dir
static TempStr DebugEnvTemp(const char* name) {
#if defined(DEBUG)
    const char* v = getenv(name);
    if (v) {
        return str::DupTemp(Str(v));
    }
#else
    (void)name;
#endif
    return {};
}

static TempStr Caj2pdfBaseUrlTemp() {
    TempStr url = DebugEnvTemp("SUMATRAPDF_CAJ2PDF_URL");
    return len(url) > 0 ? url : str::DupTemp(Str(kCaj2pdfBaseUrl));
}

static TempStr Caj2pdfDirTemp() {
    TempStr dir = DebugEnvTemp("SUMATRAPDF_CAJ2PDF_DIR");
    if (len(dir) > 0) {
        return dir;
    }
    TempStr dataDir = GetSumatraDataDirTemp();
    return len(dataDir) > 0 ? path::JoinTemp(dataDir, StrL("caj2pdf")) : TempStr{};
}

static TempStr LatestVerPathTemp(Str dir) {
    return path::JoinTemp(dir, StrL("latestver.json"));
}

static TempStr Caj2pdfExePathTemp(Str dir, Str ver) {
    return path::JoinTemp(dir, ver, StrL("caj2pdf.exe"));
}

// ver becomes a path segment and a part of a url
static bool IsValidVer(Str ver) {
    if (len(ver) == 0 || len(ver) > 32 || ver.s[0] == '.') {
        return false;
    }
    for (int i = 0; i < len(ver); i++) {
        char c = ver.s[i];
        bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == '-';
        if (!ok) {
            return false;
        }
    }
    return true;
}

static bool ParseLatestVer(Str data, Caj2pdfVer* out) {
    json::Reader r(data);
    for (json::Value& v : r) {
        if (json::PathMatch(v.path, StrL("/Ver"))) {
            out->ver = str::DupTemp(v.value);
        } else if (json::PathMatch(v.path, StrL("/Sha1"))) {
            out->sha1 = str::DupTemp(v.value);
        } else if (json::PathMatch(v.path, StrL("/Size"))) {
            out->size = (i64)atoll(CStrTemp(v.value));
        }
    }
    return r.Ok() && IsValidVer(out->ver) && len(out->sha1) > 0 && out->size > 0;
}

static bool FileMatchesVer(Str path, const Caj2pdfVer& v) {
    if (file::GetSize(path) != v.size) {
        return false;
    }
    Str data = file::ReadFile(path);
    if (len(data) == 0) {
        return false;
    }
    u8 digest[20]{};
    CalcSHA1Digest(data, digest);
    str::Free(data);
    TempStr hex = str::MemToHexTemp(Str((const char*)digest, dimofi(digest)));
    return str::EqI(hex, v.sha1);
}

// per process and thread, because other instances and tabs may do the same
static TempStr UniqueTmpPathTemp(Str path) {
    return fmt("%s.%u.%u.tmp", path, (uint)GetCurrentProcessId(), (uint)GetCurrentThreadId());
}

static void RemoveOtherVersions(Str dir, Str ver) {
    DirIter di{dir};
    di.includeFiles = false;
    di.includeDirs = true;
    for (DirIterEntry* de : di) {
        if (str::Eq(de->name, ver) || str::Eq(de->name, StrL("cache"))) {
            continue;
        }
        // fails while an older exe is still converting; the next check retries
        bool ok = dir::RemoveAll(de->filePath);
        logf("caj2pdf: removing '%s' -> %d\n", de->filePath, (int)ok);
    }
}

static bool DownloadExe(Str dir, const Caj2pdfVer& v) {
    TempStr exePath = Caj2pdfExePathTemp(dir, v.ver);
    if (FileMatchesVer(exePath, v)) {
        return true;
    }
    if (!dir::CreateForFile(exePath)) {
        logf("caj2pdf: dir::CreateForFile('%s') failed\n", exePath);
        return false;
    }

    TempStr url = fmt("%s/%s/caj2pdf.exe", Caj2pdfBaseUrlTemp(), v.ver);
    TempStr tmpPath = UniqueTmpPathTemp(exePath);
    Func1<HttpProgress*> noProgress{};
    if (!HttpGetToFile(url, tmpPath, noProgress, kMaxCaj2pdfSize)) {
        logf("caj2pdf: downloading '%s' failed\n", url);
        return false;
    }
    if (!FileMatchesVer(tmpPath, v)) {
        logf("caj2pdf: '%s' doesn't match latestver.json\n", url);
        file::Delete(tmpPath);
        return false;
    }
    if (!file::RenameReplace(exePath, tmpPath)) {
        // another instance got there first and may be running it
        file::Delete(tmpPath);
        return FileMatchesVer(exePath, v);
    }
    return true;
}

// Installs the published version. latestver.json is written after the exe is in
// place, so it never describes an exe we don't have.
static bool UpdateCaj2pdfLocked() {
    TempStr dir = Caj2pdfDirTemp();
    if (len(dir) == 0 || !dir::CreateAll(dir)) {
        logf("caj2pdf: no usable data dir\n");
        return false;
    }

    TempStr url = fmt("%s/latestver.json", Caj2pdfBaseUrlTemp());
    HttpRsp rsp;
    if (!HttpGet(url, &rsp) || !IsHttpRspOk(&rsp)) {
        logf("caj2pdf: downloading '%s' failed\n", url);
        return false;
    }
    Str json = ToStr(rsp.data);
    Caj2pdfVer v;
    if (!ParseLatestVer(json, &v)) {
        logf("caj2pdf: invalid '%s'\n", url);
        return false;
    }
    if (!DownloadExe(dir, v)) {
        return false;
    }

    TempStr latestPath = LatestVerPathTemp(dir);
    TempStr tmpPath = UniqueTmpPathTemp(latestPath);
    if (!file::WriteFile(tmpPath, json) || !file::RenameReplace(latestPath, tmpPath)) {
        logf("caj2pdf: writing '%s' failed\n", latestPath);
        file::Delete(tmpPath);
        return false;
    }
    logf("caj2pdf: using version '%s'\n", v.ver);
    RemoveOtherVersions(dir, v.ver);
    return true;
}

static void CheckForCaj2pdfUpdate() {
    // an update already running does the same work
    if (!gCaj2pdfUpdateLock.TryLock()) {
        return;
    }
    UpdateCaj2pdfLocked();
    gCaj2pdfUpdateLock.Unlock();
}

static i64 FileAgeSec(FILETIME ft) {
    FILETIME nowFt{};
    GetSystemTimeAsFileTime(&nowFt);
    u64 now = FileTimeToU64(nowFt);
    u64 then = FileTimeToU64(ft);
    if (then == 0 || then > now) {
        return 0;
    }
    return (i64)((now - then) / kFileTimeTicksPerSec);
}

// a document shouldn't wait for the network when we can already convert it
static void MaybeCheckForCaj2pdfUpdate(Str dir) {
    if (!HasPermission(Perm::InternetAccess)) {
        return;
    }
    FILETIME checkedAt = file::GetModificationTime(LatestVerPathTemp(dir));
    if (FileAgeSec(checkedAt) >= kUpdateCheckAgeSec) {
        RunAsync(MkFunc0Void(CheckForCaj2pdfUpdate), StrL("CheckForCaj2pdfUpdate"));
    }
}

static TempStr InstalledCaj2pdfTemp() {
    TempStr dir = Caj2pdfDirTemp();
    if (len(dir) == 0) {
        return {};
    }
    Str data = file::ReadFile(LatestVerPathTemp(dir));
    Caj2pdfVer v;
    bool ok = len(data) > 0 && ParseLatestVer(data, &v);
    str::Free(data);
    if (!ok) {
        return {};
    }
    TempStr exePath = Caj2pdfExePathTemp(dir, v.ver);
    return file::Exists(exePath) ? exePath : TempStr{};
}

static TempStr EnsureCaj2pdfTemp(PasswordUI* pwdUI) {
    TempStr exePath = InstalledCaj2pdfTemp();
    if (len(exePath) > 0) {
        MaybeCheckForCaj2pdfUpdate(Caj2pdfDirTemp());
        return exePath;
    }
    if (!HasPermission(Perm::InternetAccess)) {
        logf("caj2pdf: not installed, internet access is restricted\n");
        return {};
    }

    ScopedMutex scope(&gCaj2pdfUpdateLock);
    // another document may have installed it while we waited
    exePath = InstalledCaj2pdfTemp();
    if (len(exePath) > 0) {
        return exePath;
    }
    // under the lock, so documents opened together ask one at a time
    if (!pwdUI || !pwdUI->ConfirmCaj2pdfDownload()) {
        logf("caj2pdf: not installed, download declined\n");
        return {};
    }
    auto timeStart = TimeGet();
    if (!UpdateCaj2pdfLocked()) {
        return {};
    }
    logf("caj2pdf: installed in %.2f ms\n", TimeSinceInMs(timeStart));
    return InstalledCaj2pdfTemp();
}

static bool RunCaj2pdf(Str exe, Str src, Str dst) {
    TempStr cmdLine = fmt("%s %s -o %s --force --quiet", QuoteCmdLineArgTemp(exe), QuoteCmdLineArgTemp(src),
                          QuoteCmdLineArgTemp(dst));
    HANDLE process = LaunchProcessInDir(cmdLine, {}, CREATE_NO_WINDOW);
    if (!process) {
        logf("caj2pdf: CreateProcess failed: %s\n", cmdLine);
        return false;
    }
    DWORD wait = WaitForSingleObject(process, kConvertTimeoutMs);
    DWORD exitCode = EXIT_FAILURE;
    GetExitCodeProcess(process, &exitCode);
    if (wait != WAIT_OBJECT_0) {
        TerminateProcess(process, 1);
        logf("caj2pdf: timed out: %s\n", cmdLine);
    }
    CloseHandle(process);
    if (wait != WAIT_OBJECT_0) {
        return false;
    }
    if (exitCode != EXIT_SUCCESS) {
        logf("caj2pdf: exit %u: %s\n", exitCode, cmdLine);
        return false;
    }
    return true;
}

static TempStr CajCacheDirTemp() {
    TempStr dir = Caj2pdfDirTemp();
    return len(dir) > 0 ? path::JoinTemp(dir, StrL("cache")) : TempStr{};
}

// md5(path|size|mtime).pdf: an edited file gets a new name, so a PDF that an open
// tab still has locked never needs replacing
static TempStr CajPdfPathTemp(Str dir, Str src) {
    FILETIME mtime = file::GetModificationTime(src);
    TempStr key = fmt("%s|%lld|%u|%u", src, (long long)file::GetSize(src), mtime.dwHighDateTime, mtime.dwLowDateTime);
    u8 digest[16]{};
    CalcMD5Digest(key, digest);
    TempStr hex = str::MemToHexTemp(Str((const char*)digest, dimofi(digest)));
    return path::JoinTemp(dir, str::JoinTemp(hex, StrL(".pdf")));
}

// A hit bumps the access time, which keeps it from DeleteStaleCajCache.
static TempStr EnsureCajPdfTemp(Str src, PasswordUI* pwdUI) {
    TempStr dir = CajCacheDirTemp();
    if (len(dir) == 0 || !dir::CreateAll(dir)) {
        logf("caj2pdf: cache dir unavailable\n");
        return {};
    }

    TempStr pdfPath = CajPdfPathTemp(dir, src);
    if (file::Exists(pdfPath)) {
        FILETIME now{};
        GetSystemTimeAsFileTime(&now);
        file::SetAccessTime(pdfPath, now);
        logf("caj2pdf: cache hit '%s'\n", pdfPath);
        return pdfPath;
    }

    TempStr exePath = EnsureCaj2pdfTemp(pwdUI);
    if (len(exePath) == 0) {
        return {};
    }

    TempStr tmpPath = UniqueTmpPathTemp(pdfPath);
    auto timeStart = TimeGet();
    if (!RunCaj2pdf(exePath, src, tmpPath)) {
        file::Delete(tmpPath);
        return {};
    }
    if (!file::RenameReplace(pdfPath, tmpPath)) {
        // same name means same content, so a copy that got there first will do
        file::Delete(tmpPath);
        if (!file::Exists(pdfPath)) {
            logf("caj2pdf: rename to '%s' failed\n", pdfPath);
            return {};
        }
    }
    logf("caj2pdf: converted '%s' to '%s' with '%s' in %.2f ms\n", src, pdfPath, exePath, TimeSinceInMs(timeStart));
    return pdfPath;
}

void DeleteStaleCajCache() {
    TempStr dir = CajCacheDirTemp();
    if (len(dir) == 0 || path::GetType(dir) != path::Type::Dir) {
        return;
    }
    DirIter di{dir};
    di.includeFiles = true;
    di.includeDirs = false;
    for (DirIterEntry* de : di) {
        bool isPdf = str::EndsWithI(de->name, StrL(".pdf"));
        bool isTmp = str::EndsWithI(de->name, StrL(".tmp"));
        if (!isPdf && !isTmp) {
            continue;
        }
        i64 ageSec = FileAgeSec(de->accessTime);
        if (ageSec < (isTmp ? kCajTmpMaxAgeSec : kCajCacheMaxAgeSec)) {
            continue;
        }
        bool ok = file::Delete(de->filePath);
        logf("DeleteStaleCajCache: delete '%s' -> %d\n", de->filePath, (int)ok);
    }
}

// a proxy, so that FilePath() stays the user's file. A tab must not save
// annotations into a cached PDF that gets deleted after a week.
class EngineCaj : public EnginePdfProxy {
  public:
    EngineCaj() {
        kind = kindEngineCaj;
        defaultExt = str::Dup(StrL(".caj"));
    }

    ~EngineCaj() override { str::Free(cachedPdf); }

    EngineBase* Clone() override {
        if (len(cachedPdf) == 0 || !file::Exists(cachedPdf)) {
            return nullptr;
        }
        EngineBase* inner = CreateEngineMupdfFromFile(cachedPdf, FileType::PDF, DpiGet(), nullptr);
        if (!inner) {
            return nullptr;
        }
        EngineCaj* clone = new EngineCaj();
        if (FilePath()) {
            clone->SetFilePath(FilePath());
        }
        clone->pdfEngine = inner;
        clone->cachedPdf = str::Dup(cachedPdf);
        clone->CopyStateFromPdfEngine();
        return clone;
    }

    // saving as .pdf writes the conversion; anything else copies the original
    bool SaveFileAs(Str dstPath) override {
        if (str::EndsWithI(dstPath, StrL(".pdf")) && len(cachedPdf) > 0) {
            return file::Copy(dstPath, cachedPdf, false);
        }
        Str srcPath = FilePath();
        return len(srcPath) > 0 && file::Copy(dstPath, srcPath, false);
    }

    Str cachedPdf;

    bool Load(Str fileName, PasswordUI* pwdUI) {
        pageCount = 0;
        if (len(fileName) == 0) {
            return false;
        }
        SetFilePath(fileName);
        TempStr ext = path::GetExtTemp(fileName);
        if (len(ext) > 0) {
            str::Free(defaultExt);
            defaultExt = str::Dup(ext);
        }
        TempStr pdf = EnsureCajPdfTemp(fileName, pwdUI);
        if (len(pdf) == 0) {
            return false;
        }
        cachedPdf = str::Dup(pdf);
        pdfEngine = CreateEngineMupdfFromFile(cachedPdf, FileType::PDF, DpiGet(), nullptr);
        if (!pdfEngine) {
            return false;
        }
        CopyStateFromPdfEngine();
        return true;
    }
};

EngineBase* CreateEngineCajFromFile(Str fileName, PasswordUI* pwdUI) {
    EngineCaj* engine = new EngineCaj();
    if (!engine->Load(fileName, pwdUI)) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return engine;
}

bool IsEngineCajSupportedFileType(FileType kind) {
    return kind == FileType::Caj;
}

#else

EngineBase* CreateEngineCajFromFile(Str, PasswordUI*) {
    return nullptr;
}

bool IsEngineCajSupportedFileType(FileType) {
    return false;
}

void DeleteStaleCajCache() {}

#endif
