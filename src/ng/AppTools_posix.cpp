/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the POSIX half of AppTools.cpp. App data and PATH-based TeX editor
// detection live here; registry services and window placement remain win32.

#include "base/Base.h"
#include "base/Crypto.h"
#include "base/File.h"

#include <unistd.h>
#if OS_LINUX || OS_DARWIN
#include <sys/xattr.h>
#endif

#include "SumatraConfig.h"
#include "Version.h"
#include "AppTools.h"

static Str gAppDataDir;

// portable mode is "settings next to the executable"; on POSIX the binary
// usually lives in a read-only prefix, so it is off unless the directory is
// writable and already holds a settings file
bool IsRunningInPortableMode() {
    static int sCacheIsPortable = -1;
    if (sCacheIsPortable != -1) {
        return sCacheIsPortable != 0;
    }
    sCacheIsPortable = 0;
#if !OS_WASM
    TempStr dir = GetSelfExeDirTemp();
    TempStr path = path::JoinTemp(dir, StrL("SumatraPDF-settings.txt"));
    if (file::Exists(path) && dir::HasWriteAccess(dir)) {
        sCacheIsPortable = 1;
    }
#endif
    return sCacheIsPortable != 0;
}

void DeleteAppTools() {
    // gAppDataDir is allocated from gPermArena (freed wholesale on exit)
    gAppDataDir = {};
}

void SetAppDataDir(Str dir) {
    dir = path::NormalizeTemp(dir);
    bool ok = dir::CreateAll(dir);
    if (!ok) {
        logf("SetAppDataDir: failed to create directory '%s'\n", dir);
    }
    gAppDataDir = str::Dup(GetPermArena(), dir);
}

// $XDG_CONFIG_HOME/SumatraPDF on Linux, ~/Library/Application Support/SumatraPDF
// on mac, /settings on wasm (OPFS, mirrored in MEMFS)
static TempStr DefaultAppDataDirTemp() {
#if OS_WASM
    return str::DupTemp(StrL("/settings"));
#else
    const char* home = getenv("HOME");
#if OS_DARWIN
    if (home && *home) {
        TempStr dir = path::JoinTemp(Str((char*)home), StrL("Library/Application Support"));
        return path::JoinTemp(dir, StrL(kAppName));
    }
#else
    const char* xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) {
        return path::JoinTemp(Str((char*)xdg), StrL(kAppName));
    }
    if (home && *home) {
        TempStr dir = path::JoinTemp(Str((char*)home), StrL(".config"));
        return path::JoinTemp(dir, StrL(kAppName));
    }
#endif
    const char* tmp = getenv("TMPDIR");
    return path::JoinTemp(Str((char*)(tmp && *tmp ? tmp : "/tmp")), StrL(kAppName));
#endif
}

TempStr GetAppDataDirTemp() {
    if (gAppDataDir) {
        return gAppDataDir;
    }
    TempStr dir;
    if (IsRunningInPortableMode()) {
        dir = GetSelfExeDirTemp();
    } else {
        dir = DefaultAppDataDirTemp();
    }
    logf("GetAppDataDirTemp(): '%s'\n", dir);
    SetAppDataDir(dir);
    return gAppDataDir;
}

// Generate full path for a file or directory for storing data
TempStr GetPathInAppDataDirTemp(Str name) {
    if (len(name) == 0) {
        return {};
    }
    TempStr dir = GetAppDataDirTemp();
    return path::JoinTemp(dir, name);
}

TempStr GetTempDirPathTemp() {
    const char* tmp = getenv("TMPDIR");
    return str::DupTemp(Str((char*)(tmp && *tmp ? tmp : "/tmp")));
}

int CurrentProcessId() {
    return (int)getpid();
}

// resources embedded in the exe are a win32 concept; there is no dll build
// off Windows
bool IsDllBuild() {
    return false;
}

bool IsInstallerOrUninstallerExe() {
    TempStr exeName = path::GetBaseNameTemp(GetSelfExePathTemp());
    return str::ContainsI(exeName, StrL("uninstall")) || str::ContainsI(exeName, StrL("install"));
}

static char gAppSha1[41]{};

Str Sha1OfAppExe() {
    if (gAppSha1[0]) {
        return Str(gAppSha1);
    }
    TempStr appPath = GetSelfExePathTemp();
    Str data = file::ReadFile(appPath);
    if (len(data) == 0) {
        return {};
    }
    u8 sha1[20]{};
    CalcSHA1Digest(data, sha1);
    str::Free(data);
    for (int i = 0; i < dimofi(sha1); i++) {
        snprintf(gAppSha1 + i * 2, 3, "%02x", sha1[i]);
    }
    return Str(gAppSha1);
}

