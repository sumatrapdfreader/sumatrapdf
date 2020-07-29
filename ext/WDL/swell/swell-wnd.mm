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
  

    This file provides basic windows APIs for handling windows, as well as the stubs to enable swell-dlggen to work.

  */


#ifndef SWELL_PROVIDED_BY_APP

#import <Cocoa/Cocoa.h>
#import <objc/objc-runtime.h>
#include "swell.h"
#include "../mutex.h"
#include "../ptrlist.h"
#include "../queue.h"
#include "../wdlcstring.h"

#include "swell-dlggen.h"

#define SWELL_INTERNAL_MERGESORT_IMPL
#define SWELL_INTERNAL_HTREEITEM_IMPL
#include "swell-internal.h"

static bool SWELL_NeedModernListViewHacks()
{
#ifdef __LP64__
  return false;
#else
  // only needed on 32 bit yosemite as of 10.10.3, but who knows when it will be necessary elsewhere
  return SWELL_GetOSXVersion() >= 0x10a0;
#endif
}

static LRESULT sendSwellMessage(id obj, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  if (obj && [obj respondsToSelector:@selector(onSwellMessage:p1:p2:)])
    return [(SWELL_hwndChild *)obj onSwellMessage:uMsg p1:wParam p2:lParam];
  return 0;
}
static void InvalidateSuperViews(NSView *view);
#define STANDARD_CONTROL_NEEDSDISPLAY_IMPL(classname) \
  - (const char *)swellGetClass { return ( classname ); } \
  - (void)setNeedsDisplay:(BOOL)flag \
  { \
  [super setNeedsDisplay:flag]; \
  if (flag) InvalidateSuperViews(self); \
  } \
  - (void)setNeedsDisplayInRect:(NSRect)rect \
  { \
  [super setNeedsDisplayInRect:rect]; \
  InvalidateSuperViews(self); \
  }


int g_swell_osx_readonlytext_wndbg = 0;
int g_swell_want_nice_style = 1;
static void *SWELL_CStringToCFString_FilterPrefix(const char *str)
{
  int c=0;
  while (str[c] && str[c] != '&' && c++<1024);
  if (!str[c] || c>=1024 || strlen(str)>=1024) return SWELL_CStringToCFString(str);
  char buf[1500];
  const char *p=str;
  char *op=buf;
  while (*p)
  {
    if (*p == '&')  p++;
    if (!*p) break;
    *op++=*p++;
  }
  *op=0;
  return SWELL_CStringToCFString(buf);

}

static int _nsStringSearchProc(const void *_a, const void *_b)
{
  NSString *a=(NSString *)_a;
  NSString *b = (NSString *)_b;
  return (int)[a compare:b];
}
static int _nsMenuSearchProc(const void *_a, const void *_b)
{
  NSString *a=(NSString *)_a;
  NSMenuItem *b = (NSMenuItem *)_b;
  return (int)[a compare:[b title]];
}
static int _listviewrowSearchFunc(const void *_a, const void *_b, const void *ctx)
{
  const char *a = (const char *)_a;
  SWELL_ListView_Row *row = (SWELL_ListView_Row *)_b;
  const char *b="";
  if (!row || !(b=row->m_vals.Get(0))) b="";
  return strcmp(a,b);
}
static int _listviewrowSearchFunc2(const void *_a, const void *_b, const void *ctx)
{
  const char *a = (const char *)_a;
  SWELL_ListView_Row *row = (SWELL_ListView_Row *)_b;
  const char *b="";
  if (!row || !(b=row->m_vals.Get(0))) b="";
  return strcmp(b,a);
}

// modified bsearch: returns place item SHOULD be in if it's not found
static NSInteger arr_bsearch_mod(void *key, NSArray *arr, int (*compar)(const void *, const void *))
{
  const NSInteger nmemb = [arr count];
  NSInteger p,lim,base=0;
  
	for (lim = nmemb; lim != 0; lim >>= 1) {
		p = base + (lim >> 1);
		int cmp = compar(key, [arr objectAtIndex:p]);
		if (cmp == 0) return (p);
		if (cmp > 0) {	/* key > p: move right */
      // check to see if key is less than p+1, if it is, we're done
			base = p + 1;
      if (base >= nmemb || compar(key,[arr objectAtIndex:base])<=0) return base;
			lim--;
		} /* else move left */
	}
	return 0;
}


template<class T> static int ptrlist_bsearch_mod(void *key, WDL_PtrList<T> *arr, int (*compar)(const void *, const void *, const void *ctx), void *ctx)
{
  const int nmemb = arr->GetSize();
  int base=0, lim, p;
  
	for (lim = nmemb; lim != 0; lim >>= 1) {
		p = base + (lim >> 1);
		int cmp = compar(key, arr->Get(p),ctx);
		if (cmp == 0) return (p);
		if (cmp > 0) {	/* key > p: move right */
      // check to see if key is less than p+1, if it is, we're done
			base = p + 1;
      if (base >= nmemb || compar(key,arr->Get(base),ctx)<=0) return base;
			lim--;
		} /* else move left */
	}
	return 0;
}


@implementation SWELL_TabView
STANDARD_CONTROL_NEEDSDISPLAY_IMPL("SysTabControl32")

-(void)setNotificationWindow:(id)dest
{
  m_dest=dest;
}
-(id)getNotificationWindow
{
  return m_dest;
}
-(NSInteger) tag
{
  return m_tag;
}
-(void) setTag:(NSInteger)tag
{
  m_tag=tag;
}
- (void)tabView:(NSTabView *)tabView didSelectTabViewItem:(NSTabViewItem *)tabViewItem
{
  if (m_dest)
  {
    NMHDR nm={(HWND)self,(UINT_PTR)[self tag],TCN_SELCHANGE};
    SendMessage((HWND)m_dest,WM_NOTIFY,nm.idFrom,(LPARAM)&nm);
  }
}
@end


@implementation SWELL_ProgressView
STANDARD_CONTROL_NEEDSDISPLAY_IMPL("msctls_progress32")

-(NSInteger) tag
{
  return m_tag;
}
-(void) setTag:(NSInteger)tag
{
  m_tag=tag;
}
-(LRESULT)onSwellMessage:(UINT)msg p1:(WPARAM)wParam p2:(LPARAM)lParam
{
  if (msg == PBM_SETRANGE)
  {
    [self setMinValue:LOWORD(lParam)];
    [self setMaxValue:HIWORD(lParam)];
  }
  else if (msg==PBM_SETPOS)
  {
    [self setDoubleValue:(double)wParam];
    [self stopAnimation:self];    
  }
  else if (msg==PBM_DELTAPOS)
  {
    [self incrementBy:(double)wParam];
  }
  return 0;
}

@end

@implementation SWELL_ListViewCell
-(NSColor *)highlightColorWithFrame:(NSRect)cellFrame inView:(NSView *)controlView 
{
  if ([controlView isKindOfClass:[SWELL_ListView class]] && ((SWELL_ListView *)controlView)->m_selColors) return nil;
  if ([controlView isKindOfClass:[SWELL_TreeView class]] && ((SWELL_TreeView *)controlView)->m_selColors) return nil;
  return [super highlightColorWithFrame:cellFrame inView:controlView];
}
@end

@implementation SWELL_StatusCell
-(id)initNewCell
{
  if ((self=[super initTextCell:@""]))
  {
    status=0;
  }
  return self;
}
-(void)setStatusImage:(NSImage *)img
{
  status=img;
}
- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
  if (status)
  {
//    [controlView lockFocus];
    int w=wdl_min(cellFrame.size.width, cellFrame.size.height);
    [status drawInRect:NSMakeRect(cellFrame.origin.x,cellFrame.origin.y,w,cellFrame.size.height) fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1.0];
 //   [controlView unlockFocus];
  }
  cellFrame.origin.x += cellFrame.size.height + 2.0;
  cellFrame.size.width -= cellFrame.size.height + 2.0;
  [super drawWithFrame:cellFrame inView:controlView];
}

-(NSColor *)highlightColorWithFrame:(NSRect)cellFrame inView:(NSView *)controlView 
{
  if ([controlView isKindOfClass:[SWELL_ListView class]] && ((SWELL_ListView *)controlView)->m_selColors) return nil;
  return [super highlightColorWithFrame:cellFrame inView:controlView];
}

@end

@implementation SWELL_TreeView
STANDARD_CONTROL_NEEDSDISPLAY_IMPL("SysTreeView32")

-(id) init
{
  if ((self = [super init]))
  {
    m_fakerightmouse=false;
    m_items=new WDL_PtrList<HTREEITEM__>;
    m_fgColor=0;
    m_selColors=0;
  }
  return self;
}
-(void) dealloc
{
  if (m_items) m_items->Empty(true);
  delete m_items;
  m_items=0;
  [m_fgColor release];
  [m_selColors release];
  [super dealloc];
}

-(bool) findItem:(HTREEITEM)item parOut:(HTREEITEM__ **)par idxOut:(int *)idx
{
  if (!m_items||!item) return false;
  int x=m_items->Find((HTREEITEM__*)item);
  if (x>=0)
  {
    *par=NULL;
    *idx=x;
    return true;
  }
  for (x = 0; x < m_items->GetSize(); x++)
  {
    if (m_items->Get(x)->FindItem(item,par,idx)) return true;
  }

  return false;
}

-(NSInteger) outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
  if (item == nil) return m_items ? m_items->GetSize() : 0;
  return ((HTREEITEM__*)[item getValue])->m_children.GetSize();
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
  if (item==nil) return YES;
  HTREEITEM__ *it=(HTREEITEM__ *)[item getValue];
  
  return it && it->m_haschildren;
}

- (id)outlineView:(NSOutlineView *)outlineView
            child:(NSInteger)index
           ofItem:(id)item
{
  HTREEITEM__ *row=item ? ((HTREEITEM__*)[item getValue])->m_children.Get(index) : m_items ? m_items->Get(index) : 0;

  return (id)(row ? row->m_dh : NULL);
}

