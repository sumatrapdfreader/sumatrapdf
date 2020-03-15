/* Cockos SWELL (Simple/Small Win32 Emulation Layer for Linux/OSX)
   Copyright (C) 2006 and later, Cockos, Inc.

    This software is provided 'as-is', without any express or implied
    warranty.  In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
       claim that you wrote the original software. If you use this software
       in a product, an acknowledgment in the product documentation would be
       appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
       misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.
*/
  
#ifndef _SWELL_INTERNAL_H_
#define _SWELL_INTERNAL_H_

#include "../ptrlist.h"

class SWELL_ListView_Row
{
public:
  SWELL_ListView_Row() : m_param(0), m_imageidx(0), m_tmp(0) { }
  ~SWELL_ListView_Row() { m_vals.Empty(true,free); }
  WDL_PtrList<char> m_vals;

  LPARAM m_param;
  int m_imageidx;
  int m_tmp; // Cocoa uses this temporarily, generic uses it as a mask (1= selected)
};

struct HTREEITEM__;

#ifdef SWELL_TARGET_OSX

#if 0
  // at some point we should enable this and use it in most SWELL APIs that call Cocoa code...
  #define SWELL_BEGIN_TRY @try { 
  #define SWELL_END_TRY(x) } @catch (NSException *ex) { NSLog(@"SWELL exception in %s:%d :: %@:%@\n",__FILE__,__LINE__,[ex name], [ex reason]); x }
#else
  #define SWELL_BEGIN_TRY
  #define SWELL_END_TRY(x)
#endif

#define __SWELL_PREFIX_CLASSNAME3(a,b) a##b
#define __SWELL_PREFIX_CLASSNAME2(a,b) __SWELL_PREFIX_CLASSNAME3(a,b)
#define __SWELL_PREFIX_CLASSNAME(cname) __SWELL_PREFIX_CLASSNAME2(SWELL_APP_PREFIX,cname)

// this defines interfaces to internal swell classes
#define SWELL_hwndChild __SWELL_PREFIX_CLASSNAME(_hwnd) 
#define SWELL_hwndCarbonHost __SWELL_PREFIX_CLASSNAME(_hwndcarbonhost)

#define SWELL_ModelessWindow __SWELL_PREFIX_CLASSNAME(_modelesswindow)
#define SWELL_ModalDialog __SWELL_PREFIX_CLASSNAME(_dialogbox)

#define SWELL_TextField __SWELL_PREFIX_CLASSNAME(_textfield)
#define SWELL_ListView __SWELL_PREFIX_CLASSNAME(_listview)
#define SWELL_TreeView __SWELL_PREFIX_CLASSNAME(_treeview)
#define SWELL_TabView __SWELL_PREFIX_CLASSNAME(_tabview)
#define SWELL_ProgressView  __SWELL_PREFIX_CLASSNAME(_progind)
#define SWELL_TextView  __SWELL_PREFIX_CLASSNAME(_textview)
#define SWELL_BoxView __SWELL_PREFIX_CLASSNAME(_box)
#define SWELL_Button __SWELL_PREFIX_CLASSNAME(_button)
#define SWELL_PopUpButton __SWELL_PREFIX_CLASSNAME(_pub)
#define SWELL_ComboBox __SWELL_PREFIX_CLASSNAME(_cbox)

#define SWELL_StatusCell __SWELL_PREFIX_CLASSNAME(_statuscell)
#define SWELL_ListViewCell __SWELL_PREFIX_CLASSNAME(_listviewcell)
#define SWELL_ODListViewCell __SWELL_PREFIX_CLASSNAME(_ODlistviewcell)
#define SWELL_ODButtonCell __SWELL_PREFIX_CLASSNAME(_ODbuttoncell)
#define SWELL_ImageButtonCell __SWELL_PREFIX_CLASSNAME(_imgbuttoncell)

#define SWELL_FocusRectWnd __SWELL_PREFIX_CLASSNAME(_drawfocusrectwnd)

#define SWELL_DataHold __SWELL_PREFIX_CLASSNAME(_sdh)
#define SWELL_ThreadTmp __SWELL_PREFIX_CLASSNAME(_thread)
#define SWELL_PopupMenuRecv __SWELL_PREFIX_CLASSNAME(_trackpopupmenurecv)

#define SWELL_TimerFuncTarget __SWELL_PREFIX_CLASSNAME(_tft)


#define SWELL_Menu __SWELL_PREFIX_CLASSNAME(_menu)

#ifdef __OBJC__


#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
typedef int NSInteger;
typedef unsigned int NSUInteger;
#endif

@interface SWELL_Menu : NSMenu
{
}
-(void)dealloc;
- (id)copyWithZone:(NSZone *)zone;
@end

@interface SWELL_DataHold : NSObject
{
  void *m_data;
}
-(id) initWithVal:(void *)val;
-(void *) getValue;
@end

@interface SWELL_TimerFuncTarget : NSObject
{
  TIMERPROC m_cb;
  HWND m_hwnd;
  UINT_PTR m_timerid;
}
-(id) initWithId:(UINT_PTR)tid hwnd:(HWND)h callback:(TIMERPROC)cb;
-(void)SWELL_Timer:(id)sender;
@end

typedef struct OwnedWindowListRec
{
  NSWindow *hwnd;
  struct OwnedWindowListRec *_next;
} OwnedWindowListRec;

typedef struct WindowPropRec
{
  char *name; // either <64k or a strdup'd name
  void *data;
  struct WindowPropRec *_next;
} WindowPropRec;



