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
  

    This file provides basic windows menu API to interface an NSMenu

  */

#ifndef SWELL_PROVIDED_BY_APP

#import <Cocoa/Cocoa.h>

#include "swell.h"
#include "swell-menugen.h"

#include "swell-internal.h"


static void __filtnametobuf(char *out, const char *in, int outsz)
{
  while (*in && outsz>1)
  {
    if (*in == '\t') break;
    if (*in == '&')
    {
      in++;
    }
    *out++=*in++;
    outsz--;
  }
  *out=0;
}



bool SetMenuItemText(HMENU hMenu, int idx, int flag, const char *text)
{
  NSMenu *menu=(NSMenu *)hMenu;
  
  NSMenuItem *item;
  if (flag & MF_BYPOSITION) item=[menu itemAtIndex:idx];
  else item =[menu itemWithTag:idx];
  if (!item) 
  {
    if (!(flag & MF_BYPOSITION))
    {
      const int n = (int) [menu numberOfItems];
      for (int x = 0; x < n; x ++)
      {
        item=[menu itemAtIndex:x];
        if (item && [item hasSubmenu])
        {
          NSMenu *m=[item submenu];
          if (m && SetMenuItemText((HMENU)m,idx,flag,text)) return true;
        }
      }
    }
    return false;
  }
  char buf[1024];
  __filtnametobuf(buf,text?text:"",sizeof(buf));
  NSString *label=(NSString *)SWELL_CStringToCFString(buf);
  
  [item setTitle:label];
  if ([item hasSubmenu] && [item submenu]) [[item submenu] setTitle:label];

  [label release];
  return true;
}

bool EnableMenuItem(HMENU hMenu, int idx, int en)
{
  NSMenu *menu=(NSMenu *)hMenu;
  
  NSMenuItem *item;
  if (en & MF_BYPOSITION) item=[menu itemAtIndex:idx];
  else item =[menu itemWithTag:idx];
  if (!item) 
  {
    if (!(en & MF_BYPOSITION))
    {
      const int n=(int)[menu numberOfItems];
      for (int x = 0; x < n; x ++)
      {
        item=[menu itemAtIndex:x];
        if (item && [item hasSubmenu])
        {
          NSMenu *m=[item submenu];
          if (m && EnableMenuItem((HMENU)m,idx,en)) return true;
        }
      }
    }
    return false;
  }
  [item setEnabled:((en&MF_GRAYED)?NO:YES)];
  return true;
}

bool CheckMenuItem(HMENU hMenu, int idx, int chk)
{
  NSMenu *menu=(NSMenu *)hMenu;
  if (!menu) return false;
  
  NSMenuItem *item;
  if (chk & MF_BYPOSITION) item=[menu itemAtIndex:idx];
  else item =[menu itemWithTag:idx];
  if (!item) 
  {
    if (!(chk & MF_BYPOSITION))
    {
      const int n=(int)[menu numberOfItems];
      for (int x = 0; x < n; x ++)
      {
        item=[menu itemAtIndex:x];
        if (item && [item hasSubmenu])
        {
          NSMenu *m=[item submenu];
          if (m && CheckMenuItem((HMENU)m,idx,chk)) return true;
        }
      }
    }
    return false;  
  }
  [item setState:((chk&MF_CHECKED)?NSOnState:NSOffState)];
  
  return true;
}
HMENU SWELL_GetCurrentMenu()
{
  return (HMENU)[NSApp mainMenu];
}

extern int g_swell_terminating;

void SWELL_SetCurrentMenu(HMENU hmenu)
{
  if (hmenu && [(id)hmenu isKindOfClass:[NSMenu class]])
  {
    if (!g_swell_terminating) [NSApp setMainMenu:(NSMenu *)hmenu];
  }
}

HMENU GetSubMenu(HMENU hMenu, int pos)
{
  NSMenu *menu=(NSMenu *)hMenu;
  
  NSMenuItem *item=menu && pos >=0 && pos < [menu numberOfItems] ? [menu itemAtIndex:pos] : 0; 
  if (item && [item hasSubmenu]) return (HMENU)[item submenu];
  return 0;
}

int GetMenuItemCount(HMENU hMenu)
{
  NSMenu *menu=(NSMenu *)hMenu;
  return (int)[menu numberOfItems];
}

