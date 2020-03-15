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
  

    This file provides basic APIs for browsing for files, directories, and messageboxes.

    These APIs don't all match the Windows equivelents, but are close enough to make it not too much trouble.

  */


#ifndef SWELL_PROVIDED_BY_APP

#include "swell.h"
#include "../wdlcstring.h"
#import <Cocoa/Cocoa.h>

struct swell_autoarp {
  swell_autoarp() { pool = [[NSAutoreleasePool alloc] init]; }
  ~swell_autoarp() { [pool release]; }
  NSAutoreleasePool *pool;
};

static NSMutableArray *extensionsFromList(const char *extlist, const char *def_ext=NULL)
{
	NSMutableArray *fileTypes = [[NSMutableArray alloc] initWithCapacity:30];
	while (*extlist)
	{
		extlist += strlen(extlist)+1;
		if (!*extlist) break; 
		while (*extlist)
		{
			while (*extlist && *extlist != '.') extlist++;
			if (!*extlist) break;
			extlist++;
			char tmp[32];
			lstrcpyn_safe(tmp,extlist,sizeof(tmp));
			if (strstr(tmp,";")) strstr(tmp,";")[0]=0;
			if (tmp[0] && tmp[0]!='*')
			{
				NSString *s=(NSString *)SWELL_CStringToCFString(tmp);
                                const size_t tmp_len = strlen(tmp);
                                if (def_ext && *def_ext &&
                                    !strnicmp(def_ext,tmp,tmp_len) &&
                                    (!def_ext[tmp_len] || def_ext[tmp_len] == ';'))
                                  [fileTypes insertObject:s atIndex:0];
                                else
  				  [fileTypes addObject:s];
				[s release];
			}
			while (*extlist && *extlist != ';') extlist++;
			if (*extlist == ';') extlist++;
		}
		extlist++;
	}
	
	return fileTypes;
}

static const char *BFSF_Templ_dlgid;
static DLGPROC BFSF_Templ_dlgproc;
static struct SWELL_DialogResourceIndex *BFSF_Templ_reshead;
void BrowseFile_SetTemplate(const char *dlgid, DLGPROC dlgProc, struct SWELL_DialogResourceIndex *reshead)
{
  BFSF_Templ_reshead=reshead;
  BFSF_Templ_dlgid=dlgid;
  BFSF_Templ_dlgproc=dlgProc;
}

static LRESULT fileTypeChooseProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  const int wndh = 22, lblw = 80, combow = 250, def_wid = lblw + 4 + combow + 4;
  switch (uMsg)
  {
    case WM_CREATE:
      SetOpaque(hwnd,FALSE);
      SetWindowPos(hwnd,NULL,0,0,def_wid,wndh,SWP_NOMOVE|SWP_NOZORDER);
      SWELL_MakeSetCurParms(1,1,0,0,hwnd,true,false);
      SWELL_MakeLabel(1,"File type:",1001,0,2,lblw,wndh,0);
      SWELL_MakeCombo(1000, lblw + 4,0, combow, wndh,3/*CBS_DROPDOWNLIST*/);
      SWELL_MakeSetCurParms(1,1,0,0,NULL,false,false);
      {
        const char *extlist = ((const char **)lParam)[0];
        SetWindowLongPtr(hwnd,GWLP_USERDATA,(LPARAM)extlist);
        const char *initial_file = ((const char **)lParam)[1];

        if (initial_file) initial_file=WDL_get_fileext(initial_file);
        const size_t initial_file_len = initial_file  && *initial_file ? strlen(++initial_file) : 0;

        int def_sel = -1;

        HWND combo = GetDlgItem(hwnd,1000);
        while (*extlist)
        {
          const char *next = extlist + strlen(extlist)+1;
          if (!*next) break;

          if (strcmp(next,"*.*"))
          {
            int a = (int)SendMessage(combo,CB_ADDSTRING,0,(LPARAM)extlist);

            // userdata for each item is pointer to un-terminated extension.
            const char *p = next;
            while (*p && *p != '.') p++;
            if (*p) p++;
            const char *bestp = p;

            // scan extension list for matching initial file, use that (and set default)
            if (def_sel < 0 && initial_file) while (*p)
            {
              if (!strnicmp(p,initial_file,initial_file_len) && (p[initial_file_len] == ';' || !p[initial_file_len])) 
              {
                bestp = p;
                def_sel = a;
                break;
              }
              else
              {
                while (*p && *p != '.') p++;
                if (*p) p++;
              }
            }
            SendMessage(combo,CB_SETITEMDATA,a,(LPARAM)bestp);
          }

          extlist = next + strlen(next)+1;
          if (!*extlist) break;
        }
        SendMessage(combo,CB_SETCURSEL,def_sel>=0?def_sel:0,0);
      }
    return 0;
    case WM_SIZE:
      {
        RECT r;
        GetClientRect(hwnd,&r);
        const int xpos = r.right / 2 - def_wid/2;
        SetWindowPos(GetDlgItem(hwnd,1001),NULL, xpos,2,0,0,SWP_NOZORDER|SWP_NOSIZE);
        SetWindowPos(GetDlgItem(hwnd,1000),NULL, xpos + lblw + 4,0,0,0,SWP_NOZORDER|SWP_NOSIZE);

      }
    return 0;
    case WM_COMMAND:
      if (LOWORD(wParam) == 1000 && HIWORD(wParam) == CBN_SELCHANGE)
      {
        int a = (int)SendDlgItemMessage(hwnd,1000,CB_GETCURSEL,0,0);
        if (a>=0)
        {
          const char *extlist = (const char *)GetWindowLongPtr(hwnd,GWLP_USERDATA);
          if (extlist)
          {
            NSArray *fileTypes = extensionsFromList(extlist,
                (const char *)SendDlgItemMessage(hwnd,1000,CB_GETITEMDATA,a,0));
            if ([fileTypes count]>0) 
            {
              NSSavePanel *par = (NSSavePanel*)[(NSView *)hwnd window];
              if ([par isKindOfClass:[NSSavePanel class]]) [(NSSavePanel *)par setAllowedFileTypes:fileTypes];
            }
            [fileTypes release];
          }
        }
      }
    return 0;
  }
  return DefWindowProc(hwnd,uMsg,wParam,lParam);
}

