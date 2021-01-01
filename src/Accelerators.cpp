/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "resource.h"
#include "Commands.h"

//#define CTRL (FCONTROL | FVIRTKEY)
//#define SHIFT_CTRL (FSHIFT | FCONTROL | FVIRTKEY)

// TODO: should FVIRTKEY be set for chars like 'A'
ACCEL gAccelerators[] = {
    {FCONTROL | FVIRTKEY, 'A', CmdSelectAll},
    {FCONTROL | FVIRTKEY, 'B', CmdFavoriteAdd},
    {FCONTROL | FVIRTKEY, 'C', CmdCopySelection},
    {FCONTROL | FVIRTKEY, 'D', CmdProperties},
    {FCONTROL | FVIRTKEY, 'F', CmdFindFirst},
    {FCONTROL | FVIRTKEY, 'G', CmdGoToPage},
    {FCONTROL | FVIRTKEY, 'L', CmdViewPresentationMode},
    {FSHIFT | FCONTROL | FVIRTKEY, 'L', CmdViewFullScreen},
    {FCONTROL | FVIRTKEY, 'N', CmdNewWindow},
    {FSHIFT | FCONTROL | FVIRTKEY, 'N', CmdDuplicateInNewWindow},
    {FCONTROL | FVIRTKEY, 'O', CmdOpen},
    {FCONTROL | FVIRTKEY, 'S', CmdSaveAs},
    {FSHIFT | FCONTROL | FVIRTKEY, 'S', CmdSaveAsBookmark},
    {FCONTROL | FVIRTKEY, 'P', CmdPrint},
    {FCONTROL | FVIRTKEY, 'Q', CmdExit},
    {FCONTROL | FVIRTKEY, 'W', CmdClose},
    {FCONTROL | FVIRTKEY, 'Y', CmdZoomCustom},
    {FCONTROL | FVIRTKEY, '0', CmdZoomFitPage},
    {FCONTROL | FVIRTKEY, VK_NUMPAD0, CmdZoomFitPage},
    {FCONTROL | FVIRTKEY, '1', CmdZoomActualSize},
    {FCONTROL | FVIRTKEY, VK_NUMPAD1, CmdZoomActualSize},
    {FCONTROL | FVIRTKEY, '2', CmdZoomFitWidth},
    {FCONTROL | FVIRTKEY, VK_NUMPAD2, CmdZoomFitWidth},
    {FCONTROL | FVIRTKEY, '3', CmdZoomFitContent},
    {FCONTROL | FVIRTKEY, VK_NUMPAD3, CmdZoomFitContent},
    {FCONTROL | FVIRTKEY, '6', CmdViewSinglePage},
    {FCONTROL | FVIRTKEY, VK_NUMPAD6, CmdViewSinglePage},
    {FCONTROL | FVIRTKEY, '7', CmdViewFacing},
    {FCONTROL | FVIRTKEY, VK_NUMPAD7, CmdViewFacing},
    {FCONTROL | FVIRTKEY, '8', CmdViewBook},
    {FCONTROL | FVIRTKEY, VK_NUMPAD8, CmdViewBook},
    {FCONTROL | FVIRTKEY, VK_ADD, CmdZoomIn},
    {FSHIFT | FCONTROL | FVIRTKEY, VK_ADD, CmdViewRotateRight},
    {FCONTROL | FVIRTKEY, VK_OEM_PLUS, CmdZoomIn},
    {FSHIFT | FCONTROL | FVIRTKEY, VK_OEM_PLUS, CmdViewRotateRight},
    {FCONTROL | FVIRTKEY, VK_INSERT, CmdCopySelection},
    {FVIRTKEY, VK_F2, CmdRenameFile},
    {FVIRTKEY, VK_F3, CmdFindNext},
    {FSHIFT | FVIRTKEY, VK_F3, CmdFindPrev},
    {FCONTROL | FVIRTKEY, VK_F3, CmdFindNextSel},
    {FSHIFT | FCONTROL | FVIRTKEY, VK_F3, CmdFindPrevSel},
    {FCONTROL | FVIRTKEY, VK_F4, CmdClose},
    {FVIRTKEY, VK_F5, CmdViewPresentationMode},
    {FVIRTKEY, VK_F6, CmdMoveFrameFocus},
    {FVIRTKEY, VK_F8, CmdViewShowHideToolbar},

    {FVIRTKEY, VK_F9, CmdViewShowHideMenuBar},
    {FVIRTKEY, VK_F11, CmdViewFullScreen},
    {FSHIFT | FVIRTKEY, VK_F11, CmdViewPresentationMode},
    {FVIRTKEY, VK_F12, CmdViewBookmarks},
    {FCONTROL | FVIRTKEY, VK_SUBTRACT, CmdZoomOut},
    {FSHIFT | FCONTROL | FVIRTKEY, VK_SUBTRACT, CmdViewRotateLeft},
    {FCONTROL | FVIRTKEY, VK_OEM_MINUS, CmdZoomOut},
    {FSHIFT | FCONTROL | FVIRTKEY, VK_OEM_MINUS, CmdViewRotateLeft},
    {FALT | FVIRTKEY, VK_LEFT, CmdGoToNavBack},
    {FALT | FVIRTKEY, VK_RIGHT, CmdGoToNavForward},
};

HACCEL CreateSumatraAcceleratorTable() {
    int n = (int)dimof(gAccelerators);
    HACCEL res = CreateAcceleratorTableW(gAccelerators, n);
    CrashIf(res == nullptr);
    return res;
}