int GetMenuItemID(HMENU hMenu, int pos)
{
  NSMenu *menu=(NSMenu *)hMenu;
  if (pos < 0 || pos >= (int)[menu numberOfItems]) return 0;
  
  NSMenuItem *item=[menu itemAtIndex:pos]; 
  if (item) 
  {
    if ([item hasSubmenu]) return -1;
    return (int)[item tag];
  }
  return 0;
}

bool SetMenuItemModifier(HMENU hMenu, int idx, int flag, int code, unsigned int mask)
{

#if 0 // enable this once we make SWELL_KeyToASCII decent
  int n2=0;
  int n1 = SWELL_KeyToASCII(code,flag,&n2);
  if (n1)
  {
    code=n1;
    flag=n2;
  }
#endif
  
  NSMenu *menu=(NSMenu *)hMenu;
  
  NSMenuItem *item;
  if (flag & MF_BYPOSITION) item=[menu itemAtIndex:idx];
  else item =[menu itemWithTag:idx];
  if (!item) 
  {
    if (!(flag & MF_BYPOSITION))
    {
      const int n = (int)[menu numberOfItems];
      for (int x = 0; x < n; x ++)
      {
        item=[menu itemAtIndex:x];
        if (item && [item hasSubmenu])
        {
          NSMenu *m=[item submenu];
          if (m && SetMenuItemModifier((HMENU)m,idx,flag,code,mask)) return true;
        }
      }
    }
    return false;
  }
  
  bool suppressShift = false;
  unichar arrowKey = 0;
  int codelow = code&127;
  if ((code>='A' && code <='Z') ||
      (code>='0' && code <= '9') ||   
      ( !(mask&FVIRTKEY) && 
       ( 
         codelow == '\'' ||
         codelow == '/' ||
         codelow == '\\' ||
         codelow == '|' ||
         codelow == '"' || 
         codelow == ',' ||
         codelow == '.' || 
         codelow == '!' ||
         codelow == '?' ||
         codelow == '[' || 
         codelow == ']' 
        )))      
  {
    arrowKey=codelow;
    if (!(mask & FSHIFT) && arrowKey < 256) arrowKey=tolower(arrowKey);
    
    if (code>='A' && code<='Z') suppressShift=true;
  }
  else if (code >= VK_F1 && code <= VK_F24)
  {
    arrowKey = NSF1FunctionKey + code - VK_F1;
  }
  else switch (code&0xff)
  {
    #define DEFKP(wink,mack) case wink: arrowKey = mack; break;
    DEFKP(VK_UP,NSUpArrowFunctionKey)
    DEFKP(VK_DOWN,NSDownArrowFunctionKey)
    DEFKP(VK_LEFT,NSLeftArrowFunctionKey)
    DEFKP(VK_RIGHT,NSRightArrowFunctionKey)
    DEFKP(VK_INSERT,NSInsertFunctionKey)
    DEFKP(VK_DELETE,NSDeleteCharacter)
    DEFKP(VK_BACK,NSBackspaceCharacter) 
    DEFKP(VK_HOME,NSHomeFunctionKey)
    DEFKP(VK_END,NSEndFunctionKey)
    DEFKP(VK_NEXT,NSPageDownFunctionKey)
    DEFKP(VK_PRIOR,NSPageUpFunctionKey)
    DEFKP(VK_SUBTRACT,'-')
  }
   
  unsigned int mask2=0;
  if (mask&FALT) mask2|=NSAlternateKeyMask;
  if (!suppressShift) if (mask&FSHIFT) mask2|=NSShiftKeyMask;
  if (mask&FCONTROL) mask2|=NSCommandKeyMask;
  if (mask&FLWIN) mask2|=NSControlKeyMask;
     
  [item setKeyEquivalentModifierMask:mask2];
  [item setKeyEquivalent:arrowKey?[NSString stringWithCharacters:&arrowKey length:1]:@""];
  return true;
}

// #define SWELL_MENU_ACCOUNTING

#ifdef SWELL_MENU_ACCOUNTING
struct menuTmp
{
  NSMenu *menu;
  NSString *lbl;
};

WDL_PtrList<menuTmp> allMenus;
#endif

