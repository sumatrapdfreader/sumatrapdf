#include "main.h"

#include "resource.h"


WDL_DLGRET MainDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_INITDIALOG:
      g_hwnd=hwndDlg;

      SetDlgItemText(hwndDlg,IDC_EDIT1,"Text yay");

      ShowWindow(hwndDlg,SW_SHOW);
    return 1;
    case WM_DESTROY:
      g_hwnd=NULL;

      #ifdef _WIN32
         PostQuitMessage(0);
      #else
         // this isnt just "PostQuitMessage", because the behaviors don't totally match -- i.e. 
         // it is relatively normal for the OS X app to get terminated without us calling this. Sooo, our exit handler 
         // code shouldn't rely on us having called this, etc...
         SWELL_PostQuitMessage(hwndDlg); 
      #endif

    return 0;
    case WM_CLOSE:
      DestroyWindow(hwndDlg);
    return 0;
    case WM_COMMAND:
      switch (LOWORD(wParam))
      {
        case ID_QUIT:
          DestroyWindow(hwndDlg);
        return 0;
        case ID_SOMETHING:
          {
            char buf[1024];
            if (GetDlgItemText(hwndDlg,IDC_EDIT1,buf,sizeof(buf)))
            {
              MessageBox(hwndDlg,buf,"The string!",MB_OK);
            }
          }
        return 0;
        case ID_ABOUT:
          MessageBox(hwndDlg,"SWELL test app","Hooray",MB_OK);
        return 0;
      }
    return 0;
  }
  return 0;
}
