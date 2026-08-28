/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/AppendStore.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

struct AppendStoreTestState {
    Vec<AppendStoreRecord*> records;
    Vec<Str> inlineData;
};

static void OnAppendStoreTestRecord(AppendStoreRecord* rec, Str data, void* userData) {
    auto* state = (AppendStoreTestState*)userData;
    VecAppend(state->records, rec);
    VecAppend(state->inlineData, data.s ? str::Dup(data) : Str());
}

static void FreeAppendStoreTestState(AppendStoreTestState& state) {
    for (int i = 0; i < state.inlineData.len; i++) {
        str::Free(state.inlineData[i]);
    }
}

void AppendStoreTest() {
    Str testDir = str::Dup(GetTempFilePathTemp(StrL("ast")));
    utassert(testDir.len > 0);
    file::Delete(testDir);
    utassert(dir::Create(testDir));

    AppendStoreTestState written;
    AppendStore store;
    store.dataDir = testDir;
    store.indexFileName = StrL("history.txt");
    store.dataFileName = StrL("history.bin");
    store.onRecord = OnAppendStoreTestRecord;
    store.userData = &written;
    utassert(AppendStoreOpen(&store));

    AppendStoreAppendOptions dataOpts;
    dataOpts.kind = StrL("clip");
    dataOpts.meta = StrL("first");
    dataOpts.data = StrL("hello\nworld");
    dataOpts.timestampMs = 123;
    utassert(AppendStoreAppend(&store, dataOpts));

    AppendStoreAppendOptions inlineOpts;
    inlineOpts.mode = AppendStoreMode::Inline;
    inlineOpts.kind = StrL("delete");
    inlineOpts.meta = StrL("0");
    inlineOpts.data = StrL("small");
    inlineOpts.timestampMs = 456;
    utassert(AppendStoreAppend(&store, inlineOpts));

    AppendStoreAppendOptions fileOpts;
    fileOpts.mode = AppendStoreMode::File;
    fileOpts.kind = StrL("attachment");
    fileOpts.fileName = StrL("attachment.txt");
    fileOpts.fileData = StrL("file body");
    fileOpts.inlineMeta = StrL("meta");
    fileOpts.timestampMs = 789;
    utassert(AppendStoreAppend(&store, fileOpts));
    utassert(written.records.len == 3);
    AppendStoreClose(&store);

    AppendStoreTestState replayed;
    AppendStore reopened;
    reopened.dataDir = testDir;
    reopened.indexFileName = StrL("history.txt");
    reopened.dataFileName = StrL("history.bin");
    reopened.onRecord = OnAppendStoreTestRecord;
    reopened.userData = &replayed;
    utassert(AppendStoreOpen(&reopened));
    utassert(replayed.records.len == 3);
    utassert(replayed.records[0]->mode == AppendStoreMode::DataFile);
    utassert(replayed.records[0]->timestampMs == 123);
    utassert(str::Eq(replayed.records[0]->kind, StrL("clip")));
    utassert(str::Eq(replayed.records[0]->meta, StrL("first")));
    utassert(replayed.inlineData[0].s == nullptr);
    utassert(str::Eq(replayed.inlineData[1], StrL("small")));
    utassert(replayed.records[2]->mode == AppendStoreMode::File);
    utassert(str::Eq(replayed.inlineData[2], StrL("meta")));

    Str part = AppendStoreReadPayloadPart(&reopened, replayed.records[0], 5);
    utassert(str::Eq(part, StrL("hello")));
    str::Free(part);
    Str full = AppendStoreReadPayload(&reopened, replayed.records[0]);
    utassert(str::Eq(full, StrL("hello\nworld")));
    str::Free(full);
    Str fileData = AppendStoreReadFile(&reopened, replayed.records[2]);
    utassert(str::Eq(fileData, StrL("file body")));
    str::Free(fileData);
    AppendStoreClose(&reopened);
    FreeAppendStoreTestState(written);
    FreeAppendStoreTestState(replayed);

    utassert(dir::RemoveAll(testDir));
    str::Free(testDir);
}
