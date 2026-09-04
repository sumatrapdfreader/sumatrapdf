/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/AppendStore.h"

static void SetStoreError(AppendStore* store, Str error) {
    str::BufSet(Str(store->error, dimofi(store->error)), error);
}

// what the caller was doing plus the OS error that stopped it
static void SetStoreOSError(AppendStore* store, Str what) {
    SetStoreError(store, str::JoinTemp(what, StrL(": "), file::LastErrorTemp()));
}

Str AppendStoreError(AppendStore* store) {
    return store ? Str(store->error) : Str();
}

static bool HasChar(Str s, char c) {
    return str::IndexOfChar(s, c) >= 0;
}

static bool ValidateKindAndMeta(AppendStore* store, Str kind, Str meta) {
    if (len(kind) == 0) {
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

static bool OpenAppendFile(AppendStore* store, Str path, file::FileHandle* fileOut) {
    if (*fileOut != file::kInvalidFileHandle) {
        return true;
    }
    file::FileHandle file = file::OpenReadWrite(path, true);
    if (file == file::kInvalidFileHandle) {
        SetStoreOSError(store, str::JoinTemp(StrL("could not open "), path));
        return false;
    }
    *fileOut = file;
    return true;
}

static bool WriteAll(AppendStore* store, file::FileHandle file, Str data) {
    if (!file::WriteAll(file, data)) {
        SetStoreOSError(store, StrL("could not append data"));
        return false;
    }
    return true;
}

static bool AppendToFile(AppendStore* store, Str path, file::FileHandle* filePtr, Str data, i64* offsetOut) {
    if (!OpenAppendFile(store, path, filePtr)) {
        return false;
    }
    i64 offset = file::SeekEnd(*filePtr);
    if (offset < 0) {
        SetStoreOSError(store, StrL("could not seek to the end of append store"));
        return false;
    }
    if (offsetOut) {
        *offsetOut = offset;
    }
    if (!WriteAll(store, *filePtr, data)) {
        return false;
    }
    if (data.len > 0 && data.s[data.len - 1] != '\n' && !WriteAll(store, *filePtr, StrL("\n"))) {
        return false;
    }
    if (store->syncWrite && !file::Flush(*filePtr)) {
        SetStoreOSError(store, StrL("could not flush append store"));
        return false;
    }
    return true;
}

static bool ParseNonNegative(Str s, i64* valueOut) {
    if (len(s) == 0) {
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
    if (rec->mode == AppendStoreMode::File && len(rec->fileName) == 0) {
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
    if (!store || len(store->dataDir) == 0) {
        return false;
    }
    Str dataDir = store->dataDir;
    Str indexName = store->indexFileName.len > 0 ? store->indexFileName : StrL("index.txt");
    Str dataName = store->dataFileName.len > 0 ? store->dataFileName : StrL("data.bin");
    store->arena = ArenaNew();
    store->dataDir = str::Dup(store->arena, dataDir);
    store->indexFileName = str::Dup(store->arena, indexName);
    store->dataFileName = str::Dup(store->arena, dataName);
    store->indexFilePath = path::Join(store->arena, store->dataDir, store->indexFileName);
    store->dataFilePath = path::Join(store->arena, store->dataDir, store->dataFileName);
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
    if (store->indexFile != file::kInvalidFileHandle) {
        file::Close(store->indexFile);
        store->indexFile = file::kInvalidFileHandle;
    }
    if (store->dataFile != file::kInvalidFileHandle) {
        file::Close(store->dataFile);
        store->dataFile = file::kInvalidFileHandle;
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
    if (len(meta) == 0) {
        return fmt("%s %lld %lld %s\n", offset, rec->dataSize, rec->timestampMs, rec->kind);
    }
    return fmt("%s %lld %lld %s %s\n", offset, rec->dataSize, rec->timestampMs, rec->kind, meta);
}

bool AppendStoreAppend(AppendStore* store, const AppendStoreAppendOptions& opts, AppendStoreRecord** recOut) {
    if (!store || !store->arena || !ValidateKindAndMeta(store, opts.kind, opts.meta)) {
        return false;
    }
    if (opts.mode == AppendStoreMode::Inline && !ValidateInlineData(store, opts.data)) {
        return false;
    }
    if (opts.mode == AppendStoreMode::File &&
        (len(opts.fileName) == 0 || HasChar(opts.fileName, '\r') || HasChar(opts.fileName, '\n') ||
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
        publishedPath = path::JoinTemp(store->dataDir, opts.fileName);
        tempPath = str::JoinTemp(publishedPath, StrL(".tmp"));
        if (!dir::CreateForFile(publishedPath) || !file::WriteFile(tempPath, opts.fileData)) {
            file::Delete(tempPath);
            SetStoreOSError(store, str::JoinTemp(StrL("could not write "), publishedPath));
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
        i64 offset = file::SeekEnd(store->indexFile);
        if (offset < 0) {
            if (tempPath.s) {
                file::Delete(tempPath);
            }
            SetStoreOSError(store, StrL("could not seek in index file"));
            return false;
        }
        rec->dataOffset = offset;
        if (!WriteAll(store, store->indexFile, callbackData) ||
            (callbackData.s[callbackData.len - 1] != '\n' && !WriteAll(store, store->indexFile, StrL("\n")))) {
            if (tempPath.s) {
                file::Delete(tempPath);
            }
            return false;
        }
        if (store->syncWrite && !file::Flush(store->indexFile)) {
            if (tempPath.s) {
                file::Delete(tempPath);
            }
            SetStoreOSError(store, StrL("could not flush index file"));
            return false;
        }
    }
    if (opts.mode == AppendStoreMode::File && !file::RenameReplace(publishedPath, tempPath)) {
        file::Delete(tempPath);
        SetStoreOSError(store, str::JoinTemp(StrL("could not publish "), publishedPath));
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

static Str ReadFilePart(AppendStore* store, Str path, file::FileHandle* filePtr, i64 offset, i64 size) {
    if (size <= 0 || size > INT_MAX) {
        return size == 0 ? str::Dup(Str()) : Str();
    }
    if (*filePtr == file::kInvalidFileHandle) {
        *filePtr = file::OpenReadWrite(path, false);
        if (*filePtr == file::kInvalidFileHandle) {
            SetStoreOSError(store, str::JoinTemp(StrL("could not read "), path));
            return Str();
        }
    }
    char* buf = AllocArray<char>(nullptr, (int)size + 1);
    if (!file::ReadAt(*filePtr, offset, buf, (int)size)) {
        // the handle may be at an unknown position now, so drop it and let the
        // next read re-open the file
        file::Close(*filePtr);
        *filePtr = file::kInvalidFileHandle;
        str::Free(Str(buf, (int)size));
        SetStoreOSError(store, str::JoinTemp(StrL("could not read payload from "), path));
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
    file::FileHandle* file = inDataFile ? &store->dataFile : &store->indexFile;
    return ReadFilePart(store, path, file, rec->dataOffset, size);
}

Str AppendStoreReadPayload(AppendStore* store, const AppendStoreRecord* rec) {
    return AppendStoreReadPayloadPart(store, rec, rec ? rec->dataSize : 0);
}

Str AppendStoreReadFile(AppendStore* store, const AppendStoreRecord* rec) {
    if (!store || !rec || rec->mode != AppendStoreMode::File) {
        return Str();
    }
    return file::ReadFile(path::JoinTemp(store->dataDir, rec->fileName));
}
