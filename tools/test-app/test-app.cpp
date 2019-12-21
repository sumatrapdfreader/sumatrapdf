#include "test-app.h"
#include "utils/BaseUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"

// in TestDirectDraw.cpp
extern int TestDirectDraw(HINSTANCE hInstance, int nCmdShow);
// in TestTab.cpp
extern int TestTab(HINSTANCE hInstance, int nCmdShow);
// in TestLayout.cpp
extern int TestLayout(HINSTANCE hInstance, int nCmdShow);


int APIENTRY WinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNUSED(hInstance);
    UNUSED(hPrevInstance);
    UNUSED(lpCmdLine);
    UNUSED(nCmdShow);
    //SetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);
    //SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

    INITCOMMONCONTROLSEX cc = { 0 };
    cc.dwSize = sizeof(cc);
    cc.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&cc);

    //return TestDirectDraw(hInstance, nCmdShow);
    //return TestTab(hInstance, nCmdShow);
    //return TestLayout(hInstance, nCmdShow);

    auto w = new Window();
    w->backgroundColor = MkRgb((u8)0xee, (u8)0xee, (u8)0xee);
    w->SetTitle("this is a title");
    w->initialPos = {100, 100, 100 + 480, 100 + 640};
    bool ok = w->Create();
    CrashIf(!ok);
    w->SetIsVisible(true);

    auto res = RunMessageLoop(nullptr);
    return res;
}
