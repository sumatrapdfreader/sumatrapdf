/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"
#include "base/AppendStore.h"

static void SetStoreError(AppendStore* store, Str error) {
    str::BufSet(Str(store->error, dimofi(store->error)), error);
}

static void SetStoreWinError(AppendStore* store, Str what) {
    SetStoreError(store, str::JoinTemp(what, StrL(": "), GetLastErrorAsStr(GetTempArena())));
}

Str AppendStoreError(AppendStore* store) {
    return store ? Str(store->error) : Str();
}

static bool HasChar(Str s, char c) {
    return str::IndexOfChar(s, c) >= 0;
}

static bool ValidateKindAndMeta(AppendStore* store, Str kind, Str meta) {
    if (kind.len == 0) {
        SetStoreError(store, StrL("kind is empty"));
        return false;
    }
    if (HasChar(kind, ' ') || HasChar(kind, '\n') || HasChar(kind, '\r')) {
        SetStoreError(store, StrL("kind cannot contain whitespace"));
        return false;
    }
    if (HasChar(meta, '\n') || HasChar(meta, '\r')) {
        SetStoreError(store, StrL("metadata cannot contain newlines"));
        return false;
    }
    return true;
}

static bool ValidateInlineData(AppendStore* store, Str data) {
    if (HasChar(data, '\n') || HasChar(data, '\r')) {
        SetStoreError(store, StrL("inline data cannot contain newlines"));
        return false;
    }
    return true;
}

static i64 UnixTimeMsNow() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER value;
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    return ((i64)value.QuadPart - 116444736000000000LL) / 10000;
}

static bool OpenAppendFile(AppendStore* store, Str path, HANDLE* fileOut) {
    if (*fileOut != INVALID_HANDLE_VALUE) {
        return true;
    }
    HANDLE file = CreateFileW(ToWStrTemp(path).s, GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        SetStoreWinError(store, str::JoinTemp(StrL("could not open "), path));
        return false;
    }
    *fileOut = file;
    return true;
}

static bool WriteAll(AppendStore* store, HANDLE file, Str data) {
    int written = 0;
    while (written < data.len) {
        DWORD n = 0;
        if (!WriteFile(file, data.s + written, (DWORD)(data.len - written), &n, nullptr) || n == 0) {
            SetStoreWinError(store, StrL("could not append data"));
            return false;
        }
        written += (int)n;
    }
    return true;
}

static bool AppendToFile(AppendStore* store, Str path, HANDLE* filePtr, Str data, i64* offsetOut) {
    if (!OpenAppendFile(store, path, filePtr)) {
        return false;
    }
    LARGE_INTEGER zero = {};
    LARGE_INTEGER offset = {};
    if (!SetFilePointerEx(*filePtr, zero, &offset, FILE_END)) {
        SetStoreWinError(store, StrL("could not seek to the end of append store"));
        return false;
    }
    if (offsetOut) {
        *offsetOut = offset.QuadPart;
    }
    if (!WriteAll(store, *filePtr, data)) {
        return false;
    }
    if (data.len > 0 && data.s[data.len - 1] != '\n' && !WriteAll(store, *filePtr, StrL("\n"))) {
        return false;
    }
    if (store->syncWrite && !FlushFileBuffers(*filePtr)) {
        SetStoreWinError(store, StrL("could not flush append store"));
        return false;
    }
    return true;
}