- (id)outlineView:(NSOutlineView *)outlineView
    objectValueForTableColumn:(NSTableColumn *)tableColumn
           byItem:(id)item
{
  if (!item) return @"";
  HTREEITEM__ *it=(HTREEITEM__ *)[item getValue];
  
  if (!it || !it->m_value) return @"";
 
  NSString *str=(NSString *)SWELL_CStringToCFString(it->m_value);    
  
  return [str autorelease];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView
         writeItems:(NSArray *)items
       toPasteboard:(NSPasteboard *)pasteboard
{
  if (self->style & TVS_DISABLEDRAGDROP) return NO;
  [pasteboard declareTypes:[NSArray arrayWithObject:@"swell_treeview"] owner:nil];
  [pasteboard setString:@"" forType:@"swell_treeview"];
  return YES;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView
         acceptDrop:(id<NSDraggingInfo>)info
               item:(id)item
         childIndex:(NSInteger)index
{
  HWND par = GetParent((HWND)self);
  if (par && GetCapture() == par)
  {
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(par,&p);
    SendMessage(par,WM_LBUTTONUP,0,MAKELPARAM(p.x,p.y));
  }
  return YES;
}

- (void)outlineView:(NSOutlineView *)outlineView
    draggingSession:(NSDraggingSession *)session
       endedAtPoint:(NSPoint)screenPoint
          operation:(NSDragOperation)operation
{
  [self unregisterDraggedTypes];
  HWND par = GetParent((HWND)self);
  if (par && GetCapture() == par)
  {
    // usually acceptDrop above will be the one that is called, but if the user ended up elsewhere
    // this might, let the caller clean up capture
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(par,&p);
    SendMessage(par,WM_LBUTTONUP,0,MAKELPARAM(p.x,p.y));
  }
}

- (void)outlineView:(NSOutlineView *)outlineView
    draggingSession:(NSDraggingSession *)session
   willBeginAtPoint:(NSPoint)screenPoint
           forItems:(NSArray *)draggedItems
{
  if (self->style & TVS_DISABLEDRAGDROP) return;
  HWND hwnd = (HWND)self, par = GetParent(hwnd);
  if (par)
  {
    TVHITTESTINFO tht;
    memset(&tht,0,sizeof(tht));
    GetCursorPos(&tht.pt);
    ScreenToClient(hwnd, &tht.pt);
    HTREEITEM sel = TreeView_GetSelection(hwnd), hit = TreeView_HitTest(hwnd, &tht);
    if (hit && hit != sel) 
    {
      TreeView_SelectItem(hwnd,hit);
      sel = hit;
    }

    NMTREEVIEW nm={{hwnd,(UINT_PTR)[self tag],TVN_BEGINDRAG},};
    nm.itemNew.hItem = sel;
    nm.itemNew.lParam = sel ? sel->m_param : 0;
    SendMessage(par,WM_NOTIFY,nm.hdr.idFrom,(LPARAM)&nm);
    if (GetCapture() == par)
      [self registerForDraggedTypes:[NSArray arrayWithObject: @"swell_treeview"]];
  }
}

- (NSDragOperation)outlineView:(NSOutlineView *)outlineView
                  validateDrop:(id<NSDraggingInfo>)info
                  proposedItem:(id)item
            proposedChildIndex:(NSInteger)index
{
  HWND hwnd=(HWND)self, par = GetParent(hwnd);
  if (par && GetCapture()==par)
  {
    POINT p;
    GetCursorPos(&p);
    TVHITTESTINFO tht;
    memset(&tht,0,sizeof(tht));
    tht.pt = p;

    ScreenToClient(par,&p);
    LRESULT move_res = SendMessage(par,WM_MOUSEMOVE,0,MAKELPARAM(p.x,p.y));
    if (move_res == (LRESULT)-1) return NSDragOperationNone;
    if (move_res == (LRESULT)-2) // move to end
    {
      HTREEITEM par_item = NULL;
      HTREEITEM li = self->m_items ? self->m_items->Get(self->m_items->GetSize()-1) : NULL;
      while (li && li->m_children.GetSize())
      {
        par_item = li;
        li = li->m_children.Get(li->m_children.GetSize()-1);
      }
      if (par_item && par_item->m_children.GetSize()) [self setDropItem:par_item->m_dh dropChildIndex:par_item->m_children.GetSize()];
    }
    else if (move_res >= 65536)
    {
      HTREEITEM paritem = NULL;
      int idx=0;
      // it is safe (but time consuming!) to call findItem: on a possibly-junk pointer
      if ([self findItem:(HTREEITEM)(INT_PTR)move_res parOut:&paritem idxOut:&idx] && paritem)
        [self setDropItem:paritem->m_dh dropChildIndex:idx];
    }
    return NSDragOperationPrivate;
  }
  return NSDragOperationNone;

}

-(void)mouseDown:(NSEvent *)theEvent
{
  if (([theEvent modifierFlags] & NSControlKeyMask) && IsRightClickEmulateEnabled())
  {
    m_fakerightmouse=1;  
  }
  else 
  {
    
    NMCLICK nmlv={{(HWND)self,(UINT_PTR)[self tag], NM_CLICK},};
    SendMessage((HWND)[self target],WM_NOTIFY,nmlv.hdr.idFrom,(LPARAM)&nmlv);
    
    m_fakerightmouse=0;
    [super mouseDown:theEvent];
  }
}

-(void)mouseDragged:(NSEvent *)theEvent
{
}

-(void)mouseUp:(NSEvent *)theEvent
{   
  if (m_fakerightmouse||([theEvent modifierFlags] & NSControlKeyMask)) [self rightMouseUp:theEvent];
  else [super mouseUp:theEvent];
}
- (void)rightMouseUp:(NSEvent *)theEvent
{
  bool wantContext=true;

  NMCLICK nmlv={{(HWND)self,(UINT_PTR)[self tag], NM_RCLICK},};
  if (SendMessage((HWND)[self target],WM_NOTIFY,nmlv.hdr.idFrom,(LPARAM)&nmlv)) wantContext=false;
  
  if (wantContext)
  {
    POINT p;
    GetCursorPos(&p);
    SendMessage((HWND)[self target],WM_CONTEXTMENU,(WPARAM)self,(p.x&0xffff)|(p.y<<16));
  }
  
  m_fakerightmouse=0;
}

- (void)highlightSelectionInClipRect:(NSRect)theClipRect
{
  if (m_selColors)
  {
    int a = GetFocus() == (HWND)self ? 0 : 2;
    if ([m_selColors count] >= a)
    {
      NSColor *c=[m_selColors objectAtIndex:a];
      if (c)
      {
        // calculate rect of selected items, combine with theClipRect, and fill these areas with our background (phew!)

        NSInteger x = [self selectedRow];
        if (x>=0)
        {
          NSRect r = [self rectOfRow:x];
          r = NSIntersectionRect(r,theClipRect);
          if (r.size.height>0 && r.size.width>0)
          {
            [c setFill];      
            NSRectFill(r);
          }
        }
        return ;
      }
    }
  }
  return [super highlightSelectionInClipRect:theClipRect];
}




@end





@implementation SWELL_ListView
STANDARD_CONTROL_NEEDSDISPLAY_IMPL( m_lbMode ? "SysListView32_LB" : "SysListView32" )

-(LONG)getSwellStyle { return style; }

-(void)setSwellStyle:(LONG)st 
{
  bool hdrchg= ((style&LVS_NOCOLUMNHEADER) != (st&LVS_NOCOLUMNHEADER));
  style=st;
  if ((style&LVS_REPORT) && hdrchg)
  {
    // todo some crap with NSTableView::setHeaderView, but it's complicated
  }
}

-(id) init
{
  if ((self = [super init]))
  {
    m_selColors=0;
    m_fgColor = 0;
    ownermode_cnt=0;
    m_status_imagelist_type=-1;
    m_status_imagelist=0;
    m_leftmousemovecnt=0;
    m_fakerightmouse=false;
    m_lbMode=0;
    m_fastClickMask=0;
    m_last_shift_clicked_item = m_last_plainly_clicked_item=-1;
    m_start_item=-1;
    m_start_subitem=-1;
    m_start_item_clickmode=0; // 0=clicked item, 1=clicked image, &2=sent drag message, &4=quickclick mode
    m_cols = new WDL_PtrList<NSTableColumn>;
    m_items=new WDL_PtrList<SWELL_ListView_Row>;
  }
  return self;
}
-(void) dealloc
{
  if (m_items) m_items->Empty(true);
  delete m_items;
  delete m_cols;
  m_cols=0;
  m_items=0;
  [m_fgColor release];
  [m_selColors release];
  [super dealloc];
}

-(int)getColumnPos:(int)idx // get current position of column that was originally at idx
{
  int pos=idx;
  if (m_cols)
  {
    NSTableColumn* col=m_cols->Get(idx);
    if (col)
    {
      NSArray* arr=[self tableColumns];
      if (arr)
      {
        pos=(int)[arr indexOfObject:col];
      }
    }
  }
  return pos;
}

- (void)highlightSelectionInClipRect:(NSRect)theClipRect
{
  if (m_selColors)
  {
    int a = GetFocus() == (HWND)self ? 0 : 2;
    if ([m_selColors count] >= a)
    {
      NSColor *c=[m_selColors objectAtIndex:a];
      if (c)
      {
        // calculate rect of selected items, combine with theClipRect, and fill these areas with our background (phew!)
        bool needfillset=true;
        NSInteger x = [self rowAtPoint:NSMakePoint(0,theClipRect.origin.y)];
        if (x<0)x=0;
        const NSInteger n = [self numberOfRows];
        for (;x <n;x++)
        {
          NSRect r = [self rectOfRow:x];
          if (r.origin.y >= theClipRect.origin.y + theClipRect.size.height) break;
          
          if ([self isRowSelected:x])
          {
            r = NSIntersectionRect(r,theClipRect);
            if (r.size.height>0 && r.size.width>0)
            {
              if (needfillset) { needfillset=false; [c setFill]; }
              NSRectFill(r);
            }
          }
        }
        return ;
      }
    }
  }
  return [super highlightSelectionInClipRect:theClipRect];
}
-(int)getColumnIdx:(int)pos // get original index of column that is currently at position
{
  int idx=pos;
  NSArray* arr=[self tableColumns];
  if (arr && pos>=0 && pos < [arr count])
  {
    NSTableColumn* col=[arr objectAtIndex:pos];
    if (col && m_cols)
    {
      idx=m_cols->Find(col);
    }
  }
  return idx;
}

-(NSInteger)columnAtPoint:(NSPoint)pt
{
  int pos=(int)[super columnAtPoint:pt];
  return (NSInteger) [self getColumnIdx:pos];
}


- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView
{
  return (!m_lbMode && (style & LVS_OWNERDATA)) ? ownermode_cnt : (m_items ? m_items->GetSize():0);
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex
{
  NSString *str=NULL;
  int image_idx=0;
  
  if (!m_lbMode && (style & LVS_OWNERDATA))
  {
    HWND tgt=(HWND)[self target];

    char buf[1024];
    NMLVDISPINFO nm={{(HWND)self, (UINT_PTR)[self tag], LVN_GETDISPINFO}};
    nm.item.mask=LVIF_TEXT;
    if (m_status_imagelist_type==LVSIL_STATE) nm.item.mask |= LVIF_STATE;
    else if (m_status_imagelist_type == LVSIL_SMALL) nm.item.mask |= LVIF_IMAGE;
    nm.item.iImage = -1;
    nm.item.iItem=(int)rowIndex;
    nm.item.iSubItem=m_cols->Find(aTableColumn);
    nm.item.pszText=buf;
    nm.item.cchTextMax=sizeof(buf)-1;
    buf[0]=0;
    SendMessage(tgt,WM_NOTIFY,nm.hdr.idFrom,(LPARAM)&nm);
    
    if (m_status_imagelist_type == LVSIL_STATE) image_idx=(nm.item.state>>16)&0xff;
    else if (m_status_imagelist_type == LVSIL_SMALL) image_idx = nm.item.iImage + 1;
    str=(NSString *)SWELL_CStringToCFString(nm.item.pszText); 
  }
  else
  {
    char *p=NULL;
    SWELL_ListView_Row *r=0;
    if (m_items && m_cols && (r=m_items->Get(rowIndex))) 
    {
      p=r->m_vals.Get(m_cols->Find(aTableColumn));
      if (m_status_imagelist_type == LVSIL_STATE || m_status_imagelist_type == LVSIL_SMALL)
      {
        image_idx=r->m_imageidx;
      }
    }
    
    str=(NSString *)SWELL_CStringToCFString(p);    
    
    if (style & LBS_OWNERDRAWFIXED)
    {
      SWELL_ODListViewCell *cell=[aTableColumn dataCell];
      if ([cell isKindOfClass:[SWELL_ODListViewCell class]]) [cell setItemIdx:(int)rowIndex];
    }
  }
  
  if (!m_lbMode && m_status_imagelist)
  {
    SWELL_StatusCell *cell=(SWELL_StatusCell*)[aTableColumn dataCell];
    if ([cell isKindOfClass:[SWELL_StatusCell class]])
    {
      HICON icon=m_status_imagelist->Get(image_idx-1);      
      NSImage *img=NULL;
      if (icon)  img=(NSImage *)GetNSImageFromHICON(icon);
      [cell setStatusImage:img];
    }
  }
  
  return [str autorelease];

}


-(void)mouseDown:(NSEvent *)theEvent
{
  if (([theEvent modifierFlags] & NSControlKeyMask) && IsRightClickEmulateEnabled())
  {
    m_fakerightmouse=1;  
    m_start_item=-1;
    m_start_subitem=-1;
  }
  else 
  {
    if ([theEvent clickCount]>1 && SWELL_NeedModernListViewHacks())
    {
      [super mouseDown:theEvent];
      return;
    }
    m_leftmousemovecnt=0;
    m_fakerightmouse=0;
    
    NSPoint pt=[theEvent locationInWindow];
    pt=[self convertPoint:pt fromView:nil];
    m_start_item=(int)[self rowAtPoint:pt];
    m_start_subitem=(int)[self columnAtPoint:pt];
    
    
    
    m_start_item_clickmode=0;
    if (m_start_item >=0 && (m_fastClickMask&(1<<m_start_subitem)))
    {
      NMLISTVIEW nmlv={{(HWND)self,(UINT_PTR)[self tag], NM_CLICK}, m_start_item, m_start_subitem, 0, 0, 0, {NSPOINT_TO_INTS(pt)}, };
      SWELL_ListView_Row *row=m_items->Get(nmlv.iItem);
      if (row)
        nmlv.lParam = row->m_param;
      SendMessage((HWND)[self target],WM_NOTIFY,nmlv.hdr.idFrom,(LPARAM)&nmlv);
      m_start_item_clickmode=4;
    }
    else
    {
      if (m_start_item>=0 && m_status_imagelist && LVSIL_STATE == m_status_imagelist_type && pt.x <= [self rowHeight]) // in left area
      {
        m_start_item_clickmode=1;
      }
    }
  }
}

-(void)mouseDragged:(NSEvent *)theEvent
{
  if (++m_leftmousemovecnt==4)
  {
    if (m_start_item>=0 && !(m_start_item_clickmode&3))
    {
      if (!m_lbMode)
      {
        // if m_start_item isnt selected, change selection to it now
        if (!(m_start_item_clickmode&4) && ![self isRowSelected:m_start_item]) 
        {
          [self selectRowIndexes:[NSIndexSet indexSetWithIndex:m_start_item] byExtendingSelection:!!(GetAsyncKeyState(VK_CONTROL)&0x8000)];
        }
        NMLISTVIEW hdr={{(HWND)self,(UINT_PTR)[self tag],LVN_BEGINDRAG},m_start_item,m_start_subitem,0,};
        SendMessage((HWND)[self target],WM_NOTIFY,hdr.hdr.idFrom, (LPARAM) &hdr);
        m_start_item_clickmode |= 2;
      }
    }
  }
  else if (m_leftmousemovecnt > 4 && !(m_start_item_clickmode&1))
  {
    HWND tgt=(HWND)[self target];
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(tgt,&p);
    
    SendMessage(tgt,WM_MOUSEMOVE,0,(p.x&0xffff) + (((int)p.y)<<16));
  }
}

-(void)mouseUp:(NSEvent *)theEvent
{
  if ((m_fakerightmouse||([theEvent modifierFlags] & NSControlKeyMask)) && IsRightClickEmulateEnabled())
  {
    [self rightMouseUp:theEvent];
  }
  else 
  {
    if ([theEvent clickCount]>1 && SWELL_NeedModernListViewHacks())
    {
      [super mouseUp:theEvent];
      return;
    }
    if (!(m_start_item_clickmode&1))
    {
      if (m_leftmousemovecnt>=0 && m_leftmousemovecnt<4 && !(m_start_item_clickmode&4))
      {
        const bool msel = [self allowsMultipleSelection];
        if (m_lbMode && !msel) // listboxes --- allow clicking to reset the selection
        {
          [self deselectAll:self];
        }

        if (SWELL_NeedModernListViewHacks())
        {
          if (m_start_item>=0)
          {
            NSMutableIndexSet *m = [[NSMutableIndexSet alloc] init];
            if (GetAsyncKeyState(VK_CONTROL)&0x8000)
            {
              [m addIndexes:[self selectedRowIndexes]];
              if ([m containsIndex:m_start_item]) [m removeIndex:m_start_item];
              else 
              {
                if (!msel) [m removeAllIndexes];
                [m addIndex:m_start_item];
              }
              m_last_plainly_clicked_item = m_start_item;
            }
            else if (msel && (GetAsyncKeyState(VK_SHIFT)&0x8000))
            {
              [m addIndexes:[self selectedRowIndexes]];
              const int n = ListView_GetItemCount((HWND)self);
              if (m_last_plainly_clicked_item<0 || m_last_plainly_clicked_item>=n)
                m_last_plainly_clicked_item=m_start_item;
  
              if (m_last_shift_clicked_item>=0 && 
                  m_last_shift_clicked_item<n && 
                  m_last_plainly_clicked_item != m_last_shift_clicked_item)
              {
                int a1 = m_last_shift_clicked_item;
                int a2 = m_last_plainly_clicked_item;
                if (a2<a1) { int tmp=a1; a1=a2; a2=tmp; }
                [m removeIndexesInRange:NSMakeRange(a1,a2-a1 + 1)];
              }
              
              int a1 = m_start_item;
              int a2 = m_last_plainly_clicked_item;
              if (a2<a1) { int tmp=a1; a1=a2; a2=tmp; }
              [m addIndexesInRange:NSMakeRange(a1,a2-a1 + 1)];

              m_last_shift_clicked_item = m_start_item;
            }
            else
            {
              m_last_plainly_clicked_item = m_start_item;
              [m addIndex:m_start_item];
            }
  
            [self selectRowIndexes:m byExtendingSelection:NO];
  
            [m release];
  
          }
          else [self deselectAll:self];
        }
        else
        {
          [super mouseDown:theEvent];
          [super mouseUp:theEvent];
        }
      }
      else if (m_leftmousemovecnt>=4)
      {
        HWND tgt=(HWND)[self target];
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(tgt,&p);      
        SendMessage(tgt,WM_LBUTTONUP,0,(p.x&0xffff) + (((int)p.y)<<16));      
      }
    }
  }
  
  if (!m_lbMode && !(m_start_item_clickmode&(2|4)))
  {
    NSPoint pt=[theEvent locationInWindow];
    pt=[self convertPoint:pt fromView:nil];    
    int col = (int)[self columnAtPoint:pt];
    NMLISTVIEW nmlv={{(HWND)self,(UINT_PTR)[self tag], NM_CLICK}, (int)[self rowAtPoint:pt], col, 0, 0, 0, {NSPOINT_TO_INTS(pt)}, };
    SWELL_ListView_Row *row=m_items->Get(nmlv.iItem);
    if (row) nmlv.lParam = row->m_param;
    SendMessage((HWND)[self target],WM_NOTIFY,nmlv.hdr.idFrom,(LPARAM)&nmlv);
  }  
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
  bool wantContext=true;
  
  if (!m_lbMode) 
  {
    NSPoint pt=[theEvent locationInWindow];
    pt=[self convertPoint:pt fromView:nil];
    
    // note, windows selects on right mousedown    
    NSInteger row =[self rowAtPoint:pt];
    if (row >= 0 && ![self isRowSelected:row])
    {
      NSIndexSet* rows=[NSIndexSet indexSetWithIndex:row];
      [self deselectAll:self];
      [self selectRowIndexes:rows byExtendingSelection:NO];
    }       
    
    NMLISTVIEW nmlv={{(HWND)self,(UINT_PTR)[self tag], NM_RCLICK}, (int)row, (int)[self columnAtPoint:pt], 0, 0, 0, {NSPOINT_TO_INTS(pt)}, };
    if (SendMessage((HWND)[self target],WM_NOTIFY,nmlv.hdr.idFrom,(LPARAM)&nmlv)) wantContext=false;
  }
  if (wantContext)
  {
    POINT p;
    GetCursorPos(&p);
    SendMessage((HWND)[self target],WM_CONTEXTMENU,(WPARAM)self,(p.x&0xffff)|(p.y<<16));
  }
  m_fakerightmouse=0;
}

-(LRESULT)onSwellMessage:(UINT)msg p1:(WPARAM)wParam p2:(LPARAM)lParam
{
  HWND hwnd=(HWND)self;
    switch (msg)
    {
      case LB_RESETCONTENT:
          ownermode_cnt=0;
          if (m_items) 
          {
            m_items->Empty(true);
            [self reloadData];
          }
      return 0;
      case LB_ADDSTRING:
      case LB_INSERTSTRING:
      {
        int cnt=ListView_GetItemCount(hwnd);
        if (msg == LB_ADDSTRING && (style & LBS_SORT))
        {
          SWELL_ListView *tv=(SWELL_ListView*)hwnd;
          if (tv->m_lbMode && tv->m_items)
          {            
            cnt=ptrlist_bsearch_mod((char *)lParam,tv->m_items,_listviewrowSearchFunc,NULL);
          }
        }
        if (msg==LB_ADDSTRING) wParam=cnt;
        else if (wParam > cnt) wParam=cnt;
        LVITEM lvi={LVIF_TEXT,(int)wParam,0,0,0,(char *)lParam};
        ListView_InsertItem(hwnd,&lvi);
      }
      return wParam;
      case LB_GETCOUNT: return ListView_GetItemCount(hwnd);
      case LB_SETSEL:
        ListView_SetItemState(hwnd, (int)lParam,wParam ? LVIS_SELECTED : 0,LVIS_SELECTED);
        return 0;
      case LB_GETTEXT:
        if (lParam)
        {
          SWELL_ListView_Row *row=self->m_items ? self->m_items->Get(wParam) : NULL;
          *(char *)lParam = 0;
          if (row && row->m_vals.Get(0))
          {
            strcpy((char *)lParam, row->m_vals.Get(0));
            return (LRESULT)strlen(row->m_vals.Get(0));
          }
        }
      return LB_ERR;
      case LB_GETTEXTLEN:
        {
          SWELL_ListView_Row *row=self->m_items ? self->m_items->Get(wParam) : NULL;
          if (row) 
          {
            const char *p=row->m_vals.Get(0);
            return p?strlen(p):0;
          }
        }
      return LB_ERR;
      case LB_FINDSTRINGEXACT:
        if (lParam)
        {
          int x = (int) wParam + 1;
          if (x < 0) x=0;
          const int n = self->m_items ? self->m_items->GetSize() : 0;
          for (int i = 0; i < n; i ++)
          {
            SWELL_ListView_Row *row=self->m_items->Get(x);
            if (row)
            {
              const char *p = row->m_vals.Get(0);
              if (p && !stricmp(p,(const char *)lParam)) return x;
            }
            if (++x >= n) x=0;
          }
        }
      return LB_ERR;
      case LB_GETSEL:
        return !!(ListView_GetItemState(hwnd,(int)wParam,LVIS_SELECTED)&LVIS_SELECTED);
      case LB_GETCURSEL:
        return [self selectedRow];
      case LB_SETCURSEL:
      {
        if (wParam<ListView_GetItemCount(hwnd))
        {
          [self selectRowIndexes:[NSIndexSet indexSetWithIndex:wParam] byExtendingSelection:NO];        
          [self scrollRowToVisible:wParam];
        }
        else
        {
          [self deselectAll:self];
        }
      }
        return 0;
      case LB_GETITEMDATA:
      {
        if (m_items)
        {
          SWELL_ListView_Row *row=m_items->Get(wParam);
          if (row) return row->m_param;
        }
      }
        return 0;
      case LB_SETITEMDATA:
      {
        if (m_items)
        {
          SWELL_ListView_Row *row=m_items->Get(wParam);
          if (row) row->m_param=lParam;
        }
      }
        return 0;
      case LB_GETSELCOUNT:
        return [[self selectedRowIndexes] count];
      case LB_DELETESTRING:
        ListView_DeleteItem((HWND)self, (int)wParam);
        return 0;
      case WM_SETREDRAW:
        if (wParam)
        {
          if (SWELL_GetOSXVersion() >= 0x1070 && [self respondsToSelector:@selector(endUpdates)])
            [self endUpdates];
        }
        else
        {
          if (SWELL_GetOSXVersion() >= 0x1070 && [self respondsToSelector:@selector(beginUpdates)])
            [self beginUpdates];
        }

        return 0;
    }
  return 0;
}

-(int)getSwellNotificationMode
{
  return m_lbMode;
}
-(void)setSwellNotificationMode:(int)lbMode
{
  m_lbMode=lbMode;
}

-(void)onSwellCommand:(int)cmd
{
  // ignore commands
}

@end

@implementation SWELL_ImageButtonCell
- (NSRect)drawTitle:(NSAttributedString *)title withFrame:(NSRect)frame inView:(NSView *)controlView
{
  return NSMakeRect(0,0,0,0);
}
@end

@implementation SWELL_ODButtonCell
- (BOOL)isTransparent
{
  return YES;
}
- (BOOL)isOpaque
{
  return NO;
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
  NSView *ctl=[self controlView];
  if (!ctl) { [super drawWithFrame:cellFrame inView:controlView]; return; }
  
  HDC hdc=GetDC((HWND)controlView);
  if (hdc)
  {
    HWND notWnd = GetParent((HWND)ctl);
    DRAWITEMSTRUCT dis={ODT_BUTTON,(UINT)[ctl tag],0,0,0,(HWND)ctl,hdc,{0,},0};
    NSRECT_TO_RECT(&dis.rcItem,cellFrame);
    SendMessage(notWnd,WM_DRAWITEM,dis.CtlID,(LPARAM)&dis);
  
    ReleaseDC((HWND)controlView,hdc);
  }
  
}
@end

@implementation SWELL_ODListViewCell
-(void)setOwnerControl:(SWELL_ListView *)t { m_ownctl=t; m_lastidx=0; }
-(void)setItemIdx:(int)idx
{
  m_lastidx=idx;
}
- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
  if (!m_ownctl) { [super drawInteriorWithFrame:cellFrame inView:controlView]; return; }
  
  int itemidx=m_lastidx;
  LPARAM itemData=0;
  SWELL_ListView_Row *row=m_ownctl->m_items->Get(itemidx);
  if (row) itemData=row->m_param;

  HDC hdc=GetDC((HWND)controlView);
  if (hdc)
  {
    HWND notWnd = GetParent((HWND)m_ownctl);
    DRAWITEMSTRUCT dis={ODT_LISTBOX,(UINT)[m_ownctl tag],(UINT)itemidx,0,0,(HWND)m_ownctl,hdc,{0,},(DWORD_PTR)itemData};
    NSRECT_TO_RECT(&dis.rcItem,cellFrame);
    SendMessage(notWnd,WM_DRAWITEM,dis.CtlID,(LPARAM)&dis);
  
    ReleaseDC((HWND)controlView,hdc);
  }
}


@end





HWND GetDlgItem(HWND hwnd, int idx)
{
  if (WDL_NOT_NORMALLY(!hwnd)) return 0;

  NSView *v=0;
  id pid=(id)hwnd;
  if ([pid isKindOfClass:[NSWindow class]]) v=[((NSWindow *)pid) contentView];
  else if ([pid isKindOfClass:[NSView class]]) v=(NSView *)pid;
  
  if (!idx || !v) return (HWND)v;
  
  SWELL_BEGIN_TRY

  NSArray *ar = [v subviews];
  const NSInteger n=[ar count];
  for (NSInteger x=0;x<n;x++)
  {
    NSView *sv = [ar objectAtIndex:x];
    if (sv)
    {
      if ([sv respondsToSelector:@selector(tag)] && [sv tag] == idx) return (HWND)sv;

      if (sv && [sv isKindOfClass:[NSScrollView class]])
      {
        sv=[(NSScrollView *)sv documentView];
        if (sv && [sv respondsToSelector:@selector(tag)] && [sv tag] == idx) return (HWND)sv;
      }
      if (sv && [sv isKindOfClass:[NSClipView class]]) 
      {
        sv = [(NSClipView *)sv documentView];
        if (sv && [sv respondsToSelector:@selector(tag)] && [sv tag] == idx) return (HWND)sv;
      }
    }
  }
  // we might want to enable this for max compat with old code, but hopefully not:  return [v viewWithTag:idx]; 
  SWELL_END_TRY(;)
  return NULL;
}


LONG_PTR SetWindowLong(HWND hwnd, int idx, LONG_PTR val)
{
  if (WDL_NOT_NORMALLY(!hwnd)) return 0;

  SWELL_BEGIN_TRY
  id pid=(id)hwnd;
  if (idx==GWL_EXSTYLE && [pid respondsToSelector:@selector(swellSetExtendedStyle:)])
  {
    LONG ret=(LONG) [(SWELL_hwndChild*)pid swellGetExtendedStyle];
    [(SWELL_hwndChild*)pid swellSetExtendedStyle:(LONG)val];
    return ret;
  }
  if (idx==GWL_USERDATA && [pid respondsToSelector:@selector(setSwellUserData:)])
  {
    LONG_PTR ret=(LONG_PTR)[(SWELL_hwndChild*)pid getSwellUserData];
    [(SWELL_hwndChild*)pid setSwellUserData:(LONG_PTR)val];
    return ret;
  }
    
  if (idx==GWL_ID && [pid respondsToSelector:@selector(tag)] && [pid respondsToSelector:@selector(setTag:)])
  {
    int ret= (int) [pid tag];
    [pid setTag:(int)val];
    return (LONG_PTR)ret;
  }
  
  if (idx==GWL_WNDPROC && [pid respondsToSelector:@selector(setSwellWindowProc:)])
  {
    WNDPROC ov=(WNDPROC)[pid getSwellWindowProc];
    [pid setSwellWindowProc:(WNDPROC)val];
    return (LONG_PTR)ov;
  }
  if (idx==DWL_DLGPROC && [pid respondsToSelector:@selector(setSwellDialogProc:)])
  {
    DLGPROC ov=(DLGPROC)[pid getSwellDialogProc];
    [pid setSwellDialogProc:(DLGPROC)val];
    return (LONG_PTR)ov;
  }
  
  if (idx==GWL_STYLE)
  {
    if ([pid respondsToSelector:@selector(setSwellStyle:)])
    {
      LONG ov=[pid getSwellStyle];
      [pid setSwellStyle:(LONG)(val & ~WS_VISIBLE)];
      return ov;
    }
    else if ([pid isKindOfClass:[NSButton class]]) 
    {
      int ret=(int)GetWindowLong(hwnd,idx);
      
      if ((val&0xf) == BS_AUTO3STATE)
      {
        [pid setButtonType:NSSwitchButton];
        [pid setAllowsMixedState:YES];
        if ([pid isKindOfClass:[SWELL_Button class]]) [pid swellSetRadioFlags:0];
      }    
      else if ((val & 0xf) == BS_AUTOCHECKBOX)
      {
        [pid setButtonType:NSSwitchButton];
        [pid setAllowsMixedState:NO];
        if ([pid isKindOfClass:[SWELL_Button class]]) [pid swellSetRadioFlags:0];
      }
      else if ((val &  0xf) == BS_AUTORADIOBUTTON)
      {
        [pid setButtonType:NSRadioButton];
        if ([pid isKindOfClass:[SWELL_Button class]]) [pid swellSetRadioFlags:(val&WS_GROUP)?3:1];
      }               
      
      return ret;
    }
    else 
    {
      if ([[pid window] contentView] == pid)
      {
        NSView *tv=(NSView *)pid;
        NSWindow *oldw = [tv window];
        NSUInteger smask = [oldw styleMask];
        int mf=0;
        if (smask & NSTitledWindowMask)
        {
          mf|=WS_CAPTION;
          if (smask & NSResizableWindowMask) mf|=WS_THICKFRAME;
        }
        if (mf != (val&(WS_CAPTION|WS_THICKFRAME)))
        {
          BOOL dovis = IsWindowVisible((HWND)oldw);
          NSWindow *oldpar = [oldw parentWindow];
          char oldtitle[2048];
          oldtitle[0]=0;
          GetWindowText(hwnd,oldtitle,sizeof(oldtitle));
          NSRect fr=[oldw frame];
          HWND oldOwner=NULL;
          if ([oldw respondsToSelector:@selector(swellGetOwner)]) oldOwner=(HWND)[(SWELL_ModelessWindow*)oldw swellGetOwner];
          NSInteger oldlevel = [oldw level];

          
          [tv retain];
          SWELL_hwndChild *tempview = [[SWELL_hwndChild alloc] initChild:nil Parent:(NSView *)oldw dlgProc:nil Param:0];          
          [tempview release];          
          
          unsigned int mask=0;
          
          if (val & WS_CAPTION)
          {
            mask|=NSTitledWindowMask;
            if (val & WS_THICKFRAME)
              mask|=NSMiniaturizableWindowMask|NSClosableWindowMask|NSResizableWindowMask;
          }
      
          HWND SWELL_CreateModelessFrameForWindow(HWND childW, HWND ownerW, unsigned int);
          HWND bla=SWELL_CreateModelessFrameForWindow((HWND)tv,(HWND)oldOwner,mask);
          
          if (bla)
          {
            [tv release];
            // move owned windows over
            if ([oldw respondsToSelector:@selector(swellGetOwnerWindowHead)])
            {
              void **p=(void **)[(SWELL_ModelessWindow*)oldw swellGetOwnerWindowHead];
              if (p && [(id)bla respondsToSelector:@selector(swellGetOwnerWindowHead)])
              {
                void **p2=(void **)[(SWELL_ModelessWindow*)bla swellGetOwnerWindowHead];
                if (p && p2) 
                {
                  *p2=*p;
                  *p=0;
                  OwnedWindowListRec *rec = (OwnedWindowListRec *) *p2;
                  while (rec)
                  {
                    if (rec->hwnd && [rec->hwnd respondsToSelector:@selector(swellSetOwner:)])
                      [(SWELL_ModelessWindow *)rec->hwnd swellSetOwner:(id)bla];
                    rec=rec->_next;
                  }
                }
              }
            }
            // move all child and owned windows over to new window
            NSArray *ar=[oldw childWindows];
            if (ar)
            {
              int x;
              for (x = 0; x < [ar count]; x ++)
              {
                NSWindow *cw=[ar objectAtIndex:x];
                if (cw)
                {
                  [cw retain];
                  [oldw removeChildWindow:cw];
                  [(NSWindow *)bla addChildWindow:cw ordered:NSWindowAbove];
                  [cw release];
                  
                  
                }
              }
            }
          
            if (oldpar) [oldpar addChildWindow:(NSWindow *)bla ordered:NSWindowAbove];
            if (oldtitle[0]) SetWindowText(hwnd,oldtitle);
            
            [(NSWindow *)bla setFrame:fr display:dovis];
            [(NSWindow *)bla setLevel:oldlevel];
            if (dovis) ShowWindow(bla,SW_SHOW);
      
            DestroyWindow((HWND)oldw);
          }
          else
          {
            [oldw setContentView:tv];
            [tv release];
          }  
      
        }
      }
    }
    return 0;
  }

  if (idx == GWL_HWNDPARENT)
  {
    NSWindow *window = [pid window];
    if (![window respondsToSelector:@selector(swellGetOwner)]) return 0;

    NSWindow *new_owner = val && [(id)(INT_PTR)val isKindOfClass:[NSView class]] ? [(NSView *)(INT_PTR)val window] : NULL;
    if (new_owner && ![new_owner respondsToSelector:@selector(swellAddOwnedWindow:)]) new_owner=NULL;

    NSWindow *old_owner = [(SWELL_ModelessWindow *)window swellGetOwner];
    if (old_owner != new_owner)
    {
      if (old_owner) [(SWELL_ModelessWindow*)old_owner swellRemoveOwnedWindow:window];
      [(SWELL_ModelessWindow *)window swellSetOwner:nil];
      if (new_owner) [(SWELL_ModelessWindow *)new_owner swellAddOwnedWindow:window];
    }
    return (old_owner ? (LONG_PTR)[old_owner contentView] : 0);
  }
  
  if ([pid respondsToSelector:@selector(setSwellExtraData:value:)])
  {
    LONG_PTR ov=0;
    if ([pid respondsToSelector:@selector(getSwellExtraData:)]) ov=(LONG_PTR)[pid getSwellExtraData:idx];

    [pid setSwellExtraData:idx value:val];
    
    return ov;
  }
   
  SWELL_END_TRY(;)
  return 0;
}

LONG_PTR GetWindowLong(HWND hwnd, int idx)
{
  if (WDL_NOT_NORMALLY(!hwnd)) return 0;
  id pid=(id)hwnd;

  SWELL_BEGIN_TRY
  
  if (idx==GWL_EXSTYLE && [pid respondsToSelector:@selector(swellGetExtendedStyle)])
  {
    return (LONG_PTR)[pid swellGetExtendedStyle];
  }
  
  if (idx==GWL_USERDATA && [pid respondsToSelector:@selector(getSwellUserData)])
  {
    return (LONG_PTR)[pid getSwellUserData];
  }
  if (idx==GWL_USERDATA && [pid isKindOfClass:[NSText class]])
  {
    return 0xdeadf00b;
  }
  
  if (idx==GWL_ID && [pid respondsToSelector:@selector(tag)])
    return [pid tag];
  
  
  if (idx==GWL_WNDPROC && [pid respondsToSelector:@selector(getSwellWindowProc)])
  {
    return (LONG_PTR)[pid getSwellWindowProc];
  }
  if (idx==DWL_DLGPROC && [pid respondsToSelector:@selector(getSwellDialogProc)])
  {
    return (LONG_PTR)[pid getSwellDialogProc];
  }  
  if (idx==GWL_STYLE)
  {
    int ret=0;
    if (![pid isHidden]) ret |= WS_VISIBLE;
    if ([pid respondsToSelector:@selector(getSwellStyle)])
    {
      return (LONG_PTR)(([pid getSwellStyle]&~WS_VISIBLE) | ret);
    }    
    
    if ([pid isKindOfClass:[NSButton class]]) 
    {
      int tmp;
      if ([pid allowsMixedState]) ret |= BS_AUTO3STATE;
      else if ([pid isKindOfClass:[SWELL_Button class]] && (tmp = (int)[pid swellGetRadioFlags]))
      {
        ret |= BS_AUTORADIOBUTTON;
        if (tmp&2) ret|=WS_GROUP;
      }
      else ret |= BS_AUTOCHECKBOX; 
    }
    
    if ([pid isKindOfClass:[NSView class]])
    {
      if ([[pid window] contentView] != pid) ret |= WS_CHILDWINDOW;
      else
      {
        NSUInteger smask  =[[pid window] styleMask];
        if (smask & NSTitledWindowMask)
        {
          ret|=WS_CAPTION;
          if (smask & NSResizableWindowMask) ret|=WS_THICKFRAME;
        }
      }
    }
    
    return ret;
  }
  if (idx == GWL_HWNDPARENT)
  {
    NSWindow *window = [pid window];
    if (![window respondsToSelector:@selector(swellGetOwner)]) return 0;
    NSWindow *old_owner = [(SWELL_ModelessWindow *)window swellGetOwner];
    return (old_owner ? (LONG_PTR)[old_owner contentView] : 0);
  }

  if ([pid respondsToSelector:@selector(getSwellExtraData:)])
  {
    return (LONG_PTR)[pid getSwellExtraData:idx];
  }
  
  SWELL_END_TRY(;)
  return 0;
}

static bool IsWindowImpl(NSView *ch, NSView *par)
{
  if (!par || ![par isKindOfClass:[NSView class]]) return false;

  NSArray *ar = [par subviews];
  if (!ar) return false;
  [ar retain];
  NSInteger x,n=[ar count];
  for (x=0;x<n;x++)
    if ([ar objectAtIndex:x] == ch) 
    {
      [ar release];
      return true;
    }

  for (x=0;x<n;x++)
    if (IsWindowImpl(ch,[ar objectAtIndex:x])) 
    {
      [ar release];
      return true;
    }

  [ar release];
  return false;
}
bool IsWindow(HWND hwnd)
{
  if (!hwnd) return false;
  // this is very costly, but required
  SWELL_BEGIN_TRY

  NSArray *ch=[NSApp windows];
  [ch retain];
  NSInteger x,n=[ch count];
  for(x=0;x<n; x ++)
  {
    @try { 
      NSWindow *w = [ch objectAtIndex:x]; 
      if (w == (NSWindow *)hwnd || [w contentView] == (NSView *)hwnd) 
      {
        [ch release];
        return true;
      }
    }
    @catch (NSException *ex) { 
    }
    @catch (id ex) {
    }
  }
  for(x=0;x<n; x ++)
  {
    @try { 
      NSWindow *w = [ch objectAtIndex:x];
      if (w && 
          // only validate children of our windows (maybe an option for this?)
          ([w isKindOfClass:[SWELL_ModelessWindow class]] || [w isKindOfClass:[SWELL_ModalDialog class]]) &&
          IsWindowImpl((NSView*)hwnd,[w contentView])) 
      {
        [ch release];
        return true;
      }
    } 
    @catch (NSException *ex) { 
    }
    @catch (id ex) {
    }
  }
  [ch release];

  SWELL_END_TRY(;)
  return false;
}

bool IsWindowVisible(HWND hwnd)
{
  if (!hwnd) return false;

  SWELL_BEGIN_TRY
  id turd=(id)hwnd;
  if ([turd isKindOfClass:[NSView class]])
  {
    NSWindow *w = [turd window];
    if (w && ![w isVisible]) return false;
    
    return ![turd isHiddenOrHasHiddenAncestor];
  }
  if ([turd isKindOfClass:[NSWindow class]])
  {
    return !![turd isVisible];
  }
  SWELL_END_TRY(;)
  return true;
}


bool IsWindowEnabled(HWND hwnd)
{
  if (WDL_NOT_NORMALLY(!hwnd)) return false;

  bool rv = true;

  SWELL_BEGIN_TRY

  id view = (id)hwnd;
  if ([view isKindOfClass:[NSWindow class]]) view = [view contentView];

  rv = view && [view respondsToSelector:@selector(isEnabled)] && [view isEnabled];

  SWELL_END_TRY(;)

  return rv;
}




static void *__GetNSImageFromHICON(HICON ico) // local copy to not be link dependent on swell-gdi.mm
{
  HGDIOBJ__ *i = (HGDIOBJ__ *)ico;
  if (!i || i->type != TYPE_BITMAP) return 0;
  return i->bitmapptr;
}


@implementation SWELL_Button : NSButton

STANDARD_CONTROL_NEEDSDISPLAY_IMPL("Button")

-(id) init {
  self = [super init];
  if (self != nil) {
    m_userdata=0;
    m_swellGDIimage=0;
    m_radioflags=0;
  }
  return self;
}
-(int)swellGetRadioFlags { return m_radioflags; }
-(void)swellSetRadioFlags:(int)f { m_radioflags=f; }
-(LONG_PTR)getSwellUserData { return m_userdata; }
-(void)setSwellUserData:(LONG_PTR)val {   m_userdata=val; }

-(void)setSwellGDIImage:(void *)par
{
  m_swellGDIimage=par;
}
-(void *)getSwellGDIImage
{
  return m_swellGDIimage;
}

@end

LRESULT SendMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (WDL_NOT_NORMALLY(!hwnd)) return 0;

  SWELL_BEGIN_TRY
  id obj=(id)hwnd;
  if ([obj respondsToSelector:@selector(onSwellMessage:p1:p2:)])
  {
    return (LRESULT) [obj onSwellMessage:msg p1:wParam p2:lParam];
  }
  else 
  {
    if (msg == BM_GETCHECK && [obj isKindOfClass:[NSButton class]])
    {
      NSInteger a=[(NSButton*)obj state];
      if (a==NSMixedState) return BST_INDETERMINATE;
      return a!=NSOffState;
    }
    if (msg == BM_SETCHECK && [obj isKindOfClass:[NSButton class]])
    {
      [(NSButton*)obj setState:(wParam&BST_INDETERMINATE)?NSMixedState:((wParam&BST_CHECKED)?NSOnState:NSOffState)];
      return 0;
    }
    if ((msg==BM_GETIMAGE || msg == BM_SETIMAGE) && [obj isKindOfClass:[SWELL_Button class]])
    {
      if (wParam != IMAGE_BITMAP && wParam != IMAGE_ICON) return 0; // ignore unknown types
      LONG_PTR ret=(LONG_PTR) (void *)[obj getSwellGDIImage];
      if (msg==BM_SETIMAGE)
      {
        NSImage *img=NULL;
        if (lParam) img=(NSImage *)__GetNSImageFromHICON((HICON)lParam);
        [obj setImage:img];
        [obj setSwellGDIImage:(void *)(img?lParam:0)];
      }
      return ret;
    }
    else if (msg >= CB_ADDSTRING && msg <= CB_INITSTORAGE && ([obj isKindOfClass:[NSPopUpButton class]] || [obj isKindOfClass:[NSComboBox class]]))
    {
        switch (msg)
        {
          case CB_ADDSTRING: return SWELL_CB_AddString(hwnd,0,(char*)lParam); 
          case CB_DELETESTRING: SWELL_CB_DeleteString(hwnd,0,(int)wParam); return 1;
          case CB_GETCOUNT: return SWELL_CB_GetNumItems(hwnd,0);
          case CB_GETCURSEL: return SWELL_CB_GetCurSel(hwnd,0);
          case CB_GETLBTEXT: return SWELL_CB_GetItemText(hwnd,0,(int)wParam,(char *)lParam, 1<<20);
          case CB_GETLBTEXTLEN: return SWELL_CB_GetItemText(hwnd,0,(int)wParam,NULL,0);
          case CB_INSERTSTRING: return SWELL_CB_InsertString(hwnd,0,(int)wParam,(char *)lParam);
          case CB_RESETCONTENT: SWELL_CB_Empty(hwnd,0); return 0;
          case CB_SETCURSEL: SWELL_CB_SetCurSel(hwnd,0,(int)wParam); return 0;
          case CB_GETITEMDATA: return SWELL_CB_GetItemData(hwnd,0,(int)wParam);
          case CB_SETITEMDATA: SWELL_CB_SetItemData(hwnd,0,(int)wParam,lParam); return 0;
          case CB_FINDSTRING:
          case CB_FINDSTRINGEXACT:
            if (lParam) return SWELL_CB_FindString(hwnd,0,(int)wParam,(const char *)lParam,msg==CB_FINDSTRINGEXACT);
            return CB_ERR;
          case CB_INITSTORAGE: return 0;                                                      
        }
        return 0;
    }
    else if (msg >= TBM_GETPOS && msg <= TBM_SETRANGE && ([obj isKindOfClass:[NSSlider class]]))
    {
        switch (msg)
        {
          case TBM_GETPOS: return SWELL_TB_GetPos(hwnd,0);
          case TBM_SETTIC: SWELL_TB_SetTic(hwnd,0,(int)lParam); return 1;
          case TBM_SETPOS: SWELL_TB_SetPos(hwnd,0,(int)lParam); return 1;
          case TBM_SETRANGE: SWELL_TB_SetRange(hwnd,0,LOWORD(lParam),HIWORD(lParam)); return 1;
        }
        return 0;
    }
    else if ((msg == EM_SETSEL || msg == EM_GETSEL || msg == EM_SETPASSWORDCHAR) && ([obj isKindOfClass:[NSTextField class]]))
    { 
      if (msg == EM_GETSEL)
      {
        NSRange range={0,};
        NSResponder *rs = [[obj window] firstResponder];
        if ([rs isKindOfClass:[NSView class]] && [(NSView *)rs isDescendantOf:obj])
        {
          NSText* text=[[obj window] fieldEditor:YES forObject:(NSTextField*)obj];  
          if (text) range=[text selectedRange];
        }
        if (wParam) *(int*)wParam=(int)range.location;
        if (lParam) *(int*)lParam=(int)(range.location+range.length);
      }      
      else if (msg == EM_SETSEL)
      {        
        //        [(NSTextField*)obj selectText:obj]; // Force the window's text field editor onto this control
        // don't force it, just ignore EM_GETSEL/EM_SETSEL if not in focus
        NSResponder *rs = [[obj window] firstResponder];
        if ([rs isKindOfClass:[NSView class]] && [(NSView *)rs isDescendantOf:obj])
        {
          NSText* text = [[obj window] fieldEditor:YES forObject:(NSTextField*)obj]; // then get it from the window 
          NSUInteger sl = [[text string] length];
          if (wParam == -1) lParam = wParam = 0;
          else if (lParam == -1) lParam = sl;        
          if (wParam>sl) wParam=sl;
          if (lParam>sl) lParam=sl;      
          if (text) [text setSelectedRange:NSMakeRange(wParam, wdl_max(lParam-wParam,0))]; // and set the range
        }
      }
      else if (msg == EM_SETPASSWORDCHAR)
      {
        // not implemented, because it requires replacing obj within its parent window
        // instead caller explicitly destroy the edit control and create a new one with ES_PASSWORD
      }
      return 0;
    }
    else
    {
      NSWindow *w;
      NSView *v;
      // if content view gets unhandled message send to window
      if ([obj isKindOfClass:[NSView class]] && (w=[obj window]) && [w contentView] == obj && [w respondsToSelector:@selector(onSwellMessage:p1:p2:)])
      {
        return (LRESULT) [(SWELL_hwndChild *)w onSwellMessage:msg p1:wParam p2:lParam];
      }
      // if window gets unhandled message send to content view
      else if ([obj isKindOfClass:[NSWindow class]] && (v=[obj contentView]) && [v respondsToSelector:@selector(onSwellMessage:p1:p2:)])
      {
        return (LRESULT) [(SWELL_hwndChild *)v onSwellMessage:msg p1:wParam p2:lParam];
      }
    }
  }
  SWELL_END_TRY(;)
  return 0;
}

void DestroyWindow(HWND hwnd)
{
  if (WDL_NOT_NORMALLY(!hwnd)) return;
  SWELL_BEGIN_TRY
  id pid=(id)hwnd;
  if ([pid isKindOfClass:[NSView class]])
  {
    KillTimer(hwnd,~(UINT_PTR)0);
    sendSwellMessage((id)pid,WM_DESTROY,0,0);
      
    NSWindow *pw = [(NSView *)pid window];
    if (pw && [pw contentView] == pid) // destroying contentview should destroy top level window
    {
      DestroyWindow((HWND)pw);
    }
    else 
    {
      if (pw)
      {
        id foc=[pw firstResponder];
        if (foc && (foc == pid || IsChild((HWND)pid,(HWND)foc)))
        {
          [pw makeFirstResponder:nil];
        }
      }
      [(NSView *)pid removeFromSuperview];
    }
  }
  else if ([pid isKindOfClass:[NSWindow class]])
  {
    KillTimer(hwnd,~(UINT_PTR)0);
    sendSwellMessage([(id)pid contentView],WM_DESTROY,0,0);
    sendSwellMessage((id)pid,WM_DESTROY,0,0);
      
    if ([(id)pid respondsToSelector:@selector(swellDoDestroyStuff)])
      [(id)pid swellDoDestroyStuff];
      
    NSWindow *par=[(NSWindow*)pid parentWindow];
    if (par)
    {
      [par removeChildWindow:(NSWindow*)pid];
    }
    [(NSWindow *)pid close]; // this is probably bad, but close takes too long to close!
  }
  SWELL_END_TRY(;)
}

void EnableWindow(HWND hwnd, int enable)
{
  if (WDL_NOT_NORMALLY(!hwnd)) return;
  SWELL_BEGIN_TRY
  id bla=(id)hwnd;
  if ([bla isKindOfClass:[NSWindow class]]) bla = [bla contentView];
    
  if (bla && [bla respondsToSelector:@selector(setEnabled:)])
  {
    if (enable == -1000 && [bla respondsToSelector:@selector(setEnabledSwellNoFocus)])
      [(SWELL_hwndChild *)bla setEnabledSwellNoFocus];
    else
      [bla setEnabled:(enable?YES:NO)];
    if ([bla isKindOfClass:[SWELL_TextField class]])
      [(SWELL_TextField*)bla initColors:-1];
  }
  SWELL_END_TRY(;)
}

void SetForegroundWindow(HWND hwnd)
{
  WDL_ASSERT(hwnd != NULL);
  SetFocus(hwnd);
}

void SetFocus(HWND hwnd) // these take NSWindow/NSView, and return NSView *
{
  id r=(id) hwnd;
  if (!r) return; // on win32 SetFocus(NULL) is allowed, removes focus (maybe we should implement)
  
  SWELL_BEGIN_TRY
  if ([r isKindOfClass:[NSWindow class]])
  {
    [(NSWindow *)r makeFirstResponder:[(NSWindow *)r contentView]]; 
    if ([(NSWindow *)r isVisible]) [(NSWindow *)r makeKeyAndOrderFront:nil];
  }
  else if (WDL_NORMALLY([r isKindOfClass:[NSView class]]))
  {
    NSWindow *wnd=[(NSView *)r window];
    if (wnd)
    {
      if ([wnd isVisible])
        [wnd makeKeyAndOrderFront:nil];
      if ([r acceptsFirstResponder])
        [wnd makeFirstResponder:r];
    }
  }
  SWELL_END_TRY(;)
}

void SWELL_GetViewPort(RECT *r, const RECT *sourcerect, bool wantWork)
{
  SWELL_BEGIN_TRY

  NSArray *ar=[NSScreen screens];
  
  const NSInteger cnt=[ar count];
  int cx=0;
  int cy=0;
  if (sourcerect)
  {
    cx=(sourcerect->left+sourcerect->right)/2;
    cy=(sourcerect->top+sourcerect->bottom)/2;
  }
  for (NSInteger x = 0; x < cnt; x ++)
  {
    NSScreen *sc=[ar objectAtIndex:x];
    if (sc)
    {
      NSRect tr=wantWork ? [sc visibleFrame] : [sc frame];
      if (!x || (cx >= tr.origin.x && cx < tr.origin.x+tr.size.width  &&
                cy >= tr.origin.y && cy < tr.origin.y+tr.size.height))
      {
        NSRECT_TO_RECT(r,tr);
      }
    }
  }
  if (!cnt)
  {
    r->left=r->top=0;
    r->right=1600;
    r->bottom=1200;
  }
  SWELL_END_TRY(;)
}

void ScreenToClient(HWND hwnd, POINT *p)
{
  if (WDL_NOT_NORMALLY(!hwnd)) return;
  // no need to try/catch, this should never have an issue *wince*
  
  id ch=(id)hwnd;
  if ([ch isKindOfClass:[NSWindow class]]) ch=[((NSWindow *)ch) contentView];
  if (WDL_NOT_NORMALLY(!ch || ![ch isKindOfClass:[NSView class]])) return;
  
  NSWindow *window=[ch window];
  
  NSPoint wndpt = [window convertScreenToBase:NSMakePoint(p->x,p->y)];
  
  // todo : WM_NCCALCSIZE 
  NSPOINT_TO_POINT(p, [ch convertPoint:wndpt fromView:nil]);
}

void ClientToScreen(HWND hwnd, POINT *p)
{
  if (WDL_NOT_NORMALLY(!hwnd)) return;
  
  id ch=(id)hwnd;
  if ([ch isKindOfClass:[NSWindow class]]) ch=[((NSWindow *)ch) contentView];
  if (!ch || ![ch isKindOfClass:[NSView class]]) return;
  
  NSWindow *window=[ch window];
  
  NSPoint wndpt = [ch convertPoint:NSMakePoint(p->x,p->y) toView:nil];
  
  // todo : WM_NCCALCSIZE 
  
  NSPOINT_TO_POINT(p,[window convertBaseToScreen:wndpt]);
}

static NSView *NavigateUpScrollClipViews(NSView *ch)
{
  NSView *par=[ch superview];
  if (par && [par isKindOfClass:[NSClipView class]]) 
  {
    par=[par superview];
    if (par && [par isKindOfClass:[NSScrollView class]])
    {
      ch=par;
    }
  }
  return ch;
}

HWND SWELL_NavigateUpScrollClipViews(HWND h)
{
  NSView *v = 0;
  if (h && [(id)h isKindOfClass:[NSView class]]) v = (NSView *)h;
  else if (h && [(id)h isKindOfClass:[NSWindow class]]) v = [(NSWindow *)h contentView];
  if (v)
    return (HWND)NavigateUpScrollClipViews(v);
  return 0;
}

bool GetWindowRect(HWND hwnd, RECT *r)
{
  r->left=r->top=r->right=r->bottom=0;
  if (WDL_NOT_NORMALLY(!hwnd)) return false;

  SWELL_BEGIN_TRY
  
  id ch=(id)hwnd;
  NSWindow *nswnd;
  if ([ch isKindOfClass:[NSView class]] && (nswnd=[(NSView *)ch window]) && [nswnd contentView]==ch)
    ch=nswnd;
    
  if ([ch isKindOfClass:[NSWindow class]]) 
  {
    NSRECT_TO_RECT(r,[ch frame]);
    return true;
  }
  if (![ch isKindOfClass:[NSView class]]) return false;
  ch=NavigateUpScrollClipViews(ch);
  NSRECT_TO_RECT(r,[ch bounds]);
  ClientToScreen((HWND)ch,(POINT *)r);
  ClientToScreen((HWND)ch,((POINT *)r)+1);
  SWELL_END_TRY(return false;)

  return true;
}

void GetWindowContentViewRect(HWND hwnd, RECT *r)
{
  SWELL_BEGIN_TRY
  NSWindow *nswnd;
  if (hwnd && [(id)hwnd isKindOfClass:[NSView class]] && (nswnd=[(NSView *)hwnd window]) && [nswnd contentView]==(id)hwnd)
    hwnd=(HWND)nswnd;
    
  if (hwnd && [(id)hwnd isKindOfClass:[NSWindow class]])
  {
    NSView *ch=[(id)hwnd contentView];
    NSRECT_TO_RECT(r,[ch bounds]);
    ClientToScreen(hwnd,(POINT *)r);
    ClientToScreen(hwnd,((POINT *)r)+1);
  }
  else GetWindowRect(hwnd,r);
  SWELL_END_TRY(;)
}


void GetClientRect(HWND hwnd, RECT *r)
{
  r->left=r->top=r->right=r->bottom=0;
  if (WDL_NOT_NORMALLY(!hwnd)) return;
  
  SWELL_BEGIN_TRY
  id ch=(id)hwnd;
  if ([ch isKindOfClass:[NSWindow class]]) ch=[((NSWindow *)ch) contentView];
  if (!ch || ![ch isKindOfClass:[NSView class]]) return;
  ch=NavigateUpScrollClipViews(ch);
  
  NSRECT_TO_RECT(r,[ch bounds]);

  // todo this may need more attention
  NCCALCSIZE_PARAMS tr={{*r,},};
  SendMessage(hwnd,WM_NCCALCSIZE,FALSE,(LPARAM)&tr);
  r->right = r->left + (tr.rgrc[0].right-tr.rgrc[0].left);
  r->bottom = r->top + (tr.rgrc[0].bottom-tr.rgrc[0].top);
  SWELL_END_TRY(;)
}



void SetWindowPos(HWND hwnd, HWND hwndAfter, int x, int y, int cx, int cy, int flags)
{
  if (WDL_NOT_NORMALLY(!hwnd)) return;
 
  SWELL_BEGIN_TRY
  NSWindow *nswnd; // content views = move window
  if (hwnd && [(id)hwnd isKindOfClass:[NSView class]] && (nswnd=[(NSView *)hwnd window]) && [nswnd contentView]==(id)hwnd)
    hwnd=(HWND)nswnd;
 
 // todo: handle SWP_SHOWWINDOW
  id ch=(id)hwnd;
  bool isview=false;
  if ([ch isKindOfClass:[NSWindow class]] || (isview=[ch isKindOfClass:[NSView class]])) 
  {
    if (isview)
    {
      ch=NavigateUpScrollClipViews(ch);
      if (isview && !(flags&SWP_NOZORDER))
      {
        NSView *v = (NSView *)ch;
        NSView *par = [v superview];
        NSArray *subs = [par subviews];
        NSInteger idx = [subs indexOfObjectIdenticalTo:v], cnt=[subs count];
        
        NSView *viewafter = NULL;            
        NSWindowOrderingMode omode = NSWindowAbove;
        
        if (cnt>1 && hwndAfter != (HWND)ch)
        {
          if (hwndAfter==HWND_TOPMOST||hwndAfter==HWND_NOTOPMOST)
          {
          }
          else if (hwndAfter == HWND_TOP)
          {
            if (idx<cnt-1) viewafter = [subs objectAtIndex:cnt-1];
          }
          else if (hwndAfter == HWND_BOTTOM)
          {
            if (idx>0) viewafter = [subs objectAtIndex:0];
            omode = NSWindowBelow;
          }
          else 
          {
            NSInteger a=[subs indexOfObjectIdenticalTo:(NSView *)hwndAfter];
            if (a != NSNotFound && a != (idx-1)) viewafter = (NSView *)hwndAfter;
          }
        }
        
        if (viewafter)
        { 
          HWND h = GetCapture();
          if (!h || (h!=(HWND)v && !IsChild((HWND)v,h))) // if this window is captured don't reorder!
          {
            NSView *oldfoc = (NSView*)[[v window] firstResponder];
            if (!oldfoc || ![oldfoc isKindOfClass:[NSView class]] || 
                (oldfoc != v && ![oldfoc isDescendantOf:v])) oldfoc=NULL;
          
            // better way to do this? maybe :/
            [v retain];
            [v removeFromSuperviewWithoutNeedingDisplay];
            [par addSubview:v positioned:omode relativeTo:viewafter];
            [v release];
          
            if (oldfoc) [[v window] makeFirstResponder:oldfoc];
          }
        }
      }
    }    
    NSRect f=[ch frame];
    bool repos=false;
    if (!(flags&SWP_NOMOVE))
    {
      f.origin.x=(float)x;
      f.origin.y=(float)y;
      repos=true;
    }
    if (!(flags&SWP_NOSIZE))
    {
      f.size.width=(float)cx;
      f.size.height=(float)cy;
      if (f.size.height<0)f.size.height=-f.size.height;
      repos=true;
    }
    if (repos)
    {
      if (!isview)
      {
        NSSize mins=[ch minSize];
        NSSize maxs=[ch maxSize];
        if (f.size.width  < mins.width) f.size.width=mins.width;
        else if (f.size.width > maxs.width) f.size.width=maxs.width;
        if (f.size.height < mins.height) f.size.height=mins.height;
        else if (f.size.height> maxs.height) f.size.height=maxs.height;
        [ch setFrame:f display:NO];
        [ch display];
      }
      else
      {
        // this doesnt seem to actually be a good idea anymore
  //      if ([[ch window] contentView] != ch && ![[ch superview] isFlipped])
//          f.origin.y -= f.size.height;
        [ch setFrame:f];
        if ([ch isKindOfClass:[NSScrollView class]])
        {
          NSView *cv=[ch documentView];
          if (cv && [cv isKindOfClass:[NSTextView class]])
          {
            NSRect fr=[cv frame];
            NSSize sz=[ch contentSize];
            int a=0;
            if (![ch hasHorizontalScroller]) {a ++; fr.size.width=sz.width; }
            if (![ch hasVerticalScroller]) { a++; fr.size.height=sz.height; }
            if (a) [cv setFrame:fr];
          }
        }
      }
    }    
    return;
  }  
  SWELL_END_TRY(;)  
}

BOOL EnumWindows(BOOL (*proc)(HWND, LPARAM), LPARAM lp)
{
  NSArray *ch=[NSApp windows];
  [ch retain];
  const NSInteger n=[ch count];
  for(NSInteger x=0;x<n; x ++)
  {
    NSWindow *w = [ch objectAtIndex:x];
    if (!proc((HWND)[w contentView],lp)) 
    {
      [ch release];
      return FALSE;
    }
  }
  [ch release];
  return TRUE;
}


HWND GetWindow(HWND hwnd, int what)
{
  if (WDL_NOT_NORMALLY(!hwnd)) return 0;
  SWELL_BEGIN_TRY

  if ([(id)hwnd isKindOfClass:[NSWindow class]]) hwnd=(HWND)[(id)hwnd contentView];
  if (!hwnd || ![(id)hwnd isKindOfClass:[NSView class]]) return 0;
  
  NSView *v=(NSView *)hwnd;
  if (what == GW_CHILD)
  {
    NSArray *ar=[v subviews];
    if (ar && [ar count]>0)
    {
      return (HWND)[ar objectAtIndex:0];
    }
    return 0;
  }
  if (what == GW_OWNER)
  {
    v=NavigateUpScrollClipViews(v);
    if ([[v window] contentView] == v)
    {
      if ([[v window] respondsToSelector:@selector(swellGetOwner)])
      {
        return (HWND)[(SWELL_ModelessWindow*)[v window] swellGetOwner];
      }
      return 0;
    }
    return (HWND)[v superview];
  }
  
  if (what >= GW_HWNDFIRST && what <= GW_HWNDPREV)
  {
    v=NavigateUpScrollClipViews(v);
    if ([[v window] contentView] == v)
    {
      if (what <= GW_HWNDLAST) return (HWND)hwnd; // content view is only window
      
      return 0; // we're the content view so cant do next/prev
    }
    NSView *par=[v superview];
    if (par)
    {
      NSArray *ar=[par subviews];
      NSInteger cnt;
      if (ar && (cnt=[ar count]) > 0)
      {
        if (what == GW_HWNDFIRST)
          return (HWND)[ar objectAtIndex:0];
        if (what == GW_HWNDLAST)
          return (HWND)[ar objectAtIndex:(cnt-1)];
        
        NSInteger idx=[ar indexOfObjectIdenticalTo:v];
        if (idx == NSNotFound) return 0;

        if (what==GW_HWNDNEXT) idx++;
        else if (what==GW_HWNDPREV) idx--;
        
        if (idx<0 || idx>=cnt) return 0;
        
        return (HWND)[ar objectAtIndex:idx];
      }
    }
    return 0;
  }
  SWELL_END_TRY(;)
  return 0;
}


HWND GetParent(HWND hwnd)
{  
  SWELL_BEGIN_TRY
  if (WDL_NORMALLY(hwnd) && [(id)hwnd isKindOfClass:[NSView class]])
  {
    hwnd=(HWND)NavigateUpScrollClipViews((NSView *)hwnd);

    NSView *cv=[[(NSView *)hwnd window] contentView];
    if (cv == (NSView *)hwnd) hwnd=(HWND)[(NSView *)hwnd window]; // passthrough to get window parent
    else
    {
      HWND h=(HWND)[(NSView *)hwnd superview];
      return h;
    }
  }
  
  if (hwnd && [(id)hwnd isKindOfClass:[NSWindow class]]) 
  {
    HWND h= (HWND)[(NSWindow *)hwnd parentWindow];
    if (h) h=(HWND)[(NSWindow *)h contentView];
    if (h) return h;
  }
  
  if (hwnd && [(id)hwnd respondsToSelector:@selector(swellGetOwner)])
  {
    HWND h= (HWND)[(SWELL_ModelessWindow *)hwnd swellGetOwner];
    if (h && [(id)h isKindOfClass:[NSWindow class]]) h=(HWND)[(NSWindow *)h contentView];
    return h;  
  }
  
  SWELL_END_TRY(;)
  return 0;
}

HWND SetParent(HWND hwnd, HWND newPar)
{
  SWELL_BEGIN_TRY
  NSView *v=(NSView *)hwnd;
  WDL_ASSERT(hwnd != NULL);
  if (!v || ![(id)v isKindOfClass:[NSView class]]) return 0;
  v=NavigateUpScrollClipViews(v);
  
  if ([(id)hwnd isKindOfClass:[NSView class]])
  {
    NSView *tv=(NSView *)hwnd;
    if ([[tv window] contentView] == tv) // if we're reparenting a contentview (aka top level window)
    {
      if (!newPar) return NULL;
    
      NSView *npv = (NSView *)newPar;
      if ([npv isKindOfClass:[NSWindow class]]) npv=[(NSWindow *)npv contentView];
      if (!npv || ![npv isKindOfClass:[NSView class]])
        return NULL;
    
      char oldtitle[2048];
      oldtitle[0]=0;
      GetWindowText(hwnd,oldtitle,sizeof(oldtitle));
    
      NSWindow *oldwnd = [tv window];
      id oldown = NULL;
      if ([oldwnd respondsToSelector:@selector(swellGetOwner)]) oldown=[(SWELL_ModelessWindow*)oldwnd swellGetOwner];

      if ([tv isKindOfClass:[SWELL_hwndChild class]]) ((SWELL_hwndChild*)tv)->m_lastTopLevelOwner = oldown;
    
      [tv retain];
      SWELL_hwndChild *tmpview = [[SWELL_hwndChild alloc] initChild:nil Parent:(NSView *)oldwnd dlgProc:nil Param:0];          
      [tmpview release];
    
      [npv addSubview:tv];  
      [tv release];
    
      DestroyWindow((HWND)oldwnd); // close old window since its no longer used
      if (oldtitle[0]) SetWindowText(hwnd,oldtitle);
      return (HWND)npv;
    }
    else if (!newPar) // not content view, not parent (so making it a top level modeless dialog)
    {
      char oldtitle[2048];
      oldtitle[0]=0;
      GetWindowText(hwnd,oldtitle,sizeof(oldtitle));
      
      [tv retain];
      [tv removeFromSuperview];

    
      unsigned int wf=(NSTitledWindowMask|NSMiniaturizableWindowMask|NSClosableWindowMask|NSResizableWindowMask);
      if ([tv respondsToSelector:@selector(swellCreateWindowFlags)])
        wf=(unsigned int)[(SWELL_hwndChild *)tv swellCreateWindowFlags];

      HWND newOwner=NULL;
      if ([tv isKindOfClass:[SWELL_hwndChild class]])
      {
         id oldown = ((SWELL_hwndChild*)tv)->m_lastTopLevelOwner;
         if (oldown)
         {
           NSArray *ch=[NSApp windows];
           const NSInteger n = [ch count];
           for(NSInteger x=0;x<n && !newOwner; x ++)
           {
             NSWindow *w = [ch objectAtIndex:x];
             if (w == (NSWindow *)oldown || [w contentView] == (NSView *)oldown) newOwner = (HWND)w;
           }
         }
      }

      HWND SWELL_CreateModelessFrameForWindow(HWND childW, HWND ownerW, unsigned int);
      HWND bla=SWELL_CreateModelessFrameForWindow((HWND)tv,(HWND)newOwner,wf);
      // create a new modeless frame 

     
      
      [(NSWindow *)bla display];
      
      [tv release];
      
      if (oldtitle[0]) SetWindowText(hwnd,oldtitle);
      
      return NULL;
      
    }
  }
  HWND ret=(HWND) [v superview];
  if (ret) 
  {
    [v retain];
    [v removeFromSuperview];
  }
  NSView *np=(NSView *)newPar;
  if (np && [np isKindOfClass:[NSWindow class]]) np=[(NSWindow *)np contentView];
  
  if (np && [np isKindOfClass:[NSView class]])
  {
    [np addSubview:v];
    [v release];
  }
  return ret;
  SWELL_END_TRY(;)
  return NULL;
}


int IsChild(HWND hwndParent, HWND hwndChild)
{
  if (!hwndParent || !hwndChild || hwndParent == hwndChild) return 0;
  SWELL_BEGIN_TRY
  id par=(id)hwndParent;
  id ch=(id)hwndChild;
  if (![ch isKindOfClass:[NSView class]]) return 0;
  if ([par isKindOfClass:[NSWindow class]])
  {
    return [ch window] == par;
  }
  else if ([par isKindOfClass:[NSView class]])
  {
    return !![ch isDescendantOf:par];
  }
  SWELL_END_TRY(;)
  return 0;
}

HWND GetForegroundWindow()
{
  SWELL_BEGIN_TRY
  NSWindow *window=[NSApp keyWindow];
  if (!window) return 0;
  id ret=[window firstResponder];
  if (ret && [ret isKindOfClass:[NSView class]]) 
  {
//    if (ret == [window contentView]) return (HWND) window;
    return (HWND) ret;
  }
  return (HWND)window;
  SWELL_END_TRY(;)
  return NULL;
}

HWND GetFocus()
{
  SWELL_BEGIN_TRY
  NSWindow *window=[NSApp keyWindow];
  if (!window) return 0;
  id ret=[window firstResponder];
  if (ret && [ret isKindOfClass:[NSView class]]) 
  {
//    if (ret == [window contentView]) return (HWND) window;

    if ([ret isKindOfClass:[NSTextView class]] && [ret superview] && [[ret superview] superview])
    {
      NSView* v = [[ret superview] superview];
      if ([v isKindOfClass:[NSTextField class]]) return (HWND) v;
    }

    return (HWND) ret;
  }
  SWELL_END_TRY(;)
  return 0;
}

bool IsEquivalentTextView(HWND h1, HWND h2)
{
  if (!h1 || !h2) return false;
  if (h1 == h2) return true;
  SWELL_BEGIN_TRY
  NSView* v1 = (NSView*)h1;
  NSView* v2 = (NSView*)h2;
  if ([v1 isKindOfClass:[NSTextField class]] && [v2 isKindOfClass:[NSTextView class]])
  {
    NSView* t = v1;
    v1 = v2;
    v2 = t;
  }
  if ([v1 isKindOfClass: [NSTextView class]] && [v2 isKindOfClass:[NSTextField class]])
  {
    if ([v1 superview] && [[v1 superview] superview] && [[[v1 superview] superview] superview] == v2) return true;
  }
  SWELL_END_TRY(;)
  return false;
}
  


BOOL SetDlgItemText(HWND hwnd, int idx, const char *text)
{
  NSView *obj=(NSView *)(idx ? GetDlgItem(hwnd,idx) : hwnd);
  if (WDL_NOT_NORMALLY(!obj)) return false;
  
  SWELL_BEGIN_TRY
  NSWindow *nswnd;
  if ([(id)obj isKindOfClass:[NSView class]] && (nswnd=[(NSView *)obj window]) && [nswnd contentView]==(id)obj)
  {
    SetDlgItemText((HWND)nswnd,0,text); // also set window if setting content view
  }
  
  if ([obj respondsToSelector:@selector(onSwellSetText:)])
  {
    [(SWELL_hwndChild*)obj onSwellSetText:text];
    return TRUE;
  }
  
  BOOL rv=TRUE;  
  NSString *lbl=(NSString *)SWELL_CStringToCFString(text);
  if ([obj isKindOfClass:[NSWindow class]] || [obj isKindOfClass:[NSButton class]]) [(NSButton*)obj setTitle:lbl];
  else if ([obj isKindOfClass:[NSControl class]]) 
  {
    [(NSControl*)obj setStringValue:lbl];
    if ([obj isKindOfClass:[NSTextField class]] && [(NSTextField *)obj isEditable])
    {
      if (![obj isKindOfClass:[NSComboBox class]])
      {
        HWND par = GetParent((HWND)obj);
        if (par)
          SendMessage(par,WM_COMMAND,[(NSControl *)obj tag]|(EN_CHANGE<<16),(LPARAM)obj);
      }
    }
  }
  else if ([obj isKindOfClass:[NSText class]])  
  {
    // todo if there is a way to find out that the window's NSTextField is already assigned 
    // to another field, restore the assignment afterwards
    [(NSText*)obj setString:lbl];
    [obj setNeedsDisplay:YES]; // required on Sierra, it seems -- if the parent is hidden (e.g. DialogBox() + WM_INITDIALOG), the view is not drawn
  }
  else if ([obj isKindOfClass:[NSBox class]])
  {
    [(NSBox *)obj setTitle:lbl];
  }
  else
  {
    rv=FALSE;
  }
  
  [lbl release];
  return rv;
  SWELL_END_TRY(;)
  return FALSE;
}

int GetWindowTextLength(HWND hwnd)
{
  if (WDL_NOT_NORMALLY(!hwnd)) return 0;

  SWELL_BEGIN_TRY

  NSView *pvw = (NSView *)hwnd;
  if ([(id)pvw isKindOfClass:[NSView class]] && [[(id)pvw window] contentView] == pvw)
  {
    pvw=(NSView *)[(id)pvw window];
  }

  if ([(id)pvw respondsToSelector:@selector(onSwellGetText)])
  {
    const char *p=(const char *)[(SWELL_hwndChild*)pvw onSwellGetText];
    return p ? (int)strlen(p) : 0;
  }

  NSString *s;

  if ([pvw isKindOfClass:[NSButton class]]||[pvw isKindOfClass:[NSWindow class]]) s=[((NSButton *)pvw) title];
  else if ([pvw isKindOfClass:[NSControl class]]) s=[((NSControl *)pvw) stringValue];
  else if ([pvw isKindOfClass:[NSText class]])  s=[(NSText*)pvw string];
  else if ([pvw isKindOfClass:[NSBox class]]) s=[(NSBox *)pvw title];
  else return 0;

  const char *p = s ? [s UTF8String] : NULL;
  return p ? (int)strlen(p) : 0;

  SWELL_END_TRY(;)
  return 0;
}

BOOL GetDlgItemText(HWND hwnd, int idx, char *text, int textlen)
{
  *text=0;
  NSView *pvw=(NSView *)(idx?GetDlgItem(hwnd,idx) : hwnd);
  if (WDL_NOT_NORMALLY(!pvw)) return false;

  SWELL_BEGIN_TRY
  
  if ([(id)pvw isKindOfClass:[NSView class]] && [[(id)pvw window] contentView] == pvw)
  {
    pvw=(NSView *)[(id)pvw window];
  }
  
  if ([(id)pvw respondsToSelector:@selector(onSwellGetText)])
  {  
    const char *p=(const char *)[(SWELL_hwndChild*)pvw onSwellGetText];
    lstrcpyn_safe(text,p?p:"",textlen);
    return TRUE;
  }
  
  NSString *s;
  
  if ([pvw isKindOfClass:[NSButton class]]||[pvw isKindOfClass:[NSWindow class]]) s=[((NSButton *)pvw) title];
  else if ([pvw isKindOfClass:[NSControl class]]) s=[((NSControl *)pvw) stringValue];
  else if ([pvw isKindOfClass:[NSText class]])  s=[(NSText*)pvw string];
  else if ([pvw isKindOfClass:[NSBox class]]) s=[(NSBox *)pvw title];
  else return FALSE;
  
  if (s) SWELL_CFStringToCString(s,text,textlen);
//    [s getCString:text maxLength:textlen];
    
  return !!s;
  SWELL_END_TRY(;)
  return FALSE;
}

void CheckDlgButton(HWND hwnd, int idx, int check)
{
  NSView *pvw=(NSView *)GetDlgItem(hwnd,idx);
  if (WDL_NOT_NORMALLY(!pvw)) return;
  if ([pvw isKindOfClass:[NSButton class]]) 
    [(NSButton*)pvw setState:(check&BST_INDETERMINATE)?NSMixedState:((check&BST_CHECKED)?NSOnState:NSOffState)];
}


int IsDlgButtonChecked(HWND hwnd, int idx)
{
  NSView *pvw=(NSView *)GetDlgItem(hwnd,idx);
  if (WDL_NORMALLY(pvw && [pvw isKindOfClass:[NSButton class]]))
  {
    NSInteger a=[(NSButton*)pvw state];
    if (a==NSMixedState) return BST_INDETERMINATE;
    return a!=NSOffState;
  }
  return 0;
}

void SWELL_TB_SetPos(HWND hwnd, int idx, int pos)
{
  NSSlider *p=(NSSlider *)GetDlgItem(hwnd,idx);
  if (WDL_NORMALLY(p) && [p isKindOfClass:[NSSlider class]]) 
  {
    [p setDoubleValue:(double)pos];
  }
  else 
  {
    sendSwellMessage(p,TBM_SETPOS,1,pos); 
  }
}

void SWELL_TB_SetRange(HWND hwnd, int idx, int low, int hi)
{
  NSSlider *p=(NSSlider *)GetDlgItem(hwnd,idx);
  if (WDL_NORMALLY(p) && [p isKindOfClass:[NSSlider class]])
  {
    [p setMinValue:low];
    [p setMaxValue:hi];
  }
  else 
  {
    sendSwellMessage(p,TBM_SETRANGE,1,((low&0xffff)|(hi<<16)));
  }
  
}

int SWELL_TB_GetPos(HWND hwnd, int idx)
{
  NSSlider *p=(NSSlider *)GetDlgItem(hwnd,idx);
  if (WDL_NORMALLY(p) && [p isKindOfClass:[NSSlider class]]) 
  {
    return (int) ([p doubleValue]+0.5);
  }
  else 
  {
    return (int) sendSwellMessage(p,TBM_GETPOS,0,0);
  }
  return 0;
}

void SWELL_TB_SetTic(HWND hwnd, int idx, int pos)
{
  NSSlider *p=(NSSlider *)GetDlgItem(hwnd,idx);
  WDL_ASSERT(p != NULL);
  sendSwellMessage(p,TBM_SETTIC,0,pos);
}

void SWELL_CB_DeleteString(HWND hwnd, int idx, int wh)
{
  NSComboBox *p=(NSComboBox *)GetDlgItem(hwnd,idx);
  if (WDL_NOT_NORMALLY(!p)) return;
  if ([p isKindOfClass:[SWELL_ComboBox class]])
  {
    if (wh>=0 && wh<[p numberOfItems])
    {
      SWELL_ComboBox *s = (SWELL_ComboBox *)p;
      if (s->m_ignore_selchg == wh) s->m_ignore_selchg=-1;
      else if (s->m_ignore_selchg >= wh) s->m_ignore_selchg--;
      [p removeItemAtIndex:wh];
      if (s->m_ids) ((SWELL_ComboBox*)p)->m_ids->Delete(wh);
    }
  }
  else if ( [p isKindOfClass:[NSPopUpButton class]])
  {
    NSMenu *menu = [p menu];
    if (menu)
    {
      if (wh >= 0 && wh < [menu numberOfItems])
        [menu removeItemAtIndex:wh];
    }
  }
}


int SWELL_CB_FindString(HWND hwnd, int idx, int startAfter, const char *str, bool exact)
{
  NSComboBox *p=(NSComboBox *)GetDlgItem(hwnd,idx);  
  if (WDL_NOT_NORMALLY(!p)) return 0;
  
  int pos = startAfter;
  if (pos<0)pos=0;
  else pos++;
  
  const size_t l1len = strlen(str);
  const int ni=(int)[p numberOfItems];
  
  if ([p isKindOfClass:[NSComboBox class]])
  {
    for(;pos<ni;pos++)
    {
      NSString *s=[p itemObjectValueAtIndex:pos];
      if (s)
      {
        char buf[4096];
        SWELL_CFStringToCString(s,buf,sizeof(buf));
        if (exact ? !stricmp(str,buf) : !strnicmp(str,buf,l1len))
          return pos;
      }
    }
  }
  else 
  {
    for(;pos<ni;pos++)
    {
      NSMenuItem *i=[(NSPopUpButton *)p itemAtIndex:pos];
      if (i)
      {
        NSString *s=[i title];
        if (s)          
        {
          char buf[4096];
          SWELL_CFStringToCString(s,buf,sizeof(buf));
          if (exact ? !stricmp(str,buf) : !strnicmp(str,buf,l1len))
            return pos;
        }
      }
    }
  }
  return -1;
}

int SWELL_CB_GetItemText(HWND hwnd, int idx, int item, char *buf, int bufsz)
{
  NSComboBox *p=(NSComboBox *)GetDlgItem(hwnd,idx);

  if (buf) *buf=0;
  if (WDL_NOT_NORMALLY(!p)) return CB_ERR;
  const int ni = (int)[p numberOfItems];
  if (item < 0 || item >= ni) return CB_ERR;
  
  if ([p isKindOfClass:[NSComboBox class]])
  {
    NSString *s=[p itemObjectValueAtIndex:item];
    if (s)
    {
      if (!buf) return (int) ([s lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 64);

      SWELL_CFStringToCString(s,buf,bufsz);
      return 1;
    }
  }
  else 
  {
    NSMenuItem *i=[(NSPopUpButton *)p itemAtIndex:item];
    if (i)
    {
      NSString *s=[i title];
      if (s)
      {
        if (!buf) return (int) ([s lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 64);

        SWELL_CFStringToCString(s,buf,bufsz);
        return 1;
      }
    }
  }
  return CB_ERR;
}


int SWELL_CB_InsertString(HWND hwnd, int idx, int pos, const char *str)
{
  NSString *label=(NSString *)SWELL_CStringToCFString(str);
  NSComboBox *p=(NSComboBox *)GetDlgItem(hwnd,idx);
  if (WDL_NOT_NORMALLY(!p)) return 0;
  
  bool isAppend=false;
  const int ni = (int)[p numberOfItems];
  if (pos == -1000) 
  {
    isAppend=true;
    pos=ni;
  }
  else if (pos < 0) pos=0;
  else if (pos > ni) pos=ni;
  
   
  if ([p isKindOfClass:[SWELL_ComboBox class]])
  {
    SWELL_ComboBox *s = (SWELL_ComboBox *)p;
    if (isAppend && (((int)[s getSwellStyle]) & CBS_SORT))
    {
      pos=(int)arr_bsearch_mod(label,[p objectValues],_nsStringSearchProc);
    }
    
    if (s->m_ignore_selchg >= pos) s->m_ignore_selchg++;
    if (pos==ni)
      [p addItemWithObjectValue:label];
    else
      [p insertItemWithObjectValue:label atIndex:pos];
  
    if (s->m_ids) s->m_ids->Insert(pos,(char*)0);
    [p setNumberOfVisibleItems:(ni+1)];
  }
  else
  {
    NSMenu *menu = [(NSPopUpButton *)p menu];
    if (menu)
    {
      const bool needclearsel = [p indexOfSelectedItem] < 0;
      if (isAppend && [p respondsToSelector:@selector(getSwellStyle)] && (((int)[(SWELL_PopUpButton*)p getSwellStyle]) & CBS_SORT))
      {
        pos=(int)arr_bsearch_mod(label,[menu itemArray],_nsMenuSearchProc);
      }
      NSMenuItem *item=[menu insertItemWithTitle:label action:NULL keyEquivalent:@"" atIndex:pos];
      [item setEnabled:YES];      
      if (needclearsel) [(NSPopUpButton *)p selectItemAtIndex:-1];
    }
  }
  [label release];
  return pos;
  
}

int SWELL_CB_AddString(HWND hwnd, int idx, const char *str)
{
  return SWELL_CB_InsertString(hwnd,idx,-1000,str);
}

int SWELL_CB_GetCurSel(HWND hwnd, int idx)
{
  NSComboBox *p=(NSComboBox *)GetDlgItem(hwnd,idx);
  if (WDL_NOT_NORMALLY(!p)) return -1;
  return (int)[p indexOfSelectedItem];
}

void SWELL_CB_SetCurSel(HWND hwnd, int idx, int item)
{
  NSComboBox *cb = (NSComboBox *)GetDlgItem(hwnd,idx);
  if (WDL_NOT_NORMALLY(!cb)) return;
  const bool is_swell_cb = [cb isKindOfClass:[SWELL_ComboBox class]];

  if (item < 0 || item >= [cb numberOfItems])
  {
    // combo boxes can be NSComboBox or NSPopupButton, NSComboBox needs
    // a different deselect method (selectItemAtIndex:-1 throws an exception)
    if ([cb isKindOfClass:[NSComboBox class]])
    {
      const NSInteger sel = [cb indexOfSelectedItem];
      if (sel>=0) [cb deselectItemAtIndex:sel];
      if (is_swell_cb) ((SWELL_ComboBox *)cb)->m_ignore_selchg = -1;
    }
    else if ([cb isKindOfClass:[NSPopUpButton class]])
      [(NSPopUpButton*)cb selectItemAtIndex:-1];
  }
  else
  {
    if (is_swell_cb) ((SWELL_ComboBox *)cb)->m_ignore_selchg = item;
    [cb selectItemAtIndex:item];
  }
}

int SWELL_CB_GetNumItems(HWND hwnd, int idx)
{
  NSComboBox *p=(NSComboBox *)GetDlgItem(hwnd,idx);
  if (WDL_NOT_NORMALLY(!p)) return 0;
  return (int)[p numberOfItems];
}



void SWELL_CB_SetItemData(HWND hwnd, int idx, int item, LONG_PTR data)
{
  id cb=(id)GetDlgItem(hwnd,idx);
  if (WDL_NOT_NORMALLY(!cb)) return;

  if ([cb isKindOfClass:[NSPopUpButton class]])
  {
    if (item < 0 || item >= [cb numberOfItems]) return;
    NSMenuItem *it=[(NSPopUpButton*)cb itemAtIndex:item];
    if (!it) return;
  
    SWELL_DataHold *p=[[SWELL_DataHold alloc] initWithVal:(void *)data];  
    [it setRepresentedObject:p];
    [p release];
  }
  else if ([cb isKindOfClass:[SWELL_ComboBox class]])
  {
    if (item < 0 || item >= [cb numberOfItems]) return;
    if (((SWELL_ComboBox*)cb)->m_ids) ((SWELL_ComboBox*)cb)->m_ids->Set(item,(char*)data);
  }
}

LONG_PTR SWELL_CB_GetItemData(HWND hwnd, int idx, int item)
{
  id cb=(id)GetDlgItem(hwnd,idx);
  if (WDL_NOT_NORMALLY(!cb)) return 0;
  if ([cb isKindOfClass:[NSPopUpButton class]])
  {
    if (item < 0 || item >= [cb numberOfItems]) return 0;
    NSMenuItem *it=[(NSPopUpButton*)cb itemAtIndex:item];
    if (!it) return 0;
    id p= [it representedObject];
    if (!p || ![p isKindOfClass:[SWELL_DataHold class]]) return 0;
    return (LONG_PTR) (void *)[p getValue];
  }
  else if ([cb isKindOfClass:[SWELL_ComboBox class]])
  {
    if (item < 0 || item >= [cb numberOfItems]) return 0;
    if (((SWELL_ComboBox*)cb)->m_ids) return (LONG_PTR) ((SWELL_ComboBox*)cb)->m_ids->Get(item);    
  }
  return 0;
}

void SWELL_CB_Empty(HWND hwnd, int idx)
{
  id cb=(id)GetDlgItem(hwnd,idx);
  if (WDL_NOT_NORMALLY(!cb)) return;  
  if ([cb isKindOfClass:[NSPopUpButton class]] ||
      [cb isKindOfClass:[NSComboBox class]]) [cb removeAllItems];
  
  if ([cb isKindOfClass:[SWELL_ComboBox class]])
  {
    SWELL_ComboBox *p = (SWELL_ComboBox *)cb;
    p->m_ignore_selchg = -1;
    if (p->m_ids) p->m_ids->Empty();
  }
}


BOOL SetDlgItemInt(HWND hwnd, int idx, int val, int issigned)
{
  char buf[128];
  sprintf(buf,issigned?"%d":"%u",val);
  return SetDlgItemText(hwnd,idx,buf);
}

int GetDlgItemInt(HWND hwnd, int idx, BOOL *translated, int issigned)
{
  char buf[128];
  if (!GetDlgItemText(hwnd,idx,buf,sizeof(buf)))
  {
    if (translated) *translated=0;
    return 0;
  }
  char *p=buf;
  while (*p == ' ' || *p == '\t') p++;
  int a=atoi(p);
  if ((a<0 && !issigned) || (!a && p[0] != '0')) { if (translated) *translated=0; return 0; }
  if (translated) *translated=1;
  return a;
}

void SWELL_HideApp()
{
  [NSApp hide:NSApp];
}


BOOL SWELL_GetGestureInfo(LPARAM lParam, GESTUREINFO* gi)
{
  if (!lParam || !gi) return FALSE;
  memcpy(gi, (GESTUREINFO*)lParam, sizeof(GESTUREINFO));
  return TRUE;
}
  

void ShowWindow(HWND hwnd, int cmd)
{
  id pid=(id)hwnd;
  
  if (WDL_NORMALLY(pid) && [pid isKindOfClass:[NSWindow class]])
  {
    if (cmd == SW_SHOWNA && [pid isKindOfClass:[SWELL_ModelessWindow class]])
    {
      if (((SWELL_ModelessWindow *)pid)->m_wantInitialKeyWindowOnShow)
      {
        ((SWELL_ModelessWindow *)pid)->m_wantInitialKeyWindowOnShow=false;
        cmd = SW_SHOW;
      }
    }
    if (cmd==SW_SHOW)
    {
      [pid makeKeyAndOrderFront:pid];
    }
    else if (cmd==SW_SHOWNA)
    {
      [pid orderFront:pid];
    }
    else if (cmd==SW_HIDE)
    {
      [pid orderOut:pid];
    }
    else if (cmd == SW_SHOWMINIMIZED)
    {   
      // this ought to work
      //if ([NSApp mainWindow] == pid)
      //{
      //  [NSApp hide:pid];
      //}
      //else
      //{
        [pid miniaturize:pid];
      //}
    }
    return;
  }
  if (!pid || ![pid isKindOfClass:[NSView class]]) return;
  
  pid=NavigateUpScrollClipViews(pid);
  
  switch (cmd)
  {
    case SW_SHOW:
    case SW_SHOWNA:
      [((NSView *)pid) setHidden:NO];
    break;
    case SW_HIDE:
      {
        NSWindow *pw=[pid window];
        if (pw)
        {
          id foc=[pw firstResponder];
          if (foc && (foc == pid || IsChild((HWND)pid,(HWND)foc)))
          {
            [pw makeFirstResponder:nil];
          }
        }
        if (![((NSView *)pid) isHidden])
        {
          if ((NSView *)pid != [pw contentView])
          {
            HWND par = (HWND) [(NSView *)pid superview];
            if (par)
            {
              RECT r;
              GetWindowRect((HWND)pid,&r);
              ScreenToClient(par,(LPPOINT)&r);
              ScreenToClient(par,((LPPOINT)&r)+1);
              InvalidateRect(par,&r,FALSE);
            }
          }
          [((NSView *)pid) setHidden:YES];
        }
    }
    break;
  }
  
  NSWindow *nswnd;
  if ((nswnd=[(NSView *)pid window]) && [nswnd contentView]==(id)pid)
  {
    ShowWindow((HWND)nswnd,cmd);
  }
}

void *SWELL_ModalWindowStart(HWND hwnd)
{
  if (hwnd && [(id)hwnd isKindOfClass:[NSView class]]) hwnd=(HWND)[(NSView *)hwnd window];
  if (WDL_NOT_NORMALLY(!hwnd)) return 0;
  return (void *)[NSApp beginModalSessionForWindow:(NSWindow *)hwnd];
}

bool SWELL_ModalWindowRun(void *ctx, int *ret) // returns false and puts retval in *ret when done
{
  if (!ctx) return false;
  NSInteger r=[NSApp runModalSession:(NSModalSession)ctx];
  if (r==NSRunContinuesResponse) return true;
  if (ret) *ret=(int)r;
  return false;
}

void SWELL_ModalWindowEnd(void *ctx)
{
  if (ctx) 
  {
    if ([NSApp runModalSession:(NSModalSession)ctx] == NSRunContinuesResponse)
    {    
      [NSApp stopModal];
      while ([NSApp runModalSession:(NSModalSession)ctx]==NSRunContinuesResponse) Sleep(10);
    }
    [NSApp endModalSession:(NSModalSession)ctx];
  }
}

void SWELL_CloseWindow(HWND hwnd)
{
  if (WDL_NORMALLY(hwnd) && [(id)hwnd isKindOfClass:[NSWindow class]])
  {
    [((NSWindow*)hwnd) close];
  }
  else if (hwnd && [(id)hwnd isKindOfClass:[NSView class]])
  {
    [[(NSView*)hwnd window] close];
  }
}


#include "swell-dlggen.h"

static id m_make_owner;
static NSRect m_transform;
static float m_parent_h;
static bool m_doautoright;
static NSRect m_lastdoauto;
static bool m_sizetofits;
static int m_make_radiogroupcnt;

#define ACTIONTARGET (m_make_owner)

void SWELL_MakeSetCurParms(float xscale, float yscale, float xtrans, float ytrans, HWND parent, bool doauto, bool dosizetofit)
{
  m_make_radiogroupcnt=0;
  m_sizetofits=dosizetofit;
  m_lastdoauto.origin.x = 0;
  m_lastdoauto.origin.y = -100;
  m_lastdoauto.size.width = 0;
  m_doautoright=doauto;
  m_transform.origin.x=xtrans;
  m_transform.origin.y=ytrans;
  m_transform.size.width=xscale;
  m_transform.size.height=yscale;
  m_make_owner=(id)parent;
  if ([m_make_owner isKindOfClass:[NSWindow class]]) m_make_owner=[(NSWindow *)m_make_owner contentView];
  m_parent_h=100.0;
  if ([(id)m_make_owner isKindOfClass:[NSView class]])
  {
    m_parent_h=[(NSView *)m_make_owner bounds].size.height;
    if (m_transform.size.height > 0 && [(id)parent isFlipped])
      m_transform.size.height*=-1;
  }
}

static void UpdateAutoCoords(NSRect r)
{
  m_lastdoauto.size.width=r.origin.x + r.size.width - m_lastdoauto.origin.x;
}

static NSRect MakeCoords(int x, int y, int w, int h, bool wantauto, bool ignorevscaleheight=false)
{
  if (w<0&&h<0)
  {
    return NSMakeRect(-x,-y,-w,-h);
  }
  float ysc=m_transform.size.height;
  float ysc2 = ignorevscaleheight ? 1.0 : ysc;
  int newx=(int)((x+m_transform.origin.x)*m_transform.size.width + 0.5);
  int newy=(int)((ysc >= 0.0 ? m_parent_h - ((y+m_transform.origin.y) )*ysc + h*ysc2 : 
                         ((y+m_transform.origin.y) )*-ysc) + 0.5);
                         
  NSRect ret= NSMakeRect(newx,  
                         newy,                  
                        (int) (w*m_transform.size.width+0.5),
                        (int) (h*fabs(ysc2)+0.5));
                        
  NSRect oret=ret;
  if (wantauto && m_doautoright)
  {
    float dx = ret.origin.x - m_lastdoauto.origin.x;
    if (fabs(dx)<32 && m_lastdoauto.origin.y >= ret.origin.y && m_lastdoauto.origin.y < ret.origin.y + ret.size.height)
    {
      ret.origin.x += (int) m_lastdoauto.size.width;
    }
    
    m_lastdoauto.origin.x = oret.origin.x + oret.size.width;
    m_lastdoauto.origin.y = ret.origin.y + ret.size.height*0.5;
    m_lastdoauto.size.width=0;
  }
  return ret;
}

static const double minwidfontadjust=1.81;
#define TRANSFORMFONTSIZE (m_transform.size.width<1?8:m_transform.size.width<2?10:12)
/// these are for swell-dlggen.h
HWND SWELL_MakeButton(int def, const char *label, int idx, int x, int y, int w, int h, int flags)
{  
  UINT_PTR a=(UINT_PTR)label;
  if (a < 65536) label = "ICONTEMP";
  SWELL_Button *button=[[SWELL_Button alloc] init];
  if (flags & BS_BITMAP)
  {
    SWELL_ImageButtonCell * cell = [[SWELL_ImageButtonCell alloc] init];
    [button setCell:cell];
    [cell release];
  }
  
  if (m_transform.size.width < minwidfontadjust)
  {
    [button setFont:[NSFont systemFontOfSize:TRANSFORMFONTSIZE]];
  }
  
  [button setTag:idx];
  if (g_swell_want_nice_style==1)
    [button setBezelStyle:NSShadowlessSquareBezelStyle ];
  else
    [button setBezelStyle:NSRoundedBezelStyle ];
  NSRect tr=MakeCoords(x,y,w,h,true);
  
  
  if (g_swell_want_nice_style!=1 && tr.size.height >= 18 && tr.size.height<24)
  {
    tr.size.height=24;
  }
  
  [button setFrame:tr];
  NSString *labelstr=(NSString *)SWELL_CStringToCFString_FilterPrefix(label);
  [button setTitle:labelstr];
  [button setTarget:ACTIONTARGET];
  [button setAction:@selector(onSwellCommand:)];
  if ((flags & BS_XPOSITION_MASK) == BS_LEFT) [button setAlignment:NSLeftTextAlignment];
  if (flags&SWELL_NOT_WS_VISIBLE) [button setHidden:YES];
  [m_make_owner addSubview:button];
  if (m_doautoright) UpdateAutoCoords([button frame]);
  if (def) [[m_make_owner window] setDefaultButtonCell:(NSButtonCell*)button];
  [labelstr release];
  [button release];
  return (HWND) button;
}


@implementation SWELL_TextView

STANDARD_CONTROL_NEEDSDISPLAY_IMPL("Edit")

-(NSInteger) tag
{
  return m_tag;
}

-(void) setTag:(NSInteger)tag
{
  m_tag=tag;
}

-(LRESULT)onSwellMessage:(UINT)msg p1:(WPARAM)wParam p2:(LPARAM)lParam
{
  switch (msg)
  {
    case EM_SCROLL:
      if (wParam == SB_TOP)
      {
        [self scrollRangeToVisible:NSMakeRange(0, 0)];
      }
      else if (wParam == SB_BOTTOM)
      {
        NSUInteger len = [[self string] length];
        [self scrollRangeToVisible:NSMakeRange(len, 0)];
      }
    return 0;
    
    case EM_SETSEL:    
    {
      NSUInteger sl =  [[self string] length];
      if (wParam == -1) lParam = wParam = 0;
      else if (lParam == -1) lParam = sl;
      
      if (wParam>sl)wParam=sl;
      if (lParam>sl)lParam=sl;
      [self setSelectedRange:NSMakeRange(wParam, lParam>wParam ? lParam-wParam : 0)];
    }
    return 0;
    
    case EM_GETSEL:
    {
      NSRange r = [self selectedRange];
      if (wParam) *(int*)wParam = (int)r.location;
      if (lParam) *(int*)lParam = (int)(r.location+r.length);
    }
    return 0;
    case EM_REPLACESEL:
      if (lParam)
      {
        NSTextStorage *ts = [self textStorage];
        if (ts)
        {
          NSRange r = [self selectedRange];
          const char *s = (const char *)lParam;
          NSString *str = *s ? (NSString*)SWELL_CStringToCFString(s) : NULL;

          if (r.length > 0 && !str)
            [ts deleteCharactersInRange:r];
          else if (str)
            [ts replaceCharactersInRange:r withString:str];

          if (str) [str release];
        }
      }
    return 0;
      
    case WM_SETFONT:
    {
      HGDIOBJ__* obj = (HGDIOBJ__*)wParam;
      if (obj && obj->type == TYPE_FONT)
      {
        if (obj->ct_FontRef)
        {
          [self setFont:(NSFont *)obj->ct_FontRef];
        }
#ifdef SWELL_ATSUI_TEXT_SUPPORT
        else if (obj->atsui_font_style)
        {
          ATSUFontID fontid = kATSUInvalidFontID;      
          Fixed fsize = 0;          
          Boolean isbold = NO;
          Boolean isital = NO;
          Boolean isunder = NO;          
          if (ATSUGetAttribute(obj->atsui_font_style, kATSUFontTag, sizeof(ATSUFontID), &fontid, 0) == noErr &&
              ATSUGetAttribute(obj->atsui_font_style, kATSUSizeTag, sizeof(Fixed), &fsize, 0) == noErr && fsize &&
              ATSUGetAttribute(obj->atsui_font_style, kATSUQDBoldfaceTag, sizeof(Boolean), &isbold, 0) == noErr && 
              ATSUGetAttribute(obj->atsui_font_style, kATSUQDItalicTag, sizeof(Boolean), &isital, 0) == noErr &&
              ATSUGetAttribute(obj->atsui_font_style, kATSUQDUnderlineTag, sizeof(Boolean), &isunder, 0) == noErr)
          {
            char name[255];
            name[0]=0;
            ByteCount namelen=0;
            if (ATSUFindFontName(fontid, kFontFullName, (FontPlatformCode)kFontNoPlatform, kFontNoScriptCode, kFontNoLanguageCode, sizeof(name), name, &namelen, 0) == noErr && name[0] && namelen)
            {
              namelen /= 2;
              int i;
              for (i = 0; i < namelen; ++i) name[i] = name[2*i];
              name[namelen]=0;

              // todo bold/ital/underline
              NSString* str = (NSString*)SWELL_CStringToCFString(name);
              CGFloat sz = Fix2Long(fsize);
              NSFont* font = [NSFont fontWithName:str size:sz];
              [str release];
              if (font) 
              {
                [self setFont:font];
              }
            }
          }            
        }
#endif
      }
    }
    return 0;
  }
  return 0;
}

- (BOOL)becomeFirstResponder;
{
  BOOL didBecomeFirstResponder = [super becomeFirstResponder];
  if (didBecomeFirstResponder) SendMessage(GetParent((HWND)self),WM_COMMAND,[self tag]|(EN_SETFOCUS<<16),(LPARAM)self);
  return didBecomeFirstResponder;
}
@end

@implementation SWELL_TextField
STANDARD_CONTROL_NEEDSDISPLAY_IMPL([self isSelectable] ? "Edit" : "Static")

- (BOOL)becomeFirstResponder;
{
  BOOL didBecomeFirstResponder = [super becomeFirstResponder];
  if (didBecomeFirstResponder) SendMessage(GetParent((HWND)self),WM_COMMAND,[self tag]|(EN_SETFOCUS<<16),(LPARAM)self);
  return didBecomeFirstResponder;
}
- (void)initColors:(int)darkmode
{
  if (darkmode >= 0)
  {
    m_ctlcolor_set = false;
    m_last_dark_mode = darkmode ? 1 : 0;
  }

  if ([self isEditable])
  {
    if (SWELL_osx_is_dark_mode(1))
    {
      if (m_last_dark_mode)
        [self setBackgroundColor:[NSColor windowBackgroundColor]];
      else
        [self setBackgroundColor:[NSColor textBackgroundColor]];
    }
    else
    {
      if (g_swell_osx_readonlytext_wndbg)
        [self setBackgroundColor:[NSColor textBackgroundColor]];
    }
  }
  else if (![self isBordered] && ![self drawsBackground]) // looks like a static text control
  {
    NSColor *col;
    if (!m_ctlcolor_set && SWELL_osx_is_dark_mode(1))
      col = [NSColor textColor];
    else
      col = [self textColor];

    float alpha = ([self isEnabled] ? 1.0f : 0.5f);
    [self setTextColor:[col colorWithAlphaComponent:alpha]];
  }
  else
  {
    // not editable
    if (g_swell_osx_readonlytext_wndbg)
      [self setBackgroundColor:[NSColor windowBackgroundColor]];
  }
}

- (void) drawRect:(NSRect)r
{
  if (!m_ctlcolor_set && SWELL_osx_is_dark_mode(1))
  {
    const bool m = SWELL_osx_is_dark_mode(0);
    if (m != m_last_dark_mode) [self initColors:m];
  }
  [super drawRect:r];
}
@end



HWND SWELL_MakeEditField(int idx, int x, int y, int w, int h, int flags)
{  
  if ((flags&WS_VSCROLL) || (flags&WS_HSCROLL)) // || (flags & ES_READONLY))
  {
    SWELL_TextView *obj=[[SWELL_TextView alloc] init];
    [obj setEditable:(flags & ES_READONLY)?NO:YES];
    if (m_transform.size.width < minwidfontadjust)
      [obj setFont:[NSFont systemFontOfSize:TRANSFORMFONTSIZE]];
    [obj setTag:idx];
    [obj setDelegate:ACTIONTARGET];
    [obj setRichText:NO];

    [obj setHorizontallyResizable:NO];
    
    if (flags & WS_VSCROLL)
    {
      NSRect fr=MakeCoords(x,y,w,h,true);
      
      [obj setVerticallyResizable:YES];
      NSScrollView *obj2=[[NSScrollView alloc] init];
      [obj2 setFrame:fr];
      if (flags&WS_VSCROLL) [obj2 setHasVerticalScroller:YES];
      if (flags&WS_HSCROLL) 
      {
        [obj2 setHasHorizontalScroller:YES];
        [obj setMaxSize:NSMakeSize(FLT_MAX, FLT_MAX)];
        [obj setHorizontallyResizable:YES];
        [[obj textContainer] setWidthTracksTextView:NO];
        [[obj textContainer] setContainerSize:NSMakeSize(FLT_MAX, FLT_MAX)];
      }
      [obj2 setAutohidesScrollers:YES];
      [obj2 setDrawsBackground:NO];
      [obj2 setDocumentView:obj];
      [m_make_owner addSubview:obj2];
      if (m_doautoright) UpdateAutoCoords([obj2 frame]);
      if (flags&SWELL_NOT_WS_VISIBLE) [obj2 setHidden:YES];
      [obj2 release];
      
      NSRect tr;
      memset(&tr,0,sizeof(tr));
      tr.size = [obj2 contentSize];
      [obj setFrame:tr];
      [obj release];
      
      return (HWND)obj2;
    }
    else
    {
      [obj setFrame:MakeCoords(x,y,w,h,true)];
      [obj setVerticallyResizable:NO];
      if (flags&SWELL_NOT_WS_VISIBLE) [obj setHidden:YES];
      [m_make_owner addSubview:obj];
      if (m_doautoright) UpdateAutoCoords([obj frame]);
      [obj release];
      return (HWND)obj;
    }  
  }  
  
  NSTextField *obj;
  
  if (flags & ES_PASSWORD) obj=[[NSSecureTextField alloc] init];
  else obj=[[SWELL_TextField alloc] init];
  [obj setEditable:(flags & ES_READONLY)?NO:YES];
  if (flags & ES_READONLY) [obj setSelectable:YES];
  if (m_transform.size.width < minwidfontadjust)
    [obj setFont:[NSFont systemFontOfSize:TRANSFORMFONTSIZE]];
  
  if ([obj isKindOfClass:[SWELL_TextField class]])
    [(SWELL_TextField *)obj initColors:SWELL_osx_is_dark_mode(0)];

  NSCell* cell = [obj cell];  
  if (flags&ES_CENTER) [cell setAlignment:NSCenterTextAlignment];
  else if (flags&ES_RIGHT) [cell setAlignment:NSRightTextAlignment];
  if (abs(h) < 20)
  {
    [cell setWraps:NO];
    [cell setScrollable:YES];
  }
  [obj setTag:idx];
  [obj setTarget:ACTIONTARGET];
  [obj setAction:@selector(onSwellCommand:)];
  [obj setDelegate:ACTIONTARGET];
  
  [obj setFrame:MakeCoords(x,y,w,h,true)];
  if (flags&SWELL_NOT_WS_VISIBLE) [obj setHidden:YES];
  [m_make_owner addSubview:obj];
  if (m_doautoright) UpdateAutoCoords([obj frame]);
  [obj release];

  return (HWND)obj;
}

HWND SWELL_MakeLabel( int align, const char *label, int idx, int x, int y, int w, int h, int flags)
{
  NSTextField *obj=[[SWELL_TextField alloc] init];
  [obj setEditable:NO];
  [obj setSelectable:NO];
  [obj setBordered:NO];
  [obj setBezeled:NO];
  [obj setDrawsBackground:NO];
  if (m_transform.size.width < minwidfontadjust)
    [obj setFont:[NSFont systemFontOfSize:TRANSFORMFONTSIZE]];

  if (flags & SS_NOTIFY)
  {
    [obj setTarget:ACTIONTARGET];
    [obj setAction:@selector(onSwellCommand:)];
  }
  
  NSString *labelstr=(NSString *)SWELL_CStringToCFString_FilterPrefix(label);
  [obj setStringValue:labelstr];
  [obj setAlignment:(align<0?NSLeftTextAlignment:align>0?NSRightTextAlignment:NSCenterTextAlignment)];
  
  [[obj cell] setWraps:(h>12 ? YES : NO)];
  
  [obj setTag:idx];
  [obj setFrame:MakeCoords(x,y,w,h,true)];
  if (flags&SWELL_NOT_WS_VISIBLE) [obj setHidden:YES];
  [m_make_owner addSubview:obj];
  if (m_sizetofits && strlen(label)>1)[obj sizeToFit];
  if (m_doautoright) UpdateAutoCoords([obj frame]);
  [obj release];
  [labelstr release];
  return (HWND)obj;
}


HWND SWELL_MakeCheckBox(const char *name, int idx, int x, int y, int w, int h, int flags=0)
{
  return SWELL_MakeControl(name,idx,"Button",BS_AUTOCHECKBOX|flags,x,y,w,h,0);
}

HWND SWELL_MakeListBox(int idx, int x, int y, int w, int h, int styles)
{
  HWND hw=SWELL_MakeControl("",idx,"SysListView32_LB",styles,x,y,w,h,0);
/*  if (hw)
  {
    LVCOLUMN lvc={0,};
    RECT r;
    GetClientRect(hw,&r);
    lvc.cx=300;//yer.right-r.left;
    lvc.pszText="";
    ListView_InsertColumn(hw,0,&lvc);
  }
  */
  return hw;
}


typedef struct ccprocrec
{
  SWELL_ControlCreatorProc proc;
  int cnt;
  struct ccprocrec *next;
} ccprocrec;

static ccprocrec *m_ccprocs;

void SWELL_RegisterCustomControlCreator(SWELL_ControlCreatorProc proc)
{
  if (!proc) return;
  
  ccprocrec *p=m_ccprocs;
  while (p && p->next)
  {
    if (p->proc == proc)
    {
      p->cnt++;
      return;
    }
    p=p->next;
  }
  ccprocrec *ent = (ccprocrec*)malloc(sizeof(ccprocrec));
  ent->proc=proc;
  ent->cnt=1;
  ent->next=0;
  
  if (p) p->next=ent;
  else m_ccprocs=ent;
}

void SWELL_UnregisterCustomControlCreator(SWELL_ControlCreatorProc proc)
{
  if (!proc) return;
  
  ccprocrec *lp=NULL;
  ccprocrec *p=m_ccprocs;
  while (p)
  {
    if (p->proc == proc)
    {
      if (--p->cnt <= 0)
      {
        if (lp) lp->next=p->next;
        else m_ccprocs=p->next;
        free(p);
      }
      return;
    }
    lp=p;
    p=p->next;
  }
}


HWND SWELL_MakeControl(const char *cname, int idx, const char *classname, int style, int x, int y, int w, int h, int exstyle)
{
  if (m_ccprocs)
  {
    NSRect wcr=MakeCoords(x,y,w,h,false);
    ccprocrec *p=m_ccprocs;
    while (p)
    {
      HWND hwnd=p->proc((HWND)m_make_owner,cname,idx,classname,style,
          (int)(wcr.origin.x+0.5),(int)(wcr.origin.y+0.5),(int)(wcr.size.width+0.5),(int)(wcr.size.height+0.5));
      if (hwnd) 
      {
        if (exstyle) SetWindowLong(hwnd,GWL_EXSTYLE,exstyle);
        return hwnd;
      }
      p=p->next;
    }
  }
  if (!stricmp(classname,"SysTabControl32"))
  {
    SWELL_TabView *obj=[[SWELL_TabView alloc] init];
    if (1) // todo: only if on 10.4 maybe?
    {
      y-=1;
      h+=6;
    }
    [obj setTag:idx];
    [obj setDelegate:(id)obj];
    [obj setAllowsTruncatedLabels:YES];
    [obj setNotificationWindow:ACTIONTARGET];
    [obj setHidden:NO];
    [obj setFrame:MakeCoords(x,y,w,h,false)];
    if (style&SWELL_NOT_WS_VISIBLE) [obj setHidden:YES];
    [m_make_owner addSubview:obj];
    SetAllowNoMiddleManRendering((HWND)m_make_owner,FALSE);
    [obj release];
    return (HWND)obj;
  }
  else if (!stricmp(classname, "SysListView32")||!stricmp(classname, "SysListView32_LB"))
  {
    SWELL_ListView *obj = [[SWELL_ListView alloc] init];
    [obj setColumnAutoresizingStyle:NSTableViewNoColumnAutoresizing];
    [obj setFocusRingType:NSFocusRingTypeNone];
    [obj setDataSource:(id)obj];
    obj->style=style;

    BOOL isLB=!stricmp(classname, "SysListView32_LB");
    [obj setSwellNotificationMode:isLB];
    
    if (isLB)
    {
      [obj setHeaderView:nil];
      [obj setAllowsMultipleSelection:!!(style & LBS_EXTENDEDSEL)];
    }
    else
    {
      if ((style & LVS_NOCOLUMNHEADER) || !(style & LVS_REPORT))  [obj setHeaderView:nil];
      [obj setAllowsMultipleSelection:!(style & LVS_SINGLESEL)];
    }
    [obj setAllowsColumnReordering:NO];
    [obj setAllowsEmptySelection:YES];
    [obj setTag:idx];
    [obj setHidden:NO];
    id target=ACTIONTARGET;
    [obj setDelegate:target];
    [obj setTarget:target];
    [obj setAction:@selector(onSwellCommand:)];
    if ([target respondsToSelector:@selector(swellOnControlDoubleClick:)])
    {
      [obj setDoubleAction:@selector(swellOnControlDoubleClick:)];
    }
    else
    {
      [obj setDoubleAction:@selector(onSwellCommand:)];
    }
    NSScrollView *obj2=[[NSScrollView alloc] init];
    NSRect tr=MakeCoords(x,y,w,h,false);
    [obj2 setFrame:tr];
    [obj2 setDocumentView:obj];
    [obj2 setHasVerticalScroller:YES];
    if (!isLB) [obj2 setHasHorizontalScroller:YES];
    [obj2 setAutohidesScrollers:YES];
    [obj2 setDrawsBackground:NO];
    [obj release];
    if (style&SWELL_NOT_WS_VISIBLE) [obj2 setHidden:YES];
    [m_make_owner addSubview:obj2];
    [obj2 release];
    
    if (isLB || !(style & LVS_REPORT))
    {
      LVCOLUMN lvc={0,};
      lvc.mask=LVCF_TEXT|LVCF_WIDTH;
      lvc.cx=(int)ceil(wdl_max(tr.size.width - 4.0,isLB ? 1200.0 : 300.0));
      lvc.pszText=(char*)"";
      ListView_InsertColumn((HWND)obj,0,&lvc);
      if (isLB && (style & LBS_OWNERDRAWFIXED))
      {
        NSArray *ar=[obj tableColumns];
        NSTableColumn *c;
        if (ar && [ar count] && (c=[ar objectAtIndex:0]))
        {
          SWELL_ODListViewCell *t=[[SWELL_ODListViewCell alloc] init];
          [c setDataCell:t];
          [t setOwnerControl:obj];
          [t release];
        }
      }
    }
    
    return (HWND)obj;
  }
  else if (!stricmp(classname, "SysTreeView32"))
  {
    SWELL_TreeView *obj = [[SWELL_TreeView alloc] init];
    [obj setFocusRingType:NSFocusRingTypeNone];
    [obj setDataSource:(id)obj];
    obj->style=style;
    id target=ACTIONTARGET;
    [obj setHeaderView:nil];    
    [obj setDelegate:target];
    [obj setAllowsColumnReordering:NO];
    [obj setAllowsMultipleSelection:NO];
    [obj setAllowsEmptySelection:YES];
    [obj setTag:idx];
    [obj setHidden:NO];
    [obj setTarget:target];
    [obj setAction:@selector(onSwellCommand:)];
    if ([target respondsToSelector:@selector(swellOnControlDoubleClick:)])
      [obj setDoubleAction:@selector(swellOnControlDoubleClick:)];
    else
      [obj setDoubleAction:@selector(onSwellCommand:)];
    NSScrollView *obj2=[[NSScrollView alloc] init];
    NSRect tr=MakeCoords(x,y,w,h,false);
    [obj2 setFrame:tr];
    [obj2 setDocumentView:obj];
    [obj2 setHasVerticalScroller:YES];
    [obj2 setAutohidesScrollers:YES];
    [obj2 setDrawsBackground:NO];
    [obj release];
    if (style&SWELL_NOT_WS_VISIBLE) [obj2 setHidden:YES];
    [m_make_owner addSubview:obj2];
    [obj2 release];

    {
      NSTableColumn *col=[[NSTableColumn alloc] init];
      SWELL_ListViewCell *cell = [[SWELL_ListViewCell alloc] initTextCell:@""];
      [col setDataCell:cell];
      [cell release];

      [col setWidth:(int)ceil(wdl_max(tr.size.width,300.0))];
      [col setEditable:NO];
      [[col dataCell] setWraps:NO];     
      [obj addTableColumn:col];
      [obj setOutlineTableColumn:col];

      [col release];
    }
///    [obj setIndentationPerLevel:10.0];
    
    return (HWND)obj;
  }
  else if (!stricmp(classname, "msctls_progress32"))
  {
    SWELL_ProgressView *obj=[[SWELL_ProgressView alloc] init];
    [obj setStyle:NSProgressIndicatorBarStyle];
    [obj setIndeterminate:NO];
    [obj setTag:idx];
    [obj setFrame:MakeCoords(x,y,w,h,false)];
    if (style&SWELL_NOT_WS_VISIBLE) [obj setHidden:YES];
    [m_make_owner addSubview:obj];
    [obj release];
    return (HWND)obj;
  }
  else if (!stricmp(classname,"Edit"))
  {
    return SWELL_MakeEditField(idx,x,y,w,h,style);
  }
  else if (!stricmp(classname, "Static"))
  {
    if ((style&SS_TYPEMASK) == SS_ETCHEDHORZ || (style&SS_TYPEMASK) == SS_ETCHEDVERT || (style&SS_TYPEMASK) == SS_ETCHEDFRAME)
    {
      SWELL_BoxView *obj=[[SWELL_BoxView alloc] init];
      obj->m_etch_mode = style & SS_TYPEMASK;
      [obj setTag:idx];
      [obj setFrame:MakeCoords(x,y,w,h,false)];
      [m_make_owner addSubview:obj];
      [obj release];
      return (HWND)obj;
    }
    NSTextField *obj=[[SWELL_TextField alloc] init];
    [obj setEditable:NO];
    [obj setSelectable:NO];
    [obj setBordered:NO];
    [obj setBezeled:NO];
    [obj setDrawsBackground:NO];
    if (m_transform.size.width < minwidfontadjust)
      [obj setFont:[NSFont systemFontOfSize:TRANSFORMFONTSIZE]];

    if (cname && *cname)
    {
      NSString *labelstr=(NSString *)SWELL_CStringToCFString_FilterPrefix(cname);
      [obj setStringValue:labelstr];
      [labelstr release];
    }
    
    if ((style&SS_TYPEMASK) == SS_LEFTNOWORDWRAP) [[obj cell] setWraps:NO];
    else if ((style&SS_TYPEMASK) == SS_CENTER) [[obj cell] setAlignment:NSCenterTextAlignment];
    else if ((style&SS_TYPEMASK) == SS_RIGHT) [[obj cell] setAlignment:NSRightTextAlignment];

    [obj setTag:idx];
    [obj setFrame:MakeCoords(x,y,w,h,true)];
    if (style&SWELL_NOT_WS_VISIBLE) [obj setHidden:YES];
    [m_make_owner addSubview:obj];
    if ((style & SS_TYPEMASK) == SS_BLACKRECT)
    {
      [obj setHidden:YES];
    }
    [obj release];
    return (HWND)obj;
  }
  else if (!stricmp(classname,"Button"))
  {
    if (style & BS_GROUPBOX)
    {
      return SWELL_MakeGroupBox(cname, idx, x, y, w, h, style &~BS_GROUPBOX);
    }
    if (style & BS_DEFPUSHBUTTON)
    {
       return SWELL_MakeButton(1, cname, idx, x,y,w,h,style &~BS_DEFPUSHBUTTON);
    }
    if (style & BS_PUSHBUTTON)
    {
       return SWELL_MakeButton(0, cname, idx, x,y,w,h,style &~BS_PUSHBUTTON);
    }
    SWELL_Button *button=[[SWELL_Button alloc] init];
    [button setTag:idx];
    NSRect fr=MakeCoords(x,y,w,h,true);
    SEL actionSel = @selector(onSwellCommand:);
    if ((style & 0xf) == BS_AUTO3STATE)
    {
      [button setButtonType:NSSwitchButton];
      [button setAllowsMixedState:YES];
    }    
    else if ((style & 0xf) == BS_AUTOCHECKBOX)
    {
      [button setButtonType:NSSwitchButton];
      [button setAllowsMixedState:NO];
    }
    else if ((style & 0xf) == BS_AUTORADIOBUTTON)
    {
#ifdef MAC_OS_X_VERSION_10_8
      // Compiling with the OSX 10.8+ SDK and running on 10.8+ causes radio buttons with a common action selector to
      // be treated as a group. This works around that. if you need more than 8 groups (seriously?!), add the extra 
      // functions in swell-dlg.mm and in the switch below
      {
        NSView *v;
        NSArray *sv;
        if ((style & WS_GROUP) ||
              !(sv = [m_make_owner subviews]) || 
              ![sv count] ||
              !(v = [sv lastObject]) ||
              ![v isKindOfClass:[SWELL_Button class]] ||
              ([(SWELL_Button *)v swellGetRadioFlags]&2)) m_make_radiogroupcnt++;
      }
      switch (m_make_radiogroupcnt & 7)
      {
        case 0: actionSel = @selector(onSwellCommand0:); break;
        case 1: break; // default
        case 2: actionSel = @selector(onSwellCommand2:); break;
        case 3: actionSel = @selector(onSwellCommand3:); break;
        case 4: actionSel = @selector(onSwellCommand4:); break;
        case 5: actionSel = @selector(onSwellCommand5:); break;
        case 6: actionSel = @selector(onSwellCommand6:); break;
        case 7: actionSel = @selector(onSwellCommand7:); break;
      }
#endif
     
      [button setButtonType:NSRadioButton];
      [button swellSetRadioFlags:(style & WS_GROUP)?3:1];
    }
    else if ((style & 0xf) == BS_OWNERDRAW)
    {
      SWELL_ODButtonCell *cell = [[SWELL_ODButtonCell alloc] init];
      [button setCell:cell];
      [cell release];
      //NSButtonCell
    }
    else // normal button
    {
      if (style & BS_BITMAP)
      {
        SWELL_ImageButtonCell * cell = [[SWELL_ImageButtonCell alloc] init];
        [button setCell:cell];
        [cell release];
      }
      if ((style & BS_XPOSITION_MASK) == BS_LEFT) [button setAlignment:NSLeftTextAlignment];
//      fr.size.width+=8;
    }
    
    if (m_transform.size.width < minwidfontadjust)
      [button setFont:[NSFont systemFontOfSize:TRANSFORMFONTSIZE]];
    [button setFrame:fr];
    NSString *labelstr=(NSString *)SWELL_CStringToCFString_FilterPrefix(cname);
    [button setTitle:labelstr];
    [button setTarget:ACTIONTARGET];
    [button setAction:actionSel];
    if (style&BS_LEFTTEXT) [button setImagePosition:NSImageRight];
    if (style&SWELL_NOT_WS_VISIBLE) [button setHidden:YES];
    [m_make_owner addSubview:button];
    if (m_sizetofits && (style & 0xf) != BS_OWNERDRAW) [button sizeToFit];
    if (m_doautoright) UpdateAutoCoords([button frame]);
    [labelstr release];
    [button release];
    return (HWND)button;
  }
  else if (!stricmp(classname,"REAPERhfader")||!stricmp(classname,"msctls_trackbar32"))
  {
    NSSlider *obj=[[NSSlider alloc] init];
    [obj setTag:idx];
    [obj setMinValue:0.0];
    [obj setMaxValue:1000.0];
    [obj setFrame:MakeCoords(x,y,w,h,false)];
    if (!stricmp(classname, "msctls_trackbar32"))
    {
      [[obj cell] setControlSize:NSMiniControlSize];
    }
    [obj setTarget:ACTIONTARGET];
    [obj setAction:@selector(onSwellCommand:)];
    if (style&SWELL_NOT_WS_VISIBLE) [obj setHidden:YES];
    [m_make_owner addSubview:obj];
    [obj release];
    return (HWND)obj;
  }
  else if (!stricmp(classname,"COMBOBOX"))
  {
    return SWELL_MakeCombo(idx, x, y, w, h, style);
  }
  return 0;
}

HWND SWELL_MakeCombo(int idx, int x, int y, int w, int h, int flags)
{
  if ((flags & 0x3) == CBS_DROPDOWNLIST)
  {
    SWELL_PopUpButton *obj=[[SWELL_PopUpButton alloc] init];
    [obj setTag:idx];
    [obj setFont:[NSFont systemFontOfSize:10.0f]];
    NSRect rc=MakeCoords(x,y,w,18,true,true);
        
    [obj setSwellStyle:flags];
    [obj setFrame:rc];
    [obj setAutoenablesItems:NO];
    [obj setTarget:ACTIONTARGET];
    [obj setAction:@selector(onSwellCommand:)];

    if (g_swell_want_nice_style==1)
    {
      [obj setBezelStyle:NSShadowlessSquareBezelStyle ];
      [[obj cell] setArrowPosition:NSPopUpArrowAtBottom];
    }
    if (flags&SWELL_NOT_WS_VISIBLE) [obj setHidden:YES];
    [m_make_owner addSubview:obj];
    if (m_doautoright) UpdateAutoCoords([obj frame]);
    [obj release];
    return (HWND)obj;
  }
  else
  {
    SWELL_ComboBox *obj=[[SWELL_ComboBox alloc] init];
    [obj setFocusRingType:NSFocusRingTypeNone];
    [obj setFont:[NSFont systemFontOfSize:10.0f]];
    [obj setEditable:(flags & 0x3) == CBS_DROPDOWNLIST?NO:YES];
    [obj setSwellStyle:flags];
    [obj setTag:idx];
    [obj setFrame:MakeCoords(x,y-1,w,22,true,true)];
    [obj setTarget:ACTIONTARGET];
    [obj setAction:@selector(onSwellCommand:)];
    [obj setDelegate:ACTIONTARGET];
    if (flags&SWELL_NOT_WS_VISIBLE) [obj setHidden:YES];
    [m_make_owner addSubview:obj];
    if (m_doautoright) UpdateAutoCoords([obj frame]);
    [obj release];
    return (HWND)obj;
  }
}

@implementation SWELL_BoxView

STANDARD_CONTROL_NEEDSDISPLAY_IMPL(m_etch_mode ? "Static" : "Button")

-(NSInteger) tag
{
  return m_tag;
}
-(void) setTag:(NSInteger)tag
{
  m_tag=tag;
}

-(void) drawRect:(NSRect) r
{
  // m_etch_mode override?
  [super drawRect:r];
}
-(int)swellIsEtchBox
{
  return m_etch_mode;
}

@end

HWND SWELL_MakeGroupBox(const char *name, int idx, int x, int y, int w, int h, int style)
{
  SWELL_BoxView *obj=[[SWELL_BoxView alloc] init];
  obj->m_etch_mode = 0;
  
  // this just doesn't work, you can't color the border unless it's NSBoxCustom, 
  // and I can't get it to show the title text if it's NSBoxCustom
  //[obj setBoxType:(NSBoxType)4];   // NSBoxCustom, so we can color the border 
  //[obj setTitlePosition:(NSTitlePosition)2];  // NSAtTop, default but NSBoxCustom unsets it
  
//  [obj setTag:idx];
  if (1) // todo: only if on 10.4 maybe?
  {
    y-=1;
    h+=3;
  }
  NSString *labelstr=(NSString *)SWELL_CStringToCFString_FilterPrefix(name);
  [obj setTitle:labelstr];
  [obj setTag:idx];
  [labelstr release];
  if ((style & BS_XPOSITION_MASK) == BS_CENTER)
  {
    [[obj titleCell] setAlignment:NSCenterTextAlignment];
  }
  [obj setFrame:MakeCoords(x,y,w,h,false)];
  [m_make_owner addSubview:obj positioned:NSWindowBelow relativeTo:nil];
  [obj release];
  return (HWND)obj;
}


int TabCtrl_GetItemCount(HWND hwnd)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_TabView class]])) return 0;
  SWELL_TabView *tv=(SWELL_TabView*)hwnd;
  return (int)[tv numberOfTabViewItems];
}

