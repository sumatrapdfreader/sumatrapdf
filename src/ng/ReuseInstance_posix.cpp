/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the running instance listens on a unix domain socket in a directory only
// the user can enter. A later process with -reuse-instance, -dde or the
// ReuseInstance setting connects, sends the DDE commands orig sends with
// WM_COPYDATA ([Open(...)][Search(...)] etc.) and exits. A message is a
// little-endian u32 length and that many bytes of UTF-8; the instance answers
// with one byte once the commands are queued for its UI thread.
//
// The socket's name hashes the exe path and the app data directory, as orig's
// mapping does, so side-by-side installs don't talk to each other.

#include "base/Base.h"

#if !OS_WASM

#include <atomic>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "base/File.h"
#include "base/UITask.h"

#include "Settings.h"
#include "Flags.h"
#include "AppTools.h"
#include "AppSettings.h"
#include "SumatraConfig.h"
#include "ReuseInstance.h"

#include "SumatraLog.h"

constexpr u32 kMaxInstanceMsg = 1024 * 1024;
constexpr int kInstanceIoTimeoutMs = 5000;

static std::atomic<bool> gInstanceServerStop{false};
static std::atomic<bool> gInstanceServerRunning{false};

// macOS gives each user a private $TMPDIR, Linux $XDG_RUNTIME_DIR. Elsewhere
// /tmp/sumatrapdf-<uid>, which must be ours, not a symlink and closed to others.
static TempStr PrivateDirTemp() {
#if OS_DARWIN
    const char* dir = getenv("TMPDIR");
#else
    const char* dir = getenv("XDG_RUNTIME_DIR");
#endif
    if (dir && *dir) {
        TempStr res = str::DupTemp(Str(dir));
        while (len(res) > 1 && str::EndsWith(res, StrL("/"))) {
            res = Str(res.s, len(res) - 1);
        }
        return res;
    }
    TempStr res = fmt("/tmp/sumatrapdf-%d", (int)getuid());
    mkdir(res.s, 0700);
    struct stat st;
    if (lstat(res.s, &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != getuid() || (st.st_mode & 077) != 0) {
        logf("PrivateDirTemp: '%s' isn't a private directory\n", res);
        return {};
    }
    return res;
}

static TempStr InstanceSocketPathTemp() {
    TempStr dir = PrivateDirTemp();
    if (len(dir) == 0) {
        return {};
    }
    TempStr id = str::JoinTemp(GetSelfExePathTemp(), StrL("|"), GetAppDataDirTemp());
    return fmt("%s/SumatraPDF-%08x.sock", dir, MurmurHash2(id));
}

static bool SocketAddr(Str path, sockaddr_un* addr) {
    *addr = {};
    addr->sun_family = AF_UNIX;
    if (len(path) == 0 || len(path) >= (int)sizeof(addr->sun_path)) {
        return false;
    }
    memcpy(addr->sun_path, path.s, (size_t)len(path));
    return true;
}

static bool WaitFd(int fd, short events) {
    pollfd p{fd, events, 0};
    for (;;) {
        int r = poll(&p, 1, kInstanceIoTimeoutMs);
        if (r > 0) {
            return true;
        }
        if (r < 0 && errno == EINTR) {
            continue;
        }
        return false;
    }
}

static bool ReadAll(int fd, void* buf, size_t n) {
    u8* p = (u8*)buf;
    while (n > 0) {
        if (!WaitFd(fd, POLLIN)) {
            return false;
        }
        ssize_t r = read(fd, p, n);
        if (r < 0 && errno == EINTR) {
            continue;
        }
        if (r <= 0) {
            return false;
        }
        p += r;
        n -= (size_t)r;
    }
    return true;
}

static bool WriteAll(int fd, const void* buf, size_t n) {
    const u8* p = (const u8*)buf;
    while (n > 0) {
        if (!WaitFd(fd, POLLOUT)) {
            return false;
        }
        ssize_t r = send(fd, p, n, 0);
        if (r < 0 && errno == EINTR) {
            continue;
        }
        if (r <= 0) {
            return false;
        }
        p += r;
        n -= (size_t)r;
    }
    return true;
}

static void NoSigPipe(int fd) {
#if OS_DARWIN
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#else
    (void)fd;
#endif
}

// -1 when no instance listens there
static int ConnectToInstance(Str path) {
    sockaddr_un addr;
    if (!SocketAddr(path, &addr)) {
        return -1;
    }
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    NoSigPipe(fd);
    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static bool SendInstanceMsg(int fd, Str cmd) {
    u32 n = (u32)len(cmd);
    u8 hdr[4] = {(u8)n, (u8)(n >> 8), (u8)(n >> 16), (u8)(n >> 24)};
    u8 ack = 0;
    return WriteAll(fd, hdr, sizeof(hdr)) && WriteAll(fd, cmd.s, (size_t)n) && ReadAll(fd, &ack, 1) && ack == 1;
}

// one connection per message: the instance closes it after answering
static bool SendToInstance(Str path, Str cmd) {
    int fd = ConnectToInstance(path);
    if (fd < 0) {
        return false;
    }
    bool ok = SendInstanceMsg(fd, cmd);
    close(fd);
    logf("SendToInstance: '%s' ok=%d\n", cmd, (int)ok);
    return ok;
}

// orig's ForwardToExistingInstance, without the virtual desktop and the
// elevation handling it needs on Windows
bool ForwardToRunningInstance(Flags& flags, bool* otherInstanceRunning) {
    TempStr path = InstanceSocketPathTemp();
    int probe = ConnectToInstance(path);
    *otherInstanceRunning = probe >= 0;
    if (probe >= 0) {
        close(probe);
    }
    if (flags.printDialog || flags.configureRestrictedUse || gForTesting) {
        // the print dialog and the test runs always use this process
        return false;
    }
    if (flags.dde) {
        SendToInstance(path, flags.dde);
        return true;
    }
    bool reuse = flags.reuseDdeInstance || gSettings->reuseInstance;
    if (!reuse || !*otherInstanceRunning) {
        return false;
    }
    int nFiles = len(flags.fileNames);
    if (nFiles == 0) {
        // issue #2306: -new-window opens a new window even with no files
        if (flags.inNewWindow || flags.inNewWindowTabs) {
            return false;
        }
        return SendToInstance(path, StrL("[NewWindow]"));
    }
    // -new-window: each file in its own window. -new-window-tabs: one new
    // window, the other files as its tabs (issue #5044)
    bool newWindowEach = flags.inNewWindow && !flags.inNewWindowTabs;
    for (int n = 0; n < nFiles; n++) {
        bool newWindow = newWindowEach || (flags.inNewWindowTabs && n == 0);
        TempStr fullPath = path::NormalizeTemp(flags.fileNames[n]);
        TempStr cmd = OpenDdeCmdTemp(fullPath, flags, n == 0, newWindow ? 2 : 0);
        if (!SendToInstance(path, cmd)) {
            // it went away: open what's left here
            for (int i = 0; i < n; i++) {
                flags.fileNames.RemoveAt(0);
            }
            return false;
        }
    }
    return true;
}

// SearchAndDDE.cpp (its header needs most of the app)
bool ExecuteDdeCmds(Str cmd);

struct InstanceCmd {
    Str cmd;
};

static void ExecuteInstanceCmd(InstanceCmd* d) {
    logf("ExecuteInstanceCmd: '%s'\n", d->cmd);
    ExecuteDdeCmds(d->cmd);
    str::Free(d->cmd);
    delete d;
}

// the socket's directory keeps others out; this also refuses another user
// who could connect anyway (root, a shared $TMPDIR)
static bool IsSameUser(int fd) {
#if OS_DARWIN
    uid_t uid = 0;
    gid_t gid = 0;
    if (getpeereid(fd, &uid, &gid) != 0) {
        return false;
    }
    return uid == getuid();
#else
    ucred cred{};
    socklen_t n = sizeof(cred);
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &n) != 0) {
        return false;
    }
    return cred.uid == getuid();
#endif
}