@implementation SWELL_Menu
- (id)copyWithZone:(NSZone *)zone
{
  id rv = [super copyWithZone:zone];
#ifdef SWELL_MENU_ACCOUNTING
  if (rv)
  {
    menuTmp *mt = new menuTmp;
    mt->menu=(NSMenu *)rv;
    NSString *lbl = [(SWELL_Menu *)rv title];
    mt->lbl = lbl;
    [lbl retain];
    allMenus.Add(mt);
    NSLog(@"copy menu, new count=%d lbl=%@\n",allMenus.GetSize(),lbl);
  }
#endif
  return rv;
}
-(void)dealloc
{
#ifdef SWELL_MENU_ACCOUNTING
  int x;
  bool f=false;
  for(x=0;x<allMenus.GetSize();x++)
  {
    if (allMenus.Get(x)->menu == self)
    {
      NSLog(@"dealloc menu, found self %@\n",allMenus.Get(x)->lbl);
      allMenus.Delete(x);
      f=true;
      break;
    }
  }

  NSLog(@"dealloc menu, new count=%d %@\n",allMenus.GetSize(), [self title]);
  if (!f) 
  {
    NSLog(@"deleting unfound menu!!\n");
  }
#endif
  [super dealloc];
}
@end

HMENU CreatePopupMenu()
{
  return CreatePopupMenuEx(NULL);
}
HMENU CreatePopupMenuEx(const char *title)
{
  SWELL_Menu *m;
  if (title)
  {
    char buf[1024];
    __filtnametobuf(buf,title,sizeof(buf));
    NSString *lbl=(NSString *)SWELL_CStringToCFString(buf);
    m=[[SWELL_Menu alloc] initWithTitle:lbl];
#ifdef SWELL_MENU_ACCOUNTING
    menuTmp *mt = new menuTmp;
    mt->menu=m;
    mt->lbl = lbl;
    [lbl retain];
    allMenus.Add(mt);
    NSLog(@"alloc menu, new count=%d lbl=%@\n",allMenus.GetSize(),lbl);
#endif
    [lbl release];
  }
  else
  {
    m=[[SWELL_Menu alloc] init];
#ifdef SWELL_MENU_ACCOUNTING
    menuTmp *mt = new menuTmp;
    mt->menu=m;
    mt->lbl = @"<none>";
    allMenus.Add(mt);
    NSLog(@"alloc menu, new count=%d lbl=%@\n",allMenus.GetSize(),@"<none>");
#endif
  }
  [m setAutoenablesItems:NO];

  return (HMENU)m;
}

void DestroyMenu(HMENU hMenu)
{
  if (hMenu)
  {
    SWELL_SetMenuDestination(hMenu,NULL);
    NSMenu *m=(NSMenu *)hMenu;
    [m release];
  }
}



int AddMenuItem(HMENU hMenu, int pos, const char *name, int tagid)
{
  if (!hMenu) return -1;
  NSMenu *m=(NSMenu *)hMenu;
  NSString *label=(NSString *)SWELL_CStringToCFString(name); 
  NSMenuItem *item=[m insertItemWithTitle:label action:NULL keyEquivalent:@"" atIndex:pos];
  [label release];
  [item setTag:tagid];
  [item setEnabled:YES];
  return 0;
}

bool DeleteMenu(HMENU hMenu, int idx, int flag)
{
  if (!hMenu) return false;
  NSMenu *m=(NSMenu *)hMenu;
  NSMenuItem *item=NULL;
  
  if (flag&MF_BYPOSITION)
  {
    if (idx >=0 && idx < [m numberOfItems])
      item=[m itemAtIndex:idx];
    if (!item) return false;
  }
  else
  {
    item=[m itemWithTag:idx];
    if (!item) 
    {
      const int n = (int) [m numberOfItems];
      for (int x=0;x<n;x++)
      {
        item=[m itemAtIndex:x];
        if (item && [item hasSubmenu])
        {
          if (DeleteMenu((HMENU)[item submenu],idx,flag)) return true;
        }
      }
      return false;
    }
  }
  
  if ([item hasSubmenu])
  {
    HMENU sm = (HMENU)[item submenu];
    if (sm) SWELL_SetMenuDestination(sm,NULL);
    [m setSubmenu:nil forItem:item];
  }
  [m removeItem:item];
  return true;
}


