/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// The plain-text viewer behind Show PDF Info, Show Document Table Of Contents,
// Show Errors and -list-printers. Like orig, this is a separate top-level
// window containing a read-only multi-line editor.

#include "gui/GpuiBridge.h"

#include "MainWindow.h"
#include "gui/AppShell.h"
#include "TextViewWnd.h"

#include "SumatraLog.h"

struct TextViewView {
    gp::InputState text;

    static gp::El* Render(TextViewView* self, gp::Ctx* cx);
    static void OnKey(TextViewView* self, gp::Ctx* cx, const gp::KeyEvent* ev);
};

static gp::Entity<TextViewView> gTextViewView;
static gp::Window* gTextViewWin = nullptr;
static Str gTextViewTitle;

enum class TextViewReset {
    KeepWindow,
    CloseWindow,
};

static void ResetTextView(TextViewReset reset) {
    gp::Window* window = gTextViewWin;
    gp::App* app = window ? window->app : AppShellGetApp();
    gTextViewWin = nullptr;
    str::FreePtr(&gTextViewTitle);
    if (reset == TextViewReset::CloseWindow && window && window->running) {
        gp::AppQuit(window);
    }
    if (app && gTextViewView.IsValid()) {
        gp::EntityDrop(app, gTextViewView.id);
    }
    gTextViewView = {};
}

bool IsTextViewWindowVisible() {
    if (gTextViewWin && gTextViewWin->running) {
        return true;
    }
    ResetTextView(TextViewReset::KeepWindow);
    return false;
}

bool IsTextViewWindowTitle(Str title) {
    return IsTextViewWindowVisible() && str::Eq(gTextViewTitle, title);
}

void SetTextViewWindowText(Str text) {
    if (!IsTextViewWindowVisible()) {
        return;
    }
    TextViewView* view = gTextViewView.Get(gTextViewWin->app);
    if (!view) {
        return;
    }
    gp::InputSetValue(&view->text, ToGpui(text));
    gp::AppInvalidate(gTextViewWin);
}

void CloseTextViewWindow() {
    ResetTextView(TextViewReset::CloseWindow);
}

gp::El* TextViewView::Render(TextViewView* self, gp::Ctx* cx) {
    cx->win->input = &self->text;
    gp::WinSize size = gp::WindowSize(cx->win);
    const gp::Theme& theme = gp::ThemeNow(cx->app);
    gp::El* editor = gpc::Textarea::New(cx, GStrL("text-view"), &self->text)
                         ->H(size.dipH)
                         ->SoftWrap(false)
                         ->Readonly()
                         ->IntoEl()
                         ->Mono();
    return gp::Div(cx->a)->SizeFull()->Bg(theme.tokens.background)->Child(editor);
}

void TextViewView::OnKey(TextViewView*, gp::Ctx* cx, const gp::KeyEvent* ev) {
    if (ev->down && ev->vk == gp::KeyEscape) {
        // Keep this entity alive until the key listener returns. The next
        // visibility check or ShowTextInWindow call releases it.
        gp::AppQuit(cx->win);
    }
}

void ShowTextInWindow(MainWindow* win, Str title, Str text) {
    if (win && !IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    gp::App* app = AppShellGetApp();
    if (!app) {
        return;
    }
    CloseTextViewWindow();

    gTextViewView = gp::EntityNew<TextViewView>(app);
    TextViewView* view = gTextViewView.Get(app);
    view->text.kind = gp::InputKind::Textarea;
    view->text.mode.kind = gp::LayoutModeKind::PlainText;
    view->text.mode.tabSize = 4;
    view->text.softWrap = false;
    view->text.readonly = true;
    gp::InputSetValue(&view->text, ToGpui(text));

    gTextViewWin = gp::KitOpenWindow(app, ToGpui(title), 800, 600, gTextViewView.id, gp::WinOpts{});
    if (!gTextViewWin) {
        ResetTextView(TextViewReset::KeepWindow);
        return;
    }
    gTextViewTitle = str::Dup(title);
    gp::WindowOnKey(gTextViewWin, gp::ListenTo(gTextViewView, &TextViewView::OnKey));
    logf("ShowTextInWindow: '%s', %d bytes\n", title, len(text));
}

void ShowTextInWindowDialog(Str title, Str text) {
    ShowTextInWindow(nullptr, title, text);
}
