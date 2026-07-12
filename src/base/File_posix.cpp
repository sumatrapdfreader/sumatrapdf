/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "base/File.h"

static char* PathZTemp(Str path) {
    return CStrTemp(path);
}

static bool StatPath(Str path, struct stat& st) {
    if (!path) {
        return false;
    }
    return stat(PathZTemp(path), &st) == 0;
}

static FILETIME FileTimeFromTimespec(time_t sec, long nsec) {
    u64 t = (u64)sec * 1000000000ULL + (u64)nsec;
    FILETIME ft;
    ft.dwLowDateTime = (DWORD)t;
    ft.dwHighDateTime = (DWORD)(t >> 32);
    return ft;
}

static u64 FileTimeToNs(FILETIME ft) {
    return ((u64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

#if defined(__APPLE__)
static timespec StatAccessTime(const struct stat& st) {
    return st.st_atimespec;
}

static timespec StatModificationTime(const struct stat& st) {
    return st.st_mtimespec;
}
#else
static timespec StatAccessTime(const struct stat& st) {
    return st.st_atim;
}

static timespec StatModificationTime(const struct stat& st) {
    return st.st_mtim;
}
#endif

static FILETIME FileTimeFromTimespec(timespec ts) {
    return FileTimeFromTimespec(ts.tv_sec, ts.tv_nsec);
}

static timespec TimespecFromFileTime(FILETIME ft) {
    u64 ns = FileTimeToNs(ft);
    timespec ts;
    ts.tv_sec = (time_t)(ns / 1000000000ULL);
    ts.tv_nsec = (long)(ns % 1000000000ULL);
    return ts;
}

namespace path {

Type GetType(Str path) {
    struct stat st;
    if (!StatPath(path, st)) {
        return Type::None;
    }
    if (S_ISDIR(st.st_mode)) {
        return Type::Dir;
    }
    return Type::File;
}

bool IsDirectory(Str path) {
    struct stat st;
    return StatPath(path, st) && S_ISDIR(st.st_mode);
}

TempStr NormalizeTemp(Str path) {
    char resolved[PATH_MAX];
    if (realpath(PathZTemp(path), resolved)) {
        return str::DupTemp(resolved);
    }
    if (IsAbsolute(path)) {
        return str::DupTemp(path);
    }
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) {
        return str::DupTemp(path);
    }
    return path::JoinTemp(Str(cwd), path);
}

TempStr ShortPathTemp(Str path) {
    return NormalizeTemp(path);
}

bool IsSame(Str path1, Str path2) {
    if (str::IsNull(path1) || str::IsNull(path2)) {
        return false;
    }

    struct stat st1;
    struct stat st2;
    if (StatPath(path1, st1) && StatPath(path2, st2)) {
        return st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino;
    }

    TempStr npath1 = NormalizeTemp(path1);
    TempStr npath2 = NormalizeTemp(path2);
    return npath1 && str::Eq(npath1, npath2);
}

bool HasVariableDriveLetter(Str) {
    return false;
}

bool IsOnNetworkDrive(Str) {
    return false;
}

bool IsCloudPlaceholder(Str) {
    return false;
}

bool IsOnFixedDrive(Str) {
    return true;
}

bool SupportsChangeNotifications(Str) {
    return false;
}

bool IsAbsolute(Str path) {
    return path && IsSep(path.s[0]);
}

TempStr GetNonVirtualTemp(Str virtualPath) {
    return virtualPath;
}

} // namespace path

TempStr GetTempFilePathTemp(Str filePrefix) {
    const char* tmpDir = getenv("TMPDIR");
    if (!tmpDir || !tmpDir[0]) {
        tmpDir = "/tmp";
    }
    if (!filePrefix) {
        return str::DupTemp(tmpDir);
    }

    TempStr name = fmt("%sXXXXXX", filePrefix);
    TempStr path = path::JoinTemp(Str(tmpDir), name);
    char* pathZ = CStrTemp(path);
    int fd = mkstemp(pathZ);
    if (fd < 0) {
        return {};
    }
    close(fd);
    return Str(pathZ);
}

TempStr GetPathInExeDirTemp(Str fileName) {
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) {
        return fileName;
    }
    return path::NormalizeTemp(path::JoinTemp(Str(cwd), fileName));
}

namespace file {

FILE* OpenFILE(Str path) {
    ReportIf(!path);
    if (!path) {
        return nullptr;
    }
    return fopen(PathZTemp(path), "rb");
}

FileHandle OpenReadOnly(Str path) {
    return open(PathZTemp(path), O_RDONLY);
}

bool Exists(Str path) {
    struct stat st;
    return StatPath(path, st) && S_ISREG(st.st_mode);
}

i64 GetSize(FileHandle h) {
    if (h == kInvalidFileHandle) {
        return -1;
    }
    struct stat st;
    if (fstat(h, &st) != 0 || S_ISDIR(st.st_mode)) {
        return -1;
    }
    return (i64)st.st_size;
}

i64 GetSize(Str path) {
    struct stat st;
    if (!StatPath(path, st) || S_ISDIR(st.st_mode)) {
        return -1;
    }
    return (i64)st.st_size;
}

// Maps the whole file at path into memory as a read-only view backed by the
// OS page cache. Unlike ReadFile() this doesn't allocate private memory for
// the file content: pages are faulted in from disk on first access and can
// be discarded by the OS under memory pressure. Caveat: if the backing file
// becomes unreadable while mapped (e.g. a network mount disconnects) or is
// truncated by another process, touching a mapped page raises SIGBUS instead
// of returning an error, so avoid mapping files on unreliable media.
bool MemoryMap(Str path, Mapping* res) {
    int fd = open(PathZTemp(path), O_RDONLY);
    if (fd < 0) {
        return false;
    }
    i64 size = GetSize(fd);
    if (size <= 0) {
        close(fd);
        return false;
    }
    void* data = mmap(nullptr, (size_t)size, PROT_READ, MAP_PRIVATE, fd, 0);
    // the mapping stays valid after the fd is closed
    close(fd);
    if (data == MAP_FAILED) {
        return false;
    }
    res->data = (u8*)data;
    res->size = size;
    return true;
}

void MemoryUnmap(Mapping* m) {
    if (m->data) {
        munmap(m->data, (size_t)m->size);
    }
    *m = {};
}

bool WriteFile(Str path, Str d) {
    int fd = open(PathZTemp(path), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        return false;
    }
    AutoCall closeFile(close, fd);

    const char* data = d.s;
    size_t left = (size_t)d.len;
    while (left > 0) {
        ssize_t n = write(fd, data, left);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        data += n;
        left -= (size_t)n;
    }
    return true;
}

bool Delete(Str path) {
    if (!path) {
        return false;
    }
    if (unlink(PathZTemp(path)) == 0) {
        return true;
    }
    return errno == ENOENT;
}

bool DeleteFileToTrash(Str path) {
    return Delete(path);
}

bool Copy(Str dst, Str src, bool dontOverwrite) {
    return Copy(dst, src, dontOverwrite, {});
}

bool Copy(Str dst, Str src, bool dontOverwrite, const CopyProgressCb& cbProgress) {
    int srcFd = open(PathZTemp(src), O_RDONLY);
    if (srcFd < 0) {
        return false;
    }
    AutoCall closeSrc(close, srcFd);

    int flags = O_WRONLY | O_CREAT | O_TRUNC;
    if (dontOverwrite) {
        flags |= O_EXCL;
    }
    int dstFd = open(PathZTemp(dst), flags, 0666);
    if (dstFd < 0) {
        return false;
    }
    AutoCall closeDst(close, dstFd);

    i64 total = GetSize(src);
    i64 copied = 0;
    u8 buf[64 * 1024];
    for (;;) {
        ssize_t nRead = read(srcFd, buf, sizeof(buf));
        if (nRead < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (nRead == 0) {
            return true;
        }

        u8* p = buf;
        ssize_t left = nRead;
        while (left > 0) {
            ssize_t nWritten = write(dstFd, p, (size_t)left);
            if (nWritten < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return false;
            }
            p += nWritten;
            left -= nWritten;
        }

        copied += nRead;
        if (!cbProgress.IsEmpty()) {
            CopyProgress progress{copied, total < 0 ? 0 : total};
            cbProgress.Call(&progress);
        }
    }
}

FILETIME GetAccessTime(Str path) {
    struct stat st;
    if (!StatPath(path, st)) {
        return {};
    }
    return FileTimeFromTimespec(StatAccessTime(st));
}

bool SetAccessTime(Str path, FILETIME accessTime) {
    struct stat st;
    if (!StatPath(path, st)) {
        return false;
    }
    timespec ts[2];
    ts[0] = TimespecFromFileTime(accessTime);
    ts[1] = StatModificationTime(st);
    return utimensat(AT_FDCWD, PathZTemp(path), ts, 0) == 0;
}

FILETIME GetModificationTime(Str path) {
    struct stat st;
    if (!StatPath(path, st)) {
        return {};
    }
    return FileTimeFromTimespec(StatModificationTime(st));
}

bool SetModificationTime(Str path, FILETIME lastMod) {
    struct stat st;
    if (!StatPath(path, st)) {
        return false;
    }
    timespec ts[2];
    ts[0] = StatAccessTime(st);
    ts[1] = TimespecFromFileTime(lastMod);
    return utimensat(AT_FDCWD, PathZTemp(path), ts, 0) == 0;
}

DWORD GetAttributes(Str path) {
    struct stat st;
    if (!StatPath(path, st)) {
        return (DWORD)-1;
    }
    return (DWORD)st.st_mode;
}

bool SetAttributes(Str path, DWORD attrs) {
    return chmod(PathZTemp(path), (mode_t)(attrs & 07777)) == 0;
}

int GetZoneIdentifier(Str) {
    return URLZONE_INVALID;
}

bool SetZoneIdentifier(Str, int) {
    return true;
}

bool DeleteZoneIdentifier(Str) {
    return true;
}

bool Rename(Str newPath, Str oldPath) {
    if (!newPath || !oldPath) {
        return false;
    }
    return rename(PathZTemp(oldPath), PathZTemp(newPath)) == 0;
}

bool OverwriteAtomicRetry(Str dst, Str src, int retryCount, int retrySleepMs) {
    if (!dst || !src) {
        return false;
    }

    TempStr dstDir = path::GetDirTemp(dst);
    TempStr dstName = path::GetBaseNameTemp(dst);
    TempStr tempTemplate = fmt("%s/.%s.tmp.XXXXXX", dstDir, dstName);
    char* tempPathZ = CStrTemp(tempTemplate);
    int tempFd = mkstemp(tempPathZ);
    if (tempFd < 0) {
        return false;
    }
    close(tempFd);

    TempStr tempPath = Str(tempPathZ);
    if (!Copy(tempPath, src, false)) {
        Delete(tempPath);
        return false;
    }

    if (retryCount < 1) {
        retryCount = 1;
    }
    for (int i = 0; i < retryCount; i++) {
        if (rename(PathZTemp(tempPath), PathZTemp(dst)) == 0) {
            return true;
        }
        if (i + 1 < retryCount && retrySleepMs > 0) {
            usleep((useconds_t)retrySleepMs * 1000);
        }
    }

    Delete(tempPath);
    return false;
}

} // namespace file

int FileTimeDiffInSecs(const FILETIME& ft1, const FILETIME& ft2) {
    i64 diff = (i64)FileTimeToNs(ft1) - (i64)FileTimeToNs(ft2);
    return (int)(diff / 1000000000LL);
}

namespace dir {

bool Exists(WStr dir) {
    TempStr dirUtf8 = ToUtf8Temp(dir);
    return Exists(dirUtf8);
}

bool Exists(Str dir) {
    struct stat st;
    return StatPath(dir, st) && S_ISDIR(st.st_mode);
}

bool Create(Str dir) {
    if (mkdir(PathZTemp(dir), 0777) == 0) {
        return true;
    }
    return errno == EEXIST && Exists(dir);
}

static bool RemoveAllZ(const char* dir) {
    DIR* d = opendir(dir);
    if (!d) {
        return errno == ENOENT;
    }
    AutoCall closeDir(closedir, d);

    for (;;) {
        errno = 0;
        dirent* ent = readdir(d);
        if (!ent) {
            break;
        }
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        TempStr child = path::JoinTemp(Str(dir), Str(ent->d_name));
        struct stat st;
        if (lstat(child.s, &st) != 0) {
            return false;
        }
        if (S_ISDIR(st.st_mode)) {
            if (!RemoveAllZ(child.s)) {
                return false;
            }
        } else if (unlink(child.s) != 0) {
            return false;
        }
    }
    if (errno != 0) {
        return false;
    }
    return rmdir(dir) == 0;
}

bool RemoveAll(Str dir) {
    return RemoveAllZ(PathZTemp(dir));
}

bool HasWriteAccess(Str dir) {
    if (!dir) {
        return false;
    }
    TempStr path = path::JoinTemp(dir, StrL("__sumatra_write_test__.tmp"));
    int fd = open(PathZTemp(path), O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd < 0) {
        return false;
    }
    close(fd);
    unlink(PathZTemp(path));
    return true;
}

} // namespace dir

TempStr GetHomeDirTemp() {
    const char* home = getenv("HOME");
    if (!home || !home[0]) {
        return {};
    }
    return str::DupTemp(home);
}

TempStr ExpandEnvVarTemp(Str varName) {
    const char* val = getenv(CStrTemp(varName));
    if (!val) {
        return {};
    }
    return str::DupTemp(val);
}

TempStr ToAbsolutePathTemp(Str path) {
    return path::NormalizeTemp(path);
}