BOOL TabCtrl_AdjustRect(HWND hwnd, BOOL fLarger, RECT *r)
{
  if (WDL_NOT_NORMALLY(!r || !hwnd || ![(id)hwnd isKindOfClass:[SWELL_TabView class]])) return FALSE;
  
  int sign=fLarger?-1:1;
  r->left+=sign*7; // todo: correct this?
  r->right-=sign*7;
  r->top+=sign*26;
  r->bottom-=sign*3;
  return TRUE;
}


BOOL TabCtrl_DeleteItem(HWND hwnd, int idx)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_TabView class]])) return 0;
  SWELL_TabView *tv=(SWELL_TabView*)hwnd;
  if (idx<0 || idx>= [tv numberOfTabViewItems]) return 0;
  [tv removeTabViewItem:[tv tabViewItemAtIndex:idx]];
  return TRUE;
}

int TabCtrl_InsertItem(HWND hwnd, int idx, TCITEM *item)
{
  if (WDL_NOT_NORMALLY(!item || !hwnd || ![(id)hwnd isKindOfClass:[SWELL_TabView class]])) return -1;
  if (!(item->mask & TCIF_TEXT) || !item->pszText) return -1;
  SWELL_TabView *tv=(SWELL_TabView*)hwnd;

  const int ni = (int)[tv numberOfTabViewItems];
  if (idx<0) idx=0;
  else if (idx>ni) idx=ni;
  
  NSTabViewItem *tabitem=[[NSTabViewItem alloc] init];
  NSString *str=(NSString *)SWELL_CStringToCFString(item->pszText);  
  [tabitem setLabel:str];
  [str release];
  id turd=[tv getNotificationWindow];
  [tv setNotificationWindow:nil];
  [tv insertTabViewItem:tabitem atIndex:idx];
  [tv setNotificationWindow:turd];
  [tabitem release];
  return idx;
}