static bool ParseNonNegative(Str s, i64* valueOut) {
    if (s.len == 0) {
        return false;
    }
    i64 value = 0;
    for (int i = 0; i < s.len; i++) {
        if (!str::IsDigit(s.s[i])) {
            return false;
        }
        int digit = s.s[i] - '0';
        if (value > (INT64_MAX - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
    }
    *valueOut = value;
    return true;
}

static int SplitIndexFields(Str line, Str* parts) {
    int n = 0;
    int start = 0;
    for (int i = 0; i < line.len && n < 4; i++) {
        if (line.s[i] == ' ') {
            if (i > start) {
                parts[n++] = Str(line.s + start, i - start);
            }
            start = i + 1;
        }
    }
    if (start < line.len) {
        parts[n++] = Str(line.s + start, line.len - start);
    }
    return n;
}

static AppendStoreRecord* ParseIndexLine(AppendStore* store, Str line, i64 indexOffset) {
    Str parts[5] = {};
    int n = SplitIndexFields(line, parts);
    if (n < 4) {
        SetStoreError(store, str::JoinTemp(StrL("invalid index line: "), line));
        return nullptr;
    }

    auto* rec = AllocArray<AppendStoreRecord>(store->arena);
    rec->indexOffset = indexOffset;
    if (str::Eq(parts[0], StrL("_"))) {
        rec->mode = AppendStoreMode::Inline;
    } else if (str::Eq(parts[0], StrL("f"))) {
        rec->mode = AppendStoreMode::File;
    } else if (!ParseNonNegative(parts[0], &rec->dataOffset)) {
        SetStoreError(store, str::JoinTemp(StrL("invalid data offset: "), parts[0]));
        return nullptr;
    }
    if (!ParseNonNegative(parts[1], &rec->dataSize) || !ParseNonNegative(parts[2], &rec->timestampMs)) {
        SetStoreError(store, str::JoinTemp(StrL("invalid size or timestamp: "), line));
        return nullptr;
    }
    rec->kind = str::Dup(store->arena, parts[3]);
    if (n == 5) {
        if (rec->mode == AppendStoreMode::File) {
            rec->fileName = str::Dup(store->arena, parts[4]);
        } else {
            rec->meta = str::Dup(store->arena, parts[4]);
        }
    }
    if (rec->mode == AppendStoreMode::File && rec->fileName.len == 0) {
        SetStoreError(store, StrL("file record is missing its file name"));
        return nullptr;
    }
    return rec;
}

static bool ReplayIndex(AppendStore* store) {
    Str data = file::ReadFile(store->indexFilePath);
    if (!data.s) {
        SetStoreError(store, str::JoinTemp(StrL("could not read "), store->indexFilePath));
        return false;
    }
    int at = 0;
    while (at < data.len) {
        int lineStart = at;
        while (at < data.len && data.s[at] != '\n') {
            at++;
        }
        int lineEnd = at;
        if (lineEnd > lineStart && data.s[lineEnd - 1] == '\r') {
            lineEnd--;
        }
        if (at < data.len) {
            at++;
        }
        if (lineEnd == lineStart) {
            continue;
        }
        AppendStoreRecord* rec = ParseIndexLine(store, Str(data.s + lineStart, lineEnd - lineStart), lineStart);
        if (!rec) {
            str::Free(data);
            return false;
        }

        Str inlineData;
        if (rec->mode == AppendStoreMode::Inline || rec->mode == AppendStoreMode::File) {
            rec->dataOffset = at;
            if (rec->dataSize > data.len - at) {
                SetStoreError(store, StrL("inline record extends past the end of the index"));
                str::Free(data);
                return false;
            }
            inlineData = Str(data.s + at, (int)rec->dataSize);
            at += (int)rec->dataSize;
            if (at < data.len && data.s[at] == '\n') {
                at++;
            }
        }
        if (store->onRecord) {
            store->onRecord(rec, inlineData, store->userData);
        }
    }
    str::Free(data);
    return true;
}

bool AppendStoreOpen(AppendStore* store) {
    if (!store || store->dataDir.len == 0) {
        return false;
    }
    Str dataDir = store->dataDir;
    Str indexName = store->indexFileName.len > 0 ? store->indexFileName : StrL("index.txt");
    Str dataName = store->dataFileName.len > 0 ? store->dataFileName : StrL("data.bin");
    store->arena = ArenaNew();
    store->dataDir = str::Dup(store->arena, dataDir);
    store->indexFileName = str::Dup(store->arena, indexName);
    store->dataFileName = str::Dup(store->arena, dataName);
    store->indexFilePath = str::Join(store->arena, store->dataDir, StrL("\\"), store->indexFileName);
    store->dataFilePath = str::Join(store->arena, store->dataDir, StrL("\\"), store->dataFileName);
    store->error[0] = 0;

    if (!dir::CreateAll(store->dataDir)) {
        SetStoreError(store, str::JoinTemp(StrL("could not create "), store->dataDir));
        return false;
    }
    if (!OpenAppendFile(store, store->indexFilePath, &store->indexFile)) {
        return false;
    }
    return ReplayIndex(store);
}

void AppendStoreClose(AppendStore* store) {
    if (!store) {
        return;
    }
    if (store->indexFile != INVALID_HANDLE_VALUE) {
        CloseHandle(store->indexFile);
        store->indexFile = INVALID_HANDLE_VALUE;
    }
    if (store->dataFile != INVALID_HANDLE_VALUE) {
        CloseHandle(store->dataFile);
        store->dataFile = INVALID_HANDLE_VALUE;
    }
    ArenaDelete(store->arena);
    store->arena = nullptr;
}

static TempStr SerializeRecordTemp(const AppendStoreRecord* rec) {
    Str offset;
    if (rec->mode == AppendStoreMode::File) {
        offset = StrL("f");
    } else if (rec->mode == AppendStoreMode::Inline || rec->dataSize == 0) {
        offset = StrL("_");
    } else {
        offset = fmt("%lld", rec->dataOffset);
    }
    Str meta = rec->mode == AppendStoreMode::File ? rec->fileName : rec->meta;
    if (meta.len == 0) {
        return fmt("%s %lld %lld %s\n", offset, rec->dataSize, rec->timestampMs, rec->kind);
    }
    return fmt("%s %lld %lld %s %s\n", offset, rec->dataSize, rec->timestampMs, rec->kind, meta);
}

static bool ReplaceFile(Str from, Str to) {
    return MoveFileExW(ToWStrTemp(from).s, ToWStrTemp(to).s, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

bool AppendStoreAppend(AppendStore* store, const AppendStoreAppendOptions& opts, AppendStoreRecord** recOut) {
    if (!store || !store->arena || !ValidateKindAndMeta(store, opts.kind, opts.meta)) {
        return false;
    }
    if (opts.mode == AppendStoreMode::Inline && !ValidateInlineData(store, opts.data)) {
        return false;
    }
    if (opts.mode == AppendStoreMode::File &&
        (opts.fileName.len == 0 || HasChar(opts.fileName, '\r') || HasChar(opts.fileName, '\n') ||
         !ValidateInlineData(store, opts.inlineMeta))) {
        SetStoreError(store, StrL("invalid file record"));
        return false;
    }

    auto* rec = AllocArray<AppendStoreRecord>(store->arena);
    rec->mode = opts.mode;
    rec->kind = str::Dup(store->arena, opts.kind);
    rec->meta = str::Dup(store->arena, opts.meta);
    rec->fileName = str::Dup(store->arena, opts.fileName);
    rec->timestampMs = opts.timestampMs > 0 ? opts.timestampMs : UnixTimeMsNow();
    Str callbackData = opts.data;
    Str publishedPath;
    Str tempPath;

    if (opts.mode == AppendStoreMode::DataFile) {
        rec->dataSize = opts.data.len;
        if (opts.data.len > 0 &&
            !AppendToFile(store, store->dataFilePath, &store->dataFile, opts.data, &rec->dataOffset)) {
            return false;
        }
    } else if (opts.mode == AppendStoreMode::Inline) {
        rec->dataSize = opts.data.len;
    } else {
        rec->dataSize = opts.inlineMeta.len;
        callbackData = opts.inlineMeta;
        publishedPath = str::JoinTemp(store->dataDir, StrL("\\"), opts.fileName);
        tempPath = str::JoinTemp(publishedPath, StrL(".tmp"));
        if (!dir::CreateForFile(publishedPath) || !file::WriteFile(tempPath, opts.fileData)) {
            file::Delete(tempPath);
            SetStoreWinError(store, str::JoinTemp(StrL("could not write "), publishedPath));
            return false;
        }
    }

    TempStr line = SerializeRecordTemp(rec);
    if (!AppendToFile(store, store->indexFilePath, &store->indexFile, line, &rec->indexOffset)) {
        if (tempPath.s) {
            file::Delete(tempPath);
        }
        return false;
    }
    if ((opts.mode == AppendStoreMode::Inline || opts.mode == AppendStoreMode::File) && callbackData.len > 0) {
        LARGE_INTEGER zero = {};
        LARGE_INTEGER offset = {};
        if (!SetFilePointerEx(store->indexFile, zero, &offset, FILE_END)) {
            if (tempPath.s) {
                file::Delete(tempPath);
            }
            SetStoreWinError(store, StrL("could not seek in index file"));
            return false;
        }
        rec->dataOffset = offset.QuadPart;
        if (!WriteAll(store, store->indexFile, callbackData) ||
            (callbackData.s[callbackData.len - 1] != '\n' && !WriteAll(store, store->indexFile, StrL("\n")))) {
            if (tempPath.s) {
                file::Delete(tempPath);
            }
            return false;
        }
        if (store->syncWrite && !FlushFileBuffers(store->indexFile)) {
            if (tempPath.s) {
                file::Delete(tempPath);
            }
            SetStoreWinError(store, StrL("could not flush index file"));
            return false;
        }
    }
    if (opts.mode == AppendStoreMode::File && !ReplaceFile(tempPath, publishedPath)) {
        file::Delete(tempPath);
        SetStoreWinError(store, str::JoinTemp(StrL("could not publish "), publishedPath));
        return false;
    }
    if (store->onRecord) {
        store->onRecord(rec, callbackData, store->userData);
    }
    if (recOut) {
        *recOut = rec;
    }
    return true;
}

static Str ReadFilePart(AppendStore* store, Str path, HANDLE* filePtr, i64 offset, i64 size) {
    if (size <= 0 || size > INT_MAX) {
        return size == 0 ? str::Dup(Str()) : Str();
    }
    if (*filePtr == INVALID_HANDLE_VALUE) {
        *filePtr = CreateFileW(ToWStrTemp(path).s, GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
        if (*filePtr == INVALID_HANDLE_VALUE) {
            SetStoreWinError(store, str::JoinTemp(StrL("could not read "), path));
            return Str();
        }
    }
    LARGE_INTEGER pos;
    pos.QuadPart = offset;
    bool ok = SetFilePointerEx(*filePtr, pos, nullptr, FILE_BEGIN) != 0;
    char* buf = AllocArray<char>(nullptr, (int)size + 1);
    int total = 0;
    while (ok && total < size) {
        DWORD n = 0;
        ok = ReadFile(*filePtr, buf + total, (DWORD)(size - total), &n, nullptr) != 0 && n > 0;
        total += (int)n;
    }
    if (!ok) {
        CloseHandle(*filePtr);
        *filePtr = INVALID_HANDLE_VALUE;
        str::Free(Str(buf, (int)size));
        SetStoreWinError(store, str::JoinTemp(StrL("could not read payload from "), path));
        return Str();
    }
    return Str(buf, (int)size);
}

Str AppendStoreReadPayloadPart(AppendStore* store, const AppendStoreRecord* rec, i64 maxBytes) {
    if (!store || !rec || maxBytes < 0) {
        return Str();
    }
    i64 size = rec->dataSize < maxBytes ? rec->dataSize : maxBytes;
    bool inDataFile = rec->mode == AppendStoreMode::DataFile;
    Str path = inDataFile ? store->dataFilePath : store->indexFilePath;
    HANDLE* file = inDataFile ? &store->dataFile : &store->indexFile;
    return ReadFilePart(store, path, file, rec->dataOffset, size);
}

Str AppendStoreReadPayload(AppendStore* store, const AppendStoreRecord* rec) {
    return AppendStoreReadPayloadPart(store, rec, rec ? rec->dataSize : 0);
}

Str AppendStoreReadFile(AppendStore* store, const AppendStoreRecord* rec) {
    if (!store || !rec || rec->mode != AppendStoreMode::File) {
        return Str();
    }
    return file::ReadFile(str::JoinTemp(store->dataDir, StrL("\\"), rec->fileName));
}
