/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */
#ifndef EbookUiMsg_h
#define EbookUiMsg_h

class MobiDoc;

struct FinishedMobiLoadingData {
    TCHAR *     fileName;
    MobiDoc *   mobiDoc;
    double      loadingTimeMs;

    void Free() {
        free(fileName);
    }
};

class UiMsg {
public:
    enum Type {
        FinishedMobiLoading
    };

    Type type;
    union {
        FinishedMobiLoadingData finishedMobiLoading;
    };

    UiMsg(Type type) : type(type) {
    }
    ~UiMsg() {
        if (FinishedMobiLoading == type)
            finishedMobiLoading.Free();
    }
};
#endif
