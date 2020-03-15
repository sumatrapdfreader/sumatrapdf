/* 
    WDL - richeditctrl.h
    Copyright (C) 2005 and later Cockos Incorporated

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


#ifndef _WDL_RICHEDITCTRL_H
#define _WDL_RICHEDITCTRL_H

#include <windows.h>
#include <richedit.h>

class WDL_RichEditCtrl
{
  public:
    WDL_RichEditCtrl() { setWnd(NULL); m_color=0; m_bold=0; }
    WDL_RichEditCtrl(HWND hwnd) { setWnd(hwnd); m_color=0; m_bold=0; }
    ~WDL_RichEditCtrl() { };

    void setWnd(HWND hwnd) { 
      m_hwnd=hwnd; 
      if(hwnd) {
        SendMessage(m_hwnd, EM_SETEVENTMASK, 0, ENM_LINK);
        SendMessage(m_hwnd, EM_AUTOURLDETECT, 1, 0);
      }
    }
    HWND getWnd() { return m_hwnd; }

    int getLength() { return GetWindowTextLength(m_hwnd); }
    void getText(char *txt, int size) { GetWindowText(m_hwnd, txt, size); }
    void setText(char *txt) { SetWindowText(m_hwnd, txt); }
    void addText(char *txt)
    {
      setSel(getLength(), getLength());
      CHARFORMAT2 cf2;
      cf2.cbSize=sizeof(cf2);
      cf2.dwMask=CFM_COLOR|CFM_BOLD;
      cf2.dwEffects=m_bold?CFE_BOLD:0;
      cf2.crTextColor=m_color;
      SendMessage(m_hwnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf2);
      replaceSel(txt);

      int mi,ma;
      GetScrollRange(m_hwnd,SB_VERT,&mi,&ma);
      SendMessage(m_hwnd, WM_VSCROLL, (ma<<16)+SB_THUMBPOSITION,0);
      //SetScrollPos(m_hwnd,SB_VERT,ma,TRUE);
      
      //SendMessage(m_hwnd, WM_VSCROLL, SB_BOTTOM,0); // buggy on windows ME
    }
    void setSel(int start, int end) { SendMessage(m_hwnd, EM_SETSEL, start, end); }
    void replaceSel(char *txt) { SendMessage(m_hwnd, EM_REPLACESEL, 0L, (LPARAM)txt); }

    void clear() { setText(""); }

    void setTextColor(int color) { m_color=color; }

    void setBold(int b) { m_bold=b; }

  protected:
    HWND m_hwnd;
    COLORREF m_color;
    int m_bold;
};

#endif//_WDL_RICHEDITCTRL_H