@interface SWELL_TextField : NSTextField
{
  @public
  bool m_last_dark_mode;
  bool m_ctlcolor_set;
}
- (void)setNeedsDisplay:(BOOL)flag;
- (void)setNeedsDisplayInRect:(NSRect)rect;
- (void)drawRect:(NSRect)rect;
- (void)initColors:(int)darkmode; // -1 to not update darkmode but trigger update of colors
@end

@interface SWELL_TabView : NSTabView
{
  NSInteger m_tag;
  id m_dest;
}
@end

@interface SWELL_ProgressView : NSProgressIndicator
{
  NSInteger m_tag;
}
@end

@interface SWELL_ListViewCell : NSTextFieldCell
{
}
@end

@interface SWELL_StatusCell : NSTextFieldCell
{
  NSImage *status;
}
@end

@interface SWELL_TreeView : NSOutlineView
{
  @public
  bool m_fakerightmouse;
  LONG style;
  WDL_PtrList<HTREEITEM__> *m_items;
  NSColor *m_fgColor;
  NSMutableArray *m_selColors;
}
@end

@interface SWELL_ListView : NSTableView
{
  int m_leftmousemovecnt;
  bool m_fakerightmouse;
  @public
  LONG style;
  int ownermode_cnt;
  int m_start_item;
  int m_start_subitem;
  int m_start_item_clickmode;

  int m_lbMode;
  WDL_PtrList<SWELL_ListView_Row> *m_items;
  WDL_PtrList<NSTableColumn> *m_cols;
  WDL_PtrList<HGDIOBJ__> *m_status_imagelist;
  int m_status_imagelist_type;
  int m_fastClickMask;	
  NSColor *m_fgColor;
  NSMutableArray *m_selColors;

  // these are for the new yosemite mouse handling code
  int m_last_plainly_clicked_item, m_last_shift_clicked_item;

}
-(LONG)getSwellStyle;
-(void)setSwellStyle:(LONG)st;
-(int)getSwellNotificationMode;
-(void)setSwellNotificationMode:(int)lbMode;
-(NSInteger)columnAtPoint:(NSPoint)pt;
-(int)getColumnPos:(int)idx; // get current position of column that was originally at idx
-(int)getColumnIdx:(int)pos; // get original index of column that is currently at position
@end

@interface SWELL_ImageButtonCell : NSButtonCell
{
}
- (NSRect)drawTitle:(NSAttributedString *)title withFrame:(NSRect)frame inView:(NSView *)controlView;
@end

@interface SWELL_ODButtonCell : NSButtonCell
{
}
@end

@interface SWELL_ODListViewCell : NSCell
{
  SWELL_ListView *m_ownctl;
  int m_lastidx;
}
-(void)setOwnerControl:(SWELL_ListView *)t;
-(void)setItemIdx:(int)idx;
@end

@interface SWELL_Button : NSButton
{
  void *m_swellGDIimage;
  LONG_PTR m_userdata;
  int m_radioflags;
}
-(int)swellGetRadioFlags;
-(void)swellSetRadioFlags:(int)f;
-(LONG_PTR)getSwellUserData;
-(void)setSwellUserData:(LONG_PTR)val;
-(void)setSwellGDIImage:(void *)par;
-(void *)getSwellGDIImage;
@end

@interface SWELL_TextView : NSTextView
{
  NSInteger m_tag;
}
-(NSInteger) tag;
-(void) setTag:(NSInteger)tag;
@end

@interface SWELL_BoxView : NSBox
{
  NSInteger m_tag;
}
-(NSInteger) tag;
-(void) setTag:(NSInteger)tag;
@end

@interface SWELL_FocusRectWnd : NSView
{
}
@end

@interface SWELL_ThreadTmp : NSObject
{
@public
  void *a, *b;
}
-(void)bla:(id)obj;
@end



@interface SWELL_hwndChild : NSView // <NSDraggingSource>
{
@public
  int m_enabled; // -1 if preventing focus
  DLGPROC m_dlgproc;
  WNDPROC m_wndproc;
  LONG_PTR m_userdata;
  LONG_PTR m_extradata[32];
  NSInteger m_tag;
  int m_isfakerightmouse;
  char m_hashaddestroy; // 2 = WM_DESTROY has finished completely
  HMENU m_menu;
  BOOL m_flip;
  bool m_supports_ddrop;
  bool m_paintctx_used;
  HDC m_paintctx_hdc;
  WindowPropRec *m_props;
  NSRect m_paintctx_rect;
  BOOL m_isopaque;
  char m_titlestr[1024];
  unsigned int m_create_windowflags;
  NSOpenGLContext *m_glctx;
  char m_isdirty; // &1=self needs redraw, &2=children may need redraw
  char m_allow_nomiddleman;
  id m_lastTopLevelOwner; // save a copy of the owner, if any
  id m_access_cacheptrs[6];
  const char *m_classname;

#ifndef SWELL_NO_METAL
  char m_use_metal; // 1=normal mode, 2=full pipeline (GetDC() etc support). -1 is for non-metal async layered mode. -2 for non-metal non-async layered

  // metal state (if used)
  char m_metal_dc_dirty;  // used to track state during paint or getdc/releasedc. set to 1 if dirty, 2 if GetDC() but no write yet
  char m_metal_gravity; // &1=resizing left, &2=resizing top
  bool m_metal_retina; // last-retina-state, triggered to true by StretchBlt() with a 2:1 ratio