BOOL SetMenuItemInfo(HMENU hMenu, int pos, BOOL byPos, MENUITEMINFO *mi)
{
  if (!hMenu) return 0;
  NSMenu *m=(NSMenu *)hMenu;
  NSMenuItem *item;
  if (byPos) item=[m itemAtIndex:pos];
  else item=[m itemWithTag:pos];

  if (!item) 
  {
    if (!byPos)
    {
      const int n = (int)[m numberOfItems];
      for (int x = 0; x < n; x ++)
      {
        item=[m itemAtIndex:x];
        if (item && [item hasSubmenu])
        {
          NSMenu *m1=[item submenu];
          if (m1 && SetMenuItemInfo((HMENU)m1,pos,byPos,mi)) return true;
        }
      }      
    }
    return 0;
  }
  
  if (mi->fMask & MIIM_TYPE)
  {
    if (mi->fType == MFT_STRING && mi->dwTypeData)
    {
      char buf[1024];
      __filtnametobuf(buf,mi->dwTypeData?mi->dwTypeData:"(null)",sizeof(buf));
      NSString *label=(NSString *)SWELL_CStringToCFString(buf); 
      
      [item setTitle:label];

      if ([item hasSubmenu])
      {
        NSMenu *subm=[item submenu];
        if (subm) [subm setTitle:label];
      }
      
      [label release];      
    }
  }
  if (mi->fMask & MIIM_SUBMENU) 
  {
    NSMenu *oldMenu = [item hasSubmenu] ? [item submenu] : NULL;
    NSMenu *newMenu = (NSMenu*)mi->hSubMenu;
    if (oldMenu != newMenu)
    {
      if (oldMenu) [oldMenu retain]; // we do not destroy the old menu, caller responsibility

      if (newMenu) [newMenu setTitle:[item title]];
      [m setSubmenu:newMenu forItem:item];
      if (newMenu) [newMenu release]; // let the parent menu free it
    }
  }

  if (mi->fMask & MIIM_STATE)
  {
    [item setState:((mi->fState&MFS_CHECKED)?NSOnState:NSOffState)];
    [item setEnabled:((mi->fState&MFS_GRAYED)?NO:YES)];
  }
  if (mi->fMask & MIIM_ID)
  {
    [item setTag:mi->wID];
  }
  if (mi->fMask & MIIM_DATA)
  {
    SWELL_DataHold* newh = [[SWELL_DataHold alloc] initWithVal:(void*)mi->dwItemData];
    [item setRepresentedObject:newh]; 
    [newh release];    
  }
  
  return true;
}

BOOL GetMenuItemInfo(HMENU hMenu, int pos, BOOL byPos, MENUITEMINFO *mi)
{
  if (!hMenu) return 0;
  NSMenu *m=(NSMenu *)hMenu;
  NSMenuItem *item;
  if (byPos)
  {
    item=[m itemAtIndex:pos];
  }
  else item=[m itemWithTag:pos];
  
  if (!item) 
  {
    if (!byPos)
    {
      const int n = (int)[m numberOfItems];
      for (int x = 0; x < n; x ++)
      {
        item=[m itemAtIndex:x];
        if (item && [item hasSubmenu])
        {
          NSMenu *m1=[item submenu];
          if (m1 && GetMenuItemInfo((HMENU)m1,pos,byPos,mi)) return true;
        }
      }      
    }
    return 0;
  }
  
  if (mi->fMask & MIIM_TYPE)
  {
    if ([item isSeparatorItem]) mi->fType = MFT_SEPARATOR;
    else
    {
      mi->fType = MFT_STRING;
      if (mi->dwTypeData && mi->cch)
      {
        mi->dwTypeData[0]=0;
        SWELL_CFStringToCString([item title], (char *)mi->dwTypeData, mi->cch);
      }
    }
  }
  
  if (mi->fMask & MIIM_DATA)
  {
    SWELL_DataHold *h=[item representedObject];
    mi->dwItemData =  (INT_PTR)(h && [h isKindOfClass:[SWELL_DataHold class]]? [h getValue] : 0);
  }
  
  if (mi->fMask & MIIM_STATE)
  {
    mi->fState=0;
    if ([item state]) mi->fState|=MFS_CHECKED;
    if (![item isEnabled]) mi->fState|=MFS_GRAYED;
  }
  
  if (mi->fMask & MIIM_ID)
  {
    mi->wID = (unsigned int)[item tag];
  }
  
  if(mi->fMask & MIIM_SUBMENU)
  {
    mi->hSubMenu = (HMENU) ([item hasSubmenu] ? [item submenu] : NULL);
  }
  
  return 1;
  
}

