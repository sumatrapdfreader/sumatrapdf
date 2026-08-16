/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/PlatformWindow.h"

#include "Commands.h"
#include "KeyboardHelp.h"

struct KbCommandDef {
    int id;
    const char* defaultShortcut;
};

// clang-format off
static const KbCommandDef kSecNav[] = {
    {CmdScrollUp, "Up, K"}, {CmdScrollDown, "Down, J"}, {CmdScrollLeft, "Left, H"}, {CmdScrollRight, "Right, L"},
    {CmdScrollUpPage, "Page Up"}, {CmdScrollDownPage, "Page Down"},
    {CmdGoToNextPage, "N"}, {CmdGoToPrevPage, "P"},
    {CmdGoToFirstPage, "Home"}, {CmdGoToLastPage, "End"}, {CmdGoToPage, "Ctrl + G"},
    {CmdNavigateBack, "Alt + Left"}, {CmdNavigateForward, "Alt + Right"}, {0, nullptr},
};
static const KbCommandDef kSecView[] = {
    {CmdZoomIn, "Ctrl + +"}, {CmdZoomOut, "Ctrl + -"},
    {CmdZoomFitPage, "Ctrl + 0"}, {CmdZoomFitWidth, "Ctrl + 2"}, {CmdZoomActualSize, "Ctrl + 1"},
    {CmdToggleZoom, "Z"}, {CmdSinglePageView, "Ctrl + 6"}, {CmdFacingView, "Ctrl + 7"},
    {CmdBookView, "Ctrl + 8"}, {CmdToggleContinuousView, "C"},
    {CmdRotateLeft, "["}, {CmdRotateRight, "]"}, {CmdToggleFullscreen, "F"},
    {CmdTogglePresentationMode, "F5"}, {CmdInvertColors, "Shift + I"}, {0, nullptr},
};
static const KbCommandDef kSecDoc[] = {
    {CmdOpenFile, "Ctrl + O"}, {CmdSaveAs, "Ctrl + S"}, {CmdPrint, "Ctrl + P"}, {CmdReloadDocument, "R"},
    {CmdClose, "Ctrl + W"}, {CmdNewWindow, "Ctrl + N"}, {CmdOpenNextFileInFolder, "Ctrl + Shift + Right"},
    {CmdOpenPrevFileInFolder, "Ctrl + Shift + Left"}, {CmdRenameFile, "F2"}, {CmdProperties, "Ctrl + D"}, {0, nullptr},
};
static const KbCommandDef kSecFind[] = {
    {CmdFindFirst, "Ctrl + F"}, {CmdFindNext, "F3"}, {CmdFindPrev, "Shift + F3"},
    {CmdSelectAll, "Ctrl + A"}, {CmdCopySelection, "Ctrl + C"}, {CmdSelectTextViaKeyboard, "F7"},
    {CmdToggleKeyboardLinkFollowing, "Shift + F"}, {0, nullptr},
};
static const KbCommandDef kSecTabs[] = {
    {CmdNextTabSmart, "Ctrl + Tab"}, {CmdNextTab, "Ctrl + Page Down"}, {CmdPrevTab, "Ctrl + Page Up"},
    {CmdMoveTabLeft, "Ctrl + Shift + Page Up"}, {CmdMoveTabRight, "Ctrl + Shift + Page Down"},
    {CmdReopenLastClosedFile, "Ctrl + Shift + T"}, {0, nullptr},
};
static const KbCommandDef kSecAnnot[] = {
    {CmdCreateAnnotHighlight, "A"}, {CmdCreateAnnotUnderline, "U"}, {CmdSaveAnnotations, "Ctrl + Shift + S"},
    {CmdDeleteAnnotation, "Ctrl + Delete"}, {0, nullptr},
};
static const KbCommandDef kSecIface[] = {
    {CmdToggleBookmarks, "F12"}, {CmdToggleToolbar, "F8"}, {CmdToggleMenuBar, "F9"},
    {CmdToggleCursorPosition, "M"}, {CmdTogglePageInfo, "I"}, {CmdCommandPalette, "Ctrl + K"},
    {CmdFavoriteAdd, "Ctrl + B"}, {CmdFavoriteToggle, ""}, {CmdHelpOpenManual, "F1"},
    {CmdToggleKeyboardHelp, "?"}, {0, nullptr},
};
// clang-format on