  bool m_metal_in_needref_list;
  RECT m_metal_in_needref_rect; 
  NSRect m_metal_lastframe;

  id m_metal_texture; // id<MTLTexture> -- owned if in full pipeline mode, otherwise reference to m_metal_drawable
  id m_metal_pipelineState; // id<MTLRenderPipelineState> -- only used in full pipeline mode
  id m_metal_commandQueue; // id<MTLCommandQueue> -- only used in full pipeline mode
  id m_metal_drawable; // id<CAMetalDrawable> -- only used in normal mode
  id m_metal_device; // id<MTLDevice> -- set to last-used-device
  DWORD m_metal_device_lastchkt;
#endif

}
- (id)initChild:(SWELL_DialogResourceIndex *)resstate Parent:(NSView *)parent dlgProc:(DLGPROC)dlgproc Param:(LPARAM)par;
- (LRESULT)onSwellMessage:(UINT)msg p1:(WPARAM)wParam p2:(LPARAM)lParam;
-(HANDLE)swellExtendedDragOp:(id <NSDraggingInfo>)sender retGlob:(BOOL)retG;
- (const char *)onSwellGetText;
-(void)onSwellSetText:(const char *)buf;
-(LONG)swellGetExtendedStyle;
-(void)swellSetExtendedStyle:(LONG)st;
-(HMENU)swellGetMenu;
-(BOOL)swellHasBeenDestroyed;
-(void)swellSetMenu:(HMENU)menu;
-(LONG_PTR)getSwellUserData;
-(void)setSwellUserData:(LONG_PTR)val;
-(void)setOpaque:(bool)isOpaque;
-(LPARAM)getSwellExtraData:(int)idx;
-(void)setSwellExtraData:(int)idx value:(LPARAM)val;
-(void)setSwellWindowProc:(WNDPROC)val;
-(WNDPROC)getSwellWindowProc;
-(void)setSwellDialogProc:(DLGPROC)val;
-(DLGPROC)getSwellDialogProc;

- (NSArray*) namesOfPromisedFilesDroppedAtDestination:(NSURL*)droplocation;

-(void) getSwellPaintInfo:(PAINTSTRUCT *)ps;
- (int)swellCapChangeNotify;
-(unsigned int)swellCreateWindowFlags;

-(bool)swellCanPostMessage;
-(int)swellEnumProps:(PROPENUMPROCEX)proc lp:(LPARAM)lParam;
-(void *)swellGetProp:(const char *)name wantRemove:(BOOL)rem;
-(int)swellSetProp:(const char *)name value:(void *)val ;
-(NSOpenGLContext *)swellGetGLContext;
- (void) setEnabledSwellNoFocus;
-(const char *)getSwellClass;

// NSAccessibility

// attribute methods
//- (NSArray *)accessibilityAttributeNames;
- (id)accessibilityAttributeValue:(NSString *)attribute;
//- (BOOL)accessibilityIsAttributeSettable:(NSString *)attribute;
//- (void)accessibilitySetValue:(id)value forAttribute:(NSString *)attribute;

// parameterized attribute methods
//- (NSArray *)accessibilityParameterizedAttributeNames;
//- (id)accessibilityAttributeValue:(NSString *)attribute forParameter:(id)parameter;

// action methods
//- (NSArray *)accessibilityActionNames;
//- (NSString *)accessibilityActionDescription:(NSString *)action;
//- (void)accessibilityPerformAction:(NSString *)action;

// Return YES if the UIElement doesn't show up to the outside world - i.e. its parent should return the UIElement's children as its own - cutting the UIElement out. E.g. NSControls are ignored when they are single-celled.
- (BOOL)accessibilityIsIgnored;

// Returns the deepest descendant of the UIElement hierarchy that contains the point. You can assume the point has already been determined to lie within the receiver. Override this method to do deeper hit testing within a UIElement - e.g. a NSMatrix would test its cells. The point is bottom-left relative screen coordinates.
- (id)accessibilityHitTest:(NSPoint)point;

// Returns the UI Element that has the focus. You can assume that the search for the focus has already been narrowed down to the reciever. Override this method to do a deeper search with a UIElement - e.g. a NSMatrix would determine if one of its cells has the focus.
- (id)accessibilityFocusedUIElement;

-(void) swellOnControlDoubleClick:(id)sender;

#ifdef MAC_OS_X_VERSION_10_8
// for radio button with the OSX 10.8+ SDK, see comment in SWELL_MakeControl
-(void) onSwellCommand0:(id)sender;
-(void) onSwellCommand2:(id)sender;
-(void) onSwellCommand3:(id)sender;
-(void) onSwellCommand4:(id)sender;
-(void) onSwellCommand5:(id)sender;
-(void) onSwellCommand6:(id)sender;
-(void) onSwellCommand7:(id)sender;
#endif

#ifndef SWELL_NO_METAL
-(BOOL) swellWantsMetal;
-(void) swellDrawMetal:(const RECT *)forRect;
#endif
@end

