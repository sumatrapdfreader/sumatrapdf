/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the mac build updates itself from the .zip of SumatraPDF.app that
// build-ng-mac.ts uploads. The new bundle is unpacked next to ours (same
// volume, so swapping them is two renames), must be signed by the same team
// as we are, and replaces us while we run. A shell started before we exit
// waits for us to go away and opens the new bundle.

#include "base/Base.h"

// ng: objc's BOOL is bool (or signed char); ours is int. Rename theirs for
// the import. Point / Rect / Size are handled by MacTypesHide.h.
#include "base/MacTypesHide.h"
#define BOOL MacObjcBool
#import <Foundation/Foundation.h>
#include <IOKit/IOKitLib.h>
#include <Security/Security.h>
#undef BOOL
#include "base/MacTypesShow.h"

#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/File.h"
#include "base/Crypto.h"

#include "UpdateCheck.h"
#include "UpdateCheckCommon.h"

#include "SumatraLog.h"

extern char** environ;

// the build zips the bundle with ditto --keepParent
static const Str kZippedAppName = StrL("SumatraPDF.app");

static TempStr CFStringTemp(CFStringRef s) {
    if (!s) {
        return {};
    }
    char buf[256];
    if (!CFStringGetCString(s, buf, sizeof(buf), kCFStringEncodingUTF8)) {
        return {};
    }
    return str::DupTemp(Str(buf));
}

// Hardware UUID, as in System Information
static TempStr PlatformUuidTemp() {
    io_service_t svc = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
    if (!svc) {
        return {};
    }
    CFTypeRef v = IORegistryEntryCreateCFProperty(svc, CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0);
    IOObjectRelease(svc);
    if (!v) {
        return {};
    }
    TempStr res{};
    if (CFGetTypeID(v) == CFStringGetTypeID()) {
        res = CFStringTemp((CFStringRef)v);
    }
    CFRelease(v);
    return res;
}

// stable per machine; hashed (and salted, so it doesn't match other apps'
// ids) so the hardware UUID never leaves the machine
TempStr MacMachineIdHashTemp() {
    TempStr id = PlatformUuidTemp();
    if (len(id) == 0) {
        return StrL("unknown");
    }
    u8 digest[32];
    CalcSHA2Digest(str::JoinTemp(StrL("SumatraPDF:"), id), digest);
    return str::MemToHexTemp(Str((char*)digest, 16));
}

TempStr MacUpdateZipPathTemp() {
    NSString* dir = NSTemporaryDirectory();
    if (!dir) {
        return {};
    }
    return path::JoinTemp(Str([dir UTF8String]), fmt("sumatra-update-%d.zip", (int)getpid()));
}

static TempStr SelfAppBundleTemp() {
    NSString* p = [[NSBundle mainBundle] bundlePath];
    if (!p || ![p hasSuffix:@".app"]) {
        return {};
    }
    return str::DupTemp(Str([p UTF8String]));
}

// "" if we're ad-hoc signed (a developer build)
static TempStr SelfTeamIdTemp() {
    SecCodeRef code = nullptr;
    if (SecCodeCopySelf(kSecCSDefaultFlags, &code) != errSecSuccess) {
        return {};
    }
    SecStaticCodeRef staticCode = nullptr;
    OSStatus st = SecCodeCopyStaticCode(code, kSecCSDefaultFlags, &staticCode);
    CFRelease(code);
    if (st != errSecSuccess) {
        return {};
    }
    CFDictionaryRef info = nullptr;
    st = SecCodeCopySigningInformation(staticCode, kSecCSSigningInformation, &info);
    CFRelease(staticCode);
    if (st != errSecSuccess || !info) {
        return {};
    }
    TempStr res = CFStringTemp((CFStringRef)CFDictionaryGetValue(info, kSecCodeInfoTeamIdentifier));
    CFRelease(info);
    return res;
}

// a valid signature of the whole bundle by teamId, with our bundle id
static bool IsSignedByTeam(Str appPath, Str teamId) {
    NSString* bundleId = [[NSBundle mainBundle] bundleIdentifier];
    if (!bundleId) {
        return false;
    }
    TempStr reqStr = fmt("identifier \"%s\" and anchor apple generic and certificate leaf[subject.OU] = \"%s\"",
                         Str([bundleId UTF8String]), teamId);
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, (const UInt8*)appPath.s, appPath.len, true);
    CFStringRef reqCf = CFStringCreateWithCString(nullptr, reqStr.s, kCFStringEncodingUTF8);
    SecStaticCodeRef code = nullptr;
    SecRequirementRef req = nullptr;
    OSStatus st = errSecParam;
    if (url && reqCf && SecStaticCodeCreateWithPath(url, kSecCSDefaultFlags, &code) == errSecSuccess &&
        SecRequirementCreateWithString(reqCf, kSecCSDefaultFlags, &req) == errSecSuccess) {
        SecCSFlags flags = kSecCSCheckAllArchitectures | kSecCSStrictValidate | kSecCSCheckNestedCode;
        st = SecStaticCodeCheckValidity(code, flags, req);
    }
    if (req) {
        CFRelease(req);
    }
    if (code) {
        CFRelease(code);
    }
    if (reqCf) {
        CFRelease(reqCf);
    }
    if (url) {
        CFRelease(url);
    }
    if (st != errSecSuccess) {
        logf("IsSignedByTeam: '%s' failed the check '%s': %d\n", appPath, reqStr, (int)st);
    }
    return st == errSecSuccess;
}

