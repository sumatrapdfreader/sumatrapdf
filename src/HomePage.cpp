/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/GdiPlusUtil.h"
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
#include "gui/GuiColors.h"
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
#include "DarkMode_win.h"
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

static void FreeHomeFileIcons();

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
    FreeHomeFileIcons();
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

// The About screen's two text columns: a Table (ILayout) whose left column
// is right-aligned and right column left-aligned. Rows with a url become
// VirtLink (owning the hit-testing, the hand cursor and the tooltip), the rest
// plain VirtText. Table is an ILayout child so ElementFromPoint walks the cells.
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
    void Sync();
    void UpdateLayout(Rect clientRc);
    VirtText* LeftAt(int i);
    VirtText* RightAt(int i);
    void PaintChildren(VirtPaintCtx&) override;
    int LayoutChildCount() override;
    ILayout* LayoutChildAt(int) override;
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

// table is not a VirtCtrl child; expose it so ElementFromPoint walks the cells
int AboutCtrl::LayoutChildCount() {
    return VirtCtrl::LayoutChildCount() + (table ? 1 : 0);
}

ILayout* AboutCtrl::LayoutChildAt(int i) {
    int n = VirtCtrl::LayoutChildCount();
    if (i < n) {
        return VirtCtrl::LayoutChildAt(i);
    }
    return table;
}

// build the table once, then keep text, fonts and colors in step with the theme
// and the DPI. Sizing happens in UpdateLayout(), which measures what we set here
void AboutCtrl::Sync() {
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

    logo->font = GetUserGuiFont(kSumatraTxtFont, DpiScale(kSumatraTxtFontSize));

    PlatformFont* fontLeftTxt = GetUserGuiFont(kLeftTextFont, DpiScale(kLeftTextFontSize));
    PlatformFont* fontRightTxt = GetUserGuiFont(kRightTextFont, DpiScale(kRightTextFontSize));
    Color colText = ThemeWindowTextColor();
    Color colLink = ThemeWindowLinkColor();

    for (int i = 0; i < n; i++) {
        AboutRow* el = &gAboutRows[i];
        VirtText* left = LeftAt(i);
        left->font = fontLeftTxt;

        VirtText* right = RightAt(i);
        right->font = fontRightTxt;
        bool isLink = canAccessDisk && el->url;
        // the right column is a link when the row has a url we can open
        right->SetColor(kColText, isLink ? colLink : colText);
        // without disk access the url can't be opened, so it isn't a link
        right->withUnderline = isLink;
        right->SetFlag(vwfNoHitTest, !isLink);
        right->SetText(el->rightTxt ? TrimGitTemp(el->rightTxt) : Str(GetAppVersionTemp()));
    }
}

