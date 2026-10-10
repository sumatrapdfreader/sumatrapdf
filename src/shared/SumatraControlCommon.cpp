/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/UITask.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "DisplayMode.h"
#include "DocumentLayout.h"
#include "DocController.h"
#include "DocProperties.h"
#include "DocumentProperties.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "RenderCache.h"
#include "Commands.h"
#include "CommandAvailability.h"
#include "AppSettings.h"
#include "Flags.h"
#include "SumatraTest.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "AnnotPlacement.h"
#include "WindowTab.h"
#include "TextSelection.h"
#include "Selection.h"
#include "SelectionHandlers.h"
#include "FileHistory.h"
#include "Favorites.h"
#include "PagePosition.h"
#include "SelectionTranslate.h"
#include "ImageSaveCropResize.h"
#include "base/GuessFileType.h"
#include "FindWindow.h"
#include "FindBar.h"
#include "Toolbar.h"
#include "LinkFollow.h"
#include "SelectTextKeyboard.h"
#include "SelectionToolbar.h"
#include "HomePage.h"
#include "Notifications.h"
#include "AIChatCommon.h"
#include "SumatraDialogs.h"
#include "AnnotEditToolbar.h"
#include "AnnotFilterToolbar.h"
#include "Annotation.h"
#include "Menu.h"
#include "EngineAll.h"
#include "EutlTrust.h"
#include "CommandPalette.h"
#include "PdfTools.h"
#include "ReadAloud.h"
#include "ReadingAutoScroll.h"
#include "ReadingBar.h"
#include "NavFilesInFolder.h"
#include "PerfLog.h"
#include "SumatraControl.h"
#include "SumatraControlCommon.h"

// Parts of the -dbg-control channel that orig and ng have in common. ng has no
// listener thread on wasm, so nothing calls them there.
#if !OS_WASM

// Boxes the current page actually declares (issue #814). Optional int arg is pageNo.
TempStr PageBoxesResultTemp(int pageNo, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    if (!win || !win->IsDocLoaded() || !win->ctrl) {
        return finish(StrL("NOTREADY no-doc"), 2);
    }
    DisplayModel* dm = win->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return finish(StrL("ERROR not-fixed-page"), 1);
    }
    if (pageNo < 1) {
        pageNo = win->ctrl->CurrentPageNo();
    }
    if (!win->ctrl->ValidPageNo(pageNo)) {
        return finish(fmt("ERROR bad-page page=%d", pageNo), 1);
    }
    Vec<PdfPageBox> boxes;
    engine->GetPdfPageBoxes(pageNo, boxes);
    str::Builder line;
    line.Append(fmt("OK page=%d show=%d", pageNo, win->showPageBoxes ? 1 : 0));
    for (const PdfPageBox& box : boxes) {
        line.Append(fmt(" %s=%.2f,%.2f,%.2f,%.2f", Str(PdfPageBoxName(box.kind)), box.rect.x, box.rect.y, box.rect.dx,
                        box.rect.dy));
    }
    return finish(ToStrTemp(line), 0);
}

void AppendLayoutRect(str::Builder& out, Str name, bool visible, Rect rect) {
    out.Append(
        fmt("item name=%s visible=%d rect=%d,%d,%d,%d\n", name, visible ? 1 : 0, rect.x, rect.y, rect.dx, rect.dy));
}

// Expand SelectionHandlers placeholders against the current tab's selection
// (discussion #6015 ${selectionPosition}).
TempStr SelectionVarsResultTemp(Str pattern, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };
    if (len(gWindows) == 0 || !gWindows[0]) {
        return finish(StrL("NOTREADY no-window\n"), 2);
    }
    WindowTab* tab = gWindows[0]->CurrentTab();
    bool isTextOnly = false;
    TempStr sel = tab ? GetSelectedTextTemp(tab, StrL("\n"), isTextOnly) : TempStr{};
    if (len(sel) == 0) {
        sel = StrL("");
    }
    if (str::IsEmptyOrWhiteSpace(pattern)) {
        pattern = StrL("${selectionPosition}");
    }
    TempStr expanded = ExpandSelectionVarsTemp(pattern, sel, false, 0, nullptr, tab);
    out.Append(StrL("pattern="));
    out.Append(pattern);
    out.AppendChar('\n');
    out.Append(StrL("expanded="));
    out.Append(expanded);
    out.AppendChar('\n');
    if (tab && tab->selectionOnPage) {
        out.Append(fmt("nrects=%d\n", len(*tab->selectionOnPage)));
        for (SelectionOnPage& onPage : *tab->selectionOnPage) {
            RectF r = onPage.rect;
            out.Append(fmt("rect=%g,%g,%g,%g page=%d\n", r.x, r.y, r.dx, r.dy, onPage.pageNo));
            if (onPage.HasQuad()) {
                QuadF q = onPage.quad;
                out.Append(fmt("quad=%g,%g %g,%g %g,%g %g,%g\n", q.ul.x, q.ul.y, q.ur.x, q.ur.y, q.ll.x, q.ll.y, q.lr.x,
                               q.lr.y));
            }
        }
    } else {
        out.Append(StrL("nrects=0\n"));
    }
    return finish({}, 0);
}

