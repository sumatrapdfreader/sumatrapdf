#include "test-app.h"
#include "BaseUtil.h"

// in TestDirectDraw.cpp
extern int TestDirectDraw(HINSTANCE hInstance, int nCmdShow);
// in TestTab.cpp
extern int TestTab(HINSTANCE hInstance, int nCmdShow);

int APIENTRY WinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNUSED(hPrevInstance);
    UNUSED(lpCmdLine);
    //SetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);
    //SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

    INITCOMMONCONTROLSEX cc = { 0 };
    cc.dwSize = sizeof(cc);
    cc.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&cc);

    //return TestDirectDraw(hInstance, nCmdShow);
    return TestTab(hInstance, nCmdShow);
}
