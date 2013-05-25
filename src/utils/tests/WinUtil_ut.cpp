/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "UtAssert.h"
#include "WinUtil.h"

void WinUtilTest()
{
    ScopedCom comScope;

    {
        char *string = "abcde";
        size_t stringSize = 5, len;
        ScopedComPtr<IStream> stream(CreateStreamFromData(string, stringSize));
        assert(stream);
        char *data = (char *)GetDataFromStream(stream, &len);
        assert(data && stringSize == len && str::Eq(data, string));
        free(data);
    }

    {
        WCHAR *string = L"abcde";
        size_t stringSize = 10, len;
        ScopedComPtr<IStream> stream(CreateStreamFromData(string, stringSize));
        assert(stream);
        WCHAR *data = (WCHAR *)GetDataFromStream(stream, &len);
        assert(data && stringSize == len && str::Eq(data, string));
        free(data);
    }

    {
        RectI oneScreen = GetFullscreenRect(NULL);
        RectI allScreens = GetVirtualScreenRect();
        assert(allScreens.Intersect(oneScreen) == oneScreen);
    }

    {
        COLORREF c = AdjustLightness(RGB(255, 0, 0), 1.0f);
        assert(c == RGB(255, 0, 0));
        c = AdjustLightness(RGB(255, 0, 0), 2.0f);
        assert(c == RGB(255, 255, 255));
        c = AdjustLightness(RGB(255, 0, 0), 0.25f);
        assert(c == RGB(64, 0, 0));
        c = AdjustLightness(RGB(226, 196, 226), 95 / 255.0f);
        assert(c == RGB(105, 52, 105));
        c = AdjustLightness(RGB(255, 255, 255), 0.5f);
        assert(c == RGB(128, 128, 128));
    }
}