int TabCtrl_SetCurSel(HWND hwnd, int idx)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_TabView class]])) return -1;
  SWELL_TabView *tv=(SWELL_TabView*)hwnd;
  int ret=TabCtrl_GetCurSel(hwnd);
  if (idx>=0 && idx < [tv numberOfTabViewItems])
  {
    [tv selectTabViewItemAtIndex:idx];
  }
  return ret;
}

int TabCtrl_GetCurSel(HWND hwnd)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_TabView class]])) return 0;
  SWELL_TabView *tv=(SWELL_TabView*)hwnd;
  NSTabViewItem *item=[tv selectedTabViewItem];
  if (!item) return 0;
  return (int)[tv indexOfTabViewItem:item];
}

void ListView_SetExtendedListViewStyleEx(HWND h, int mask, int style)
{
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]])) return;
  SWELL_ListView *tv=(SWELL_ListView*)h;
  
  if (mask&LVS_EX_GRIDLINES)
  {
    int s=0;
    if (style&LVS_EX_GRIDLINES) 
    {
      s=NSTableViewSolidVerticalGridLineMask|NSTableViewSolidHorizontalGridLineMask;
    }
    [tv setGridStyleMask:s];
  }
  
  if (mask&LVS_EX_HEADERDRAGDROP)
  {
    [tv setAllowsColumnReordering:!!(style&LVS_EX_HEADERDRAGDROP)];
  }
  
  
  // todo LVS_EX_FULLROWSELECT (enabled by default on OSX)
}

