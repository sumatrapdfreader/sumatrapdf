/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef EbookController_h
#define EbookController_h

#include "BaseUtil.h"
#include "Mui.h"
#include "Vec.h"

using namespace mui;

struct  EbookControls;
struct  PageData;
class   UiMsg;
class   MobiDoc;

class EbookController : public IClicked, ISizeChanged
{
    EbookControls * ctrls;

    MobiDoc *       mb;
    const char *    html;

    Vec<PageData*>* pages;
    int             currPageNo; // within pages
    int             pageDx, pageDy; // size of the page for which pages was generated

    void SetStatusText() const;
    void DeletePages();
    void PageLayout(int dx, int dy);
    void AdvancePage(int dist);
    void SetPage(int newPageNo);

    // IClickHandler
    virtual void Clicked(Control *c, int x, int y);

    // ISizeChanged
    virtual void SizeChanged(Control *c, int dx, int dy);

public:
    EbookController(EbookControls *ctrls);
    virtual ~EbookController();

    void SetHtml(const char *html);
    void LoadMobi(const TCHAR *fileName);
    void MobiFinishedLoading(UiMsg *msg);
};

#endif
