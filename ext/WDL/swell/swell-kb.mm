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
  

    This file provides basic key and mouse cursor querying, as well as a mac key to windows key translation function.

  */

#ifndef SWELL_PROVIDED_BY_APP

#include "swell.h"
#include "swell-dlggen.h"
#include "../wdltypes.h"
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>



static int MacKeyCodeToVK(int code)
{
  switch (code)
  {
    case 51: return VK_BACK;
    case 65: return VK_DECIMAL;
    case 67: return VK_MULTIPLY;
    case 69: return VK_ADD;
    case 71: return VK_NUMLOCK;
    case 75: return VK_DIVIDE;
    case 76: return VK_RETURN|0x8000;
    case 78: return VK_SUBTRACT;
    case 81: return VK_SEPARATOR;
    case 82: return VK_NUMPAD0;
    case 83: return VK_NUMPAD1;
    case 84: return VK_NUMPAD2;
    case 85: return VK_NUMPAD3;
    case 86: return VK_NUMPAD4;
    case 87: return VK_NUMPAD5;
    case 88: return VK_NUMPAD6;
    case 89: return VK_NUMPAD7;
    case 91: return VK_NUMPAD8;
    case 92: return VK_NUMPAD9;
    case 96: return VK_F5;
    case 97: return VK_F6;
    case 98: return VK_F7;
    case 99: return VK_F3;
    case 100: return VK_F8;
    case 101: return VK_F9;
    case 109: return VK_F10;
    case 103: return VK_F11;
    case 111: return VK_F12;
    case 114: return VK_INSERT;
    case 115: return VK_HOME;
    case 117: return VK_DELETE;
    case 116: return VK_PRIOR;
    case 118: return VK_F4;
    case 119: return VK_END;
    case 120: return VK_F2;
    case 121: return VK_NEXT;
    case 122: return VK_F1;
    case 123: return VK_LEFT;
    case 124: return VK_RIGHT;
    case 125: return VK_DOWN;
    case 126: return VK_UP;
    case 0x69: return VK_F13;
    case 0x6B: return VK_F14;
    case 0x71: return VK_F15;
    case 0x6A: return VK_F16;
  }
  return 0;
}

bool IsRightClickEmulateEnabled();

#ifdef MAC_OS_X_VERSION_10_5

static int charFromVcode(int keyCode) // used for getting the root char (^, `) from dead keys on other keyboards,
                                       // only used when using MacKeyToWindowsKeyEx() with mode=1, for now 
{
  static char loaded;
  static TISInputSourceRef (*_TISCopyCurrentKeyboardInputSource)( void);
  static void* (*_TISGetInputSourceProperty) ( TISInputSourceRef inputSource, CFStringRef propertyKey);

  if (!loaded)
  {
    loaded++;
    CFBundleRef b = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.Carbon"));
    if (b)
    {
      *(void **)&_TISGetInputSourceProperty = CFBundleGetFunctionPointerForName(b,CFSTR("TISGetInputSourceProperty"));
      *(void **)&_TISCopyCurrentKeyboardInputSource = CFBundleGetFunctionPointerForName(b,CFSTR("TISCopyCurrentKeyboardInputSource"));
    }
  }
  if (!_TISCopyCurrentKeyboardInputSource || !_TISGetInputSourceProperty) return 0;
  
  TISInputSourceRef currentKeyboard = _TISCopyCurrentKeyboardInputSource();
  CFDataRef uchr = (CFDataRef)_TISGetInputSourceProperty(currentKeyboard, CFSTR("TISPropertyUnicodeKeyLayoutData"));
  const UCKeyboardLayout *keyboardLayout = (const UCKeyboardLayout*)CFDataGetBytePtr(uchr);

  if(keyboardLayout)
  {
    UInt32 deadKeyState = 0;
    UniCharCount maxStringLength = 255;
    UniCharCount actualStringLength = 0;
    UniChar unicodeString[maxStringLength];

    OSStatus status = UCKeyTranslate(keyboardLayout,
                                     keyCode, kUCKeyActionDown, 0,
                                     LMGetKbdType(), 0,
                                     &deadKeyState,
                                     maxStringLength,
                                     &actualStringLength, unicodeString);

    if (actualStringLength == 0 && deadKeyState)
    {
        status = UCKeyTranslate(keyboardLayout,
                                         kVK_Space, kUCKeyActionDown, 0,
                                         LMGetKbdType(), 0,
                                         &deadKeyState,
                                         maxStringLength,
                                         &actualStringLength, unicodeString);   
    }
    if(actualStringLength > 0 && status == noErr) return unicodeString[0]; 
  }
  return 0;
}
#endif