// return true
bool BrowseForSaveFile(const char *text, const char *initialdir, const char *initialfile, const char *extlist,
                       char *fn, int fnsize)
{
  swell_autoarp auto_arp;

  NSSavePanel *panel = [NSSavePanel savePanel];
  NSMutableArray *fileTypes = extensionsFromList(extlist);	
  NSString *title=(NSString *)SWELL_CStringToCFString(text); 
  NSString *ifn=NULL, *idir = NULL;

  [panel setTitle:title];
  [panel setAccessoryView:nil];
  HWND av_parent = (HWND)panel;

  if ([fileTypes count]>1)
  {
    const char *ar[2]={extlist,initialfile};
    av_parent = SWELL_CreateDialog(NULL,0,NULL,fileTypeChooseProc,(LPARAM)ar);
    if (!av_parent) av_parent = (HWND)panel;
  }

  HWND oh=NULL;
  if (BFSF_Templ_dlgproc && BFSF_Templ_dlgid) // create a child dialog and set it to the panel
  {
    oh=SWELL_CreateDialog(BFSF_Templ_reshead, BFSF_Templ_dlgid, av_parent, BFSF_Templ_dlgproc, 0);
    BFSF_Templ_dlgproc=0;
    BFSF_Templ_dlgid=0;
  }
  if (av_parent != (HWND)panel) 
  {
    if (oh) 
    { 
      RECT r1,r2;
      GetClientRect(oh,&r1);
      GetClientRect(av_parent,&r2);

      SetWindowPos(oh,NULL,0,r2.bottom,0,0,SWP_NOZORDER|SWP_NOSIZE);
      if (r2.right < r1.right) r2.right=r1.right;
      SetWindowPos(av_parent,NULL,0,0,r2.right,r2.bottom+r1.bottom,SWP_NOZORDER|SWP_NOMOVE);
      ShowWindow(oh,SW_SHOWNA);
    }
    oh = av_parent;

    NSWindow *oldw = [(NSView *)av_parent window];
    [panel setAccessoryView:(NSView *)av_parent]; // we resized our accessory view
    SendMessage(av_parent,WM_COMMAND,(CBN_SELCHANGE<<16) | 1000,0);
    [(NSView *)av_parent setHidden:NO];
    [oldw release];
  }
  else if ([fileTypes count]>0)
  {
    [panel setAllowedFileTypes:fileTypes];
  }

  if (initialfile && *initialfile && *initialfile != '.')
  {
    char s[2048];
    lstrcpyn_safe(s,initialfile,sizeof(s));
    char *p=s;
    while (*p) p++;
    while (p >= s && *p != '/') p--;
    if (p>=s)
    {
      *p=0;
      ifn=(NSString *)SWELL_CStringToCFString(p+1);
      idir=(NSString *)SWELL_CStringToCFString(s[0]?s:"/");
    }
    else 
      ifn=(NSString *)SWELL_CStringToCFString(s);
  }
  if (!idir && initialdir && *initialdir)
  {
    idir=(NSString *)SWELL_CStringToCFString(initialdir);
  }
	
  HMENU hm=SWELL_GetDefaultModalWindowMenu();
  if (hm) hm=SWELL_DuplicateMenu(hm);
  SWELL_SetCurrentMenu(hm);

  NSInteger result = [panel runModalForDirectory:idir file:ifn];
  SWELL_SetCurrentMenu(GetMenu(GetFocus()));
  if (hm) DestroyMenu(hm);
  
  if (oh) SendMessage(oh,WM_DESTROY,0,0);
  [panel setAccessoryView:nil];

  [title release];
  [fileTypes release];
  [idir release];
  [ifn release];
	
  if (result == NSOKButton)
  {
    NSString *str = [panel filename];
    if (str && fn && fnsize>0) 
    {
      SWELL_CFStringToCString(str,fn,fnsize);
      return fn[0] != 0;
    }
  }
  return false;
}

