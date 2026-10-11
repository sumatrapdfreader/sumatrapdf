/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by SelectionToolbarCommon.cpp and each app's SelectionToolbar.cpp ---

struct SelectionToolbarButton {
    int cmdId = 0;
    Str label; // English literal, translated for button text or an icon tooltip
    // A selection handler's SelectToolbarNameOrSvg can also supply user text or
    // an SVG icon. For an icon, userLabel is its Name-based tooltip.
    Str userLabel;
    Str svgIcon;
    // orig: svgIcon rendered at the current size; not owned
    Pixmap* icon = nullptr;
    Pixmap* iconDisabled = nullptr;
    bool enabled = true;
};
const SelectionToolbarButton* FindCandidateButton(int cmdId);
void CollectBuiltInSelectionToolbarCmds(Vec<int>& out);
void NormalizeSelectionToolbarSeparators(Vec<SelectionToolbarButton>& buttons);
bool IsActivelySelecting(MainWindow* win);

void InitButtons(Vec<SelectionToolbarButton>& buttons, MainWindow* win);

// implemented by each app
void AppendSelectionHandlerButtons(Vec<SelectionToolbarButton>& buttons, const AppCommandCtx& ctx);
Vec<SelectionToolbarButton>* SelectionToolbarButtons(MainWindow* win);
Rect SelectionCanvasRect(MainWindow* win, DisplayModel* dm);
void InvokeSelectionToolbarCommand(MainWindow* win, int cmdId);

Str ButtonLabel(const SelectionToolbarButton& b);
bool GetSelectionBounds(MainWindow* win, Rect& out);

constexpr int kCopiedNotifTimeoutMs = 1500;

static Kind kNotifCopiedToClipboard = "notifCopiedToClipboard";
