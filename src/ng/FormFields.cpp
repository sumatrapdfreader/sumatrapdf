/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's FormFields.cpp - clicking a text or choice form field floats an
// edit over it. orig creates a real WC_EDIT / LISTBOX child of the canvas;
// here it is a gpui Input (or a list of the choice's options) placed at the
// field's canvas rect. The field highlight, the /DA font size and the Tab /
// Enter / Escape keys are orig's.

#include "gui/GpuiBridge.h"
#include "base/UITask.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"
#include "VirtKeys.h"

#include <mupdf/pdf.h>

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "Annotation.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "Theme.h"
#include "Translations.h"
#include "Commands.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Toolbar.h"
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"
#include "SumatraDialogs.h"
#include "FormFields.h"

#include "SumatraLog.h"

// One field is edited at a time: either a text box or a choice list floats
// over the page.
struct ActiveFormEdit {
    MainWindow* win = nullptr;
    Annotation* widget = nullptr;
    gpui::InputState* edit = nullptr;
    StrVec options;
    int sel = -1;
    bool multiline = false;
    bool masked = false;
    bool isChoice = false;
    bool wantFocus = false;
};

static ActiveFormEdit gEdit;
static bool gCommitting = false;

static void FreeFormFieldEdit(gpui::InputState* st) {
    delete st;
}

// True while a form field is being edited in place.
bool IsFormFieldEditActive() {
    return gEdit.widget != nullptr;
}

// Acrobat / Chrome pale blue, translucent so the page still shows through.
constexpr Color kFormFieldHighlightCol = MkRgb(166, 202, 240);
constexpr u8 kFormFieldHighlightAlpha = 96;

// Tint empty fillable fields so they are visible without hovering (issue #5966).
void PaintFormFieldHighlights(MainWindow* win, gp::PaintCtx* ctx) {
    if (!gSettings || !gSettings->highlightFormFields || !ctx) {
        return;
    }
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return;
    }
    EngineBase* engine = dm->GetEngine();
    if (!EngineMupdfIsPdf(engine)) {
        return;
    }
    Vec<Rect> screenRects;
    int pageCount = dm->PageCount();
    for (int pageNo = 1; pageNo <= pageCount; pageNo++) {
        PageInfo* pi = dm->GetPageInfo(pageNo);
        if (!pi || !pi->isShown || pi->visibleRatio == 0) {
            continue;
        }
        Vec<RectF> pageRects;
        EngineMupdfGetFormFieldHighlightRects(engine, pageNo, gEdit.widget, pageRects);
        for (RectF& pr : pageRects) {
            Rect rc = dm->CvtToScreen(pageNo, pr);
            if (!rc.IsEmpty()) {
                VecAppend(screenRects, rc);
            }
        }
    }
    if (len(screenRects) > 0) {
        CanvasFillRects(ctx, screenRects.els, len(screenRects), kFormFieldHighlightCol, kFormFieldHighlightAlpha, 0);
    }
}

// Cancel the active form edit if it is for this widget (no save). Safe no-op
// when no edit is active or the widget does not match.
void CancelFormFieldEditIfWidget(Annotation* widget) {
    if (!widget || gEdit.widget != widget) {
        return;
    }
    CommitFormFieldEdit(false);
}