int SWELL_MacKeyToWindowsKeyEx(void *nsevent, int *flags, int mode)
{
  NSEvent *theEvent = (NSEvent *)nsevent;
  if (!theEvent) theEvent = [NSApp currentEvent];

  const NSInteger mod=[theEvent modifierFlags];
    
  int flag=0;
  if (mod & NSShiftKeyMask) flag|=FSHIFT;
  if (mod & NSCommandKeyMask) flag|=FCONTROL; // todo: this should be command once we figure it out
  if (mod & NSAlternateKeyMask) flag|=FALT;
  if ((mod&NSControlKeyMask) && !IsRightClickEmulateEnabled()) flag|=FLWIN;
    
  int rawcode=[theEvent keyCode];

  int code=MacKeyCodeToVK(rawcode);
  if (!code)
  {
    NSString *str=NULL;
    if (mode == 1) str=[theEvent characters];

    if (!str || ![str length]) str=[theEvent charactersIgnoringModifiers];

    if (!str || ![str length]) 
    {
    #ifdef MAC_OS_X_VERSION_10_5
      if (mode==1) code=charFromVcode(rawcode);
    #endif
      if (!code)
      {
        code = 1024+rawcode; // raw code
        flag|=FVIRTKEY;
      }
    }
    else
    {
      code=[str characterAtIndex:0];
      if (code >= NSF1FunctionKey && code <= NSF24FunctionKey)
      {
        flag|=FVIRTKEY;
        code += VK_F1 - NSF1FunctionKey;
      }
      else 
      {
        if (code >= 'a' && code <= 'z') code+='A'-'a';
        if (code == 25 && (flag&FSHIFT)) code=VK_TAB;
        if (isalnum(code)||code==' ' || code == '\r' || code == '\n' || code ==27 || code == VK_TAB) flag|=FVIRTKEY;
      }
    }
  }
  else
  {
    flag|=FVIRTKEY;
    if (code==8) code='\b';
  }

  if (!(flag&FVIRTKEY)) flag&=~FSHIFT;
  
  if (flags) *flags=flag;
  return code;
}

int SWELL_MacKeyToWindowsKey(void *nsevent, int *flags)
{
  if (!nsevent) return 0;
  return SWELL_MacKeyToWindowsKeyEx(nsevent,flags,0);
}

int SWELL_KeyToASCII(int wParam, int lParam, int *newflags)
{
  if (wParam >= '0' && wParam <= '9' && lParam == (FSHIFT|FVIRTKEY))
  {
    *newflags = lParam&~(FSHIFT|FVIRTKEY);
    if (!(lParam & (FCONTROL|FLWIN))) switch (wParam) 
    {
      case '1': return '!';
      case '2': return '@';
      case '3': return '#';
      case '4': return '$';
      case '5': return '%';
      case '6': return '^';
      case '7': return '&';
      case '8': return '*';
      case '9': return '(';
      case '0': return ')';      
    }
  }
  return 0;
}


WORD GetAsyncKeyState(int key)
{
  CGEventFlags state=0;
  if (key == VK_LBUTTON || key == VK_RBUTTON || key == VK_MBUTTON)
  {
    state=GetCurrentEventButtonState();
  }
  else    
  {
    state=CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState);
  }

  if ((key == VK_LBUTTON && (state&1)) ||
      (key == VK_RBUTTON && (state&2)) ||
      (key == VK_MBUTTON && (state&4)) ||      
      (key == VK_SHIFT && (state&kCGEventFlagMaskShift)) ||
      (key == VK_CONTROL && (state&kCGEventFlagMaskCommand)) ||
      (key == VK_MENU && (state&kCGEventFlagMaskAlternate)) ||
      (key == VK_LWIN && !IsRightClickEmulateEnabled() && (state&kCGEventFlagMaskControl)))
  {
    return 0x8000;
  }
  
  return 0;
}