void SWELL_SetListViewFastClickMask(HWND hList, int mask)
{
  if (WDL_NOT_NORMALLY(!hList || ![(id)hList isKindOfClass:[SWELL_ListView class]])) return;
  SWELL_ListView *lv = (SWELL_ListView *)hList;
  lv->m_fastClickMask=mask;

}


void ListView_SetImageList(HWND h, HIMAGELIST imagelist, int which)
{
  if (WDL_NOT_NORMALLY(!h)) return;
  
  SWELL_ListView *v=(SWELL_ListView *)h;
  
  v->m_status_imagelist_type=which;
  v->m_status_imagelist=(WDL_PtrList<HGDIOBJ__> *)imagelist;
  if (v->m_cols && v->m_cols->GetSize()>0)
  {
    NSTableColumn *col=(NSTableColumn*)v->m_cols->Get(0);
    if (![col isKindOfClass:[SWELL_StatusCell class]])
    {
      SWELL_StatusCell *cell=[[SWELL_StatusCell alloc] initNewCell];
      [cell setWraps:NO];
      [col setDataCell:cell];
      [cell release];
    }
  }  
}

int ListView_GetColumnWidth(HWND h, int pos)
{
  if (!h || ![(id)h isKindOfClass:[SWELL_ListView class]]) return 0;
  SWELL_ListView *v=(SWELL_ListView *)h;
  if (!v->m_cols || pos < 0 || pos >= v->m_cols->GetSize()) return 0;
  
  NSTableColumn *col=v->m_cols->Get(pos);
  if (!col) return 0;
  
  if ([col respondsToSelector:@selector(isHidden)] && [(SWELL_TableColumnExtensions*)col isHidden]) return 0;
  return (int) floor(0.5+[col width]);
}

void ListView_InsertColumn(HWND h, int pos, const LVCOLUMN *lvc)
{
  if (WDL_NOT_NORMALLY(!h || !lvc || ![(id)h isKindOfClass:[SWELL_ListView class]])) return;

  SWELL_BEGIN_TRY

  SWELL_ListView *v=(SWELL_ListView *)h;
  NSTableColumn *col=[[NSTableColumn alloc] init];
  // note, not looking at lvc->mask at all

  [col setEditable:NO];
  // [col setResizingMask:2];  // user resizable, this seems to be the default
  
  if (lvc->fmt == LVCFMT_CENTER) [[col headerCell] setAlignment:NSCenterTextAlignment];
  else if (lvc->fmt == LVCFMT_RIGHT) [[col headerCell] setAlignment:NSRightTextAlignment];
  
  if (!v->m_lbMode && !(v->style & LVS_NOCOLUMNHEADER))
  {
    NSString *lbl=(NSString *)SWELL_CStringToCFString(lvc->pszText);  
    [[col headerCell] setStringValue:lbl];
    [lbl release];
  }
  
  if (!pos && v->m_status_imagelist) 
  {
    SWELL_StatusCell *cell=[[SWELL_StatusCell alloc] initNewCell];
    [cell setWraps:NO];
    [col setDataCell:cell];
    [cell release];
  }
  else
  {  
    SWELL_ListViewCell *cell = [[SWELL_ListViewCell alloc] initTextCell:@""];
    [col setDataCell:cell];
    [cell setWraps:NO];
   
    if (lvc->fmt == LVCFMT_CENTER) [cell setAlignment:NSCenterTextAlignment];
    else if (lvc->fmt == LVCFMT_RIGHT) [cell setAlignment:NSRightTextAlignment];
    [cell release];
  }

  [v addTableColumn:col];
  v->m_cols->Add(col);
  [col release];

  if (lvc->mask&LVCF_WIDTH)
  {
    ListView_SetColumnWidth(h,pos,lvc->cx);
  }
  SWELL_END_TRY(;)
}

