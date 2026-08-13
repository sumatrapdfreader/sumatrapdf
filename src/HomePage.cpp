/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "gui/Dpi.h"
#include "base/File.h"
#include "base/Pixmap.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "DocController.h"
#include "SumatraConfig.h"
#include "FileHistory.h"
#include "GlobalPrefs.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Commands.h"
#include "Accelerators.h"
#include "CommandPalette.h"
#include "FilterHighlightDraw.h"
#include "FileThumbnails.h"
#include "Menu.h"
#include "HomePage.h"
#include "Translations.h"
#include "Version.h"
#include "Theme.h"
#include "AppSettings.h"
#include "DarkModeSubclass.h"
#include "SvgIcons.h"

// how the shared tip code (TipText.cpp) opens a url link
static void OpenTipUrl(Str url) {
    // documentation links open in the embedded manual browser
    if (!MaybeLaunchDocumentation(url)) {
        SumatraLaunchBrowser(url);
    }
}

// what the shared tip code (TipText.cpp) knows about our commands: it names
// them in (Key/Cmd...) and in link targets, but knows nothing about the command
// table itself
struct SumatraCommandsContext : CommandsContext {
    TempStr GetCommandShortcutTemp(Str cmdName) override {
        int cmdId = GetCommandIdByName(cmdName);
        if (cmdId <= 0) {
            return {}; // not a command: the markup stays literal text
        }
        TempStr accel = AppendAccelKeyToMenuStringTemp("", cmdId);
        if (!accel || !*accel.s) {
            return str::DupTemp(cmdName); // a command, but unbound
        }
        // AppendAccelKeyToMenuStringTemp prepends 	, skip it
        if (accel.s[0] == '	') {
            return Str(accel.s + 1);
        }
        return accel;
    }

    // `cmd` can carry arguments (e.g. "CmdFixDefaultApp .pdf"); those go through
    // CreateCommandFromDefinition so FrameOnCommand sees a CustomCommand with args
    void ExecuteCommand(HWND hwnd, Str cmd) override {
        CustomCommand* custom = CreateCommandFromDefinition(cmd);
        if (custom) {
            HwndSendCommand(hwnd, custom->id);
            return;
        }
        int cmdId = GetCommandIdByName(cmd);
        if (cmdId > 0) {
            HwndSendCommand(hwnd, cmdId);
        }
    }
};

static SumatraCommandsContext gSumatraCommandsContext;

struct TipHookInstaller {
    TipHookInstaller() {
        gTipOpenUrl = OpenTipUrl;
        gCommandsContext = &gSumatraCommandsContext;
    }
};
static TipHookInstaller gTipHookInstaller;

#ifndef ABOUT_USE_LESS_COLORS
#define ABOUT_LINE_OUTER_SIZE 2
#else
#define ABOUT_LINE_OUTER_SIZE 1
#endif
#define ABOUT_LINE_SEP_SIZE 1

static Str sumatraTips = StrL(R"tips(You can [customize scrollbar](CmdChangeScrollbar).
You can [customize keyboard shortcuts](Help/Customize-keyboard-shortcuts).
You can [customize toolbar](Help/Customize-toolbar).
Press (Key/CmdCommandPalette) to open [command palette](CmdCommandPalette).
To open file from history open [command palette](CmdCommandPalette) with (Key/CmdCommandPalette) and type `#`.
You can [extract text from PDF file](Help/Tool-x-extract-text-from-pdf).
You can [toggle menu bar](CmdToggleMenuBar) with (Key/CmdToggleMenuBar).
You can [toggle toolbar](CmdToggleToolbar) with (Key/CmdToggleToolbar).
You can [edit PDF annotations](Help/Editing-annotations).
You can preview where a citation, figure or footnote link points by hovering it — [Toggle Hover Preview](CmdToggleHoverPreview) or set CitationHoverDelay in [advanced settings](CmdAdvancedSettings).
)tips");

static Str sumatraPromos = StrL(R"promos(Try [Edna](https://edna.arslexis.io): a note taking web app for power users.
Try [MarkLexis](https://marklexis.arslexis.io): a bookmarking web application.
)promos");

static Str promoFromServer;

// the tip markup, one line each; the selected one is parsed by the tip band
static StrVec gTipLines;
static StrVec gPromoLines;
static bool gTipsParsed = false;
static bool gSelectedIsPromo = false;
static int gSelectedTipIdx = -1;

static void CollectTipsFromString(Str src, Str prefix, StrVec* out) {
    StrVec lines;
    Split(&lines, src, "\n");
    for (int i = 0; i < len(lines); i++) {
        Str line = lines[i];
        if (str::IsEmptyOrWhiteSpace(line)) {
            continue;
        }
        if (prefix) {
            out->Append(str::JoinTemp(prefix, line));
        } else {
            out->Append(line);
        }
    }
}

// the markup of the tip currently on show, {} when there is none
static Str SelectedTipLine() {
    if (!gGlobalPrefs->showTips || gSelectedTipIdx < 0) {
        return {};
    }
    StrVec& v = gSelectedIsPromo ? gPromoLines : gTipLines;
    if (gSelectedTipIdx >= len(v)) {
        return {};
    }
    return v[gSelectedTipIdx];
}

static void PickRandomTipOrPromo() {
    bool pickPromo = (len(gPromoLines) > 0) && (rand() % 100 < 30);
    if (pickPromo) {
        gSelectedIsPromo = true;
        gSelectedTipIdx = rand() % len(gPromoLines);
    } else if (len(gTipLines) > 0) {
        gSelectedIsPromo = false;
        gSelectedTipIdx = rand() % len(gTipLines);
    }
}

static void EnsureTipsParsed() {
    if (gTipsParsed) {
        return;
    }
    CollectTipsFromString(sumatraTips, "Tip: ", &gTipLines);
    CollectTipsFromString(sumatraPromos, {}, &gPromoLines);
    gTipsParsed = true;
    PickRandomTipOrPromo();
}

static void ClearHomeLayoutCache();

void FreeHomePageTips() {
    if (gTipsParsed) {
        gTipLines.Reset();
        gPromoLines.Reset();
        gTipsParsed = false;
    }
    str::Free(promoFromServer);
    ClearHomeLayoutCache();
}

static void PickAnotherRandomTip() {
    bool prevIsPromo = gSelectedIsPromo;
    int prev = gSelectedTipIdx;
    // keep picking until we get a different one
    int maxIter = 100;
    while (maxIter-- > 0) {
        PickRandomTipOrPromo();
        if (gSelectedIsPromo != prevIsPromo || gSelectedTipIdx != prev) {
            return;
        }
    }
}

constexpr Color kAboutBorderCol = kColBlack;

constexpr int kAboutLeftRightSpaceDx = 8;
constexpr int kAboutMarginDx = 10;
constexpr int kAboutBoxMarginDy = 6;
constexpr int kAboutTxtDy = 6;
constexpr int kAboutRectPadding = 8;

constexpr int kInnerPadding = 8;

static const Str kSumatraTxtFont = StrL("Arial Black");
constexpr int kSumatraTxtFontSize = 24;

#define LAYOUT_LTR 0

static ATOM gAtomAbout;
static HWND gHwndAbout;
static VirtRoot* gAboutRoot = nullptr;
static Tooltip* gAboutTooltip = nullptr;
static Str gClickedURL;

// one row of the About screen's two-column table
struct AboutRow {
    Str leftTxt;
    Str rightTxt;
    Str url;
};

static AboutRow gAboutRows[] = {
    // a null rightTxt means "the app version", filled in by Sync() because it
    // isn't known until runtime (32/64-bit, debug)
    {"version", nullptr, nullptr},
    {"build", "Built: " __DATE__ " " __TIME__, nullptr},
    {"website", "SumatraPDF website", kWebsiteURL},
    {"manual", "SumatraPDF manual", kManualURL},
    {"forums", "SumatraPDF forums", "https://github.com/sumatrapdfreader/sumatrapdf/discussions"},
    {"programming", "The Programmers", "https://github.com/sumatrapdfreader/sumatrapdf/blob/master/AUTHORS"},
    {"licenses", "Various Open Source", "https://github.com/sumatrapdfreader/sumatrapdf/blob/master/AUTHORS"},
#if defined(GIT_COMMIT_ID_STR)
    {"last change", "git commit " GIT_COMMIT_ID_STR,
     "https://github.com/sumatrapdfreader/sumatrapdf/commit/" GIT_COMMIT_ID_STR},
#endif
#if defined(PRE_RELEASE_VER)
    {"a note", "Pre-release version, for testing only!", nullptr},
#endif
#ifdef DEBUG
    {"a note", "Debug version, for testing only!", nullptr},
#endif
    {nullptr, nullptr, nullptr}};

// The About screen's two text columns: a Table (LayoutBase) whose left column
// is right-aligned and right column left-aligned. Rows with a url become
// VirtLink (owning the hit-testing, the hand cursor and the tooltip), the rest
// plain VirtText. Table is pure layout; AboutCtrl paints and hit-tests the
// cells' VirtCtrls itself (they are not VirtCtrl children of AboutCtrl).
static Kind kindAboutCtrl = "aboutCtrl";

struct SumatraLogo;

struct AboutCtrl : VirtCtrl {
    // the two text columns; owned here (not a VirtCtrl child)
    Table* table = nullptr;
    // "Show frequently read", bottom right of the About page (not the window)
    VirtLink* showFreqRead = nullptr;
    // the colored app name on top of the box
    SumatraLogo* logo = nullptr;

    // geometry, computed by UpdateLayout()
    Rect aboutRect;  // the framed box
    Size headerSize; // the "SumatraPDF" band on top of it
    int dividerX = 0;

    AboutCtrl();
    ~AboutCtrl() override;
    void Sync(HDC hdc);
    void UpdateLayout(HWND hwnd, Rect clientRc);
    VirtText* LeftAt(int i);
    VirtText* RightAt(int i);
    void PaintChildren(VirtPaintCtx&) override;
    VirtCtrl* ExtraFromPoint(Point ptWindow, Point* ptLocalOut, u32 flags) override;
};

static void OpenAboutUrl(VirtMouseEvent* ev) {
    auto* link = (VirtLink*)ev->target;
    if (len(link->target) > 0) {
        SumatraLaunchBrowser(link->target);
    }
}

void SetPromoString(Str s) {
    if (!s) return;
    str::ReplaceWithCopy(&promoFromServer, s);
}

static TempStr GetAppVersionTemp() {
    TempStr s = str::DupTemp("v" CURR_VERSION_STRA);
    if (IsProcess64()) {
        s = str::JoinTemp(s, StrL(" 64-bit"));
    } else {
        s = str::JoinTemp(s, StrL(" 32-bit"));
    }
    if (gIsDebugBuild) {
        s = str::JoinTemp(s, StrL(" (dbg)"));
    }
    return s;
}

constexpr Color kCol1 = MkRgb(196, 64, 50);
constexpr Color kCol2 = MkRgb(227, 107, 35);
constexpr Color kCol3 = MkRgb(93, 160, 40);
constexpr Color kCol4 = MkRgb(69, 132, 190);
constexpr Color kCol5 = MkRgb(112, 115, 207);

static Kind kindSumatraLogo = "sumatraLogo";

// the app name centered in its bounds, each letter in a different color (so it
// can't be a VirtText). The version isn't part of it: it is the first row of
// the About table
struct SumatraLogo : VirtCtrl {
    PlatformFont* font = nullptr; // not owned

    SumatraLogo();
    Size GetIdealSize() override;
    void Paint(VirtPaintCtx&) override;
};

SumatraLogo::SumatraLogo() {
    kind = kindSumatraLogo;
    flags |= vwfNoHitTest;
}

Size SumatraLogo::GetIdealSize() {
    Size sz = PlatformFontMeasureText(font, kAppName);
    HWND hwnd = GetHwnd();
    sz.dy += DpiScale(kAboutBoxMarginDy * 2);
    sz.dx += 2 * DpiScale(kInnerPadding);
    return sz;
}

void SumatraLogo::Paint(VirtPaintCtx& ctx) {
    static Color cols[] = {kCol1, kCol2, kCol3, kCol4, kCol5, kCol5, kCol4, kCol3, kCol2, kCol1};
    Size txtSize = PlatformFontMeasureText(font, kAppName);
    Rect r = ctx.bounds;
    Point pt{r.x + ((r.dx - txtSize.dx) / 2), r.y + ((r.dy - txtSize.dy) / 2)};
    char buf[2] = {};
    for (int i = 0; i < len(kAppName); i++) {
        buf[0] = kAppName[i];
        Str letter{buf, 1};
        Size sz = PlatformFontMeasureText(font, letter);
        ctx.gfx->DrawText(letter, {pt.x, pt.y, sz.dx, sz.dy}, 0, font, cols[i % dimofi(cols)]);
        pt.x += sz.dx;
    }
}

static TempStr TrimGitTemp(Str s) {
    if (gitCommidId && str::EndsWith(s, gitCommidId)) {
        int sLen = len(s);
        int gitLen = len(gitCommidId);
        return str::DupTemp(Str(s.s, sLen - gitLen - 7));
    }
    return s;
}

// the About screen's virtual controls for one HWND. Positions come from
// AboutCtrl::UpdateLayout(), so the root must not run a layout of its own
static AboutCtrl* EnsureAboutCtrl(VirtRoot** rootPtr, HWND hwnd, Rect clientRc) {
    VirtRoot* root = *rootPtr;
    if (!root) {
        root = new VirtRoot(hwnd);
        *rootPtr = root;
    }
    if (!IsVirtCtrlOfKind(root->owned, kindAboutCtrl)) {
        root->SetChild(new AboutCtrl());
    }
    root->bounds = clientRc;
    root->needsLayout = false;
    auto* about = (AboutCtrl*)root->owned;
    about->SetBounds(clientRc);
    return about;
}

static int AboutRowCount() {
    int n = 0;
    for (AboutRow* el = gAboutRows; el->leftTxt; el++) {
        n++;
    }
    return n;
}

AboutCtrl::AboutCtrl() {
    kind = kindAboutCtrl;
    flags |= vwfNoHitTest;
    table = new Table();
    logo = new SumatraLogo();
    AddChild(logo);
}

AboutCtrl::~AboutCtrl() {
    delete table;
    table = nullptr;
}

VirtText* AboutCtrl::LeftAt(int i) {
    return (VirtText*)table->GetCell(i, 0);
}

VirtText* AboutCtrl::RightAt(int i) {
    return (VirtText*)table->GetCell(i, 1);
}

// paint logo (VirtCtrl children) and the table's VirtText / VirtLink cells
void AboutCtrl::PaintChildren(VirtPaintCtx& ctx) {
    VirtCtrl::PaintChildren(ctx);
    if (!table) {
        return;
    }
    for (int i = 0; i < table->LayoutChildCount(); i++) {
        VirtCtrl* v = table->LayoutChildAt(i)->AsVirtCtrl();
        if (!v) {
            continue;
        }
        v->SetRoot(root);
        // cells were given absolute window coords by Table::SetBounds
        v->PaintTree(ctx.gfx, {0, 0}, ctx.clip);
    }
}

// table cells are not VirtCtrl children; CtrlFromPoint asks here after logo / showFreqRead miss
VirtCtrl* AboutCtrl::ExtraFromPoint(Point ptWindow, Point* ptLocalOut, u32 flags) {
    if (!table) {
        return nullptr;
    }
    for (int i = table->LayoutChildCount() - 1; i >= 0; i--) {
        VirtCtrl* v = table->LayoutChildAt(i)->AsVirtCtrl();
        if (!v) {
            continue;
        }
        v->SetRoot(root);
        VirtCtrl* hit = CtrlFromPoint(v, ptWindow, ptLocalOut, flags);
        if (hit) {
            return hit;
        }
    }
    return nullptr;
}

// build the table once, then keep text, fonts and colors in step with the theme
// and the DPI. Sizing happens in UpdateLayout(), which measures what we set here
void AboutCtrl::Sync(HDC hdc) {
    int n = AboutRowCount();
    bool canAccessDisk = CanAccessDisk();
    if (table->rows != n) {
        table->SetSize(n, 2);
        for (int i = 0; i < n; i++) {
            AboutRow* el = &gAboutRows[i];
            TableCell& left = table->SetCell(i, 0, new VirtText(el->leftTxt));
            // the left column is flush against the divider line
            left.alignH = CrossAxisAlign::CrossEnd;
            left.alignV = CrossAxisAlign::CrossCenter;

            VirtText* rightTxt;
            if (el->url) {
                auto* link = new VirtLink(el->rightTxt);
                link->SetTarget(el->url);
                link->SetTooltip(el->url);
                link->withUnderline = true;
                // the underline sat 3px above the bottom of the text box
                link->underlineOffsetY = -3;
                link->onClick = MkFunc1Void(OpenAboutUrl);
                rightTxt = link;
            } else {
                rightTxt = new VirtText(el->rightTxt);
            }
            TableCell& right = table->SetCell(i, 1, rightTxt);
            right.alignV = CrossAxisAlign::CrossCenter;
        }
    }

    logo->font = GetPlatformFont(HdcCreateSimpleFont(hdc, kSumatraTxtFont, kSumatraTxtFontSize));

    HFONT fontLeftTxt = HdcCreateSimpleFont(hdc, kLeftTextFont, kLeftTextFontSize);
    HFONT fontRightTxt = HdcCreateSimpleFont(hdc, kRightTextFont, kRightTextFontSize);
    Color colText = ThemeWindowTextColor();
    Color colLink = ThemeWindowLinkColor();

    for (int i = 0; i < n; i++) {
        AboutRow* el = &gAboutRows[i];
        VirtText* left = LeftAt(i);
        left->font = GetPlatformFont(fontLeftTxt);
        left->textColor = colText;

        VirtText* right = RightAt(i);
        right->font = GetPlatformFont(fontRightTxt);
        bool isLink = canAccessDisk && el->url;
        right->textColor = isLink ? colLink : colText;
        // without disk access the url can't be opened, so it isn't a link
        right->withUnderline = isLink;
        right->SetFlag(vwfNoHitTest, !isLink);
        right->SetText(el->rightTxt ? TrimGitTemp(el->rightTxt) : Str(GetAppVersionTemp()));
    }
}

// the About box is the title band above the two-column table. This sizes it from
// the table, centers it in clientRc and positions the table inside it
void AboutCtrl::UpdateLayout(HWND hwnd, Rect clientRc) {
    headerSize = logo->GetIdealSize();

    int leftRightSpaceDx = DpiScale(kAboutLeftRightSpaceDx);
    int marginDx = DpiScale(kAboutMarginDx);
    int aboutTxtDy = DpiScale(kAboutTxtDy);

    table->colGap = 2 * leftRightSpaceDx;
    table->rowGap = aboutTxtDy;
    Size tableSize = table->Layout(ExpandInf());

    Rect r;
    // the divider line is drawn inside the gap between the two columns
    r.dx = std::max(tableSize.dx + ABOUT_LINE_SEP_SIZE, headerSize.dx) + (2 * ABOUT_LINE_OUTER_SIZE) + (2 * marginDx);
    // one extra row gap so the last row isn't flush against the frame
    r.dy = headerSize.dy + tableSize.dy + aboutTxtDy + (2 * ABOUT_LINE_OUTER_SIZE) + 4;
    r.x = clientRc.x + ((clientRc.dx - r.dx) / 2);
    r.y = clientRc.y + ((clientRc.dy - r.dy) / 2);
    aboutRect = r;

    logo->SetBounds({r.x + ((r.dx - headerSize.dx) / 2), r.y, headerSize.dx, headerSize.dy});

    int x = r.x + ABOUT_LINE_OUTER_SIZE + marginDx;
    int y = r.y + headerSize.dy + 4;
    table->SetBounds({x, y, tableSize.dx, tableSize.dy});
    dividerX = table->CellRect(0, 1).x - leftRightSpaceDx;
}

// prepares the About tree for hwnd and computes its geometry
static AboutCtrl* UpdateAboutLayout(VirtRoot** rootPtr, HWND hwnd, HDC hdc, Rect clientRc) {
    AboutCtrl* about = EnsureAboutCtrl(rootPtr, hwnd, clientRc);
    about->Sync(hdc);
    about->UpdateLayout(hwnd, clientRc);
    return about;
}

/* Draws the about screen. The text columns are painted by the AboutCtrl tree;
   this draws the frame around them. It transcribes the design I did in graphics
   software - hopeless to understand without seeing the design. */
static void DrawAbout(HWND hwnd, HDC hdc, VirtRoot* root) {
    auto* about = (AboutCtrl*)root->owned;
    Rect rect = about->aboutRect;
    auto col = ThemeWindowTextColor();
    AutoDeletePen penBorder(CreatePen(PS_SOLID, ABOUT_LINE_OUTER_SIZE, col));
    AutoDeletePen penDivideLine(CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, col));
    col = ThemeWindowLinkColor();
    AutoDeletePen penLinkLine(CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, col));

    HFONT fontLeftTxt = HdcCreateSimpleFont(hdc, kLeftTextFont, kLeftTextFontSize);
    HFONT fontRightTxt = HdcCreateSimpleFont(hdc, kRightTextFont, kRightTextFontSize);

    ScopedSelectObject font(hdc, fontLeftTxt); /* Just to remember the orig font */

    Rect rc = HwndClientRect(hwnd);
    col = ThemeMainWindowBackgroundColor();
    AutoDeleteBrush brushAboutBg = CreateSolidBrush(col);
    HdcFillRect(hdc, rc, brushAboutBg);

    /* render title */
    Rect titleRect(rect.TL(), about->headerSize);

    ScopedSelectObject brush(hdc, CreateSolidBrush(col), true);
    ScopedSelectObject pen(hdc, penBorder);