bool BrowseForDirectory(const char *text, const char *initialdir, char *fn, int fnsize)
{
  swell_autoarp auto_arp;

  NSOpenPanel *panel = [NSOpenPanel openPanel];
  NSString *title=(NSString *)SWELL_CStringToCFString(text); 
  NSString *idir=NULL;

  [panel setTitle:title];
  [panel setAllowsMultipleSelection:NO];
  [panel setCanChooseFiles:NO];
  [panel setCanCreateDirectories:YES];
  [panel setCanChooseDirectories:YES];
  [panel setResolvesAliases:YES];

  HWND oh=NULL;
  if (BFSF_Templ_dlgproc && BFSF_Templ_dlgid) // create a child dialog and set it to the panel
  {
    oh=SWELL_CreateDialog(BFSF_Templ_reshead, BFSF_Templ_dlgid, (HWND)panel, BFSF_Templ_dlgproc, 0);
    BFSF_Templ_dlgproc=0;
    BFSF_Templ_dlgid=0;
  }
	
  if (initialdir && *initialdir)
  {
    idir=(NSString *)SWELL_CStringToCFString(initialdir);
  }
	
  HMENU hm=SWELL_GetDefaultModalWindowMenu();
  if (hm) hm=SWELL_DuplicateMenu(hm);
  SWELL_SetCurrentMenu(hm);
  NSInteger result = [panel runModalForDirectory:idir file:nil types:nil];
  SWELL_SetCurrentMenu(GetMenu(GetFocus()));
  if (hm) DestroyMenu(hm);
	
  if (oh) SendMessage(oh,WM_DESTROY,0,0);
  [panel setAccessoryView:nil];
  
  [idir release];
  [title release];
	
  if (result != NSOKButton) return 0;
	
  NSArray *filesToOpen = [panel filenames];
  NSInteger count = [filesToOpen count];
		
  if (!count) return 0;
		
  NSString *aFile = [filesToOpen objectAtIndex:0];
  if (!aFile) return 0;
  SWELL_CFStringToCString(aFile,fn,fnsize);
  fn[fnsize-1]=0;
  return 1;
}


