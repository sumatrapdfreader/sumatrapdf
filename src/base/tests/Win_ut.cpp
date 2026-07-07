/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/Win.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

void WinUtilTest() {
    ScopedCom comScope;

    {
        Str string = "abcde";
        size_t stringSize = string.len;
        auto strm = CreateStreamFromData(Str((char*)string.s, (int)stringSize));
        ScopedComPtr<IStream> stream(strm);
        utassert(stream);
        Str data = ReadIStream(stream);
        utassert((u8*)data.s);
        utassert(stringSize == (size_t)data.len);
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
        Rect oneScreen = GetFullscreenRect(nullptr);
        Rect allScreens = GetVirtualScreenRect();
        utassert(allScreens.Intersect(oneScreen) == oneScreen);
    }

    // TODO: moved AdjustLigthness() to Colors.[h|cpp] which is outside of utils directory
#if 0
    {
        COLORREF c = AdjustLightness(RGB(255, 0, 0), 1.0f);
        utassert(c == RGB(255, 0, 0));
        c = AdjustLightness(RGB(255, 0, 0), 2.0f);
        utassert(c == RGB(255, 255, 255));
        c = AdjustLightness(RGB(255, 0, 0), 0.25f);
        utassert(c == RGB(64, 0, 0));
        c = AdjustLightness(RGB(226, 196, 226), 95 / 255.0f);
        utassert(c == RGB(105, 52, 105));
        c = AdjustLightness(RGB(255, 255, 255), 0.5f);
        utassert(c == RGB(128, 128, 128));
    }
#endif
}
