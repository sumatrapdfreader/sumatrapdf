/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef BaseEngine_h
#define BaseEngine_h

#include "BaseUtil.h"
#include "GeomUtil.h"
#include "Vec.h"

/* certain OCGs will only be rendered for some of these (e.g. watermarks) */
enum RenderTarget { Target_View, Target_Print, Target_Export };

enum PageLayoutType { Layout_Single = 0, Layout_Facing = 1, Layout_Book = 2,
                      Layout_R2L = 16, Layout_NonContinuous = 32 };

class RenderedBitmap {
protected:
    HBITMAP _hbmp;
    int     _width;
    int     _height;

public:
    // whether this bitmap will (have to) be replaced soon
    bool outOfDate;

    RenderedBitmap(HBITMAP hbmp, int width, int height) :
        _hbmp(hbmp), _width(width), _height(height), outOfDate(false) { }
    ~RenderedBitmap() { DeleteObject(_hbmp); }

    // callers must not delete this (use CopyImage if you have to modify it)
    HBITMAP GetBitmap() const { return _hbmp; }
    SizeI Size() const { return SizeI(_width, _height); }

    // render the bitmap into the target rectangle (streching and skewing as requird)
    void StretchDIBits(HDC hdc, RectI target) {
        HDC bmpDC = CreateCompatibleDC(hdc);
        HGDIOBJ oldBmp = SelectObject(bmpDC, _hbmp);
        SetStretchBltMode(hdc, HALFTONE);
        StretchBlt(hdc, target.x, target.y, target.dx, target.dy,
            bmpDC, 0, 0, _width, _height, SRCCOPY);
        SelectObject(bmpDC, oldBmp);
        DeleteDC(bmpDC);
    }

    void GrayOut(float alpha) {
        HDC hDC = GetDC(NULL);
        BITMAPINFO bmi = { 0 };

        bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
        bmi.bmiHeader.biHeight = _height;
        bmi.bmiHeader.biWidth = _width;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        unsigned char *bmpData = (unsigned char *)malloc(_width * _height * 4);
        if (GetDIBits(hDC, _hbmp, 0, _height, bmpData, &bmi, DIB_RGB_COLORS)) {
            int dataLen = _width * _height * 4;
            for (int i = 0; i < dataLen; i++)
                if ((i + 1) % 4) // don't affect the alpha channel
                    bmpData[i] = (unsigned char)(bmpData[i] * alpha + (alpha > 0 ? 0 : 255));
            SetDIBits(hDC, _hbmp, 0, _height, bmpData, &bmi, DIB_RGB_COLORS);
        }

        free(bmpData);
        ReleaseDC(NULL, hDC);
    }
    void InvertColors() { GrayOut(-1); }

    // create data for a .bmp file from this bitmap (if saved to disk, the HBITMAP
    // can be deserialized with LoadImage(NULL, ..., LD_LOADFROMFILE) and its
    // dimensions determined with GetObject(hbmp, sizeof(BITMAP), ...) )
    unsigned char *Serialize(size_t *cbCount) {
        size_t bmpHeaderLen = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFO);
        size_t bmpDataLen = ((_width * 3 + 3) / 4) * 4 * _height + bmpHeaderLen;
        unsigned char *bmpData = SAZA(unsigned char, bmpDataLen);
        if (!bmpData)
            return NULL;

        BITMAPINFO *bmi = (BITMAPINFO *)(bmpData + sizeof(BITMAPFILEHEADER));
        bmi->bmiHeader.biSize = sizeof(bmi->bmiHeader);
        bmi->bmiHeader.biHeight = _height;
        bmi->bmiHeader.biWidth = _width;
        bmi->bmiHeader.biPlanes = 1;
        bmi->bmiHeader.biBitCount = 24;
        bmi->bmiHeader.biCompression = BI_RGB;

        HDC hDC = GetDC(NULL);
        if (GetDIBits(hDC, _hbmp, 0, _height, bmpData + bmpHeaderLen, bmi, DIB_RGB_COLORS)) {
            BITMAPFILEHEADER *bmpfh = (BITMAPFILEHEADER *)bmpData;
            bmpfh->bfType = MAKEWORD('B', 'M');
            bmpfh->bfOffBits = bmpHeaderLen;
            bmpfh->bfSize = bmpDataLen;
        }
        else {
            free(bmpData);
            bmpData = NULL;
        }
        ReleaseDC(NULL, hDC);

        if (cbCount)
            *cbCount = bmpDataLen;
        return bmpData;
    }
};

// interface to be implemented for saving embedded documents that a link points to
class LinkSaverUI {
public:
    virtual bool SaveEmbedded(unsigned char *data, int cbCount) = 0;
};

// a link destination
class PageDestination {
public:
    // type of the destination (see LinkHandler::GotoLink in WindowInfo.cpp for
    // the supported values; the most common values are "ScrollTo" and "LaunchURL")
    virtual const char *GetType() const = 0;
    // page the destination points to (0 for external destinations such as URLs)
    virtual int GetDestPageNo() const = 0;
    // rectangle of the destination on the above returned page
    virtual RectD GetDestRect() const = 0;
    // string value associated with the destination (e.g. a path or a URL)
    // caller must free() the result
    virtual TCHAR *GetDestValue() const { return NULL; }

    // if this destination's target is an embedded file, this allows to
    // save that file efficiently (the LinkSaverUI might get passed a link
    // to an internal buffer in order to avoid unnecessary memory allocations)
    virtual bool SaveEmbedded(LinkSaverUI &saveUI) { return false; }
};