struct KbSectionDef {
    const char* title;
    const KbCommandDef* commands;
};

static const KbSectionDef kSections[] = {
    {"Navigation", kSecNav}, {"View & Zoom", kSecView},  {"Document", kSecDoc},    {"Find & Select", kSecFind},
    {"Tabs", kSecTabs},      {"Annotations", kSecAnnot}, {"Interface", kSecIface},
};

static const KbCommandDef* FindCommandDef(int cmdId) {
    for (const KbSectionDef& section : kSections) {
        for (const KbCommandDef* command = section.commands; command->id; command++) {
            if (command->id == cmdId) {
                return command;
            }
        }
    }
    return nullptr;
}

struct DefaultKeyboardHelpDataSource : KeyboardHelpDataSource {
    Str Translate(Str s) override { return s; }

    TempStr CommandDescriptionTemp(int cmdId) override { return str::DupTemp(GetCommandDescription(cmdId)); }

    TempStr CommandShortcutTemp(int cmdId, int) override {
        const KbCommandDef* command = FindCommandDef(cmdId);
        return command ? str::DupTemp(command->defaultShortcut) : TempStr{};
    }
};

static DefaultKeyboardHelpDataSource gDefaultDataSource;

KeyboardHelpDataSource* GetDefaultKeyboardHelpDataSource() {
    return &gDefaultDataSource;
}

struct KbRow {
    Str keys;
    Str description;
    StrVec tokens;
    Rect keysRect;
    Rect descriptionRect;
};

struct KbSection {
    Str title;
    Vec<KbRow*> rows;
    Rect titleRect;
    int estimatedHeight = 0;
    int column = 0;
};

struct KeyboardHelpWindow {
    PlatformWindow* window = nullptr;
    NativeWnd parent = nullptr;
    KeyboardHelpDataSource* dataSource = nullptr;
    bool parentFullscreen = false;
    bool closeHovered = false;
    bool closePressed = false;

    PlatformFont* fontTitle = nullptr;
    PlatformFont* fontHeader = nullptr;
    PlatformFont* fontRow = nullptr;
    Str title;
    Str footer;
    Vec<KbSection*> sections;

    Rect titleRect;
    Rect closeRect;
    Rect separatorRect;
    Rect footerRect;
    int contentTop = 0;
    int capPadX = 0;
    int capGap = 0;
    int capRadius = 0;

    ~KeyboardHelpWindow();
    bool Create(const KeyboardHelpArgs&);
    void CollectContent();
    Size LayoutContent();
    void Paint(PlatformWindowPaintEvent*);
    void OnPointer(PlatformPointerEvent*);
    void OnKey(PlatformKeyEvent*);
};

static KeyboardHelpWindow* gKeyboardHelpWindow = nullptr;

static void FreeSection(KbSection* section) {
    if (!section) {
        return;
    }
    str::Free(section->title);
    for (KbRow* row : section->rows) {
        str::Free(row->keys);
        str::Free(row->description);
        delete row;
    }
    delete section;
}

KeyboardHelpWindow::~KeyboardHelpWindow() {
    delete window;
    window = nullptr;
    str::Free(title);
    str::Free(footer);
    for (KbSection* section : sections) {
        FreeSection(section);
    }
}

static void SafeDeleteKeyboardHelpWindow() {
    if (!gKeyboardHelpWindow) {
        return;
    }
    KeyboardHelpWindow* help = gKeyboardHelpWindow;
    NativeWnd parent = help->parent;
    gKeyboardHelpWindow = nullptr;
    delete help;
    PlatformWindowActivateIfForeground(parent);
}

static void ScheduleCloseKeyboardHelp() {
    if (gKeyboardHelpWindow) {
        PlatformPostTask(MkFunc0Void(SafeDeleteKeyboardHelpWindow));
    }
}