@interface SWELL_ModelessWindow : NSWindow
{
@public
  NSSize lastFrameSize;
  id m_owner;
  OwnedWindowListRec *m_ownedwnds;
  BOOL m_enabled;
  int m_wantraiseamt;
  bool  m_wantInitialKeyWindowOnShow;
}
- (id)initModeless:(SWELL_DialogResourceIndex *)resstate Parent:(HWND)parent dlgProc:(DLGPROC)dlgproc Param:(LPARAM)par outputHwnd:(HWND *)hwndOut forceStyles:(unsigned int)smask;
- (id)initModelessForChild:(HWND)child owner:(HWND)owner styleMask:(unsigned int)smask;
- (void)swellDestroyAllOwnedWindows;
- (void)swellRemoveOwnedWindow:(NSWindow *)wnd;
- (void)swellSetOwner:(id)owner;
- (id)swellGetOwner;
- (void **)swellGetOwnerWindowHead;
-(void)swellDoDestroyStuff;
-(void)swellResetOwnedWindowLevels;
@end

@interface SWELL_ModalDialog : NSPanel
{
  NSSize lastFrameSize;
  id m_owner;
  OwnedWindowListRec *m_ownedwnds;
  
  int m_rv;
  bool m_hasrv;
  BOOL m_enabled;
}
- (id)initDialogBox:(SWELL_DialogResourceIndex *)resstate Parent:(HWND)parent dlgProc:(DLGPROC)dlgproc Param:(LPARAM)par;
- (void)swellDestroyAllOwnedWindows;
- (void)swellRemoveOwnedWindow:(NSWindow *)wnd;
- (void)swellSetOwner:(id)owner;
- (id)swellGetOwner;
- (void **)swellGetOwnerWindowHead;
-(void)swellDoDestroyStuff;

-(void)swellSetModalRetVal:(int)r;
-(int)swellGetModalRetVal;
-(bool)swellHasModalRetVal;
@end

#ifndef SWELL_NO_METAL
void swell_removeMetalDirty(SWELL_hwndChild *slf);
void swell_updateAllMetalDirty(void);
void swell_addMetalDirty(SWELL_hwndChild *slf, const RECT *r, bool isReleaseDC=false);
HDC SWELL_CreateMetalDC(SWELL_hwndChild *);
#endif


@interface SWELL_hwndCarbonHost : SWELL_hwndChild
#ifdef MAC_OS_X_VERSION_10_7
<NSWindowDelegate>
#endif
{
@public
  NSWindow *m_cwnd;

  bool m_whileresizing;
  void* m_wndhandler;   // won't compile if declared EventHandlerRef, wtf
  void* m_ctlhandler;   // not sure if these need to be separate but cant hurt  
  bool m_wantallkeys;
}
-(BOOL)swellIsCarbonHostingView;
-(void)swellDoRepos;
@end


@interface SWELL_PopupMenuRecv : NSObject
{
  int m_act;
  HWND cbwnd;
}
-(id) initWithWnd:(HWND)wnd;
-(void) onSwellCommand:(id)sender;
-(int) isCommand;
- (void)menuNeedsUpdate:(NSMenu *)menu;

@end

@interface SWELL_PopUpButton : NSPopUpButton
{
  LONG m_style;
}
-(void)setSwellStyle:(LONG)style;
-(LONG)getSwellStyle;
@end

@interface SWELL_ComboBox : NSComboBox
{
@public
  LONG m_style;
  WDL_PtrList<char> *m_ids;
}
-(id)init;
-(void)dealloc;
-(void)setSwellStyle:(LONG)style;
-(LONG)getSwellStyle;
@end



// GDI internals

#ifndef __AVAILABILITYMACROS__
#error  __AVAILABILITYMACROS__ not defined, include AvailabilityMacros.h!
#endif

// 10.4 doesn't support CoreText, so allow ATSUI if targetting 10.4 SDK
#ifndef MAC_OS_X_VERSION_10_5
  // 10.4 SDK
  #define SWELL_NO_CORETEXT
  #define SWELL_ATSUI_TEXT_SUPPORT
#elif !defined(__LP64__)
  #if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5
    #ifndef MAC_OS_X_VERSION_10_9 // not sure when ATSUI was dropped completely, definitely gone in 10.13!
      #define SWELL_ATSUI_TEXT_SUPPORT
    #endif
  #endif
#endif

struct HGDIOBJ__
{
  int type;

  int additional_refcnt; // refcnt of 0 means one owner (if >0, additional owners)
  
  // used by pen/brush
  CGColorRef color;
  int wid;
  int color_int;
  NSImage *bitmapptr;  
  
  NSMutableDictionary *__old_fontdict; // unused, for ABI compat
  //
  // if ATSUI used, meaning IsCoreTextSupported() returned false
  ATSUStyle atsui_font_style;

  float font_rotation;

  bool _infreelist;
  struct HGDIOBJ__ *_next;
 
  // if using CoreText to draw text
  void *ct_FontRef;
};

struct HDC__ {
  CGContextRef ctx; 
  void *ownedData; // always use via SWELL_GetContextFrameBuffer() (which performs necessary alignment)
#ifndef SWELL_NO_METAL
  void *metal_ctx; // SWELL_hwndChild
#endif
  HGDIOBJ__ *curpen;
  HGDIOBJ__ *curbrush;
  HGDIOBJ__ *curfont;
  
  NSColor *__old_nstextcol; // provided for ABI compat, but unused
  int cur_text_color_int; // text color as int
  
  int curbkcol;
  int curbkmode;
  float lastpos_x,lastpos_y;
  
  void *GLgfxctx; // optionally set
  bool _infreelist;
  struct HDC__ *_next;

  CGColorRef curtextcol; // text color as CGColor
};





