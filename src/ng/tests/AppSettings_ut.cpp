/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: settings round-trip. The settings file format is part of the port's
// contract (agents.md), so the golden file in docs/test is orig's default
// serialization and must survive a load + save unchanged.

#include "base/Base.h"
#include "base/File.h"
#include "base/SettingsUtil.h"

#define INCLUDE_SETTINGSSTRUCTS_METADATA
#include "Settings.h"
#include "AppSettings.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

// in src/tools/test_util.cpp
TempStr TestDocPathTemp(Str relPath);

void AppSettingsTest() {
    TempStr path = TestDocPathTemp(StrL("docs/test/SumatraPDF-settings.txt"));
    Str golden = file::ReadFile(path);
    utassert(len(golden) > 0);
    AutoCall freeGolden((void (*)(Str))str::Free, golden);

    // the defaults serialize to exactly the golden file
    Settings* def = NewSettings({});
    utassert(def != nullptr);
    Str serialized = SerializeSettings(def, {});
    utassert(str::Eq(serialized, golden));
    str::Free(serialized);
    DeleteSettings(def);

    // and a load + save of it changes nothing
    Settings* loaded = NewSettings(golden);
    utassert(loaded != nullptr);
    Str out = SerializeSettings(loaded, golden);
    utassert(str::Eq(out, golden));
    str::Free(out);
    DeleteSettings(loaded);
}
