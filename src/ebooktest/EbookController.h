/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef EbookController_h
#define EbookController_h

#include "BaseUtil.h"
#include "Mui.h"
#include "ThreadUtil.h"
#include "Vec.h"

using namespace mui;

struct  EbookControls;
struct  PageData;
class   UiMsg;
class   MobiDoc;
class   ThreadLayoutMobi;

class EbookController : public IClicked, ISizeChanged
{
    EbookControls * ctrls;

    MobiDoc *       mobiDoc;
    const char *    html;

    Vec<PageData*>* pages;
    int             currPageNo; // within pages
    int             pageDx, pageDy; // size of the page for which pages was generated

    ThreadLayoutMobi *layoutThread;
    void SetStatusText() const;
    void DeletePages();
    void AdvancePage(int dist);
    void SetPage(int newPageNo);
    void TriggerLayout();
    void LayoutHtml(int dx, int dy);

    // IClickHandler
    virtual void Clicked(Control *c, int x, int y);

    // ISizeChanged
    virtual void SizeChanged(Control *c, int dx, int dy);

public:
    EbookController(EbookControls *ctrls);
    virtual ~EbookController();

    void SetHtml(const char *html);
    void LoadMobi(const TCHAR *fileName);
    void FinishedMobiLoading(UiMsg *msg);
    void FinishedMobiLayout(UiMsg *msg);
    void OnLayoutTimer();
};

#endif
