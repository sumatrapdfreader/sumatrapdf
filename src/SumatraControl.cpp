/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/ThreadUtil.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "Flags.h"
#include "SumatraTest.h"
#include "SumatraControl.h"

#include "utils/Log.h"

extern Flags* gCli;

enum class ControlCmd : u16 {
    Ping = 1,
    Quit = 2,
    TestSynctex = 10,
    TestSearch = 11,
    TestDest = 12,
    TestNamedDest = 13,
    TestChm = 14,
};

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
    char* str = nullptr;
    Vec<ControlArg*>* list = nullptr;
};

static void DeleteControlArg(ControlArg* arg) {
    if (!arg) {
        return;
    }
    free(arg->bytes);
    str::Free(arg->str);
    if (arg->list) {
        for (ControlArg* el : *arg->list) {
            DeleteControlArg(el);
        }
        delete arg->list;
    }
    delete arg;
}

struct ControlRequest {
    u16 cmd = 0;
    u16 reqId = 0;
    Vec<ControlArg*> args;
    StrBuilder results;
    HANDLE done = nullptr;
};

static void DeleteControlRequest(ControlRequest* req) {
    if (!req) {
        return;
    }
    for (ControlArg* arg : req->args) {
        DeleteControlArg(arg);
    }
    SafeCloseHandle(&req->done);
    delete req;
}

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

    bool ReadBytes(u8* dst, size_t len) {
        if (pos + len > size) {
            return false;
        }
        memcpy(dst, data + pos, len);
        pos += len;
        return true;
    }
};

static void AppendU16(StrBuilder& s, u16 v) {
    u8 buf[2] = {(u8)(v & 0xff), (u8)((v >> 8) & 0xff)};
    s.Append(buf, sizeof(buf));
}

static void AppendU32(StrBuilder& s, u32 v) {
    u8 buf[4] = {(u8)(v & 0xff), (u8)((v >> 8) & 0xff), (u8)((v >> 16) & 0xff), (u8)((v >> 24) & 0xff)};
    s.Append(buf, sizeof(buf));
}

static void AppendArgEnd(StrBuilder& s) {
    AppendU16(s, (u16)ControlArgType::End);
}

static void AppendArgInt(StrBuilder& s, i32 v) {
    AppendU16(s, (u16)ControlArgType::Int32);
    AppendU32(s, (u32)v);
}

static void AppendArgString(StrBuilder& s, const char* str) {
    if (!str) {
        str = "";
    }
    size_t len = str::Len(str);
    AppendU16(s, (u16)ControlArgType::String);
    AppendU32(s, (u32)len);
    s.Append(str, len);
    s.AppendChar(0);
}

static bool ParseArg(PacketReader& r, ControlArg** argOut);

static bool ParseArgList(PacketReader& r, Vec<ControlArg*>* args, bool explicitCount, u16 count = 0) {
    for (u16 i = 0; !explicitCount || i < count; i++) {
        ControlArg* arg = nullptr;
        if (!ParseArg(r, &arg)) {
            return false;
        }
        if (!arg) {
            return !explicitCount;
        }
        args->Append(arg);
    }
    return true;
}