static void HandleInstanceConn(int fd) {
    NoSigPipe(fd);
    if (!IsSameUser(fd)) {
        logf("HandleInstanceConn: refusing another user\n");
        return;
    }
    u8 hdr[4];
    if (!ReadAll(fd, hdr, sizeof(hdr))) {
        return;
    }
    u32 n = (u32)hdr[0] | ((u32)hdr[1] << 8) | ((u32)hdr[2] << 16) | ((u32)hdr[3] << 24);
    if (n == 0 || n > kMaxInstanceMsg) {
        return;
    }
    char* buf = AllocArray<char>(n + 1);
    if (!ReadAll(fd, buf, n)) {
        free(buf);
        return;
    }
    u8 ack = 0;
    if (!gInstanceServerStop) {
        auto* d = new InstanceCmd{Str(buf, (int)n)};
        if (uitask::Post(MkFunc0<InstanceCmd>(ExecuteInstanceCmd, d), "InstanceCmd")) {
            ack = 1;
        } else {
            delete d;
        }
    }
    if (!ack) {
        free(buf);
    }
    WriteAll(fd, &ack, 1);
}

static ino_t SocketFileIno(const char* path) {
    struct stat st;
    if (lstat(path, &st) != 0 || !S_ISSOCK(st.st_mode)) {
        return 0;
    }
    return st.st_ino;
}

