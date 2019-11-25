/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/BitManip.h"
#include "utils/ScopedWin.h"

#include "wingui/WinGui.h"
#include "wingui/TreeModel.h"
#include "wingui/TreeCtrl.h"

#include "BaseEngine.h"

void SerializeBookmarksRec(DocTocItem* node, int level, str::Str<char>& s) {
    if (level == 0) {
        s.Append(":default bookmarks view\n");
    }

    while (node) {
        for (int i = 0; i < level; i++) {
            s.Append("  ");
        }
        WCHAR* title = node->Text();
        auto titleA = str::conv::ToUtf8(title);
        s.AppendView(titleA.AsView());
        auto flags = node->fontFlags;
        if (bit::IsSet(flags, fontBitItalic)) {
            s.Append(" font:italic");
        }
        if (bit::IsSet(flags, fontBitBold)) {
            s.Append(" font:bold");
        }
        if (node->color != ColorUnset) {
            s.Append(" ");
            SerializeColor(node->color, s);
        }
        PageDestination* dest = node->GetLink();
        if (dest) {
            int pageNo = dest->GetDestPageNo();
            s.AppendFmt(" page:%d", pageNo);
            auto ws = dest->GetDestValue();
            if (ws != nullptr) {
                auto str = str::conv::ToUtf8(ws);
                s.Append(",dest:");
                s.AppendView(str.AsView());
            }
        }
        s.Append("\n");

        SerializeBookmarksRec(node->child, level + 1, s);
        node = node->next;
    }
}