static bool ParseArg(PacketReader& r, ControlArg** argOut) {
    u16 typeRaw = 0;
    if (!r.ReadU16(typeRaw)) {
        return false;
    }
    ControlArgType type = (ControlArgType)typeRaw;
    if (type == ControlArgType::End) {
        *argOut = nullptr;
        return true;
    }

    ControlArg* arg = new ControlArg();
    arg->type = type;
    if (type == ControlArgType::Int32) {
        u32 v = 0;
        if (!r.ReadU32(v)) {
            DeleteControlArg(arg);
            return false;
        }
        arg->intVal = (i32)v;
    } else if (type == ControlArgType::Bytes) {
        u32 len = 0;
        if (!r.ReadU32(len)) {
            DeleteControlArg(arg);
            return false;
        }
        arg->bytes = AllocArray<u8>(len + 1);
        arg->bytesLen = len;
        if (!r.ReadBytes(arg->bytes, len)) {
            DeleteControlArg(arg);
            return false;
        }
    } else if (type == ControlArgType::String) {
        u32 len = 0;
        if (!r.ReadU32(len)) {
            DeleteControlArg(arg);
            return false;
        }
        arg->str = AllocArray<char>((size_t)len + 1);
        if (!r.ReadBytes((u8*)arg->str, len)) {
            DeleteControlArg(arg);
            return false;
        }
        u8 zero = 1;
        if (!r.ReadBytes(&zero, 1) || zero != 0) {
            DeleteControlArg(arg);
            return false;
        }
    } else if (type == ControlArgType::List) {
        u16 count = 0;
        if (!r.ReadU16(count)) {
            DeleteControlArg(arg);
            return false;
        }
        arg->list = new Vec<ControlArg*>();
        if (!ParseArgList(r, arg->list, true, count)) {
            DeleteControlArg(arg);
            return false;
        }
    } else {
        DeleteControlArg(arg);
        return false;
    }
    *argOut = arg;
    return true;
}

static ControlArg* ArgAt(ControlRequest* req, size_t idx, ControlArgType type) {
    if (idx >= req->args.size()) {
        return nullptr;
    }
    ControlArg* arg = req->args.at(idx);
    if (arg->type != type) {
        return nullptr;
    }
    return arg;
}

static const char* StringArg(ControlRequest* req, size_t idx) {
    ControlArg* arg = ArgAt(req, idx, ControlArgType::String);
    return arg ? arg->str : nullptr;
}

static bool IntArg(ControlRequest* req, size_t idx, i32& valOut) {
    ControlArg* arg = ArgAt(req, idx, ControlArgType::Int32);
    if (!arg) {
        return false;
    }
    valOut = arg->intVal;
    return true;
}

static void AppendError(ControlRequest* req, const char* msg) {
    req->results.Reset();
    AppendArgInt(req->results, -1);
    AppendArgString(req->results, msg);
    AppendArgEnd(req->results);
}

static void AppendTestResult(ControlRequest* req, int exitCode, char* result) {
    AppendArgInt(req->results, exitCode);
    AppendArgString(req->results, result);
    AppendArgEnd(req->results);
    str::Free(result);
}

