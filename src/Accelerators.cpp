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
    {FCONTROL | FVIRTKEY, 'B', (WORD)IDM_FAV_ADD},
    {FCONTROL | FVIRTKEY, 'C', (WORD)Cmd::CopySelection},
    {FCONTROL | FVIRTKEY, 'D', (WORD)Cmd::Properties},
    {FCONTROL | FVIRTKEY, 'F', (WORD)IDM_FIND_FIRST},
    {FCONTROL | FVIRTKEY, 'G', (WORD)IDM_GOTO_PAGE},
    {FCONTROL | FVIRTKEY, 'L', (WORD)Cmd::ViewPresentationMode},
    {FSHIFT | FCONTROL | FVIRTKEY, 'L', (WORD)Cmd::ViewFullScreen},
    {FCONTROL | FVIRTKEY, 'N', (WORD)IDM_NEW_WINDOW},
    {FSHIFT | FCONTROL | FVIRTKEY, 'N', (WORD)IDM_DUPLICATE_IN_NEW_WINDOW},
    {FCONTROL | FVIRTKEY, 'O', (WORD)Cmd::Open},
    {FCONTROL | FVIRTKEY, 'S', (WORD)Cmd::SaveAs},
    {FSHIFT | FCONTROL | FVIRTKEY, 'S', (WORD)Cmd::SaveAsBookmark},
    {FCONTROL | FVIRTKEY, 'P', (WORD)Cmd::Print},
    {FCONTROL | FVIRTKEY, 'Q', (WORD)Cmd::Exit},
    {FCONTROL | FVIRTKEY, 'W', (WORD)Cmd::Close},
    {FCONTROL | FVIRTKEY, 'Y', (WORD)Cmd::ZoomCustom},
    {FCONTROL | FVIRTKEY, '0', (WORD)IDM_ZOOM_FIT_PAGE},
    {FCONTROL | FVIRTKEY, VK_NUMPAD0, (WORD)IDM_ZOOM_FIT_PAGE},
    {FCONTROL | FVIRTKEY, '1', (WORD)IDM_ZOOM_ACTUAL_SIZE},
    {FCONTROL | FVIRTKEY, VK_NUMPAD1, (WORD)IDM_ZOOM_ACTUAL_SIZE},
    {FCONTROL | FVIRTKEY, '2', (WORD)IDM_ZOOM_FIT_WIDTH},
    {FCONTROL | FVIRTKEY, VK_NUMPAD2, (WORD)IDM_ZOOM_FIT_WIDTH},
    {FCONTROL | FVIRTKEY, '3', (WORD)IDM_ZOOM_FIT_CONTENT},
    {FCONTROL | FVIRTKEY, VK_NUMPAD3, (WORD)IDM_ZOOM_FIT_CONTENT},
    {FCONTROL | FVIRTKEY, '6', (WORD)Cmd::ViewSinglePage},
    {FCONTROL | FVIRTKEY, VK_NUMPAD6, (WORD)Cmd::ViewSinglePage},
    {FCONTROL | FVIRTKEY, '7', (WORD)Cmd::ViewFacing},
    {FCONTROL | FVIRTKEY, VK_NUMPAD7, (WORD)Cmd::ViewFacing},
    {FCONTROL | FVIRTKEY, '8', (WORD)Cmd::ViewBook},
    {FCONTROL | FVIRTKEY, VK_NUMPAD8, (WORD)Cmd::ViewBook},
    {FCONTROL | FVIRTKEY, VK_ADD, (WORD)IDT_VIEW_ZOOMIN},
    {FSHIFT | FCONTROL | FVIRTKEY, VK_ADD, (WORD)Cmd::ViewRotateRight},
    {FCONTROL | FVIRTKEY, VK_OEM_PLUS, (WORD)IDT_VIEW_ZOOMIN},
    {FSHIFT | FCONTROL | FVIRTKEY, VK_OEM_PLUS, (WORD)Cmd::ViewRotateRight},
    {FCONTROL | FVIRTKEY, VK_INSERT, (WORD)Cmd::CopySelection},
    {FVIRTKEY, VK_F2, (WORD)IDM_RENAME_FILE},
    {FVIRTKEY, VK_F3, (WORD)IDM_FIND_NEXT},
    {FSHIFT | FVIRTKEY, VK_F3, (WORD)IDM_FIND_PREV},
    {FCONTROL | FVIRTKEY, VK_F3, (WORD)IDM_FIND_NEXT_SEL},
    {FSHIFT | FCONTROL | FVIRTKEY, VK_F3, (WORD)IDM_FIND_PREV_SEL},
    {FCONTROL | FVIRTKEY, VK_F4, (WORD)Cmd::Close},
    {FVIRTKEY, VK_F5, (WORD)Cmd::ViewPresentationMode},
    {FVIRTKEY, VK_F6, (WORD)IDM_MOVE_FRAME_FOCUS},
    {FVIRTKEY, VK_F8, (WORD)Cmd::ViewShowHideToolbar},

    {FVIRTKEY, VK_F9, (WORD)Cmd::ViewShowHideMenuBar},
    {FVIRTKEY, VK_F11, (WORD)Cmd::ViewFullScreen},
    {FSHIFT | FVIRTKEY, VK_F11, (WORD)Cmd::ViewPresentationMode},
    {FVIRTKEY, VK_F12, (WORD)Cmd::ViewBookmarks},
    {FCONTROL | FVIRTKEY, VK_SUBTRACT, (WORD)IDT_VIEW_ZOOMOUT},
    {FSHIFT | FCONTROL | FVIRTKEY, VK_SUBTRACT, (WORD)Cmd::ViewRotateLeft},
    {FCONTROL | FVIRTKEY, VK_OEM_MINUS, (WORD)IDT_VIEW_ZOOMOUT},
    {FSHIFT | FCONTROL | FVIRTKEY, VK_OEM_MINUS, (WORD)Cmd::ViewRotateLeft},
    {FALT | FVIRTKEY, VK_LEFT, (WORD)IDM_GOTO_NAV_BACK},
    {FALT | FVIRTKEY, VK_RIGHT, (WORD)IDM_GOTO_NAV_FORWARD},
};

HACCEL CreateSumatraAcceleratorTable() {
    int n = (int)dimof(gAccelerators);
    HACCEL res = CreateAcceleratorTableW(gAccelerators, n);
    CrashIf(res == nullptr);
    return res;
}