static bool RunAndWait(const char** argv) {
    pid_t pid = 0;
    if (posix_spawn(&pid, argv[0], nullptr, nullptr, (char* const*)argv, environ) != 0) {
        logf("RunAndWait: posix_spawn of '%s' failed\n", Str(argv[0]));
        return false;
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            return false;
        }
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static void RemoveDir(Str dir) {
    const char* argv[] = {"/bin/rm", "-rf", dir.s, nullptr};
    RunAndWait(argv);
}

// a bundle we can replace: not run from a disk image or another read-only
// place, nor translocated (macOS runs a downloaded app that wasn't moved out
// of Downloads from a random read-only path), and signed by a team
bool MacCanUpdateSelf() {
    TempStr app = SelfAppBundleTemp();
    if (len(app) == 0) {
        logf("MacCanUpdateSelf: not in an .app bundle\n");
        return false;
    }
    if (str::Contains(app, StrL("/AppTranslocation/"))) {
        logf("MacCanUpdateSelf: '%s' is translocated\n", app);
        return false;
    }
    TempStr parent = path::GetDirTemp(app);
    if (access(parent.s, W_OK) != 0 || access(app.s, W_OK) != 0) {
        logf("MacCanUpdateSelf: can't write '%s'\n", app);
        return false;
    }
    if (len(SelfTeamIdTemp()) == 0) {
        logf("MacCanUpdateSelf: not signed by a team\n");
        return false;
    }
    return true;
}

// true when the new bundle replaced ours and will be opened once we exit
bool MacInstallUpdateZip(Str zipPath) {
    if (!MacCanUpdateSelf()) {
        return false;
    }
    TempStr app = SelfAppBundleTemp();
    TempStr teamId = SelfTeamIdTemp();
    TempStr parent = path::GetDirTemp(app);

    char stage[PATH_MAX];
    snprintf(stage, sizeof(stage), "%s/.SumatraPDF-update-XXXXXX", parent.s);
    if (!mkdtemp(stage)) {
        logf("MacInstallUpdateZip: mkdtemp('%s') failed\n", Str(stage));
        return false;
    }
    Str stageDir(stage);
    const char* dittoArgv[] = {"/usr/bin/ditto", "-x", "-k", zipPath.s, stage, nullptr};
    TempStr newApp = path::JoinTemp(stageDir, kZippedAppName);
    TempStr oldApp = path::JoinTemp(stageDir, StrL("old.app"));
    if (!RunAndWait(dittoArgv) || !dir::Exists(newApp)) {
        logf("MacInstallUpdateZip: unpacking '%s' failed\n", zipPath);
        RemoveDir(stageDir);
        return false;
    }
    if (!IsSignedByTeam(newApp, teamId)) {
        RemoveDir(stageDir);
        return false;
    }
    if (rename(app.s, oldApp.s) != 0) {
        logf("MacInstallUpdateZip: moving '%s' away failed: %d\n", app, errno);
        RemoveDir(stageDir);
        return false;
    }
    if (rename(newApp.s, app.s) != 0) {
        logf("MacInstallUpdateZip: moving the new bundle to '%s' failed: %d\n", app, errno);
        rename(oldApp.s, app.s);
        RemoveDir(stageDir);
        return false;
    }
    file::Delete(zipPath);

    // in its own session so it outlives us
    TempStr pid = fmt("%d", (int)getpid());
    const char* script =
        "while kill -0 \"$1\" 2>/dev/null; do sleep 0.2; done; /bin/rm -rf \"$2\"; /usr/bin/open \"$3\"";
    const char* shArgv[] = {"/bin/sh", "-c", script, "sh", pid.s, stage, app.s, nullptr};
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSID);
    pid_t child = 0;
    int err = posix_spawn(&child, shArgv[0], nullptr, &attr, (char* const*)shArgv, environ);
    posix_spawnattr_destroy(&attr);
    if (err != 0) {
        // the new version is in place; it runs next time the user starts us
        logf("MacInstallUpdateZip: starting the relauncher failed: %d\n", err);
    }
    logf("MacInstallUpdateZip: replaced '%s'\n", app);
    return true;
}
