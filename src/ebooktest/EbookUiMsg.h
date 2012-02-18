/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */
#ifndef EbookUiMsg_h
#define EbookUiMsg_h

struct PageData;
class  MobiDoc;
class  ThreadLayoutMobi;

struct FinishedMobiLoadingData {
    TCHAR *     fileName;
    MobiDoc *   mobiDoc;
    double      loadingTimeMs;

    void Free() {
        free(fileName);
    }
};

struct FinishedMobiLayoutData {
    Vec<PageData*> *    pages;
    ThreadLayoutMobi *  thread;
};

class UiMsg {
public:
    enum Type {
        FinishedMobiLoading,
        FinishedMobiLayout
    };

    Type type;
    union {
        FinishedMobiLoadingData finishedMobiLoading;
        FinishedMobiLayoutData  finishedMobiLayout;
    };

    UiMsg(Type type) : type(type) {
    }
    ~UiMsg() {
        if (FinishedMobiLoading == type)
            finishedMobiLoading.Free();
    }
};
#endif
