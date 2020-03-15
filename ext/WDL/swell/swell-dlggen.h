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
  
  DialogBox emulation is here. To declare the resource at a global level, use (in any source file that includes this file and resource.h):

  
  #ifdef MAC


  SWELL_DEFINE_DIALOG_RESOURCE_BEGIN(IDD_SOMEDIALOG,0,"Dialog Box Title",222,55,1.8) // width, height, scale (1.8 is usually good)

  BEGIN
  DEFPUSHBUTTON   "OK",IDOK,117,33,47,14
  CONTROL         "Expand MIDI tracks to new REAPER tracks                    ",IDC_CHECK1,
  "Button",BS_AUTOCHECKBOX | WS_TABSTOP,4,7,214,10
  CONTROL         "Merge MIDI tempo map to project tempo map at                ",
  IDC_CHECK2,"Button",BS_AUTOCHECKBOX | WS_TABSTOP,4,19,
  214,10
  PUSHBUTTON      "Cancel",IDCANCEL,168,33,50,14
  END

  SWELL_DEFINE_DIALOG_RESOURCE_END(IDD_SOMEDIALOG)


  #endif


  This file also provides functions to dynamically create controls in a view from a win32 resource script.



*/




#ifndef _SWELL_DLGGEN_H_
#define _SWELL_DLGGEN_H_

#ifdef BEGIN
#undef BEGIN
#endif

#ifdef END
#undef END
#endif


struct SWELL_DlgResourceEntry
{
  const char *str1;
  int flag1;
  const char *str2;
  
  int p1; // often used for ID

  // todo: see at runtime if some of these can be short instead of int (p2-p6 probably can, but not completely sure) -- i.e. do we use any
  // big values in flags. note: it can't be unsigned short, because we want negative values.
  int p2, p3, p4, p5, p6; // meaning is dependent on class
};


#define BEGIN {NULL,
#define END  },

#define PUSHBUTTON     }, { "__SWELL_BUTTON", 0, 
#define DEFPUSHBUTTON  }, { "__SWELL_BUTTON", 1, 
#define EDITTEXT       }, { "__SWELL_EDIT", 0, "",
#define CTEXT          }, { "__SWELL_LABEL", 0, 
#define LTEXT          }, { "__SWELL_LABEL", -1, 
#define RTEXT          }, { "__SWELL_LABEL", 1, 
#define CONTROL        }, { 
#define COMBOBOX       }, { "__SWELL_COMBO", 0, "", 
#define GROUPBOX       }, { "__SWELL_GROUP", 0, 
#define CHECKBOX       }, { "__SWELL_CHECKBOX", 0, 
#define LISTBOX        }, { "__SWELL_LISTBOX", 0, "", 

#define NOT 
                                    
// flags we may use
#define CBS_DROPDOWNLIST 0x0003L
#define CBS_DROPDOWN 0x0002L
#define CBS_SORT     0x0100L
#define ES_PASSWORD 0x0020L
#define ES_READONLY 0x0800L
#define ES_WANTRETURN 0x1000L
#define ES_NUMBER 0x2000L
         
#define SS_LEFT 0
#define SS_CENTER 0x1L                                                                     
#define SS_RIGHT 0x2L
#define SS_BLACKRECT 0x4L
#define SS_BLACKFRAME (SS_BLACKRECT)
#define SS_LEFTNOWORDWRAP 0xCL
#define SS_TYPEMASK 0x1FL
#define SS_NOTIFY 0x0100L

#define BS_CENTER 0x0300L
#define BS_LEFTTEXT 0x0020L
#define BS_LEFT 0x100L
#define BS_GROUPBOX      0x20000000
#define BS_DEFPUSHBUTTON 0x10000000
#define BS_PUSHBUTTON    0x8000000
                                       
#define LVS_LIST 0 /* 0x0003 */
#define LVS_NOCOLUMNHEADER 0x4000
#define LVS_NOSORTHEADER   0x8000
#define LVS_REPORT 0x0001
#define LVS_TYPEMASK 0x0003
#define LVS_SINGLESEL 0x0004
#define LVS_OWNERDATA 0x1000       
#define LVS_SORTASCENDING       0x0010
#define LVS_SORTDESCENDING      0x0020
                              
#define LBS_SORT           0x0002L
#define LBS_OWNERDRAWFIXED 0x0010L
#define LBS_EXTENDEDSEL 0x0800L
                                        
#define ES_LEFT 0
#define ES_CENTER 1
#define ES_RIGHT 2
#define ES_MULTILINE 4
#define ES_AUTOHSCROLL 0x80
                                    
