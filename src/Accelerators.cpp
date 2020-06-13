/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "resource.h"
#include "ResourceIds.h"

//#define CTRL (FCONTROL | FVIRTKEY)
//#define SHIFT_CTRL (FSHIFT | FCONTROL | FVIRTKEY)

// TODO: should FVIRTKEY be set for chars like 'A'
ACCEL gAccelerators[] = {
    {FCONTROL | FVIRTKEY, 'A', (WORD)Cmd::SelectAll},
    {FCONTROL | FVIRTKEY, 'B', (WORD)Cmd::FavAdd},
    {FCONTROL | FVIRTKEY, 'C', (WORD)Cmd::CopySelection},
    {FCONTROL | FVIRTKEY, 'D', (WORD)Cmd::Properties},
    {FCONTROL | FVIRTKEY, 'F', (WORD)Cmd::FindFirst},
    {FCONTROL | FVIRTKEY, 'G', (WORD)Cmd::GoToPage},
    {FCONTROL | FVIRTKEY, 'L', (WORD)Cmd::ViewPresentationMode},
    {FSHIFT | FCONTROL | FVIRTKEY, 'L', (WORD)Cmd::ViewFullScreen},
    {FCONTROL | FVIRTKEY, 'N', (WORD)Cmd::NewWindow},
    {FSHIFT | FCONTROL | FVIRTKEY, 'N', (WORD)Cmd::DuplicateInNewWindow},
    {FCONTROL | FVIRTKEY, 'O', (WORD)Cmd::Open},
    {FCONTROL | FVIRTKEY, 'S', (WORD)Cmd::SaveAs},
    {FSHIFT | FCONTROL | FVIRTKEY, 'S', (WORD)Cmd::SaveAsBookmark},
    {FCONTROL | FVIRTKEY, 'P', (WORD)Cmd::Print},
    {FCONTROL | FVIRTKEY, 'Q', (WORD)Cmd::Exit},
    {FCONTROL | FVIRTKEY, 'W', (WORD)Cmd::Close},
    {FCONTROL | FVIRTKEY, 'Y', (WORD)Cmd::ZoomCustom},
    {FCONTROL | FVIRTKEY, '0', (WORD)Cmd::ZoomFitPage},
    {FCONTROL | FVIRTKEY, VK_NUMPAD0, (WORD)Cmd::ZoomFitPage},
    {FCONTROL | FVIRTKEY, '1', (WORD)Cmd::ZoomActualSize},
    {FCONTROL | FVIRTKEY, VK_NUMPAD1, (WORD)Cmd::ZoomActualSize},
    {FCONTROL | FVIRTKEY, '2', (WORD)Cmd::ZoomFitWidth},
    {FCONTROL | FVIRTKEY, VK_NUMPAD2, (WORD)Cmd::ZoomFitWidth},
    {FCONTROL | FVIRTKEY, '3', (WORD)Cmd::ZoomFitContent},
    {FCONTROL | FVIRTKEY, VK_NUMPAD3, (WORD)Cmd::ZoomFitContent},
    {FCONTROL | FVIRTKEY, '6', (WORD)Cmd::ViewSinglePage},
    {FCONTROL | FVIRTKEY, VK_NUMPAD6, (WORD)Cmd::ViewSinglePage},
    {FCONTROL | FVIRTKEY, '7', (WORD)Cmd::ViewFacing},
    {FCONTROL | FVIRTKEY, VK_NUMPAD7, (WORD)Cmd::ViewFacing},
    {FCONTROL | FVIRTKEY, '8', (WORD)Cmd::ViewBook},
    {FCONTROL | FVIRTKEY, VK_NUMPAD8, (WORD)Cmd::ViewBook},
    {FCONTROL | FVIRTKEY, VK_ADD, (WORD)Cmd::ViewZoomIn},
    {FSHIFT | FCONTROL | FVIRTKEY, VK_ADD, (WORD)Cmd::ViewRotateRight},
    {FCONTROL | FVIRTKEY, VK_OEM_PLUS, (WORD)Cmd::ViewZoomIn},
    {FSHIFT | FCONTROL | FVIRTKEY, VK_OEM_PLUS, (WORD)Cmd::ViewRotateRight},
    {FCONTROL | FVIRTKEY, VK_INSERT, (WORD)Cmd::CopySelection},
    {FVIRTKEY, VK_F2, (WORD)Cmd::RenameFile},
    {FVIRTKEY, VK_F3, (WORD)Cmd::FindNext},
    {FSHIFT | FVIRTKEY, VK_F3, (WORD)Cmd::FindPrev},
    {FCONTROL | FVIRTKEY, VK_F3, (WORD)Cmd::FindNextSel},
    {FSHIFT | FCONTROL | FVIRTKEY, VK_F3, (WORD)Cmd::FindPrevSel},
    {FCONTROL | FVIRTKEY, VK_F4, (WORD)Cmd::Close},
    {FVIRTKEY, VK_F5, (WORD)Cmd::ViewPresentationMode},
    {FVIRTKEY, VK_F6, (WORD)Cmd::MoveFrameFocus},
    {FVIRTKEY, VK_F8, (WORD)Cmd::ViewShowHideToolbar},

    {FVIRTKEY, VK_F9, (WORD)Cmd::ViewShowHideMenuBar},
    {FVIRTKEY, VK_F11, (WORD)Cmd::ViewFullScreen},
    {FSHIFT | FVIRTKEY, VK_F11, (WORD)Cmd::ViewPresentationMode},
    {FVIRTKEY, VK_F12, (WORD)Cmd::ViewBookmarks},
    {FCONTROL | FVIRTKEY, VK_SUBTRACT, (WORD)Cmd::ViewZoomOut},
    {FSHIFT | FCONTROL | FVIRTKEY, VK_SUBTRACT, (WORD)Cmd::ViewRotateLeft},
    {FCONTROL | FVIRTKEY, VK_OEM_MINUS, (WORD)Cmd::ViewZoomOut},
    {FSHIFT | FCONTROL | FVIRTKEY, VK_OEM_MINUS, (WORD)Cmd::ViewRotateLeft},
    {FALT | FVIRTKEY, VK_LEFT, (WORD)Cmd::GoToNavBack},
    {FALT | FVIRTKEY, VK_RIGHT, (WORD)Cmd::GoToNavForward},
};

HACCEL CreateSumatraAcceleratorTable() {
    int n = (int)dimof(gAccelerators);
    HACCEL res = CreateAcceleratorTableW(gAccelerators, n);
    CrashIf(res == nullptr);
    return res;
}
