/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

void WinUtilTest() {
    ScopedCom comScope;

    {
        const char* string = "abcde";
        size_t stringSize = str::Len(string);
        auto strm = CreateStreamFromData({(u8*)string, stringSize});
        ScopedComPtr<IStream> stream(strm);
        utassert(stream);
        ByteSlice data = GetDataFromStream(stream, nullptr);
        utassert(data.Get());
        utassert(stringSize == data.size());
        const char* s = data;
        utassert(str::Eq(s, string));
        data.Free();
    }

    {
        const WCHAR* string = L"abcde";
        size_t stringSize = str::Len(string) * sizeof(WCHAR);
        auto strm = CreateStreamFromData({(u8*)string, stringSize});
        ScopedComPtr<IStream> stream(strm);
        utassert(stream);
        ByteSlice dataTmp = GetDataFromStream(stream, nullptr);
        WCHAR* data = (WCHAR*)dataTmp.Get();
        utassert(data && stringSize == dataTmp.size() && str::Eq(data, string));
        dataTmp.Free();
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
