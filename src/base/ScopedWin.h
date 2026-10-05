/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/AutoWin.h"

#if OS_WIN
template <class T>
using ScopedComPtr = AutoReleaseComPtr<T>;
template <class T>
using ScopedComQIPtr = AutoReleaseComQIPtr<T>;
template <class T>
using ScopedGdiObj = AutoDeleteGdiObj<T>;
using ScopedGetDC = AutoReleaseDC;
using ScopedSelectObject = AutoRestoreGdiObject;
using ScopedSelectFont = AutoRestoreFont;
using ScopedSelectPen = AutoRestorePen;
using ScopedSelectBrush = AutoRestoreBrush;
using ScopedCom = AutoCoUninitialize;
using ScopedOle = AutoOleUninitialize;
using ScopedGdiPlus = AutoGdiPlusShutdown;
using AutoDeleteObject = AutoDeleteGdiObj<HGDIOBJ>;

#endif