TempStr GetWebViewDataDirTemp() {
    TempStr dir = path::JoinTemp(GetAppDataDirTemp(), StrL("SumatraPDF-data"));
    char id[7] = "000000";
    Str sha1 = Sha1OfAppExe();
    if (sha1) {
        str::BufSet(Str(id, dimof(id)), sha1);
    }
    dir = path::JoinTemp(dir, Str(id));
    return path::JoinTemp(dir, fmt("webview-%d", CurrentProcessId()));
}

bool AdjustVariableDriveLetter(Str&) {
    return false;
}

static bool HasDownloadMetadata(Str path) {
    if (len(path) == 0) {
        return false;
    }
#if OS_DARWIN
    return getxattr(CStrTemp(path), "com.apple.quarantine", nullptr, 0, 0, 0) >= 0;
#elif OS_LINUX
    return getxattr(CStrTemp(path), "user.xdg.origin.url", nullptr, 0) >= 0 ||
           getxattr(CStrTemp(path), "user.xdg.referrer.url", nullptr, 0) >= 0;
#else
    return false;
#endif
}

bool IsUntrustedFile(Str filePath, Str fileUrl) {
    TempStr protocol;
    if (fileUrl && !str::IsNull(str::Parse(fileUrl, "%S:", &protocol)) && len(protocol) > 1 &&
        !str::EqI(protocol, StrL("file"))) {
        return true;
    }
    return HasDownloadMetadata(filePath);
}

struct PosixEditorRule {
    Str binary;
    Str inverseSearchArgs;
};

static PosixEditorRule gEditorRules[] = {
    {StrL("code"), StrL(R"(--goto "%f:%l:%c")")},  {StrL("codium"), StrL(R"(--goto "%f:%l:%c")")},
    {StrL("subl"), StrL(R"("%f:%l:%c")")},         {StrL("texstudio"), StrL(R"("%f" -line %l)")},
    {StrL("texmaker"), StrL(R"("%f" -line %l)")},  {StrL("kate"), StrL(R"(-l %l -c %c "%f")")},
    {StrL("emacsclient"), StrL(R"(+%l:%c "%f")")}, {StrL("gvim"), StrL(R"("%f" +%l)")},
};

static TextEditor gEditors[dimofi(gEditorRules)];
static bool gEditorsFound = false;

static TempStr FindInPathTemp(Str binary, Str pathList) {
    if (len(pathList) == 0) {
        return {};
    }
    while (len(pathList) > 0) {
        int sep = str::IndexOfChar(pathList, ':');
        Str dir = sep < 0 ? pathList : Str(pathList.s, sep);
        if (len(dir) == 0) {
            dir = StrL(".");
        }
        TempStr path = path::JoinTemp(dir, binary);
        if (access(CStrTemp(path), X_OK) == 0 && !dir::Exists(path)) {
            return str::DupTemp(path);
        }
        if (sep < 0) {
            break;
        }
        pathList = Str(pathList.s + sep + 1, pathList.len - sep - 1);
    }
    return {};
}

static void FindTextEditors() {
    if (gEditorsFound) {
        return;
    }
    gEditorsFound = true;
    for (int i = 0; i < dimofi(gEditorRules); i++) {
        PosixEditorRule& rule = gEditorRules[i];
        const char* pathEnv = getenv("PATH");
        TempStr path = FindInPathTemp(rule.binary, Str((char*)pathEnv));
        if (len(path) == 0) {
            continue;
        }
        TextEditor& editor = gEditors[i];
        editor.binaryFilename = rule.binary;
        editor.inverseSearchArgs = rule.inverseSearchArgs;
        editor.fullPath = str::Dup(GetPermArena(), path);
        editor.openFileCmd = str::Dup(GetPermArena(), fmt("\"%s\" %s", path, rule.inverseSearchArgs));
    }
}

void DetectTextEditors(Vec<TextEditor*>& out) {
    FindTextEditors();
    for (TextEditor& editor : gEditors) {
        if (len(editor.openFileCmd) > 0) {
            VecAppend(out, &editor);
        }
    }
}

#if !OS_WASM
bool AppToolsPosix_UnitTests() {
    TempStr shell = FindInPathTemp(StrL("sh"), StrL("/bin:/usr/bin"));
    TempStr absent = FindInPathTemp(StrL("sumatrapdf-ng-no-such-editor"), StrL("/bin:/usr/bin"));
    Str sha1 = Sha1OfAppExe();
    return file::Exists(shell) && len(absent) == 0 && len(sha1) == 40 &&
           str::ContainsChar(StrL("0123456789abcdef"), sha1.s[0]) &&
           IsUntrustedFile({}, StrL("https://example.org/file.pdf")) &&
           !IsUntrustedFile({}, StrL("file:///tmp/file.pdf"));
}
#endif

void CollectInverseSearchCommands(StrVec& out, Str cmdLine) {
    out.Reset();
    if (cmdLine) {
        AppendIfNotExists(&out, cmdLine);
    }
}