static SWELL_CursorResourceIndex *SWELL_curmodule_cursorresource_head;

static NSCursor* MakeCursorFromData(unsigned char* data, int hotspot_x, int hotspot_y)
{
  NSCursor *c=NULL;
  NSBitmapImageRep* bmp = [[NSBitmapImageRep alloc] 
    initWithBitmapDataPlanes:0
    pixelsWide:16
    pixelsHigh:16
    bitsPerSample:8
    samplesPerPixel:2  
    hasAlpha:YES
    isPlanar:NO 
    colorSpaceName:NSCalibratedWhiteColorSpace
    bytesPerRow:(16*2)
    bitsPerPixel:16]; 
  
  if (bmp)
  {
    unsigned char* p = [bmp bitmapData];
    if (p)
    {  
      int i;
      for (i = 0; i < 16*16; ++i)
      {
        // tried 4 bits per sample and memcpy, didn't work
        p[2*i] = (data[i]&0xF0) | data[i]>>4;
        p[2*i+1] = (data[i]<<4) | (data[i]&0xf);
      }
  
      NSImage *img = [[NSImage alloc] init];
      if (img)
      {
        [img addRepresentation:bmp];  
        NSPoint hs = NSMakePoint(hotspot_x, hotspot_y);
        c = [[NSCursor alloc] initWithImage:img hotSpot:hs];
        [img release];
      }   
    }
    [bmp release];
  }
  return c;
}

