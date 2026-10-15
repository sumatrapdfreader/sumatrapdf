/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by SumatraControlCommon.cpp and each app's SumatraControl.cpp ---

TempStr PageBoxesResultTemp(int pageNo, int* exitCodeOut);
void AppendLayoutRect(str::Builder& out, Str name, bool visible, Rect rect);
TempStr SelectionVarsResultTemp(Str pattern, int* exitCodeOut);
TempStr DocumentSignaturesResultTemp(int* exitCodeOut);
TempStr DocumentFontListResultTemp(int* exitCodeOut);
TempStr DocumentPropertiesResultTemp(int* exitCodeOut);
enum class ControlArgType : u16 {
    End = 0,
    Int32 = 1,
    Bytes = 2,
    String = 3,
    List = 4,
};
struct ControlArg {
    ControlArgType type = ControlArgType::End;
    i32 intVal = 0;
    u8* bytes = nullptr;
    u32 bytesLen = 0;
    Str str;
    Vec<ControlArg*>* list = nullptr;
};
void DeleteControlArg(ControlArg* arg);
void AppendU16(str::Builder& s, u16 v);
void AppendU32(str::Builder& s, u32 v);
void AppendArgEnd(str::Builder& s);
void AppendArgInt(str::Builder& s, i32 v);
void AppendArgString(str::Builder& s, Str str);

enum class RenderIdleState : u8 {
    NotReady = 0,
    Busy = 1,
    Idle = 2,
};
// a manual-reset event, from the portable primitives
struct DoneEvent {
    Mutex mutex;
    ConditionVariable cond;
    bool isSet = false;

    void Set() {
        mutex.Lock();
        isSet = true;
        mutex.Unlock();
        cond.WakeAll();
    }
    void Reset() {
        mutex.Lock();
        isSet = false;
        mutex.Unlock();
    }
    void Wait() {
        mutex.Lock();
        while (!isSet) {
            cond.Wait(&mutex);
        }
        mutex.Unlock();
    }
};

struct ControlRequest {
    u16 cmd = 0;
    u16 reqId = 0;
    Vec<ControlArg*> args;
    str::Builder results;
    DoneEvent done;
    RenderIdleState idleState = RenderIdleState::NotReady;
    char idleInfo[320]{};
};
void DeleteControlRequest(ControlRequest* req);
Str StringArg(ControlRequest* req, size_t idx);
bool IntArg(ControlRequest* req, size_t idx, i32& valOut);
void AppendError(ControlRequest* req, Str msg);
void AppendTestResult(ControlRequest* req, int exitCode, Str result);

struct PacketReader {
    const u8* data = nullptr;
    size_t size = 0;
    size_t pos = 0;

    bool ReadU16(u16& v) {
        if (pos + 2 > size) {
            return false;
        }
        v = (u16)(data[pos] | (data[pos + 1] << 8));
        pos += 2;
        return true;
    }

    bool ReadU32(u32& v) {
        if (pos + 4 > size) {
            return false;
        }
        v = (u32)data[pos] | ((u32)data[pos + 1] << 8) | ((u32)data[pos + 2] << 16) | ((u32)data[pos + 3] << 24);
        pos += 4;
        return true;
    }

    bool ReadBytes(u8* dst, size_t n) {
        if (pos + n > size) {
            return false;
        }
        memcpy(dst, data + pos, n);
        pos += n;
        return true;
    }
};
bool ParseArgList(PacketReader& r, Vec<ControlArg*>* args, bool explicitCount, u16 count = 0);
void RunWaitRenderIdle(ControlRequest* req);
void RunWaitSessionRestored(ControlRequest* req);

// implemented by each app
bool ParseArg(PacketReader& r, ControlArg** argOut);
void SnapshotRenderIdle(ControlRequest* req);
void SnapshotSessionRestore(ControlRequest* req);
void AddFavoriteSilent(MainWindow* win, int pageNo);
bool ControlFavoritesMenu(MainWindow* win, Str action, TempStr* idsOut);
void ControlTogglePresentation(MainWindow* win);
void ControlToggleFullScreen(MainWindow* win);

#if OS_WIN
using ControlConn = HANDLE;
bool ReadExact(HANDLE h, void* data, DWORD n);
bool WriteExact(HANDLE h, Str data);
struct ControlThreadArg {
    Str pipeName;
};
void SumatraControlThread(ControlThreadArg* arg);
// implemented by each app
bool ProcessControlConnection(HANDLE h);
#else
// a connected unix domain socket
using ControlConn = int;
bool ReadExact(int fd, void* data, u32 n);
bool WriteExact(int fd, Str data);
#endif

// wire ids shared by both apps' ControlCmd. 101/102 differ, so they stay per app.
enum class ControlFrameCmd : u16 {
    Quit = 2,
    WaitRenderIdle = 54,
    CrashMe = 93,
    WaitSessionRestored = 103,
};

enum class ControlFrameAction : u8 {
    RunOnUi = 0,
    WaitRenderIdle = 1,
    WaitSessionRestored = 2,
};

struct ControlServeResult {
    bool hadRequest = false;
    bool wrote = false;
    bool isQuit = false;
};

ControlFrameAction ControlFrameActionFor(u16 cmd);
bool ControlCmdEndsListener(u16 cmd);
using ControlExecFn = void (*)(ControlRequest*);
ControlServeResult ServeControlRequest(ControlConn h, ControlExecFn execOnUi);

struct LayoutProbeState {
    MainWindow* win = nullptr;
    int count = 0;
    bool active = false;
};

ControlRequest* ReadControlRequest(ControlConn h);
bool WriteControlResponse(ControlConn h, ControlRequest* req);

TempStr FavoriteNavResultTemp(Str action, int pageNo, int* exitCodeOut);
TempStr DisplayModeResultTemp(Str action, int* exitCodeOut);
