/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/VecSegmented.h"
#include "Actions.h"

VecSegmented<Action> gActions;

Action* GetActionByClass(enum Actions action) {
    return gActions.AtPtr((int)action);
}

Action* GetActionByName(const char* name) {
    for (Action* a : gActions) {
        if (str::EqI(a->name, name)) {
            return a;
        }
    }
    return nullptr;
}