// use in PageDestination::GetDestRect for values that don't matter
#define DEST_USE_DEFAULT    -999.9

// hoverable (and maybe interactable) element on a single page
class PageElement {
public:
    virtual ~PageElement() { }
    // page this element lives on (0 for elements in a ToC)
    virtual int GetPageNo() const = 0;
    // rectangle that can be interacted with
    virtual RectD GetRect() const = 0;
    // string value associated with this element (e.g. displayed in an infotip)
    // caller must free() the result
    virtual TCHAR *GetValue() const = 0;

    // if this element is a link, this returns information about the link's destination
    // (the result is owned by the PageElement and MUST NOT be deleted)
    virtual PageDestination *AsLink() { return NULL; }
};

// an item in a document's Table of Content
class DocToCItem {
public:
    // the item's visible label
    TCHAR *title;
    // whether any child elements are to be displayed
    bool open;
    // page this item points to (0 for non-page destinations)
    int pageNo;
    // arbitrary number allowing to distinguish this DocToCItem
    // from any other of the same ToC tree
    int id;

    // first child item
    DocToCItem *child;
    // next sibling
    DocToCItem *next;

    DocToCItem(TCHAR *title) :
        title(title), open(true), pageNo(0), id(0), child(NULL), next(NULL) { }

    virtual ~DocToCItem() {
        delete child;
        delete next;
        free(title);
    }

    // returns the destination this ToC item points to
    // (the result is owned by the DocToCItem and MUST NOT be deleted)
    virtual PageDestination *GetLink() = 0;
};

class BaseEngine {
public:
    virtual ~BaseEngine() { }
    // creates a clone of this engine (e.g. for printing on a different thread)
    virtual BaseEngine *Clone() = 0;

    // the name of the file this engine handles
    virtual const TCHAR *FileName() const = 0;
    // number of pages the loaded document contains
    virtual int PageCount() const = 0;

    // the box containing the visible page content (usually RectD(0, 0, pageWidth, pageHeight))
    virtual RectD PageMediabox(int pageNo) = 0;
    // the box inside PageMediabox that actually contains any relevant content
    // (used for auto-cropping in Fit Content mode, can be PageMediabox)
    virtual RectD PageContentBox(int pageNo, RenderTarget target=Target_View) {
        return PageMediabox(pageNo);
    }
    // the angle in degrees the given page is rotated natively (usually 0 deg)
    virtual int PageRotation(int pageNo) { return 0; }

    // renders a page into a cacheable RenderedBitmap
    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View) = 0;
    // renders a page directly into an hDC (e.g. for printing)
    virtual bool RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View) = 0;

    // applies zoom and rotation to a point in user/page space converting
    // it into device/screen space - or in the inverse direction
    virtual PointD Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse=false) = 0;
    virtual RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse=false) = 0;

    // returns the binary data for the current file
    // (e.g. for saving again when the file has already been deleted)
    // caller needs to free() the result
    virtual unsigned char *GetFileData(size_t *cbCount) { return NULL; }
    // whether a document has text content at all (e.g. for hiding search UI)
    virtual bool HasTextContent() = 0;
    // extracts all text found in the given page (and optionally also the
    // coordinates of the individual glyphs)
    // caller needs to free() the result
    virtual TCHAR * ExtractPageText(int pageNo, TCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View) = 0;
    // certain optimizations can be made for a page consisting of a single large image
    virtual bool IsImagePage(int pageNo) = 0;
    // the layout type this document's author suggests (if the user doesn't care)
    virtual PageLayoutType PreferredLayout() { return Layout_Single; }

    // access to various document properties (such as Author, Title, etc.)
    virtual TCHAR *GetProperty(char *name) { return NULL; }

    // TODO: needs a more general interface
    // whether it is allowed to print the current document
    virtual bool IsPrintingAllowed() { return true; }
    // whether it is allowed to extract text from the current document
    // (except for searching an accessibility reasons)
    virtual bool IsCopyingTextAllowed() { return true; }

    // the DPI for a file is needed when converting internal measures to physical ones
    virtual float GetFileDPI() const { return 96.0f; }
    // the default file extension for a document like the currently loaded one (e.g. _T(".pdf"))
    virtual const TCHAR *GetDefaultFileExt() const = 0;

    // returns a list of all available elements for this page
    // caller must delete the result (including all elements contained in the Vec)
    virtual Vec<PageElement *> *GetElements(int pageNo) { return NULL; }
    // returns the element at a given point or NULL if there's none
    // caller must delete the result
    virtual PageElement *GetElementAtPos(int pageNo, PointD pt) { return NULL; }

    // creates a PageDestination from a name (or NULL for invalid names)
    // caller must delete the result
    virtual PageDestination *GetNamedDest(const TCHAR *name) { return NULL; }
    // checks whether this document has an associated Table of Contents
    virtual bool HasToCTree() const { return false; }
    // returns the root element for the loaded document's Table of Contents
    // caller must delete the result (when no longer needed)
    virtual DocToCItem *GetToCTree() { return NULL; }

    // returns a string to remember when the user wants to save a document's password
    // (don't implement for document types that don't support password protection)
    // caller must free() the result
    virtual char *GetDecryptionKey() { return NULL; }
    // tells the engine that this might be a good time to release some memory
    // after having rendered a page (if the Engine caches e.g. shared objects)
    virtual void RunGC() { }

    // loads the given page so that the time required can be measured
    // without also measuring rendering times
    virtual bool BenchLoadPage(int pageNo) = 0;
};

#endif
