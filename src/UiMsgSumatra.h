/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */
#ifndef UiMsgSumatra_h
#define UiMsgSumatra_h

#include "EbookController.h"

class UiMsg {
public:
    enum Type {
        FinishedMobiLoading,
        MobiLayout
    };

    Type type;
    union {
        FinishedMobiLoadingData finishedMobiLoading;
        EbookFormattingData          mobiLayout;
    };

    UiMsg(Type type) : type(type) {
    }
    ~UiMsg() {
        if (FinishedMobiLoading == type)
            finishedMobiLoading.Free();
    }
};

#endif