void ListView_SetColumn(HWND h, int pos, const LVCOLUMN *lvc)
{
  if (WDL_NOT_NORMALLY(!h || !lvc || ![(id)h isKindOfClass:[SWELL_ListView class]])) return;
  SWELL_ListView *v=(SWELL_ListView *)h;
  if (!v->m_cols || pos < 0 || pos >= v->m_cols->GetSize()) return;
  
  NSTableColumn *col=v->m_cols->Get(pos);
  if (!col) return;
  
  if (lvc->mask&LVCF_FMT)
  {
    if (lvc->fmt == LVCFMT_LEFT) [[col headerCell] setAlignment:NSLeftTextAlignment];
    else if (lvc->fmt == LVCFMT_CENTER) [[col headerCell] setAlignment:NSCenterTextAlignment];
    else if (lvc->fmt == LVCFMT_RIGHT) [[col headerCell] setAlignment:NSRightTextAlignment];
  }
  if (lvc->mask&LVCF_WIDTH)
  {
    if (!lvc->cx)
    {
      if ([col respondsToSelector:@selector(setHidden:)])  [(SWELL_TableColumnExtensions*)col setHidden:YES];
    }
    else 
    {
      if ([col respondsToSelector:@selector(setHidden:)])  [(SWELL_TableColumnExtensions*)col setHidden:NO];
      [col setWidth:lvc->cx];
    }
  }
  if (lvc->mask&LVCF_TEXT)
  {
    if (!v->m_lbMode && !(v->style&LVS_NOCOLUMNHEADER))
    {
      NSString *lbl=(NSString *)SWELL_CStringToCFString(lvc->pszText);  
      [[col headerCell] setStringValue:lbl];
      [lbl release]; 
    }
  }
}

bool ListView_DeleteColumn(HWND h, int pos)
{
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]])) return false;
	SWELL_ListView *v=(SWELL_ListView *)h;
	if (!v->m_cols || pos < 0 || pos >= v->m_cols->GetSize()) return false;
	[v removeTableColumn:v->m_cols->Get(pos)];
	v->m_cols->Delete(pos);
	return true;
}

void ListView_GetItemText(HWND hwnd, int item, int subitem, char *text, int textmax)
{
  LVITEM it={LVIF_TEXT,item,subitem,0,0,text,textmax,};
  ListView_GetItem(hwnd,&it);
}

int ListView_InsertItem(HWND h, const LVITEM *item)
{
  if (WDL_NOT_NORMALLY(!h || !item || item->iSubItem || ![(id)h isKindOfClass:[SWELL_ListView class]])) return 0;
  
  SWELL_ListView *tv=(SWELL_ListView*)h;
  if (WDL_NOT_NORMALLY(!tv->m_lbMode && (tv->style & LVS_OWNERDATA))) return -1;
  if (WDL_NOT_NORMALLY(!tv->m_items)) return -1;
    
  int a=item->iItem;
  if (a<0)a=0;
  else if (a > tv->m_items->GetSize()) a=tv->m_items->GetSize();
  
  if (!tv->m_lbMode && (item->mask & LVIF_TEXT))
  {
    if (tv->style & LVS_SORTASCENDING)
    {
       a=ptrlist_bsearch_mod((char *)item->pszText,tv->m_items,_listviewrowSearchFunc,NULL);
    }
    else if (tv->style & LVS_SORTDESCENDING)
    {
       a=ptrlist_bsearch_mod((char *)item->pszText,tv->m_items,_listviewrowSearchFunc2,NULL);
    }
  }
  
  SWELL_ListView_Row *nr=new SWELL_ListView_Row;
  nr->m_vals.Add(strdup((item->mask & LVIF_TEXT) ? item->pszText : ""));
  if (item->mask & LVIF_PARAM) nr->m_param = item->lParam;
  tv->m_items->Insert(a,nr);
  

  
  if ((item->mask&LVIF_STATE) && (item->stateMask & (0xff<<16)))
  {
    nr->m_imageidx=(item->state>>16)&0xff;
  }
  
  [tv reloadData];
  
  if (a < tv->m_items->GetSize()-1)
  {
    NSIndexSet *sel=[tv selectedRowIndexes];
    if (sel && [sel count])
    {
      NSMutableIndexSet *ms = [[NSMutableIndexSet alloc] initWithIndexSet:sel];
      [ms shiftIndexesStartingAtIndex:a by:1];
      [tv selectRowIndexes:ms byExtendingSelection:NO];
      [ms release];
    }
  }
  
  if (item->mask & LVIF_STATE)
  {
    if (item->stateMask & LVIS_SELECTED)
    {
      if (item->state&LVIS_SELECTED)
      {
        bool isSingle = tv->m_lbMode ? !(tv->style & LBS_EXTENDEDSEL) : !!(tv->style&LVS_SINGLESEL);
        [tv selectRowIndexes:[NSIndexSet indexSetWithIndex:a] byExtendingSelection:isSingle?NO:YES];        
      }
    }
  }
  
  return a;
}

void ListView_SetItemText(HWND h, int ipos, int cpos, const char *txt)
{
  if (WDL_NOT_NORMALLY(!h || cpos < 0)) return;
  if (WDL_NOT_NORMALLY(![(id)h isKindOfClass:[SWELL_ListView class]])) return;
  
  SWELL_ListView *tv=(SWELL_ListView*)h;
  if (WDL_NOT_NORMALLY(!tv->m_lbMode && (tv->style & LVS_OWNERDATA))) return;
  if (WDL_NOT_NORMALLY(!tv->m_items || !tv->m_cols)) return;

  if (WDL_NOT_NORMALLY(cpos && cpos >= tv->m_cols->GetSize())) return; // always allow setting the first
  
  SWELL_ListView_Row *p=tv->m_items->Get(ipos);
  if (!p) return;
  int x;
  for (x = p->m_vals.GetSize(); x < cpos; x ++)
  {
    p->m_vals.Add(strdup(""));
  }
  if (cpos < p->m_vals.GetSize())
  {
    free(p->m_vals.Get(cpos));
    p->m_vals.Set(cpos,strdup(txt));
  }
  else p->m_vals.Add(strdup(txt));
    
  [tv setNeedsDisplay];
}

int ListView_GetNextItem(HWND h, int istart, int flags)
{
  if (WDL_NORMALLY(flags==LVNI_FOCUSED||flags==LVNI_SELECTED))
  {
    if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]])) return -1;
    
    SWELL_ListView *tv=(SWELL_ListView*)h;
    
    if (flags==LVNI_SELECTED)
    {
      //int orig_start=istart;
      if (istart++<0)istart=0;
      const int n = (int)[tv numberOfRows];
      while (istart < n)
      {
        if ([tv isRowSelected:istart]) return istart;
        istart++;
      }
      return -1;
    }
    
    return (int)[tv selectedRow];
  }
  return -1;
}

bool ListView_SetItem(HWND h, LVITEM *item)
{
  if (WDL_NOT_NORMALLY(!item || !h || ![(id)h isKindOfClass:[SWELL_ListView class]])) return false;
    
  SWELL_ListView *tv=(SWELL_ListView*)h;
  if (tv->m_lbMode || !(tv->style & LVS_OWNERDATA))
  {
    if (WDL_NOT_NORMALLY(!tv->m_items)) return false;
    SWELL_ListView_Row *row=tv->m_items->Get(item->iItem);
    if (WDL_NOT_NORMALLY(!row)) return false;  
  
    if (item->mask & LVIF_PARAM) 
    {
      row->m_param=item->lParam;
    }
    if ((item->mask & LVIF_TEXT) && item->pszText) 
    {
      ListView_SetItemText(h,item->iItem,item->iSubItem,item->pszText);
    }
    if ((item->mask&LVIF_IMAGE) && item->iImage >= 0)
    {
      row->m_imageidx=item->iImage+1;
      ListView_RedrawItems(h, item->iItem, item->iItem);
    }
  }
  if ((item->mask & LVIF_STATE) && item->stateMask)
  {
    ListView_SetItemState(h,item->iItem,item->state,item->stateMask); 
  }

  return true;
}

bool ListView_GetItem(HWND h, LVITEM *item)
{
  if (WDL_NOT_NORMALLY(!item)) return false;
  if ((item->mask&LVIF_TEXT)&&item->pszText && item->cchTextMax > 0) item->pszText[0]=0;
  item->state=0;
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]])) return false;
  
  SWELL_ListView *tv=(SWELL_ListView*)h;
  if (tv->m_lbMode || !(tv->style & LVS_OWNERDATA))
  {
    if (!tv->m_items) return false;
    
    SWELL_ListView_Row *row=tv->m_items->Get(item->iItem);
    if (!row) return false;  
  
    if (item->mask & LVIF_PARAM) item->lParam=row->m_param;
    if (item->mask & LVIF_TEXT) if (item->pszText && item->cchTextMax>0)
    {
      char *p=row->m_vals.Get(item->iSubItem);
      lstrcpyn_safe(item->pszText,p?p:"",item->cchTextMax);
    }
      if (item->mask & LVIF_STATE)
      {
        if (item->stateMask & (0xff<<16))
        {
          item->state|=row->m_imageidx<<16;
        }
      }
  }
  else
  {
    if (item->iItem <0 || item->iItem >= tv->ownermode_cnt) return false;
  }
  if (item->mask & LVIF_STATE)
  {
     if ((item->stateMask&LVIS_SELECTED) && [tv isRowSelected:item->iItem]) item->state|=LVIS_SELECTED;
     if ((item->stateMask&LVIS_FOCUSED) && [tv selectedRow] == item->iItem) item->state|=LVIS_FOCUSED;
  }

  return true;
}
int ListView_GetItemState(HWND h, int ipos, UINT mask)
{
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]])) return 0;
  SWELL_ListView *tv=(SWELL_ListView*)h;
  UINT flag=0;
  if (tv->m_lbMode || !(tv->style & LVS_OWNERDATA))
  {
    if (!tv->m_items) return 0;
    SWELL_ListView_Row *row=tv->m_items->Get(ipos);
    if (!row) return 0;  
    if (mask & (0xff<<16))
    {
      flag|=row->m_imageidx<<16;
    }
  }
  else
  {
    if (ipos<0 || ipos >= tv->ownermode_cnt) return 0;
  }
  
  if ((mask&LVIS_SELECTED) && [tv isRowSelected:ipos]) flag|=LVIS_SELECTED;
  if ((mask&LVIS_FOCUSED) && [tv selectedRow]==ipos) flag|=LVIS_FOCUSED;
  return flag;  
}

int swell_ignore_listview_changes;

bool ListView_SetItemState(HWND h, int ipos, UINT state, UINT statemask)
{
  int doref=0;
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]])) return false;
  SWELL_ListView *tv=(SWELL_ListView*)h;
  static int _is_doing_all;
  const bool isSingle = tv->m_lbMode ? !(tv->style & LBS_EXTENDEDSEL) : !!(tv->style&LVS_SINGLESEL);
  
  if (ipos == -1)
  {
    int x;
    int n=ListView_GetItemCount(h);
    NSIndexSet *oldSelection = NULL;
    if (statemask & LVIS_SELECTED)
    {
      oldSelection = [tv selectedRowIndexes];
      [oldSelection retain];
      if (state & LVIS_SELECTED)
      {
        if (isSingle)
        {
          statemask &= ~LVIS_SELECTED; // no-op and don't send LVN_ITEMCHANGED
        }
        else
        {
          swell_ignore_listview_changes++;
          [tv selectAll:nil];
          swell_ignore_listview_changes--;
        }
      }
      else
      {
        swell_ignore_listview_changes++;
        [tv deselectAll:nil];
        swell_ignore_listview_changes--;
      }
    }
    _is_doing_all++;
    for (x = 0; x < n; x ++)
    {
      if (statemask & ~LVIS_SELECTED)
        ListView_SetItemState(h,x,state,statemask & ~LVIS_SELECTED);

      if (statemask & LVIS_SELECTED)
      {
        if ([oldSelection containsIndex:x] == !(state & LVIS_SELECTED))
        {
          static int __rent;
          if (!__rent)
          {
            __rent=1;
            NMLISTVIEW nm={{(HWND)h,(UINT_PTR)[tv tag],LVN_ITEMCHANGED},x,0,state,};
            SendMessage(GetParent(h),WM_NOTIFY,nm.hdr.idFrom,(LPARAM)&nm);
            __rent=0;
          }
        }
      }
    }
    [oldSelection release];
    _is_doing_all--;
    ListView_RedrawItems(h,0,n-1);
    return true;
  }

  if (tv->m_lbMode || !(tv->style & LVS_OWNERDATA))
  {
    if (!tv->m_items) return false;
    SWELL_ListView_Row *row=tv->m_items->Get(ipos);
    if (!row) return false;  
    if (statemask & (0xff<<16))
    {
      if (row->m_imageidx!=((state>>16)&0xff))
      {
        row->m_imageidx=(state>>16)&0xff;
        doref=1;
      }
    }
  }
  else
  {
    if (ipos<0 || ipos >= tv->ownermode_cnt) return 0;
  }
  bool didsel=false;
  if (statemask & LVIS_SELECTED)
  {
    if (state & LVIS_SELECTED)
    {
      if (![tv isRowSelected:ipos])
      {
        didsel = true;
        swell_ignore_listview_changes++;
        [tv selectRowIndexes:[NSIndexSet indexSetWithIndex:ipos] byExtendingSelection:isSingle?NO:YES];
        swell_ignore_listview_changes--;
      }
    }
    else
    {
      if ([tv isRowSelected:ipos])
      {
        didsel = true;
        swell_ignore_listview_changes++;
        [tv deselectRow:ipos];
        swell_ignore_listview_changes--;
      }
    }
  }
  if (statemask & LVIS_FOCUSED)
  {
    if (state&LVIS_FOCUSED)
    {
    }
    else
    {
      
    }
  }
  
  if (!_is_doing_all)
  {
    if (didsel)
    {
      static int __rent;
      if (!__rent)
      {
        __rent=1;
        NMLISTVIEW nm={{(HWND)h,(UINT_PTR)[tv tag],LVN_ITEMCHANGED},ipos,0,state,};
        SendMessage(GetParent(h),WM_NOTIFY,nm.hdr.idFrom,(LPARAM)&nm);
        __rent=0;
      }
    }
    if (doref)
      ListView_RedrawItems(h,ipos,ipos);
  }
  return true;
}

void ListView_RedrawItems(HWND h, int startitem, int enditem)
{
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]])) return;
  SWELL_ListView *tv=(SWELL_ListView*)h;
  if (!tv->m_items) return;
  [tv reloadData];
}

void ListView_DeleteItem(HWND h, int ipos)
{
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]])) return;
  
  SWELL_ListView *tv=(SWELL_ListView*)h;
  if (!tv->m_items) return;
  
  if (ipos >=0 && ipos < tv->m_items->GetSize())
  {
    if (ipos != tv->m_items->GetSize()-1)
    {
      NSIndexSet *sel=[tv selectedRowIndexes];
      if (sel && [sel count])
      {
        NSMutableIndexSet *ms = [[NSMutableIndexSet alloc] initWithIndexSet:sel];
        [ms shiftIndexesStartingAtIndex:ipos+1 by:-1];
        [tv selectRowIndexes:ms byExtendingSelection:NO];
        [ms release];
      }
    }
    tv->m_items->Delete(ipos,true);
    
    [tv reloadData];
    
  }
}

void ListView_DeleteAllItems(HWND h)
{
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]])) return;
  
  SWELL_ListView *tv=(SWELL_ListView*)h;
  tv->ownermode_cnt=0;
  if (tv->m_items) tv->m_items->Empty(true);
  
  [tv reloadData];
}

int ListView_GetSelectedCount(HWND h)
{
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]])) return 0;
  
  SWELL_ListView *tv=(SWELL_ListView*)h;
  return (int)[tv numberOfSelectedRows];
}

int ListView_GetItemCount(HWND h)
{
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]])) return 0;
  
  SWELL_ListView *tv=(SWELL_ListView*)h;
  if (tv->m_lbMode || !(tv->style & LVS_OWNERDATA))
  {
    if (!tv->m_items) return 0;
  
    return tv->m_items->GetSize();
  }
  return tv->ownermode_cnt;
}

int ListView_GetSelectionMark(HWND h)
{
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]])) return 0;
  
  SWELL_ListView *tv=(SWELL_ListView*)h;
  return (int)[tv selectedRow];
}

int SWELL_GetListViewHeaderHeight(HWND h)
{
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]])) return 0;
  
  SWELL_ListView* tv=(SWELL_ListView*)h;
  NSTableHeaderView* hv=[tv headerView];
  NSRect r=[hv bounds];
  return (int)(r.size.height+0.5);
}

void ListView_SetColumnWidth(HWND h, int pos, int wid)
{
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]])) return;
  SWELL_ListView *v=(SWELL_ListView *)h;
  if (!v->m_cols || pos < 0 || pos >= v->m_cols->GetSize()) return;
  
  NSTableColumn *col=v->m_cols->Get(pos);
  if (!col) return;
  
  if (!wid)
  {
    if ([col respondsToSelector:@selector(setHidden:)])  [(SWELL_TableColumnExtensions*)col setHidden:YES];
  }
  else 
  {
    if ([col respondsToSelector:@selector(setHidden:)])  [(SWELL_TableColumnExtensions*)col setHidden:NO];
    [col setWidth:wid];
  }
}

BOOL ListView_GetColumnOrderArray(HWND h, int cnt, int* arr)
{
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]])) return FALSE;
  SWELL_ListView* lv=(SWELL_ListView*)h;
  if (!lv->m_cols || lv->m_cols->GetSize() != cnt) return FALSE;
  
  int i;
  for (i=0; i < cnt; ++i)
  {
    arr[i]=[lv getColumnPos:i];
  }

  return TRUE;
}

BOOL ListView_SetColumnOrderArray(HWND h, int cnt, int* arr)
{
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]])) return FALSE;
  SWELL_ListView* lv=(SWELL_ListView*)h;
  if (!lv->m_cols || lv->m_cols->GetSize() != cnt) return FALSE;
  
  int i;
  for (i=0; i < cnt; ++i)
  {
    int pos=[lv getColumnPos:i];
    int dest=arr[i];
    if (dest>=0 && dest<cnt) [lv moveColumn:pos toColumn:dest];
  }

  return TRUE;
}

HWND ListView_GetHeader(HWND h)
{
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]])) return 0;
  return h;
}

int Header_GetItemCount(HWND h)
{
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]])) return 0;
  SWELL_ListView* lv=(SWELL_ListView*)h;
  if (lv->m_cols) return lv->m_cols->GetSize();
  return 0;
}

BOOL Header_GetItem(HWND h, int col, HDITEM* hi)
{
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]] || !hi)) return FALSE;
  SWELL_ListView* lv=(SWELL_ListView*)h;
  if (!lv->m_cols || col < 0 || col >= lv->m_cols->GetSize()) return FALSE;
  NSTableColumn* hcol=lv->m_cols->Get(col);
  if (WDL_NOT_NORMALLY(!hcol)) return FALSE;
  
  if (hi->mask&HDI_FORMAT)
  {
    hi->fmt=0;
    NSImage* img=[lv indicatorImageInTableColumn:hcol];
    if (img)
    {
      NSString* imgname=[img name];
      if (imgname)
      {
        if ([imgname isEqualToString:@"NSAscendingSortIndicator"]) hi->fmt |= HDF_SORTUP;
        else if ([imgname isEqualToString:@"NSDescendingSortIndicator"]) hi->fmt |= HDF_SORTDOWN;
      }
    }
  }
  // etc todo
  
  return TRUE;
}

BOOL Header_SetItem(HWND h, int col, HDITEM* hi)
{
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]] || !hi)) return FALSE;
  SWELL_ListView* lv=(SWELL_ListView*)h;
  if (!lv->m_cols || col < 0 || col >= lv->m_cols->GetSize()) return FALSE;
  NSTableColumn* hcol=lv->m_cols->Get(col);
  if (!hcol) return FALSE;
  
  if (hi->mask&HDI_FORMAT)
  {
    NSImage* img=0;
    if (hi->fmt&HDF_SORTUP) img=[NSImage imageNamed:@"NSAscendingSortIndicator"];
    else if (hi->fmt&HDF_SORTDOWN) img=[NSImage imageNamed:@"NSDescendingSortIndicator"];
    [lv setIndicatorImage:img inTableColumn:hcol];
  }
  // etc todo
  
  return TRUE;
}

int ListView_HitTest(HWND h, LVHITTESTINFO *pinf)
{
  if (WDL_NOT_NORMALLY(!h || !pinf)) return -1;
  if (WDL_NOT_NORMALLY(![(id)h isKindOfClass:[SWELL_ListView class]])) return -1;
  
  SWELL_ListView *tv=(SWELL_ListView*)h;
  // return index
  pinf->flags=0;
  pinf->iItem=-1;
  
  // rowAtPoint will return a row even if it is scrolled out of the clip view
  NSScrollView* sv=(NSScrollView *)NavigateUpScrollClipViews(tv);
  if (![sv isKindOfClass:[NSScrollView class]] && ![sv isKindOfClass:[NSClipView class]]) sv=NULL;
  
  NSRect r=[sv documentVisibleRect];
  int x=pinf->pt.x-r.origin.x;
  int y=pinf->pt.y-r.origin.y;

  if (x < 0) pinf->flags |= LVHT_TOLEFT;
  if (x >= r.size.width) pinf->flags |= LVHT_TORIGHT;
  if (y < 0) pinf->flags |= LVHT_ABOVE;
  if (y >= r.size.height) pinf->flags |= LVHT_BELOW;
  
  if (!pinf->flags)
  {
    NSPoint pt = NSMakePoint( pinf->pt.x, pinf->pt.y );
    pinf->iItem=(int)[(NSTableView *)h rowAtPoint:pt];
    if (pinf->iItem >= 0)
    {
      if (tv->m_status_imagelist && pt.x <= [tv rowHeight])
      {
        pinf->flags=LVHT_ONITEMSTATEICON;
      }
      else 
      {
        pinf->flags=LVHT_ONITEMLABEL;
      }
    }
    else 
    {
      pinf->flags=LVHT_NOWHERE;
    }
  }
  
  return pinf->iItem;
}

int ListView_SubItemHitTest(HWND h, LVHITTESTINFO *pinf)
{
  int row = ListView_HitTest(h, pinf);

  NSPoint pt=NSMakePoint(pinf->pt.x,pinf->pt.y);
  if (row < 0 && pt.y < 0)
  { // Fake the point in the client area of the listview to get the column # (like win32)
    pt.y = 0;
  }
  pinf->iSubItem=(int)[(NSTableView *)h columnAtPoint:pt];
  return row;
}

void ListView_SetItemCount(HWND h, int cnt)
{
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]])) return;
  
  SWELL_ListView *tv=(SWELL_ListView*)h;
  if (!tv->m_lbMode && (tv->style & LVS_OWNERDATA))
  {
    tv->ownermode_cnt=cnt;
    [tv noteNumberOfRowsChanged];
  }
}

void ListView_EnsureVisible(HWND h, int i, BOOL pok)
{
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]])) return;
  
  SWELL_ListView *tv=(SWELL_ListView*)h;
  
  if (i<0)i=0;
  if (!tv->m_lbMode && (tv->style & LVS_OWNERDATA))
  {
    if (i >=tv->ownermode_cnt-1) i=tv->ownermode_cnt-1;
  }
  else
  {
    if (tv->m_items && i >= tv->m_items->GetSize()) i=tv->m_items->GetSize()-1;
  }
  if (i>=0)
  {
    [tv scrollRowToVisible:i];
  }  
}

static bool ListViewGetRectImpl(HWND h, int item, int subitem, RECT* r) // subitem<0 for full item rect
{
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_ListView class]])) return false;
  if (item < 0 || item > ListView_GetItemCount(h)) return false;
  SWELL_ListView *tv=(SWELL_ListView*)h;
  
  if (subitem >= 0 && (!tv->m_cols || subitem >= tv->m_cols->GetSize())) return false;
  subitem=[tv getColumnPos:subitem];
  
  NSRect ar;
  if (subitem < 0) ar = [tv rectOfRow:item];
  else ar=[tv frameOfCellAtColumn:subitem row:item];
  NSSize sp=[tv intercellSpacing];
  
  ar.size.width += sp.width;
  ar.size.height += sp.height;
  NSRECT_TO_RECT(r,ar);
  
  return true;
}

bool ListView_GetSubItemRect(HWND h, int item, int subitem, int code, RECT *r)
{
  return ListViewGetRectImpl(h, item, subitem, r);
}

bool ListView_GetItemRect(HWND h, int item, RECT *r, int code)
{
  return ListViewGetRectImpl(h, item, -1, r);
}

int ListView_GetTopIndex(HWND h)
{
  NSTableView* tv = (NSTableView*)h;
  if (WDL_NOT_NORMALLY(!tv)) return -1;
  NSScrollView* sv = [tv enclosingScrollView];
  if (WDL_NOT_NORMALLY(!sv)) return -1;  
  
  NSPoint pt = { 0, 0 };
  NSView *hdr = [tv headerView];
  if (hdr && ![hdr isHidden])
  {
    NSRect fr=[hdr frame];
    if (fr.size.height > 0.0) pt.y = fr.origin.y + fr.size.height;
  }
  pt.y += [sv documentVisibleRect].origin.y;
  return (int)[tv rowAtPoint:pt];      
}

int ListView_GetCountPerPage(HWND h)
{
  NSTableView* tv = (NSTableView*)h;
  if (WDL_NOT_NORMALLY(!tv)) return 0;
  NSScrollView* sv = [tv enclosingScrollView];
  if (WDL_NOT_NORMALLY(!sv)) return 0;  
  
  NSRect tvr = [sv documentVisibleRect];
  int rowh = [tv rowHeight];
  return tvr.size.height/rowh;
}

bool ListView_Scroll(HWND h, int xscroll, int yscroll)
{
  NSTableView* tv = (NSTableView*)h;
  NSScrollView* sv = [tv enclosingScrollView];
  if (WDL_NOT_NORMALLY(!sv)) return false;
  
  NSRect tvr = [sv documentVisibleRect];
  NSPoint pt = { tvr.origin.x, tvr.origin.y };
  if (xscroll > 0) pt.x += tvr.size.width-1;
  if (yscroll > 0) pt.y += tvr.size.height-1;
  
  const NSInteger nr = [tv numberOfRows];
  NSInteger rowidx = [tv rowAtPoint:pt];
  if (rowidx < 0) rowidx=0;
  else if (rowidx >= nr) rowidx=nr-1;
  
  const NSInteger nc = [tv numberOfColumns];
  NSInteger colidx = [tv columnAtPoint:pt];
  if (colidx < 0) colidx=0;
  else if (colidx >= nc) colidx = nc-1;

  // colidx is our column index, not the display order, convert
  if ([tv isKindOfClass:[SWELL_ListView class]]) colidx = [(SWELL_ListView*)tv getColumnPos:(int)colidx];

  NSRect ir = [tv frameOfCellAtColumn:colidx row:rowidx];

  if (yscroll)
  {
    if (ir.size.height) rowidx += yscroll / ir.size.height;

    if (rowidx < 0) rowidx=0;
    else if (rowidx >= nr) rowidx = nr-1;
    [tv scrollRowToVisible:rowidx];
  }
  
  if (xscroll)
  {
    if (ir.size.width) colidx += xscroll / ir.size.width;
   
    if (colidx < 0) colidx=0;
    else if (colidx >= nc) colidx = nc-1;

    // scrollColumnToVisible takes display order, which we have here
    [tv scrollColumnToVisible:colidx];
  }
  
  
  return true;
}

bool ListView_GetScroll(HWND h, POINT* p)
{
  NSTableView* tv = (NSTableView*)h;
  NSScrollView* sv = [tv enclosingScrollView];
  if (WDL_NORMALLY(sv))
  {
    NSRect cr = [sv documentVisibleRect];
    p->x = cr.origin.x;
    p->y = cr.origin.y;
    return true;
  }
  p->x=p->y=0;
  return false;
}

void ListView_SortItems(HWND hwnd, PFNLVCOMPARE compf, LPARAM parm)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_ListView class]])) return;
  SWELL_ListView *tv=(SWELL_ListView*)hwnd;
  if (tv->m_lbMode || (tv->style & LVS_OWNERDATA) || !tv->m_items) return;
    
  WDL_HeapBuf tmp;
  tmp.Resize(tv->m_items->GetSize()*sizeof(void *));
  int x;
  int sc=0;
  for(x=0;x<tv->m_items->GetSize();x++)
  {
    SWELL_ListView_Row *r = tv->m_items->Get(x);
    if (r) 
    {
      r->m_tmp = !![tv isRowSelected:x];
      sc++;
    }
  }
  __listview_mergesort_internal(tv->m_items->GetList(),tv->m_items->GetSize(),sizeof(void *),compf,parm,(char*)tmp.Get());
  if (sc)
  {
    NSMutableIndexSet *indexSet = [[NSMutableIndexSet  alloc] init];
    
    for(x=0;x<tv->m_items->GetSize();x++)
    {
      SWELL_ListView_Row *r = tv->m_items->Get(x);
      if (r && (r->m_tmp&1)) [indexSet addIndex:x];
    }
    [tv selectRowIndexes:indexSet byExtendingSelection:NO];
    [indexSet release];
  }
  
  [tv reloadData];
}


