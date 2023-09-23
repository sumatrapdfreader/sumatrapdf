/*----------------------------------------------------------------------
Copyright (c)  Gipsysoft. All Rights Reserved.
Web site: http://gipsysoft.com

This software is provided 'as-is', without any express or implied warranty.

In no event will the author be held liable for any damages arising from the
use of this software.

Permission is granted to anyone to use this software for any purpose, including
commercial applications, and to alter it and redistribute it freely, subject
to the following restrictions:

1) The origin of this software must not be misrepresented; you must not claim
   that you wrote the original software. If you use this software in a product,
         an acknowledgment in the product documentation is requested but not required.
2) Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software. Altered source is encouraged
         to be submitted back to the original author so it can be shared with the
         community. Please share your changes.
3) This notice may not be removed or altered from any source distribution.

File:	DialogSizer.h
Owner:	russf@gipsysoft.com
Purpose:	Main include file for sizeable dialogs API
----------------------------------------------------------------------*/

//	Predefined sizing information
#define DS_MoveX 1
#define DS_MoveY 2
#define DS_SizeX 4
#define DS_SizeY 8

typedef struct DialogSizerSizingItem {
    uint uControlID;
    uint uSizeInfo;
} DialogSizerSizingItem;

#define DIALOG_SIZER_START(name) const DialogSizerSizingItem name[] = {
#define DIALOG_SIZER_ENTRY(controlID, flags) {controlID, flags},
#define DIALOG_SIZER_END()     \
    { 0xFFFFFFFF, 0xFFFFFFFF } \
    }                          \
    ;

// SumatraPDF: removed the fourth argument and fixed the comment:
//	Set a window as sizeable, passing the sizing data for each control.
extern "C" BOOL DialogSizer_Set(HWND hwnd, const DialogSizerSizingItem* psd, BOOL bShowSizingGrip);
