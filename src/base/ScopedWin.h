/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/AutoWin.h"

#if OS_WIN
template <class T>
using ScopedComPtr = AutoReleaseComPtr<T>;
template <class T>
using ScopedComQIPtr = AutoReleaseComQIPtr<T>;
using ScopedGetDC = AutoReleaseDC;
using ScopedSelectFont = AutoRestoreFont;
using ScopedCom = AutoCoUninitialize;
using ScopedOle = AutoOleUninitialize;
using ScopedGdiPlus = AutoGdiPlusShutdown;

#endif