static NSCursor* MakeSWELLSystemCursor(const char *id)
{
  // bytemaps are (white<<4)|(alpha)
  const unsigned char B = 0xF;
  const unsigned char W = 0xFF;
  const unsigned char G = 0xF8;
  
  static NSCursor* carr[4] = { 0, 0, 0, 0 };
  
  NSCursor** pc=0;
  if (id == IDC_SIZEALL) pc = &carr[0];
  else if (id == IDC_SIZENWSE) pc = &carr[1];
  else if (id == IDC_SIZENESW) pc = &carr[2];
  else if (id == IDC_NO) pc = &carr[3];
  else return 0;
  
  if (!(*pc))
  {
    if (id == IDC_SIZEALL)
    {
      static unsigned char p[16*16] = 
      {
        0, 0, 0, 0, 0, 0, G, W, W, G, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, G, W, B, B, W, G, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, W, B, B, B, B, W, 0, 0, 0, 0, 0,
        0, 0, 0, 0, G, B, B, B, B, B, B, G, 0, 0, 0, 0,
        0, 0, 0, G, 0, 0, W, B, B, W, 0, 0, G, 0, 0, 0,
        0, G, W, B, 0, 0, W, B, B, W, 0, 0, B, W, G, 0,
        G, W, B, B, W, W, W, B, B, W, W, W, B, B, W, G,
        W, B, B, B, B, B, B, B, B, B, B, B, B, B, B, W,
        W, B, B, B, B, B, B, B, B, B, B, B, B, B, B, W,
        G, W, B, B, W, W, W, B, B, W, W, W, B, B, W, G,
        0, G, W, B, 0, 0, W, B, B, W, 0, 0, B, W, G, 0,
        0, 0, 0, G, 0, 0, W, B, B, W, 0, 0, G, 0, 0, 0,
        0, 0, 0, 0, G, B, B, B, B, B, B, G, 0, 0, 0, 0,
        0, 0, 0, 0, 0, W, B, B, B, B, W, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, G, W, B, B, W, G, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, G, W, W, G, 0, 0, 0, 0, 0, 0,
      };
      *pc = MakeCursorFromData(p, 8, 8);
    }
    else if (id == IDC_SIZENWSE || id == IDC_SIZENESW)
    {
      static unsigned char p[16*16] = 
      {
        W, W, W, W, W, W, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        W, G, G, G, W, G, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        W, G, B, W, G, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        W, G, W, B, W, G, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      
        W, W, G, W, B, W, G, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        W, G, 0, G, W, B, W, G, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, G, W, B, W, G, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, G, W, B, W, G, 0, 0, 0, 0, 0, 0,         
        0, 0, 0, 0, 0, 0, G, W, B, W, G, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, G, W, B, W, G, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, G, W, B, W, G, 0, G, W, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, G, W, B, W, G, W, W, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, G, W, B, W, G, W, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, G, W, B, G, W, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, G, W, G, G, G, W, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, W, W, W, W, W, W,         
      };
      if (id == IDC_SIZENESW)
      {
        int x, y;
        for (y = 0; y < 16; ++y) 
        {
          for (x = 0; x < 8; ++x)
          {
            unsigned char tmp = p[16*y+x];
            p[16*y+x] = p[16*y+16-x-1];
            p[16*y+16-x-1] = tmp;
          }
        }
      }
     *pc = MakeCursorFromData(p, 8, 8);   
      if (id == IDC_SIZENESW) // swap back!
      {
        int x, y;
        for (y = 0; y < 16; ++y) 
        {
          for (x = 0; x < 8; ++x)
          {
            unsigned char tmp = p[16*y+x];
            p[16*y+x] = p[16*y+16-x-1];
            p[16*y+16-x-1] = tmp;
          }
        }
      }      
    }
    else if (id == IDC_NO)
    {
      static unsigned char p[16*16] = 
      {
        0, 0, 0, 0, G, W, W, W, W, W, W, G, 0, 0, 0, 0,
        0, 0, G, W, W, B, B, B, B, B, B, W, W, G, 0, 0,
        0, G, W, B, B, B, W, W, W, W, B, B, B, W, G, 0,
        0, W, B, B, W, W, G, 0, 0, G, W, G, B, B, W, 0,        
        G, W, B, W, G, 0, 0, 0, 0, G, W, B, G, B, W, G,
        W, B, B, W, 0, 0, 0, 0, G, W, B, W, W, B, B, W,
        W, B, W, G, 0, 0, 0, G, W, B, W, G, G, W, B, W,
        W, B, W, 0, 0, 0, G, W, B, W, G, 0, 0, W, B, W,      
        W, B, W, 0, 0, G, W, B, W, G, 0, 0, 0, W, B, W,
        W, B, W, G, G, W, B, W, G, 0, 0, 0, G, W, B, W,
        W, B, B, W, W, B, W, G, 0, 0, 0, 0, W, B, B, W,
        G, W, B, G, B, W, G, 0, 0, 0, 0, G, W, B, W, G,        
        0, W, B, B, G, W, G, 0, 0, G, W, W, B, B, W, 0,
        0, G, W, B, B, B, W, W, W, W, B, B, B, W, G, 0,
        0, 0, G, W, W, B, B, B, B, B, B, W, W, G, 0, 0,
        0, 0, 0, 0, G, W, W, W, W, W, W, G, 0, 0, 0, 0,                                                                                                                                                                                                                                                                                                                                                                                                                                
      };
      *pc = MakeCursorFromData(p, 8, 8);        
    }
  }  
  
  return *pc;
}

