#ifndef _SWELL_MENUGEN_H_
#define _SWELL_MENUGEN_H_


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

  Dynamic menu generation

  Usage:

  See: mac_resgen.php etc

  */


#include "swell.h"


#ifdef BEGIN
#undef BEGIN
#endif

#ifdef END
#undef END
#endif


typedef struct SWELL_MenuResourceIndex
{
  const char *resid;
  void (*createFunc)(HMENU hMenu);
  struct SWELL_MenuResourceIndex *_next;
} SWELL_MenuResourceIndex;
extern SWELL_MenuResourceIndex *SWELL_curmodule_menuresource_head;


#define SWELL_MENUGEN_POPUP_PREFIX "/.BO^O:"
#define SWELL_MENUGEN_ENDPOPUP "EN%%%^:"
struct SWELL_MenuGen_Entry
{
  const char *name; // will begin with SWELL_MENUGEN_POPUP_PREFIX on submenus, and will be SWELL_MENUGEN_ENDPOPUP at the end of a submenu
  unsigned short idx;
  unsigned short flags;
};

class SWELL_MenuGenHelper
{
  public:
    SWELL_MenuResourceIndex m_rec;
    SWELL_MenuGenHelper(SWELL_MenuResourceIndex **h, void (*cf)(HMENU), int recid)
    {
      m_rec.resid=MAKEINTRESOURCE(recid); 
      m_rec.createFunc=cf; 
      m_rec._next=*h;
      *h = &m_rec;
    }
};

#define SWELL_DEFINE_MENU_RESOURCE_BEGIN(recid) \
  static void __swell_menu_cf__##recid(HMENU hMenu); \
  static SWELL_MenuGenHelper __swell_menu_cf_helper__##recid(&SWELL_curmodule_menuresource_head, __swell_menu_cf__##recid, recid);  \
  static void __swell_menu_cf__##recid(HMENU hMenu) { static const SWELL_MenuGen_Entry list[]={{NULL,0,0
        
#define SWELL_DEFINE_MENU_RESOURCE_END(recid) } }; SWELL_GenerateMenuFromList(hMenu,list+1,sizeof(list)/sizeof(list[0])-1);  } 


#define GRAYED 1
#define INACTIVE 2
#define POPUP }, { SWELL_MENUGEN_POPUP_PREFIX
#define MENUITEM }, { 
#define SEPARATOR NULL, 0xffff
#define BEGIN
#define END }, { SWELL_MENUGEN_ENDPOPUP
                             
#endif//_SWELL_MENUGEN_H_
                             