void KeyboardHelpWindow::CollectContent() {
    title = str::Dup(dataSource->Translate(StrL("Keyboard Shortcuts")));
    footer = str::Dup(dataSource->Translate(StrL("Press ? to close")));
    int rowHeight = PlatformFontLineHeight(fontRow) + DpiScale(8);
    int headerHeight = PlatformFontLineHeight(fontHeader) + DpiScale(8);
    int sectionGap = DpiScale(14);

    for (const KbSectionDef& definition : kSections) {
        auto* section = new KbSection();
        section->title = str::Dup(dataSource->Translate(Str(definition.title)));
        for (const KbCommandDef* command = definition.commands; command->id; command++) {
            TempStr keys = dataSource->CommandShortcutTemp(command->id, 2);
            TempStr description = dataSource->CommandDescriptionTemp(command->id);
            if (len(keys) == 0 || len(description) == 0) {
                continue;
            }
            auto* row = new KbRow();
            row->keys = str::Dup(keys);
            row->description = str::Dup(description);
            Split(&row->tokens, row->keys, StrL(", "), false, 4);
            section->rows.Append(row);
        }
        if (len(section->rows) == 0) {
            FreeSection(section);
            continue;
        }
        section->estimatedHeight = headerHeight + len(section->rows) * rowHeight + sectionGap;
        sections.Append(section);
    }

    int totalHeight = 0;
    for (KbSection* section : sections) {
        totalHeight += section->estimatedHeight;
    }
    int bestSplit = len(sections);
    int bestBalance = totalHeight;
    int prefix = 0;
    for (int i = 1; i < len(sections); i++) {
        prefix += sections[i - 1]->estimatedHeight;
        int balance = std::max(prefix, totalHeight - prefix);
        if (balance < bestBalance) {
            bestBalance = balance;
            bestSplit = i;
        }
    }
    for (int i = 0; i < len(sections); i++) {
        sections[i]->column = i < bestSplit ? 0 : 1;
    }
}

Size KeyboardHelpWindow::LayoutContent() {
    int pad = DpiScale(20);
    int columnGap = DpiScale(16);
    int keysDescriptionGap = DpiScale(12);
    int rowGap = DpiScale(8);
    int sectionGap = DpiScale(14);
    int headerHeight = PlatformFontLineHeight(fontHeader) + DpiScale(8);
    int rowHeight = PlatformFontLineHeight(fontRow) + DpiScale(5);
    capPadX = DpiScale(7);
    capGap = DpiScale(5);
    capRadius = DpiScale(5);

    int keyWidths[2] = {0, 0};
    int descriptionWidths[2] = {0, 0};
    int headerWidths[2] = {0, 0};
    for (KbSection* section : sections) {
        int column = section->column;
        headerWidths[column] = std::max(headerWidths[column], PlatformFontMeasureText(fontHeader, section->title).dx);
        for (KbRow* row : section->rows) {
            int keysDx = 0;
            for (int i = 0; i < row->tokens.size; i++) {
                keysDx += PlatformFontMeasureText(fontRow, row->tokens.At(i)).dx + 2 * capPadX;
            }
            keysDx += std::max(row->tokens.size - 1, 0) * capGap;
            keyWidths[column] = std::max(keyWidths[column], keysDx);
            descriptionWidths[column] =
                std::max(descriptionWidths[column], PlatformFontMeasureText(fontRow, row->description).dx);
        }
    }

    int columnWidths[2];
    for (int column = 0; column < 2; column++) {
        columnWidths[column] =
            std::max(headerWidths[column], keyWidths[column] + keysDescriptionGap + descriptionWidths[column]);
    }
    int width = 2 * pad + columnWidths[0];
    if (columnWidths[1] > 0) {
        width += columnGap + columnWidths[1];
    }

    int titleHeight = std::max(PlatformFontLineHeight(fontTitle), DpiScale(24));
    titleRect = {pad, pad, width - 2 * pad - titleHeight - DpiScale(8), titleHeight};
    closeRect = {width - pad - titleHeight, pad, titleHeight, titleHeight};
    separatorRect = {pad, pad + titleHeight + DpiScale(6), width - 2 * pad, DpiScale(1)};
    contentTop = separatorRect.Bottom() + DpiScale(10);

    int columnX[2] = {pad, pad + columnWidths[0] + columnGap};
    int columnY[2] = {contentTop, contentTop};
    bool firstSection[2] = {true, true};
    for (KbSection* section : sections) {
        int column = section->column;
        int& y = columnY[column];
        if (!firstSection[column]) {
            y += sectionGap;
        }
        firstSection[column] = false;
        section->titleRect = {columnX[column], y, columnWidths[column], headerHeight};
        y += headerHeight;
        for (KbRow* row : section->rows) {
            y += rowGap;
            row->keysRect = {columnX[column], y, keyWidths[column], rowHeight};
            row->descriptionRect = {columnX[column] + keyWidths[column] + keysDescriptionGap, y,
                                    descriptionWidths[column], rowHeight};
            y += rowHeight;
        }
    }

    int footerY = std::max(columnY[0], columnY[1]) + DpiScale(12);
    footerRect = {pad, footerY, width - 2 * pad, PlatformFontLineHeight(fontRow)};
    int height = footerRect.Bottom() + pad;
    return {width, height};
}