char *BrowseForFiles(const char *text, const char *initialdir, 
                     const char *initialfile, bool allowmul, const char *extlist)
{
  swell_autoarp auto_arp;

  NSOpenPanel *panel = [NSOpenPanel openPanel];
  NSString *title=(NSString *)SWELL_CStringToCFString(text); 
  NSString *ifn=NULL, *idir=NULL;
  NSMutableArray *fileTypes = extensionsFromList(extlist);	

  HWND oh=NULL;
  if (BFSF_Templ_dlgproc && BFSF_Templ_dlgid) // create a child dialog and set it to the panel
  {
    oh=SWELL_CreateDialog(BFSF_Templ_reshead, BFSF_Templ_dlgid, (HWND)panel, BFSF_Templ_dlgproc, 0);
    BFSF_Templ_dlgproc=0;
    BFSF_Templ_dlgid=0;
  }

  [panel setTitle:title];
  [panel setAllowsMultipleSelection:(allowmul?YES:NO)];
  [panel setCanChooseFiles:YES];
  [panel setCanChooseDirectories:NO];
  [panel setResolvesAliases:YES];
	
  if (initialfile && *initialfile)
  {
    char s[2048];
    lstrcpyn_safe(s,initialfile,sizeof(s));
    char *p=s;
    while (*p) p++;
    while (p >= s && *p != '/') p--;
    if (p>=s)
    {
      *p=0;
      ifn=(NSString *)SWELL_CStringToCFString(p+1);
      idir=(NSString *)SWELL_CStringToCFString(s[0]?s:"/");
    }
    else 
      ifn=(NSString *)SWELL_CStringToCFString(s);
  }
  if (!idir && initialdir && *initialdir)
  {
    idir=(NSString *)SWELL_CStringToCFString(initialdir);
  }
	
  HMENU hm=SWELL_GetDefaultModalWindowMenu();
  if (hm) hm=SWELL_DuplicateMenu(hm);
  SWELL_SetCurrentMenu(hm);
  
  NSInteger result = [panel runModalForDirectory:idir file:ifn types:([fileTypes count]>0 ? fileTypes : nil)];

  SWELL_SetCurrentMenu(GetMenu(GetFocus()));
  if (hm) DestroyMenu(hm);
	
  if (oh) SendMessage(oh,WM_DESTROY,0,0);
  [panel setAccessoryView:nil];
  
  [ifn release];
  [idir release];
	
  [fileTypes release];
  [title release];
	
  if (result != NSOKButton) return 0;
	
  NSArray *filesToOpen = [panel filenames];
  const NSInteger count = [filesToOpen count];
		
  if (!count) return 0;
		
  char fn[2048];
  if (count==1||!allowmul)
  {
    NSString *aFile = [filesToOpen objectAtIndex:0];
    if (!aFile) return 0;
    SWELL_CFStringToCString(aFile,fn,sizeof(fn));
    fn[sizeof(fn)-1]=0;
    char *ret=(char *)malloc(strlen(fn)+2);
    memcpy(ret,fn,strlen(fn));
    ret[strlen(fn)]=0;
    ret[strlen(fn)+1]=0;
    return ret;
  }
		
  size_t rsize=1;
  char *ret=0;
  for (NSInteger i=0; i<count; i++) 
  {
    NSString *aFile = [filesToOpen objectAtIndex:i];
    if (!aFile) continue;
    SWELL_CFStringToCString(aFile,fn,sizeof(fn));
    fn[sizeof(fn)-1]=0;
		
    size_t tlen=strlen(fn)+1;
    ret=(char *)realloc(ret,rsize+tlen+1);
    if (!ret) return 0;
    
    if (rsize==1) ret[0]=0;
    strcpy(ret+rsize,fn);
    rsize+=tlen;
    ret[rsize]=0;
  }	
  return ret;
}




int MessageBox(HWND hwndParent, const char *text, const char *caption, int type)
{
  swell_autoarp auto_arp;

  NSInteger ret=0;

  NSString *tit=(NSString *)SWELL_CStringToCFString(caption?caption:""); 
  NSString *text2=(NSString *)SWELL_CStringToCFString(text?text:"");
  
  if (type == MB_OK)
  {
    NSRunAlertPanel(tit,@"%@",@"OK",@"",@"",text2);
    ret=IDOK;
  }	
  else if (type == MB_OKCANCEL)
  {
    ret=NSRunAlertPanel(tit,@"%@",@"OK",@"Cancel",@"",text2);
    if (ret) ret=IDOK;
    else ret=IDCANCEL;
  }
  else if (type == MB_YESNO)
  {
    ret=NSRunAlertPanel(tit,@"%@",@"Yes",@"No",@"",text2);
  //  printf("ret=%d\n",ret);
    if (ret) ret=IDYES;
    else ret=IDNO;
  }
  else if (type == MB_RETRYCANCEL)
  {
    ret=NSRunAlertPanel(tit,@"%@",@"Retry",@"Cancel",@"",text2);
//    printf("ret=%d\n",ret);
    if (ret) ret=IDRETRY;
    else ret=IDCANCEL;
  }
  else if (type == MB_YESNOCANCEL)
  {
    ret=NSRunAlertPanel(tit,@"%@",@"Yes",@"Cancel",@"No",text2);
    if (ret == 1) ret=IDYES;
    else if (ret==-1) ret=IDNO;
    else ret=IDCANCEL;
  }
  
  [text2 release];
  [tit release];
  
  return (int)ret; 
}

#endif
