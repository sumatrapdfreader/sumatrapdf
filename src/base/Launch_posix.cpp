/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// ng: what Win.cpp's LaunchFileShell / LaunchBrowser /
// OpenPathInDefaultFileManager do with ShellExecuteEx, done with fork + exec.

#include "base/Base.h"
#include "base/File.h"

#include <spawn.h>
#include <sys/stat.h>
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

// 0 when the program couldn't be started
static pid_t SpawnArgs(const StrVec& args) {
    int n = len(args);
    if (n == 0) {
        return 0;
    }
    auto** argv = AllocArrayTemp<char*>(n + 1);
    for (int i = 0; i < n; i++) {
        argv[i] = (char*)CStrTemp(args[i]);
    }
    argv[n] = nullptr;
    pid_t pid = 0;
    int err = posix_spawnp(&pid, argv[0], nullptr, nullptr, argv, environ);
    if (err != 0) {
        logf("SpawnArgs: posix_spawnp('%s') failed with %d\n", args[0], err);
        return 0;
    }
    logf("SpawnArgs: launched '%s', pid %d\n", args[0], (int)pid);
    return pid;
}

static bool SpawnDetached(const StrVec& args) {
    return SpawnArgs(args) != 0;
}

struct ChildProcess {
    pid_t pid = 0;
    bool exited = false;
};

ChildProcess* StartChildProcess(Str cmdLine) {
    StrVec args;
    SplitCmdLine(cmdLine, args);
    pid_t pid = SpawnArgs(args);
    if (pid == 0) {
        return nullptr;
    }
    auto* p = new ChildProcess();
    p->pid = pid;
    return p;
}

bool gBlockExternalLaunch = false;
int gExternalLaunchesBlocked = 0;

bool ExternalLaunchBlocked(Str what) {
    if (!gBlockExternalLaunch) {
        return false;
    }
    logf("ExternalLaunchBlocked: not opening '%s' in test mode\n", what);
    gExternalLaunchesBlocked++;
    return true;
}

bool LaunchFileShellArgs(const StrVec& args) {
    if (len(args) > 0 && ExternalLaunchBlocked(args[0])) {
        return false;
    }
    return SpawnDetached(args);
}

// even an executable is handed to the desktop: documents on vfat / smb mounts
// or from archives often have the exec bit. Programs go through LaunchProgram
static void ShellOpenArgs(Str path, StrVec& args) {
    args.Append(Str(OpenerProgram()));
    args.Append(path);
}

bool LaunchFileShell(Str path, Str, Str, bool) {
    if (len(path) == 0 || ExternalLaunchBlocked(path)) {
        return false;
    }
    StrVec args;
    ShellOpenArgs(path, args);
    return SpawnDetached(args);
}

// argv for exe + argsTemplate. The template is split before expand substitutes
// into each argument, so substituted text (a file name with ") can't add arguments
void ProgramArgs(Str exe, Str argsTemplate, const Func1<Str*>& expand, StrVec& args) {
    args.Append(exe);
    StrVec templ;
    SplitCmdLine(argsTemplate, templ);
    for (Str arg : templ) {
        expand.Call(&arg);
        args.Append(arg);
    }
}

bool LaunchProgram(Str exe, Str cmdLine) {
    StrVec args;
    ProgramArgs(exe, cmdLine, {}, args);
    return LaunchFileShellArgs(args);
}

bool LaunchBrowser(Str url) {
    if (ExternalLaunchBlocked(url)) {
        return false;
    }
    StrVec args;
    args.Append(Str(OpenerProgram()));
    args.Append(url);
    return SpawnDetached(args);
}

void OpenPathInDefaultFileManager(Str path) {
    if (len(path) == 0 || ExternalLaunchBlocked(path)) {
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

#if IS_DEBUG
static void ExpandDocPath(Str* docPath, Str* arg) {
    *arg = str::ReplaceTemp(*arg, StrL("%1"), *docPath);
}

bool Launch_UnitTests() {
    // a document with the exec bit (vfat / smb mounts, archives) is opened, not run
    TempStr doc = GetTempFilePathTemp(StrL("launch-ut"));
    bool ok = file::WriteFile(doc, StrL("#!/bin/sh\n")) && chmod(CStrTemp(doc), 0755) == 0;
    StrVec args;
    ShellOpenArgs(doc, args);
    ok = ok && len(args) == 2 && str::Eq(args[0], Str(OpenerProgram())) && str::Eq(args[1], doc);
    file::Delete(doc);

    // a quote in the document path doesn't add arguments
    Str docPath = StrL("/tmp/a\" --attach \"/etc/passwd.pdf");
    args.Reset();
    ProgramArgs(StrL("xdg-email"), StrL("--attach \"%1\" -x"), MkFunc1(ExpandDocPath, &docPath), args);
    ok = ok && len(args) == 4 && str::Eq(args[2], docPath) && str::Eq(args[3], StrL("-x"));
    return ok;
}
#endif