static Rect PositionHelpWindow(NativeWnd parent, bool fullscreen, Size size) {
    Rect work = PlatformWindowWorkArea(parent);
    if (work.IsEmpty()) {
        work = {0, 0, std::max(size.dx, 1920), std::max(size.dy, 1080)};
    }
    Rect frame = PlatformWindowRect(parent);
    if (!parent || frame.IsEmpty()) {
        return {work.x + (work.dx - size.dx) / 2, work.y + (work.dy - size.dy) / 2, size.dx, size.dy};
    }
    if (fullscreen || PlatformWindowIsMaximized(parent)) {
        int x = std::max(work.x, work.Right() - size.dx);
        int y = limitValue(work.y + (work.dy - size.dy) / 2, work.y, std::max(work.y, work.Bottom() - size.dy));
        return {x, y, size.dx, size.dy};
    }
    int rightSpace = work.Right() - frame.Right();
    int leftSpace = frame.x - work.x;
    int x = rightSpace >= leftSpace ? frame.Right() : frame.x - size.dx;
    x = limitValue(x, work.x, std::max(work.x, work.Right() - size.dx));
    int y = limitValue(frame.y, work.y, std::max(work.y, work.Bottom() - size.dy));
    return {x, y, size.dx, size.dy};
}

static void PaintCloseButton(KeyboardHelpWindow* help, Gfx* gfx) {
    Rect r = help->closeRect;
    Color glyph = gColsCloseBtn[help->closeHovered ? kColCloseXHover : kColCloseX];
    if (help->closeHovered) {
        gfx->FillEllipse(r, gColsCloseBtn[kColCloseCircleHover]);
    }
    int inset = std::max(r.dx / 3, 3);
    Point p1{r.x + inset, r.y + inset};
    Point p2{r.Right() - inset, r.Bottom() - inset};
    Point p3{r.Right() - inset, r.y + inset};
    Point p4{r.x + inset, r.Bottom() - inset};
    gfx->DrawLineAA(p1, p2, glyph, 1.5f);
    gfx->DrawLineAA(p3, p4, glyph, 1.5f);
}

void KeyboardHelpWindow::Paint(PlatformWindowPaintEvent* ev) {
    Gfx* gfx = ev->gfx;
    Color background = gColsFill[kColFillBg];
    Color text = gColsText[kColText];
    Color border = gColsLine[kColLineFg];
    gfx->FillRect(ev->clientRect, background);
    gfx->DrawText(title, titleRect, gfxTextVCenter | gfxTextSingleLine, fontTitle, text);
    gfx->FillRect(separatorRect, border);
    PaintCloseButton(this, gfx);

    Color capBackground = AccentColor(background, 16);
    for (KbSection* section : sections) {
        gfx->DrawText(section->title, section->titleRect, gfxTextVCenter | gfxTextSingleLine, fontHeader, text);
        for (KbRow* row : section->rows) {
            int capsDx = std::max(row->tokens.size - 1, 0) * capGap;
            for (int i = 0; i < row->tokens.size; i++) {
                capsDx += PlatformFontMeasureText(fontRow, row->tokens.At(i)).dx + 2 * capPadX;
            }
            int x = row->keysRect.Right() - capsDx;
            for (int i = 0; i < row->tokens.size; i++) {
                Str token = row->tokens.At(i);
                int dx = PlatformFontMeasureText(fontRow, token).dx + 2 * capPadX;
                Rect cap{x, row->keysRect.y, dx, row->keysRect.dy};
                gfx->FillRoundedRect(cap, capRadius, capBackground, border);
                gfx->DrawText(token, cap, gfxTextCenter | gfxTextVCenter | gfxTextEllipsis, fontRow, text);
                x += dx + capGap;
            }
            gfx->DrawText(row->description, row->descriptionRect, gfxTextVCenter | gfxTextSingleLine, fontRow, text);
        }
    }
    Color footerColor = AccentColor(text, 90);
    gfx->DrawText(footer, footerRect, gfxTextVCenter | gfxTextSingleLine, fontRow, footerColor);
}

