#ifndef _WDL_WIN32_HELPERS_H_
#define _WDL_WIN32_HELPERS_H_


static HMENU InsertSubMenu(HMENU hMenu, int pos, const char *name, int flags=0)
{
  HMENU sub = NULL;
  if (hMenu) 
  {
    sub = CreatePopupMenu();
    InsertMenu(hMenu,pos,flags|MF_BYPOSITION|(sub?MF_POPUP:0)|MF_STRING,(UINT_PTR)sub, name);
  }
  return sub;
}

static void InsertMenuString(HMENU hMenu, int pos, const char *name, int idx, int flags=0)
{
  if (hMenu) InsertMenu(hMenu,pos,flags|MF_BYPOSITION|MF_STRING, idx, name);
}

static void InsertMenuSeparator(HMENU hMenu, int pos)
{
  if (hMenu) InsertMenu(hMenu,pos,MF_BYPOSITION|MF_SEPARATOR, 0, NULL);
}

#endif