#ifndef ABOUT_USE_LESS_COLORS
    Rectangle(hdc, rect.x, rect.y + ABOUT_LINE_OUTER_SIZE, rect.x + rect.dx,
              rect.y + titleRect.dy + ABOUT_LINE_OUTER_SIZE);
#else
    Rect titleBgBand(0, rect.y, rc.dx, titleRect.dy);
    RECT rcLogoBg = titleBgBand.ToRECT();
    HdcFillRect(hdc, ToRect(rcLogoBg), bgBrush);
    HdcDrawLine(hdc, Rect(0, rect.y, rc.dx, 0));
    HdcDrawLine(hdc, Rect(0, rect.y + titleRect.dy, rc.dx, 0));
#endif

    /* render attribution box */
    col = ThemeWindowTextColor();
    SetTextColor(hdc, col);
    SetBkMode(hdc, TRANSPARENT);

#ifndef ABOUT_USE_LESS_COLORS
    Rectangle(hdc, rect.x, rect.y + titleRect.dy, rect.x + rect.dx, rect.y + rect.dy);
#endif

    /* render both text columns */
    GfxHdc gfx(hdc);
    root->Paint(&gfx, rc);

    SelectObject(hdc, penDivideLine);
    Rect divideLine(about->dividerX, rect.y + titleRect.dy + 4, 0, rect.dy - titleRect.dy - 8);
    HdcDrawLine(hdc, divideLine);
}

static void OnPaintAbout(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    SetLayout(hdc, LAYOUT_LTR);
    UpdateAboutLayout(&gAboutRoot, hwnd, hdc, HwndClientRect(hwnd));
    DrawAbout(hwnd, hdc, gAboutRoot);
    EndPaint(hwnd, &ps);
}

static void CopyAboutInfoToClipboard() {
    str::Builder info(512);
    TempStr ver = GetAppVersionTemp();
    info.Append(fmt("%s %s\r\n", Str(kAppName), ver));
    for (int i = len(info) - 2; i > 0; i--) {
        info.AppendChar('-');
    }
    info.Append("\r\n");
    // concatenate all the information into a single string
    // (cf. CopyPropertiesToClipboard in SumatraProperties.cpp)
    int maxLen = 0;
    for (AboutRow* el = gAboutRows; el->leftTxt; el++) {
        maxLen = std::max(maxLen, len(el->leftTxt));
    }
    for (AboutRow* el = gAboutRows; el->leftTxt; el++) {
        for (int i = maxLen - len(el->leftTxt); i > 0; i--) {
            info.AppendChar(' ');
        }
        info.Append(fmt("%s: %s\r\n", el->leftTxt, el->url ? el->url.s : el->rightTxt));
    }
    CopyTextToClipboard(ToStr(info));
}

static void CreateInfotipForLink(Str tooltip, const Rect& rc) {
    if (gAboutTooltip != nullptr) {
        return;
    }

    Tooltip::CreateArgs args;
    args.parent = gHwndAbout;
    args.font = GetAppFont();
    args.isRtl = IsUIRtl();

    gAboutTooltip = new Tooltip();
    gAboutTooltip->Create(args);
    gAboutTooltip->SetSingle(tooltip, rc, false);
}

static void DeleteInfotip() {
    if (gAboutTooltip == nullptr) {
        return;
    }
    // gAboutTooltip->Hide();
    delete gAboutTooltip;
    gAboutTooltip = nullptr;
}