static NSImage *swell_imageFromCursorString(const char *name, POINT *hotSpot)
{
  NSImage *img=NULL;
  FILE *fp = NULL;  
  bool isFullFn=false;
  
  if (!strstr(name,"/") && strlen(name)<1024)
  {
    char tmpn[4096];
    GetModuleFileName(NULL,tmpn,(DWORD)(sizeof(tmpn)-128-strlen(name)));
    strcat(tmpn,"/Contents/Resources/");
    strcat(tmpn,name);
    strcat(tmpn,".cur");
    fp = WDL_fopenA(tmpn,"rb");
  }
  else 
  {
    isFullFn=true;
    if (strlen(name)>4 && !stricmp(name+strlen(name)-4,".cur")) fp = WDL_fopenA(name,"rb");
  }  
  
  if (fp)
  {
    unsigned char buf[4096];
    if (fread(buf,1,6,fp)==6 && !buf[0] && !buf[1] && buf[2] == 2 && buf[3] == 0 && buf[4] == 1 && buf[5] == 0)
    {
      static char tempfn[512];
      if (!tempfn[0])
      {
        GetTempPath(256,tempfn);
        snprintf(tempfn+strlen(tempfn),256,"swellcur%x%x.ico", timeGetTime(),(int)getpid());
      }
      
      FILE *outfp = WDL_fopenA(tempfn,"wb");
      if (outfp)
      {
        bool wantLoad=false;
        buf[2]=1; // change to .ico
        fwrite(buf,1,6,outfp);
        
        fread(buf,1,16,fp);
        int xHot = buf[4]|(buf[5]<<8);
        int yHot = buf[6]|(buf[7]<<8);
        
        buf[4]=1; buf[5]=0; // 1 color plane
        buf[6]=0; buf[7]=0; // 0 for pixel depth means "auto"
        
        if (!buf[3])
        {
          fwrite(buf,1,16,outfp);
          for (;;)
          {
            size_t a = fread(buf,1,sizeof(buf),fp);
            if (a<1) break;
            fwrite(buf,1,a,outfp);
          }           
          wantLoad=true;
        }              
        
        fclose(outfp);
        if (wantLoad)
        {
          NSString *str = (NSString *)SWELL_CStringToCFString(tempfn);     
          img = [[NSImage alloc] initWithContentsOfFile:str];
          [str release];
          if (img && hotSpot) 
          {
            hotSpot->x = xHot;
            hotSpot->y = yHot;
          }
          //          printf("loaded converted ico for %s %s %d\n",tempfn,name,!!img);
        }
        unlink(tempfn);
      }      
      
    }
    
    fclose(fp);
  }
  
  if (!img) // fall back
  {
    NSString *str = (NSString *)SWELL_CStringToCFString(name);     
    
    if (isFullFn) img = [[NSImage alloc] initWithContentsOfFile:str];
    else
    {
      img = [NSImage imageNamed:str];
      if (img) [img retain];
    }
    [str release];
  }
  
  return img;
}

  
HCURSOR SWELL_LoadCursorFromFile(const char *fn)
{
  POINT hotspot={0,};
  NSImage *img = swell_imageFromCursorString(fn,&hotspot);    
  if (img)
  {      
    HCURSOR ret=(HCURSOR)[[NSCursor alloc] initWithImage:img hotSpot:NSMakePoint(hotspot.x,hotspot.y)];      
    [img release];
    return ret;
  }
  return NULL;
}
  
// todo: support for loading from file
HCURSOR SWELL_LoadCursor(const char *_idx)
{
  if (_idx == IDC_NO||_idx==IDC_SIZENWSE || _idx == IDC_SIZENESW || _idx == IDC_SIZEALL) return (HCURSOR) MakeSWELLSystemCursor(_idx);
  if (_idx == IDC_SIZEWE) return (HCURSOR)[NSCursor resizeLeftRightCursor];
  if (_idx == IDC_SIZENS) return (HCURSOR)[NSCursor resizeUpDownCursor];
  if (_idx == IDC_ARROW) return (HCURSOR)[NSCursor arrowCursor];
  if (_idx == IDC_HAND) return (HCURSOR)[NSCursor openHandCursor];
  if (_idx == IDC_UPARROW) return (HCURSOR)[NSCursor resizeUpCursor];
  if (_idx == IDC_IBEAM) return (HCURSOR)[NSCursor IBeamCursor];
  
  // search registered cursor list
  SWELL_CursorResourceIndex *p = SWELL_curmodule_cursorresource_head;
  while (p)
  {
    if (p->resid == _idx)
    {
      if (p->cachedCursor) return p->cachedCursor;
      
      NSImage *img = swell_imageFromCursorString(p->resname,&p->hotspot);    
      if (img)
      {      
        p->cachedCursor=(HCURSOR)[[NSCursor alloc] initWithImage:img hotSpot:NSMakePoint(p->hotspot.x,p->hotspot.y)];      
        [img release];
        return p->cachedCursor;
      }
    }
    p=p->_next;
  }
  return 0;
}

static HCURSOR m_last_setcursor;

void SWELL_SetCursor(HCURSOR curs)
{
  if (curs && [(id) curs isKindOfClass:[NSCursor class]])
  {
    m_last_setcursor=curs;
    [(NSCursor *)curs set];
  }
  else
  {
    m_last_setcursor=NULL;
    [[NSCursor arrowCursor] set];
  }
}

