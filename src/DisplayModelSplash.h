/* Copyright Krzysztof Kowalczyk 2006-2007
   License: GPLv2 */
#ifndef _DISPLAY_MODEL_SPLASH_H_
#define _DISPLAY_MODEL_SPLASH_H_

#include "DisplayState.h"
#include "DisplayModel.h"

class GooString;
class Link;
class LinkAction;
class LinkDest;
class LinkGoTo;
class LinkGoToR;
class LinkLaunch;
class LinkNamed;
class LinkURI;
class Links;
class PDFDoc;
class SplashBitmap;
class TextOutputDev;
class TextPage;
class UGooString;

class DisplayModelSplash : public DisplayModel
{
public:
    DisplayModelSplash(DisplayMode displayMode);
    virtual ~DisplayModelSplash();

    PdfEnginePoppler * pdfEnginePoppler() { return (PdfEnginePoppler*)pdfEngine(); }

    virtual void  handleLink(PdfLink *pdfLink);
    virtual void  goToTocLink(void *link);

    TextPage *    GetTextPage(int pageNo);

    void          EnsureSearchHitVisible();

    void          handleLinkAction(LinkAction *action);
    void          handleLinkGoTo(LinkGoTo *linkGoTo);
    void          handleLinkGoToR(LinkGoToR *linkGoToR);
    void          handleLinkURI(LinkURI *linkURI);
    void          handleLinkLaunch(LinkLaunch* linkLaunch);
    void          handleLinkNamed(LinkNamed *linkNamed);
    BOOL          CanGoToNextPage();
    BOOL          CanGoToPrevPage();

    void          FindInit(int startPageNo);
    BOOL          FindNextForward();
    BOOL          FindNextBackward();


    void          FreeTextPages(void);
    void          RecalcLinks(void);
    void          GoToDest(LinkDest *linkDest);
    void          GoToNamedDest(UGooString *dest);
    void          FreeLinks(void);

    virtual int   getTextInRegion(int pageNo, RectD *region, unsigned short *buf, int buflen);
    virtual void  cvtUserToScreen(int pageNo, double *x, double *y);
    virtual void  cvtScreenToUser(int *pageNo, double *x, double *y);

protected:
    virtual void  MapResultRectToScreen(PdfSearchResult *rect);

public:
    PDFDoc *            pdfDoc;
};

DisplayModelSplash *DisplayModelSplash_CreateFromFileName(const char *fileName,
                                            SizeD totalDrawAreaSize,
                                            int scrollbarXDy, int scrollbarYDx,
                                            DisplayMode displayMode, int startPage,
                                            WindowInfo *win);
#endif