// flags we ignore
#define LVS_SHOWSELALWAYS 0
#define LVS_SHAREIMAGELISTS 0
#define ES_AUTOVSCROLL 0
#define GROUP 0
#define PBS_SMOOTH 0
#define CBS_AUTOHSCROLL 0
#define TBS_NOTICKS 0
#define TBS_TOP 0
#define TBS_BOTH 0
#define LBS_NOINTEGRALHEIGHT 0
#define TVS_HASLINES 0
#define TVS_LINESATROOT 0
#define TVS_SHOWSELALWAYS 0
#define TVS_HASBUTTONS 0
#define BS_FLAT 0
#define TVS_DISABLEDRAGDROP 0
#define TVS_TRACKSELECT 0
#define TVS_NONEVENHEIGHT 0
#define SS_SUNKEN 0
#define BS_RIGHT 0
#define WS_EX_STATICEDGE 0
#define WS_EX_RIGHT 0
#define SS_CENTERIMAGE 0                                       
#define SS_NOPREFIX 0

// more ignore flags for vc11+
#define LVS_ALIGNLEFT 0 /* 0x0800 */

#ifndef IDC_STATIC
#define IDC_STATIC 0
#endif

                                     

                                       
#define SWELL_DLG_WS_CHILD 1
#define SWELL_DLG_WS_RESIZABLE 2
#define SWELL_DLG_WS_FLIPPED 4
#define SWELL_DLG_WS_NOAUTOSIZE 8
#define SWELL_DLG_WS_OPAQUE 16
#define SWELL_DLG_WS_DROPTARGET 32
     
typedef struct SWELL_DialogResourceIndex
{
  const char *resid;
  const char *title;
  int windowTypeFlags;
  void (*createFunc)(HWND, int);
  int width,height;
  struct SWELL_DialogResourceIndex *_next;
} SWELL_DialogResourceIndex; 

typedef struct SWELL_CursorResourceIndex
{
  const char *resid;
  const char *resname;
  POINT hotspot;
  HCURSOR cachedCursor;
  struct SWELL_CursorResourceIndex *_next;
} SWELL_CursorResourceIndex;


class SWELL_DialogRegHelper { 
  public:
     SWELL_DialogResourceIndex m_rec;
     SWELL_DialogRegHelper(SWELL_DialogResourceIndex **h, void (*cf)(HWND,int), int recid, int flags, const char *titlestr, int wid, int hei, double scale)
     {
       if (recid) 
       {
         m_rec.resid=MAKEINTRESOURCE(recid); 
         m_rec.title=titlestr; 
         m_rec.windowTypeFlags=flags; 
         m_rec.createFunc=cf; 
         m_rec.width=(int)((wid)*(scale)); 
         m_rec.height=(int)((hei)*(scale)); 
         m_rec._next=*h;
         *h = &m_rec;
       } 
     }
};

#ifdef _DEBUG
  #include "../assocarray.h"
  class SWELL_DialogRegValidator 
  {
    public:
      SWELL_DialogRegValidator(const SWELL_DlgResourceEntry *recs, size_t recs_sz)
      {
        if (recs_sz>1)
        {
          // check for duplicate IDs
          WDL_IntKeyedArray<bool> tmp;
          for (size_t x = 0; x < recs_sz; x ++)
          {
            const SWELL_DlgResourceEntry *list = recs + x;
            const int idx = strncmp(list->str1,"__SWELL_",8) ? list->flag1 : list->p1;
            if (idx != 0 && idx != -1)
            {
              WDL_ASSERT(!tmp.Get(idx));
              tmp.Insert(idx,true);
            }
          }
        }
      }
  };
  #define SWELL_VALIDATE_DIALOG_RESOURCE(v,r) static SWELL_DialogRegValidator v(r+1, sizeof(r)/sizeof(r[0])-1); 
#else
  #define SWELL_VALIDATE_DIALOG_RESOURCE(v,r)
#endif


#define SWELL_DEFINE_DIALOG_RESOURCE_BEGIN(recid, flags, titlestr, wid, hei, scale) \
                                       static void SWELL__dlg_cf__##recid(HWND view, int wflags); \
                                       const float __swell_dlg_scale__##recid = (float) (scale); \
                                       static SWELL_DialogRegHelper __swell_dlg_helper_##recid(&SWELL_curmodule_dialogresource_head, SWELL__dlg_cf__##recid, recid,flags,titlestr,wid,hei,scale); \
                                       static const SWELL_DlgResourceEntry __swell_dlg_list__##recid[]={

                                            
#define SWELL_DEFINE_DIALOG_RESOURCE_END(recid ) }; \
                              SWELL_VALIDATE_DIALOG_RESOURCE( __swell_dlg_validator__##recid, __swell_dlg_list__##recid) \
                              static void SWELL__dlg_cf__##recid(HWND view, int wflags) { \
                                SWELL_MakeSetCurParms(__swell_dlg_scale__##recid,__swell_dlg_scale__##recid,0,0,view,false,!(wflags&SWELL_DLG_WS_NOAUTOSIZE));  \
                                SWELL_GenerateDialogFromList(__swell_dlg_list__##recid+1,sizeof(__swell_dlg_list__##recid)/sizeof(__swell_dlg_list__##recid[0])-1); \
                              }

#endif
