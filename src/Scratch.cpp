/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// this is for adding temporary code for testing

#include "base/Base.h"
#include "base/Archive.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/win/WebView.h"

#include "DocProperties.h"
#include "SimpleBrowserWindow.h"
#include "PalmDbReader.h"
#include "EbookBase.h"
#include "Scratch.h"

// ----------------

void TestBrowser() {
    SimpleBrowserCreateArgs args;
    args.title = StrL("Test Browser Window");
    args.url = StrL("https://blog.kowalczyk.info/");
    args.pos = {CW_USEDEFAULT, CW_USEDEFAULT, 480, 640};
    auto* w = new SimpleBrowserWindow();
    w->Create(args);
    // RunMessageLoop(nullptr, w->hwnd);
    // delete w;
}