HCURSOR SWELL_GetCursor()
{
  return (HCURSOR)[NSCursor currentCursor];
}
HCURSOR SWELL_GetLastSetCursor()
{
  return m_last_setcursor;
}

static POINT g_swell_mouse_relmode_curpos; // stored in osx-native coordinates (y=0=top of screen)
static bool g_swell_mouse_relmode;



void GetCursorPos(POINT *pt)
{
  if (g_swell_mouse_relmode)
  {
    *pt=g_swell_mouse_relmode_curpos;
    return;
  }
  NSPoint localpt=[NSEvent mouseLocation];
  pt->x=(int)floor(localpt.x);
  pt->y=-(int)floor(-localpt.y); // floor() is used with negative sign, effectively ceil(), because screen coordinates are flipped and everywhere else we use nonflipped rounding
}

DWORD GetMessagePos()
{  
  if (g_swell_mouse_relmode)
  {
    return MAKELONG((int)g_swell_mouse_relmode_curpos.x,(int)g_swell_mouse_relmode_curpos.y);
  }
  NSPoint localpt=[NSEvent mouseLocation];
  return MAKELONG((int)floor(localpt.x), -(int)floor(-localpt.y)); // floor() is used with negative sign, effectively ceil(), because screen coordinates are flipped and everywhere else we use nonflipped rounding
}


NSPoint swellProcessMouseEvent(int msg, NSView *view, NSEvent *event)
{
  if (g_swell_mouse_relmode && msg==WM_MOUSEMOVE) // event will have relative coordinates
  {
    int idx=(int)[event deltaX];
    int idy=(int)[event deltaY];
    g_swell_mouse_relmode_curpos.x += idx;
    g_swell_mouse_relmode_curpos.y -= idy;
  }
  if (g_swell_mouse_relmode) 
  {
    POINT p=g_swell_mouse_relmode_curpos;
    ScreenToClient((HWND)view,&p);
    return NSMakePoint(p.x,p.y);
  }
  NSPoint localpt=[event locationInWindow];
  return [view convertPoint:localpt fromView:nil];
}

static int m_curvis_cnt;
bool SWELL_IsCursorVisible()
{
  return m_curvis_cnt>=0;
}
int SWELL_ShowCursor(BOOL bShow)
{
  m_curvis_cnt += (bShow?1:-1);
  if (m_curvis_cnt==-1 && !bShow) 
  {
    GetCursorPos(&g_swell_mouse_relmode_curpos);
    CGDisplayHideCursor(kCGDirectMainDisplay);
    CGAssociateMouseAndMouseCursorPosition(false);
    g_swell_mouse_relmode=true;
  }
  if (m_curvis_cnt==0 && bShow) 
  {
    CGDisplayShowCursor(kCGDirectMainDisplay);
    CGAssociateMouseAndMouseCursorPosition(true);
    g_swell_mouse_relmode=false;
    SetCursorPos(g_swell_mouse_relmode_curpos.x,g_swell_mouse_relmode_curpos.y);
  }  
  return m_curvis_cnt;
}


BOOL SWELL_SetCursorPos(int X, int Y)
{  
  if (g_swell_mouse_relmode)
  {
    g_swell_mouse_relmode_curpos.x=X;
    g_swell_mouse_relmode_curpos.y=Y;
    return TRUE;
  }

  const int h = (int)CGDisplayPixelsHigh(CGMainDisplayID());
  CGPoint pos=CGPointMake(X,h-Y);
  return CGWarpMouseCursorPosition(pos)==kCGErrorSuccess;
}

void SWELL_Register_Cursor_Resource(const char *idx, const char *name, int hotspot_x, int hotspot_y)
{
  SWELL_CursorResourceIndex *ri = (SWELL_CursorResourceIndex*)malloc(sizeof(SWELL_CursorResourceIndex));
  ri->hotspot.x = hotspot_x;
  ri->hotspot.y = hotspot_y;
  ri->resname=name;
  ri->cachedCursor=0;
  ri->resid = idx;
  ri->_next = SWELL_curmodule_cursorresource_head;
  SWELL_curmodule_cursorresource_head = ri;
}




#endif
