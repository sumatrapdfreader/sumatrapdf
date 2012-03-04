/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */
#ifndef UiMsgSumatra_h
#define UiMsgSumatra_h

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

struct MobiLayoutData {
    enum { MAX_PAGES = 32 };
    PageData *         pages[MAX_PAGES];
    size_t             pageCount;
    bool               fromBeginning;
    bool               finished;
    ThreadLayoutMobi * thread;
};

class UiMsg {
public:
    enum Type {
        FinishedMobiLoading,
        MobiLayout
    };

    Type type;
    union {
        FinishedMobiLoadingData finishedMobiLoading;
        MobiLayoutData          mobiLayout;
    };

    UiMsg(Type type) : type(type) {
    }
    ~UiMsg() {
        if (FinishedMobiLoading == type)
            finishedMobiLoading.Free();
    }
};
#endif
