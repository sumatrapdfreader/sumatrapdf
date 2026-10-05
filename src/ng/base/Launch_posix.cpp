/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// ng: what Win.cpp's LaunchFileShell / LaunchBrowser /
// OpenPathInDefaultFileManager do with ShellExecuteEx, done with fork + exec.

#include "base/Base.h"
#include "base/File.h"

#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/Launch.h"

extern char** environ;

// the desktop's "open this with whatever handles it" program
static const char* OpenerProgram() {
#if OS_DARWIN
    return "open";
#else
    return "xdg-open";
#endif
}

// one command line into argv, honoring double quotes. Same rules as the
// Windows command line the settings file is written for.
static void SplitCmdLine(Str cmdLine, StrVec& out) {
    int i = 0;
    int n = len(cmdLine);
    str::Builder cur;
    bool inQuote = false;
    bool any = false;
    while (i < n) {
        char c = cmdLine.s[i++];
        if (c == '"') {
            inQuote = !inQuote;
            any = true;
            continue;
        }
        if (c == ' ' && !inQuote) {
            if (any) {
                out.Append(ToStrTemp(cur));
                cur.Reset();
                any = false;
            }
            continue;
        }
        cur.AppendChar(c);
        any = true;
    }
    if (any) {
        out.Append(ToStrTemp(cur));
    }
    cur.Reset();
}

static bool SpawnDetached(const StrVec& args) {
    int n = len(args);
    if (n == 0) {
        return false;
    }
    auto** argv = AllocArrayTemp<char*>(n + 1);
    for (int i = 0; i < n; i++) {
        argv[i] = (char*)CStrTemp(args[i]);
    }
    argv[n] = nullptr;
    pid_t pid = 0;
    int err = posix_spawnp(&pid, argv[0], nullptr, nullptr, argv, environ);
    if (err != 0) {
        logf("SpawnDetached: posix_spawnp('%s') failed with %d\n", args[0], err);
        return false;
    }
    logf("SpawnDetached: launched '%s', pid %d\n", args[0], (int)pid);
    return true;
}

bool LaunchFileShellArgs(const StrVec& args) {
    return SpawnDetached(args);
}

bool LaunchFileShell(Str path, Str params, Str, bool) {
    if (len(path) == 0) {
        return false;
    }
    StrVec args;
    // an executable is run directly, anything else is handed to the desktop
    if (access(CStrTemp(path), X_OK) == 0 && !dir::Exists(path)) {
        args.Append(path);
        if (len(params) > 0) {
            SplitCmdLine(params, args);
        }
    } else {
        args.Append(Str(OpenerProgram()));
        args.Append(path);
    }
    return SpawnDetached(args);
}

bool LaunchBrowser(Str url) {
    StrVec args;
    args.Append(Str(OpenerProgram()));
    args.Append(url);
    return SpawnDetached(args);
}

void OpenPathInDefaultFileManager(Str path) {
    if (len(path) == 0) {
        return;
    }
    if (dir::Exists(path)) {
        StrVec args;
        args.Append(Str(OpenerProgram()));
        args.Append(path);
        SpawnDetached(args);
        return;
    }

    TempStr fullPath = path::NormalizeTemp(path);
    StrVec args;
#if OS_DARWIN
    args.Append(StrL("open"));
    args.Append(StrL("-R"));
    args.Append(fullPath);
#else
    TempStr uri = fmt("file://%s", url::EncodePathTemp(fullPath));
    args.Append(StrL("dbus-send"));
    args.Append(StrL("--session"));
    args.Append(StrL("--dest=org.freedesktop.FileManager1"));
    args.Append(StrL("--type=method_call"));
    args.Append(StrL("/org/freedesktop/FileManager1"));
    args.Append(StrL("org.freedesktop.FileManager1.ShowItems"));
    args.Append(fmt("array:string:%s", uri));
    args.Append(StrL("string:"));
#endif
    if (SpawnDetached(args)) {
        return;
    }

    TempStr dir = path::GetDirTemp(path);
    args.Reset();
    args.Append(Str(OpenerProgram()));
    args.Append(dir);
    SpawnDetached(args);
}
