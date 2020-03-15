#import "Controller.h"

#include "main.cpp" // this would otherwise conflict in object name with main.m, which xcode made us.


@implementation Controller
-(void)awakeFromNib
{
  SWELL_RegisterCustomControlCreator(ccontrolCreator);
  HWND h=CreateDialog(NULL,MAKEINTRESOURCE(IDD_DIALOG1),NULL,dlgProc);
  ShowWindow(h,SW_SHOW);
}
@end