TempStr DocumentSignaturesResultTemp(int* exitCodeOut) {
    auto finish = [exitCodeOut](Str result, int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return str::DupTemp(result);
    };
    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return finish(StrL("NOTREADY no-fixed-document"), 2);
    }
    EutlRegisterLookup();
    Props props;
    engine->GetProperties(props);
    Str sigs = GetPropValueTemp(props, DocProp::Signatures);
    if (len(sigs) == 0) {
        return finish(StrL("ERROR no-signatures"), 1);
    }
    return finish(str::DupTemp(sigs), 0);
}

TempStr DocumentFontListResultTemp(int* exitCodeOut) {
    auto finish = [exitCodeOut](Str result, int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return str::DupTemp(result);
    };
    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return finish(StrL("NOTREADY no-fixed-document"), 2);
    }
    TempStr fonts = engine->GetPropertyTemp(DocProp::FontList);
    if (len(fonts) == 0) {
        return finish(StrL("ERROR no-fonts"), 1);
    }
    return finish(fmt("OK fonts=%s", fonts), 0);
}

TempStr DocumentPropertiesResultTemp(int* exitCodeOut) {
    auto finish = [exitCodeOut](Str result, int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return str::DupTemp(result);
    };
    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return finish(StrL("NOTREADY no-fixed-document"), 2);
    }
    Props props;
    engine->GetProperties(props);
    str::Builder out;
    out.Append(StrL("OK"));
    int n = len(props);
    for (int i = 0; i < n; i++) {
        TempStr name = PropNameTemp(props[i].prop);
        if (len(name) == 0) {
            continue;
        }
        out.Append(StrL("\n"));
        out.Append(name);
        out.Append(StrL("="));
        out.Append(props[i].val);
    }
    // what Save As offers, and the sniffed type Properties shows
    out.Append(fmt("\ndefaultExt=%s", engine->defaultExt));
    FileType ft = GuessFileTypeFromFile(engine->FilePath());
    out.Append(fmt("\nfileTypeExt=%s", GetExtForFileTypeTemp(ft)));
    return finish(ToStrTemp(out), 0);
}

void DeleteControlArg(ControlArg* arg) {
    if (!arg) {
        return;
    }
    free(arg->bytes);
    str::FreePtr(&arg->str);
    if (arg->list) {
        for (ControlArg* el : *arg->list) {
            DeleteControlArg(el);
        }
        delete arg->list;
    }
    delete arg;
}

void AppendU16(str::Builder& s, u16 v) {
    u8 buf[2] = {(u8)(v & 0xff), (u8)((v >> 8) & 0xff)};
    s.Append(Str((char*)buf, (int)sizeof(buf)));
}

void AppendU32(str::Builder& s, u32 v) {
    u8 buf[4] = {(u8)(v & 0xff), (u8)((v >> 8) & 0xff), (u8)((v >> 16) & 0xff), (u8)((v >> 24) & 0xff)};
    s.Append(Str((char*)buf, (int)sizeof(buf)));
}

void AppendArgEnd(str::Builder& s) {
    AppendU16(s, (u16)ControlArgType::End);
}

void AppendArgInt(str::Builder& s, i32 v) {
    AppendU16(s, (u16)ControlArgType::Int32);
    AppendU32(s, (u32)v);
}

void AppendArgString(str::Builder& s, Str str) {
    if (len(str) == 0) {
        str = StrL("");
    }
    size_t n = (size_t)str.len;
    AppendU16(s, (u16)ControlArgType::String);
    AppendU32(s, (u32)n);
    s.Append(str);
    s.AppendChar(0);
}

#endif // !OS_WASM
