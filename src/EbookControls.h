/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class FrameRateWnd;

class HtmlFormatter;
class HtmlFormatterArgs;
class PageControl;
class PagesLayout;
using namespace mui;

// controls managed by EbookController
struct EbookControls {
    ParsedMui* muiDef;
    HwndWrapper* mainWnd;

    ScrollBar* progress;
    Button* status;
    ILayout* topPart;
    PagesLayout* pagesLayout;
};

EbookControls* CreateEbookControls(HWND hwnd, FrameRateWnd*);
void DestroyEbookControls(EbookControls* controls);
void SetMainWndBgCol(EbookControls* ctrls);

class HtmlPage;
struct DrawInstr;

// control that shows a single ebook page
// TODO: move to a separate file
class PageControl : public Control {
    HtmlPage* page;
    int cursorX, cursorY;

  public:
    PageControl();
    virtual ~PageControl();

    void SetPage(HtmlPage* newPage);
    HtmlPage* GetPage() const {
        return page;
    }

    Size GetDrawableSize() const;
    DrawInstr* GetLinkAt(int x, int y) const;

    virtual void Paint(Graphics* gfx, int offX, int offY);

    virtual void NotifyMouseMove(int x, int y);
};

// PagesLayout is for 2 controls separated with a space:
// [ ctrl1 ][ spaceDx ][ ctrl2]
// It sets the size of child controls to fit within its space
// One of the controls can be hidden, in which case it takes
// all the space
class PagesLayout : public ILayout {
  protected:
    Size desiredSize;
    PageControl* page1;
    PageControl* page2;
    int spaceDx;

  public:
    PagesLayout(PageControl* p1, PageControl* p2, int dx = 8) {
        page1 = p1;
        page2 = p2;
        CrashIf(dx < 0);
        spaceDx = dx;
    }
    virtual ~PagesLayout() {
    }
    virtual Size DesiredSize() {
        return desiredSize;
    }

    virtual Size Measure(const Size availableSize);
    virtual void Arrange(const Rect finalRect);

    PageControl* GetPage1() const {
        return page1;
    }
    PageControl* GetPage2() const {
        return page2;
    }
    void SetSpaceDx(int dx) {
        spaceDx = dx;
        // TODO: trigger re-layout ?
    }
    int GetSpaceDx() const {
        return spaceDx;
    }
};
