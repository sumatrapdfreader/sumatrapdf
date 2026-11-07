/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: settings round-trip. The settings file format is part of the port's
// contract (agents.md), so the golden file in docs/test is orig's default
// serialization and must survive a load + save unchanged.

#include "base/Base.h"
#include "base/File.h"
#include "base/SettingsUtil.h"

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

    // ng has no Windows scrollbars, so that orig default becomes smart
    TempStr expectLoaded = str::ReplaceTemp(golden, StrL("Scrollbars = windows"), StrL("Scrollbars = smart"));
    // ng also sizes tabs to their names by default
    TempStr expect = str::ReplaceTemp(expectLoaded, StrL("TabsEqualWidth = true"), StrL("TabsEqualWidth = false"));

    // the defaults serialize to the golden file, with those substitutions
    Settings* def = NewSettings({});
    utassert(def != nullptr);
    Str serialized = SerializeSettings(def, {});
    utassert(str::Eq(serialized, expect));
    str::Free(serialized);
    DeleteSettings(def);

    // and a load + save of it changes nothing, apart from that scrollbar value
    Settings* loaded = NewSettings(golden);
    utassert(loaded != nullptr);
    Str out = SerializeSettings(loaded, golden);
    utassert(str::Eq(out, expectLoaded));
    str::Free(out);
    DeleteSettings(loaded);

    // ng on Windows shares orig's settings file, and array elements don't keep
    // unknown keys: a save must not drop what orig wrote into a FileState
    Str withFile = StrL(
        "FileStates [\n"
        "\t[\n"
        "\t\tFilePath = C:\\doc.pdf\n"
        "\t\tPageNo = 3\n"
        "\t\tMeasurementScale = 2.5\n"
        "\t\tMeasurementUnit = mm\n"
        "\t]\n"
        "]\n");
    loaded = NewSettings(withFile);
    utassert(loaded != nullptr);
    out = SerializeSettings(loaded, withFile);
    utassert(str::Contains(out, StrL("MeasurementScale = 2.5")));
    utassert(str::Contains(out, StrL("MeasurementUnit = mm")));
    str::Free(out);
    DeleteSettings(loaded);
}
