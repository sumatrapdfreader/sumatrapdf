/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/CmdLineArgsIter.h"
#include "base/ScopedWin.h"
#include "base/Win.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

// Round-trip QuoteCmdLineArgTemp through CommandLineToArgvW (ParseCmdLine).
// The GHSA-xvxg-cwmx-hr7j breakout payload must stay a single argv element.
static void QuoteCmdLineArgTest() {
    auto roundTripOne = [](Str input) {
        TempStr quoted = QuoteCmdLineArgTemp(input);
        utassert(quoted.s != nullptr);
        // ParseCmdLine uses CommandLineToArgvW; prefix a dummy argv[0].
        TempStr cmdLine = fmt("exe %s", quoted);
        StrVec args;
        ParseCmdLine(cmdLine, args);
        utassert(len(args) >= 1);
        utassert(str::Eq(args[0], StrL("exe")));
        if (len(input) == 0) {
            // `exe ""` → only "exe" after empty-token skip in ParseCmdLine
            utassert(len(args) == 1);
            return;
        }
        utassert(len(args) == 2);
        utassert(str::Eq(args[1], input));
    };

    roundTripOne(StrL("hello"));
    roundTripOne(StrL("hello world"));
    roundTripOne(StrL("say \"hi\""));
    roundTripOne(StrL("path\\with\\backslashes"));
    roundTripOne(StrL("trailing\\"));
    roundTripOne(StrL("trailing\\\\"));
    // PoC from the advisory: naive " -> \" turns this into a breakout.
    roundTripOne(StrL("X\\\" --always-approve "));
    roundTripOne(StrL("some text \\\" --dangerously-skip-permissions "));
    roundTripOne(StrL("Text: X\\\" --model evil"));
    roundTripOne(StrL(""));
    roundTripOne(StrL("a\\\"b\\\"c"));
    roundTripOne(StrL("ends with quote\""));
    roundTripOne(StrL("\\"));
    roundTripOne(StrL("\\\""));

    // Explicit expected encodings for the breakout cases
    utassert(str::Eq(QuoteCmdLineArgTemp(StrL("X\\\" --always-approve ")), StrL("\"X\\\\\\\" --always-approve \"")));
    utassert(str::Eq(QuoteCmdLineArgTemp(StrL("trailing\\")), StrL("\"trailing\\\\\"")));
    utassert(str::Eq(QuoteCmdLineArgTemp(StrL("a b")), StrL("\"a b\"")));
    utassert(str::Eq(QuoteCmdLineArgTemp(StrL("")), StrL("\"\"")));
    utassert(QuoteCmdLineArgTemp({}).s == nullptr);
}

void WinUtilTest() {
    ScopedCom comScope;

    QuoteCmdLineArgTest();

    {
        Str string = StrL("abcde");
        auto strm = CreateStreamFromData(string);
        ScopedComPtr<IStream> stream(strm);
        utassert(stream);
        Str data = ReadIStream(stream);
        utassert((u8*)data.s);
        utassert(string.len == data.len);
        utassert(data.s[data.len] == 0);
        utassert(data.s[data.len + 1] == 0);
        Str s = data;
        utassert(str::Eq(s, string));
        str::Free(data);
    }

    {
        WStr string = L"abcde";
        size_t stringSize = string.len * sizeof(WCHAR);
        auto strm = CreateStreamFromData(Str((char*)string.s, (int)stringSize));
        ScopedComPtr<IStream> stream(strm);
        utassert(stream);
        Str dataTmp = ReadIStream(stream);
        WStr data = WStr((WCHAR*)(u8*)dataTmp.s, (int)((size_t)dataTmp.len / sizeof(WCHAR)));
        utassert(data && stringSize == (size_t)dataTmp.len && wstr::Eq(data, string));
        utassert(dataTmp.s[dataTmp.len] == 0);
        utassert(dataTmp.s[dataTmp.len + 1] == 0);
        str::Free(dataTmp);
    }

    {
        Rect oneScreen = HwndGetFullscreenRect(nullptr);
        Rect allScreens = GetVirtualScreenRect();
        utassert(allScreens.Intersect(oneScreen) == oneScreen);
    }

    // TODO: moved AdjustLigthness() to Colors.[h|cpp] which is outside of utils directory
#if 0
    {
        Color c = AdjustLightness(kColRed, 1.0f);
        utassert(c == kColRed);
        c = AdjustLightness(kColRed, 2.0f);
        utassert(c == kColWhite);
        c = AdjustLightness(kColRed, 0.25f);
        utassert(c == MkRgb(64, 0, 0));
        c = AdjustLightness(MkRgb(226, 196, 226), 95 / 255.0f);
        utassert(c == MkRgb(105, 52, 105));
        c = AdjustLightness(kColWhite, 0.5f);
        utassert(c == MkRgb(128, 128, 128));
    }
#endif
}