// the About box is the title band above the two-column table. This sizes it from
// the table, centers it in clientRc and positions the table inside it
void AboutCtrl::UpdateLayout(Rect clientRc) {
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
static AboutCtrl* UpdateAboutLayout(VirtRoot** rootPtr, HWND hwnd, Rect clientRc) {
    AboutCtrl* about = EnsureAboutCtrl(rootPtr, hwnd, clientRc);
    about->Sync();
    about->UpdateLayout(clientRc);
    return about;
}

/* Draws the about screen. The text columns are painted by the AboutCtrl tree;
   this draws the frame around them. It transcribes the design I did in graphics
   software - hopeless to understand without seeing the design. */
static void DrawAbout(Gfx* gfx, VirtRoot* root, Rect clientRc) {
    auto* about = (AboutCtrl*)root->owned;
    Rect rect = about->aboutRect;
    Color lineCol = ThemeWindowTextColor();
    Color bgCol = ThemeMainWindowBackgroundColor();
    gfx->FillRect(clientRc, bgCol);

    /* render title */
    Rect titleRect(rect.TL(), about->headerSize);

#ifndef ABOUT_USE_LESS_COLORS
    gfx->DrawRect({rect.x, rect.y + ABOUT_LINE_OUTER_SIZE, rect.dx, titleRect.dy}, lineCol, ABOUT_LINE_OUTER_SIZE);
#else
    Rect titleBgBand(0, rect.y, clientRc.dx, titleRect.dy);
    gfx->FillRect(titleBgBand, bgCol);
    gfx->DrawLine(Rect(0, rect.y, clientRc.dx, 0), lineCol);
    gfx->DrawLine(Rect(0, rect.y + titleRect.dy, clientRc.dx, 0), lineCol);
#endif

    /* render attribution box */
#ifndef ABOUT_USE_LESS_COLORS
    gfx->DrawRect({rect.x, rect.y + titleRect.dy, rect.dx, rect.dy - titleRect.dy}, lineCol, ABOUT_LINE_OUTER_SIZE);
#endif

    /* render both text columns */
    root->Paint(gfx, clientRc);

    Rect divideLine(about->dividerX, rect.y + titleRect.dy + 4, 0, rect.dy - titleRect.dy - 8);
    gfx->DrawLine(divideLine, lineCol, ABOUT_LINE_SEP_SIZE);
}

static void OnPaintAbout(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    SetLayout(hdc, LAYOUT_LTR);
    Rect clientRc = HwndClientRect(hwnd);
    UpdateAboutLayout(&gAboutRoot, hwnd, clientRc);
    Gfx* gfx = GfxCreate(hdc);
    DrawAbout(gfx, gAboutRoot, clientRc);
    delete gfx;
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
                ILayout* el = ElementFromPoint(gAboutRoot, pt, &ptLocal);
                VirtCtrl* w = el ? el->AsVirtCtrl() : nullptr;
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
            DarkModeApplyToTitleBar(hwnd);
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
    AboutCtrl* about = UpdateAboutLayout(&gAboutRoot, gHwndAbout, HwndClientRect(gHwndAbout));
    Rect rc = about->aboutRect;
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

void DrawAboutPage(MainWindow* win, Gfx* gfx) {
    HWND hwnd = win->hwndCanvas;
    Rect clientRc = HwndClientRect(hwnd);
    AboutCtrl* about = UpdateAboutLayout(&win->homeRoot, hwnd, clientRc);

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
        link->font = GetUserGuiFont("MS Shell Dlg", DpiScale(16));
        link->sz = {0, 0}; // re-measure: the font may have changed with the DPI
        Size txtSize = link->GetIdealSize(true);
        Rect r = {0, 0, txtSize.dx, txtSize.dy};
        PositionRB(clientRc, r);
        MoveXY(r, -DpiScale(kInnerPadding), -DpiScale(kInnerPadding));
        link->SetBounds(r);
    }
    DrawAbout(gfx, win->homeRoot, clientRc);
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
    Gfx* gfx = nullptr;
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

// one file entry (a thumbnail or a list row): hit-testing, hover, clicks and
// painting of the row / thumbnail content
struct HomeEntryCtrl : VirtCtrl {
    Str filePath; // owned
    int idx = 0;
    VirtCloseButton* closeBtn = nullptr;
    VirtCloseButton* removeBtn = nullptr;
    HomeListIconCtrl* pinBtn = nullptr;
    // per-paint: points into the HomePageLayout being painted. Set by
    // HomePageSyncChrome right before every paint; only valid during the
    // homeRoot->Paint() that follows
    ThumbnailLayout* layout = nullptr;

    HomeEntryCtrl();
    ~HomeEntryCtrl() override;
    void Paint(VirtPaintCtx&) override;
};

// page-level list: still knows the MainWindow so it can wire entry actions and
// keep keyboard selection in sync
struct HomeEntriesCtrl : VirtCtrl {
    MainWindow* win = nullptr;
    // entry the mouse is on, -1 for none. Drives the ✕ button and the keyboard
    // selection, which follows the mouse
    int activeIdx = -1;
    Point lastHoverPt{-1, -1};
    // per-paint search-filter state, owned by the HomePageLayout being painted
    // (same lifetime rules as HomeEntryCtrl::layout)
    const StrVec* filterWords = nullptr;
    Vec<u8>* highlighted = nullptr;

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
    void Paint(VirtPaintCtx&) override;
};

// paints the border and background around the home search edit (the edit
// itself is a real HWND on top). Decoration only, never a click target
struct HomeSearchBorderCtrl : VirtCtrl {
    HomeSearchBorderCtrl();
    void Paint(VirtPaintCtx&) override;
};

static Kind kindHomeChromeCtrl = "homeChromeCtrl";

struct HomeChromeCtrl : VirtCtrl {
    HomeTipCtrl* tip = nullptr;
    HomeSearchBorderCtrl* searchBorder = nullptr;
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
static Rect HomeSelectionOutlineRect(const ThumbnailLayout& t);

static int HomePageIconSize() {
    int sz = DpiScale(gGlobalPrefs->toolbarSize);
    if (sz < 1) {
        sz = DpiScale(16);
    }
    return RoundUp(sz, 4);
}

constexpr int kThumbsMiddleMargin = 32;
// draw a gray separator line between list-view rows
static bool gShowListSeparatorLine = false;
constexpr int kSearchEditDy = 28;
constexpr int kHeaderSearchGapY = 12;
constexpr int kSearchThumbnailsGapY = 12;

static PlatformFont* HomePageFont(int size) {
    return GetUserGuiFont("MS Shell Dlg", DpiScale(size));
}

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
        FileHistoryGetFrequencyOrder(all);
    } else {
        FileHistoryGetRecentlyOpenedOrder(all);
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
    PlatformFont* font = HomePageFont(14);

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
        FileHistoryGetFrequencyOrder(allFileStates);
    } else {
        FileHistoryGetRecentlyOpenedOrder(allFileStates);
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

    PlatformFont* hdrFont = HomePageFont(24);
    PlatformFont* fontText = HomePageFont(14);

    Str txt = _TRA("Recently Opened");
    if (gGlobalPrefs->homePageSortByFrequentlyRead) {
        txt = _TRA("Frequently Read");
    }
    HomeChromeCtrl* chrome = EnsureHomeChrome(win);
    VirtText* hdr = chrome->hdr;
    hdr->SetText(txt);
    hdr->font = hdrFont;
    hdr->isRtl = isRtl;
    hdr->SetBounds(c.rcFreqRead);
    l.freqRead = hdr;

    VirtText* openDoc = chrome->openDoc->text;
    openDoc->SetText(_TRA("Open a document..."));
    openDoc->font = fontText;
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
        FileHistoryGetFrequencyOrder(allFileStates);
    } else {
        FileHistoryGetRecentlyOpenedOrder(allFileStates);
    }
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
    PlatformFont* fontText = HomePageFont(14);
    PlatformFont* hdrFont = HomePageFont(24);

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
    hdr->font = hdrFont;
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
    openDoc->font = fontText;
    openDoc->isRtl = isRtl;
    openDoc->withUnderline = true;
    txtSize = openDoc->GetIdealSize(true);

    int openDocSpacing = DpiScale(16);
    rcIconOpen.x = rcHdr.x + rcHdr.dx + openDocSpacing;
    // every header item (view icons, title, folder icon, link) is centered on
    // the header's vertical centerline
    rcIconOpen.y = rcHdr.y + ((rcHdr.dy - rcIconOpen.dy) / 2);
    if (isRtl) {
        rcIconOpen.x = rcHdr.x - openDocSpacing - rcIconOpen.dx;
    }
    l.rcIconOpen = rcIconOpen;

    Rect rcOpenDoc(rcIconOpen.x + rcIconOpen.dx + 3, rcHdr.y + ((rcHdr.dy - txtSize.dy) / 2), txtSize.dx, txtSize.dy);
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
    PlatformFont* fontTip = HomePageFont(16);
    HomeTipCtrl* tipCtrl = EnsureHomeChrome(l.win)->tip;
    tipCtrl->SetTipLine(SelectedTipLine(), fontTip);
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

struct HomeFileIcon {
    HomeFileIcon* next = nullptr;
    HIMAGELIST imageList = nullptr;
    int iconIdx = -1;
    Pixmap* pixmap = nullptr;
};

static HomeFileIcon* gFileIcons = nullptr;

static void FreeHomeFileIcons() {
    for (HomeFileIcon* icon = gFileIcons; icon; icon = icon->next) {
        FreePixmap(icon->pixmap);
    }
    ListDelete(gFileIcons);
    gFileIcons = nullptr;
}

// Shell image lists are Windows drawing objects. Convert each distinct icon to
// a Pixmap once so home-page painting stays entirely within Gfx.
static Pixmap* GetFileStateIconPixmap(FileState* fs) {
    GetFileStateIcon(fs);
    if (!fs->himl || fs->iconIdx < 0) {
        return nullptr;
    }
    for (HomeFileIcon* icon = gFileIcons; icon; icon = icon->next) {
        if (icon->imageList == fs->himl && icon->iconIdx == fs->iconIdx) {
            return icon->pixmap;
        }
    }

    HICON hicon = ImageList_GetIcon(fs->himl, fs->iconIdx, ILD_TRANSPARENT);
    Pixmap* pixmap = nullptr;
    if (hicon) {
        {
            Gdiplus::Bitmap bmp(hicon);
            pixmap = PixmapFromGdiplus(&bmp);
        }
        DestroyIcon(hicon);
    }
    auto* icon = new HomeFileIcon();
    icon->imageList = fs->himl;
    icon->iconIdx = fs->iconIdx;
    icon->pixmap = pixmap;
    ListInsertFront(&gFileIcons, icon);
    return pixmap;
}

// --- Close (✕) button for Frequently Read thumbnails (issue #283, #5745) ---
//
// Drawn onto the home-page canvas (over the top-right corner of the thumbnail
// under the mouse) rather than as a separate top-level window. The separate
// window could be left behind, drawing stray crosses over a document (#5745).
// Styled like the tab close button (gray X on a white circle; red circle +
// white X on hover). It is a HomeCloseBtnCtrl in the chrome tree, shown on the
// entry the mouse is on.

static void DrawHomeViewButton(Gfx* gfx, Pixmap* icon, Rect r, bool selected, bool hovered) {
    if (selected || hovered) {
        Color bg = selected ? ThemeControlBackgroundColor() : ThemeMainWindowBackgroundColor();
        if (hovered) {
            bg = AccentColor(bg, 20);
        }
        gfx->FillRect(r, bg);
        if (selected) {
            gfx->DrawRect(r, AccentColor(bg, 40));
        }
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

static void DrawHomeRoundedOutline(Gfx* gfx, const Rect& r, int radius, Color color, int thickness) {
    Rect line = r;
    for (int i = 0; i < thickness && !line.IsEmpty(); i++) {
        gfx->FillRoundedRect(line, std::max(radius - (2 * i), 1), kColorTransparent, color);
        line.Inflate(-1, -1);
    }
}

static void DrawHomeSelectionOutline(Gfx* gfx, const Rect& r, int radius) {
    int penDx = DpiScale(2);
    DrawHomeRoundedOutline(gfx, r, radius, kHomeSelectionColor, penDx);
}

// Give the file name the width it needs and put the directory path in what's
// left, right-aligned (mirrored for RTL). Done on first paint of a row, not
// during layout: measuring every history entry made layout (and so scrolling)
// slow, and measuring only the rows visible at layout time meant rows scrolled
// into view later never got a directory.
static void MeasureHomeListRowText(Gfx* gfx, ThumbnailLayout& thumb, PlatformFont* font, bool isRtl) {
    if (thumb.listTextMeasured) {
        return;
    }
    thumb.listTextMeasured = true;

    Rect rcFileName = thumb.rcListFileName;
    TempStr fileName = path::GetBaseNameTemp(thumb.fs->filePath);
    int nameDx = gfx->MeasureText(fileName, font).dx + DpiScale(4);
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

static void DrawHomeListRow(Gfx* gfx, ThumbnailLayout& thumb, const StrVec& filterWords, Vec<u8>& highlighted,
                            PlatformFont* fontText, Color backgroundColor, bool isRtl, bool isSelected) {
    FileState* fs = thumb.fs;
    Rect row = thumb.rcListRow;
    MeasureHomeListRowText(gfx, thumb, fontText, isRtl);
    if (isSelected) {
        DrawHomeSelectionOutline(gfx, HomeSelectionOutlineRect(thumb), 4);
    }

    if (gShowListSeparatorLine) {
        Color lineCol = AccentColor(ThemeMainWindowBackgroundColor(), 30);
        gfx->DrawLine(Rect(row.x, row.y + row.dy - 1, row.dx, 0), lineCol);
    }

    // LoadThumbnail only hits disk the first time; result stays on fs->thumbnail
    Pixmap* thumbImg = LoadThumbnail(fs);
    Rect thumbBox = thumb.rcListThumb;
    if (thumbImg) {
        Size szThumb(thumbImg->width, thumbImg->height);
        Rect thumbDst = FitRectInRect(szThumb, thumbBox);
        gfx->DrawPixmap(thumbImg, thumbDst);
        thumb.szThumb = szThumb;
    }
    Str path = fs->filePath;
    TempStr fileName = path::GetBaseNameTemp(path);
    u32 nameFmt = gfxTextEllipsis | gfxTextVCenter | (isRtl ? gfxTextRight : gfxTextLeft);
    DrawMaybeHighlightedText(gfx, thumb.rcListFileName, fileName, filterWords, highlighted, backgroundColor, isRtl,
                             false, nameFmt, fontText, ThemeWindowTextColor());

    // directory path, right-aligned and muted, in the space the file name doesn't need.
    // Must use DrawTextW: dirPath is UTF-8; DrawTextA treated it as the system ANSI
    // code page and mangled non-ASCII path characters (#5824).
    if (!thumb.rcListPath.IsEmpty()) {
        TempStr dirPath = path::GetDirTemp(path);
        u32 pathFmt = gfxTextVCenter | gfxTextPathEllipsis | (isRtl ? gfxTextLeft : gfxTextRight);
        Rect pathRect = thumb.rcListPath;
        gfx->DrawText(dirPath, pathRect, pathFmt, fontText, ThemeWindowTextDisabledColor());
    }

    // file::GetSize once per row, then cache on ThumbnailLayout (scroll reuses
    // it). kSizeNotFetched = not tried; kSizeFetchFail = GetSize failed; >= 0
    // is a real size (including empty files).
    if (thumb.fileSize == kSizeNotFetched) {
        i64 sz = file::GetSize(path);
        thumb.fileSize = (sz < 0) ? kSizeFetchFail : sz;
    }
    TempStr fileSize = FileSizeForHomeListTemp(thumb.fileSize);
    u32 sizeFmt = gfxTextVCenter | gfxTextEllipsis | (isRtl ? gfxTextLeft : gfxTextRight);
    Rect sizeRect = thumb.rcListSize;
    gfx->DrawText(fileSize, sizeRect, sizeFmt, fontText, ThemeWindowTextColor());

    if (fs->isPinned) {
        gfx->FillRect(thumb.rcListPin, ThemeControlBackgroundColor());
    }
    {
        int pinDx = thumb.rcListPin.dx > 0 ? thumb.rcListPin.dx : DpiScale(16);
        int pinDy = thumb.rcListPin.dy > 0 ? thumb.rcListPin.dy : pinDx;
        Pixmap* pin = GetCachedPixmapForSvg(gIconPin, pinDx, pinDy);
        if (pin) {
            gfx->DrawPixmap(pin, thumb.rcListPin);
        }
    }
}

// one thumbnail: the page image with a rounded outline, the caption (with
// search-filter highlights) and the file-type icon
static void DrawHomeThumbnail(Gfx* gfx, ThumbnailLayout& thumb, const StrVec& filterWords, Vec<u8>& highlighted,
                              PlatformFont* fontText, Color backgroundColor, bool isRtl) {
    FileState* fs = thumb.fs;
    const Rect& page = thumb.rcPage;
    // disk load only first time; stays on fs->thumbnail afterwards
    Pixmap* thumbImg = LoadThumbnail(fs);
    if (thumbImg) {
        thumb.szThumb = Size(thumbImg->width, thumbImg->height);
        gfx->PushClip(page);
        // note: we used to invert bitmaps in dark theme but that doesn't
        // make sense for thumbnails
        gfx->DrawPixmap(thumbImg, page);
        gfx->PopClip();
    }
    DrawHomeRoundedOutline(gfx, page, 10, ThemeWindowTextColor(), kThumbsBorderDx);

    const Rect& rect = thumb.rcText;
    Str path = fs->filePath;
    TempStr fileName = path::GetBaseNameTemp(path);
    u32 fmt = gfxTextEllipsis | (isRtl ? gfxTextRight : gfxTextLeft);
    DrawMaybeHighlightedText(gfx, rect, fileName, filterWords, highlighted, backgroundColor, isRtl, false, fmt,
                             fontText, ThemeWindowTextColor());

    Pixmap* icon = GetFileStateIconPixmap(fs);
    int x = isRtl ? page.x + page.dx - DpiScale(16) : page.x;
    if (icon) {
        gfx->DrawPixmap(icon, {x, rect.y, icon->width, icon->height});
    }
}

// a white circle with a black "?" inside: the home page's affordance for the
// keyboard-shortcuts sheet. The disc is drawn with GDI+ so its edge is smooth;
// nothing is painted outside it, so the page background shows through.
static void DrawHomeHelpButton(Gfx* gfx, Rect r) {
    gfx->FillEllipse(r, kColWhite);
    PlatformFont* font = HomePageFont(14);
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
    cursor = CursorId::Hand;
}

void HomeViewIconCtrl::Paint(VirtPaintCtx& ctx) {
    bool selected = (listView == HomePageIsListView());
    DrawHomeViewButton(ctx.gfx, pixmap, ctx.bounds, selected, HasFlag(vwfHovered));
}

HomeOpenDocCtrl::HomeOpenDocCtrl() {
    cursor = CursorId::Hand;
}

void HomeOpenDocCtrl::Paint(VirtPaintCtx& ctx) {
    if (!pixmap) {
        return;
    }
    Rect r = {ctx.bounds.x + rcIconLocal.x, ctx.bounds.y + rcIconLocal.y, pixmap->width, pixmap->height};
    ctx.gfx->DrawPixmap(pixmap, r);
}

HomeHelpBtnCtrl::HomeHelpBtnCtrl() {
    cursor = CursorId::Hand;
    SetTooltip(_TRA("Keyboard Shortcuts"));
}

void HomeHelpBtnCtrl::Paint(VirtPaintCtx& ctx) {
    DrawHomeHelpButton(ctx.gfx, ctx.bounds);
}

HomeSearchBorderCtrl::HomeSearchBorderCtrl() {
    SetFlag(vwfNoHitTest, true);
}

// border and fill around the search edit; the edit HWND sits inside, so only
// the 1px frame and the padding around it are actually visible
void HomeSearchBorderCtrl::Paint(VirtPaintCtx& ctx) {
    Color bgCol = ThemeControlBackgroundColor();
    ctx.gfx->FillRect(ctx.bounds, bgCol);
    ctx.gfx->DrawRect(ctx.bounds, AccentColor(bgCol, 40));
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

// the band's background; the markup (VirtRichText child) paints over it
void HomeTipCtrl::Paint(VirtPaintCtx& ctx) {
    ctx.gfx->FillRect(ctx.bounds, ThemeControlBackgroundColor());
}

// rcTip is the whole band (its background), rcText where the markup goes
void HomeTipCtrl::Sync(const Rect& rcTip, const Rect& rcText) {
    if (!rich || rcTip.IsEmpty()) {
        visibility = Visibility::Collapse;
        return;
    }
    visibility = Visibility::Visible;
    SetBounds(rcTip);
    rich->SetBounds(rcText);
}

//--- file entries

static Rect HomeEntryRect(const ThumbnailLayout& t);

// the ✕ sits in the top-right corner of the thumbnail (top-left in RTL)
static Rect HomeCloseBtnRectForThumb(const Rect& thumb) {
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
    FileState* fs = FileHistoryFindByPath(path);
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
    if (btn->listView) {
        win->DeleteToolTip();
    }
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
    cursor = CursorId::Hand;
    onGetTooltip = MkMethod1<HomeListIconCtrl, VirtTooltipEvent*, &HomeListIconCtrl::OnGetTooltip>(this);
}

void HomeListIconCtrl::OnGetTooltip(VirtTooltipEvent* ev) {
    auto* entry = (HomeEntryCtrl*)parent;
    FileState* fs = FileHistoryFindByPath(entry->filePath);
    bool pinned = fs && fs->isPinned;
    ev->tip = str::DupTemp(pinned ? _TRA("Unpin") : _TRA("Pin"));
}

HomeEntryCtrl::~HomeEntryCtrl() {
    str::Free(filePath);
}

HomeEntryCtrl::HomeEntryCtrl() {
    cursor = CursorId::Hand;
}

// paints this entry's list row or thumbnail. `layout` points into the
// HomePageLayout being painted; HomePageSyncChrome set it just before
void HomeEntryCtrl::Paint(VirtPaintCtx& ctx) {
    ThumbnailLayout* t = layout;
    auto* entries = (HomeEntriesCtrl*)parent;
    if (!t || !entries || !entries->win || !entries->filterWords || !entries->highlighted) {
        return;
    }
    MainWindow* win = entries->win;
    Gfx* gfx = ctx.gfx;
    bool isRtl = IsUIRtl();
    PlatformFont* fontText = HomePageFont(14);
    Color backgroundColor = ThemeMainWindowBackgroundColor();
    // no selection chrome while typing in the search box
    bool isSelected = (idx == win->homePageSelIdx) && !HomeSearchHasFocus(win);
    // ctx.clip is the entries band (vwfClipChildren on the parent): an entry
    // scrolled partially out must not paint over the header / tip band
    gfx->PushClip(ctx.clip);
    if (HomePageIsListView()) {
        DrawHomeListRow(gfx, *t, *entries->filterWords, *entries->highlighted, fontText, backgroundColor, isRtl,
                        isSelected);
    } else {
        DrawHomeThumbnail(gfx, *t, *entries->filterWords, *entries->highlighted, fontText, backgroundColor, isRtl);
    }
    gfx->PopClip();
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

    chrome->searchBorder = new HomeSearchBorderCtrl();
    chrome->AddChild(chrome->searchBorder);

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
    ILayout* el = ElementFromPoint(root, pt, &ptLocal);
    VirtCtrl* w = el ? el->AsVirtCtrl() : nullptr;
    if (!w || !w->OnSetCursor(ptLocal)) {
        return false;
    }
    // no tooltip of its own means "leave the tooltip alone", not "hide it": a
    // thumbnail entry's tip is put up by HomePageShowSelectionTooltip() and would
    // otherwise be torn down by the WM_SETCURSOR that follows the hover
    TempStr tip = w->GetTooltipTemp(ptLocal);
    if (tip && *tip.s) {
        Rect r = w->BoundsInWindow();
        win->ShowToolTip(tip, r);
    } else if (HomePageIsListView()) {
        HomeEntriesCtrl* entries = HomeEntries(win);
        if (entries && entries->EntryForCtrl(w)) {
            win->DeleteToolTip();
        }
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

    chrome->searchBorder->visibility = l.rcSearchBorder.IsEmpty() ? Visibility::Collapse : Visibility::Visible;
    chrome->searchBorder->SetBounds(l.rcSearchBorder);

    // file entries: clipped to the thumbnails band, like the static links were
    HomeEntriesCtrl* entries = chrome->entries;
    entries->SetBounds(l.rcThumbsArea);
    // what the entries paint with; owned by l, which outlives the paint
    entries->filterWords = &l.filterWords;
    entries->highlighted = &l.highlighted;
    int nEntries = len(l.thumbnails);
    entries->SetEntryCount(nEntries);
    bool listView = HomePageIsListView();
    for (int i = 0; i < nEntries; i++) {
        ThumbnailLayout& t = l.thumbnails[i];
        HomeEntryCtrl* e = entries->EntryAt(i);
        e->idx = i;
        e->layout = &t;
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
            e->closeBtn->SetBounds(HomeCloseBtnRectForThumb(rc.Intersect(l.rcThumbsArea)));
        }
    }
    entries->UpdateCloseBtnVisibility();

    Size iconSize = l.rcIconThumbnailView.Size();
    chrome->thumbView->pixmap = GetCachedPixmapForSvg(gIconHomeThumbnails, iconSize.dx, iconSize.dy);
    chrome->thumbView->SetBounds(l.rcIconThumbnailView);
    chrome->listView->pixmap = GetCachedPixmapForSvg(gIconHomeList, iconSize.dx, iconSize.dy);
    chrome->listView->SetBounds(l.rcIconListView);

    // re-apply: the bounds were set before the parent was positioned
    chrome->hdr->SetBounds(l.freqRead->lastBounds);

    // one click target covering the icon and the link text
    Rect rcOpen = l.rcIconOpen.Union(l.openDoc->lastBounds);
    rcOpen.Inflate(10, 10);
    HomeOpenDocCtrl* od = chrome->openDoc;
    od->pixmap = GetCachedPixmapForSvg(gIconFileOpen, l.rcIconOpen.dx, l.rcIconOpen.dy);
    od->SetBounds(rcOpen);
    od->rcIconLocal = {l.rcIconOpen.x - rcOpen.x, l.rcIconOpen.y - rcOpen.y, l.rcIconOpen.dx, l.rcIconOpen.dy};
    // "Open a document" acts as a link, so it is drawn in the link color
    od->text->SetColor(kColText, gColsLink[kColText]);
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
    Gfx* gfx = l.gfx;
    auto* win = l.win;

    gfx->FillRect(l.rc, ThemeMainWindowBackgroundColor());

    int nThumbs = len(l.thumbnails);
    // keep the keyboard selection inside the (possibly filtered) list
    if (win->homePageSelIdx >= nThumbs) {
        win->homePageSelIdx = nThumbs - 1;
    }

    // the chrome tree paints everything else: search border, file entries
    // (thumbnails / list rows), tip band, header, view buttons, "Open a
    // document..." and the help button (which has to land on top of the tip
    // band, so the chrome keeps it as the last child)
    win->homeRoot->Paint(gfx, l.rc);

    // thumbnails selection outline: over the chrome and unclipped, so its top
    // edge isn't cut off on the first row (list rows draw their own outline,
    // under the row content, in HomeEntryCtrl::Paint)
    int selIdx = win->homePageSelIdx;
    bool showSel = !HomePageIsListView() && !HomeSearchHasFocus(win) && selIdx >= 0 && selIdx < nThumbs;
    if (showSel) {
        ThumbnailLayout& t = l.thumbnails[selIdx];
        if (IsHomeThumbOnScreen(t.rcPage.Union(t.rcText), l.rcThumbsArea)) {
            DrawHomeSelectionOutline(gfx, HomeSelectionOutlineRect(t), 10);
        }
    }
}

void DrawHomePage(MainWindow* win, Gfx* gfx) {
    HomePageLayout l;
    l.rc = HwndClientRect(win->hwndCanvas);
    l.gfx = gfx;
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
static Rect HomeSelectionOutlineRect(const ThumbnailLayout& t) {
    if (HomePageIsListView()) {
        // list: outline is the row, 1px shorter (separator line), with 0.5rem
        // of breathing room on the left and right
        int pad = DpiScale(8);
        return {t.rcListRow.x - pad, t.rcListRow.y, t.rcListRow.dx + (2 * pad), t.rcListRow.dy - 1};
    }
    // thumbnails: page ∪ name, inflated by the same amounts as paint
    Rect sel = t.rcPage.Union(t.rcText);
    sel.Inflate(DpiScale(4), DpiScale(3));
    return sel;
}

// Show/update the infotip for the keyboard-selected home thumbnail. Placed just
// below the blue selection outline, left-aligned with the outline's left edge;
// shifted left if it would extend past the right edge of the last outline in
// that row.
static void HomePageShowSelectionTooltip(MainWindow* win) {
    if (!win || HomePageIsListView() || HomeSearchHasFocus(win)) {
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
    TempStr tip = str::DupTemp(fs->filePath);
    i64 size = file::GetSize(fs->filePath);
    if (size >= 0) {
        tip = fmt("%s  %s", tip, str::FormatSizeShortTemp(size, nullptr));
    }

    HWND hwnd = win->hwndCanvas;
    Rect outline = HomeSelectionOutlineRect(t);
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
        Rect lastOutline = HomeSelectionOutlineRect(c.thumbs[lastInRow]);
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

// File of the entry at (x,y), empty if there is no entry there. Replaces the
// old "look the click up in win->staticLinks" - entries are VirtCtrls now
Str HomePageFilePathAtTemp(MainWindow* win, int x, int y) {
    HomeEntriesCtrl* entries = HomeEntries(win);
    if (!entries) {
        return {};
    }
    Point ptLocal{0, 0};
    ILayout* el = ElementFromPoint(win->homeRoot, {x, y}, &ptLocal);
    VirtCtrl* w = el ? el->AsVirtCtrl() : nullptr;
    HomeEntryCtrl* e = entries->EntryForCtrl(w);
    if (!e) {
        return {};
    }
    return str::DupTemp(e->filePath);
}

// Mouse over a file entry: update homePageSelIdx and, in thumbnail view, show
// the tip at that entry (not at the cursor).
bool HomePageOnHover(MainWindow* win, int x, int y) {
    HomeEntriesCtrl* entries = HomeEntries(win);
    if (!entries) {
        return false;
    }
    Point ptLocal{0, 0};
    ILayout* el = ElementFromPoint(win->homeRoot, {x, y}, &ptLocal);
    VirtCtrl* w = el ? el->AsVirtCtrl() : nullptr;
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
    int lineDy = HomePageIsListView() ? kHomeListRowDy : kThumbnailDy + kThumbsSpaceBetweenY;
    int pageDy = lineDy * 3;

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
    int thumbsRowDy = HomePageIsListView() ? kHomeListRowDy : kThumbnailDy + kThumbsSpaceBetweenY;

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