HWND WindowFromPoint(POINT p)
{
  NSArray *windows=[NSApp orderedWindows];
  const NSInteger cnt=windows ? [windows count] : 0;

  NSWindow *kw = [NSApp keyWindow];
  if (kw && windows && [windows containsObject:kw]) kw=NULL;

  NSWindow *bestwnd=0;
  for (NSInteger x = kw ? -1 : 0; x < cnt; x ++)
  {
    NSWindow *wnd = kw;
    if (x>=0) wnd=[windows objectAtIndex:x];
    if (wnd && [wnd isVisible])
    {
      NSRect fr=[wnd frame];
      if (p.x >= fr.origin.x && p.x < fr.origin.x + fr.size.width &&
          p.y >= fr.origin.y && p.y < fr.origin.y + fr.size.height)
      {
        bestwnd=wnd;
        break;
      }    
    }
  }
  
  if (!bestwnd) return 0;
  NSPoint pt=NSMakePoint(p.x,p.y);
  NSPoint lpt=[bestwnd convertScreenToBase:pt];
  NSView *v=[[bestwnd contentView] hitTest:lpt];
  if (v) return (HWND)v;
  return (HWND)[bestwnd contentView]; 
}

void UpdateWindow(HWND hwnd)
{
  if (WDL_NORMALLY(hwnd && [(id)hwnd isKindOfClass:[NSView class]]))
  {
#ifndef SWELL_NO_METAL
    if ([(id)hwnd isKindOfClass:[SWELL_hwndChild class]] && 
        ((SWELL_hwndChild *)hwnd)->m_use_metal > 0)
    {
      // do nothing for metal windows, let the timer catch it
    }
    else 
#endif
    {
      if ([(NSView *)hwnd needsDisplay])
      {
        NSWindow *wnd = [(NSView *)hwnd window];
        [wnd displayIfNeeded];
      }
    }
  }
}

void SWELL_FlushWindow(HWND h)
{
  if (WDL_NORMALLY(h))
  {
    NSWindow *w=NULL;
    if ([(id)h isKindOfClass:[NSView class]]) 
    {
#ifndef SWELL_NO_METAL
      if ([(id)h isKindOfClass:[SWELL_hwndChild class]] && ((SWELL_hwndChild *)h)->m_use_metal > 0)
        return;
#endif

      if ([(NSView *)h needsDisplay]) return;
      
      w = [(NSView *)h window];
    }
    else if ([(id)h isKindOfClass:[NSWindow class]]) w = (NSWindow *)h;
    
    if (w && ![w viewsNeedDisplay])
    {
      [w flushWindow];
    }
  }
}

static void InvalidateSuperViews(NSView *view)
{
  if (!view) return;
  view = [view superview];
  while (view)
  {
    if ([view isKindOfClass:[SWELL_hwndChild class]]) 
    {
      if (((SWELL_hwndChild *)view)->m_isdirty&2) break;
      ((SWELL_hwndChild *)view)->m_isdirty|=2;
    }
    view = [view superview];
  }
}
           
BOOL InvalidateRect(HWND hwnd, const RECT *r, int eraseBk)
{ 
  if (WDL_NOT_NORMALLY(!hwnd)) return FALSE;
  id view=(id)hwnd;
  if ([view isKindOfClass:[NSWindow class]]) view=[view contentView];
  if (WDL_NORMALLY([view isKindOfClass:[NSView class]]))
  {

    NSView *sv = view;
    
    bool skip_parent_invalidate=false;
    if ([view isKindOfClass:[SWELL_hwndChild class]])
    {
#ifndef SWELL_NO_METAL
      SWELL_hwndChild *hc = (SWELL_hwndChild*)view;
      if (hc->m_use_metal > 0)
      {
        if (![hc isHiddenOrHasHiddenAncestor]) 
        {
          swell_addMetalDirty(hc,r);
        }
        return TRUE;
      }
#endif
      if (!(((SWELL_hwndChild *)view)->m_isdirty&1))
      {
        ((SWELL_hwndChild *)view)->m_isdirty|=1;
      }
      else skip_parent_invalidate=true; // if already dirty, then assume parents are already dirty too
    }
    if (!skip_parent_invalidate)
    {
      InvalidateSuperViews(view);
    }
    if (r)
    {
      RECT tr=*r;
      if (tr.top>tr.bottom)
      {
        int a = tr.top; tr.top=tr.bottom; tr.bottom=a;
      }
      [sv setNeedsDisplayInRect:NSMakeRect(tr.left,tr.top,tr.right-tr.left,tr.bottom-tr.top)]; 
    }
    else [sv setNeedsDisplay:YES];
    
  }
  return TRUE;
}

static HWND m_fakeCapture;
static BOOL m_capChangeNotify;
HWND GetCapture()
{

  return m_fakeCapture;
}

HWND SetCapture(HWND hwnd)
{
  HWND oc=m_fakeCapture;
  int ocn=m_capChangeNotify;
  m_fakeCapture=hwnd;
  m_capChangeNotify = hwnd && [(id)hwnd respondsToSelector:@selector(swellCapChangeNotify)] && [(SWELL_hwndChild*)hwnd swellCapChangeNotify];

  if (hwnd && WDL_NORMALLY([(id)hwnd isKindOfClass:[NSView class]]))
    [[(NSView *)hwnd window] disableCursorRects];

  if (ocn && oc && oc != hwnd) SendMessage(oc,WM_CAPTURECHANGED,0,(LPARAM)hwnd);
  return oc;
}


void ReleaseCapture()
{
  HWND h=m_fakeCapture;
  m_fakeCapture=NULL;
  if (m_capChangeNotify && h)
  {
    SendMessage(h,WM_CAPTURECHANGED,0,0);
  }
}


HDC BeginPaint(HWND hwnd, PAINTSTRUCT *ps)
{
  if (WDL_NOT_NORMALLY(!ps)) return 0;
  memset(ps,0,sizeof(PAINTSTRUCT));
  if (WDL_NOT_NORMALLY(!hwnd)) return 0;
  id turd = (id)hwnd;
  if (![turd respondsToSelector:@selector(getSwellPaintInfo:)]) return 0;

  [(SWELL_hwndChild*)turd getSwellPaintInfo:(PAINTSTRUCT *)ps];
  return ps->hdc;
}

BOOL EndPaint(HWND hwnd, PAINTSTRUCT *ps)
{
  WDL_ASSERT(hwnd != NULL && ps != NULL);
  return TRUE;
}

LRESULT DefWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (msg==WM_RBUTTONUP||msg==WM_NCRBUTTONUP)
  {  
    POINT p={GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)};
    HWND hwndDest=hwnd;
    if (msg==WM_RBUTTONUP)
    {
      ClientToScreen(hwnd,&p);
      HWND h=WindowFromPoint(p);
      if (h && IsChild(hwnd,h)) hwndDest=h;
    }
    SendMessage(hwnd,WM_CONTEXTMENU,(WPARAM)hwndDest,(p.x&0xffff)|(p.y<<16));
    return 1;
  }
  else if (msg==WM_CONTEXTMENU || msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL || msg == WM_GESTURE)
  {
    if ([(id)hwnd isKindOfClass:[NSView class]])
    {
      NSView *h=(NSView *)hwnd;
      while (h && [[h window] contentView] != h)
      {
        h=[h superview];
        if (h && [h respondsToSelector:@selector(onSwellMessage:p1:p2:)]) 
        {
           return SendMessage((HWND)h,msg,wParam,lParam);    
        }
      }
    }
  }
  else if (msg==WM_NCHITTEST) 
  {
    int rv=HTCLIENT;
    SWELL_BEGIN_TRY
    RECT r;
    GetWindowRect(hwnd,&r);
    POINT pt={GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)};

    if (r.top > r.bottom) 
    { 
      pt.y = r.bottom + (r.top - pt.y); // translate coordinate into flipped-window

      int a=r.top; r.top=r.bottom; r.bottom=a; 
    }
    NCCALCSIZE_PARAMS p={{r,}};
    SendMessage(hwnd,WM_NCCALCSIZE,FALSE,(LPARAM)&p);
    if (!PtInRect(&p.rgrc[0],pt)) rv=HTNOWHERE;
    SWELL_END_TRY(;)
    return rv;
  }
  else if (msg==WM_KEYDOWN || msg==WM_KEYUP) return 69;
  else if (msg == WM_DISPLAYCHANGE)
  {
    if ([(id)hwnd isKindOfClass:[NSView class]])
    {
      NSArray *ch = [(NSView *)hwnd subviews];
      if (ch)
      {
        int x;
        for(x=0;x<[ch count]; x ++)
        {
          NSView *v = [ch objectAtIndex:x];
          sendSwellMessage(v,WM_DISPLAYCHANGE,wParam,lParam);
        }
        if (x)
        {
          void SWELL_DoDialogColorUpdates(HWND hwnd, DLGPROC d, bool isUpdate);
          DLGPROC d = (DLGPROC)GetWindowLong(hwnd,DWL_DLGPROC);
          if (d) SWELL_DoDialogColorUpdates(hwnd,d,true);
        }
      }
      if ([(id)hwnd respondsToSelector:@selector(swellWantsMetal)] && [(SWELL_hwndChild *)hwnd swellWantsMetal])
        InvalidateRect((HWND)hwnd,NULL,FALSE);
    }
  }
  else if (msg == WM_CTLCOLORSTATIC && wParam)
  {
    if (SWELL_osx_is_dark_mode(0))
    {
      static HBRUSH br;
      if (!br)
      {
        br = CreateSolidBrush(RGB(0,0,0)); // todo hm
        br->color = (CGColorRef) [[NSColor windowBackgroundColor] CGColor];
        CFRetain(br->color);
      }
      SetTextColor((HDC)wParam,RGB(255,255,255));
      return (LRESULT)br;
    }
  }
  return 0;
}

void SWELL_BroadcastMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  int x;
  NSArray *ch=[NSApp orderedWindows];
  for(x=0;x<[ch count]; x ++)
  {
    NSView *v = [[ch objectAtIndex:x] contentView];
    if (v && [v respondsToSelector:@selector(onSwellMessage:p1:p2:)])
    {
      [(SWELL_hwndChild *)v onSwellMessage:uMsg p1:wParam p2:lParam];
      
      if (uMsg == WM_DISPLAYCHANGE)
        InvalidateRect((HWND)v,NULL,FALSE);
    }
  }  
}














///////////////// clipboard compatability (NOT THREAD SAFE CURRENTLY)


BOOL DragQueryPoint(HDROP hDrop,LPPOINT pt)
{
  if (!hDrop) return 0;
  DROPFILES *df=(DROPFILES*)GlobalLock(hDrop);
  BOOL rv=!df->fNC;
  *pt=df->pt;
  GlobalUnlock(hDrop);
  return rv;
}

void DragFinish(HDROP hDrop)
{
//do nothing for now (caller will free hdrops)
}

UINT DragQueryFile(HDROP hDrop, UINT wf, char *buf, UINT bufsz)
{
  if (!hDrop) return 0;
  DROPFILES *df=(DROPFILES*)GlobalLock(hDrop);

  size_t rv=0;
  char *p=(char*)df + df->pFiles;
  if (wf == 0xFFFFFFFF)
  {
    while (*p)
    {
      rv++;
      p+=strlen(p)+1;
    }
  }
  else
  {
    while (*p)
    {
      if (!wf--)
      {
        if (buf)
        {
          lstrcpyn_safe(buf,p,bufsz);
          rv=strlen(buf);
        }
        else rv=strlen(p);
          
        break;
      }
      p+=strlen(p)+1;
    }
  }
  GlobalUnlock(hDrop);
  return (UINT)rv;
}











static WDL_PtrList<void> m_clip_recs;
static WDL_PtrList<NSString> m_clip_fmts;
static WDL_PtrList<char> m_clip_curfmts;
struct swell_pendingClipboardStates
{
  UINT type;
  HANDLE h;
  swell_pendingClipboardStates(UINT _type, HANDLE _h)
  {
    type = _type;
    h = _h;
  }
  ~swell_pendingClipboardStates()
  {
    GlobalFree(h);
  }
};

static WDL_PtrList<swell_pendingClipboardStates> m_clipsPending;

bool OpenClipboard(HWND hwndDlg)
{
  m_clipsPending.Empty(true);
  RegisterClipboardFormat(NULL);

  NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
  m_clip_curfmts.Empty();
  if (SWELL_GetOSXVersion()>=0x1060)
  {
    NSArray *list = [pasteboard
      readObjectsForClasses:[NSArray arrayWithObject:[NSURL class]]
      options:[NSMutableDictionary dictionaryWithCapacity:1]];
    if ([list count]) m_clip_curfmts.Add((char*)(INT_PTR)CF_HDROP);
  }
  NSArray *ar=[pasteboard types];

  if (ar && [ar count])
  {
    int x;
    
    for (x = 0; x < [ar count]; x ++)
    {
      NSString *s=[ar objectAtIndex:x];
      if (!s) continue;
      int y;
      for (y = 0; y < m_clip_fmts.GetSize(); y ++)
      {
        NSString *cs = m_clip_fmts.Get(y);
        if (cs && [s compare:cs]==NSOrderedSame)
        {
          char *tok = (char*)(INT_PTR)(y+1);
          if (m_clip_curfmts.Find(tok)<0) m_clip_curfmts.Add(tok);
          break;
        }
      }
      
    }
  }
  return true;
}

void CloseClipboard() // frees any remaining items in clipboard
{
  m_clip_recs.Empty(GlobalFree);
  
  if (m_clipsPending.GetSize())
  {
    int x;
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    
    NSMutableArray *ar = [[NSMutableArray alloc] initWithCapacity:m_clipsPending.GetSize()];
    
    int hdrop_cnt=0;
    for (x=0;x<m_clipsPending.GetSize();x++)
    {
      swell_pendingClipboardStates *cs=m_clipsPending.Get(x);
      if (cs->type == CF_HDROP)
      {
        hdrop_cnt++;
      }
      else
      {
      NSString *fmt=m_clip_fmts.Get(cs->type-1);
      if (fmt) [ar addObject:fmt];
    }
    }

    if (hdrop_cnt || [ar count])
    {
    if ([ar count])
      [pasteboard declareTypes:ar owner:nil];
      else if (SWELL_GetOSXVersion() >= 0x1060)
        [pasteboard clearContents];
      for (x=0;x<m_clipsPending.GetSize();x++)
      {
        swell_pendingClipboardStates *cs=m_clipsPending.Get(x);
        
        void *buf=GlobalLock(cs->h);
        if (buf)
        {
          int bufsz=GlobalSize(cs->h);
          if (cs->type == CF_TEXT)
          {
            char *t = (char *)malloc(bufsz+1);
            if (t)
            {
              memcpy(t,buf,bufsz);
              t[bufsz]=0;
              NSString *s = (NSString*)SWELL_CStringToCFString(t);
              [pasteboard setString:s forType:NSStringPboardType];
              [s release];
              free(t);
            }
          }
          else if (cs->type == CF_HDROP)
          {
            if (WDL_NORMALLY(bufsz > sizeof(DROPFILES)))
            {
              const DROPFILES *hdr = (const DROPFILES *)buf;
              if (
                  WDL_NORMALLY(hdr->pFiles < bufsz) &&
                  WDL_NORMALLY(!hdr->fWide) // todo deal with UTF-16
              )
              {
                NSMutableArray *list = [NSMutableArray arrayWithCapacity:20];
                const char *rd = (const char *)buf;
                DWORD rdo = hdr->pFiles;
                while (rdo < bufsz && rd[rdo])
                {
                  NSString *fnstr=(NSString *)SWELL_CStringToCFString(rd+rdo);
                  NSURL *url = [NSURL fileURLWithPath:fnstr];
                  [fnstr release];
                  if (url) [list addObject:url];
                  rdo += strlen(rd+rdo)+1;
                }

                if ([list count] && SWELL_GetOSXVersion() >= 0x1060)
                {
                  [pasteboard writeObjects:list];
                }
              }
            }
          }
          else
          {
            NSString *fmt=m_clip_fmts.Get(cs->type-1);
            if (fmt)
          {
            NSData *data=[NSData dataWithBytes:buf length:bufsz];
            [pasteboard setData:data forType:fmt];
          }
          }
          GlobalUnlock(cs->h);
        }
      }
    }
    [ar release];
    m_clipsPending.Empty(true);
  }  
}

UINT EnumClipboardFormats(UINT lastfmt)
{
  if (lastfmt == 0) return (UINT)(INT_PTR)m_clip_curfmts.Get(0);
  const int idx = m_clip_curfmts.Find((char *)(INT_PTR)lastfmt);
  return idx >= 0 ? (UINT)(INT_PTR)m_clip_curfmts.Get(idx+1) : 0;
}

HANDLE GetClipboardData(UINT type)
{
  RegisterClipboardFormat(NULL);
  NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
  
  HANDLE h=0;
  if (type == CF_TEXT)
  {
    [pasteboard types];
    NSString *str = [pasteboard stringForType:NSStringPboardType];
    if (str)
    {
      int l = (int) ([str length]*4 + 32);
      char *buf = (char *)malloc(l);
      if (!buf) return 0;
      SWELL_CFStringToCString(str,buf,l);
      buf[l-1]=0;
      l = (int) (strlen(buf)+1);
      h=GlobalAlloc(0,l);  
      memcpy(GlobalLock(h),buf,l);
      GlobalUnlock(h);
      free(buf);
    }
  }
  else if (type == CF_HDROP)
  {
    if (SWELL_GetOSXVersion()>=0x1060)
    {
      [pasteboard types];
      NSArray *list = [pasteboard
        readObjectsForClasses:[NSArray arrayWithObject:[NSURL class]]
        options:[NSMutableDictionary dictionaryWithCapacity:1]
      ];
      int nf = (int) [list count];
      if (nf > 0)
      {
        WDL_TypedQueue<char> flist;
        flist.Add(NULL,sizeof(DROPFILES));
        for (int x=0;x<nf;x++)
        {
          NSURL *url = (NSURL *)[list objectAtIndex:x];
          if ([url isFileURL])
          {
            const char *ptr = [[url path] UTF8String];
            if (ptr && *ptr) flist.Add(ptr, strlen(ptr)+1);
          }
        }
        if (flist.GetSize()>sizeof(DROPFILES))
        {
          flist.Add("",1);
          DROPFILES *hdr = (DROPFILES*)flist.Get();
          memset(hdr,0,sizeof(*hdr));
          hdr->pFiles = sizeof(DROPFILES);
          h=GlobalAlloc(0,flist.GetSize());
          if (h) memcpy(GlobalLock(h),flist.Get(),flist.GetSize());
          GlobalUnlock(h);
        }
      }
    }
  }
  else
  {
    NSString *fmt=m_clip_fmts.Get(type-1);
    if (fmt)
    {
    NSData *data=[pasteboard dataForType:fmt];
    if (!data) return 0; 
    int l = (int)[data length];
    h=GlobalAlloc(0,l);  
    if (h) memcpy(GlobalLock(h),[data bytes],l);
    GlobalUnlock(h);
  }
  }
  
  if (h) m_clip_recs.Add(h);
	return h;
}

void EmptyClipboard()
{
  m_clipsPending.Empty(true);
}


void SetClipboardData(UINT type, HANDLE h)
{
  m_clipsPending.Add(new swell_pendingClipboardStates(type,h));    
}

UINT RegisterClipboardFormat(const char *desc)
{
  if (!m_clip_fmts.GetSize())
  {
    m_clip_fmts.Add([NSStringPboardType retain]); // CF_TEXT
    m_clip_fmts.Add(NULL); // CF_HDROP
  }
  if (!desc || !*desc) return 0;

  if (!strcmp(desc,"SWELL__CF_TEXT")) return CF_TEXT; // for legacy SWELL users

  NSString *s=(NSString*)SWELL_CStringToCFString(desc);
  int x;
  for (x = 0; x < m_clip_fmts.GetSize(); x ++)
  {
    NSString *ts=m_clip_fmts.Get(x);
    if (ts && [ts compare:s]==NSOrderedSame)
    {
      [s release];
      return x+1;
    }
  }
  m_clip_fmts.Add(s);
  return m_clip_fmts.GetSize();
}

int EnumPropsEx(HWND hwnd, PROPENUMPROCEX proc, LPARAM lParam)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd respondsToSelector:@selector(swellEnumProps:lp:)])) return -1;
  return (int)[(SWELL_hwndChild *)hwnd swellEnumProps:proc lp:lParam];
}

HANDLE GetProp(HWND hwnd, const char *name)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd respondsToSelector:@selector(swellGetProp:wantRemove:)])) return NULL;
  return (HANDLE)[(SWELL_hwndChild *)hwnd swellGetProp:name wantRemove:NO];
}

BOOL SetProp(HWND hwnd, const char *name, HANDLE val)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd respondsToSelector:@selector(swellSetProp:value:)])) return FALSE;
  return (BOOL)!![(SWELL_hwndChild *)hwnd swellSetProp:name value:val];
}

HANDLE RemoveProp(HWND hwnd, const char *name)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd respondsToSelector:@selector(swellGetProp:wantRemove:)])) return NULL;
  return (HANDLE)[(SWELL_hwndChild *)hwnd swellGetProp:name wantRemove:YES];
}


int GetSystemMetrics(int p)
{
switch (p)
{
case SM_CXSCREEN:
case SM_CYSCREEN:
{
  NSScreen *s=[NSScreen mainScreen];
  if (!s) return 1024;
  return p==SM_CXSCREEN ? [s frame].size.width : [s frame].size.height;
}
case SM_CXHSCROLL: return 16;
case SM_CYHSCROLL: return 16;
case SM_CXVSCROLL: return 16;
case SM_CYVSCROLL: return 16;
}
return 0;
}

BOOL ScrollWindow(HWND hwnd, int xamt, int yamt, const RECT *lpRect, const RECT *lpClipRect)
{
  if (hwnd && [(id)hwnd isKindOfClass:[NSWindow class]]) hwnd=(HWND)[(id)hwnd contentView];
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[NSView class]])) return FALSE;

  if (!xamt && !yamt) return FALSE;
  
  // move child windows only
  if (1)
  {
    if (xamt || yamt)
    {
      NSArray *ar=[(NSView*)hwnd subviews];
      NSInteger i,c=[ar count];
      for(i=0;i<c;i++)
      {
        NSView *v=(NSView *)[ar objectAtIndex:i];
        NSRect r=[v frame];
        r.origin.x+=xamt;
        r.origin.y+=yamt;
        [v setFrame:r];
      }
    }
    [(id)hwnd setNeedsDisplay:YES];
  }
  else
  {
    NSRect r=[(NSView*)hwnd bounds];
    r.origin.x -= xamt;
    r.origin.y -= yamt;
    [(id)hwnd setBoundsOrigin:r.origin];
    [(id)hwnd setNeedsDisplay:YES];
  }
  return TRUE;
}

HWND FindWindowEx(HWND par, HWND lastw, const char *classname, const char *title)
{
  // note: this currently is far far far from fully functional, bleh
  if (!par)
  {
    if (!title) return NULL;

    // get a list of top level windows, find any that match
    // (this does not scan child windows, which is a todo really)
    HWND rv=NULL;
    NSArray *ch=[NSApp windows];
    NSInteger x=0,n=[ch count];
    if (lastw)
    {
      for(;x<n; x ++)
      {
        NSWindow *w = [ch objectAtIndex:x]; 
        if ((HWND)w == lastw || (HWND)[w contentView] == lastw) break;
      }
      x++;
    }

    NSString *srch=(NSString*)SWELL_CStringToCFString(title);
    for(;x<n && !rv; x ++)
    {
      NSWindow *w = [ch objectAtIndex:x]; 
      if ([[w title] isEqualToString:srch]) 
      {
        rv=(HWND)[w contentView];
        if (classname)
        {
          char tmp[1024];
          if (!GetClassName(rv,tmp,sizeof(tmp)) || strcmp(tmp,classname))
            rv = NULL;
        }
      }
    }
    [srch release]; 

    return rv;
  }
  HWND h=lastw?GetWindow(lastw,GW_HWNDNEXT):GetWindow(par,GW_CHILD);
  while (h)
  {
    bool isOk=true;
    if (title)
    {
      char buf[512];
      buf[0]=0;
      GetWindowText(h,buf,sizeof(buf));
      if (strcmp(title,buf)) isOk=false;
    }
    if (isOk && classname)
    {
      char tmp[1024];
      if (!GetClassName(h,tmp,sizeof(tmp)) || strcmp(tmp,classname)) 
      {
        if (!stricmp(classname,"Static") && [(id)h isKindOfClass:[NSTextField class]])
        {
          // allow finding "Static" to match a textfield
        }
        else
          isOk = false;
      }
    }
    
    if (isOk) return h;
    h=GetWindow(h,GW_HWNDNEXT);
  }
  return h;
}

BOOL TreeView_SetIndent(HWND hwnd, int indent)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_TreeView class]])) return 0;
  SWELL_TreeView* tv = (SWELL_TreeView*)hwnd;  
  [tv setIndentationPerLevel:(float)indent];  
  return TRUE;
}

HTREEITEM TreeView_InsertItem(HWND hwnd, TV_INSERTSTRUCT *ins)
{
  if (WDL_NOT_NORMALLY(!hwnd || !ins || ![(id)hwnd isKindOfClass:[SWELL_TreeView class]])) return 0;
  
  SWELL_TreeView *tv=(SWELL_TreeView*)hwnd;

  HTREEITEM__ *par=NULL;
  int inspos=0;
  
  if (ins->hParent && ins->hParent != TVI_ROOT && ins->hParent != TVI_FIRST && ins->hParent != TVI_LAST && ins->hParent != TVI_SORT)
  {
    if ([tv findItem:ins->hParent parOut:&par idxOut:&inspos])
    {
      par = (HTREEITEM__ *)ins->hParent; 
    }
    else return 0;
  }
  
  if (ins->hInsertAfter == TVI_FIRST) inspos=0;
  else if (ins->hInsertAfter == TVI_LAST || ins->hInsertAfter == TVI_SORT || !ins->hInsertAfter) inspos=par ? par->m_children.GetSize() : tv->m_items ? tv->m_items->GetSize() : 0;
  else inspos = par ? par->m_children.Find((HTREEITEM__*)ins->hInsertAfter)+1 : tv->m_items ? tv->m_items->Find((HTREEITEM__*)ins->hInsertAfter)+1 : 0;      
  
  HTREEITEM__ *item=new HTREEITEM__;
  if (ins->item.mask & TVIF_CHILDREN)
    item->m_haschildren = !!ins->item.cChildren;
  if (ins->item.mask & TVIF_PARAM) item->m_param = ins->item.lParam;
  if (ins->item.mask & TVIF_TEXT) item->m_value = strdup(ins->item.pszText);
  if (!par)
  {
    if (!tv->m_items) tv->m_items = new WDL_PtrList<HTREEITEM__>;
    tv->m_items->Insert(inspos,item);
  }
  else par->m_children.Insert(inspos,item);
  
  [tv reloadData];
  return (HTREEITEM) item;
}

BOOL TreeView_Expand(HWND hwnd, HTREEITEM item, UINT flag)
{
  if (WDL_NOT_NORMALLY(!hwnd || !item)) return false;
  
  if (WDL_NOT_NORMALLY(![(id)hwnd isKindOfClass:[SWELL_TreeView class]])) return false;
  
  SWELL_TreeView *tv=(SWELL_TreeView*)hwnd;
  
  id itemid=((HTREEITEM__*)item)->m_dh;
  bool isExp=!![tv isItemExpanded:itemid];
  
  if (flag == TVE_EXPAND && !isExp) [tv expandItem:itemid];
  else if (flag == TVE_COLLAPSE && isExp) [tv collapseItem:itemid];
  else if (flag==TVE_TOGGLE) 
  {
    if (isExp) [tv collapseItem:itemid];
    else [tv expandItem:itemid];
  }
  else return FALSE;

  return TRUE;
  
}

HTREEITEM TreeView_GetSelection(HWND hwnd)
{ 
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_TreeView class]])) return NULL;
  
  SWELL_TreeView *tv=(SWELL_TreeView*)hwnd;
  NSInteger idx=[tv selectedRow];
  if (idx<0) return NULL;
  
  SWELL_DataHold *t=[tv itemAtRow:idx];
  if (t) return (HTREEITEM)[t getValue];
  return NULL;
  
}

void TreeView_DeleteItem(HWND hwnd, HTREEITEM item)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_TreeView class]])) return;
  SWELL_TreeView *tv=(SWELL_TreeView*)hwnd;
  
  HTREEITEM__ *par=NULL;
  int idx=0;
  
  if ([tv findItem:item parOut:&par idxOut:&idx])
  {
    if (par)
    {
      par->m_children.Delete(idx,true);
    }
    else if (tv->m_items)
    {
      tv->m_items->Delete(idx,true);
    }
    [tv reloadData];
  }
}

void TreeView_DeleteAllItems(HWND hwnd)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_TreeView class]])) return;
  SWELL_TreeView *tv=(SWELL_TreeView*)hwnd;
  
  if (tv->m_items) tv->m_items->Empty(true);
  [tv reloadData];
}

void TreeView_EnsureVisible(HWND hwnd, HTREEITEM item)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_TreeView class]])) return;
  if (!item) return;
  NSInteger row=[(SWELL_TreeView*)hwnd rowForItem:((HTREEITEM__*)item)->m_dh];
  if (row>=0)
    [(SWELL_TreeView*)hwnd scrollRowToVisible:row];
}

void TreeView_SelectItem(HWND hwnd, HTREEITEM item)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_TreeView class]])) return;
  
  NSInteger row=[(SWELL_TreeView*)hwnd rowForItem:((HTREEITEM__*)item)->m_dh];
  if (row>=0)
    [(SWELL_TreeView*)hwnd selectRowIndexes:[NSIndexSet indexSetWithIndex:row] byExtendingSelection:NO];            
  static int __rent;
  if (!__rent)
  {
    __rent=1;
    NMTREEVIEW nm={{(HWND)hwnd,(UINT_PTR)[(SWELL_TreeView*)hwnd tag],TVN_SELCHANGED},};
    nm.itemNew.hItem = item;
    nm.itemNew.lParam = item ? item->m_param : 0;
    SendMessage(GetParent(hwnd),WM_NOTIFY,nm.hdr.idFrom,(LPARAM)&nm);
    __rent=0;
  }
}