// some extras so we can call functions available only on some OSX versions without warnings, and with the correct types
#define SWELL_DelegateExtensions __SWELL_PREFIX_CLASSNAME(_delext)
#define SWELL_ViewExtensions __SWELL_PREFIX_CLASSNAME(_viewext)
#define SWELL_AppExtensions __SWELL_PREFIX_CLASSNAME(_appext)
#define SWELL_WindowExtensions __SWELL_PREFIX_CLASSNAME(_wndext)
#define SWELL_TableColumnExtensions __SWELL_PREFIX_CLASSNAME(_tcolext)

@interface SWELL_WindowExtensions : NSWindow
-(void)setCollectionBehavior:(NSUInteger)a;
@end
@interface SWELL_ViewExtensions : NSView
-(void)_recursiveDisplayRectIfNeededIgnoringOpacity:(NSRect)rect isVisibleRect:(BOOL)vr rectIsVisibleRectForView:(NSView*)v topView:(NSView *)v2;
@end

@interface SWELL_DelegateExtensions : NSObject
-(bool)swellPostMessage:(HWND)dest msg:(int)message wp:(WPARAM)wParam lp:(LPARAM)lParam;
-(void)swellPostMessageClearQ:(HWND)dest;
-(void)swellPostMessageTick:(id)sender;
@end

@interface SWELL_AppExtensions : NSApplication
-(NSUInteger)presentationOptions;
-(void)setPresentationOptions:(NSUInteger)o;
@end
@interface SWELL_TableColumnExtensions : NSTableColumn
-(BOOL)isHidden;
-(void)setHidden:(BOOL)h;
@end




#else
  // compat when compiling targetting OSX but not in objectiveC mode
  struct SWELL_DataHold;
#endif // !__OBJC__

// 10.4 sdk just uses "float"
#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
  #ifdef __LP64__
    typedef double CGFloat;
  #else
    typedef float CGFloat;
#endif

#endif


#define NSPOINT_TO_INTS(pt) (int)floor((pt).x+0.5), (int)floor((pt).y+0.5)

#ifdef __OBJC__
static WDL_STATICFUNC_UNUSED void NSPOINT_TO_POINT(POINT *p, const NSPoint &pt)
{
  p->x = (int)floor(pt.x+0.5);
  p->y = (int)floor((pt).y+0.5);
}
static WDL_STATICFUNC_UNUSED void NSRECT_TO_RECT(RECT *r, const NSRect &tr)
{
  r->left=(int)floor(tr.origin.x+0.5);
  r->right=(int)floor(tr.origin.x+tr.size.width+0.5);
  r->top=(int)floor(tr.origin.y+0.5);
  r->bottom=(int)floor(tr.origin.y+tr.size.height+0.5);
}
#endif

#elif defined(SWELL_TARGET_GDK)


#ifdef SWELL_SUPPORT_GTK
#include <gtk/gtk.h>
#endif

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>


#else
// generic 

#endif // end generic

struct HTREEITEM__
{
  HTREEITEM__();
  ~HTREEITEM__();
  bool FindItem(HTREEITEM it, HTREEITEM__ **parOut, int *idxOut);
  
#ifdef SWELL_TARGET_OSX
  SWELL_DataHold *m_dh;
#else
  int m_state; // TVIS_EXPANDED, for ex
#endif
  
  bool m_haschildren;
  char *m_value;
  WDL_PtrList<HTREEITEM__> m_children; // only used in tree mode
  LPARAM m_param;
};


#ifndef SWELL_TARGET_OSX 

#include "../wdlstring.h"

#ifdef SWELL_LICE_GDI
#include "../lice/lice.h"
#endif
#include "../assocarray.h"

LRESULT SwellDialogDefaultWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


#ifdef SWELL_TARGET_GDK
typedef GdkWindow *SWELL_OSWINDOW;
#else
typedef void *SWELL_OSWINDOW; // maps to the HWND__ itself on visible, non-GDK, top level windows
#endif

struct HWND__
{
  HWND__(HWND par, int wID=0, RECT *wndr=NULL, const char *label=NULL, bool visible=false, WNDPROC wndproc=NULL, DLGPROC dlgproc=NULL, HWND ownerWindow=NULL);
  ~HWND__(); // DO NOT USE!!! We would make this private but it breaks PtrList using it on gcc. 

  // using this API prevents the HWND from being valid -- it'll still get its resources destroyed via DestroyWindow() though.
  // DestroyWindow() does cleanup, then the final Release().
  void Retain() { m_refcnt++; }
  void Release() { if (!--m_refcnt) delete this; }
 
  const char *m_classname;

  SWELL_OSWINDOW m_oswindow;

  WDL_FastString m_title;

  HWND__ *m_children, *m_parent, *m_next, *m_prev;
  HWND__ *m_owner, *m_owned_list, *m_owned_next, *m_owned_prev;
  HWND__ *m_focused_child; // only valid if hwnd itself is in focus chain, and must be validated before accessed
  RECT m_position;
  UINT m_id;
  int m_style, m_exstyle;
  INT_PTR m_userdata;
  WNDPROC m_wndproc;
  DLGPROC m_dlgproc;
  INT_PTR m_extra[64];
  INT_PTR m_private_data; // used by internal controls

  bool m_visible;
  char m_hashaddestroy; // 1 in destroy, 2 full destroy
  bool m_enabled;
  bool m_wantfocus;

  bool m_israised;
  bool m_has_had_position;
  bool m_oswindow_fullscreen;

  int m_refcnt; 
  int m_oswindow_private; // private state for generic-gtk or whatever

  HMENU m_menu;
  HFONT m_font;

