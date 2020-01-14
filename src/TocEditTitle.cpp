/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/Log.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
#include "wingui/TreeModel.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/TreeCtrl.h"
#include "wingui/ButtonCtrl.h"

#include "EngineBase.h"

#include "ParseBKM.h"
#include "TocEditor.h"
#include "TocEditTitle.h"

bool StartTocEditTitle(HWND hwndOwner, TocItem* ti) {
    return false;
}