void SWELL_InsertMenu(HMENU menu, int pos, unsigned int flag, UINT_PTR idx, const char *str)
{
  MENUITEMINFO mi={sizeof(mi),MIIM_ID|MIIM_STATE|MIIM_TYPE,MFT_STRING,
    (flag & ~MF_BYPOSITION),(flag&MF_POPUP) ? 0 : (unsigned int)idx,NULL,NULL,NULL,0,(char *)str};
  
  if (flag&MF_POPUP) 
  {
    mi.hSubMenu = (HMENU)idx;
    mi.fMask |= MIIM_SUBMENU;
    mi.fState &= ~MF_POPUP;
  }
  
  if (flag&MF_SEPARATOR)
  {
    mi.fMask=MIIM_TYPE;
    mi.fType=MFT_SEPARATOR;
    mi.fState &= ~MF_SEPARATOR;
  }
  
  if (flag&MF_BITMAP)
  {
    mi.fType=MFT_BITMAP;
    mi.fState &= ~MF_BITMAP;
  }
    
  InsertMenuItem(menu,pos,(flag&MF_BYPOSITION) ?  TRUE : FALSE, &mi);
}


void InsertMenuItem(HMENU hMenu, int pos, BOOL byPos, MENUITEMINFO *mi)
{
  if (!hMenu) return;
  NSMenu *m=(NSMenu *)hMenu;
  NSMenuItem *item;
  int ni = (int)[m numberOfItems];
  
  if (!byPos) 
  {
    pos = (int)[m indexOfItemWithTag:pos];
  }
  if (pos < 0 || pos > ni) pos=ni; 
  
  NSString *label=0;
  if (mi->fType == MFT_STRING)
  {
    char buf[1024];
    __filtnametobuf(buf,mi->dwTypeData?mi->dwTypeData:"(null)",sizeof(buf));
    label=(NSString *)SWELL_CStringToCFString(buf); 
    item=[m insertItemWithTitle:label action:NULL keyEquivalent:@"" atIndex:pos];
  }
  else if (mi->fType == MFT_BITMAP)
  {
    item=[m insertItemWithTitle:@"(no image)" action:NULL keyEquivalent:@"" atIndex:pos];
    if (mi->dwTypeData)
    {
      NSImage *i=(NSImage *)GetNSImageFromHICON((HICON)mi->dwTypeData);
      if (i)
      {
        [item setImage:i];
        [item setTitle:@""];
      }
    }
  }
  else
  {
    item = [NSMenuItem separatorItem];
    [m insertItem:item atIndex:pos];
  }
  
  if ((mi->fMask & MIIM_SUBMENU) && mi->hSubMenu)
  {
    if (label) [(NSMenu *)mi->hSubMenu setTitle:label];
    [m setSubmenu:(NSMenu *)mi->hSubMenu forItem:item];
    [((NSMenu *)mi->hSubMenu) release]; // let the parent menu free it
  }
  if (label) [label release];
  
  if (!ni) [m setAutoenablesItems:NO];
  [item setEnabled:YES];
  
  if (mi->fMask & MIIM_STATE)
  {
    if (mi->fState&MFS_GRAYED)
    {
      [item setEnabled:NO];
    }
    if (mi->fState&MFS_CHECKED)
    {
      [item setState:NSOnState];
    }
  }
 
   if (mi->fMask & MIIM_DATA)
  {
    SWELL_DataHold *h=[[SWELL_DataHold alloc] initWithVal:(void*)mi->dwItemData];
    [item setRepresentedObject:h];
    [h release];
  }
  else
  {
    [item setRepresentedObject:nil];
  }
 
  if (mi->fMask & MIIM_ID)
  {
    [item setTag:mi->wID];
  }
  
  int i;
  ni = (int)[m numberOfItems];
  // try to find a valid action/target
  for (i = 0; i < ni; i ++)
  {
    NSMenuItem *fi=[m itemAtIndex:i];
    if (fi && fi != item)
    {
      SEL act = [fi action];
      id tgt = [fi target];
      if (act || tgt)
      {
        if (act) [item setAction:act];
        if (tgt) [item setTarget:tgt];
        break;
      }
    }
    if (i == 5 && ni > 14) i = ni-6; // only look at first and last 6 items or so
  }
}



@implementation SWELL_PopupMenuRecv
-(id) initWithWnd:(HWND)wnd
{
  if ((self = [super init]))
  {
    cbwnd=wnd;
    m_act=0;
  }
  return self;
}

-(void) onSwellCommand:(id)sender
{
  int tag=(int) [sender tag];
  if (tag)
    m_act=tag;
}