// Commit (save=true) or cancel (save=false) the active form-field edit, if any.
void CommitFormFieldEdit(bool save) {
    if (!gEdit.widget || gCommitting) {
        return;
    }
    gCommitting = true;
    Annotation* widget = gEdit.widget;
    MainWindow* win = gEdit.win;
    bool isChoice = gEdit.isChoice;

    Str text;
    if (save) {
        if (isChoice) {
            if (gEdit.sel >= 0 && gEdit.sel < len(gEdit.options)) {
                text = str::DupTemp(gEdit.options[gEdit.sel]);
            } else {
                save = false; // nothing selected
            }
        } else if (gEdit.edit) {
            text = str::DupTemp(FromGpui(gp::InputValue(gEdit.edit)));
        }
    }
    if (gEdit.edit && win && win->gpuiWin) {
        gp::InputBlur(gEdit.edit, win->gpuiWin->app, win->gpuiWin);
    }
    // ng: the commit runs from the field's own gpui listener, so gpui still
    // holds the InputState for the rest of the event; free it on the next
    // uitask drain instead
    if (gEdit.edit) {
        uitask::Post(MkFunc0(FreeFormFieldEdit, gEdit.edit), "FreeFormFieldEdit");
    }
    gEdit = {};

    bool changed = false;
    if (save && widget) {
        changed = isChoice ? SetWidgetChoiceValue(widget, text) : SetWidgetTextValue(widget, text);
    }
    if (win) {
        if (changed) {
            MainWindowRerender(win);
            // refresh the tab's unsaved-changes (red dot) indicator and toolbar
            // state now, otherwise it only updates on the next repaint trigger
            ToolbarUpdateStateForWindow(win, false);
        }
        AppShellInvalidate(win);
    }
    gCommitting = false;
}

// commit, then move to the next / prev editable field on the page
bool FormFieldEditOnTab(bool back) {
    if (!gEdit.widget) {
        return false;
    }
    Annotation* cur = gEdit.widget;
    MainWindow* win = gEdit.win;
    CommitFormFieldEdit(true);
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    // cur may be dead if commit triggered a document reload; only walk to the
    // next field when the widget is still live
    if (dm && AnnotationIsLive(cur)) {
        Annotation* next = EngineMupdfGetAdjacentWidget(dm->GetEngine(), cur, !back);
        if (next) {
            StartFormFieldEdit(win, next);
        }
    }
    return true;
}

// the field's on-screen font height in pixels: the /DA font size (PDF points)
// scaled to the page's current zoom, or a height-derived fallback for
// auto-sized (/DA size 0) fields.
static int FieldFontPx(Annotation* widget, Rect rc) {
    float daSize = GetWidgetFontSize(widget);
    float pageDy = widget->bounds.dy; // field height in page (PDF) units
    if (daSize > 0 && pageDy > 0) {
        float scale = (float)rc.dy / pageDy; // screen px per PDF unit
        return std::max(8, (int)(daSize * scale));
    }
    return std::max(8, (int)((float)rc.dy * 0.7f));
}

static bool FieldTextWithinLimit(gp::Str text, int64_t maxLen) {
    return Utf8CodepointCount(FromGpui(text)) <= (int)maxLen;
}

// Clicking a signature field the document's author left unsigned opens Sign
// Document with that field selected. Signed fields are left alone (clicking one
// shouldn't offer to overwrite it), and so is everything else (issue #5964).
bool StartSignatureFieldSigning(MainWindow* win, Annotation* widget) {
    if (!win || !AnnotationIsLive(widget)) {
        return false;
    }
    if (GetWidgetType(widget) != PDF_WIDGET_TYPE_SIGNATURE) {
        return false;
    }
    if (GetWidgetFieldFlags(widget) & PDF_FIELD_IS_READ_ONLY) {
        return false;
    }
    TempStr fieldName;
    if (!IsUnsignedSignatureWidget(widget, &fieldName)) {
        return false;
    }
    logf("StartSignatureFieldSigning: '%s'\n", fieldName);
    ShowSignDocumentDialog(win, fieldName, true);
    return true;
}

