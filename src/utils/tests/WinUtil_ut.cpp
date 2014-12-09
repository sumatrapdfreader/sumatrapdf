/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "WinUtil.h"

// must be last due to assert() over-write
#include "UtAssert.h"

void WinUtilTest()
{
    ScopedCom comScope;

    {
        char *string = "abcde";
        size_t stringSize = 5, len;
        ScopedComPtr<IStream> stream(CreateStreamFromData(string, stringSize));
        utassert(stream);
        char *data = (char *)GetDataFromStream(stream, &len);
        utassert(data && stringSize == len && str::Eq(data, string));
        free(data);
    }

    {
        WCHAR *string = L"abcde";
        size_t stringSize = 10, len;
        ScopedComPtr<IStream> stream(CreateStreamFromData(string, stringSize));
        utassert(stream);
        WCHAR *data = (WCHAR *)GetDataFromStream(stream, &len);
        utassert(data && stringSize == len && str::Eq(data, string));
        free(data);
    }

    {
        RectI oneScreen = GetFullscreenRect(nullptr);
        RectI allScreens = GetVirtualScreenRect();
        utassert(allScreens.Intersect(oneScreen) == oneScreen);
    }

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
}