static LRESULT CALLBACK WndProcAbout(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Point pt;

    // the links are VirtLinks: let the tree hit-test, click and set the
    // cursor. Its GetTooltipTemp() drives this window's own Tooltip
    if (gAboutRoot && gAboutRoot->owned) {
        LRESULT res = 0;
        switch (msg) {
            case WM_MOUSEMOVE:
            case WM_MOUSELEAVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
                if (gAboutRoot->OnMessage(msg, wp, lp, res)) {
                    return res;
                }
                break;
            case WM_SETCURSOR: {
                pt = HwndGetCursorPos(hwnd);
                Point ptLocal{0, 0};
                VirtCtrl* w = CtrlFromPoint(gAboutRoot, pt, &ptLocal);
                if (w && w->OnSetCursor(ptLocal)) {
                    TempStr tip = w->GetTooltipTemp(ptLocal);
                    if (tip && *tip.s) {
                        Rect r = w->BoundsInWindow();
                        CreateInfotipForLink(tip, r);
                    }
                    return TRUE;
                }
                DeleteInfotip();
                return DefWindowProc(hwnd, msg, wp, lp);
            }
        }
    }

    switch (msg) {
        case WM_CREATE:
            ReportIf(gHwndAbout);
            if (UseDarkModeLib()) {
                DarkMode::setDarkTitleBarEx(hwnd, true);
            }
            break;

        case WM_ERASEBKGND:
            // do nothing, helps to avoid flicker
            return TRUE;

        case WM_PAINT:
            OnPaintAbout(hwnd);
            break;

        case WM_SETCURSOR:
            DeleteInfotip();
            return DefWindowProc(hwnd, msg, wp, lp);

        case WM_CHAR:
            if (VK_ESCAPE == wp) {
                DestroyWindow(hwnd);
            }
            break;

        case WM_COMMAND:
            if (CmdCopySelection == LOWORD(wp)) {
                CopyAboutInfoToClipboard();
            }
            break;

        case WM_DESTROY:
            DeleteInfotip();
            delete gAboutRoot;
            gAboutRoot = nullptr;
            ReportIf(!gHwndAbout);
            gHwndAbout = nullptr;
            break;

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

constexpr const WCHAR* kAboutClassName = L"SUMATRA_PDF_ABOUT";

void ShowAboutWindow(MainWindow* win) {
    if (gHwndAbout) {
        SetActiveWindow(gHwndAbout);
        return;
    }

    if (!gAtomAbout) {
        WNDCLASSEX wcex;
        FillWndClassEx(wcex, kAboutClassName, WndProcAbout);
        HMODULE h = GetModuleHandleW(nullptr);
        wcex.hIcon = LoadIcon(h, MAKEINTRESOURCE(GetAppIconID()));
        gAtomAbout = RegisterClassEx(&wcex);
        ReportIf(!gAtomAbout);
    }

    WCHAR* title = CWStrTemp(_TRA("About SumatraPDF"));
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int dx = CW_USEDEFAULT;
    int dy = CW_USEDEFAULT;
    HINSTANCE h = GetModuleHandleW(nullptr);
    gHwndAbout = CreateWindowExW(0, kAboutClassName, title, style, x, y, dx, dy, nullptr, nullptr, h, nullptr);
    if (!gHwndAbout) {
        return;
    }

    HwndSetRtl(gHwndAbout, IsUIRtl());

    // get the dimensions required for the about box's content
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(gHwndAbout, &ps);
    SetLayout(hdc, LAYOUT_LTR);
    AboutCtrl* about = UpdateAboutLayout(&gAboutRoot, gHwndAbout, hdc, HwndClientRect(gHwndAbout));
    Rect rc = about->aboutRect;
    EndPaint(gHwndAbout, &ps);
    int rectPadding = DpiScale(kAboutRectPadding);
    rc.Inflate(rectPadding, rectPadding);

    // resize the new window to just match these dimensions
    Rect wRc = HwndWindowRect(gHwndAbout);
    Rect cRc = HwndClientRect(gHwndAbout);
    wRc.dx += rc.dx - cRc.dx;
    wRc.dy += rc.dy - cRc.dy;
    MoveWindow(gHwndAbout, wRc.x, wRc.y, wRc.dx, wRc.dy, FALSE);

    HwndPositionInCenterOf(gHwndAbout, win->hwndFrame);
    ShowWindow(gHwndAbout, SW_SHOW);
}

static void ShowFrequentlyRead(VirtMouseEvent* ev) {
    auto* win = (MainWindow*)ev->target->userData;
    gGlobalPrefs->showStartPage = true;
    win->RedrawAll(true);
}

void DrawAboutPage(MainWindow* win, HDC hdc) {
    HWND hwnd = win->hwndCanvas;
    Rect clientRc = HwndClientRect(hwnd);
    AboutCtrl* about = UpdateAboutLayout(&win->homeRoot, hwnd, hdc, clientRc);

    bool showLink = HasPermission(Perm::SavePreferences | Perm::DiskAccess) && SettingsRememberOpenedFiles();
    if (showLink && !about->showFreqRead) {
        auto* link = new VirtLink(_TRA("Show frequently read"));
        link->withUnderline = true;
        link->isRtl = IsUIRtl();
        link->userData = (uintptr_t)win;
        link->onClick = MkFunc1Void(ShowFrequentlyRead);
        about->showFreqRead = link;
        about->AddChild(link);
    }
    if (about->showFreqRead) {
        VirtLink* link = about->showFreqRead;
        link->visibility = showLink ? Visibility::Visible : Visibility::Collapse;
        link->font = GetPlatformFont(HdcCreateSimpleFont(hdc, "MS Shell Dlg", 16));
        link->textColor = ThemeWindowLinkColor();
        link->sz = {0, 0}; // re-measure: the font may have changed with the DPI
        Size txtSize = link->GetIdealSize(true);
        Rect r = {0, 0, txtSize.dx, txtSize.dy};
        PositionRB(clientRc, r);
        MoveXY(r, -DpiScale(kInnerPadding), -DpiScale(kInnerPadding));
        link->SetBounds(r);
    }
    DrawAbout(hwnd, hdc, win->homeRoot);
}

/* alternate static page to display when no document is loaded */

constexpr int kThumbsSeparatorDy = 2;
constexpr int kThumbsBorderDx = 1;
#define kThumbsMarginLeft DpiScale(40)
#define kThumbsMarginRight DpiScale(40)
#define kThumbsMarginTop DpiScale(50)
#define kThumbsMarginBottom DpiScale(40)
#define kThumbsSpaceBetweenX DpiScale(38)
#define kThumbsSpaceBetweenY DpiScale(58)
#define kThumbsBottomBoxDy DpiScale(50)
#define kHomeListThumbDx DpiScale(30)
#define kHomeListThumbDy DpiScale(40)
#define kHomeListRowDy DpiScale(46)
#define kHomeListRowGapDx DpiScale(8)

// ThumbnailLayout::fileSize cache: AppendBlanks zero-fills, so set
// kSizeNotFetched after each AppendBlanks (default member init never runs).
constexpr i64 kSizeNotFetched = -2;
constexpr i64 kSizeFetchFail = -1;

struct ThumbnailLayout {
    Rect rcPage;
    Size szThumb;
    Rect rcText;
    Rect rcListRow;
    Rect rcListThumb;
    Rect rcListFileName;
    Rect rcListPath;
    Rect rcListSize;
    Rect rcListRemove;
    Rect rcListPin;
    FileState* fs = nullptr; // info needed to draw the thumbnail
    // Cached file::GetSize() so we don't hit the disk on every paint.
    // AppendBlanks zero-fills, so set to kSizeNotFetched after AppendBlanks.
    i64 fileSize = kSizeNotFetched;
    // false until MeasureHomeListRowText() has split rcListFileName into name +
    // directory. Also relies on AppendBlanks zero-fill, so false must mean "not
    // measured yet"
    bool listTextMeasured = false;
};

static TempStr FileSizeForHomeListTemp(i64 size);

// true if r overlaps the visible thumbs band (optionally with a small margin)
static bool IsHomeThumbOnScreen(const Rect& r, const Rect& thumbsArea, int marginY = 0) {
    if (r.IsEmpty() || thumbsArea.IsEmpty()) {
        return false;
    }
    Rect band = thumbsArea;
    if (marginY > 0) {
        band.y -= marginY;
        band.dy += 2 * marginY;
    }
    return !r.Intersect(band).IsEmpty();
}

// HomePageViewMode setting ("thumbnails" or "list")
bool HomePageIsListView() {
    return gGlobalPrefs && str::EqI(gGlobalPrefs->homePageViewMode, StrL("list"));
}

void SetHomePageListView(bool listView) {
    Str mode = listView ? StrL("list") : StrL("thumbnails");
    str::ReplaceWithCopy(&gGlobalPrefs->homePageViewMode, mode);
}

struct HomePageLayout {
    // args in
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    Rect rc;
    MainWindow* win = nullptr;

    Rect rcIconOpen;
    Rect rcIconListView;
    Rect rcIconThumbnailView;

    VirtText* freqRead = nullptr;
    VirtText* openDoc = nullptr;
    VirtText* hideShowFreqRead = nullptr;
    Vec<ThumbnailLayout> thumbnails; // info for each thumbnail
    int totalContentDy = 0;          // total height of all thumbnail rows
    int thumbsVisibleDy = 0;         // visible height for thumbnails area
    Rect rcThumbsArea;               // clip rect for thumbnails

    // search filter
    StrVec filterWords;
    Vec<u8> highlighted;
    Rect rcSearchBorder; // border rect drawn around the edit control

    // tip layout
    Rect rcTip;     // background rect for tip area
    Rect rcTipText; // where the markup goes inside the band
    bool hasTip = false;

    ~HomePageLayout();
};

// freqRead / openDoc are borrowed from the persistent chrome tree (owned by
// win->homeRoot), not created per layout
HomePageLayout::~HomePageLayout() = default;

// --- home page chrome as a VirtCtrl tree ---
// The chrome (header, view-mode buttons, "Open a document..." link, help
// button) lives for as long as the window, so hover / pressed state survives
// the repaints that scrolling and filtering cause. Geometry still comes from
// LayoutHomePage(): HomePageSyncChrome() just feeds it into the tree.

// Leaf home-page controls: no MainWindow*. Wire onClick / hwndForCmds when
// building the chrome so the same VirtCtrl types stay reusable.

struct HomeViewIconCtrl : VirtCtrl {
    Pixmap* pixmap = nullptr; // not owned, from GetCachedPixmapForSvg()
    // true for the "show as list" button, false for "show as thumbnails"
    bool listView = false;

    HomeViewIconCtrl();
    void Paint(VirtPaintCtx&) override;
};

struct HomeOpenDocCtrl : VirtCtrl {
    Pixmap* pixmap = nullptr; // not owned, from GetCachedPixmapForSvg()
    VirtText* text = nullptr; // child
    // icon position, relative to our bounds
    Rect rcIconLocal;

    HomeOpenDocCtrl();
    void Paint(VirtPaintCtx&) override;
};

struct HomeHelpBtnCtrl : VirtCtrl {
    HomeHelpBtnCtrl();
    void Paint(VirtPaintCtx&) override;
};

struct HomeEntryCtrl;

// the pin icon of a list-view row. It is drawn by DrawHomeListRow, so this is a
// hit target only (the row's ✕ is a VirtCloseButton, which draws itself)
struct HomeListIconCtrl : VirtCtrl {
    bool isPin = true;

    HomeListIconCtrl();
    void OnGetTooltip(VirtTooltipEvent*);
};

// one file entry (a thumbnail or a list row). Painting still happens in
// DrawHomePageLayout(); this owns hit-testing, hover and clicks
struct HomeEntryCtrl : VirtCtrl {
    Str filePath; // owned
    int idx = 0;
    VirtCloseButton* closeBtn = nullptr;
    VirtCloseButton* removeBtn = nullptr;
    HomeListIconCtrl* pinBtn = nullptr;

    HomeEntryCtrl();
    ~HomeEntryCtrl() override;
};

// page-level list: still knows the MainWindow so it can wire entry actions and
// keep keyboard selection in sync
struct HomeEntriesCtrl : VirtCtrl {
    MainWindow* win = nullptr;
    // entry the mouse is on, -1 for none. Drives the ✕ button and the keyboard
    // selection, which follows the mouse
    int activeIdx = -1;
    Point lastHoverPt{-1, -1};

    HomeEntriesCtrl();
    void OnMouseMove(VirtMouseEvent*);

    HomeEntryCtrl* EntryAt(int idx);
    HomeEntryCtrl* EntryForCtrl(VirtCtrl*);
    void SetEntryCount(int n);
    void SetActiveEntry(int idx);
    void UpdateCloseBtnVisibility();
};

// the tip band at the bottom. The markup is its VirtRichText child, which draws
// itself and runs its own links; clicking the band anywhere else picks another
// tip
struct HomeTipCtrl : VirtCtrl {
    // for link commands inside the tip markup (like VirtRichText)
    HWND hwndForCmds = nullptr;
    // onClick (VirtCtrl): band click outside a link picks another tip
    VirtRichText* rich = nullptr; // owned, as our only child
    Str richFor;                  // owned, the markup `rich` was parsed from

    ~HomeTipCtrl() override;
    void SetTipLine(Str line, PlatformFont* font);
    void Sync(const Rect& rcTip, const Rect& rcText);
};

static Kind kindHomeChromeCtrl = "homeChromeCtrl";

struct HomeChromeCtrl : VirtCtrl {
    HomeTipCtrl* tip = nullptr;
    HomeEntriesCtrl* entries = nullptr;
    VirtText* hdr = nullptr;
    HomeViewIconCtrl* thumbView = nullptr;
    HomeViewIconCtrl* listView = nullptr;
    HomeOpenDocCtrl* openDoc = nullptr;
    HomeHelpBtnCtrl* helpBtn = nullptr;
};

static HomeChromeCtrl* EnsureHomeChrome(MainWindow* win);
static HomeEntriesCtrl* HomeEntries(MainWindow* win);
static void HomePageSyncChrome(HomePageLayout& l);

static int HomePageIconSize() {
    int sz = DpiScale(gGlobalPrefs->toolbarSize);
    if (sz < 1) {
        sz = DpiScale(16);
    }
    return RoundUp(sz, 4);
}

constexpr int kOpenDocumentYShift = 7;
constexpr int kThumbsMiddleMargin = 32;
constexpr int kSearchEditDy = 28;
constexpr int kHeaderSearchGapY = 12;
constexpr int kSearchThumbnailsGapY = 12;

static void HomeSelectFromSearchReturnCol(MainWindow* win);
static void HomePageShowSelectionTooltip(MainWindow* win);

struct HomeSearchEdit : Edit {
    MainWindow* win = nullptr;

    void WndProc(ControlBase::WndProcEvent* ev) {
        if (ev->msg == WM_KEYDOWN && ev->wparam == VK_DOWN) {
            // down from the search box moves into the file list (issue #1136),
            // restoring the column we left from when going up
            if (win) {
                HomeSelectFromSearchReturnCol(win);
                HwndSetFocus(win->hwndCanvas);
                HwndInvalidate(win->hwndCanvas);
                HomePageShowSelectionTooltip(win);
            }
            ev->result = 0;
            ev->didHandle = true;
            return;
        }
        if (ev->msg == WM_KEYDOWN && ev->wparam == VK_ESCAPE) {
            SetText("");
            if (win) {
                HwndSetFocus(win->hwndCanvas);
                win->RedrawAll(true);
            }
            ev->result = 0;
            ev->didHandle = true;
            return;
        }
        if (ev->msg == WM_MOUSEWHEEL) {
            // the home page scrolls, not the one-line edit
            ev->result = SendMessageW(GetParent(ev->hwnd), ev->msg, ev->wparam, ev->lparam);
            ev->didHandle = true;
            return;
        }
        Edit::WndProc(ev);
    }
};

// Home-list entries with a path (same set as thumbnails when search is empty).
static int CountHomePageFiles() {
    Vec<FileState*> all;
    if (gGlobalPrefs && gGlobalPrefs->homePageSortByFrequentlyRead) {
        gFileHistory.GetFrequencyOrder(all);
    } else {
        gFileHistory.GetRecentlyOpenedOrder(all);
    }
    int n = 0;
    for (FileState* fs : all) {
        if (fs && len(fs->filePath) > 0) {
            n++;
        }
    }
    return n;
}

// Cue banner when the search field is empty: "Search N files (Ctrl + F)".
static void UpdateHomeSearchCueBanner(MainWindow* win) {
    if (!win || !win->homeSearch) {
        return;
    }
    // _TRA returns Str; pass .s into type-safe fmt for the format string.
    TempStr cue = fmt(_TRA("Search %d files (Ctrl + F)").s, CountHomePageFiles());
    win->homeSearch->SetCue(cue);
}

static void HomeSearchTextChanged(MainWindow* win) {
    win->homePageScrollY = 0;
    // the filter changed the list, so select its first entry (#1136)
    HomePageSelectFirst(win);
    HwndInvalidate(win->hwndCanvas);
}

// the keyboard selection outline is hidden while the search box has the focus
static void HomeSearchFocusChanged(MainWindow* win) {
    HwndInvalidate(win->hwndCanvas);
}

static void EnsureHomeSearchCreated(MainWindow* win) {
    if (win->homeSearch) {
        UpdateHomeSearchCueBanner(win);
        return;
    }
    HWND parent = win->hwndCanvas;
    HDC hdc = GetDC(parent);
    HFONT font = HdcCreateSimpleFont(hdc, "MS Shell Dlg", 14);
    ReleaseDC(parent, hdc);

    Edit::CreateArgs args;
    args.parent = parent;
    args.font = font;
    // the home page draws the box around it, so the edit has no border of its own
    auto* e = new HomeSearchEdit();
    e->win = win;
    e->Create(args);
    // Edit::Create wired Edit::WndProc; re-route to HomeSearchEdit for Esc/Down/wheel
    e->onWndProc = MkMethod1<HomeSearchEdit, ControlBase::WndProcEvent*, &HomeSearchEdit::WndProc>(e);
    e->SetColors(ThemeWindowTextColor(), ThemeControlBackgroundColor());
    e->onTextChanged = MkFunc0(HomeSearchTextChanged, win);
    e->onFocus = MkFunc0(HomeSearchFocusChanged, win);
    e->onKillFocus = MkFunc0(HomeSearchFocusChanged, win);
    win->homeSearch = e;
    UpdateHomeSearchCueBanner(win);
    // add left/right padding so text doesn't overlap the border
    int margin = DpiScale(6);
    e->SetMargins(margin, margin);
    // restore the query from before the edit control was destroyed
    // (e.g. by switching to a document tab and back)
    if (len(win->homeSearchQuery) > 0) {
        e->SetText(win->homeSearchQuery);
    }
    // the box is kSearchEditDy tall but the edit is only as tall as its text,
    // so a one-child HBox centers it in there instead of us doing that by hand
    auto* box = new HBox();
    box->alignCross = CrossAxisAlign::CrossCenter;
    box->AddChild(e, 1);
    win->homeSearchLayout = box;
}

void HomePageDestroySearch(MainWindow* win) {
    if (!win->homeSearch) {
        return;
    }
    TempStr query = win->homeSearch->GetTextTemp();
    str::ReplaceWithCopy(&win->homeSearchQuery, query);
    // destroying the edit's window pumps messages, and the canvas answers most
    // of them by calling us again (see WndProcCanvas), so drop our pointers
    // before deleting - otherwise the re-entered call deletes the tree twice
    ILayout* layout = win->homeSearchLayout;
    win->homeSearchLayout = nullptr;
    win->homeSearch = nullptr;
    // the layout owns the edit
    delete layout;
}

// after a theme change; the edit paints itself from these (see
// Edit::OnMessageReflect and the reflection in WndProcCanvas)
void HomePageUpdateSearchColors(MainWindow* win) {
    if (win->homeSearch) {
        win->homeSearch->SetColors(ThemeWindowTextColor(), ThemeControlBackgroundColor());
    }
}

void HomePageFocusSearch(MainWindow* win) {
    EnsureHomeSearchCreated(win);
    win->homeSearch->SetIsVisible(true);
    HwndSetFocus(win->homeSearch->hwnd);
}

void PickAnotherRandomPromotion() {
    PickAnotherRandomTip();
}

// Just the path here — no file::GetSize during layout/scroll, it's too slow on
// network drives (was ~11% of home-page scroll CPU). The size is appended when
// the tooltip is actually shown, see LinkTooltipTemp().
static TempStr HomeThumbTooltipTemp(Str path) {
    return str::DupTemp(path);
}

// --- scroll-friendly layout cache: full LayoutHomePage only when content/size/
// filter changes; pure scrollY changes just offset stored thumb rects ---
struct HomePageLayoutCache {
    bool valid = false;
    Rect canvasRc;
    int scrollY = 0;
    int nFiles = 0;
    bool listView = false;
    bool sortByFreq = false;
    bool showTips = false;
    int tipIdx = -1;
    bool tipIsPromo = false;
    Str filterText; // owned

    Rect rcThumbsArea;
    Rect rcSearchBorder;
    Rect rcIconOpen;
    Rect rcIconListView;
    Rect rcIconThumbnailView;
    Rect rcTip;
    Rect rcFreqRead;
    Rect rcOpenDoc;
    int totalContentDy = 0;
    int thumbsVisibleDy = 0;
    Rect rcTipText;
    bool hasTip = false;
    Vec<ThumbnailLayout> thumbs;
    StrVec filterWords;
};

static HomePageLayoutCache gHomeLayoutCache;

static void ClearHomeLayoutCache() {
    gHomeLayoutCache.valid = false;
    str::Free(gHomeLayoutCache.filterText);
    gHomeLayoutCache.filterText = {};
    gHomeLayoutCache.thumbs.Reset();
    gHomeLayoutCache.filterWords.Reset();
    gHomeLayoutCache.hasTip = false;
    gHomeLayoutCache.nFiles = 0;
    gHomeLayoutCache.scrollY = 0;
}

// The cache holds raw FileState* (ThumbnailLayout::fs) owned by gGlobalPrefs.
// Reloading settings frees and rebuilds those, so the cache has to be dropped
// first or hover / selection reads freed memory (crash 8c34d7eda). It is
// rebuilt on the next paint.
// must be called before the FileState objects the cache points at are freed
void HomePageInvalidateLayoutCache() {
    ClearHomeLayoutCache();
}

static void OffsetThumbnailLayouts(Vec<ThumbnailLayout>& thumbs, int dy) {
    if (dy == 0) {
        return;
    }
    for (ThumbnailLayout& t : thumbs) {
        t.rcPage.y += dy;
        t.rcText.y += dy;
        t.rcListRow.y += dy;
        t.rcListThumb.y += dy;
        t.rcListFileName.y += dy;
        t.rcListPath.y += dy;
        t.rcListSize.y += dy;
        t.rcListRemove.y += dy;
        t.rcListPin.y += dy;
    }
}

static TempStr HomeSearchQueryTemp(MainWindow* win) {
    if (!win->homeSearch) {
        return {};
    }
    return win->homeSearch->GetTextTemp();
}

static bool HomeLayoutCacheMatches(const Rect& rc, Str filterText) {
    auto& c = gHomeLayoutCache;
    if (!c.valid) {
        return false;
    }
    if (c.canvasRc != rc) {
        return false;
    }
    if (c.listView != HomePageIsListView()) {
        return false;
    }
    if (c.sortByFreq != (gGlobalPrefs && gGlobalPrefs->homePageSortByFrequentlyRead)) {
        return false;
    }
    if (c.showTips != (gGlobalPrefs && gGlobalPrefs->showTips)) {
        return false;
    }
    if (c.tipIdx != gSelectedTipIdx || c.tipIsPromo != gSelectedIsPromo) {
        return false;
    }
    if (!str::Eq(c.filterText, filterText)) {
        return false;
    }
    // pin/remove/reorder changes FileState pointers or order → invalidate
    // (nFiles alone is not enough: pin does not change count)
    return true;
}

// true if cached thumb FileState* sequence still matches the current file list
static bool HomeLayoutCacheFilesMatch(const Vec<FileState*>& files) {
    auto& c = gHomeLayoutCache;
    if (len(files) != c.nFiles || len(c.thumbs) != c.nFiles) {
        return false;
    }
    for (int i = 0; i < c.nFiles; i++) {
        if (c.thumbs[i].fs != files[i]) {
            return false;
        }
    }
    return true;
}

static void CollectHomePageFiles(MainWindow* win, Vec<FileState*>& fileStates, StrVec& filterWords) {
    Vec<FileState*> allFileStates;
    if (gGlobalPrefs->homePageSortByFrequentlyRead) {
        gFileHistory.GetFrequencyOrder(allFileStates);
    } else {
        gFileHistory.GetRecentlyOpenedOrder(allFileStates);
    }

    TempStr searchQuery = HomeSearchQueryTemp(win);
    bool hasFilter = searchQuery && searchQuery.s[0];
    if (hasFilter) {
        SplitFilterToWords(searchQuery, filterWords);
    }
    for (int i = 0; i < len(allFileStates); i++) {
        FileState* fs = allFileStates[i];
        if (len(fs->filePath) == 0) {
            continue;
        }
        if (hasFilter) {
            TempStr baseName = path::GetBaseNameTemp(fs->filePath);
            if (!FilterMatches(baseName, filterWords)) {
                continue;
            }
        }
        fileStates.Append(fs);
    }
}

static void SaveHomeLayoutCache(const HomePageLayout& l, Str filterText, int scrollY) {
    auto& c = gHomeLayoutCache;
    c.valid = true;
    c.canvasRc = l.rc;
    c.scrollY = scrollY;
    c.nFiles = len(l.thumbnails);
    c.listView = HomePageIsListView();
    c.sortByFreq = gGlobalPrefs && gGlobalPrefs->homePageSortByFrequentlyRead;
    c.showTips = gGlobalPrefs && gGlobalPrefs->showTips;
    c.tipIdx = gSelectedTipIdx;
    c.tipIsPromo = gSelectedIsPromo;
    str::ReplaceWithCopy(&c.filterText, filterText);
    c.rcThumbsArea = l.rcThumbsArea;
    c.rcSearchBorder = l.rcSearchBorder;
    c.rcIconOpen = l.rcIconOpen;
    c.rcIconListView = l.rcIconListView;
    c.rcIconThumbnailView = l.rcIconThumbnailView;
    c.rcTip = l.rcTip;
    c.rcFreqRead = l.freqRead ? l.freqRead->lastBounds : Rect{};
    c.rcOpenDoc = l.openDoc ? l.openDoc->lastBounds : Rect{};
    c.totalContentDy = l.totalContentDy;
    c.thumbsVisibleDy = l.thumbsVisibleDy;
    c.rcTipText = l.rcTipText;
    c.hasTip = l.hasTip;
    c.thumbs = l.thumbnails;
    c.filterWords = l.filterWords;
}

// rebuild chrome VirtText + copy cached geometry into l (no full layout)
static void ApplyHomeLayoutCache(HomePageLayout& l, int scrollY) {
    auto& c = gHomeLayoutCache;
    auto* win = l.win;
    auto* hdc = l.hdc;
    bool isRtl = IsUIRtl();

    // clamp scroll using cached content height
    int maxScrollY = std::max(0, c.totalContentDy - c.thumbsVisibleDy);
    if (scrollY > maxScrollY) {
        scrollY = maxScrollY;
        win->homePageScrollY = scrollY;
    }
    if (scrollY < 0) {
        scrollY = 0;
        win->homePageScrollY = 0;
    }

    int dy = c.scrollY - scrollY; // content moves opposite scroll direction
    OffsetThumbnailLayouts(c.thumbs, dy);
    c.scrollY = scrollY;

    l.rcThumbsArea = c.rcThumbsArea;
    l.rcSearchBorder = c.rcSearchBorder;
    l.rcIconOpen = c.rcIconOpen;
    l.rcIconListView = c.rcIconListView;
    l.rcIconThumbnailView = c.rcIconThumbnailView;
    l.rcTip = c.rcTip;
    l.totalContentDy = c.totalContentDy;
    l.thumbsVisibleDy = c.thumbsVisibleDy;
    l.rcTipText = c.rcTipText;
    l.hasTip = c.hasTip;
    l.thumbnails = c.thumbs;
    l.filterWords = c.filterWords;

    HFONT hdrFont = HdcCreateSimpleFont(hdc, "MS Shell Dlg", 24);
    HFONT fontText = HdcCreateSimpleFont(hdc, "MS Shell Dlg", 14);

    Str txt = _TRA("Recently Opened");
    if (gGlobalPrefs->homePageSortByFrequentlyRead) {
        txt = _TRA("Frequently Read");
    }
    HomeChromeCtrl* chrome = EnsureHomeChrome(win);
    VirtText* hdr = chrome->hdr;
    hdr->SetText(txt);
    hdr->font = GetPlatformFont(hdrFont);
    hdr->isRtl = isRtl;
    hdr->SetBounds(c.rcFreqRead);
    l.freqRead = hdr;

    VirtText* openDoc = chrome->openDoc->text;
    openDoc->SetText(_TRA("Open a document..."));
    openDoc->font = GetPlatformFont(fontText);
    openDoc->isRtl = isRtl;
    openDoc->withUnderline = true;
    openDoc->SetBounds(c.rcOpenDoc);
    l.openDoc = openDoc;
}

// after paint, keep lazy fileSize values in the cache for the next frame
static void SyncHomeLayoutCacheFileSizes(const HomePageLayout& l) {
    auto& c = gHomeLayoutCache;
    if (!c.valid || len(c.thumbs) != len(l.thumbnails)) {
        return;
    }
    for (int i = 0; i < len(c.thumbs); i++) {
        c.thumbs[i].fileSize = l.thumbnails[i].fileSize;
        // geometry may have been filled for newly visible list rows
        c.thumbs[i].rcListFileName = l.thumbnails[i].rcListFileName;
        c.thumbs[i].rcListPath = l.thumbnails[i].rcListPath;
        c.thumbs[i].rcListSize = l.thumbnails[i].rcListSize;
        c.thumbs[i].listTextMeasured = l.thumbnails[i].listTextMeasured;
        c.thumbs[i].szThumb = l.thumbnails[i].szThumb;
    }
}

static void LayoutHomePage(HomePageLayout& l) {
    EnsureTipsParsed();

    Vec<FileState*> allFileStates;
    if (gGlobalPrefs->homePageSortByFrequentlyRead) {
        gFileHistory.GetFrequencyOrder(allFileStates);
    } else {
        gFileHistory.GetRecentlyOpenedOrder(allFileStates);
    }
    auto* hdc = l.hdc;
    auto rc = l.rc;
    auto* win = l.win;

    // filter by search query if present
    TempStr searchQuery = nullptr;
    if (win->homeSearch) {
        searchQuery = win->homeSearch->GetTextTemp();
    }
    bool hasFilter = searchQuery && searchQuery.s[0];
    if (hasFilter) {
        SplitFilterToWords(searchQuery, l.filterWords);
    }
    Vec<FileState*> fileStates;
    for (int i = 0; i < len(allFileStates); i++) {
        FileState* fs = allFileStates[i];
        // a state without a path can't be opened or thumbnailed - don't show it
        if (len(fs->filePath) == 0) {
            continue;
        }
        if (hasFilter) {
            TempStr baseName = path::GetBaseNameTemp(fs->filePath);
            if (!FilterMatches(baseName, l.filterWords)) {
                continue;
            }
        }
        fileStates.Append(fs);
    }

    bool isRtl = IsUIRtl();
    HFONT fontText = HdcCreateSimpleFont(hdc, "MS Shell Dlg", 14);
    HFONT hdrFont = HdcCreateSimpleFont(hdc, "MS Shell Dlg", 24);

    // --- Pre-compute thumbnail grid x offset so header can align with it ---
    // use unfiltered count so layout stays stable when search filters results
    int nFilesForLayout = len(allFileStates);
    int colsForLayout =
        (rc.dx - kThumbsMarginLeft - kThumbsMarginRight + kThumbsSpaceBetweenX) / (kThumbnailDx + kThumbsSpaceBetweenX);
    int thumbsColsForLayout = std::max(colsForLayout, 1);
    int thumbsStartX = rc.x + kThumbsMarginLeft +
                       ((rc.dx - (thumbsColsForLayout * kThumbnailDx) -
                         ((thumbsColsForLayout - 1) * kThumbsSpaceBetweenX) - kThumbsMarginLeft - kThumbsMarginRight) /
                        2);
    if (thumbsStartX < DpiScale(kInnerPadding)) {
        thumbsStartX = DpiScale(kInnerPadding);
    } else if (nFilesForLayout == 0) {
        thumbsStartX = kThumbsMarginLeft;
    }
    int thumbsContentWidth = (thumbsColsForLayout * kThumbnailDx) + ((thumbsColsForLayout - 1) * kThumbsSpaceBetweenX);

    // --- Step 1: layout header at the top ---
    Rect rcIconView(0, 0, 0, 0);
    rcIconView.dx = rcIconView.dy = HomePageIconSize();

    Str txt = _TRA("Recently Opened");
    if (gGlobalPrefs->homePageSortByFrequentlyRead) {
        txt = _TRA("Frequently Read");
    }
    HomeChromeCtrl* chrome = EnsureHomeChrome(win);
    VirtText* hdr = chrome->hdr;
    hdr->SetText(txt);
    hdr->font = GetPlatformFont(hdrFont);
    l.freqRead = hdr;
    hdr->isRtl = isRtl;
    Size txtSize = hdr->GetIdealSize(true);

    int hdrY = DpiScale(8);
    int iconGap = DpiScale(4);
    int titleGap = DpiScale(8);
    int viewIconsDx = (2 * rcIconView.dx) + iconGap;
    Rect rcHdr(thumbsStartX + viewIconsDx + titleGap, hdrY, txtSize.dx, txtSize.dy);
    l.rcIconThumbnailView = {thumbsStartX, rcHdr.y + ((rcHdr.dy - rcIconView.dy) / 2), rcIconView.dx, rcIconView.dy};
    l.rcIconListView = {l.rcIconThumbnailView.x + rcIconView.dx + iconGap, l.rcIconThumbnailView.y, rcIconView.dx,
                        rcIconView.dy};
    if (isRtl) {
        int groupDx = viewIconsDx + titleGap + rcHdr.dx;
        int groupX = rc.dx - thumbsStartX - groupDx;
        rcHdr.x = groupX;
        l.rcIconListView = {rcHdr.x + rcHdr.dx + titleGap, l.rcIconListView.y, rcIconView.dx, rcIconView.dy};
        l.rcIconThumbnailView = {l.rcIconListView.x + rcIconView.dx + iconGap, l.rcIconThumbnailView.y, rcIconView.dx,
                                 rcIconView.dy};
    }
    hdr->SetBounds(rcHdr);

    /* "Open a document" link next to header */
    Rect rcIconOpen(0, 0, 0, 0);
    rcIconOpen.dx = rcIconOpen.dy = HomePageIconSize();

    txt = _TRA("Open a document...");
    VirtText* openDoc = chrome->openDoc->text;
    openDoc->SetText(txt);
    openDoc->font = GetPlatformFont(fontText);
    openDoc->isRtl = isRtl;
    openDoc->withUnderline = true;
    txtSize = openDoc->GetIdealSize(true);

    int openDocSpacing = DpiScale(16);
    rcIconOpen.x = rcHdr.x + rcHdr.dx + openDocSpacing;
    rcIconOpen.y = rcHdr.y + rcHdr.dy - rcIconOpen.dy - kOpenDocumentYShift + 3;
    if (isRtl) {
        rcIconOpen.x = rcHdr.x - openDocSpacing - rcIconOpen.dx;
    }
    l.rcIconOpen = rcIconOpen;

    Rect rcOpenDoc(rcIconOpen.x + rcIconOpen.dx + 3, rcHdr.y + rcHdr.dy - txtSize.dy - kOpenDocumentYShift, txtSize.dx,
                   txtSize.dy);
    if (isRtl) {
        rcOpenDoc.x = rcIconOpen.x - rcOpenDoc.dx - 3;
    }
    openDoc->SetBounds(rcOpenDoc);

    l.openDoc = openDoc;

    int headerBottomY = rcHdr.y + rcHdr.dy;

    // --- Position search edit below header ---
    EnsureHomeSearchCreated(win);
    int searchEditDy = DpiScale(kSearchEditDy);
    int headerSearchGap = DpiScale(kHeaderSearchGapY);
    int searchThumbsGap = DpiScale(kSearchThumbnailsGapY);
    {
        int borderDx = thumbsContentWidth * 3 / 4;
        borderDx = std::max(borderDx, DpiScale(200));
        int borderX = thumbsStartX + ((thumbsContentWidth - borderDx) / 2);
        int borderY = headerBottomY + headerSearchGap;
        int borderDy = searchEditDy + 2; // 1px border on each side
        l.rcSearchBorder = {borderX, borderY, borderDx, borderDy};
        // inside the 1px border: the layout gives the edit the full width and
        // its own (text-sized) height, centered vertically
        Rect rcEdit = {borderX + 1, borderY + 1, borderDx - 2, searchEditDy};
        LayoutToSize(win->homeSearchLayout, rcEdit.Size());
        win->homeSearchLayout->SetBounds(rcEdit);
    }
    // border is 1px top + 1px bottom = 2px
    int searchAreaDy = headerSearchGap + searchEditDy + 2 + searchThumbsGap;
    headerBottomY += searchAreaDy;

    // --- Step 2: calculate tip area at the bottom (before thumbnails) ---
    int tipHeight = 0;
    HFONT fontTip = HdcCreateSimpleFont(hdc, "MS Shell Dlg", 16);
    HomeTipCtrl* tipCtrl = EnsureHomeChrome(l.win)->tip;
    tipCtrl->SetTipLine(SelectedTipLine(), GetPlatformFont(fontTip));
    VirtRichText* tip = tipCtrl->rich;
    if (tip) {
        int tipPadding = DpiScale(8);
        tipHeight = tip->MinIntrinsicHeight(thumbsContentWidth) + (2 * tipPadding);
    }

    // --- Step 3: middle area for thumbnails/list ---
    // content starts directly after headerBottomY (which includes kSearchThumbnailsGapY)
    int thumbsTopY = headerBottomY;
    int thumbsBottomY = rc.dy - tipHeight - kThumbsMiddleMargin;
    int thumbsVisibleDy = std::max(0, thumbsBottomY - thumbsTopY);

    l.rcThumbsArea = {0, thumbsTopY, rc.dx, thumbsVisibleDy};

    int nFiles = len(fileStates);
    bool showList = HomePageIsListView();
    // Leave room above the first row so RoundRect / selection outline top edges
    // aren't clipped by rcThumbsArea (they extend a few px upward).
    int thumbsContentPadTop = showList ? DpiScale(2) : DpiScale(5);
    int thumbsRows = 0;
    int thumbsContentDy = 0;
    if (showList) {
        thumbsRows = nFiles;
        thumbsContentDy = nFiles * kHomeListRowDy;
    } else {
        thumbsRows = (nFiles + thumbsColsForLayout - 1) / thumbsColsForLayout;
        if (thumbsRows > 0) {
            thumbsContentDy = (thumbsRows * (kThumbnailDy + kThumbsSpaceBetweenY)) - kThumbsSpaceBetweenY;
        }
    }
    if (thumbsContentDy > 0) {
        thumbsContentDy += thumbsContentPadTop;
    }

    int scrollY = win->homePageScrollY;
    int maxScrollY = std::max(0, thumbsContentDy - thumbsVisibleDy);
    if (scrollY > maxScrollY) {
        scrollY = maxScrollY;
        win->homePageScrollY = scrollY;
    }
    l.totalContentDy = thumbsContentDy;
    l.thumbsVisibleDy = thumbsVisibleDy;

    Point ptOff(thumbsStartX, thumbsTopY + thumbsContentPadTop - scrollY);

    if (showList) {
        int listX = thumbsStartX;
        if (isRtl) {
            listX = rc.dx - thumbsStartX - thumbsContentWidth;
        }
        int listIconDx = l.rcIconListView.dx;
        int listIconGap = DpiScale(6);
        // fixed size column — never call file::GetSize during layout (disk/network I/O)
        int listSizeDx = DpiScale(56);
        // one-row margin so a quick scroll still has measured name/path splits ready
        int listPrefetchY = kHomeListRowDy;
        for (int row = 0; row < nFiles; row++) {
            ThumbnailLayout& thumb = *l.thumbnails.AppendBlanks(1);
            thumb.fileSize = kSizeNotFetched;
            FileState* fs = fileStates[row];
            thumb.fs = fs;
            Rect rcRow(listX, ptOff.y + (row * kHomeListRowDy), thumbsContentWidth, kHomeListRowDy);
            thumb.rcListRow = rcRow;
            bool onScreen = IsHomeThumbOnScreen(rcRow, l.rcThumbsArea, listPrefetchY);

            Rect rcThumb(rcRow.x, rcRow.y + ((rcRow.dy - kHomeListThumbDy) / 2), kHomeListThumbDx, kHomeListThumbDy);
            Rect rcPin(rcRow.x + rcRow.dx - listIconDx, rcRow.y + ((rcRow.dy - listIconDx) / 2), listIconDx,
                       listIconDx);
            Rect rcRemove(rcPin.x - listIconGap - listIconDx, rcPin.y, listIconDx, listIconDx);
            Rect rcSize(rcRemove.x - listIconGap - listSizeDx, rcRow.y, listSizeDx, rcRow.dy);
            Rect rcFileName(rcThumb.x + rcThumb.dx + kHomeListRowGapDx, rcRow.y,
                            rcSize.x - (rcThumb.x + rcThumb.dx + kHomeListRowGapDx) - kHomeListRowGapDx, rcRow.dy);
            if (isRtl) {
                rcThumb.x = rcRow.x + rcRow.dx - rcThumb.dx;
                rcPin.x = rcRow.x;
                rcRemove.x = rcPin.x + listIconDx + listIconGap;
                rcSize.x = rcRemove.x + listIconDx + listIconGap;
                rcFileName.x = rcSize.x + rcSize.dx + kHomeListRowGapDx;
                rcFileName.dx = rcThumb.x - rcFileName.x - kHomeListRowGapDx;
            }
            rcFileName.dx = std::max(rcFileName.dx, 0);
            // rcFileName is the whole name+path span; MeasureHomeListRowText()
            // splits it when the row is first painted. Doing it here would
            // measure text for every history entry on every layout, and doing it
            // only for rows that happen to be on screen *now* left rows scrolled
            // in later without their directory (#5870 follow-up).
            thumb.rcListThumb = rcThumb;
            thumb.rcListPin = rcPin;
            thumb.rcListRemove = rcRemove;
            thumb.rcListSize = rcSize;
            thumb.rcListFileName = rcFileName;
            // already-cached in-memory thumb size only (no LoadThumbnail / disk)
            if (onScreen && fs->thumbnail) {
                thumb.szThumb = Size(fs->thumbnail->width, fs->thumbnail->height);
            }
        }
    } else {
        int thumbPrefetchY = kThumbnailDy + kThumbsSpaceBetweenY;
        for (int row = 0; row < thumbsRows; row++) {
            for (int col = 0; col < thumbsColsForLayout; col++) {
                if ((row * thumbsColsForLayout) + col >= nFiles) {
                    // no more files to display
                    thumbsRows = col > 0 ? row + 1 : row;
                    break;
                }
                ThumbnailLayout& thumb = *l.thumbnails.AppendBlanks(1);
                thumb.fileSize = kSizeNotFetched;
                FileState* fs = fileStates[(row * thumbsColsForLayout) + col];
                thumb.fs = fs;

                Rect rcPage(ptOff.x + (col * (kThumbnailDx + kThumbsSpaceBetweenX)),
                            ptOff.y + (row * (kThumbnailDy + kThumbsSpaceBetweenY)), kThumbnailDx, kThumbnailDy);
                if (isRtl) {
                    rcPage.x = rc.dx - rcPage.x - rcPage.dx;
                }
                bool onScreen = IsHomeThumbOnScreen(rcPage, l.rcThumbsArea, thumbPrefetchY);
                // only use already-resident thumbnails for aspect adjust — never LoadThumbnail
                // during layout (disk I/O dominated scroll/paint CPU)
                if (onScreen && fs->thumbnail) {
                    Size szThumb(fs->thumbnail->width, fs->thumbnail->height);
                    if (szThumb.dx != kThumbnailDx || szThumb.dy != kThumbnailDy) {
                        rcPage.dy = szThumb.dy * kThumbnailDx / szThumb.dx;
                        rcPage.y += kThumbnailDy - rcPage.dy;
                    }
                    thumb.szThumb = szThumb;
                }
                thumb.rcPage = rcPage;
                int iconSpace = DpiScale(20);
                Rect rcText(rcPage.x + iconSpace, rcPage.y + rcPage.dy + 3, rcPage.dx - iconSpace, iconSpace);
                if (isRtl) {
                    rcText.x -= iconSpace;
                }
                thumb.rcText = rcText;
            }
        }
    }

    // layout tip at the bottom
    if (tip) {
        Rect rcClient = HwndClientRect(win->hwndCanvas);
        int tipPadding = DpiScale(8);

        int tipY = rcClient.dy - tipHeight;
        // background spans full window width
        l.rcTip = {0, tipY, rcClient.dx, tipHeight};
        l.hasTip = true;

        // text area aligned with thumbnails
        int tipStartX = thumbsStartX;
        int tipStartY = tipY + tipPadding;
        l.rcTipText = {tipStartX, tipStartY, thumbsContentWidth, tip->MinIntrinsicHeight(thumbsContentWidth)};
    }
}

static void GetFileStateIcon(FileState* fs) {
    if (fs->himl) {
        return;
    }
    SHFILEINFO sfi{};
    sfi.iIcon = -1;
    uint flags = SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES;
    WCHAR* filePathW = CWStrTemp(fs->filePath);
    fs->himl = (HIMAGELIST)SHGetFileInfoW(filePathW, 0, &sfi, sizeof(sfi), flags);
    fs->iconIdx = sfi.iIcon;
}

// --- Close (✕) button for Frequently Read thumbnails (issue #283, #5745) ---
//
// Drawn onto the home-page canvas (over the top-right corner of the thumbnail
// under the mouse) rather than as a separate top-level window. The separate
// window could be left behind, drawing stray crosses over a document (#5745).
// Styled like the tab close button (gray X on a white circle; red circle +
// white X on hover). It is a HomeCloseBtnCtrl in the chrome tree, shown on the
// entry the mouse is on.

static void DrawHomeViewButton(Gfx* gfx, Pixmap* icon, Rect r, bool selected) {
    if (selected) {
        Color bg = ThemeControlBackgroundColor();
        gfx->FillRect(r, bg);
        gfx->DrawRect(r, AccentColor(bg, 40));
    }
    if (icon) {
        gfx->DrawPixmap(icon, {r.x, r.y, icon->width, icon->height});
    }
}

static Rect FitRectInRect(Size src, Rect dst) {
    if (src.dx <= 0 || src.dy <= 0 || dst.dx <= 0 || dst.dy <= 0) {
        return dst;
    }
    int dx = dst.dx;
    int dy = src.dy * dx / src.dx;
    if (dy > dst.dy) {
        dy = dst.dy;
        dx = src.dx * dy / src.dy;
    }
    Rect r(dst.x + ((dst.dx - dx) / 2), dst.y + ((dst.dy - dy) / 2), dx, dy);
    return r;
}

static TempStr FileSizeForHomeListTemp(i64 size) {
    if (size < 0) {
        return str::DupTemp("");
    }
    return str::FormatSizeShortTemp(size, nullptr);
}

// light blue outline marking the keyboard-selected entry (issue #1136).
// A fixed color: it has to read as "selected" against both the light and the
// dark page background
constexpr Color kHomeSelectionColor = MkRgb(0x4c, 0xa6, 0xff);

static void DrawHomeSelectionOutline(HDC hdc, const Rect& r, int radius) {
    int penDx = DpiScale(2);
    ScopedSelectObject pen(hdc, CreatePen(PS_SOLID, penDx, kHomeSelectionColor), true);
    ScopedSelectObject brush(hdc, GetStockBrush(NULL_BRUSH));
    RoundRect(hdc, r.x, r.y, r.x + r.dx, r.y + r.dy, radius, radius);
}

// Give the file name the width it needs and put the directory path in what's
// left, right-aligned (mirrored for RTL). Done on first paint of a row, not
// during layout: measuring every history entry made layout (and so scrolling)
// slow, and measuring only the rows visible at layout time meant rows scrolled
// into view later never got a directory.
static void MeasureHomeListRowText(HDC hdc, ThumbnailLayout& thumb, HFONT font, bool isRtl) {
    if (thumb.listTextMeasured) {
        return;
    }
    thumb.listTextMeasured = true;

    Rect rcFileName = thumb.rcListFileName;
    TempStr fileName = path::GetBaseNameTemp(thumb.fs->filePath);
    int nameDx = HdcMeasureText(hdc, Str(fileName), font).dx + DpiScale(4);
    int minPathDx = DpiScale(80);
    if (nameDx + kHomeListRowGapDx + minPathDx > rcFileName.dx) {
        // no room for a path, the name gets the whole span
        return;
    }
    int pathDx = rcFileName.dx - nameDx - kHomeListRowGapDx;
    if (isRtl) {
        thumb.rcListPath = Rect(rcFileName.x, rcFileName.y, pathDx, rcFileName.dy);
        rcFileName.x = rcFileName.x + rcFileName.dx - nameDx;
    } else {
        thumb.rcListPath = Rect(rcFileName.x + nameDx + kHomeListRowGapDx, rcFileName.y, pathDx, rcFileName.dy);
    }
    rcFileName.dx = nameDx;
    thumb.rcListFileName = rcFileName;
}

// True when keyboard focus is in the home search box (hide list selection then).
static bool HomeSearchHasFocus(MainWindow* win) {
    return win && win->homeSearch && GetFocus() == win->homeSearch->hwnd;
}

static void DrawHomeListRow(HomePageLayout& l, ThumbnailLayout& thumb, HFONT fontText, Color backgroundColor,
                            bool isRtl, bool isSelected) {
    HDC hdc = l.hdc;
    FileState* fs = thumb.fs;
    Rect row = thumb.rcListRow;
    if (!IsHomeThumbOnScreen(row, l.rcThumbsArea)) {
        return;
    }
    MeasureHomeListRowText(hdc, thumb, fontText, isRtl);
    // no selection chrome while typing in the search box
    if (isSelected && !HomeSearchHasFocus(l.win)) {
        DrawHomeSelectionOutline(hdc, Rect(row.x, row.y, row.dx, row.dy - 1), 4);
    }

    Color lineCol = AccentColor(ThemeMainWindowBackgroundColor(), 30);
    ScopedSelectObject pen(hdc, CreatePen(PS_SOLID, 1, lineCol), true);
    HdcDrawLine(hdc, Rect(row.x, row.y + row.dy - 1, row.dx, 0));

    // LoadThumbnail only hits disk the first time; result stays on fs->thumbnail
    Pixmap* thumbImg = LoadThumbnail(fs);
    Rect thumbBox = thumb.rcListThumb;
    if (thumbImg) {
        Size szThumb(thumbImg->width, thumbImg->height);
        Rect thumbDst = FitRectInRect(szThumb, thumbBox);
        BlitPixmap(thumbImg, hdc, thumbDst);
        thumb.szThumb = szThumb;
    }
    Str path = fs->filePath;
    TempStr fileName = path::GetBaseNameTemp(path);
    u32 nameFmt = gfxTextEllipsis | gfxTextVCenter | (isRtl ? gfxTextRight : gfxTextLeft);
    SelectObject(hdc, fontText);
    {
        GfxHdc gfx(hdc);
        DrawMaybeHighlightedText(&gfx, thumb.rcListFileName, fileName, l.filterWords, l.highlighted, backgroundColor,
                                 isRtl, false, nameFmt, GetPlatformFont(fontText));
    }

    // directory path, right-aligned and muted, in the space the file name doesn't need.
    // Must use DrawTextW: dirPath is UTF-8; DrawTextA treated it as the system ANSI
    // code page and mangled non-ASCII path characters (#5824).
    if (!thumb.rcListPath.IsEmpty()) {
        TempStr dirPath = path::GetDirTemp(path);
        SetTextColor(hdc, ThemeWindowTextDisabledColor());
        UINT pathFmt = DT_SINGLELINE | DT_VCENTER | DT_PATH_ELLIPSIS | DT_NOPREFIX | (isRtl ? DT_LEFT : DT_RIGHT);
        Rect pathRect = thumb.rcListPath;
        HdcDrawText(hdc, dirPath, pathRect, pathFmt);
    }

    // file::GetSize once per row, then cache on ThumbnailLayout (scroll reuses
    // it). kSizeNotFetched = not tried; kSizeFetchFail = GetSize failed; >= 0
    // is a real size (including empty files).
    if (thumb.fileSize == kSizeNotFetched) {
        i64 sz = file::GetSize(path);
        thumb.fileSize = (sz < 0) ? kSizeFetchFail : sz;
    }
    TempStr fileSize = FileSizeForHomeListTemp(thumb.fileSize);
    SetTextColor(hdc, ThemeWindowTextColor());
    UINT sizeFmt = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX | (isRtl ? DT_LEFT : DT_RIGHT);
    Rect sizeRect = thumb.rcListSize;
    HdcDrawText(hdc, fileSize, sizeRect, sizeFmt);

    if (fs->isPinned) {
        HdcFillRect(hdc, thumb.rcListPin, ThemeControlBackgroundColor());
    }
    {
        int pinDx = thumb.rcListPin.dx > 0 ? thumb.rcListPin.dx : DpiScale(16);
        int pinDy = thumb.rcListPin.dy > 0 ? thumb.rcListPin.dy : pinDx;
        Pixmap* pin = GetCachedPixmapForSvg(gIconPin, pinDx, pinDy);
        if (pin) {
            BlitPixmapAlpha(pin, hdc, thumb.rcListPin);
        }
    }
}

// a white circle with a black "?" inside: the home page's affordance for the
// keyboard-shortcuts sheet. The disc is drawn with GDI+ so its edge is smooth;
// nothing is painted outside it, so the page background shows through.
static void DrawHomeHelpButton(Gfx* gfx, Rect r) {
    gfx->FillEllipse(r, kColWhite);
    // the screen dc is only for sizing the font (it is cached and interned by
    // both helpers, so this doesn't create anything per paint)
    AutoReleaseDC dc(nullptr);
    PlatformFont* font = GetPlatformFont(HdcCreateSimpleFont(dc, "MS Shell Dlg", 14));
    gfx->DrawText("?", r, gfxTextCenter | gfxTextVCenter, font, kColBlack);
}

// What the home page list drew for each row: the path, the size text as drawn,
// and the size column's rect. Reads the layout cache, so it needs a paint to
// have happened; NOTREADY until then. A test can't sample the size text from
// the pixels instead: the column's position depends on the window width, the
// DPI and the theme, so a fixed sample band lands on the path (identical for
// two renders of the same file) on any other machine. Used by tests/issue-5870.ts.
TempStr HomeListRowsResultTemp(int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    auto& c = gHomeLayoutCache;
    if (!c.valid) {
        out.Append("NOTREADY no-layout\n");
        return finish(2);
    }
    if (!c.listView) {
        out.Append("ERROR not-list-view\n");
        return finish(1);
    }
    out.Append(fmt("OK rows=%d\n", len(c.thumbs)));
    for (int i = 0; i < len(c.thumbs); i++) {
        ThumbnailLayout& t = c.thumbs[i];
        Rect r = t.rcListSize;
        Str path = t.fs ? t.fs->filePath : Str{};
        // fileSize is fetched when a row is first drawn, so an off-screen row
        // still reads kSizeNotFetched and its size text is empty
        out.Append(fmt("row=%d size='%s' sizeRect=%d,%d,%d,%d path=%s\n", i, FileSizeForHomeListTemp(t.fileSize), r.x,
                       r.y, r.dx, r.dy, path));
    }
    return finish(0);
}

//--- home page chrome VirtCtrls

HomeViewIconCtrl::HomeViewIconCtrl() {
    cursor = IDC_HAND;
}

void HomeViewIconCtrl::Paint(VirtPaintCtx& ctx) {
    bool selected = (listView == HomePageIsListView());
    DrawHomeViewButton(ctx.gfx, pixmap, ctx.bounds, selected);
}

HomeOpenDocCtrl::HomeOpenDocCtrl() {
    cursor = IDC_HAND;
}

void HomeOpenDocCtrl::Paint(VirtPaintCtx& ctx) {
    if (!pixmap) {
        return;
    }
    Rect r = {ctx.bounds.x + rcIconLocal.x, ctx.bounds.y + rcIconLocal.y, pixmap->width, pixmap->height};
    ctx.gfx->DrawPixmap(pixmap, r);
}

HomeHelpBtnCtrl::HomeHelpBtnCtrl() {
    cursor = IDC_HAND;
    SetTooltip(_TRA("Keyboard Shortcuts"));
}

void HomeHelpBtnCtrl::Paint(VirtPaintCtx& ctx) {
    DrawHomeHelpButton(ctx.gfx, ctx.bounds);
}

//--- tip links

HomeTipCtrl::~HomeTipCtrl() {
    str::Free(richFor);
}

// re-parses when the markup changes; the parse is what the band draws
void HomeTipCtrl::SetTipLine(Str line, PlatformFont* font) {
    if (rich && str::Eq(richFor, line)) {
        rich->font = font;
        rich->hwndForCmds = hwndForCmds;
        return;
    }
    if (rich) {
        RemoveChild(rich, true);
        rich = nullptr;
    }
    str::ReplaceWithCopy(&richFor, line);
    if (!line) {
        return;
    }
    rich = ParseTip(line);
    rich->font = font;
    rich->hwndForCmds = hwndForCmds;
    AddChild(rich);
}

// rcTip is the whole band (its background), rcText where the markup goes
void HomeTipCtrl::Sync(const Rect& rcTip, const Rect& rcText) {
    if (!rich || rcTip.IsEmpty()) {
        visibility = Visibility::Collapse;
        return;
    }
    visibility = Visibility::Visible;
    SetBounds(rcTip);
    rich->textColor = ThemeWindowTextColor();
    rich->linkColor = ThemeWindowLinkColor();
    rich->bgColor = ThemeControlBackgroundColor();
    rich->SetBounds(rcText);
}

//--- file entries

static Rect HomeEntryRect(const ThumbnailLayout& t);

// the ✕ sits in the top-right corner of the thumbnail (top-left in RTL)
static Rect HomeCloseBtnRectForThumb(MainWindow* win, const Rect& thumb) {
    int sz = DpiScale(18);
    int margin = DpiScale(5);
    int bx = IsUIRtl() ? (thumb.x + margin) : (thumb.x + thumb.dx - sz - margin);
    int by = thumb.y + margin;
    return {bx, by, sz, sz};
}

// the ✕ of a thumbnail / list row: forget the file it belongs to
static void HomeForgetEntryClicked(MainWindow* win, VirtMouseEvent* ev) {
    auto* entry = (HomeEntryCtrl*)ev->target->parent;
    TempStr path = str::DupTemp(entry->filePath);
    if (len(path) > 0) {
        ForgetFileFromFrequentlyRead(win, path);
    }
}

static void HomePinEntryClicked(MainWindow* win, VirtMouseEvent* ev) {
    auto* entry = (HomeEntryCtrl*)ev->target->parent;
    TempStr path = str::DupTemp(entry->filePath);
    if (len(path) == 0) {
        return;
    }
    FileState* fs = gFileHistory.FindByPath(path);
    if (!fs) {
        return;
    }
    fs->isPinned = !fs->isPinned;
    SaveSettings();
    win->DeleteToolTip();
    win->RedrawAll(true);
}

static void HomeEntryOpenClicked(MainWindow* win, VirtMouseEvent* ev) {
    auto* entry = (HomeEntryCtrl*)ev->target;
    if (len(entry->filePath) == 0) {
        return;
    }
    LoadArgs args(entry->filePath, win);
    // ctrl forces always opening
    args.activateExisting = !ev->isCtrl;
    args.activateExistingInWindow = true;
    StartLoadDocument(&args);
}

static void HomeViewModeClicked(MainWindow* win, VirtMouseEvent* ev) {
    auto* btn = (HomeViewIconCtrl*)ev->target;
    if (btn->listView == HomePageIsListView()) {
        return;
    }
    SetHomePageListView(btn->listView);
    win->homePageScrollY = 0;
    SaveSettings();
    win->RedrawAll(true);
}

static void HomeOpenDocClicked(MainWindow* win, VirtMouseEvent*) {
    HwndSendCommand(win->hwndFrame, CmdOpenFile);
}

static void HomeHelpClicked(MainWindow* win, VirtMouseEvent*) {
    HwndSendCommand(win->hwndFrame, CmdToggleKeyboardHelp);
}

static void HomeTipBandClicked(MainWindow* win, VirtMouseEvent*) {
    PickAnotherRandomPromotion();
    win->RedrawAll(true);
}

HomeListIconCtrl::HomeListIconCtrl() {
    cursor = IDC_HAND;
    onGetTooltip = MkMethod1<HomeListIconCtrl, VirtTooltipEvent*, &HomeListIconCtrl::OnGetTooltip>(this);
}

void HomeListIconCtrl::OnGetTooltip(VirtTooltipEvent* ev) {
    auto* entry = (HomeEntryCtrl*)parent;
    FileState* fs = gFileHistory.FindByPath(entry->filePath);
    bool pinned = fs && fs->isPinned;
    ev->tip = str::DupTemp(pinned ? _TRA("Unpin") : _TRA("Pin"));
}

HomeEntryCtrl::~HomeEntryCtrl() {
    str::Free(filePath);
}

HomeEntryCtrl::HomeEntryCtrl() {
    cursor = IDC_HAND;
}

HomeEntryCtrl* HomeEntriesCtrl::EntryAt(int idx) {
    return (HomeEntryCtrl*)ChildAt(idx);
}

// the wnd the mouse is on may be one of an entry's buttons
HomeEntryCtrl* HomeEntriesCtrl::EntryForCtrl(VirtCtrl* w) {
    while (w && w != this) {
        if (w->parent == this) {
            return (HomeEntryCtrl*)w;
        }
        w = w->parent;
    }
    return nullptr;
}

void HomeEntriesCtrl::SetEntryCount(int n) {
    while (ChildCount() > n) {
        RemoveChild(children[ChildCount() - 1], true);
    }
    while (ChildCount() < n) {
        auto* e = new HomeEntryCtrl();
        e->idx = ChildCount();
        e->onClick = MkFunc1(HomeEntryOpenClicked, win);

        e->closeBtn = new VirtCloseButton();
        // it sits on the thumbnail, so it needs the circle behind it
        e->closeBtn->withCircle = true;
        e->closeBtn->SetTooltip(_TRA("Remove from Frequently Read"));
        e->closeBtn->onClick = MkFunc1(HomeForgetEntryClicked, win);
        e->closeBtn->visibility = Visibility::Collapse;
        e->AddChild(e->closeBtn);

        e->removeBtn = new VirtCloseButton();
        e->removeBtn->SetTooltip(_TRA("Remove from Frequently Read"));
        e->removeBtn->onClick = MkFunc1(HomeForgetEntryClicked, win);
        e->removeBtn->visibility = Visibility::Collapse;
        e->AddChild(e->removeBtn);

        e->pinBtn = new HomeListIconCtrl();
        e->pinBtn->onClick = MkFunc1(HomePinEntryClicked, win);
        e->pinBtn->visibility = Visibility::Collapse;
        e->AddChild(e->pinBtn);

        AddChild(e);
    }
    if (activeIdx >= n) {
        activeIdx = -1;
    }
}

// the ✕ shows on the entry the mouse is on, and only in thumbnail view
void HomeEntriesCtrl::UpdateCloseBtnVisibility() {
    bool canShow = CanAccessDisk() && !HomePageIsListView();
    int n = ChildCount();
    for (int i = 0; i < n; i++) {
        HomeEntryCtrl* e = EntryAt(i);
        bool show = canShow && (i == activeIdx);
        e->closeBtn->visibility = show ? Visibility::Visible : Visibility::Collapse;
    }
}

void HomeEntriesCtrl::SetActiveEntry(int idx) {
    if (idx == activeIdx) {
        return;
    }
    activeIdx = idx;
    UpdateCloseBtnVisibility();
    if (idx >= 0 && idx != win->homePageSelIdx) {
        win->homePageSelIdx = idx;
    }
    HwndInvalidate(win->hwndCanvas);
    if (idx >= 0) {
        // the tip is anchored to the active entry, never to the cursor
        HomePageShowSelectionTooltip(win);
    }
}

// mouse events bubble up to us from the entry (or one of its buttons) that was
// hit, so this is where the active entry is tracked
HomeEntriesCtrl::HomeEntriesCtrl() {
    onMouseMove = MkMethod1<HomeEntriesCtrl, VirtMouseEvent*, &HomeEntriesCtrl::OnMouseMove>(this);
}

void HomeEntriesCtrl::OnMouseMove(VirtMouseEvent* ev) {
    // keyboard nav invalidates the canvas and Windows may re-send WM_MOUSEMOVE
    // with the same coordinates: ignore those so the selection doesn't snap
    // back under a stationary cursor
    if (ev->ptWindow == lastHoverPt) {
        return;
    }
    lastHoverPt = ev->ptWindow;
    HomeEntryCtrl* e = EntryForCtrl(ev->hit);
    SetActiveEntry(e ? e->idx : -1);
    return;
}

// created once per window so that hover / pressed state survives the repaints
// that scrolling and filtering cause
static HomeChromeCtrl* EnsureHomeChrome(MainWindow* win) {
    // the canvas root holds either the home page's chrome or the About page's
    // controls, depending on which one is showing
    if (win->homeRoot && IsVirtCtrlOfKind(win->homeRoot->owned, kindHomeChromeCtrl)) {
        return (HomeChromeCtrl*)win->homeRoot->owned;
    }
    HWND hwnd = win->hwndCanvas;
    if (!win->homeRoot) {
        win->homeRoot = new VirtRoot(hwnd);
    }

    auto* chrome = new HomeChromeCtrl();
    chrome->kind = kindHomeChromeCtrl;
    chrome->flags |= vwfNoHitTest;

    // first, so that the rest of the chrome (notably the help button, which can
    // overlap the thumbnails) hit-tests and paints on top of the entries
    // below everything else: the tip band sits at the bottom of the page
    chrome->tip = new HomeTipCtrl();
    chrome->tip->hwndForCmds = win->hwndFrame;
    chrome->tip->onClick = MkFunc1(HomeTipBandClicked, win);
    chrome->AddChild(chrome->tip);

    chrome->entries = new HomeEntriesCtrl();
    chrome->entries->win = win;
    // hit-testable so that moving into the gaps between thumbnails still
    // reaches OnMouseMove() and clears the active entry
    chrome->entries->flags |= vwfClipChildren;
    chrome->AddChild(chrome->entries);

    chrome->thumbView = new HomeViewIconCtrl();
    chrome->thumbView->listView = false;
    chrome->thumbView->SetTooltip(_TRA("Show as thumbnails"));
    chrome->thumbView->onClick = MkFunc1(HomeViewModeClicked, win);
    chrome->AddChild(chrome->thumbView);

    chrome->listView = new HomeViewIconCtrl();
    chrome->listView->listView = true;
    chrome->listView->SetTooltip(_TRA("Show as list"));
    chrome->listView->onClick = MkFunc1(HomeViewModeClicked, win);
    chrome->AddChild(chrome->listView);

    chrome->hdr = new VirtText(StrL(""));
    chrome->AddChild(chrome->hdr);

    chrome->openDoc = new HomeOpenDocCtrl();
    chrome->openDoc->text = new VirtText(StrL(""));
    chrome->openDoc->text->withUnderline = true;
    chrome->openDoc->AddChild(chrome->openDoc->text);
    chrome->openDoc->onClick = MkFunc1(HomeOpenDocClicked, win);
    chrome->AddChild(chrome->openDoc);

    chrome->helpBtn = new HomeHelpBtnCtrl();
    chrome->helpBtn->onClick = MkFunc1(HomeHelpClicked, win);
    chrome->AddChild(chrome->helpBtn);

    win->homeRoot->SetChild(chrome);
    return chrome;
}

void HomePageDestroyChrome(MainWindow* win) {
    delete win->homeRoot;
    win->homeRoot = nullptr;
}

// gives the home page's virtual controls first shot at the canvas messages.
// Returns true when the event was consumed and the caller should stop
bool HomePageOnCanvasMessage(MainWindow* win, UINT msg, WPARAM wp, LPARAM lp, LRESULT& res) {
    VirtRoot* root = win->homeRoot;
    if (!root || !root->owned) {
        return false;
    }
    // Hover feedback (highlight, ✕ button, tooltips) must stay quiet while
    // another window is in front. The mouse still moves over the home page when
    // e.g. the command palette or the theme window is up, and putting a tooltip
    // over them steals activation: the palette closes on kill-focus and the
    // theme window ends up behind the main window. Clicks are exempt: clicking
    // a background window is meant to activate it.
    bool isHoverMsg = (msg == WM_MOUSEMOVE) || (msg == WM_SETCURSOR);
    if (isHoverMsg && GetForegroundWindow() != win->hwndFrame) {
        return false;
    }
    if (msg != WM_SETCURSOR) {
        bool didHandle = root->OnMessage(msg, wp, lp, res);
        // moving outside the entries band (or off the canvas) drops the active
        // entry, so the ✕ button goes away
        if (msg == WM_MOUSEMOVE && !root->hovered) {
            HomeEntriesCtrl* entries = HomeEntries(win);
            if (entries) {
                entries->SetActiveEntry(-1);
            }
        }
        return didHandle;
    }
    Point pt = HwndGetCursorPos(win->hwndCanvas);
    Point ptLocal{0, 0};
    VirtCtrl* w = CtrlFromPoint(root, pt, &ptLocal);
    if (!w || !w->OnSetCursor(ptLocal)) {
        return false;
    }
    // no tooltip of its own means "leave the tooltip alone", not "hide it": a
    // file entry's tip is put up by HomePageShowSelectionTooltip() and would
    // otherwise be torn down by the WM_SETCURSOR that follows the hover
    TempStr tip = w->GetTooltipTemp(ptLocal);
    if (tip && *tip.s) {
        Rect r = w->BoundsInWindow();
        win->ShowToolTip(tip, r);
    }
    res = TRUE;
    return true;
}

// feeds the geometry LayoutHomePage() computed into the persistent chrome tree
static void HomePageSyncChrome(HomePageLayout& l) {
    MainWindow* win = l.win;
    HomeChromeCtrl* chrome = EnsureHomeChrome(win);
    VirtRoot* root = win->homeRoot;
    // the chrome positions its children itself, so don't let the root re-layout
    root->bounds = l.rc;
    root->needsLayout = false;
    chrome->SetBounds(l.rc);

    chrome->tip->Sync(l.rcTip, l.rcTipText);

    // file entries: clipped to the thumbnails band, like the static links were
    HomeEntriesCtrl* entries = chrome->entries;
    entries->SetBounds(l.rcThumbsArea);
    int nEntries = len(l.thumbnails);
    entries->SetEntryCount(nEntries);
    bool listView = HomePageIsListView();
    for (int i = 0; i < nEntries; i++) {
        ThumbnailLayout& t = l.thumbnails[i];
        HomeEntryCtrl* e = entries->EntryAt(i);
        e->idx = i;
        Str path = t.fs ? t.fs->filePath : Str{};
        if (!str::Eq(e->filePath, path)) {
            str::ReplaceWithCopy(&e->filePath, path);
        }
        Rect rc = HomeEntryRect(t);
        e->visibility = rc.IsEmpty() ? Visibility::Collapse : Visibility::Visible;
        e->SetBounds(rc);
        if (listView) {
            e->removeBtn->visibility = Visibility::Visible;
            e->removeBtn->SetBounds(t.rcListRemove);
            e->pinBtn->visibility = Visibility::Visible;
            e->pinBtn->SetBounds(t.rcListPin);
        } else {
            e->removeBtn->visibility = Visibility::Collapse;
            e->pinBtn->visibility = Visibility::Collapse;
            // relative to the on-screen part of the entry, so the ✕ stays
            // visible on a thumbnail scrolled half-way out of the band
            e->closeBtn->SetBounds(HomeCloseBtnRectForThumb(win, rc.Intersect(l.rcThumbsArea)));
        }
    }
    entries->UpdateCloseBtnVisibility();

    Size iconSize = l.rcIconThumbnailView.Size();
    chrome->thumbView->pixmap = GetCachedPixmapForSvg(gIconHomeThumbnails, iconSize.dx, iconSize.dy);
    chrome->thumbView->SetBounds(l.rcIconThumbnailView);
    chrome->listView->pixmap = GetCachedPixmapForSvg(gIconHomeList, iconSize.dx, iconSize.dy);
    chrome->listView->SetBounds(l.rcIconListView);

    chrome->hdr->textColor = ThemeWindowTextColor();
    // re-apply: the bounds were set before the parent was positioned
    chrome->hdr->SetBounds(l.freqRead->lastBounds);

    // one click target covering the icon and the link text
    Rect rcOpen = l.rcIconOpen.Union(l.openDoc->lastBounds);
    rcOpen.Inflate(10, 10);
    HomeOpenDocCtrl* od = chrome->openDoc;
    od->pixmap = GetCachedPixmapForSvg(gIconFileOpen, l.rcIconOpen.dx, l.rcIconOpen.dy);
    od->SetBounds(rcOpen);
    od->rcIconLocal = {l.rcIconOpen.x - rcOpen.x, l.rcIconOpen.y - rcOpen.y, l.rcIconOpen.dx, l.rcIconOpen.dy};
    od->text->textColor = ThemeWindowLinkColor();
    // re-apply now that the parent moved: bounds are relative to it
    od->text->SetBounds(l.openDoc->lastBounds);

    // "?" help button in the bottom-right corner, opening the keyboard
    // shortcuts sheet; sits above the tip band when a tip is showing
    {
        int diam = DpiScale(30);
        int margin = DpiScale(16);
        int bottom = l.hasTip ? l.rcTip.y : l.rc.dy;
        Rect btn{l.rc.dx - margin - diam, bottom - margin - diam, diam, diam};
        chrome->helpBtn->SetBounds(btn);
    }
}

static void DrawHomePageLayout(HomePageLayout& l) {
    bool isRtl = IsUIRtl();
    auto* hdc = l.hdc;
    auto* win = l.win;
    auto backgroundColor = ThemeMainWindowBackgroundColor();

    {
        Rect rc = HwndClientRect(win->hwndCanvas);
        auto color = ThemeMainWindowBackgroundColor();
        HdcFillRect(hdc, rc, color);
    }

    // draw search edit border and background on the canvas
    {
        Color bgCol = ThemeControlBackgroundColor();
        const Rect& sb = l.rcSearchBorder;
        RECT rcBorder = {sb.x, sb.y, sb.x + sb.dx, sb.y + sb.dy};
        // fill interior with control background so padding matches the edit
        HBRUSH brBg = CreateSolidBrush(bgCol);
        HdcFillRect(hdc, ToRect(rcBorder), brBg);
        DeleteObject(brBg);
        // draw border frame
        Color borderCol = AccentColor(bgCol, 40);
        HBRUSH brBorder = CreateSolidBrush(borderCol);
        FrameRect(hdc, &rcBorder, brBorder);
        DeleteObject(brBorder);
    }

    auto color = ThemeWindowTextColor();
    HFONT fontText = HdcCreateSimpleFont(hdc, "MS Shell Dlg", 14);

    AutoDeletePen penThumbBorder(CreatePen(PS_SOLID, kThumbsBorderDx, color));
    color = ThemeWindowLinkColor();
    AutoDeletePen penLinkLine(CreatePen(PS_SOLID, 1, color));

    SelectObject(hdc, penThumbBorder);
    SetBkMode(hdc, TRANSPARENT);
    color = ThemeWindowTextColor();
    SetTextColor(hdc, color);

    SelectObject(hdc, GetStockBrush(NULL_BRUSH));

    // clip thumbnails to the middle area
    {
        const Rect& ta = l.rcThumbsArea;
        HRGN thumbsClip = CreateRectRgn(ta.x, ta.y, ta.x + ta.dx, ta.y + ta.dy);
        SelectClipRgn(hdc, thumbsClip);
        DeleteObject(thumbsClip);
    }

    int nThumbs = len(l.thumbnails);
    // keep the keyboard selection inside the (possibly filtered) list
    if (win->homePageSelIdx >= nThumbs) {
        win->homePageSelIdx = nThumbs - 1;
    }
    // no selection rectangle while the search box has focus
    const bool showKeyboardSel = !HomeSearchHasFocus(win);
    // draw selection after restoring the thumbs clip so the outline on the
    // first row isn't clipped at the top edge of rcThumbsArea
    Rect pendingThumbSel;
    bool hasPendingThumbSel = false;
    for (int thumbIdx = 0; thumbIdx < nThumbs; thumbIdx++) {
        ThumbnailLayout& thumb = l.thumbnails[thumbIdx];
        FileState* fs = thumb.fs;
        bool isSelected = showKeyboardSel && (thumbIdx == win->homePageSelIdx);
        if (HomePageIsListView()) {
            DrawHomeListRow(l, thumb, fontText, backgroundColor, isRtl, isSelected);
            continue;
        }
        const Rect& page = thumb.rcPage;
        // skip off-screen thumbs (scroll was redoing Blit+text for every history
        // entry and dominated CPU in DrawHomePageLayout)
        if (!IsHomeThumbOnScreen(page.Union(thumb.rcText), l.rcThumbsArea)) {
            continue;
        }

        // disk load only first time; stays on fs->thumbnail afterwards
        Pixmap* thumbImg = LoadThumbnail(fs);
        if (thumbImg) {
            thumb.szThumb = Size(thumbImg->width, thumbImg->height);
            int savedDC = SaveDC(hdc);
            HRGN clip = CreateRoundRectRgn(page.x, page.y, page.x + page.dx, page.y + page.dy, 10, 10);
            ExtSelectClipRgn(hdc, clip, RGN_AND);
            // note: we used to invert bitmaps in dark theme but that doesn't
            // make sense for thumbnails
            BlitPixmap(thumbImg, hdc, page);
            RestoreDC(hdc, savedDC);
            DeleteObject(clip);
        }
        RoundRect(hdc, page.x, page.y, page.x + page.dx, page.y + page.dy, 10, 10);

        const Rect& rect = thumb.rcText;
        Str path = fs->filePath;
        TempStr fileName = path::GetBaseNameTemp(path);
        u32 fmt = gfxTextEllipsis | (isRtl ? gfxTextRight : gfxTextLeft);

        SelectObject(hdc, fontText);
        {
            GfxHdc gfx(hdc);
            DrawMaybeHighlightedText(&gfx, rect, fileName, l.filterWords, l.highlighted, backgroundColor, isRtl, false,
                                     fmt, GetPlatformFont(fontText));
        }

        GetFileStateIcon(fs);
        int x = isRtl ? page.x + page.dx - DpiScale(16) : page.x;
        ImageList_Draw(fs->himl, fs->iconIdx, hdc, x, rect.y, ILD_TRANSPARENT);

        if (isSelected) {
            Rect sel = page.Union(rect);
            sel.Inflate(DpiScale(4), DpiScale(3));
            pendingThumbSel = sel;
            hasPendingThumbSel = true;
        }
    }

    // restore full clip region
    SelectClipRgn(hdc, nullptr);

    if (hasPendingThumbSel) {
        DrawHomeSelectionOutline(hdc, pendingThumbSel, 10);
    }

    color = ThemeWindowLinkColor();
    SetTextColor(hdc, color);
    SelectObject(hdc, penLinkLine);

    // the tip band's background; the markup itself is part of the chrome tree
    if (l.hasTip) {
        HdcFillRect(hdc, l.rcTip, ThemeControlBackgroundColor());
    }

    // the chrome (header, view buttons, "Open a document...", help button)
    // paints last: none of it overlaps the thumbnails, and the help button has
    // to land on top of the tip band
    GfxHdc gfx(hdc);
    win->homeRoot->Paint(&gfx, l.rc);
}

void DrawHomePage(MainWindow* win, HDC hdc) {
    HWND hwnd = win->hwndFrame;

    HomePageLayout l;
    l.rc = HwndClientRect(win->hwndCanvas);
    l.hdc = hdc;
    l.hwnd = hwnd;
    l.win = win;

    TempStr filterText = HomeSearchQueryTemp(win);
    int scrollY = win->homePageScrollY;

    // Prefer the scroll-friendly path: when only scrollY changed, offset cached
    // thumb rects instead of re-running full LayoutHomePage (was ~30% of scroll CPU).
    bool usedCache = false;
    if (HomeLayoutCacheMatches(l.rc, filterText)) {
        Vec<FileState*> files;
        StrVec filterWords;
        CollectHomePageFiles(win, files, filterWords);
        if (HomeLayoutCacheFilesMatch(files)) {
            ApplyHomeLayoutCache(l, scrollY);
            usedCache = true;
        }
    }
    if (!usedCache) {
        LayoutHomePage(l);
        SaveHomeLayoutCache(l, filterText, win->homePageScrollY);
    }
    // Keep cue "Search N files …" in sync when history changes without recreating the edit.
    UpdateHomeSearchCueBanner(win);

    HomePageSyncChrome(l);
    DrawHomePageLayout(l);
    SyncHomeLayoutCacheFileSizes(l);

    // update overlay scrollbar for home page if thumbnails overflow visible area
    bool showScrollbarV = ScrollbarsUseOverlay() && l.totalContentDy > l.thumbsVisibleDy;
    if (showScrollbarV) {
        if (!win->overlayScrollV) {
            win->overlayScrollV =
                OverlayScrollbarCreate(win->hwndCanvas, OverlayScrollbar::Type::Vert, ScrollbarsOverlayMode());
        }
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        si.nMin = 0;
        si.nMax = l.totalContentDy - 1;
        si.nPage = l.thumbsVisibleDy;
        si.nPos = win->homePageScrollY;
        OverlayScrollbarShow(win->overlayScrollV, true);
        OverlayScrollbarSetInfo(win->overlayScrollV, &si, TRUE);
    }
    // show thin scrollbar briefly to indicate content is scrollable
    OverlayScrollbarShow(win->overlayScrollV, showScrollbarV);
}

// --- keyboard navigation of the file list (issue #1136) ---

// Selection works off the layout cache, which is filled by the last paint, so
// the entries are exactly what's on screen (same order, same filtering).
static int HomeSelectableCount() {
    return gHomeLayoutCache.valid ? len(gHomeLayoutCache.thumbs) : 0;
}

// bounding box of an entry, in window coordinates for the current scroll
static Rect HomeEntryRect(const ThumbnailLayout& t) {
    if (HomePageIsListView()) {
        return t.rcListRow;
    }
    return t.rcPage.Union(t.rcText);
}

// how many thumbnails fit in a grid row: the run of entries sharing the y of
// the first one
static int HomeGridColumnCount() {
    auto& c = gHomeLayoutCache;
    int n = len(c.thumbs);
    if (n == 0) {
        return 1;
    }
    int y0 = c.thumbs[0].rcPage.y;
    int nCols = 0;
    while (nCols < n && c.thumbs[nCols].rcPage.y == y0) {
        nCols++;
    }
    return nCols > 0 ? nCols : 1;
}

// Select the first-row entry at the column remembered when leaving for search.
static void HomeSelectFromSearchReturnCol(MainWindow* win) {
    int n = HomeSelectableCount();
    if (n <= 0) {
        win->homePageSelIdx = 0;
        return;
    }
    if (HomePageIsListView()) {
        win->homePageSelIdx = 0;
        return;
    }
    int nCols = HomeGridColumnCount();
    nCols = std::max(nCols, 1);
    int col = win->homePageSearchReturnCol;
    col = std::max(col, 0);
    if (col >= nCols) {
        col = nCols - 1;
    }
    // first row only has min(nCols, n) entries
    int firstRowN = n < nCols ? n : nCols;
    if (col >= firstRowN) {
        col = firstRowN - 1;
    }
    win->homePageSelIdx = col;
}

// Keep layout-cache thumb rects in sync with homePageScrollY (without a full
// paint) so keyboard tooltips can use up-to-date geometry after scroll.
static void HomeSyncLayoutCacheScroll(MainWindow* win) {
    auto& c = gHomeLayoutCache;
    if (!c.valid || !win) {
        return;
    }
    int scrollY = win->homePageScrollY;
    int maxScrollY = std::max(0, c.totalContentDy - c.thumbsVisibleDy);
    if (scrollY > maxScrollY) {
        scrollY = maxScrollY;
        win->homePageScrollY = scrollY;
    }
    if (scrollY < 0) {
        scrollY = 0;
        win->homePageScrollY = 0;
    }
    int dy = c.scrollY - scrollY;
    OffsetThumbnailLayouts(c.thumbs, dy);
    c.scrollY = scrollY;
}

// scroll so the selected entry is fully visible
static void HomeScrollSelectionIntoView(MainWindow* win) {
    auto& c = gHomeLayoutCache;
    int idx = win->homePageSelIdx;
    if (!c.valid || idx < 0 || idx >= len(c.thumbs)) {
        return;
    }
    // rects must match current scroll before we measure visibility
    HomeSyncLayoutCacheScroll(win);
    Rect r = HomeEntryRect(c.thumbs[idx]);
    const Rect& area = c.rcThumbsArea;
    if (r.IsEmpty() || area.IsEmpty()) {
        return;
    }
    // scrollY grows as the content moves up
    int dy = 0;
    if (r.y < area.y) {
        dy = r.y - area.y;
    } else if (r.y + r.dy > area.y + area.dy) {
        dy = (r.y + r.dy) - (area.y + area.dy);
    }
    if (dy == 0) {
        return;
    }
    int newScrollY = win->homePageScrollY + dy;
    newScrollY = std::max(newScrollY, 0);
    win->homePageScrollY = newScrollY; // layout clamps against content height
    HomeSyncLayoutCacheScroll(win);
}

// Selection outline rect — must match DrawHomePageLayout / DrawHomeListRow.
static Rect HomeSelectionOutlineRect(const ThumbnailLayout& t, HWND hwnd) {
    if (HomePageIsListView()) {
        // list: outline is the row, 1px shorter (separator line)
        return {t.rcListRow.x, t.rcListRow.y, t.rcListRow.dx, t.rcListRow.dy - 1};
    }
    // thumbnails: page ∪ name, inflated by the same amounts as paint
    Rect sel = t.rcPage.Union(t.rcText);
    sel.Inflate(DpiScale(4), DpiScale(3));
    return sel;
}

// Show/update the infotip for the keyboard-selected home entry. Placed just
// below the blue selection outline, left-aligned with the outline's left edge;
// shifted left if it would extend past the right edge of the last outline in
// that row.
static void HomePageShowSelectionTooltip(MainWindow* win) {
    if (!win || HomeSearchHasFocus(win)) {
        if (win) {
            win->DeleteToolTip();
        }
        return;
    }
    // never put a tip up from a window that isn't in front: track-mode tips are
    // topmost popups and showing one steals activation, which pulls the frame
    // over the command palette / Change Theme window
    if (GetForegroundWindow() != win->hwndFrame) {
        return;
    }
    auto& c = gHomeLayoutCache;
    int idx = win->homePageSelIdx;
    if (!c.valid || idx < 0 || idx >= len(c.thumbs)) {
        win->DeleteToolTip();
        return;
    }
    HomeSyncLayoutCacheScroll(win);
    ThumbnailLayout& t = c.thumbs[idx];
    FileState* fs = t.fs;
    if (!fs || !fs->filePath) {
        win->DeleteToolTip();
        return;
    }

    // Same text as hover: path + size (size looked up only when shown)
    TempStr tip = HomeThumbTooltipTemp(fs->filePath);
    i64 size = file::GetSize(fs->filePath);
    if (size >= 0) {
        tip = fmt("%s  %s", tip, str::FormatSizeShortTemp(size, nullptr));
    }

    HWND hwnd = win->hwndCanvas;
    Rect outline = HomeSelectionOutlineRect(t, hwnd);
    // a little below the outline so the tip clears the blue border
    int tipClientX = outline.x;
    int tipClientY = outline.y + outline.dy + DpiScale(4);

    int rightEdgeClient = outline.x + outline.dx;
    if (!HomePageIsListView()) {
        int n = len(c.thumbs);
        int nCols = HomeGridColumnCount();
        nCols = std::max(nCols, 1);
        int col = idx % nCols;
        int rowStart = idx - col;
        int lastInRow = rowStart + nCols - 1;
        if (lastInRow >= n) {
            lastInRow = n - 1;
        }
        Rect lastOutline = HomeSelectionOutlineRect(c.thumbs[lastInRow], hwnd);
        rightEdgeClient = lastOutline.x + lastOutline.dx;
    }

    POINT tl{tipClientX, tipClientY};
    POINT tr{rightEdgeClient, tipClientY};
    ClientToScreen(hwnd, &tl);
    ClientToScreen(hwnd, &tr);
    win->ShowToolTipAt(tip, outline, Point(tl.x, tl.y), false, tr.x);
}

// select the first entry, e.g. after the filter changed the list
void HomePageSelectFirst(MainWindow* win) {
    win->homePageSelIdx = 0;
    win->homePageSearchReturnCol = 0;
}

// hide keyboard-selection tip on deactivate; restore it when the frame is active
void HomePageOnWindowActivate(MainWindow* win, bool active) {
    if (!win) {
        return;
    }
    if (!active || IsIconic(win->hwndFrame)) {
        // Also when the frame is iconic: activate can fire while minimized and
        // ClientToScreen then pins the tip at the top-left of the desktop (#5928).
        win->DeleteToolTip();
        return;
    }
    // only restore the selection tip (positioned at the active entry, not cursor)
    if (win->IsCurrentTabAbout()) {
        HomePageShowSelectionTooltip(win);
    }
}

// the entries wnd of the chrome tree, if the home page is showing
static HomeEntriesCtrl* HomeEntries(MainWindow* win) {
    if (!win || !win->homeRoot) {
        return nullptr;
    }
    if (!IsVirtCtrlOfKind(win->homeRoot->owned, kindHomeChromeCtrl)) {
        return nullptr; // the About page is showing, not the home page
    }
    return ((HomeChromeCtrl*)win->homeRoot->owned)->entries;
}

// mouse left the canvas (or the page scrolled): drop the active entry so the
// close button goes away
void HomePageClearActiveEntry(MainWindow* win) {
    HomeEntriesCtrl* entries = HomeEntries(win);
    if (!entries) {
        return;
    }
    if (win->homeRoot) {
        win->homeRoot->ClearHover();
    }
    entries->SetActiveEntry(-1);
}

// mouse over a file entry: update homePageSelIdx and show the tip at that entry
// (not at the cursor). Returns true if (x,y) is over a file thumbnail/list row
// file of the entry at (x,y), empty if there is no entry there. Replaces the
// old "look the click up in win->staticLinks" - entries are VirtCtrls now
Str HomePageFilePathAtTemp(MainWindow* win, int x, int y) {
    HomeEntriesCtrl* entries = HomeEntries(win);
    if (!entries) {
        return {};
    }
    Point ptLocal{0, 0};
    VirtCtrl* w = CtrlFromPoint(win->homeRoot, {x, y}, &ptLocal);
    HomeEntryCtrl* e = entries->EntryForCtrl(w);
    if (!e) {
        return {};
    }
    return str::DupTemp(e->filePath);
}

bool HomePageOnHover(MainWindow* win, int x, int y) {
    HomeEntriesCtrl* entries = HomeEntries(win);
    if (!entries) {
        return false;
    }
    Point ptLocal{0, 0};
    VirtCtrl* w = CtrlFromPoint(win->homeRoot, {x, y}, &ptLocal);
    HomeEntryCtrl* e = entries->EntryForCtrl(w);
    if (!e) {
        return false;
    }
    entries->SetActiveEntry(e->idx);
    return true;
}

// file of the keyboard-selected entry, empty if there's no selection
Str HomePageSelectedFilePathTemp(MainWindow* win) {
    auto& c = gHomeLayoutCache;
    int idx = win->homePageSelIdx;
    if (!c.valid || idx < 0 || idx >= len(c.thumbs)) {
        return {};
    }
    FileState* fs = c.thumbs[idx].fs;
    if (!fs) {
        return {};
    }
    return str::DupTemp(fs->filePath);
}

// keyboard navigation of the file list (issue #1136). dCol/dRow are in grid
// steps; in list view only dRow matters. Moving up past the first row puts
// focus in the search box
void HomePageMoveSelection(MainWindow* win, int dCol, int dRow) {
    int n = HomeSelectableCount();
    if (n == 0) {
        win->DeleteToolTip();
        return;
    }
    int idx = win->homePageSelIdx;
    if (idx < 0 || idx >= n) {
        // nothing selected yet: any arrow key selects the first entry
        win->homePageSelIdx = 0;
        HomeScrollSelectionIntoView(win);
        HwndInvalidate(win->hwndCanvas);
        HomePageShowSelectionTooltip(win);
        return;
    }

    int nCols = HomePageIsListView() ? 1 : HomeGridColumnCount();
    int delta;
    if (HomePageIsListView()) {
        // one entry per row; left/right have nothing to move along
        delta = dRow;
    } else {
        delta = dCol + (dRow * nCols);
    }
    if (delta == 0) {
        return;
    }
    int newIdx = idx + delta;
    if (newIdx < 0) {
        // above the first row: hand focus to the search box, remember column
        if (dRow < 0 && win->homeSearch) {
            win->homePageSearchReturnCol = HomePageIsListView() ? 0 : (idx % nCols);
            win->DeleteToolTip();
            HwndSetFocus(win->homeSearch->hwnd);
            HwndInvalidate(win->hwndCanvas); // drop selection outline while typing
            return;
        }
        newIdx = 0;
    }
    if (newIdx >= n) {
        newIdx = n - 1;
    }
    if (newIdx == idx) {
        return;
    }
    win->homePageSelIdx = newIdx;
    HomeScrollSelectionIntoView(win);
    HwndInvalidate(win->hwndCanvas);
    HomePageShowSelectionTooltip(win);
}

void HomePageOnVScroll(MainWindow* win, WPARAM wp) {
    USHORT msg = LOWORD(wp);
    HDC hdc = GetDC(win->hwndCanvas);
    int lineDy = HomePageIsListView() ? kHomeListRowDy : kThumbnailDy + kThumbsSpaceBetweenY;
    int pageDy = lineDy * 3;
    ReleaseDC(win->hwndCanvas, hdc);

    int newScrollY = win->homePageScrollY;
    switch (msg) {
        case SB_LINEUP:
            newScrollY -= lineDy;
            break;
        case SB_LINEDOWN:
            newScrollY += lineDy;
            break;
        case SB_PAGEUP:
            newScrollY -= pageDy;
            break;
        case SB_PAGEDOWN:
            newScrollY += pageDy;
            break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: {
            int pos = (int)(short)HIWORD(wp);
            // overlay scrollbar sends full position in HIWORD for THUMBTRACK
            if (win->overlayScrollV) {
                pos = win->overlayScrollV->nTrackPos;
            }
            newScrollY = pos;
            break;
        }
        case SB_TOP:
            newScrollY = 0;
            break;
        case SB_BOTTOM:
            newScrollY = INT_MAX; // will be clamped by layout
            break;
    }
    newScrollY = std::max(newScrollY, 0);
    if (newScrollY != win->homePageScrollY) {
        win->homePageScrollY = newScrollY;
        HwndInvalidate(win->hwndCanvas);
    }
}

void HomePageOnMouseWheel(MainWindow* win, int delta) {
    HDC hdc = GetDC(win->hwndCanvas);
    int thumbsRowDy = HomePageIsListView() ? kHomeListRowDy : kThumbnailDy + kThumbsSpaceBetweenY;
    ReleaseDC(win->hwndCanvas, hdc);

    int scrollBy = thumbsRowDy / 3;
    if (delta > 0) {
        scrollBy = -scrollBy;
    }
    int newScrollY = win->homePageScrollY + scrollBy;
    newScrollY = std::max(newScrollY, 0);
    if (newScrollY != win->homePageScrollY) {
        win->homePageScrollY = newScrollY;
        HwndInvalidate(win->hwndCanvas);
    }
}
