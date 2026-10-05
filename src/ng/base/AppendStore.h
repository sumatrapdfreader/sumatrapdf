/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

enum class AppendStoreMode {
    DataFile,
    Inline,
    File,
};

struct AppendStoreRecord {
    AppendStoreMode mode = AppendStoreMode::DataFile;
    Str kind;
    Str meta;
    Str fileName;
    i64 timestampMs = 0;
    i64 indexOffset = 0;
    i64 dataOffset = 0;
    i64 dataSize = 0;
};

struct AppendStoreAppendOptions {
    AppendStoreMode mode = AppendStoreMode::DataFile;
    Str kind;
    Str meta;
    Str data;
    Str fileName;
    Str fileData;
    Str inlineMeta;
    i64 timestampMs = 0;
};

using AppendStoreRecordCallback = void (*)(AppendStoreRecord* rec, Str data, void* userData);

struct AppendStore {
    Str dataDir;
    Str indexFileName;
    Str dataFileName;
    bool syncWrite = false;
    AppendStoreRecordCallback onRecord = nullptr;
    void* userData = nullptr;

    Arena* arena = nullptr;
    file::FileHandle indexFile = file::kInvalidFileHandle;
    file::FileHandle dataFile = file::kInvalidFileHandle;
    Str indexFilePath;
    Str dataFilePath;
    char error[512] = {};
};

bool AppendStoreOpen(AppendStore* store);
void AppendStoreClose(AppendStore* store);
bool AppendStoreAppend(AppendStore* store, const AppendStoreAppendOptions& opts, AppendStoreRecord** recOut = nullptr);
Str AppendStoreReadPayload(AppendStore* store, const AppendStoreRecord* rec);
Str AppendStoreReadPayloadPart(AppendStore* store, const AppendStoreRecord* rec, i64 maxBytes);
Str AppendStoreReadFile(AppendStore* store, const AppendStoreRecord* rec);
Str AppendStoreError(AppendStore* store);
