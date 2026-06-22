/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// this is for adding temporary code for testing

// TODO: remove this
// #define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

#include "utils/BaseUtil.h"
#include "utils/Archive.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/WebView.h"

#include "DocProperties.h"
#include "SimpleBrowserWindow.h"
#include "PalmDbReader.h"
#include "EbookBase.h"

// ----------------

void TestBrowser() {
    SimpleBrowserCreateArgs args;
    args.title = "Test Browser Window";
    args.url = "https://blog.kowalczyk.info/";
    args.pos = {CW_USEDEFAULT, CW_USEDEFAULT, 480, 640};
    auto w = new SimpleBrowserWindow();
    w->Create(args);
    // RunMessageLoop(nullptr, w->hwnd);
    // delete w;
}