-(int) isCommand
{
  return m_act;
}

- (void)menuNeedsUpdate:(NSMenu *)menu
{
  if (cbwnd)
  {
    SendMessage(cbwnd,WM_INITMENUPOPUP,(WPARAM)menu,0);
    SWELL_SetMenuDestination((HMENU)menu,(HWND)self);
  }
}

@end

static void SWELL_SetMenuDestinationInt(NSMenu *m, HWND hwnd, bool is_top_level, bool do_skip_sub)
{
  [m setDelegate:(id)hwnd];
  const int n = (int)[m numberOfItems];
  for (int x = 0; x < n; x++)
  {
    NSMenuItem *item=[m itemAtIndex:x];
    if (item)
    {
      if ([item hasSubmenu])
      {
        NSMenu *mm=[item submenu];
        if (mm)
        {
          if (do_skip_sub) 
          {
            id del = [mm delegate];
            NSString *cn = del ? [del className] : NULL;
            if (cn)
            {
              char buf[1024];
              SWELL_CFStringToCString(cn, buf, sizeof(buf));
              if (strstr(buf,"NSServices")) continue;
            }
          }
          SWELL_SetMenuDestinationInt(mm,hwnd,false, is_top_level && !x);
        }
      }
      else
      {
        if ([item tag])
        {
          [item setTarget:(id)hwnd];
          if (hwnd) [item setAction:@selector(onSwellCommand:)];
        }
      }
    }
  }
}

void SWELL_SetMenuDestination(HMENU menu, HWND hwnd)
{
  if (!menu || (hwnd && ![(id)hwnd respondsToSelector:@selector(onSwellCommand:)])) return;

  NSMenu *m = (NSMenu *)menu, *par = [m supermenu];

  bool do_skip_sub = false;
  if (par && ![par supermenu] && [par numberOfItems]>0)
  {
    NSMenuItem *item = [par itemAtIndex:0];
    do_skip_sub = item && [item hasSubmenu] && [item submenu] == m;
  }
  SWELL_SetMenuDestinationInt(m,hwnd,!par,do_skip_sub);
}

int TrackPopupMenu(HMENU hMenu, int flags, int xpos, int ypos, int resvd, HWND hwnd, const RECT *r)
{
  ReleaseCapture(); // match win32 -- TrackPopupMenu() ends any captures
  if (hMenu)
  {
    NSMenu *m=(NSMenu *)hMenu;
    NSView *v=(NSView *)hwnd;
    if (v && [v isKindOfClass:[NSWindow class]]) v=[(NSWindow *)v contentView];
    if (!v) v=[[NSApp mainWindow] contentView];
    if (!v) return 0;
    
    NSEvent *event = [NSApp currentEvent];
       
    {      
      //create a new event at these coordinates, faking it
      NSWindow *w = [v window];
      NSPoint pt = NSMakePoint(xpos, ypos);
      pt=[w convertScreenToBase:pt];
      pt.y -= 4;
      NSInteger wn = [w windowNumber]; // event ? [event windowNumber] : [w windowNumber];
      NSTimeInterval ts = event ? [event timestamp] : 0;
      NSGraphicsContext *gctx = event ? [event context] : 0;
      event = [NSEvent otherEventWithType:NSApplicationDefined location:pt modifierFlags:0 timestamp:ts windowNumber:wn context:gctx subtype:0 data1:0 data2:0];
    }
    
    SWELL_PopupMenuRecv *recv = [[SWELL_PopupMenuRecv alloc] initWithWnd:((flags & TPM_NONOTIFY) ? 0 : hwnd)];
    
    SWELL_SetMenuDestination((HMENU)m,(HWND)recv);
    
    if (!(flags&TPM_NONOTIFY)&&hwnd)
    {
      SendMessage(hwnd,WM_INITMENUPOPUP,(WPARAM)m,0);
      SWELL_SetMenuDestination((HMENU)m,(HWND)recv);
    }
    
    [NSMenu popUpContextMenu:m withEvent:event forView:v];

    int ret=[recv isCommand];    
    SWELL_SetMenuDestination((HMENU)m,(HWND)NULL);
    [recv release];
    
    if (ret<=0) return 0;
    
    if (flags & TPM_RETURNCMD) return ret;
    
    if (hwnd) SendMessage(hwnd,WM_COMMAND,ret,0);
    
    return 1;
  }
  return 0;
}