static void ExecuteControlRequest(ControlRequest* req) {
    switch ((ControlCmd)req->cmd) {
        case ControlCmd::Ping:
            AppendArgString(req->results, "pong");
            AppendArgEnd(req->results);
            break;

        case ControlCmd::Quit:
            AppendArgInt(req->results, 0);
            AppendArgEnd(req->results);
            PostQuitMessage(0);
            break;

        case ControlCmd::TestSynctex: {
            i32 line = 0;
            const char* pdf = StringArg(req, 0);
            const char* src = StringArg(req, 1);
            if (!pdf || !src || !IntArg(req, 2, line)) {
                AppendError(req, "TestSynctex expects string pdf, string source, int line");
                break;
            }
            AppendTestResult(req, 0, TestSynctexResult(pdf, src, line));
            break;
        }

        case ControlCmd::TestSearch: {
            const char* pdf = StringArg(req, 0);
            const char* needle = StringArg(req, 1);
            const char* password = StringArg(req, 2);
            if (!pdf || !needle) {
                AppendError(req, "TestSearch expects string pdf, string needle, optional string password");
                break;
            }
            if (!password && gCli) {
                password = gCli->password;
            }
            AppendTestResult(req, 0, TestSearchResult(pdf, needle, password));
            break;
        }

        case ControlCmd::TestDest: {
            i32 destNo = 0;
            const char* pdf = StringArg(req, 0);
            if (!pdf || !IntArg(req, 1, destNo)) {
                AppendError(req, "TestDest expects string pdf, int destinationNumber");
                break;
            }
            AppendTestResult(req, 0, TestDestResult(pdf, destNo));
            break;
        }

        case ControlCmd::TestNamedDest: {
            const char* pdf = StringArg(req, 0);
            const char* name = StringArg(req, 1);
            if (!pdf || !name) {
                AppendError(req, "TestNamedDest expects string pdf, string name");
                break;
            }
            AppendTestResult(req, 0, TestNamedDestResult(pdf, name));
            break;
        }

        case ControlCmd::TestChm: {
            const char* chm = StringArg(req, 0);
            if (!chm) {
                AppendError(req, "TestChm expects string chmPath");
                break;
            }
            int exitCode = 0;
            char* res = TestChmResult(chm, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        default:
            AppendError(req, "unknown control command");
            break;
    }
    SetEvent(req->done);
}

static bool ReadExact(HANDLE h, void* data, DWORD len) {
    u8* d = (u8*)data;
    DWORD total = 0;
    while (total < len) {
        DWORD nRead = 0;
        if (!ReadFile(h, d + total, len - total, &nRead, nullptr) || nRead == 0) {
            return false;
        }
        total += nRead;
    }
    return true;
}

static bool WriteExact(HANDLE h, const void* data, DWORD len) {
    const u8* d = (const u8*)data;
    DWORD total = 0;
    while (total < len) {
        DWORD nWritten = 0;
        if (!WriteFile(h, d + total, len - total, &nWritten, nullptr) || nWritten == 0) {
            return false;
        }
        total += nWritten;
    }
    return true;
}

static ControlRequest* ReadControlRequest(HANDLE h) {
    u32 size = 0;
    if (!ReadExact(h, &size, sizeof(size))) {
        return nullptr;
    }
    if (size < 4 || size > 16 * 1024 * 1024) {
        return nullptr;
    }
    u8* data = AllocArray<u8>(size);
    if (!ReadExact(h, data, size)) {
        free(data);
        return nullptr;
    }

    PacketReader r{data, size};
    ControlRequest* req = new ControlRequest();
    if (!r.ReadU16(req->cmd) || !r.ReadU16(req->reqId) || !ParseArgList(r, &req->args, false)) {
        DeleteControlRequest(req);
        free(data);
        return nullptr;
    }
    req->done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    free(data);
    return req;
}

static bool WriteControlResponse(HANDLE h, ControlRequest* req) {
    StrBuilder payload;
    AppendU16(payload, req->reqId);
    payload.AppendSlice(req->results.AsByteSlice());

    StrBuilder packet;
    AppendU32(packet, (u32)payload.size());
    packet.AppendSlice(payload.AsByteSlice());
    return WriteExact(h, packet.LendData(), (DWORD)packet.size());
}

static void ProcessControlConnection(HANDLE h) {
    for (;;) {
        ControlRequest* req = ReadControlRequest(h);
        if (!req) {
            return;
        }
        uitask::Post(MkFunc0<ControlRequest>(ExecuteControlRequest, req), "SumatraControl");
        WaitForSingleObject(req->done, INFINITE);
        bool ok = WriteControlResponse(h, req);
        DeleteControlRequest(req);
        if (!ok) {
            return;
        }
    }
}

static WCHAR* FullPipeName(const char* pipeName) {
    if (str::StartsWith(pipeName, R"(\\.\pipe\)")) {
        return ToWStr(pipeName);
    }
    TempStr fullName = str::FormatTemp(R"(\\.\pipe\%s)", pipeName);
    return ToWStr(fullName);
}

static void SumatraControlThread(char* pipeName) {
    AutoFreeWStr pipeNameW(FullPipeName(pipeName));
    str::Free(pipeName);

    for (;;) {
        HANDLE pipe =
            CreateNamedPipeW(pipeNameW.Get(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1,
                             64 * 1024, 64 * 1024, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            logf("CreateNamedPipeW failed for control pipe\n");
            return;
        }
        BOOL connected = ConnectNamedPipe(pipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (connected) {
            ProcessControlConnection(pipe);
        }
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
}

void StartSumatraControl(const char* pipeName) {
    if (str::IsEmpty(pipeName)) {
        return;
    }
    RunAsync(MkFunc0<char>(SumatraControlThread, str::Dup(pipeName)), "SumatraControl");
}
