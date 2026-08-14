/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "gui/UIModels.h"

int ListBoxModelStrings::ItemsCount() {
    return len(strings);
}

Str ListBoxModelStrings::Item(int i) {
    return strings[i];
}