BOOL TreeView_GetItem(HWND hwnd, LPTVITEM pitem)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_TreeView class]] || !pitem) || 
      !(pitem->mask & TVIF_HANDLE) || !(pitem->hItem)) return FALSE;
  
  HTREEITEM__ *ti = (HTREEITEM__*)pitem->hItem;
  pitem->cChildren = ti->m_haschildren ? 1:0;
  pitem->lParam = ti->m_param;
  if ((pitem->mask&TVIF_TEXT)&&pitem->pszText&&pitem->cchTextMax>0)
  {
    lstrcpyn_safe(pitem->pszText,ti->m_value?ti->m_value:"",pitem->cchTextMax);
  }
  pitem->state=0;
  
  
  NSInteger itemRow = [(SWELL_TreeView*)hwnd rowForItem:ti->m_dh];
  if (itemRow >= 0 && [(SWELL_TreeView*)hwnd isRowSelected:itemRow])
    pitem->state |= TVIS_SELECTED;   
  if ([(SWELL_TreeView*)hwnd isItemExpanded:ti->m_dh])
    pitem->state |= TVIS_EXPANDED;   
  
  return TRUE;
}

BOOL TreeView_SetItem(HWND hwnd, LPTVITEM pitem)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_TreeView class]] || !pitem) || 
    !(pitem->mask & TVIF_HANDLE) || !(pitem->hItem)) return FALSE;
  
  HTREEITEM__ *par=NULL;
  int idx=0;
  
  if (![(SWELL_TreeView*)hwnd findItem:pitem->hItem parOut:&par idxOut:&idx]) return FALSE;
  
  HTREEITEM__ *ti = (HTREEITEM__*)pitem->hItem;
  
  if (pitem->mask & TVIF_CHILDREN) ti->m_haschildren = pitem->cChildren?1:0;
  if (pitem->mask & TVIF_PARAM)  ti->m_param =  pitem->lParam;
  
  if ((pitem->mask&TVIF_TEXT)&&pitem->pszText)
  {
    free(ti->m_value);
    ti->m_value=strdup(pitem->pszText);
    InvalidateRect(hwnd, 0, FALSE);
  }

  if (pitem->stateMask & TVIS_SELECTED)
  {
    NSInteger itemRow = [(SWELL_TreeView*)hwnd rowForItem:ti->m_dh];
    if (itemRow >= 0)
    {
      if (pitem->state&TVIS_SELECTED)
      {
        [(SWELL_TreeView*)hwnd selectRowIndexes:[NSIndexSet indexSetWithIndex:itemRow] byExtendingSelection:NO];
        
        static int __rent;
        if (!__rent)
        {
          __rent=1;
          NMTREEVIEW nm={{(HWND)hwnd,(UINT_PTR)[(SWELL_TreeView*)hwnd tag],TVN_SELCHANGED},};
          nm.itemNew.hItem = ti;
          nm.itemNew.lParam = ti ? ti->m_param : 0;
          SendMessage(GetParent(hwnd),WM_NOTIFY,nm.hdr.idFrom,(LPARAM)&nm);
          __rent=0;
        }
        
      }
      else
      {
        // todo figure out unselect?!
//         [(SWELL_TreeView*)hwnd selectRowIndexes:[NSIndexSet indexSetWithIndex:itemRow] byExtendingSelection:NO];
      }
    }
  }
  
  if (pitem->stateMask & TVIS_EXPANDED)
    TreeView_Expand(hwnd,pitem->hItem,(pitem->state&TVIS_EXPANDED)?TVE_EXPAND:TVE_COLLAPSE);
    
  
  return TRUE;
}

HTREEITEM TreeView_HitTest(HWND hwnd, TVHITTESTINFO *hti)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_TreeView class]] || !hti)) return NULL;
  SWELL_TreeView* tv = (SWELL_TreeView*)hwnd;
  int x = hti->pt.x;
  int y = hti->pt.y;
  
  // treeview might be clipped
  POINT wp={x, y};
  ClientToScreen(hwnd, &wp);
  RECT wr;
  GetWindowRect(hwnd, &wr);
  if (wp.x < wr.left || wp.x >= wr.right) return NULL;
  if (wp.y < wdl_min(wr.top, wr.bottom) || wp.y >= wdl_max(wr.top, wr.bottom)) return NULL;

  int i; 
  double maxy = 0.0;
  for (i = 0; i < [tv numberOfRows]; ++i)
  {
    NSRect r = [tv rectOfRow:i];
    maxy = wdl_max(maxy, r.origin.y + r.size.height);
    if (x >= r.origin.x && x < r.origin.x+r.size.width && y >= r.origin.y && y < r.origin.y+r.size.height)
    {
      SWELL_DataHold* t = [tv itemAtRow:i];
      if (t) return (HTREEITEM)[t getValue];
      return 0;
    }
  }
  if (y >= maxy)
  {
    hti->flags |= TVHT_BELOW;
  }
  
  return NULL; // not hit
}

HTREEITEM TreeView_GetRoot(HWND hwnd)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_TreeView class]])) return NULL;
  SWELL_TreeView *tv=(SWELL_TreeView*)hwnd;
  
  if (!tv->m_items) return 0;
  return (HTREEITEM) tv->m_items->Get(0);
}

HTREEITEM TreeView_GetChild(HWND hwnd, HTREEITEM item)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_TreeView class]])) return NULL;

  HTREEITEM__ *titem=(HTREEITEM__ *)item;
  if (!titem || item == TVI_ROOT) return TreeView_GetRoot(hwnd);
  
  return (HTREEITEM) titem->m_children.Get(0);
}
HTREEITEM TreeView_GetNextSibling(HWND hwnd, HTREEITEM item)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_TreeView class]])) return NULL;
  SWELL_TreeView *tv=(SWELL_TreeView*)hwnd;

  if (!item) return TreeView_GetRoot(hwnd);
  
  HTREEITEM__ *par=NULL;
  int idx=0;  
  if ([tv findItem:item parOut:&par idxOut:&idx])
  {
    if (par)
    {
      return par->m_children.Get(idx+1);
    }    
    if (tv->m_items) return tv->m_items->Get(idx+1);
  }
  return 0;
}

void TreeView_SetBkColor(HWND hwnd, int color)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_TreeView class]])) return;
  [(NSOutlineView*)hwnd setBackgroundColor:[NSColor colorWithCalibratedRed:GetRValue(color)/255.0f 
              green:GetGValue(color)/255.0f 
              blue:GetBValue(color)/255.0f alpha:1.0f]];
}
void TreeView_SetTextColor(HWND hwnd, int color)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_TreeView class]])) return;

  SWELL_TreeView *f = (SWELL_TreeView *)hwnd;
  [f->m_fgColor release];
  f->m_fgColor = [NSColor colorWithCalibratedRed:GetRValue(color)/255.0f 
              green:GetGValue(color)/255.0f 
              blue:GetBValue(color)/255.0f alpha:1.0f];
  [f->m_fgColor retain];
}
void ListView_SetBkColor(HWND hwnd, int color)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_ListView class]])) return;
  [(NSTableView*)hwnd setBackgroundColor:[NSColor colorWithCalibratedRed:GetRValue(color)/255.0f 
              green:GetGValue(color)/255.0f 
              blue:GetBValue(color)/255.0f alpha:1.0f]];
}

void ListView_SetSelColors(HWND hwnd, int *colors, int ncolors) // this works for SWELL_ListView as well as SWELL_TreeView
{
  if (WDL_NOT_NORMALLY(!hwnd)) return;
  NSMutableArray *ar=[[NSMutableArray alloc] initWithCapacity:ncolors];
  
  while (ncolors-->0)
  {
    const int color = colors ? *colors++ : 0;
    [ar addObject:[NSColor colorWithCalibratedRed:GetRValue(color)/255.0f
                                              green:GetGValue(color)/255.0f 
                                               blue:GetBValue(color)/255.0f alpha:1.0f]]; 
  }

  if ([(id)hwnd isKindOfClass:[SWELL_ListView class]]) 
  {
    SWELL_ListView *lv = (SWELL_ListView*)hwnd;
    [lv->m_selColors release];
    lv->m_selColors=ar;
  }
  else if ([(id)hwnd isKindOfClass:[SWELL_TreeView class]]) 
  {
    SWELL_TreeView *lv = (SWELL_TreeView*)hwnd;
    [lv->m_selColors release];
    lv->m_selColors=ar;
  }
  else 
  {
    WDL_ASSERT(false);
    [ar release];
  }
}
void ListView_SetGridColor(HWND hwnd, int color)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_ListView class]])) return;
  [(NSTableView*)hwnd setGridColor:[NSColor colorWithCalibratedRed:GetRValue(color)/255.0f 
              green:GetGValue(color)/255.0f 
              blue:GetBValue(color)/255.0f alpha:1.0f]];
}
void ListView_SetTextBkColor(HWND hwnd, int color)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_ListView class]])) return;
  // not implemented atm
}
void ListView_SetTextColor(HWND hwnd, int color)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[SWELL_ListView class]])) return;

  SWELL_ListView *f = (SWELL_ListView *)hwnd;
  [f->m_fgColor release];
  f->m_fgColor = [NSColor colorWithCalibratedRed:GetRValue(color)/255.0f 
              green:GetGValue(color)/255.0f 
              blue:GetBValue(color)/255.0f alpha:1.0f];
  [f->m_fgColor retain];
}


BOOL ShellExecute(HWND hwndDlg, const char *action,  const char *content1, const char *content2, const char *content3, int blah)
{
  if (content1 && (!strnicmp(content1,"http://",7) || !strnicmp(content1,"https://",8)))
  {
     NSWorkspace *wk = [NSWorkspace sharedWorkspace];
     if (!wk) return FALSE;
     NSString *fnstr=(NSString *)SWELL_CStringToCFString(content1);
     NSURL *url = [NSURL URLWithString:fnstr];
     BOOL ret=url && [wk openURL:url];
     [fnstr release];
     return ret;
  }
  
  if (content1 && !stricmp(content1,"explorer.exe")) content1="";
  else if (content1 && (!stricmp(content1,"notepad.exe")||!stricmp(content1,"notepad"))) content1="TextEdit.app";
  
  if (content2 && !stricmp(content2,"explorer.exe")) content2="";

  if (content1 && content2 && *content1 && *content2)
  {
      NSWorkspace *wk = [NSWorkspace sharedWorkspace];
      if (!wk) return FALSE;
      NSString *appstr=(NSString *)SWELL_CStringToCFString(content1);
      NSString *fnstr=(NSString *)SWELL_CStringToCFString(content2);
      BOOL ret=[wk openFile:fnstr withApplication:appstr andDeactivate:YES];
      [fnstr release];
      [appstr release];
      return ret;
  }
  else if ((content1&&*content1) || (content2&&*content2))
  {
      const char *fn = (content1 && *content1) ? content1 : content2;
      NSWorkspace *wk = [NSWorkspace sharedWorkspace];
      if (!wk) return FALSE;
      NSString *fnstr = nil;
      BOOL ret = FALSE;
    
      if (fn && !strnicmp(fn, "/select,\"", 9))
      {
        char* tmp = strdup(fn+9);
        if (*tmp && tmp[strlen(tmp)-1]=='\"') tmp[strlen(tmp)-1]='\0';
        if (*tmp)
        {
          if ([wk respondsToSelector:@selector(activateFileViewerSelectingURLs:)]) // 10.6+
          {
            fnstr=(NSString *)SWELL_CStringToCFString(tmp);
            NSURL *url = [NSURL fileURLWithPath:fnstr isDirectory:false];
            if (url)
            {
              [wk activateFileViewerSelectingURLs:[NSArray arrayWithObjects:url, nil]]; // NSArray (and NSURL) autoreleased
              ret=TRUE;
            }
          }
          else
          {
            if (WDL_remove_filepart(tmp))
            {
              fnstr=(NSString *)SWELL_CStringToCFString(tmp);
              ret=[wk openFile:fnstr];
            }
          }
        }
        free(tmp);
      }
      else if (strlen(fn)>4 && !stricmp(fn+strlen(fn)-4,".app"))
      {
        fnstr=(NSString *)SWELL_CStringToCFString(fn);
        ret=[wk launchApplication:fnstr];
      }
      else
      {
        fnstr=(NSString *)SWELL_CStringToCFString(fn);
        ret=[wk openFile:fnstr];
      }
      [fnstr release];
      return ret;
  }
  return FALSE;
}




@implementation SWELL_FocusRectWnd

-(BOOL)isOpaque { return YES; }
-(void) drawRect:(NSRect)rect
{
  NSColor *col=[NSColor colorWithCalibratedRed:0.5 green:0.5 blue:0.5 alpha:1.0];
  [col set];
  
  CGRect r = CGRectMake(rect.origin.x,rect.origin.y,rect.size.width,rect.size.height);
  
  CGContextRef ctx = (CGContextRef) [[NSGraphicsContext currentContext] graphicsPort];
  
  CGContextFillRect(ctx,r);	         
  
}
@end

// r=NULL to "free" handle
// otherwise r is in hwndPar coordinates
void SWELL_DrawFocusRect(HWND hwndPar, RECT *rct, void **handle)
{
  if (!handle) return;
  NSWindow *wnd = (NSWindow *)*handle;
  
  if (!rct)
  {
    if (wnd)
    {
      NSWindow *ow=[wnd parentWindow];
      if (ow) [ow removeChildWindow:wnd];
//      [wnd setParentWindow:nil];
      [wnd close];
      *handle=0;
    }
  }
  else 
  {
    RECT r=*rct;
    if (hwndPar)
    {
      ClientToScreen(hwndPar,((LPPOINT)&r));
      ClientToScreen(hwndPar,((LPPOINT)&r)+1);
    }
    else
    {
      // todo: flip?
    }
    if (r.top>r.bottom) { int a=r.top; r.top=r.bottom;r.bottom=a; }
    NSRect rr=NSMakeRect(r.left,r.top,r.right-r.left,r.bottom-r.top);
    
    NSWindow *par=nil;
    if (hwndPar)
    {
      if ([(id)hwndPar isKindOfClass:[NSWindow class]]) par=(NSWindow *)hwndPar;
      else if ([(id)hwndPar isKindOfClass:[NSView class]]) par=[(NSView *)hwndPar window];
      else return;
    }
    
    if (wnd && ([wnd parentWindow] != par))
    {
      NSWindow *ow=[wnd parentWindow];
      if (ow) [ow removeChildWindow:wnd];
      //      [wnd setParentWindow:nil];
      [wnd close];
      *handle=0;
      wnd=0;    
    }
    
    if (!wnd)
    {
      *handle  = wnd = [[NSWindow alloc] initWithContentRect:rr styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES];
      [wnd setOpaque:YES];
      [wnd setAlphaValue:0.5];
      [wnd setExcludedFromWindowsMenu:YES];
      [wnd setIgnoresMouseEvents:YES];
      [wnd setContentView:[[SWELL_FocusRectWnd alloc] init]];
      
      if (par) [par addChildWindow:wnd ordered:NSWindowAbove];
      else 
      {
        [wnd setLevel:NSPopUpMenuWindowLevel];
        [wnd orderFront:wnd];
      }
      //    [wnd setParentWindow:par];
//      [wnd orderWindow:NSWindowAbove relativeTo:[par windowNumber]];
    }
    
    [wnd setFrame:rr display:YES];    
  }
}


@implementation SWELL_PopUpButton
STANDARD_CONTROL_NEEDSDISPLAY_IMPL("combobox")

-(void)setSwellStyle:(LONG)style { m_style=style; }
-(LONG)getSwellStyle { return m_style; }
@end

@implementation SWELL_ComboBox
STANDARD_CONTROL_NEEDSDISPLAY_IMPL("combobox")

-(void)setSwellStyle:(LONG)style { m_style=style; }
-(LONG)getSwellStyle { return m_style; }
-(id)init {
  self = [super init];
  if (self)
  {
    m_ids=new WDL_PtrList<char>;
    m_ignore_selchg = -1;
  }
  return self;
}
-(void)dealloc { delete m_ids; [super dealloc];  }
- (BOOL)becomeFirstResponder;
{
  BOOL didBecomeFirstResponder = [super becomeFirstResponder];
  if (didBecomeFirstResponder) SendMessage(GetParent((HWND)self),WM_COMMAND,[self tag]|(EN_SETFOCUS<<16),(LPARAM)self);
  return didBecomeFirstResponder;
}
@end




bool SWELL_HandleMouseEvent(NSEvent *evt)
{
  NSEventType etype = [evt type];
  if (GetCapture()) return false;
  if (etype >= NSLeftMouseDown && etype <= NSRightMouseDragged)
  {
  }
  else return false;
  
  NSWindow *w = [evt window];
  if (w)
  {
    NSView *cview = [w contentView];
    NSView *besthit=NULL;
    if (cview)
    {
      NSPoint lpt = [evt locationInWindow];    
      NSView *hitv=[cview hitTest:lpt];
      lpt = [w convertBaseToScreen:lpt];
      
      int xpos=(int)floor(lpt.x + 0.5);
      int ypos=(int)floor(lpt.y + 0.5);
      
      while (hitv)
      {
        int ht=(int)sendSwellMessage(hitv,WM_NCHITTEST,0,MAKELPARAM(xpos,ypos));
        if (ht && ht != HTCLIENT) besthit=hitv;

        if (hitv==cview) break;
        hitv = [hitv superview];
      }
    }
    if (besthit)
    {
      if (etype == NSLeftMouseDown) [besthit mouseDown:evt];
      else if (etype == NSLeftMouseUp) [besthit mouseUp:evt];
      else if (etype == NSLeftMouseDragged) [besthit mouseDragged:evt];
      else if (etype == NSRightMouseDown) [besthit rightMouseDown:evt];
      else if (etype == NSRightMouseUp) [besthit rightMouseUp:evt];
      else if (etype == NSRightMouseDragged) [besthit rightMouseDragged:evt];
      else if (etype == NSMouseMoved) [besthit mouseMoved:evt];
      else return false;
      
      return true;
    }
  }
  return false;
}

int SWELL_GetWindowWantRaiseAmt(HWND h)
{
  SWELL_ModelessWindow* mw=0;
  if ([(id)h isKindOfClass:[SWELL_ModelessWindow class]])
  {
    mw=(SWELL_ModelessWindow*)h;
  }
  else if ([(id)h isKindOfClass:[NSView class]])
  {
    NSWindow* wnd=[(NSView*)h window];
    if (wnd && [wnd isKindOfClass:[SWELL_ModelessWindow class]])
    {
      mw=(SWELL_ModelessWindow*)wnd;
    }
  }
  else
  {
    WDL_ASSERT(false);
  }
  if (mw) return mw->m_wantraiseamt;  
  return 0; 
}

void SWELL_SetWindowWantRaiseAmt(HWND h, int  amt)
{
  SWELL_ModelessWindow *mw=NULL;
  if ([(id)h isKindOfClass:[SWELL_ModelessWindow class]]) mw=(SWELL_ModelessWindow *)h;
  else if ([(id)h isKindOfClass:[NSView class]])
  {
    NSWindow *w = [(NSView *)h window];
    if (w && [w isKindOfClass:[SWELL_ModelessWindow class]]) mw = (SWELL_ModelessWindow*)w;
  }
  if (mw) 
  {
    int diff = amt - mw->m_wantraiseamt;
    mw->m_wantraiseamt = amt;
    if (diff && [NSApp isActive]) [mw setLevel:[mw level]+diff];
  }
  else
  {
    WDL_ASSERT(false);
  }
}


int SWELL_SetWindowLevel(HWND hwnd, int newlevel)
{
  NSWindow *w = (NSWindow *)hwnd;
  if (w && [w isKindOfClass:[NSView class]]) w= [(NSView *)w window];
  
  if (WDL_NORMALLY(w && [w isKindOfClass:[NSWindow class]]))
  {
    int ol = (int)[w level];
    [w setLevel:newlevel];
    return ol;
  }
  return 0;
}

void SetAllowNoMiddleManRendering(HWND h, bool allow)
{
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_hwndChild class]])) return;
  SWELL_hwndChild* v = (SWELL_hwndChild*)h;
  v->m_allow_nomiddleman = allow;
}

void SetOpaque(HWND h, bool opaque)
{
  if (WDL_NOT_NORMALLY(!h || ![(id)h isKindOfClass:[SWELL_hwndChild class]])) return;
  SWELL_hwndChild* v = (SWELL_hwndChild*)h;
  [v setOpaque:opaque];
}

void SetTransparent(HWND h)
{
  if (WDL_NOT_NORMALLY(!h)) return;
  NSWindow* wnd=0;
  if ([(id)h isKindOfClass:[NSWindow class]]) wnd=(NSWindow*)h;
  else if ([(id)h isKindOfClass:[NSView class]]) wnd=[(NSView*)h window];
  if (WDL_NORMALLY(wnd)) 
  {
    [wnd setBackgroundColor:[NSColor clearColor]];
    [wnd setOpaque:NO];
  }  
}

int SWELL_GetDefaultButtonID(HWND hwndDlg, bool onlyIfEnabled)
{
  if (WDL_NOT_NORMALLY(![(id)hwndDlg isKindOfClass:[NSView class]])) return 0;
  NSWindow *wnd = [(NSView *)hwndDlg window];
  NSButtonCell * cell = wnd ? [wnd defaultButtonCell] : nil;
  NSView *view;
  if (!cell || !(view=[cell controlView])) return 0;
  int cmdid = (int)[view tag];
  if (cmdid && onlyIfEnabled)
  {
    if (![cell isEnabled]) return 0;
  }
  return cmdid;
}


void SWELL_SetWindowRepre(HWND hwnd, const char *fn, bool isDirty)
{
  if (WDL_NOT_NORMALLY(!hwnd)) return;
  NSWindow *w = NULL;
  if ([(id)hwnd isKindOfClass:[NSWindow class]]) w=(NSWindow *)hwnd;
  if ([(id)hwnd isKindOfClass:[NSView class]]) w=[(NSView *)hwnd window];
  
  if (WDL_NORMALLY(w))
  {
    if (GetProp((HWND)[w contentView],"SWELL_DisableWindowRepre")) return;
    
    [w setDocumentEdited:isDirty];
    
    if (!fn || !*fn) [w setRepresentedFilename:@""];
    else
    {
      NSString *str = (NSString *)SWELL_CStringToCFString(fn);
      [w setRepresentedFilename:str];
      [str release];
    }
  }
}

void SWELL_SetWindowShadow(HWND hwnd, bool shadow)
{
  if (WDL_NOT_NORMALLY(!hwnd)) return;
  NSWindow *w = (NSWindow *)hwnd;
  if ([w isKindOfClass:[NSView class]]) w = [(NSView *)w window];
  if (WDL_NORMALLY(w && [w isKindOfClass:[NSWindow class]])) [w setHasShadow:shadow];
}

#if 0 // not sure if this will interfere with coolSB
BOOL ShowScrollBar(HWND hwnd, int nBar, BOOL vis)
{
  int v=0;
  if (nBar == SB_HORZ || nBar == SB_BOTH) v |= WS_HSCROLL;
  if (nBar == SB_VERT || nBar == SB_BOTH) v |= WS_VSCROLL;
  if (v)
  {
    int s=GetWindowLong(hwnd, GWL_STYLE);
    if (vis) s |= v;
    else s &= ~v;
    SetWindowLong(hwnd, GWL_STYLE, s);
    SetWindowPos(hwnd, 0, 0, 0, 0, 0, SWP_FRAMECHANGED|SWP_NOZORDER|SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    return TRUE;
  }
  return FALSE;
}
#endif


void SWELL_GenerateDialogFromList(const void *_list, int listsz)
{
#define SIXFROMLIST list->p1,list->p2,list->p3, list->p4, list->p5, list->p6
  SWELL_DlgResourceEntry *list = (SWELL_DlgResourceEntry*)_list;
  while (listsz>0)
  {
    if (!strcmp(list->str1,"__SWELL_BUTTON"))
    {
      SWELL_MakeButton(list->flag1,list->str2, SIXFROMLIST);
    } 
    else if (!strcmp(list->str1,"__SWELL_EDIT"))
    {
      SWELL_MakeEditField(SIXFROMLIST);
    }
    else if (!strcmp(list->str1,"__SWELL_COMBO"))
    {
      SWELL_MakeCombo(SIXFROMLIST);
    }
    else if (!strcmp(list->str1,"__SWELL_LISTBOX"))
    {
      SWELL_MakeListBox(SIXFROMLIST);
    }
    else if (!strcmp(list->str1,"__SWELL_GROUP"))
    {
      SWELL_MakeGroupBox(list->str2,SIXFROMLIST);
    }
    else if (!strcmp(list->str1,"__SWELL_CHECKBOX"))
    {
      SWELL_MakeCheckBox(list->str2,SIXFROMLIST);
    }
    else if (!strcmp(list->str1,"__SWELL_LABEL"))
    {
      SWELL_MakeLabel(list->flag1, list->str2, SIXFROMLIST);
    }
    else if (!strcmp(list->str1,"__SWELL_ICON"))
    {
      // todo (str2 is likely a (const char *)(INT_PTR)resid
    }
    else if (*list->str2)
    {
      SWELL_MakeControl(list->str1, list->flag1, list->str2, SIXFROMLIST);
    }
    listsz--;
    list++;
  }
}

BOOL EnumChildWindows(HWND hwnd, BOOL (*cwEnumFunc)(HWND,LPARAM),LPARAM lParam)
{
  if (WDL_NOT_NORMALLY(!hwnd || ![(id)hwnd isKindOfClass:[NSView class]])) return TRUE;
  NSArray *ar = [(NSView *)hwnd subviews];
  if (ar)
  {
    [ar retain];
    NSInteger x,n=[ar count];
    for (x=0;x<n;x++)
    {
      NSView *v = [ar objectAtIndex:x];
      if (v)
      {
        if ([v isKindOfClass:[NSScrollView class]])
        {
          NSView *sv=[(NSScrollView *)v documentView];
          if (sv) v=sv;
        }
        if ([v isKindOfClass:[NSClipView class]]) 
        {
          NSView *sv = [(NSClipView *)v documentView];
          if (sv) v=sv;
        }

        if (!cwEnumFunc((HWND)v,lParam) || !EnumChildWindows((HWND)v,cwEnumFunc,lParam)) 
        {
          [ar release];
          return FALSE;
        }
      }
    }
    [ar release];
  }
  return TRUE;
}

void SWELL_GetDesiredControlSize(HWND hwnd, RECT *r)
{
  if (WDL_NORMALLY(hwnd && r && [(id)hwnd isKindOfClass:[NSControl class]]))
  {
    NSControl *c = (NSControl *)hwnd;
    NSRect fr = [c frame];
    [c sizeToFit];
    NSRect frnew=[c frame];
    [c setFrame:fr];
    r->left=r->top=0;
    r->right = (int)(frnew.size.width+0.5);
    r->bottom = (int)(frnew.size.height+0.5);
  }
}

BOOL SWELL_IsGroupBox(HWND hwnd)
{
  if (WDL_NORMALLY(hwnd) && [(id)hwnd isKindOfClass:[SWELL_BoxView class]])
  {
    if (![(id)hwnd respondsToSelector:@selector(swellIsEtchBox)] || ![(SWELL_BoxView *)hwnd swellIsEtchBox])
      return TRUE;
  }
  return FALSE;
}
BOOL SWELL_IsButton(HWND hwnd)
{
  if (WDL_NORMALLY(hwnd) && [(id)hwnd isKindOfClass:[SWELL_Button class]]) return TRUE;
  return FALSE;
}
BOOL SWELL_IsStaticText(HWND hwnd)
{
  if (WDL_NORMALLY(hwnd) && [(id)hwnd isKindOfClass:[NSTextField class]])
  {
    NSTextField *obj = (NSTextField *)hwnd;
    if (![obj isEditable] && ![obj isSelectable])
      return TRUE;
  }
  return FALSE;
}

void SWELL_SetClassName(HWND hwnd, const char *p)
{
  if (WDL_NORMALLY(hwnd && [(id)hwnd isKindOfClass:[SWELL_hwndChild class]]))
    ((SWELL_hwndChild *)hwnd)->m_classname=p;
}

int GetClassName(HWND hwnd, char *buf, int bufsz)
{
  if (WDL_NOT_NORMALLY(!hwnd || !buf || bufsz<1)) return 0;
  buf[0]=0;
  if ([(id)hwnd respondsToSelector:@selector(getSwellClass)])
  {
    const char *cn = [(SWELL_hwndChild*)hwnd getSwellClass];
    if (cn) lstrcpyn_safe(buf,cn,bufsz);
  }
  else if ([(id)hwnd isKindOfClass:[NSButton class]])
  {
    lstrcpyn_safe(buf,"Button",bufsz);
  }
  else if ([(id)hwnd isKindOfClass:[NSTextField class]])
  {
    NSTextField *obj = (NSTextField *)hwnd;
    if (![obj isEditable] && ![obj isSelectable])
      lstrcpyn_safe(buf,"Static",bufsz);
    else
      lstrcpyn_safe(buf,"Edit",bufsz);
  }
  else
  {
    // default handling of other controls?
  }

  return (int)strlen(buf);
}



bool SWELL_SetAppAutoHideMenuAndDock(int ah) 
{
  static char _init;
  static NSUInteger _defpres;
  if (!_init)
  {
    if (SWELL_GetOSXVersion()>=0x1060)
    {
      _init=1;
      _defpres = [(SWELL_AppExtensions*)[NSApplication sharedApplication] presentationOptions];
    }
    else
    {
      _init=-1;
    }
  }
  if (_init > 0)
  {
    const int NSApplicationPresentationAutoHideDock               = (1 <<  0),
              NSApplicationPresentationHideDock = (1<<1),
              NSApplicationPresentationAutoHideMenuBar            = (1 <<  2);

    if (ah>0) [(SWELL_AppExtensions*)[NSApplication sharedApplication] setPresentationOptions:((ah>=2?NSApplicationPresentationHideDock:NSApplicationPresentationAutoHideDock)|NSApplicationPresentationAutoHideMenuBar)];
    else [(SWELL_AppExtensions*)[NSApplication sharedApplication] setPresentationOptions:_defpres];
    return true;
  }
  return false;
}

#endif