void KeyboardHelpWindow::OnPointer(PlatformPointerEvent* ev) {
    if (ev->type == PlatformPointerEventType::Leave) {
        if (closeHovered) {
            closeHovered = false;
            window->Invalidate();
        }
        window->SetCursor(CursorId::Arrow);
        return;
    }
    bool overClose = closeRect.Contains(ev->pos);
    if (overClose != closeHovered) {
        closeHovered = overClose;
        window->Invalidate();
    }
    window->SetCursor(overClose ? CursorId::Hand : ev->pos.y < contentTop ? CursorId::Move : CursorId::Arrow);
    if (ev->button != 1) {
        return;
    }
    if (ev->type == PlatformPointerEventType::Down) {
        if (overClose) {
            closePressed = true;
            ev->didHandle = true;
        } else if (ev->pos.y < contentTop) {
            window->BeginMove(*ev);
            ev->didHandle = true;
        }
    } else if (ev->type == PlatformPointerEventType::Up && closePressed) {
        closePressed = false;
        ev->didHandle = true;
        if (overClose) {
            ScheduleCloseKeyboardHelp();
        }
    }
}

void KeyboardHelpWindow::OnKey(PlatformKeyEvent* ev) {
    if (ev->codepoint == '?') {
        ev->didHandle = true;
        ScheduleCloseKeyboardHelp();
    }
}

bool KeyboardHelpWindow::Create(const KeyboardHelpArgs& args) {
    parent = (NativeWnd)args.parent;
    parentFullscreen = args.parentFullscreen;
    dataSource = args.dataSource ? args.dataSource : GetDefaultKeyboardHelpDataSource();
    GuiColorsInitIfNeeded();
    fontRow = GetDefaultGuiFont();
    fontHeader = GetBoldPlatformFont(fontRow);
    fontTitle = GetScaledPlatformFont(fontHeader, 125);
    if (!fontRow || !fontHeader || !fontTitle) {
        return false;
    }
    CollectContent();
    Size size = LayoutContent();

    PlatformWindow::CreateArgs createArgs;
    createArgs.parent = parent;
    createArgs.title = title;
    createArgs.initialSize = size;
    createArgs.visible = false;
    createArgs.frameless = true;
    createArgs.userData = this;
    window = PlatformWindow::Create(createArgs);
    if (!window) {
        return false;
    }
    window->onPaint = MkMethod1<KeyboardHelpWindow, PlatformWindowPaintEvent*, &KeyboardHelpWindow::Paint>(this);
    window->onPointer = MkMethod1<KeyboardHelpWindow, PlatformPointerEvent*, &KeyboardHelpWindow::OnPointer>(this);
    window->onKey = MkMethod1<KeyboardHelpWindow, PlatformKeyEvent*, &KeyboardHelpWindow::OnKey>(this);
    window->onCloseRequest = MkFunc0Void(ScheduleCloseKeyboardHelp);
    window->SetBounds(PositionHelpWindow(parent, parentFullscreen, size));
    window->Show(true);
    window->Focus();
    return true;
}

void ToggleKeyboardHelp(const KeyboardHelpArgs& args) {
    if (gKeyboardHelpWindow) {
        ScheduleCloseKeyboardHelp();
        return;
    }
    auto* help = new KeyboardHelpWindow();
    gKeyboardHelpWindow = help;
    if (!help->Create(args)) {
        gKeyboardHelpWindow = nullptr;
        delete help;
        return;
    }
}

bool IsKeyboardHelpVisible() {
    return gKeyboardHelpWindow != nullptr;
}
