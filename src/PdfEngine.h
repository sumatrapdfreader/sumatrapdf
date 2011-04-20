/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef PdfEngine_h
#define PdfEngine_h

#include "BaseEngine.h"
#include "Vec.h"

class PageDestination;

class PageElement {
public:
    virtual ~PageElement() { }
    virtual RectD GetRect() const = 0;
    virtual TCHAR *GetValue() const = 0;
    virtual int GetPageNo() const = 0;
    virtual PageDestination *AsLink() { return NULL; }
};

typedef struct fz_obj_s fz_obj;

class PageDestination : public PageElement {
public:
    virtual const char *GetType() const = 0;
    // TODO: generalize a destination
    virtual fz_obj *dest() const = 0;
};

class PdfTocItem {
public:
    TCHAR *title;
    bool open;
    int pageNo;
    int id;

    PdfTocItem *child;
    PdfTocItem *next;

    PdfTocItem(TCHAR *title) :
        title(title), open(true), pageNo(0), id(0), child(NULL), next(NULL) { }

    virtual ~PdfTocItem() {
        delete child;
        delete next;
        free(title);
    }

    virtual PageDestination *GetLink() = 0;
};

class PasswordUI {
public:
    virtual TCHAR * GetPassword(const TCHAR *fileName, unsigned char *fileDigest,
                                unsigned char decryptionKeyOut[32], bool *saveKey) = 0;
};

class LinkSaverUI {
public:
    virtual bool SaveEmbedded(unsigned char *data, int cbCount) = 0;
};

class PdfEngine : public BaseEngine {
public:
    // TODO: move any of the following into BaseEngine?

    virtual Vec<PageElement *> *GetElements(int pageNo) = 0;
    virtual PageElement *GetElementAtPos(int pageNo, PointD pt) = 0;

    virtual int FindPageNo(fz_obj *dest) = 0;
    virtual fz_obj *GetNamedDest(const TCHAR *name) = 0;
    virtual bool HasToCTree() const = 0;
    virtual PdfTocItem *GetToCTree() = 0;
    virtual bool SaveEmbedded(fz_obj *obj, LinkSaverUI& saveUI) = 0;

    virtual char *GetDecryptionKey() const = 0;
    virtual void RunGC() = 0;

protected:
    virtual bool load(const TCHAR *fileName, PasswordUI *pwdUI=NULL) = 0;
    virtual bool load(IStream *stream, PasswordUI *pwdUI=NULL) = 0;

public:
    static PdfEngine *CreateFromFileName(const TCHAR *fileName, PasswordUI *pwdUI=NULL);
    static PdfEngine *CreateFromStream(IStream *stream, PasswordUI *pwdUI=NULL);
};

class XpsEngine : public BaseEngine {
public:
    virtual Vec<PageElement *> *GetElements(int pageNo) = 0;
    virtual PageElement *GetElementAtPos(int pageNo, PointD pt) = 0;

protected:
    virtual bool load(const TCHAR *fileName) = 0;
    virtual bool load(IStream *stream) = 0;

public:
    static XpsEngine *CreateFromFileName(const TCHAR *fileName);
    static XpsEngine *CreateFromStream(IStream *stream);
};

#endif