// -1 while another instance answers at the path, or when binding fails
static int TryListen(const sockaddr_un& addr, ino_t* ino) {
    int other = ConnectToInstance(Str(addr.sun_path));
    if (other >= 0) {
        close(other);
        return -1;
    }
    int listener = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listener < 0) {
        return -1;
    }
    // left behind by an instance that crashed or is exiting
    unlink(addr.sun_path);
    if (bind(listener, (sockaddr*)&addr, sizeof(addr)) != 0 || listen(listener, 8) != 0) {
        close(listener);
        return -1;
    }
    chmod(addr.sun_path, 0600);
    *ino = SocketFileIno(addr.sun_path);
    logf("InstanceServer: listening on '%s'\n", Str(addr.sun_path));
    return listener;
}

// An instance that started while another was still exiting takes over once
// that one is gone, and one whose socket file another process removed or
// replaced makes a new one: there's always someone to forward to.
static void InstanceServerThread(sockaddr_un* addr) {
    TempStr lockPath = str::JoinTemp(Str(addr->sun_path), StrL(".lock"));
    int ownership = open(CStrTemp(lockPath), O_CREAT | O_CLOEXEC | O_NOFOLLOW | O_RDWR, 0600);
    if (ownership < 0) {
        delete addr;
        gInstanceServerRunning = false;
        return;
    }
    int listener = -1;
    ino_t ino = 0;
    while (!gInstanceServerStop) {
        if (listener < 0) {
            if (flock(ownership, LOCK_EX | LOCK_NB) != 0) {
                SleepInMs(250);
                continue;
            }
            listener = TryListen(*addr, &ino);
            if (listener < 0) {
                flock(ownership, LOCK_UN);
                SleepInMs(250);
                continue;
            }
        }
        pollfd p{listener, POLLIN, 0};
        int r = poll(&p, 1, 250);
        if (r == 0) {
            if (ino == 0 || SocketFileIno(addr->sun_path) != ino) {
                close(listener);
                listener = -1;
                flock(ownership, LOCK_UN);
            }
            continue;
        }
        if (r < 0) {
            continue;
        }
        int conn = accept(listener, nullptr, nullptr);
        if (conn < 0) {
            continue;
        }
        HandleInstanceConn(conn);
        close(conn);
    }
    if (listener >= 0) {
        close(listener);
        // not the file of an instance that took over meanwhile
        if (ino != 0 && SocketFileIno(addr->sun_path) == ino) {
            unlink(addr->sun_path);
        }
    }
    // Keep the lock file so every process contends on the same inode.
    close(ownership);
    delete addr;
    gInstanceServerRunning = false;
}

void StartInstanceServer() {
    TempStr path = InstanceSocketPathTemp();
    auto* addr = new sockaddr_un;
    if (!SocketAddr(path, addr)) {
        logf("StartInstanceServer: no socket path ('%s')\n", path);
        delete addr;
        return;
    }
    gInstanceServerStop = false;
    gInstanceServerRunning = true;
    RunAsync(MkFunc0(InstanceServerThread, addr), StrL("InstanceServer"));
}

void StopInstanceServer() {
    if (!gInstanceServerRunning) {
        return;
    }
    gInstanceServerStop = true;
    // the thread notices within a poll interval, closes the socket and
    // removes its file
    for (int i = 0; i < 100 && gInstanceServerRunning; i++) {
        SleepInMs(10);
    }
}

#endif // !OS_WASM