void SWELL_Menu_AddMenuItem(HMENU hMenu, const char *name, int idx, unsigned int flags)
{
  MENUITEMINFO mi={sizeof(mi),MIIM_ID|MIIM_STATE|MIIM_TYPE,MFT_STRING,
    (unsigned int) ((flags)?MFS_GRAYED:0),(unsigned int)idx,NULL,NULL,NULL,0,(char *)name};
  if (!name)
  {
    mi.fType = MFT_SEPARATOR;
    mi.fMask&=~(MIIM_STATE|MIIM_ID);
  }
  InsertMenuItem(hMenu,GetMenuItemCount(hMenu),TRUE,&mi);
}

int SWELL_GenerateMenuFromList(HMENU hMenu, const void *_list, int listsz)
{
  SWELL_MenuGen_Entry *list = (SWELL_MenuGen_Entry *)_list;
  const int l1=strlen(SWELL_MENUGEN_POPUP_PREFIX);
  while (listsz>0)
  {
    int cnt=1;
    if (!list->name) SWELL_Menu_AddMenuItem(hMenu,NULL,-1,0);
    else if (!strcmp(list->name,SWELL_MENUGEN_ENDPOPUP)) break;
    else if (!strncmp(list->name,SWELL_MENUGEN_POPUP_PREFIX,l1)) 
    { 
      MENUITEMINFO mi={sizeof(mi),MIIM_SUBMENU|MIIM_STATE|MIIM_TYPE,MFT_STRING,0,0,CreatePopupMenuEx(list->name+l1),NULL,NULL,0,(char *)list->name+l1};
      cnt += SWELL_GenerateMenuFromList(mi.hSubMenu,list+1,listsz-1);
      InsertMenuItem(hMenu,GetMenuItemCount(hMenu),TRUE,&mi);
    }
    else SWELL_Menu_AddMenuItem(hMenu,list->name,list->idx,list->flags);

    list+=cnt;
    listsz -= cnt;
  }
  return (int) (list + 1 - (SWELL_MenuGen_Entry *)_list);
}


SWELL_MenuResourceIndex *SWELL_curmodule_menuresource_head; // todo: move to per-module thingy

static SWELL_MenuResourceIndex *resById(SWELL_MenuResourceIndex *head, const char *resid)
{
  SWELL_MenuResourceIndex *p=head;
  while (p)
  {
    if (p->resid == resid) return p;
    p=p->_next;
  }
  return 0;
}

HMENU SWELL_LoadMenu(SWELL_MenuResourceIndex *head, const char *resid)
{
  SWELL_MenuResourceIndex *p;
  
  if (!(p=resById(head,resid))) return 0;
  HMENU hMenu=CreatePopupMenu();
  if (hMenu) p->createFunc(hMenu);
  return hMenu;
}

HMENU SWELL_DuplicateMenu(HMENU menu)
{
  if (!menu) return 0;
  NSMenu *ret = (NSMenu *)[(NSMenu *)menu copy];
  return (HMENU)ret;
}

BOOL  SetMenu(HWND hwnd, HMENU menu)
{
  if (!hwnd||![(id)hwnd respondsToSelector:@selector(swellSetMenu:)]) return FALSE;
  if (g_swell_terminating)  return FALSE;

  [(id)hwnd swellSetMenu:(HMENU)menu];
  NSWindow *nswnd = (NSWindow *)hwnd;
  if ([nswnd isKindOfClass:[NSWindow class]] || 
     ([nswnd isKindOfClass:[NSView class]] && (nswnd=[(NSView *)nswnd window]) && hwnd == (HWND)[nswnd contentView]))
  {
    if ([NSApp keyWindow]==nswnd &&
        [NSApp mainMenu] != (NSMenu *)menu)
    {
      [NSApp setMainMenu:(NSMenu *)menu];
      if (menu) SendMessage(hwnd,WM_INITMENUPOPUP,(WPARAM)menu,0); // find a better place for this! TODO !!!
    }
  }
  
  return TRUE;
}

HMENU GetMenu(HWND hwnd)
{
  if (!hwnd) return NULL;
  if ([(id)hwnd isKindOfClass:[NSWindow class]]) hwnd = (HWND)[(NSWindow *)hwnd contentView];
  if ([(id)hwnd respondsToSelector:@selector(swellGetMenu)]) return (HMENU) [(id)hwnd swellGetMenu];
  return NULL;
}

void DrawMenuBar(HWND hwnd)
{
}


#endif
