/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Thread.h"
#include "base/UITask.h"
#include "base/Win.h"

#include "Settings.h"
#include "Flags.h"
#include "SumatraTest.h"
#include "SumatraPDF.h"
#include "SelectionTranslate.h"
#include "ImageSaveCropResize.h"
#include "base/GuessFileType.h"
#include "FindWindow.h"

#include "base/Log.h"

extern Flags* gCli;

enum class ControlCmd : u16 {
    Ping = 1,
    Quit = 2,
    TestSynctex = 10,
    TestSearch = 11,
    TestDest = 12,
    TestNamedDest = 13,
    TestChm = 14,
    TestSelectionTranslate = 15,
    TestTripleClickLineSelect = 16,
    TestContextMenuSelection = 17,
    TestGoToFindMatch = 18,
    // IDs 19-21 unused (reserved on the -dbg-control wire protocol; do not renumber).
    // Assign new test commands starting at 23.
    TestInverseSearch = 22,
    TestImageResizeArrowKey = 23,
    TestFindResultPageColumnClip = 24,
    TestFileKind = 25,
    TestScrollToLink = 26,
    TestI18nErrorString = 27,
    TestPageInfoOverlay = 28,
    TestGetToc = 29,
    TestPageLinks = 30,
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
    Str str;
    Vec<ControlArg*>* list = nullptr;
};

static void DeleteControlArg(ControlArg* arg) {
    if (!arg) {
        return;
    }
    free(arg->bytes);
    str::FreePtr(&arg->str);
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
    str::Builder results;
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

    bool ReadBytes(u8* dst, size_t n) {
        if (pos + n > size) {
            return false;
        }
        memcpy(dst, data + pos, n);
        pos += n;
        return true;
    }
};

static void AppendU16(str::Builder& s, u16 v) {
    u8 buf[2] = {(u8)(v & 0xff), (u8)((v >> 8) & 0xff)};
    s.Append(Str((char*)(buf), (int)(sizeof(buf))));
}

static void AppendU32(str::Builder& s, u32 v) {
    u8 buf[4] = {(u8)(v & 0xff), (u8)((v >> 8) & 0xff), (u8)((v >> 16) & 0xff), (u8)((v >> 24) & 0xff)};
    s.Append(Str((char*)(buf), (int)(sizeof(buf))));
}

static void AppendArgEnd(str::Builder& s) {
    AppendU16(s, (u16)ControlArgType::End);
}

static void AppendArgInt(str::Builder& s, i32 v) {
    AppendU16(s, (u16)ControlArgType::Int32);
    AppendU32(s, (u32)v);
}

static void AppendArgString(str::Builder& s, Str str) {
    if (!str) {
        str = StrL("");
    }
    size_t n = (size_t)str.len;
    AppendU16(s, (u16)ControlArgType::String);
    AppendU32(s, (u32)n);
    s.Append(str);
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
        u32 n = 0;
        if (!r.ReadU32(n)) {
            DeleteControlArg(arg);
            return false;
        }
        arg->bytes = AllocArray<u8>(n + 1);
        arg->bytesLen = n;
        if (!r.ReadBytes(arg->bytes, n)) {
            DeleteControlArg(arg);
            return false;
        }
    } else if (type == ControlArgType::String) {
        u32 n = 0;
        if (!r.ReadU32(n)) {
            DeleteControlArg(arg);
            return false;
        }
        char* strBuf = AllocArray<char>((size_t)n + 1);
        if (!r.ReadBytes((u8*)strBuf, n)) {
            DeleteControlArg(arg);
            return false;
        }
        arg->str = Str(strBuf, (int)n);
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
    if (idx >= (size_t)len(req->args)) {
        return nullptr;
    }
    ControlArg* arg = req->args.at(idx);
    if (arg->type != type) {
        return nullptr;
    }
    return arg;
}

static Str StringArg(ControlRequest* req, size_t idx) {
    ControlArg* arg = ArgAt(req, idx, ControlArgType::String);
    return arg ? arg->str : Str{};
}

static bool IntArg(ControlRequest* req, size_t idx, i32& valOut) {
    ControlArg* arg = ArgAt(req, idx, ControlArgType::Int32);
    if (!arg) {
        return false;
    }
    valOut = arg->intVal;
    return true;
}

