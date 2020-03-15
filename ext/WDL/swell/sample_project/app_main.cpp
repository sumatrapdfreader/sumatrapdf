#include "main.h"
#include "resource.h"

/***
 How we created the xcode project that compiles this, too:
 
 New Project -> Mac OS X -> Application -> Cocoa Application
 
 Save as...
 
 Add to "Other sources/SWELL": (from swell path) swell-dlg.mm swell-gdi.mm swell-ini.cpp swell-kb.mm swell-menu.mm swell-misc.mm swell-miscdlg.mm swell-wnd.mm swell.cpp swell-appstub.mm swellappmain.mm
 
 Add app_main.cpp main_dialog.cpp to "Other sources"
 
 Go to Frameworks -> Linked Frameworks, add existing framework, Carbon.Framework
 
 go to terminal, to project dir, and run php <pathtoswell>/mac_resgen.php sample_project.rc

 Open mainmenu.xib in Interface Builder (doubleclick it in XCode)

   + Delete the default "Window"
   + File->Read class files, find and select "swellappmain.h"
   + Go to Library, Objects, scroll to "Object" , drag to "MainMenu.xib", rename to "Controller", then open its 
     properties (Cmd+Shift+I, go to class identity tab), choose for Class "SWELLAppController".
   + Select "Application" in MainMenu.xib, go to (Cmd+Shift+I) application identity tab, select "SWELLApplication" for the class.
 
   + Customize the "NewApplication" menu.
      + Properties on "About NewApplication":
        + set the tag to 40002 (matching resource.h for about)
        + on the connection tab, "Sent Actions", remove the default connection, then drag a new connection to controller (onSysMenuCommand).
      + Properties on "Quit NewApplication":
        + set the tag to 40001 (matching resource.h for quit)
        + on the connection tab, "Sent Actions", remove the default connection, then drag a new connection to controller (onSysMenuCommand).
   + Delete the file/edit/format/view/window/help menus, if you like (or keep some of them if you want)
 
   + Save and quit IB

 Go to Targets->sample_project, hit cmd+I, go to "Properties", and change Principal class to "SWELLApplication"
 
 
 ...and then bleh you're done! :)
 
 */



HWND g_hwnd; // our main window (this corresponds to an NSView on OSX but we still use HWND)

// these are not needed on OSX but defined anyway
HINSTANCE g_hInst; 
UINT Scroll_Message; 


#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdParam, int nShowCmd)
{
  g_hInst=hInstance;
 // SWELLAPP_ONLOAD or so
  
  InitCommonControls();
  Scroll_Message = RegisterWindowMessage("MSWHEEL_ROLLMSG");

  // create dialog (SWELLAPP_LOADED)
  CreateDialog(g_hInst,MAKEINTRESOURCE(IDD_DIALOG1),GetDesktopWindow(),MainDlgProc);

  for(;;)
	{	      
    MSG msg={0,};
    int vvv = GetMessage(&msg,NULL,0,0);
    if (!vvv)  break;

    if (vvv<0)
    {
      Sleep(10);
      continue;
    }
    if (!msg.hwnd)
    {
		  DispatchMessage(&msg);
      continue;
    }

    // if you want to hook any keyboard in here, the SWELL equiv is SWELLAPP_PROCESSMESSAGE


    if (g_hwnd && IsDialogMessage(g_hwnd,&msg)) continue;

    // default processing for other dialogs
    HWND hWndParent=NULL;
    HWND temphwnd = msg.hwnd;
    do
    { 
      if (GetClassLong(temphwnd, GCW_ATOM) == (INT)32770) 
      {
        hWndParent=temphwnd;
        if (!(GetWindowLong(temphwnd,GWL_STYLE)&WS_CHILD)) break; // not a child, exit 
      }
    }
    while (temphwnd = GetParent(temphwnd));
    
    if (hWndParent && IsDialogMessage(hWndParent,&msg)) continue;

		TranslateMessage(&msg);
   	DispatchMessage(&msg);

  }

  // in case g_hwnd didnt get destroyed -- this corresponds to SWELLAPP_DESTROY roughly
  if (g_hwnd) DestroyWindow(g_hwnd);

  return 0;
}
#else

