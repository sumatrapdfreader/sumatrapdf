/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct FrameRateWnd;

class HtmlFormatter;
struct HtmlFormatterArgs;
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
void DestroyEbookControls(EbookControls* ctrls);
void SetMainWndBgCol(EbookControls* ctrls);

struct HtmlPage;
struct DrawInstr;

// control that shows a single ebook page
// TODO: move to a separate file
class PageControl : public Control {
    HtmlPage* page{nullptr};
    int cursorX{-1};
    int cursorY{-1};

  public:
    PageControl();
    virtual ~PageControl();

    void SetPage(HtmlPage* newPage);
    HtmlPage* GetPage() const {
        return page;
    }

    Size GetDrawableSize() const;
    DrawInstr* GetLinkAt(int x, int y) const;

    void Paint(Graphics* gfx, int offX, int offY) override;

    void NotifyMouseMove(int x, int y) override;
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
    Size DesiredSize() override {
        return desiredSize;
    }

    Size Measure(const Size availableSize) override;
    void Arrange(const Rect finalRect) override;

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