bool StartFormFieldEdit(MainWindow* win, Annotation* widget) {
    if (!win || !win->gpuiWin || !AnnotationIsLive(widget)) {
        return false;
    }
    int wt = GetWidgetType(widget);
    bool isText = (wt == PDF_WIDGET_TYPE_TEXT);
    bool isChoice = (wt == PDF_WIDGET_TYPE_COMBOBOX) || (wt == PDF_WIDGET_TYPE_LISTBOX);
    if (!isText && !isChoice) {
        return false;
    }
    int flags = GetWidgetFieldFlags(widget);
    if (flags & PDF_FIELD_IS_READ_ONLY) {
        return false;
    }
    CommitFormFieldEdit(true); // commit any prior edit

    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return false;
    }
    Rect rc = dm->CvtToScreen(widget->pageNo, widget->bounds); // canvas coords
    // scroll the field into view if it's off-screen (e.g. Tab moved past the
    // fold), then recompute its on-screen rect
    if (dm->ScrollScreenToRect(widget->pageNo, rc)) {
        rc = dm->CvtToScreen(widget->pageNo, widget->bounds);
    }
    if (rc.dx < 4 || rc.dy < 4) {
        return false;
    }

    gEdit.win = win;
    gEdit.widget = widget;
    gEdit.isChoice = isChoice;
    gEdit.multiline = (flags & PDF_TX_FIELD_IS_MULTILINE) != 0;
    gEdit.masked = (flags & PDF_TX_FIELD_IS_PASSWORD) != 0;
    gEdit.wantFocus = true;
    gEdit.sel = -1;
    gEdit.options.Reset();
    if (isChoice) {
        GetWidgetChoiceOptions(widget, gEdit.options);
        if (len(gEdit.options) == 0) {
            gEdit = {};
            return false;
        }
        Str cur = GetWidgetValue(widget);
        for (int i = 0; i < len(gEdit.options); i++) {
            if (str::Eq(gEdit.options[i], cur)) {
                gEdit.sel = i;
                break;
            }
        }
    } else {
        gEdit.edit = new gp::InputState();
        gEdit.edit->focus = gp::FocusHandleNew(win->gpuiWin->app);
        Str value = GetWidgetValue(widget);
        int maxLen = GetWidgetMaxLen(widget);
        if (maxLen > 0) {
            value = Utf8SliceByCodepoints(value, 0, maxLen);
            gEdit.edit->validate = FieldTextWithinLimit;
            gEdit.edit->validateArg = maxLen;
        }
        gp::InputSetValue(gEdit.edit, ToGpui(value));
    }
    logf("StartFormFieldEdit: page %d, choice %d\n", widget->pageNo, isChoice ? 1 : 0);
    AppShellInvalidate(win);
    return true;
}

// --- the gpui element -------------------------------------------------------

struct FormFieldView {
    static void OnInput(FormFieldView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnOption(FormFieldView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx);
};

static gp::Entity<FormFieldView> gFormFieldView;

void FormFieldView::OnInput(FormFieldView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind != gp::InputEventKind::PressEnter) {
        return;
    }
    if (!gEdit.multiline) {
        CommitFormFieldEdit(true);
        gp::Notify(cx);
    }
}

bool FormFieldEditOnKeyDown(int vk) {
    if (!gEdit.widget || !gEdit.isChoice) {
        return false;
    }
    int n = len(gEdit.options);
    int sel = gEdit.sel;
    switch (vk) {
        case VK_UP:
            sel = std::max(0, sel - 1);
            break;
        case VK_DOWN:
            sel = std::min(n - 1, sel + 1);
            break;
        case VK_HOME:
            sel = 0;
            break;
        case VK_END:
            sel = n - 1;
            break;
        case VK_RETURN:
            CommitFormFieldEdit(true);
            return true;
        default:
            return false;
    }
    gEdit.sel = sel;
    AppShellInvalidate(gEdit.win);
    return true;
}

void FormFieldView::OnOption(FormFieldView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx) {
    gEdit.sel = (int)idx;
    CommitFormFieldEdit(true);
    gp::Notify(cx);
}