extern HMENU SWELL_app_stocksysmenu;
extern "C" {
INT_PTR SWELLAppMain(int msg, INT_PTR parm1, INT_PTR parm2)
{
  switch (msg)
  {
    case SWELLAPP_ONLOAD:
    break;
    case SWELLAPP_LOADED:
      
      if (SWELL_app_stocksysmenu) // set the SWELL default menu, using the .nib'd menu as the default settings
      {
        HMENU menu = CreatePopupMenu();    
        HMENU nm=SWELL_DuplicateMenu(SWELL_app_stocksysmenu);
        if (nm)
        {
          MENUITEMINFO mi={sizeof(mi),MIIM_STATE|MIIM_SUBMENU|MIIM_TYPE,MFT_STRING,0,0,nm,NULL,NULL,0,"SampleApp"};
          InsertMenuItem(menu,0,TRUE,&mi);           
        }    
        SWELL_SetDefaultModalWindowMenu(menu);
      }      
      
      {
        HMENU menu = LoadMenu(NULL,MAKEINTRESOURCE(IDR_MENU1));
        {
          HMENU sm=GetSubMenu(menu,0);
          DeleteMenu(sm,ID_QUIT,MF_BYCOMMAND); // remove QUIT from our file menu, since it is in the system menu on OSX
        
          // remove any trailing separators
          int a= GetMenuItemCount(sm);
          while (a > 0 && GetMenuItemID(sm,a-1)==0) DeleteMenu(sm,--a,MF_BYPOSITION);
        }
        // delete help menu from Windows menu (since we only use it for "About", which is in the system menu
        DeleteMenu(menu,GetMenuItemCount(menu)-1,MF_BYPOSITION);
      
        if (SWELL_app_stocksysmenu) // insert the stock system menu
        {
          HMENU nm=SWELL_DuplicateMenu(SWELL_app_stocksysmenu);
          if (nm)
          {
            MENUITEMINFO mi={sizeof(mi),MIIM_STATE|MIIM_SUBMENU|MIIM_TYPE,MFT_STRING,0,0,nm,NULL,NULL,0,"SampleApp"};
            InsertMenuItem(menu,0,TRUE,&mi);           
          }
        }  
        
        // if we want to set any default modifiers for items in the menus, we can use:
        // SetMenuItemModifier(menu,commandID,MF_BYCOMMAND,'A',FCONTROL) etc.
        
        
        HWND hwnd = CreateDialog(g_hInst,MAKEINTRESOURCE(IDD_DIALOG1),NULL,MainDlgProc);
        
        SetMenu(hwnd,menu); // set the menu for the dialog to our menu (on Windows that menu is set from the .rc, but on SWELL
                            // we need to set it manually (and obviously we've edited the menu anyway)
      }
    break;
    case SWELLAPP_ONCOMMAND:
      // this is to catch commands coming from the system menu etc
      if (g_hwnd && (parm1&0xffff)) SendMessage(g_hwnd,WM_COMMAND,parm1&0xffff,0);
    break;
    case SWELLAPP_DESTROY:
      if (g_hwnd) DestroyWindow(g_hwnd);
    break;
    case SWELLAPP_PROCESSMESSAGE:
      // parm1 = (MSG*), should we want it -- look in swell.h to see what the return values refer to
     break;
  }
  return 0;
}
};

#endif




#ifndef _WIN32 // import the resources. Note: if you do not have these files, run "php ../mac_resgen.php sample_project.rc" from this directory
  #include "../swell-dlggen.h"

  #include "sample_project.rc_mac_dlg"

  #include "../swell-menugen.h"

  #include "sample_project.rc_mac_menu"
#endif
