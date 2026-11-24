/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the mac build updates itself from the .zip of SumatraPDF.app that
// build-ng-mac.ts uploads. The new bundle is unpacked next to ours (same
// volume, so the swap is a rename) and must be signed by the same team.
// The swap runs only after we have exited: moving a bundle out from under
// the process that has it open fails, and a crash between the two renames
// would remove the app. Sparkle does this from a helper it starts first.
// Ours is /bin/sh in its own session. A bad zip still falls back to the .dmg.

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

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/File.h"
#include "base/Crypto.h"

#include "SumatraConfig.h"
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

// CFBundleShortVersionString of an unpacked bundle. Empty if it has none.
static TempStr BundleShortVersionTemp(Str appPath) {
    TempStr plist = path::JoinTemp(appPath, StrL("Contents/Info.plist"));
    NSString* path = [NSString stringWithUTF8String:plist.s];
    NSDictionary* info = [NSDictionary dictionaryWithContentsOfFile:path];
    NSString* ver = info[@"CFBundleShortVersionString"];
    if (![ver isKindOfClass:[NSString class]] || ver.length == 0) {
        return {};
    }
    return str::DupTemp(Str(ver.UTF8String));
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

// true when a relauncher will swap the bundle in after we exit
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
    // Sparkle refuses a bundle older than the one that is running.
    TempStr newVer = BundleShortVersionTemp(newApp);
    if (len(newVer) == 0 || CompareProgramVersion(newVer, currentVersion) < 0) {
        logf("MacInstallUpdateZip: version '%s' is not installable over '%s'\n", newVer, currentVersion);
        RemoveDir(stageDir);
        return false;
    }

    // $1 pid, $2 our bundle, $3 stage dir, $4 unpacked bundle, $5 where the
    // old bundle goes, $6 the zip. /bin/sh is not signed as us. Sparkle skips
    // its atomic swap and gktool scan in that case (a macOS 13+ privacy
    // prompt) and moves the bundle aside instead. Quarantine and mtime are
    // fixed on the staged copy, before that move.
    TempStr pid = fmt("%d", (int)getpid());
    const char* script =
        "app=\"$2\"; stage=\"$3\"; new=\"$4\"; old=\"$5\"; zip=\"$6\"\n"
        "i=0\n"
        "while /bin/kill -0 \"$1\" 2>/dev/null; do\n"
        "  st=$(/bin/ps -o state= -p \"$1\" 2>/dev/null || true)\n"
        "  case \"$st\" in *Z*) break ;; esac\n"
        "  i=$((i + 1))\n"
        "  if [ \"$i\" -gt 300 ]; then\n"
        "    /bin/rm -rf \"$stage\"; /bin/rm -f \"$zip\"; exit 1\n"
        "  fi\n"
        "  /bin/sleep 0.2\n"
        "done\n"
        "if [ -d \"$new\" ]; then\n"
        "  /usr/bin/xattr -dr com.apple.quarantine \"$new\" 2>/dev/null || true\n"
        "  /usr/bin/touch \"$new\" || true\n"
        "fi\n"
        "if ! /bin/mv \"$app\" \"$old\"; then\n"
        "  /bin/rm -rf \"$stage\"; /bin/rm -f \"$zip\"; /usr/bin/open \"$app\"; exit 1\n"
        "fi\n"
        "if ! /bin/mv \"$new\" \"$app\"; then\n"
        "  /bin/mv \"$old\" \"$app\"; /bin/rm -rf \"$stage\"; /bin/rm -f \"$zip\"\n"
        "  /usr/bin/open \"$app\"; exit 1\n"
        "fi\n"
        "/bin/rm -rf \"$stage\"\n"
        "/bin/rm -f \"$zip\"\n"
        "exec /usr/bin/open \"$app\"\n";
    const char* shArgv[] = {"/bin/sh", "-c", script, "sh", pid.s, app.s, stage, newApp.s, oldApp.s, zipPath.s, nullptr};
    // drop inherited handles into the bundle we're about to move, and leave
    // the shell in its own session so exiting doesn't signal it
    posix_spawnattr_t attr;
    posix_spawn_file_actions_t fa;
    posix_spawnattr_init(&attr);
    posix_spawn_file_actions_init(&fa);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSID | POSIX_SPAWN_CLOEXEC_DEFAULT);
    posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&fa, 1, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", O_WRONLY, 0);
    pid_t child = 0;
    int err = posix_spawn(&child, shArgv[0], &fa, &attr, (char* const*)shArgv, environ);
    posix_spawn_file_actions_destroy(&fa);
    posix_spawnattr_destroy(&attr);
    if (err != 0) {
        logf("MacInstallUpdateZip: starting the relauncher failed: %d\n", err);
        RemoveDir(stageDir);
        return false;
    }
    logf("MacInstallUpdateZip: will replace '%s' after exit\n", app);
    return true;
}