  WDL_StringKeyedArray<void *> m_props;

#ifdef SWELL_LICE_GDI
  void *m_paintctx; // temporarily set for calls to WM_PAINT

  bool m_child_invalidated; // if a child is invalidated
  bool m_invalidated; // set to true on direct invalidate. todo RECT instead?

  LICE_IBitmap *m_backingstore; // if NULL, unused (probably only should use on top level windows, but support caching?)
#endif
};

struct HMENU__
{
  HMENU__() { m_refcnt=1; sel_vis = -1; }

  void Retain() { m_refcnt++; }
  void Release() { if (!--m_refcnt) delete this; }

  WDL_PtrList<MENUITEMINFO> items;
  int sel_vis; // for mouse/keyboard nav
  int m_refcnt;

  HMENU__ *Duplicate();
  static void freeMenuItem(void *p);

private:
  ~HMENU__() { items.Empty(true,freeMenuItem); }
};


struct HGDIOBJ__
{
  int type;
  int additional_refcnt; // refcnt of 0 means one owner (if >0, additional owners)

  int color;
  int wid;

  float alpha;

  struct HGDIOBJ__ *_next;
  bool _infreelist;
  void *typedata; // font: FT_Face, bitmap: LICE_IBitmap
};


struct HDC__ {
#ifdef SWELL_LICE_GDI
  LICE_IBitmap *surface; // owned by context. can be (and usually is, if clipping is desired) LICE_SubBitmap
  POINT surface_offs; // offset drawing into surface by this amount

  RECT dirty_rect; // in surface coordinates, used for GetWindowDC()/GetDC()/etc
  bool dirty_rect_valid;
#else
  void *ownedData; // for mem contexts, support a null rendering 
#endif

  HGDIOBJ__ *curpen;
  HGDIOBJ__ *curbrush;
  HGDIOBJ__ *curfont;
  
  int cur_text_color_int; // text color as int
  
  int curbkcol;
  int curbkmode;
  float lastpos_x,lastpos_y;
  
  struct HDC__ *_next;
  bool _infreelist;
};

HWND DialogBoxIsActive(void);
void DestroyPopupMenus(void);
HWND ChildWindowFromPoint(HWND h, POINT p);
HWND GetFocusIncludeMenus();

void SWELL_RunEvents();

bool swell_isOSwindowmenu(SWELL_OSWINDOW osw);

bool swell_is_virtkey_char(int c);

void swell_on_toplevel_raise(SWELL_OSWINDOW wnd); // called by swell-generic-gdk when a window is focused

HWND swell_oswindow_to_hwnd(SWELL_OSWINDOW w);
void swell_oswindow_focus(HWND hwnd);
void swell_oswindow_update_style(HWND hwnd, LONG oldstyle);
void swell_oswindow_update_enable(HWND hwnd);
void swell_oswindow_update_text(HWND hwnd);
void swell_oswindow_begin_resize(SWELL_OSWINDOW wnd);
void swell_oswindow_resize(SWELL_OSWINDOW wnd, int reposflag, RECT f);
void swell_oswindow_postresize(HWND hwnd, RECT f);
void swell_oswindow_invalidate(HWND hwnd, const RECT *r);
void swell_oswindow_destroy(HWND hwnd);
void swell_oswindow_manage(HWND hwnd, bool wantfocus);
void swell_oswindow_updatetoscreen(HWND hwnd, RECT *rect);
HWND swell_window_wants_all_input(); // window with an active drag of menubar will have this set, to route all mouse events to nonclient area of window
int swell_delegate_menu_message(HWND src, LPARAM lParam, int msg, bool screencoords); // menubar/menus delegate to submenus during drag.

void swell_dlg_destroyspare();

extern bool swell_is_likely_capslock; // only used when processing dit events for a-zA-Z
extern const char *g_swell_appname;
extern SWELL_OSWINDOW SWELL_focused_oswindow; // top level window which has focus (might not map to a HWND__!)
extern HWND swell_captured_window;
extern HWND SWELL_topwindows; // front of list = most recently active
extern bool swell_app_is_inactive;

#ifdef _DEBUG
void VALIDATE_HWND_LIST(HWND list, HWND par);
#else
#define VALIDATE_HWND_LIST(list, par) do { } while (0)
#endif


#endif // !OSX

HDC SWELL_CreateGfxContext(void *);

// GDP internals
#define TYPE_PEN 1
#define TYPE_BRUSH 2
#define TYPE_FONT 3
#define TYPE_BITMAP 4

typedef struct
{
  void *instptr; 
#ifdef __APPLE__
  void *bundleinstptr;
#endif
  int refcnt;

#ifndef SWELL_EXTRA_MINIMAL
  int (*SWELL_dllMain)(HINSTANCE, DWORD,LPVOID); //last parm=SWELLAPI_GetFunc
  BOOL (*dllMain)(HINSTANCE, DWORD, LPVOID);
#endif
  void *lastSymbolRequested;
} SWELL_HINSTANCE;


enum
{
  INTERNAL_OBJECT_START= 0x1000001,
  INTERNAL_OBJECT_THREAD,
  INTERNAL_OBJECT_EVENT,
  INTERNAL_OBJECT_FILE,
  INTERNAL_OBJECT_EXTERNALSOCKET, // socket not owned by us
  INTERNAL_OBJECT_SOCKETEVENT,
  INTERNAL_OBJECT_NSTASK, 
  INTERNAL_OBJECT_PID,
  INTERNAL_OBJECT_END
};