gp::El* FormFieldEditBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gEdit.widget || gEdit.win != win) {
        return nullptr;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm || !AnnotationIsLive(gEdit.widget) || !dm->PageVisible(gEdit.widget->pageNo)) {
        CommitFormFieldEdit(true);
        return nullptr;
    }
    if (!gFormFieldView.IsValid()) {
        gFormFieldView = gp::EntityNewState<FormFieldView>(cx->app);
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    Rect rc = dm->CvtToScreen(gEdit.widget->pageNo, gEdit.widget->bounds);
    float k = CanvasScale(win);
    float x = (float)rc.x * k;
    float y = (float)rc.y * k;
    float w = (float)rc.dx * k;
    float h = (float)rc.dy * k;
    float fontPx = (float)FieldFontPx(gEdit.widget, rc) * k;

    if (!gEdit.isChoice) {
        gEdit.edit->onChange = gp::ListenTo(gFormFieldView, &FormFieldView::OnInput);
        gp::El* box = gp::Div(cx->a)->Absolute()->Left(x)->Top(y)->W(w)->H(h);
        box->Child(gpc::Input::New(cx, GStrL("form-field"), gEdit.edit)
                       ->WithSize(gp::UiSize::Small)
                       ->Masked(gEdit.masked)
                       ->W(gp::kFill)
                       ->IntoEl()
                       ->H(h)
                       ->Font(fontPx)
                       ->FontFamily(GStrL("Arial")));
        if (gEdit.wantFocus) {
            gEdit.wantFocus = false;
            gp::InputFocus(gEdit.edit, cx->app, cx->win);
            gp::InputSelectAll(gEdit.edit, cx->app, cx->win);
        }
        return box;
    }

    // drop down just below the field, or above if it would fall off the canvas
    float itemDy = fontPx + (float)DpiScale(6);
    int visN = std::min(len(gEdit.options), 8);
    float listDy = (visN * itemDy) + 4;
    float listDx = std::max(w, (float)DpiScale(120));
    float ly = y + h;
    if (ly + listDy > (float)win->canvasRc.dy && y - listDy >= 0) {
        ly = y - listDy;
    }
    gp::El* list = gp::Div(cx->a)
                       ->Absolute()
                       ->Left(x)
                       ->Top(ly)
                       ->W(listDx)
                       ->MaxH(listDy)
                       ->FlexCol()
                       ->Bg(th.tokens.popover)
                       ->Border(1, th.border)
                       ->ClipY();
    for (int i = 0; i < len(gEdit.options); i++) {
        TempStr id = fmt("form-opt-%d", i);
        gp::El* row = gp::Div(cx->a)
                          ->FlexRow()
                          ->ItemsCenter()
                          ->W(gp::kFill)
                          ->H(itemDy)
                          ->PadX(6)
                          ->Cursor(gp::CursorKind::Pointer)
                          ->HoverBg(th.tokens.accent)
                          ->PathClick(GpuiDup(cx->a, id))
                          ->OnClick(gp::ListenTo(gFormFieldView, &FormFieldView::OnOption, (intptr_t)i));
        if (i == gEdit.sel) {
            row->Bg(th.tokens.accent);
        }
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, gEdit.options[i]))
                       ->Font(fontPx)
                       ->FontFamily(GStrL("Arial"))
                       ->Fg(th.popoverFg)
                       ->Truncate());
        list->Child(row);
    }
    return list;
}

TempStr FormFieldEditStateTemp() {
    if (!gEdit.widget) {
        return str::DupTemp(StrL("formEdit active=0 choice=0 value=\n"));
    }
    Str value;
    if (gEdit.isChoice) {
        value = (gEdit.sel >= 0 && gEdit.sel < len(gEdit.options)) ? gEdit.options[gEdit.sel] : Str{};
    } else if (gEdit.edit) {
        value = FromGpui(gp::InputValue(gEdit.edit));
    }
    return fmt("formEdit active=1 choice=%d page=%d value=%s\n", gEdit.isChoice ? 1 : 0, gEdit.widget->pageNo, value);
}