static void AppendError(ControlRequest* req, Str msg) {
    req->results.Reset();
    AppendArgInt(req->results, -1);
    AppendArgString(req->results, msg);
    AppendArgEnd(req->results);
}

static void AppendTestResult(ControlRequest* req, int exitCode, Str result) {
    AppendArgInt(req->results, exitCode);
    AppendArgString(req->results, result);
    AppendArgEnd(req->results);
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
            Str pdf = StringArg(req, 0);
            Str src = StringArg(req, 1);
            if (!pdf || !src || !IntArg(req, 2, line)) {
                AppendError(req, "TestSynctex expects string pdf, string source, int line");
                break;
            }
            AppendTestResult(req, 0, SynctexResultTemp(pdf, src, line));
            break;
        }

        case ControlCmd::TestInverseSearch: {
            i32 page = 0, x = 0, y = 0;
            Str pdf = StringArg(req, 0);
            if (!pdf || !IntArg(req, 1, page) || !IntArg(req, 2, x) || !IntArg(req, 3, y)) {
                AppendError(req, "TestInverseSearch expects string pdf, int page, int x, int y");
                break;
            }
            AppendTestResult(req, 0, InverseSearchResultTemp(pdf, page, x, y));
            break;
        }

        case ControlCmd::TestSearch: {
            Str pdf = StringArg(req, 0);
            Str needle = StringArg(req, 1);
            Str password = StringArg(req, 2);
            if (!pdf || !needle) {
                AppendError(req, "TestSearch expects string pdf, string needle, optional string password");
                break;
            }
            if (!password && gCli) {
                password = gCli->password;
            }
            AppendTestResult(req, 0, SearchResultTemp(pdf, needle, password));
            break;
        }

        case ControlCmd::TestDest: {
            i32 destNo = 0;
            Str pdf = StringArg(req, 0);
            if (!pdf || !IntArg(req, 1, destNo)) {
                AppendError(req, "TestDest expects string pdf, int destinationNumber");
                break;
            }
            AppendTestResult(req, 0, DestResultTemp(pdf, destNo));
            break;
        }

        case ControlCmd::TestNamedDest: {
            Str pdf = StringArg(req, 0);
            Str name = StringArg(req, 1);
            if (!pdf || !name) {
                AppendError(req, "TestNamedDest expects string pdf, string name");
                break;
            }
            AppendTestResult(req, 0, NamedDestResultTemp(pdf, name));
            break;
        }

        case ControlCmd::TestChm: {
            Str chm = StringArg(req, 0);
            if (!chm) {
                AppendError(req, "TestChm expects string chmPath");
                break;
            }
            int exitCode = 0;
            Str res = ChmResultTemp(chm, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestSelectionTranslate: {
            i32 backend = 0;
            Str srcLang = StringArg(req, 1);
            Str dstLang = StringArg(req, 2);
            Str text = StringArg(req, 3);
            if (!IntArg(req, 0, backend) || !srcLang || !dstLang || !text) {
                AppendError(req,
                            "TestSelectionTranslate expects int backend, string srcLang, string dstLang, string text");
                break;
            }
            int exitCode = 0;
            Str res = SelectionTranslateResultTemp(backend, srcLang, dstLang, text, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestTripleClickLineSelect: {
            Str pdf = StringArg(req, 0);
            Str clickWord = StringArg(req, 1);
            Str expectedLine = StringArg(req, 2);
            if (!pdf || !clickWord || !expectedLine) {
                AppendError(req, "TestTripleClickLineSelect expects string pdf, string clickWord, string expectedLine");
                break;
            }
            int exitCode = 0;
            Str res = TripleClickLineSelectResultTemp(pdf, clickWord, expectedLine, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestContextMenuSelection: {
            Str word1 = StringArg(req, 0);
            Str word2 = StringArg(req, 1);
            Str cursorWord = StringArg(req, 2);
            if (!word1 || !word2 || !cursorWord) {
                AppendError(req, "TestContextMenuSelection expects string word1, string word2, string cursorWord");
                break;
            }
            int exitCode = 0;
            Str res = ContextMenuSelectionResultTemp(word1, word2, cursorWord, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestGoToFindMatch: {
            Str word = StringArg(req, 0);
            Str typed = StringArg(req, 1);
            if (!word || !typed) {
                AppendError(req, "TestGoToFindMatch expects string word, string typed");
                break;
            }
            int exitCode = 0;
            Str res = GoToFindMatchResultTemp(word, typed, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestImageResizeArrowKey: {
            Str imagePath = StringArg(req, 0);
            if (!imagePath) {
                AppendError(req, "TestImageResizeArrowKey expects string imagePath");
                break;
            }
            int exitCode = 0;
            Str res = ImageResizeArrowKeyResultTemp(imagePath, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFindResultPageColumnClip: {
            int exitCode = 0;
            Str res = FindResultPageColumnClipResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFileKind: {
            Str path = StringArg(req, 0);
            Str expectedKind = StringArg(req, 1);
            if (!path || !expectedKind) {
                AppendError(req, "TestFileKind expects string path, string expectedKind");
                break;
            }
            int exitCode = 0;
            Str res = FileKindResultTemp(path, expectedKind, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestScrollToLink: {
            i32 minDelta = 50;
            IntArg(req, 0, minDelta);
            int exitCode = 0;
            Str res = ScrollToLinkResultTemp(minDelta, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestI18nErrorString: {
            int exitCode = 0;
            Str res = I18nErrorStringResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestPageInfoOverlay: {
            Str pathTwo = StringArg(req, 0);
            Str pathOne = StringArg(req, 1);
            if (!pathTwo || !pathOne) {
                AppendError(req, "TestPageInfoOverlay expects string pathTwoPages, string pathOnePage");
                break;
            }
            int exitCode = 0;
            Str res = PageInfoOverlayResultTemp(pathTwo, pathOne, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestGetToc: {
            Str path = StringArg(req, 0);
            if (!path) {
                AppendError(req, "TestGetToc expects string path");
                break;
            }
            int exitCode = 0;
            Str res = GetTocResultTemp(path, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestPageLinks: {
            Str path = StringArg(req, 0);
            i32 pageNo = 1;
            if (!path || !IntArg(req, 1, pageNo)) {
                AppendError(req, "TestPageLinks expects string path, int pageNo");
                break;
            }
            int exitCode = 0;
            Str res = PageLinksResultTemp(path, pageNo, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        default:
            AppendError(req, "unknown control command");
            break;
    }
    SetEvent(req->done);
}

static bool ReadExact(HANDLE h, void* data, DWORD n) {
    u8* d = (u8*)data;
    DWORD total = 0;
    while (total < n) {
        DWORD nRead = 0;
        if (!ReadFile(h, d + total, n - total, &nRead, nullptr) || nRead == 0) {
            return false;
        }
        total += nRead;
    }
    return true;
}

static bool WriteExact(HANDLE h, Str data) {
    const u8* d = (const u8*)data.s;
    int total = 0;
    while (total < data.len) {
        DWORD nWritten = 0;
        if (!WriteFile(h, d + total, (DWORD)(data.len - total), &nWritten, nullptr) || nWritten == 0) {
            return false;
        }
        total += (int)nWritten;
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
    str::Builder payload;
    AppendU16(payload, req->reqId);
    payload.Append(ToStr(req->results));

    str::Builder packet;
    AppendU32(packet, (u32)len(payload));
    packet.Append(ToStr(payload));
    return WriteExact(h, ToStr(packet));
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

static WStr FullPipeNameOwned(Str pipeName) {
    if (str::StartsWith(pipeName, R"(\\.\pipe\)")) {
        return ToWStr(pipeName);
    }
    TempStr fullName = str::JoinTemp(StrL(R"(\\.\pipe\)"), pipeName);
    return ToWStr(fullName);
}

struct ControlThreadArg {
    Str pipeName;
};

static void SumatraControlThread(ControlThreadArg* arg) {
    WStr pipeNameW = FullPipeNameOwned(arg->pipeName);
    str::FreePtr(&arg->pipeName);
    delete arg;

    for (;;) {
        HANDLE pipe = CreateNamedPipeW(pipeNameW.s, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                       1, 64 * 1024, 64 * 1024, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            logf("CreateNamedPipeW failed for control pipe, err=%u\n", (unsigned)GetLastError());
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

void StartSumatraControl(Str pipeName) {
    if (str::IsEmpty(pipeName)) {
        return;
    }
    auto* arg = new ControlThreadArg{str::Dup(pipeName)};
    RunAsync(MkFunc0(SumatraControlThread, arg), "SumatraControl");
}