typedef struct
{
   int type; // INTERNAL_OBJECT_*
   int count; // reference count
} SWELL_InternalObjectHeader;

typedef struct
{
  SWELL_InternalObjectHeader hdr;
  DWORD (*threadProc)(LPVOID);
  void *threadParm;
  pthread_t pt;
  DWORD retv;
  bool done;
} SWELL_InternalObjectHeader_Thread;

typedef struct
{
  SWELL_InternalObjectHeader hdr;
  
  pthread_mutex_t mutex;
  pthread_cond_t cond;

  bool isSignal;
  bool isManualReset;
  
} SWELL_InternalObjectHeader_Event;


// used for both INTERNAL_OBJECT_EXTERNALSOCKET and INTERNAL_OBJECT_SOCKETEVENT. 
// if EXTERNALSOCKET, socket[1] ignored and autoReset ignored.
typedef struct
{
  SWELL_InternalObjectHeader hdr;
  int socket[2]; 
  bool autoReset;
} SWELL_InternalObjectHeader_SocketEvent;
 
typedef struct
{
  SWELL_InternalObjectHeader hdr;
  
  FILE *fp;
} SWELL_InternalObjectHeader_File;

typedef struct
{
  SWELL_InternalObjectHeader hdr;
  void *task; 
} SWELL_InternalObjectHeader_NSTask;

typedef struct
{
  SWELL_InternalObjectHeader hdr;
  int pid;
  int done, result;
} SWELL_InternalObjectHeader_PID;

bool IsRightClickEmulateEnabled();

#ifdef SWELL_INTERNAL_HTREEITEM_IMPL

HTREEITEM__::HTREEITEM__()
{
  m_param=0;
  m_value=0;
  m_haschildren=false;
#ifdef SWELL_TARGET_OSX
  m_dh = [[SWELL_DataHold alloc] initWithVal:this];
#else
  m_state=0;
#endif
}
HTREEITEM__::~HTREEITEM__()
{
  free(m_value);
  m_children.Empty(true);
#ifdef SWELL_TARGET_OSX
  [m_dh release];
#endif
}


bool HTREEITEM__::FindItem(HTREEITEM it, HTREEITEM__ **parOut, int *idxOut)
{
  int a=m_children.Find((HTREEITEM__*)it);
  if (a>=0)
  {
    if (parOut) *parOut=this;
    if (idxOut) *idxOut=a;
    return true;
  }
  int x;
  const int n=m_children.GetSize();
  for (x = 0; x < n; x ++)
  {
    if (m_children.Get(x)->FindItem(it,parOut,idxOut)) return true;
  }
  return false;
}

#endif

#ifdef SWELL_INTERNAL_MERGESORT_IMPL

static int __listview_sortfunc(void *p1, void *p2, int (*compar)(LPARAM val1, LPARAM val2, LPARAM userval), LPARAM userval)
{
  SWELL_ListView_Row *a = *(SWELL_ListView_Row **)p1;
  SWELL_ListView_Row *b = *(SWELL_ListView_Row **)p2;
  return compar(a->m_param,b->m_param,userval);
}


static void __listview_mergesort_internal(void *base, size_t nmemb, size_t size, 
                                 int (*compar)(LPARAM val1, LPARAM val2, LPARAM userval), 
                                 LPARAM parm,
                                 char *tmpspace)
{
  char *b1,*b2;
  size_t n1, n2;

  if (nmemb < 2) return;

  n1 = nmemb / 2;
  b1 = (char *) base;
  n2 = nmemb - n1;
  b2 = b1 + (n1 * size);

  if (nmemb>2)
  {
    __listview_mergesort_internal(b1, n1, size, compar, parm, tmpspace);
    __listview_mergesort_internal(b2, n2, size, compar, parm, tmpspace);
  }

  char *p = tmpspace;

  do
  {
	  if (__listview_sortfunc(b1, b2, compar,parm) > 0)
	  {
	    memcpy(p, b2, size);
	    b2 += size;
	    n2--;
	  }
	  else
	  {
	    memcpy(p, b1, size);
	    b1 += size;
	    n1--;
	  }
  	p += size;
  }
  while (n1 > 0 && n2 > 0);

  if (n1 > 0) memcpy(p, b1, n1 * size);
  memcpy(base, tmpspace, (nmemb - n2) * size);
}


#endif

#ifndef SWELL_TARGET_OSX

#define SWELL_GENERIC_THEMESIZEDEFS(f,fd) \
  f(default_font_size, 12) \
  f(menubar_height, 17) \
  f(menubar_font_size, 13) \
  f(menubar_spacing_width, 8) \
  f(menubar_margin_width, 6) \
  f(scrollbar_width, 14) \
  f(smscrollbar_width, 16) \
  f(scrollbar_min_thumb_height, 4) \
  f(combo_height, 20) \


#define SWELL_GENERIC_THEMEDEFS(f,fd) \
  SWELL_GENERIC_THEMESIZEDEFS(f,fd) \
  f(_3dface,RGB(192,192,192)) \
  f(_3dshadow,RGB(96,96,96)) \
  f(_3dhilight,RGB(224,224,224)) \
  f(_3ddkshadow,RGB(48,48,48)) \
  fd(button_bg,RGB(192,192,192),_3dface) \
  f(button_text,RGB(0,0,0)) \
  f(button_text_disabled, RGB(128,128,128)) \
  fd(button_shadow, RGB(96,96,96), _3dshadow) \
  fd(button_hilight, RGB(224,224,224), _3dhilight) \
  f(checkbox_text,RGB(0,0,0)) \
  f(checkbox_text_disabled, RGB(128,128,128)) \
  f(checkbox_fg, RGB(0,0,0)) \
  f(checkbox_inter, RGB(192,192,192)) \
  f(checkbox_bg, RGB(255,255,255)) \
  f(scrollbar,RGB(32,32,32)) \
  f(scrollbar_fg, RGB(160,160,160)) \
  f(scrollbar_bg, RGB(224,224,224)) \
  f(edit_cursor,RGB(0,128,255)) \
  f(edit_bg,RGB(255,255,255)) \
  f(edit_bg_disabled,RGB(224,224,224)) \
  f(edit_text,RGB(0,0,0)) \
  f(edit_text_disabled, RGB(128,128,128)) \
  f(edit_bg_sel,RGB(128,192,255)) \
  f(edit_text_sel,RGB(255,255,255)) \
  fd(edit_hilight, RGB(224,224,224), _3dhilight) \
  fd(edit_shadow, RGB(96,96,96), _3dshadow) \
  f(info_bk,RGB(255,240,200)) \
  f(info_text,RGB(0,0,0)) \
  fd(menu_bg, RGB(192,192,192), _3dface) \
  fd(menu_shadow, RGB(96,96,96), _3dshadow) \
  fd(menu_hilight, RGB(224,224,224), _3dhilight) \
  fd(menu_text, RGB(0,0,0), button_text) \
  fd(menu_text_disabled, RGB(224,224,224), _3dhilight) \
  fd(menu_bg_sel, RGB(0,0,0), menu_text) \
  fd(menu_text_sel, RGB(224,224,224), menu_bg) \
  f(menu_scroll, RGB(64,64,64)) \
  fd(menu_scroll_arrow, RGB(96,96,96), _3dshadow) \
  fd(menu_submenu_arrow, RGB(96,96,96), _3dshadow) \
  fd(menubar_bg, RGB(192,192,192), menu_bg) \
  fd(menubar_text, RGB(0,0,0), menu_text) \
  fd(menubar_text_disabled, RGB(224,224,224), menu_text_disabled) \
  fd(menubar_bg_sel, RGB(0,0,0), menu_bg_sel) \
  fd(menubar_text_sel, RGB(224,224,224), menu_text_sel) \
  f(trackbar_track, RGB(224,224,224)) \
  f(trackbar_mark, RGB(96,96,96)) \
  f(trackbar_knob, RGB(48,48,48)) \
  f(progress,RGB(0,128,255)) \
  fd(label_text, RGB(0,0,0), button_text) \
  fd(label_text_disabled, RGB(128,128,128), button_text_disabled) \
  fd(combo_text, RGB(0,0,0), button_text) \
  fd(combo_text_disabled, RGB(128,128,128), button_text_disabled) \
  fd(combo_bg, RGB(192,192,192), _3dface) \
  f(combo_bg2, RGB(255,255,255)) \
  fd(combo_shadow, RGB(96,96,96), _3dshadow) \
  fd(combo_hilight, RGB(224,224,224), _3dhilight) \
  fd(combo_arrow, RGB(96,96,96), _3dshadow) \
  fd(combo_arrow_press, RGB(224,224,224), _3dhilight) \
  f(listview_bg, RGB(255,255,255)) \
  f(listview_bg_sel, RGB(128,128, 255)) \
  f(listview_text, RGB(0,0,0)) \
  fd(listview_text_sel, RGB(0,0,0), listview_text) \
  fd(listview_grid, RGB(224,224,224), _3dhilight) \
  f(listview_hdr_arrow,RGB(96,96,96)) \
  fd(listview_hdr_shadow, RGB(96,96,96), _3dshadow) \
  fd(listview_hdr_hilight, RGB(224,224,224), _3dhilight) \
  fd(listview_hdr_bg, RGB(192,192,192), _3dface) \
  fd(listview_hdr_text, RGB(0,0,0), button_text) \
  f(treeview_text,RGB( 0,0,0)) \
  f(treeview_bg, RGB(255,255,255)) \
  f(treeview_bg_sel, RGB(128,128,255)) \
  f(treeview_text_sel, RGB(0,0,0)) \
  f(treeview_arrow, RGB(96,96,96)) \
  fd(tab_shadow, RGB(96,96,96), _3dshadow) \
  fd(tab_hilight, RGB(224,224,224), _3dhilight) \
  fd(tab_text, RGB(0,0,0), button_text) \
  f(focusrect,RGB(255,0,0)) \
  f(group_text,RGB(0,0,0)) \
  fd(group_shadow, RGB(96,96,96), _3dshadow) \
  fd(group_hilight, RGB(224,224,224), _3dhilight) \
  f(focus_hilight, RGB(140,190,233)) \

  

struct swell_colortheme {
#define __def_theme_ent(x,c) int x;
#define __def_theme_ent_fb(x,c,fb) int x;
SWELL_GENERIC_THEMEDEFS(__def_theme_ent,__def_theme_ent_fb)
#undef __def_theme_ent
#undef __def_theme_ent_fb
};

#define SWELL_UI_SCALE(x) (((x)*g_swell_ui_scale)/256)
void swell_scaling_init(bool no_auto_hidpi);
extern int g_swell_ui_scale;
extern swell_colortheme g_swell_ctheme;
extern const char *g_swell_deffont_face;

#endif

#endif
