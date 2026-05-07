/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Dpi.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"
#include "utils/SquareTreeParser.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/VirtWnd.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "SumatraConfig.h"
#include "FileHistory.h"
#include "GlobalPrefs.h"
#include "Annotation.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "resource.h"
#include "Commands.h"
#include "Accelerators.h"
#include "CommandPalette.h"
#include "FileThumbnails.h"
#include "HomePage.h"
#include "Translations.h"
#include "Version.h"
#include "Theme.h"
#include "AppSettings.h"
#include "OverlayScrollbar.h"
#include "DarkModeSubclass.h"

#include "wingui/WebView.h"
#include <string>
#include <vector>
#include <windows.h>
#include <shlwapi.h>
#include "Toolbar.h"  
#pragma comment(lib, "shlwapi.lib")

#define HOMEPAGE_HTML_PATH "..\\prettysumatra\\webui\\home.html"

static std::string BuildFileUrl(const std::string& filePath) {
    std::string url = "file:///";
    for (char ch : filePath) {
        if (ch == '\\') {
            url.push_back('/');
        } else if (ch == ' ') {
            url += "%20";
        } else {
            url.push_back(ch);
        }
    }
    return url;
}

// Helper: navega al HTML desde archivo para preservar resolución de recursos relativos.
static bool LoadWebViewHtmlFromFile(WebviewWnd* wv, const std::vector<std::string>& candidates) {
    for (auto& p : candidates) {
        if (!PathFileExistsA(p.c_str())) {
            continue;
        }
        std::string fileUrl = BuildFileUrl(p);
        wv->Navigate(fileUrl.c_str());
        return true;
    }
    return false;
}

// Crea el WebView para la HomePage y carga el HTML
void CreateHomePageWebView(MainWindow* win) {

    if (win->homePageWebView) {
        // WebView ya existe: reposicionar y mostrar
        Rect rc = ClientRect(win->hwndCanvas);
        RECT r = { rc.x, rc.y, rc.x + rc.dx, rc.y + rc.dy };
        SetWindowPos(win->homePageWebView->hwnd, nullptr,
                     r.left, r.top, r.right - r.left, r.bottom - r.top,
                     SWP_NOZORDER | SWP_SHOWWINDOW);
        // Ensure the underlying WebView2 controller is resumed and resized after returning from document view.
        win->homePageWebView->SetControllerVisible(true);
        win->homePageWebView->UpdateWebviewSize();
        return;
    }
    

    win->homePageWebView = new WebviewWnd();
    {
        win->homePageWebView->dataDir = str::Dup(GetPathInAppDataDirTemp("webViewData"));
    }

    CreateWebViewArgs args;
    args.parent = win->hwndCanvas;
    Rect rcClient = ClientRect(win->hwndCanvas);
    args.pos = rcClient;

    if (!win->homePageWebView->Create(args)) {
        delete win->homePageWebView;
        win->homePageWebView = nullptr;
        return;
    }

    // --- FIX FLASH: ocultar la ventana WebView2 y sus hijos antes del primer pintado ---
    HWND hwndWV = win->homePageWebView->hwnd;
    // Quitar WS_VISIBLE de la ventana contenedora
    LONG_PTR style = GetWindowLongPtrW(hwndWV, GWL_STYLE);
    SetWindowLongPtrW(hwndWV, GWL_STYLE, style & ~WS_VISIBLE);
    // Ocultar también las ventanas hijas internas del runtime WebView2
    EnumChildWindows(hwndWV, [](HWND child, LPARAM) -> BOOL {
        ShowWindow(child, SW_HIDE);
        return TRUE;
    }, 0);

    // Buscar home.html en varias rutas
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    PathRemoveFileSpecA(exePath);
    std::string exeDir = std::string(exePath);
    std::vector<std::string> candidates = {
        exeDir + "\\prettysumatra\\webui\\home.html",
        exeDir + "\\..\\prettysumatra\\webui\\home.html",
        exeDir + "\\..\\..\\prettysumatra\\webui\\home.html",
    };

    if (!LoadWebViewHtmlFromFile(win->homePageWebView, candidates)) {
        std::string msg = "<html><body style='font-family:sans-serif;padding:20px'><h2>Error: home.html no encontrado</h2><p>Se intentaron:</p><ul>";
        for (auto& p : candidates) msg += "<li>" + p + "</li>";
        msg += "</ul></body></html>";
        win->homePageWebView->SetHtml(msg.c_str());
    }

    

    // --- FIX FLASH: restaurar visibilidad ahora que el HTML ya está cargado ---
    SetWindowLongPtrW(hwndWV, GWL_STYLE, style | WS_VISIBLE);
    SetWindowPos(
        hwndWV, nullptr,
        rcClient.x, rcClient.y, rcClient.dx, rcClient.dy,
        SWP_NOZORDER | SWP_SHOWWINDOW | SWP_FRAMECHANGED
    );
}

#ifndef ABOUT_USE_LESS_COLORS
#define ABOUT_LINE_OUTER_SIZE 2
#else
#define ABOUT_LINE_OUTER_SIZE 1
#endif
#define ABOUT_LINE_SEP_SIZE 1

constexpr const char* sumatraTips = R"(You can [customize scrollbar](CmdChangeScrollbar).
You can [customize keyboard shortcuts](Help/Customizing-keyboard-shortcuts).
You can [customize toolbar](Help/Customize-toolbar).
Press (Key/CmdCommandPalette) to open [command palette](CmdCommandPalette).
To open file from history open [command palette](CmdCommandPalette) with (Key/CmdCommandPalette) and type `#`.
You can [extract text from PDF file](Help/Tool-x-extract-text-from-pdf).
You can [toggle toolbar](CmdToggleToolbar) with (Key/CmdToggleToolbar).
You can [edit PDF annotations](Help/Editing-annotations).
)";

constexpr const char* sumatraPromos = R"(Try [Edna](https://edna.arslexis.io): a note taking web app for power users.
Try [MarkLexis](https://marklexis.arslexis.io): a bookmarking web application.
)";

// TODO: leaks if set
const char* promoFromServer = nullptr;

// a word in a parsed tip; can be part of a link
struct TipWord {
    char* text = nullptr; // owned
    int dx = 0;
    int dy = 0;
    int x = 0;
    int y = 0;
    bool isLink = false;
    int linkIdx = -1; // index into ParsedTip::links
};

struct TipLink {
    char* cmd = nullptr; // owned, the link_command
    int firstWord = 0;
    int lastWord = 0; // inclusive
};

struct ParsedTip {
    Vec<TipWord> words;
    Vec<TipLink> links;
    int totalDy = 0; // computed by layout

    ~ParsedTip() {
        for (auto& w : words) {
            str::Free(w.text);
        }
        for (auto& l : links) {
            str::Free(l.cmd);
        }
    }
};

// resolve (Key/CmdXxx) to keyboard shortcut string
static TempStr ResolveKeyShortcutTemp(const char* cmdName) {
    int cmdId = GetCommandIdByName(cmdName);
    if (cmdId <= 0) {
        return str::DupTemp(cmdName);
    }
    TempStr accel = AppendAccelKeyToMenuStringTemp((TempStr) "", cmdId);
    if (!accel || !*accel) {
        return str::DupTemp(cmdName);
    }
    // AppendAccelKeyToMenuStringTemp prepends \t, skip it
    if (accel[0] == '\t') {
        accel++;
    }
    return accel;
}

// resolve link command to a URL for StaticLink target
static TempStr ResolveLinkCmdTemp(const char* cmd) {
    if (str::StartsWith(cmd, "https://") || str::StartsWith(cmd, "http://")) {
        return str::DupTemp(cmd);
    }
    if (str::StartsWith(cmd, "Help/")) {
        return str::FormatTemp("https://www.sumatrapdfreader.org/docs/%s", cmd + 5);
    }
    // Cmd* - use as-is, will be resolved to command ID on click
    return str::DupTemp(cmd);
}

static void ParseTip(ParsedTip& tip, const char* s) {
    str::Str expanded;
    // first pass: expand (Key/CmdXxx) to shortcut strings
    while (*s) {
        if (*s == '(' && str::StartsWith(s + 1, "Key/")) {
            const char* end = str::FindChar(s, ')');
            if (end) {
                // extract command name between "Key/" and ")"
                const char* cmdStart = s + 5; // skip "(Key/"
                TempStr cmdName = str::DupTemp(cmdStart, (int)(end - cmdStart));
                TempStr shortcut = ResolveKeyShortcutTemp(cmdName);
                expanded.Append(shortcut);
                s = end + 1;
                continue;
            }
        }
        expanded.AppendChar(*s);
        s++;
    }

    // second pass: split into words, detecting [text](link) markdown links
    const char* p = expanded.Get();
    while (*p) {
        // skip spaces
        while (*p == ' ') {
            p++;
        }
        if (!*p) {
            break;
        }

        if (*p == '[') {
            // parse markdown link: [text](cmd)
            const char* textStart = p + 1;
            const char* textEnd = str::FindChar(textStart, ']');
            if (textEnd && textEnd[1] == '(') {
                const char* cmdStart = textEnd + 2;
                const char* cmdEnd = str::FindChar(cmdStart, ')');
                if (cmdEnd) {
                    TempStr linkCmd = str::DupTemp(cmdStart, (int)(cmdEnd - cmdStart));
                    TempStr linkText = str::DupTemp(textStart, (int)(textEnd - textStart));

                    TipLink link;
                    link.cmd = str::Dup(ResolveLinkCmdTemp(linkCmd));
                    link.firstWord = tip.words.Size();

                    // split link text into words
                    const char* lt = linkText;
                    while (*lt) {
                        while (*lt == ' ') {
                            lt++;
                        }
                        if (!*lt) {
                            break;
                        }
                        const char* wordStart = lt;
                        while (*lt && *lt != ' ') {
                            lt++;
                        }
                        TipWord w;
                        w.text = str::Dup(wordStart, (int)(lt - wordStart));
                        w.isLink = true;
                        w.linkIdx = tip.links.Size();
                        tip.words.Append(w);
                    }

                    link.lastWord = tip.words.Size() - 1;
                    tip.links.Append(link);
                    p = cmdEnd + 1;
                    continue;
                }
            }
        }

        // regular word
        const char* wordStart = p;
        while (*p && *p != ' ' && *p != '[') {
            p++;
        }
        if (p > wordStart) {
            TipWord w;
            w.text = str::Dup(wordStart, (int)(p - wordStart));
            tip.words.Append(w);
        }
    }
}

static void MeasureTipWords(ParsedTip& tip, HDC hdc, HFONT font) {
    uint fmt = DT_LEFT | DT_NOCLIP;
    for (auto& w : tip.words) {
        Size sz = HdcMeasureText(hdc, w.text, fmt, font);
        w.dx = sz.dx;
        w.dy = sz.dy;
    }
}

static void LayoutTip(ParsedTip& tip, int areaWidth, int startX, int startY) {
    int x = startX;
    int y = startY;
    int lineHeight = 0;
    int spaceWidth = 4; // approximate space between words
    for (auto& w : tip.words) {
        if (x > startX && x + w.dx > startX + areaWidth) {
            // wrap to next line
            x = startX;
            y += lineHeight + 2;
            lineHeight = 0;
        }
        w.x = x;
        w.y = y;
        x += w.dx + spaceWidth;
        if (w.dy > lineHeight) {
            lineHeight = w.dy;
        }
    }
    tip.totalDy = (y - startY) + lineHeight;
}

static ParsedTip* gParsedTips = nullptr;
static int gParsedTipCount = 0;
static ParsedTip* gParsedPromos = nullptr;
static int gParsedPromoCount = 0;
static bool gSelectedIsPromo = false;
static int gSelectedTipIdx = -1;

static int ParseTipsFromString(const char* src, const char* prefix, ParsedTip*& outTips) {
    StrVec lines;
    Split(&lines, src, "\n");
    int n = 0;
    for (int i = 0; i < lines.Size(); i++) {
        const char* line = lines.At(i);
        if (!str::IsEmptyOrWhiteSpace(line)) {
            n++;
        }
    }
    if (n == 0) {
        return 0;
    }
    outTips = new ParsedTip[n];
    int count = 0;
    for (int i = 0; i < lines.Size(); i++) {
        const char* line = lines.At(i);
        if (str::IsEmptyOrWhiteSpace(line)) {
            continue;
        }
        if (prefix) {
            TempStr prefixed = str::FormatTemp("%s%s", prefix, line);
            ParseTip(outTips[count], prefixed);
        } else {
            ParseTip(outTips[count], line);
        }
        count++;
    }
    return count;
}

static void PickRandomTipOrPromo() {
    bool pickPromo = (gParsedPromoCount > 0) && (rand() % 100 < 30);
    if (pickPromo) {
        gSelectedIsPromo = true;
        gSelectedTipIdx = rand() % gParsedPromoCount;
    } else if (gParsedTipCount > 0) {
        gSelectedIsPromo = false;
        gSelectedTipIdx = rand() % gParsedTipCount;
    }
}

static void EnsureTipsParsed() {
    if (gParsedTips || gParsedPromos) {
        return;
    }
    gParsedTipCount = ParseTipsFromString(sumatraTips, "Tip: ", gParsedTips);
    gParsedPromoCount = ParseTipsFromString(sumatraPromos, nullptr, gParsedPromos);
    PickRandomTipOrPromo();
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

constexpr COLORREF kAboutBorderCol = RGB(0, 0, 0);

constexpr int kAboutLeftRightSpaceDx = 8;
constexpr int kAboutMarginDx = 10;
constexpr int kAboutBoxMarginDy = 6;
constexpr int kAboutTxtDy = 6;
constexpr int kAboutRectPadding = 8;

constexpr int kInnerPadding = 8;

constexpr const char* kSumatraTxtFont = "Segoe UI";
constexpr int kSumatraTxtFontSize = 28;

constexpr const char* kVersionTxtFont = "Segoe UI";
constexpr int kVersionTxtFontSize = 12;

#define LAYOUT_LTR 0

static ATOM gAtomAbout;
static HWND gHwndAbout;
static Tooltip* gAboutTooltip = nullptr;
static const char* gClickedURL = nullptr;

struct AboutLayoutInfoEl {
    /* static data, must be provided */
    const char* leftTxt;
    const char* rightTxt;
    const char* url;

    /* data calculated by the layout */
    Rect leftPos;
    Rect rightPos;
};

static AboutLayoutInfoEl gAboutLayoutInfo[] = {
    {_TRN("website"), _TRN("SumatraPDF website"), kWebsiteURL},
    {_TRN("manual"), _TRN("SumatraPDF manual"), kManualURL},
    {_TRN("forums"), _TRN("SumatraPDF forums"), "https://github.com/sumatrapdfreader/sumatrapdf/discussions"},
    {_TRN("programming"), _TRN("The Programmers"), "https://github.com/sumatrapdfreader/sumatrapdf/blob/master/AUTHORS"},
    {_TRN("licenses"), _TRN("Various Open Source"), "https://github.com/sumatrapdfreader/sumatrapdf/blob/master/AUTHORS"},
#if defined(GIT_COMMIT_ID_STR)
    {_TRN("last change"), _TRN("git commit ") GIT_COMMIT_ID_STR,
     "https://github.com/sumatrapdfreader/sumatrapdf/commit/" GIT_COMMIT_ID_STR},
#endif
#if defined(PRE_RELEASE_VER)
    {_TRN("a note"), _TRN("Pre-release version, for testing only!"), nullptr},
#endif
#ifdef DEBUG
    {_TRN("a note"), _TRN("Debug version, for testing only!"), nullptr},
#endif
    {nullptr, nullptr, nullptr}};

static Vec<StaticLink*> gStaticLinks;

void SetPromoString(const char* s) {
    if (!s) return;
    str::ReplaceWithCopy(&promoFromServer, s);
}

static TempStr GetAppVersionTemp() {
    TempStr s = str::DupTemp("v" CURR_VERSION_STRA);
    if (IsProcess64()) {
        s = str::JoinTemp(s, " 64-bit");
    } else {
        s = str::JoinTemp(s, " 32-bit");
    }
    if (gIsDebugBuild) {
        s = str::JoinTemp(s, " (dbg)");
    }
    return s;
}

constexpr COLORREF kCol1 = RGB(196, 64, 50);
constexpr COLORREF kCol2 = RGB(227, 107, 35);
constexpr COLORREF kCol3 = RGB(93, 160, 40);
constexpr COLORREF kCol4 = RGB(69, 132, 190);
constexpr COLORREF kCol5 = RGB(112, 115, 207);

static void DrawSumatraVersion(HDC hdc, Rect rect) {
    uint fmt = DT_LEFT | DT_NOCLIP;
    HFONT fontSumatraTxt = CreateSimpleFont(hdc, kSumatraTxtFont, kSumatraTxtFontSize);
    HFONT fontVersionTxt = CreateSimpleFont(hdc, kVersionTxtFont, kVersionTxtFontSize);
    HFONT fontSubtitleTxt = CreateSimpleFont(hdc, kVersionTxtFont, 11);

    SetBkMode(hdc, TRANSPARENT);

    Size prettySz = HdcMeasureText(hdc, "Pretty", fmt, fontSumatraTxt);
    Size sumatraSz = HdcMeasureText(hdc, "Sumatra", fmt, fontSumatraTxt);
    Size pdfSz = HdcMeasureText(hdc, "PDF", fmt, fontSumatraTxt);
    Size subtitleSz = HdcMeasureText(hdc, _TRA("Focused reading"), fmt, fontSubtitleTxt);
    TempStr ver = GetAppVersionTemp();
    Size verSz = HdcMeasureText(hdc, ver, fmt, fontVersionTxt);

    int textDy = prettySz.dy + DpiScale(hdc, 16);
    int iconSize = textDy + DpiScale(hdc, 8);
    int totalDx = iconSize + DpiScale(hdc, 16) + prettySz.dx + sumatraSz.dx + pdfSz.dx;
    int x0 = rect.x + (rect.dx - totalDx) / 2;
    int y0 = rect.y + (rect.dy - iconSize) / 2;

    Rect iconRect(x0, y0, iconSize, iconSize);
    {
        AutoDeleteBrush brOuter(CreateSolidBrush(RGB(242, 184, 0)));
        AutoDeletePen penOuter(CreatePen(PS_SOLID, 1, RGB(215, 167, 0)));
        HGDIOBJ oldBrush = SelectObject(hdc, brOuter);
        HGDIOBJ oldPen = SelectObject(hdc, penOuter);
        RoundRect(hdc, iconRect.x, iconRect.y, iconRect.x + iconRect.dx, iconRect.y + iconRect.dy, DpiScale(hdc, 14),
                  DpiScale(hdc, 14));
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
    }
    Rect pageRect(iconRect.x + DpiScale(hdc, 11), iconRect.y + DpiScale(hdc, 8), iconRect.dx - DpiScale(hdc, 20),
                  iconRect.dy - DpiScale(hdc, 16));
    {
        AutoDeleteBrush brPage(CreateSolidBrush(RGB(255, 249, 229)));
        AutoDeletePen penPage(CreatePen(PS_SOLID, 1, RGB(221, 188, 84)));
        HGDIOBJ oldBrush = SelectObject(hdc, brPage);
        HGDIOBJ oldPen = SelectObject(hdc, penPage);
        RoundRect(hdc, pageRect.x, pageRect.y, pageRect.x + pageRect.dx, pageRect.y + pageRect.dy, DpiScale(hdc, 8),
                  DpiScale(hdc, 8));
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
    }
    Rect lensRect(iconRect.x + iconRect.dx - DpiScale(hdc, 24), iconRect.y + iconRect.dy - DpiScale(hdc, 24), DpiScale(hdc, 18),
                  DpiScale(hdc, 18));
    {
        AutoDeleteBrush brLens(CreateSolidBrush(RGB(247, 220, 128)));
        AutoDeletePen penLens(CreatePen(PS_SOLID, DpiScale(hdc, 2), RGB(204, 151, 0)));
        HGDIOBJ oldBrush = SelectObject(hdc, brLens);
        HGDIOBJ oldPen = SelectObject(hdc, penLens);
        Ellipse(hdc, lensRect.x, lensRect.y, lensRect.x + lensRect.dx, lensRect.y + lensRect.dy);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
    }

    int textX = iconRect.x + iconRect.dx + DpiScale(hdc, 16);
    int textY = iconRect.y + DpiScale(hdc, 3);
    int sumatraX = textX + prettySz.dx;
    int pdfX = sumatraX + sumatraSz.dx;

    Rect accentBg(sumatraX - DpiScale(hdc, 2), textY + prettySz.dy - DpiScale(hdc, 9), sumatraSz.dx + DpiScale(hdc, 4),
                  DpiScale(hdc, 7));
    FillRect(hdc, accentBg, RGB(255, 231, 145));

    SetTextColor(hdc, ThemeWindowTextColor());
    HdcDrawText(hdc, "Pretty", Point(textX, textY), fmt, fontSumatraTxt);
    SetTextColor(hdc, RGB(215, 167, 0));
    HdcDrawText(hdc, "Sumatra", Point(sumatraX, textY), fmt, fontSumatraTxt);
    SetTextColor(hdc, ThemeWindowTextColor());
    HdcDrawText(hdc, "PDF", Point(pdfX, textY), fmt, fontSumatraTxt);

    int subY = textY + prettySz.dy + DpiScale(hdc, 1);
    SetTextColor(hdc, ThemeWindowTextColor());
    HdcDrawText(hdc, _TRA("Focused reading"), Point(textX, subY), fmt, fontSubtitleTxt);
    HdcDrawText(hdc, ver, Point(textX + subtitleSz.dx + DpiScale(hdc, 8), subY), fmt, fontVersionTxt);

    Point p = {textX, subY + DpiScale(hdc, 13)};
    if (gIsPreReleaseBuild) {
        HdcDrawText(hdc, _TRA("Pre-release"), p, fmt);
    }
}

// draw on the bottom right
static Rect DrawHideFrequentlyReadLink(HWND hwnd, HDC hdc, const char* txt) {
    HFONT fontLeftTxt = CreateSimpleFont(hdc, "MS Shell Dlg", 16);

    VirtWndText w(hwnd, txt, fontLeftTxt);
    w.isRtl = IsUIRtl();
    w.withUnderline = true;
    Size txtSize = w.GetIdealSize(true);

    auto col = ThemeWindowLinkColor();
    ScopedSelectObject pen(hdc, CreatePen(PS_SOLID, 1, col), true);

    SetTextColor(hdc, col);
    SetBkMode(hdc, TRANSPARENT);
    Rect rc = ClientRect(hwnd);

    int innerPadding = DpiScale(hwnd, kInnerPadding);
    Rect r = {0, 0, txtSize.dx, txtSize.dy};
    PositionRB(rc, r);
    MoveXY(r, -innerPadding, -innerPadding);
    w.SetBounds(r);
    w.Paint(hdc);

    // make the click target larger
    r.Inflate(innerPadding, innerPadding);
    return r;
}

static Size CalcSumatraVersionSize(HDC hdc) {
    HFONT fontSumatraTxt = CreateSimpleFont(hdc, kSumatraTxtFont, kSumatraTxtFontSize);
    HFONT fontVersionTxt = CreateSimpleFont(hdc, kVersionTxtFont, kVersionTxtFontSize);
    HFONT fontSubtitleTxt = CreateSimpleFont(hdc, kVersionTxtFont, 11);

    uint fmt = DT_LEFT | DT_NOCLIP;
    Size prettySz = HdcMeasureText(hdc, "Pretty", fmt, fontSumatraTxt);
    Size sumatraSz = HdcMeasureText(hdc, "Sumatra", fmt, fontSumatraTxt);
    Size pdfSz = HdcMeasureText(hdc, "PDF", fmt, fontSumatraTxt);
    Size subtitleSz = HdcMeasureText(hdc, _TRA("Focused reading"), fmt, fontSubtitleTxt);
    TempStr ver = GetAppVersionTemp();
    Size verSz = HdcMeasureText(hdc, ver, fmt, fontVersionTxt);

    Size sz{};
    int textDx = prettySz.dx + sumatraSz.dx + pdfSz.dx;
    int iconDx = prettySz.dy + DpiScale(hdc, 24);
    sz.dx = iconDx + DpiScale(hdc, 16) + std::max(textDx, subtitleSz.dx + verSz.dx + DpiScale(hdc, 8)) + DpiScale(hdc, 20);
    sz.dy = std::max(iconDx, prettySz.dy + DpiScale(hdc, 24)) + DpiScale(hdc, kAboutBoxMarginDy * 2);
    return sz;
}

static TempStr TrimGitTemp(const char* s) {
    if (gitCommidId && str::EndsWith(s, gitCommidId)) {
        auto sLen = str::Len(s);
        auto gitLen = str::Len(gitCommidId);
        s = str::DupTemp(s, sLen - gitLen - 7);
    }
    return TempStr(s);
}

/* Draws the about screen and remembers some state for hyperlinking.
   It transcribes the design I did in graphics software - hopeless
   to understand without seeing the design. */
static void DrawAbout(HWND hwnd, HDC hdc, Rect rect, Vec<StaticLink*>& staticLinks) {
    auto col = ThemeWindowTextColor();
    AutoDeletePen penBorder(CreatePen(PS_SOLID, ABOUT_LINE_OUTER_SIZE, col));
    AutoDeletePen penDivideLine(CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, col));
    col = ThemeWindowLinkColor();
    AutoDeletePen penLinkLine(CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, col));

    HFONT fontLeftTxt = CreateSimpleFont(hdc, kLeftTextFont, kLeftTextFontSize);
    HFONT fontRightTxt = CreateSimpleFont(hdc, kRightTextFont, kRightTextFontSize);

    ScopedSelectObject font(hdc, fontLeftTxt); /* Just to remember the orig font */

    Rect rc = ClientRect(hwnd);
    col = ThemeMainWindowBackgroundColor();
    AutoDeleteBrush brushAboutBg = CreateSolidBrush(col);
    FillRect(hdc, rc, brushAboutBg);

    /* render title */
    Rect titleRect(rect.TL(), CalcSumatraVersionSize(hdc));

    ScopedSelectObject brush(hdc, CreateSolidBrush(col), true);
    ScopedSelectObject pen(hdc, penBorder);
#ifndef ABOUT_USE_LESS_COLORS
    Rectangle(hdc, rect.x, rect.y + ABOUT_LINE_OUTER_SIZE, rect.x + rect.dx,
              rect.y + titleRect.dy + ABOUT_LINE_OUTER_SIZE);
#else
    Rect titleBgBand(0, rect.y, rc.dx, titleRect.dy);
    RECT rcLogoBg = titleBgBand.ToRECT();
    FillRect(hdc, &rcLogoBg, brushAboutBg);
    DrawLine(hdc, Rect(0, rect.y, rc.dx, 0));
    DrawLine(hdc, Rect(0, rect.y + titleRect.dy, rc.dx, 0));
#endif

    titleRect.Offset((rect.dx - titleRect.dx) / 2, 0);
    DrawSumatraVersion(hdc, titleRect);

    /* render attribution box */
    col = ThemeWindowTextColor();
    SetTextColor(hdc, col);
    SetBkMode(hdc, TRANSPARENT);

#ifndef ABOUT_USE_LESS_COLORS
    Rectangle(hdc, rect.x, rect.y + titleRect.dy, rect.x + rect.dx, rect.y + rect.dy);
#endif

    /* render text on the left*/
    SelectObject(hdc, fontLeftTxt);
    uint fmt = DT_LEFT | DT_NOCLIP;
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        auto& pos = el->leftPos;
        HdcDrawText(hdc, _TRA(el->leftTxt), pos, fmt);
    }

    /* render text on the right */
    SelectObject(hdc, fontRightTxt);
    SelectObject(hdc, penLinkLine);
    DeleteVecMembers(staticLinks);
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        bool hasUrl = CanAccessDisk() && el->url;
        if (hasUrl) {
            col = ThemeWindowLinkColor();
        } else {
            col = ThemeWindowTextColor();
        }
        SetTextColor(hdc, col);
        TempStr s = TrimGitTemp(_TRA(el->rightTxt));
        auto& pos = el->rightPos;
        HdcDrawText(hdc, s, pos, fmt);

        if (hasUrl) {
            int underlineY = pos.y + pos.dy - 3;
            DrawLine(hdc, Rect(pos.x, underlineY, pos.dx, 0));
            auto sl = new StaticLink(pos, el->url, el->url);
            staticLinks.Append(sl);
        }
    }

    SelectObject(hdc, penDivideLine);
    Rect divideLine(gAboutLayoutInfo[0].rightPos.x - DpiScale(hwnd, kAboutLeftRightSpaceDx), rect.y + titleRect.dy + 4,
                    0, rect.y + rect.dy - 4 - gAboutLayoutInfo[0].rightPos.y);
    DrawLine(hdc, divideLine);
}

static void UpdateAboutLayoutInfo(HWND hwnd, HDC hdc, Rect* rect) {
    HFONT fontLeftTxt = CreateSimpleFont(hdc, kLeftTextFont, kLeftTextFontSize);
    HFONT fontRightTxt = CreateSimpleFont(hdc, kRightTextFont, kRightTextFontSize);

    /* calculate minimal top box size */
    Size headerSize = CalcSumatraVersionSize(hdc);

    /* calculate left text dimensions */
    int leftLargestDx = 0;
    int leftDy = 0;
    uint fmt = DT_LEFT;
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        Size txtSize = HdcMeasureText(hdc, _TRA(el->leftTxt), fmt, fontLeftTxt);
        el->leftPos.dx = txtSize.dx;
        el->leftPos.dy = txtSize.dy;

        if (el == &gAboutLayoutInfo[0]) {
            leftDy = el->leftPos.dy;
        } else {
            ReportIf(leftDy != el->leftPos.dy);
        }
        if (leftLargestDx < el->leftPos.dx) {
            leftLargestDx = el->leftPos.dx;
        }
    }

    /* calculate right text dimensions */
    int rightLargestDx = 0;
    int rightDy = 0;
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        TempStr s = TrimGitTemp(_TRA(el->rightTxt));
        Size txtSize = HdcMeasureText(hdc, s, fmt, fontRightTxt);
        el->rightPos.dx = txtSize.dx;
        el->rightPos.dy = txtSize.dy;

        if (el == &gAboutLayoutInfo[0]) {
            rightDy = el->rightPos.dy;
        } else {
            ReportIf(rightDy != el->rightPos.dy);
        }
        if (rightLargestDx < el->rightPos.dx) {
            rightLargestDx = el->rightPos.dx;
        }
    }

    int leftRightSpaceDx = DpiScale(hwnd, kAboutLeftRightSpaceDx);
    int marginDx = DpiScale(hwnd, kAboutMarginDx);
    int aboutTxtDy = DpiScale(hwnd, kAboutTxtDy);
    /* calculate total dimension and position */
    Rect minRect;
    minRect.dx = leftRightSpaceDx + leftLargestDx + ABOUT_LINE_SEP_SIZE + rightLargestDx + leftRightSpaceDx;
    if (minRect.dx < headerSize.dx) {
        minRect.dx = headerSize.dx;
    }
    minRect.dx += 2 * ABOUT_LINE_OUTER_SIZE + 2 * marginDx;

    minRect.dy = headerSize.dy;
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        minRect.dy += rightDy + aboutTxtDy;
    }
    minRect.dy += 2 * ABOUT_LINE_OUTER_SIZE + 4;

    Rect rc = ClientRect(hwnd);
    minRect.x = (rc.dx - minRect.dx) / 2;
    minRect.y = (rc.dy - minRect.dy) / 2;

    if (rect) {
        *rect = minRect;
    }

    /* calculate text positions */
    int linePosX = ABOUT_LINE_OUTER_SIZE + marginDx + leftLargestDx + leftRightSpaceDx;
    int currY = minRect.y + headerSize.dy + 4;
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        el->leftPos.x = minRect.x + linePosX - leftRightSpaceDx - el->leftPos.dx;
        el->leftPos.y = currY + (rightDy - leftDy) / 2;
        el->rightPos.x = minRect.x + linePosX + leftRightSpaceDx;
        el->rightPos.y = currY;
        currY += rightDy + aboutTxtDy;
    }
}

static void OnPaintAbout(HWND hwnd) {
    PAINTSTRUCT ps;
    Rect rc;
    HDC hdc = BeginPaint(hwnd, &ps);
    SetLayout(hdc, LAYOUT_LTR);
    UpdateAboutLayoutInfo(hwnd, hdc, &rc);
    DrawAbout(hwnd, hdc, rc, gStaticLinks);
    EndPaint(hwnd, &ps);
}

static void OnSizeAbout(HWND hwnd) {
    // TODO: do I need anything here?
}

static void CopyAboutInfoToClipboard() {
    str::Str info(512);
    TempStr ver = GetAppVersionTemp();
    info.AppendFmt("%s %s\r\n", kAppName, ver);
    for (int i = info.Size() - 2; i > 0; i--) {
        info.AppendChar('-');
    }
    info.Append("\r\n");
    // concatenate all the information into a single string
    // (cf. CopyPropertiesToClipboard in SumatraProperties.cpp)
    int maxLen = 0;
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        maxLen = std::max(maxLen, (int)str::Len(_TRA(el->leftTxt)));
    }
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        int labelLen = (int)str::Len(_TRA(el->leftTxt));
        for (int i = maxLen - labelLen; i > 0; i--) {
            info.AppendChar(' ');
        }
        info.AppendFmt("%s: %s\r\n", _TRA(el->leftTxt), el->url ? el->url : _TRA(el->rightTxt));
    }
    CopyTextToClipboard(info.LendData());
}

TempStr GetStaticLinkAtTemp(Vec<StaticLink*>& staticLinks, int x, int y, StaticLink** linkOut) {
    if (!CanAccessDisk()) {
        return nullptr;
    }

    Point pt(x, y);
    for (int i = 0; i < staticLinks.Size(); i++) {
        if (staticLinks.at(i)->rect.Contains(pt)) {
            auto link = staticLinks.At(i);
            if (linkOut) {
                *linkOut = link;
            }
            return str::DupTemp(link->target);
        }
    }

    return nullptr;
}

static void CreateInfotipForLink(StaticLink* linkInfo) {
    if (gAboutTooltip != nullptr) {
        return;
    }

    Tooltip::CreateArgs args;
    args.parent = gHwndAbout;
    args.font = GetAppFont();
    args.isRtl = IsUIRtl();

    gAboutTooltip = new Tooltip();
    gAboutTooltip->Create(args);
    gAboutTooltip->SetSingle(linkInfo->tooltip, linkInfo->rect, false);
}

static void DeleteInfotip() {
    if (gAboutTooltip == nullptr) {
        return;
    }
    // gAboutTooltip->Hide();
    delete gAboutTooltip;
    gAboutTooltip = nullptr;
}

LRESULT CALLBACK WndProcAbout(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    const char* url;
    Point pt;

    int x = GET_X_LPARAM(lp);
    int y = GET_Y_LPARAM(lp);
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

        case WM_SIZE:
            OnSizeAbout(hwnd);
            break;

        case WM_PAINT:
            OnPaintAbout(hwnd);
            break;

        case WM_SETCURSOR:
            pt = HwndGetCursorPos(hwnd);
            if (!pt.IsEmpty()) {
                StaticLink* linkInfo;
                if (GetStaticLinkAtTemp(gStaticLinks, pt.x, pt.y, &linkInfo)) {
                    CreateInfotipForLink(linkInfo);
                    SetCursorCached(IDC_HAND);
                    return TRUE;
                }
            }
            DeleteInfotip();
            return DefWindowProc(hwnd, msg, wp, lp);

        case WM_LBUTTONDOWN: {
            url = GetStaticLinkAtTemp(gStaticLinks, x, y, nullptr);
            str::ReplaceWithCopy(&gClickedURL, url);
        } break;

        case WM_LBUTTONUP:
            url = GetStaticLinkAtTemp(gStaticLinks, x, y, nullptr);
            if (url && str::Eq(url, gClickedURL)) {
                SumatraLaunchBrowser(url);
            }
            break;

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

    TempWStr title = ToWStrTemp(_TRA("About SumatraPDF"));
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
    Rect rc;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(gHwndAbout, &ps);
    SetLayout(hdc, LAYOUT_LTR);
    UpdateAboutLayoutInfo(gHwndAbout, hdc, &rc);
    EndPaint(gHwndAbout, &ps);
    int rectPadding = DpiScale(gHwndAbout, kAboutRectPadding);
    rc.Inflate(rectPadding, rectPadding);

    // resize the new window to just match these dimensions
    Rect wRc = WindowRect(gHwndAbout);
    Rect cRc = ClientRect(gHwndAbout);
    wRc.dx += rc.dx - cRc.dx;
    wRc.dy += rc.dy - cRc.dy;
    MoveWindow(gHwndAbout, wRc.x, wRc.y, wRc.dx, wRc.dy, FALSE);

    HwndPositionInCenterOf(gHwndAbout, win->hwndFrame);
    ShowWindow(gHwndAbout, SW_SHOW);
}

void DrawAboutPage(MainWindow* win, HDC hdc) {
    Rect rc = ClientRect(win->hwndCanvas);
    UpdateAboutLayoutInfo(win->hwndCanvas, hdc, &rc);
    DrawAbout(win->hwndCanvas, hdc, rc, win->staticLinks);
    if (HasPermission(Perm::SavePreferences | Perm::DiskAccess) && SettingsRememberOpenedFiles()) {
        Rect rect = DrawHideFrequentlyReadLink(win->hwndCanvas, hdc, _TRA("Show frequently read"));
        auto sl = new StaticLink(rect, kLinkShowList);
        win->staticLinks.Append(sl);
    }
}

/* alternate static page to display when no document is loaded */

constexpr int kThumbsSeparatorDy = 2;
constexpr int kThumbsBorderDx = 1;
#define kThumbsMarginLeft DpiScale(hdc, 40)
#define kThumbsMarginRight DpiScale(hdc, 40)
#define kThumbsMarginTop DpiScale(hdc, 50)
#define kThumbsMarginBottom DpiScale(hdc, 40)
#define kThumbsSpaceBetweenX DpiScale(hdc, 38)
#define kThumbsSpaceBetweenY DpiScale(hdc, 58)
#define kThumbsBottomBoxDy DpiScale(hdc, 50)

struct ThumbnailLayout {
    Rect rcPage;
    Size szThumb;
    Rect rcText;
    FileState* fs = nullptr; // info needed to draw the thumbnail
    StaticLink* sl = nullptr;
};

struct HomePageLayout {
    // args in
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    Rect rc;
    MainWindow* win = nullptr;

    Rect rcAppWithVer; // SumatraPDF colorful text + version
    Rect rcLine;       // line under bApp
    Rect rcIconOpen;

    HIMAGELIST himlOpen = nullptr;
    VirtWndText* freqRead = nullptr;
    VirtWndText* openDoc = nullptr;
    VirtWndText* hideShowFreqRead = nullptr;
    Vec<ThumbnailLayout> thumbnails; // info for each thumbnail
    int totalContentDy = 0;          // total height of all thumbnail rows
    int thumbsVisibleDy = 0;         // visible height for thumbnails area
    Rect rcThumbsArea;               // clip rect for thumbnails

    // search filter
    StrVec filterWords;
    Vec<u8> highlighted;
    Rect rcSearchBorder; // border rect drawn around the edit control
    Rect rcLauncherOpenPdf;
    Rect rcLauncherReopenLast;

    // tip layout
    Rect rcTip;               // background rect for tip area
    ParsedTip* tip = nullptr; // points to gParsedTips or gParsedPromos, not owned

    ~HomePageLayout();
};

HomePageLayout::~HomePageLayout() {
    delete freqRead;
    delete openDoc;
}

constexpr int kThumbsMiddleMargin = 30;
constexpr int kSearchEditDy = 40;
constexpr int kHeaderSearchGapY = 30;
constexpr int kSearchThumbnailsGapY = 50;
constexpr int kRecentGridExtraTopGapY = 70;
constexpr int kLauncherTopGapY = 10;
constexpr int kLauncherCardsGapY = 10;
constexpr int kLauncherMainDy = 56;
constexpr int kLauncherSecondaryDy = 48;
constexpr int kHomePageOuterPadX = 24;
constexpr int kHomePageOuterPadY = 20;
constexpr int kHomePageHeroRadius = 18;
constexpr int kHomePageCardRadius = 14;

static WNDPROC DefWndProcHomeSearch = nullptr;

static LRESULT CALLBACK WndProcHomeSearch(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
        HwndSetText(hwnd, "");
        MainWindow* win = FindMainWindowByHwnd(GetParent(hwnd));
        if (win) {
            HwndSetFocus(win->hwndCanvas);
            win->RedrawAll(true);
        }
        return 0;
    }
    if (msg == WM_MOUSEWHEEL) {
        HWND parent = GetParent(hwnd);
        return SendMessageW(parent, msg, wp, lp);
    }
    return CallWindowProcW(DefWndProcHomeSearch, hwnd, msg, wp, lp);
}

static void EnsureHomeSearchCreated(MainWindow* win) {
    if (win->hwndHomeSearch) {
        return;
    }
    HMODULE hmod = GetModuleHandleW(nullptr);
    DWORD style = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL;
    DWORD exStyle = 0;
    win->hwndHomeSearch = CreateWindowExW(exStyle, WC_EDITW, L"", style, 0, 0, 100, kSearchEditDy, win->hwndCanvas,
                                          nullptr, hmod, nullptr);
    HDC hdc = GetDC(win->hwndCanvas);
    HFONT font = CreateSimpleFont(hdc, "MS Shell Dlg", 14);
    ReleaseDC(win->hwndCanvas, hdc);
    SetWindowFont(win->hwndHomeSearch, font, TRUE);
    if (!DefWndProcHomeSearch) {
        DefWndProcHomeSearch = (WNDPROC)GetWindowLongPtr(win->hwndHomeSearch, GWLP_WNDPROC);
    }
    SetWindowLongPtr(win->hwndHomeSearch, GWLP_WNDPROC, (LONG_PTR)WndProcHomeSearch);
    Edit_SetCueBannerText(win->hwndHomeSearch, _TRW("Search files or open a document"));
    // add left/right padding so text doesn't overlap the border
    int margin = DpiScale(win->hwndCanvas, 8);
    SendMessage(win->hwndHomeSearch, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(margin, margin));
}


void HomePageDestroySearch(MainWindow* win) {
    if (win->hwndHomeSearch) {
        DestroyWindow(win->hwndHomeSearch);
        win->hwndHomeSearch = nullptr;
    }
}

// Oculta la HomePage WebView cuando se cierra
void HomePageHide(MainWindow* win) {
    if (win->homePageWebView) {
        ShowWindow(win->homePageWebView->hwnd, SW_HIDE);
    }
}

// Destruye la HomePage WebView (llamar al cerrar ventana)
void HomePageDestroy(MainWindow* win) {
    if (win->homePageWebView) {
        DestroyWindow(win->homePageWebView->hwnd);
        delete win->homePageWebView;
        win->homePageWebView = nullptr;
    }
}

void HomePageFocusSearch(MainWindow* win) {
    EnsureHomeSearchCreated(win);
    ShowWindow(win->hwndHomeSearch, SW_SHOW);
    HwndSetFocus(win->hwndHomeSearch);
}

void PickAnotherRandomPromotion() {
    PickAnotherRandomTip();
}

void LayoutHomePage(HomePageLayout& l) {
    EnsureTipsParsed();

    Vec<FileState*> allFileStates;
    if (gGlobalPrefs->homePageSortByFrequentlyRead) {
        gFileHistory.GetFrequencyOrder(allFileStates);
    } else {
        gFileHistory.GetRecentlyOpenedOrder(allFileStates);
    }
    auto hwnd = l.hwnd;
    auto hdc = l.hdc;
    auto rc = l.rc;
    auto win = l.win;

    // filter by search query if present
    TempStr searchQuery = nullptr;
    if (win->hwndHomeSearch) {
        searchQuery = HwndGetTextTemp(win->hwndHomeSearch);
    }
    bool hasFilter = searchQuery && searchQuery[0];
    if (hasFilter) {
        SplitFilterToWords(searchQuery, l.filterWords);
    }
    Vec<FileState*> fileStates;
    for (int i = 0; i < allFileStates.Size(); i++) {
        FileState* fs = allFileStates.at(i);
        if (hasFilter) {
            TempStr baseName = path::GetBaseNameTemp(fs->filePath);
            if (!FilterMatches(baseName, l.filterWords)) {
                continue;
            }
        }
        fileStates.Append(fs);
    }

    bool isRtl = IsUIRtl();
    HFONT hdrFont = CreateSimpleFont(hdc, "MS Shell Dlg", 24);

    Size sz = CalcSumatraVersionSize(hdc);
    {
        Rect& r = l.rcAppWithVer;
        r.x = rc.dx - sz.dx - 3;
        r.y = 0;
        r.SetSize(sz);
    }

    l.rcLine = {0, sz.dy, rc.dx, 0};

    int nFilesForLayout = allFileStates.Size();
    int colsForLayout =
        (rc.dx - kThumbsMarginLeft - kThumbsMarginRight + kThumbsSpaceBetweenX) / (kThumbnailDx + kThumbsSpaceBetweenX);
    colsForLayout = std::max(colsForLayout, 1);
    if (rc.dx < DpiScale(hdc, 900)) {
        colsForLayout = std::min(colsForLayout, 2);
    }
    if (rc.dx < DpiScale(hdc, 700)) {
        colsForLayout = 1;
    }
    int thumbsColsForLayout = colsForLayout;
    int thumbsStartX = rc.x + kThumbsMarginLeft +
                       (rc.dx - thumbsColsForLayout * kThumbnailDx - (thumbsColsForLayout - 1) * kThumbsSpaceBetweenX -
                        kThumbsMarginLeft - kThumbsMarginRight) /
                           2;
    if (thumbsStartX < DpiScale(hdc, kInnerPadding)) {
        thumbsStartX = DpiScale(hdc, kInnerPadding);
    } else if (nFilesForLayout == 0) {
        thumbsStartX = kThumbsMarginLeft;
    }

    int thumbsContentWidth = thumbsColsForLayout * kThumbnailDx + (thumbsColsForLayout - 1) * kThumbsSpaceBetweenX;
    int thumbsEndX = thumbsStartX + thumbsContentWidth;
    int rowPadX = 0;
    int rowY = DpiScale(hdc, 12);
    int rowLeftX = thumbsStartX;
    int rowRightX = thumbsEndX;

    const char* txt = _TRA("Recently Opened");
    if (gGlobalPrefs->homePageSortByFrequentlyRead) {
        txt = _TRA("Frequently Read");
    }
    auto hdr = new VirtWndText(hwnd, txt, hdrFont);
    l.freqRead = hdr;
    hdr->isRtl = isRtl;
    Size hdrSize = hdr->GetIdealSize(true);
    int rowHeight = hdrSize.dy;
    int hdrX = rowLeftX + rowPadX;
    int hdrDx = std::max(0, rowRightX - hdrX);
    Rect rcHdr(hdrX, rowY, hdrDx, hdrSize.dy);

    if (isRtl) {
        rcHdr.x = rc.dx - rcHdr.x - rcHdr.dx;
    }

    hdr->SetBounds(rcHdr);

    int headerBottomY = rowY + rowHeight;

    EnsureHomeSearchCreated(win);
    int searchEditDy = DpiScale(hdc, kSearchEditDy);
    int headerSearchGap = DpiScale(hdc, kHeaderSearchGapY);
    int searchThumbsGap = DpiScale(hdc, kSearchThumbnailsGapY);
    {
        int borderDx = thumbsContentWidth;
        int maxBorderDx = rc.dx - 2 * DpiScale(hdc, kHomePageOuterPadX);
        borderDx = std::min(borderDx, maxBorderDx);
        borderDx = std::max(borderDx, DpiScale(hdc, 240));
        int borderX = thumbsStartX;
        int borderY = headerBottomY + headerSearchGap;
        int borderDy = searchEditDy + 2;
        l.rcSearchBorder = {borderX, borderY, borderDx, borderDy};

        HFONT editFont = (HFONT)SendMessage(win->hwndHomeSearch, WM_GETFONT, 0, 0);
        TEXTMETRIC tm;
        HFONT oldFont = (HFONT)SelectObject(hdc, editFont);
        GetTextMetrics(hdc, &tm);
        SelectObject(hdc, oldFont);
        int fontDy = tm.tmHeight + tm.tmExternalLeading + 2;
        int editDy = std::min(fontDy, searchEditDy);
        int editY = borderY + 1 + (searchEditDy - editDy) / 2;
        MoveWindow(win->hwndHomeSearch, borderX + 1, editY, borderDx - 2, editDy, TRUE);
    }

    int searchAreaDy = headerSearchGap + searchEditDy + 2 + searchThumbsGap;
    headerBottomY += searchAreaDy;

    int launcherTop = headerBottomY + DpiScale(hdc, kLauncherTopGapY);
    int cardsGapY = DpiScale(hdc, kLauncherCardsGapY);
    int mainDy = DpiScale(hdc, kLauncherMainDy);
    int secondaryDy = DpiScale(hdc, kLauncherSecondaryDy);

    int launcherDx = l.rcSearchBorder.dx;
    int launcherX = l.rcSearchBorder.x;
    l.rcLauncherOpenPdf = {launcherX, launcherTop, launcherDx, mainDy};
    l.rcLauncherReopenLast = {launcherX, launcherTop + mainDy + cardsGapY, launcherDx, secondaryDy};
    headerBottomY = launcherTop + mainDy + cardsGapY + secondaryDy;

    if (isRtl) {
        auto mirrorRect = [rc](Rect& r) {
            r.x = rc.dx - r.x - r.dx;
        };
        mirrorRect(l.rcLauncherOpenPdf);
        mirrorRect(l.rcLauncherReopenLast);
    }

    headerBottomY += DpiScale(hdc, kRecentGridExtraTopGapY);

    int tipHeight = 0;
    HFONT fontTip = CreateSimpleFont(hdc, "MS Shell Dlg", 16);
    ParsedTip* tip = nullptr;
    if (gGlobalPrefs->showTips && gSelectedTipIdx >= 0) {
        if (gSelectedIsPromo && gSelectedTipIdx < gParsedPromoCount) {
            tip = &gParsedPromos[gSelectedTipIdx];
        } else if (!gSelectedIsPromo && gSelectedTipIdx < gParsedTipCount) {
            tip = &gParsedTips[gSelectedTipIdx];
        }
    }
    if (tip) {
        MeasureTipWords(*tip, hdc, fontTip);
        int tipPadding = DpiScale(hdc, 8);
        int tipTextWidth = thumbsColsForLayout * kThumbnailDx + (thumbsColsForLayout - 1) * kThumbsSpaceBetweenX;
        LayoutTip(*tip, tipTextWidth, 0, 0);
        tipHeight = tip->totalDy + 2 * tipPadding;
    }

    int thumbsTopY = headerBottomY;
    int thumbsBottomY = rc.dy - tipHeight - kThumbsMiddleMargin;
    int thumbsVisibleDy = std::max(0, thumbsBottomY - thumbsTopY);

    l.rcThumbsArea = {0, thumbsTopY, rc.dx, thumbsVisibleDy};

    int nFiles = fileStates.Size();
    int thumbsCols = thumbsColsForLayout;
    int thumbsRows = (nFiles + thumbsCols - 1) / thumbsCols;
    int thumbsContentDy = thumbsRows * (kThumbnailDy + kThumbsSpaceBetweenY) - kThumbsSpaceBetweenY;

    int scrollY = win->homePageScrollY;
    int maxScrollY = std::max(0, thumbsContentDy - thumbsVisibleDy);
    if (scrollY > maxScrollY) {
        scrollY = maxScrollY;
        win->homePageScrollY = scrollY;
    }
    l.totalContentDy = thumbsContentDy;
    l.thumbsVisibleDy = thumbsVisibleDy;

    Point ptOff(thumbsStartX, thumbsTopY - scrollY);

    for (int row = 0; row < thumbsRows; row++) {
        for (int col = 0; col < thumbsCols; col++) {
            if (row * thumbsCols + col >= nFiles) {
                thumbsRows = col > 0 ? row + 1 : row;
                break;
            }

            ThumbnailLayout& thumb = *l.thumbnails.AppendBlanks(1);
            FileState* fs = fileStates.at(row * thumbsCols + col);
            thumb.fs = fs;

            Rect rcPage(ptOff.x + col * (kThumbnailDx + kThumbsSpaceBetweenX),
                        ptOff.y + row * (kThumbnailDy + kThumbsSpaceBetweenY), kThumbnailDx, kThumbnailDy);
            if (isRtl) {
                rcPage.x = rc.dx - rcPage.x - rcPage.dx;
            }
            RenderedBitmap* thumbImg = LoadThumbnail(fs);
            if (thumbImg) {
                Size szThumb = thumbImg->GetSize();
                if (szThumb.dx != kThumbnailDx || szThumb.dy != kThumbnailDy) {
                    rcPage.dy = szThumb.dy * kThumbnailDx / szThumb.dx;
                    rcPage.y += kThumbnailDy - rcPage.dy;
                }
                thumb.szThumb = szThumb;
            }
            thumb.rcPage = rcPage;
            int iconSpace = DpiScale(hdc, 20);
            Rect rcText(rcPage.x + iconSpace, rcPage.y + rcPage.dy + 3, rcPage.dx - iconSpace, iconSpace);
            if (isRtl) {
                rcText.x -= iconSpace;
            }
            thumb.rcText = rcText;
            char* path = fs->filePath;
            Rect slRect = rcText.Union(rcPage).Intersect(l.rcThumbsArea);
            if (!slRect.IsEmpty()) {
                thumb.sl = new StaticLink(slRect, path, path);
                win->staticLinks.Append(thumb.sl);
            }
        }
    }

    if (tip) {
        Rect rcClient = ClientRect(win->hwndCanvas);
        int tipPadding = DpiScale(hdc, 8);

        int tipY = rcClient.dy - tipHeight;
        l.rcTip = {0, tipY, rcClient.dx, tipHeight};
        l.tip = tip;

        int tipTextWidth = thumbsColsForLayout * kThumbnailDx + (thumbsColsForLayout - 1) * kThumbsSpaceBetweenX;
        int tipStartX = thumbsStartX;
        int tipStartY = tipY + tipPadding;
        LayoutTip(*tip, tipTextWidth, tipStartX, tipStartY);

        for (auto& link : tip->links) {
            Rect linkRect;
            for (int i = link.firstWord; i <= link.lastWord; i++) {
                auto& w = tip->words[i];
                Rect wr = {w.x, w.y, w.dx, w.dy};
                if (i == link.firstWord) {
                    linkRect = wr;
                } else {
                    linkRect = linkRect.Union(wr);
                }
            }
            auto slTip = new StaticLink(linkRect, link.cmd, link.cmd);
            win->staticLinks.Append(slTip);
        }
        auto slBg = new StaticLink(l.rcTip, kLinkNextTip);
        win->staticLinks.Append(slBg);
    }
}

static void GetFileStateIcon(FileState* fs) {
    if (fs->himl) {
        return;
    }
    SHFILEINFO sfi{};
    sfi.iIcon = -1;
    uint flags = SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES;
    WCHAR* filePathW = ToWStrTemp(fs->filePath);
    fs->himl = (HIMAGELIST)SHGetFileInfoW(filePathW, 0, &sfi, sizeof(sfi), flags);
    fs->iconIdx = sfi.iIcon;
}

void DrawHomePageLayout(HomePageLayout& l) {
    bool isRtl = IsUIRtl();
    auto hdc = l.hdc;
    auto win = l.win;
    auto textColor = ThemeWindowTextColor();
    auto backgroundColor = PrettySurfaceAltColor();
    bool darkUi = DarkMode::isColorDark(backgroundColor);
    COLORREF panelCol = darkUi ? AccentColor(backgroundColor, 14) : AdjustLightness2(backgroundColor, 7);
    COLORREF panelBorderCol = darkUi ? AccentColor(panelCol, 24) : PrettyBorderColor();
    COLORREF searchCol = PrettySurfaceColor();
    COLORREF searchBorderCol = darkUi ? AccentColor(searchCol, 28) : AccentColor(searchCol, 22);
    COLORREF mutedCol = darkUi ? RGB(170, 178, 190) : RGB(93, 103, 117);
    COLORREF accent = PrettyAccentColor();

    Rect rc = ClientRect(win->hwndCanvas);
    FillRect(hdc, rc, backgroundColor);

    int shellX = std::max(DpiScale(hdc, kHomePageOuterPadX), l.rcSearchBorder.x - DpiScale(hdc, 24));
    int shellY = DpiScale(hdc, kHomePageOuterPadY);
    int shellRightLimit = rc.dx - DpiScale(hdc, kHomePageOuterPadX);
    int shellRight = std::min(shellRightLimit, l.rcSearchBorder.x + l.rcSearchBorder.dx + DpiScale(hdc, 24));
    int shellBottom = l.rcSearchBorder.y + l.rcSearchBorder.dy + DpiScale(hdc, 16);
    Rect shell(shellX, shellY, std::max(120, shellRight - shellX), std::max(DpiScale(hdc, 90), shellBottom - shellY));
    Rect shadow = shell;
    shadow.y += DpiScale(hdc, 2);
    {
        AutoDeleteBrush brShadow(CreateSolidBrush(darkUi ? AccentColor(backgroundColor, 6) : RGB(216, 226, 241)));
        AutoDeletePen penShadow(CreatePen(PS_SOLID, 1, darkUi ? AccentColor(backgroundColor, 10) : RGB(204, 216, 234)));
        HGDIOBJ oldBrush = SelectObject(hdc, brShadow);
        HGDIOBJ oldPen = SelectObject(hdc, penShadow);
        RoundRect(hdc, shadow.x, shadow.y, shadow.x + shadow.dx, shadow.y + shadow.dy, DpiScale(hdc, kHomePageHeroRadius),
                  DpiScale(hdc, kHomePageHeroRadius));
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
    }
    {
        AutoDeleteBrush brHero(CreateSolidBrush(panelCol));
        AutoDeletePen penHero(CreatePen(PS_SOLID, 1, panelBorderCol));
        HGDIOBJ oldBrush = SelectObject(hdc, brHero);
        HGDIOBJ oldPen = SelectObject(hdc, penHero);
        RoundRect(hdc, shell.x, shell.y, shell.x + shell.dx, shell.y + shell.dy, DpiScale(hdc, kHomePageHeroRadius),
                  DpiScale(hdc, kHomePageHeroRadius));
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
    }

    HFONT fontTitle = CreateSimpleFont(hdc, "MS Shell Dlg", 24);
    HFONT fontSub = CreateSimpleFont(hdc, "MS Shell Dlg", 14);
    HFONT fontBody = CreateSimpleFont(hdc, "MS Shell Dlg", 14);
    HFONT fontAction = CreateSimpleFont(hdc, "MS Shell Dlg", 15);
    HFONT fontActionSub = CreateSimpleFont(hdc, "MS Shell Dlg", 12);
    HFONT fontTip = CreateSimpleFont(hdc, "MS Shell Dlg", 13);

    int innerX = shell.x + DpiScale(hdc, 28);
    int innerRight = shell.x + shell.dx - DpiScale(hdc, 28);
    int y = shell.y + DpiScale(hdc, 24);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textColor);
    Rect rcTitle(innerX, y, shell.dx - DpiScale(hdc, 56), DpiScale(hdc, 34));
    HdcDrawText(hdc, _TRA("Open and read PDFs"), rcTitle,
                DT_SINGLELINE | DT_NOPREFIX | (isRtl ? DT_RIGHT : DT_LEFT), fontTitle);
    y += DpiScale(hdc, 38);

    Rect rcSub(innerX, y, shell.dx - DpiScale(hdc, 56), DpiScale(hdc, 22));
    SetTextColor(hdc, mutedCol);
    HdcDrawText(hdc, _TRA("Access recent files, search documents, and open a PDF in one click."), rcSub,
                DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX | (isRtl ? DT_RIGHT : DT_LEFT), fontSub);
    y += DpiScale(hdc, 28);

    // Draw visual frame for the real search edit control.
    {
        const Rect& sb = l.rcSearchBorder;
        AutoDeleteBrush brSearch(CreateSolidBrush(searchCol));
        AutoDeletePen penSearch(CreatePen(PS_SOLID, 1, searchBorderCol));
        HGDIOBJ oldBrush = SelectObject(hdc, brSearch);
        HGDIOBJ oldPen = SelectObject(hdc, penSearch);
        RoundRect(hdc, sb.x, sb.y, sb.x + sb.dx, sb.y + sb.dy, DpiScale(hdc, 12), DpiScale(hdc, 12));
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
    }

    auto drawLauncherCard = [&](const Rect& rr, COLORREF fillCol, COLORREF borderCol, COLORREF titleCol, COLORREF subCol,
                                const char* title, const char* subtitle, int radius) {
        AutoDeleteBrush brCard(CreateSolidBrush(fillCol));
        AutoDeletePen penCard(CreatePen(PS_SOLID, 1, borderCol));
        HGDIOBJ oldBrush = SelectObject(hdc, brCard);
        HGDIOBJ oldPen = SelectObject(hdc, penCard);
        RoundRect(hdc, rr.x, rr.y, rr.x + rr.dx, rr.y + rr.dy, DpiScale(hdc, radius), DpiScale(hdc, radius));
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);

        int padX = DpiScale(hdc, 14);
        int padY = DpiScale(hdc, 10);
        Rect rtTitle(rr.x + padX, rr.y + padY, rr.dx - 2 * padX, DpiScale(hdc, 24));
        Rect rtSub(rr.x + padX, rr.y + padY + DpiScale(hdc, 24), rr.dx - 2 * padX, rr.dy - DpiScale(hdc, 28));
        SetTextColor(hdc, titleCol);
        HdcDrawText(hdc, title, rtTitle, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX | (isRtl ? DT_RIGHT : DT_LEFT),
                    fontAction);
        SetTextColor(hdc, subCol);
        HdcDrawText(hdc, subtitle, rtSub,
                    DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX | (isRtl ? DT_RIGHT : DT_LEFT), fontActionSub);
    };

    COLORREF launcherMainFill = darkUi ? AccentColor(accent, 46) : AccentColor(accent, 26);
    COLORREF launcherMainBorder = darkUi ? AccentColor(accent, 64) : AccentColor(accent, 45);
    COLORREF launcherMainText = RGB(255, 255, 255);
    COLORREF launcherSubFill = darkUi ? AccentColor(panelCol, 20) : RGB(251, 253, 255);
    COLORREF launcherSubBorder = darkUi ? AccentColor(panelCol, 40) : AccentColor(panelBorderCol, 28);
    bool canReopenLast = RecentlyCloseDocumentsCount() > 0;
    COLORREF launcherDisabledFill = darkUi ? AccentColor(panelCol, 8) : RGB(240, 244, 249);
    COLORREF launcherDisabledBorder = darkUi ? AccentColor(panelCol, 18) : AccentColor(panelBorderCol, 8);
    COLORREF launcherDisabledText = darkUi ? RGB(132, 142, 156) : RGB(128, 137, 150);

    // Draw launcher cards
    drawLauncherCard(
        l.rcLauncherOpenPdf,
        launcherMainFill, launcherMainBorder,
        launcherMainText, launcherMainText,
        _TRA("Open a document"),
        _TRA("Browse your files to open any PDF"),
        kHomePageCardRadius
    );
    {
        COLORREF subTextCol = canReopenLast ? textColor : launcherDisabledText;
        COLORREF subFill    = canReopenLast ? launcherSubFill    : launcherDisabledFill;
        COLORREF subBorder  = canReopenLast ? launcherSubBorder  : launcherDisabledBorder;
        drawLauncherCard(
            l.rcLauncherReopenLast,
            subFill, subBorder,
            subTextCol, subTextCol,
            _TRA("Reopen last document"),
            _TRA("Continue where you left off"),
            kHomePageCardRadius
        );
    }

    int sectionY = l.rcThumbsArea.y - DpiScale(hdc, 36);
    int minSectionY = l.rcSearchBorder.y + l.rcSearchBorder.dy + DpiScale(hdc, 16);
    sectionY = std::max(sectionY, minSectionY);
    Rect rcSection(innerX, sectionY, shell.dx - DpiScale(hdc, 56), DpiScale(hdc, 24));
    SetTextColor(hdc, textColor);
    const char* sectionTitle = l.freqRead ? l.freqRead->s : _TRA("Recently Opened");
    HdcDrawText(hdc, sectionTitle, rcSection, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX | (isRtl ? DT_RIGHT : DT_LEFT),
                fontSub);

    Rect ta = l.rcThumbsArea;
    HRGN thumbsClip = CreateRectRgn(ta.x, ta.y, ta.x + ta.dx, ta.y + ta.dy);
    SelectClipRgn(hdc, thumbsClip);
    DeleteObject(thumbsClip);

    AutoDeletePen penThumbBorder(CreatePen(PS_SOLID, 1, panelBorderCol));
    for (const ThumbnailLayout& thumb : l.thumbnails) {
        FileState* fs = thumb.fs;
        const Rect& page = thumb.rcPage;
        const Rect& rect = thumb.rcText;

        Rect card = page.Union(rect);
        card.Inflate(DpiScale(hdc, 8), DpiScale(hdc, 8));
        {
            AutoDeleteBrush brCard(CreateSolidBrush(darkUi ? AccentColor(panelCol, 8) : RGB(252, 253, 254)));
            HGDIOBJ oldBrush = SelectObject(hdc, brCard);
            HGDIOBJ oldPen = SelectObject(hdc, penThumbBorder);
            RoundRect(hdc, card.x, card.y, card.x + card.dx, card.y + card.dy, DpiScale(hdc, kHomePageCardRadius),
                      DpiScale(hdc, kHomePageCardRadius));
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);
        }

        RenderedBitmap* thumbImg = LoadThumbnail(fs);
        if (thumbImg) {
            int savedDC = SaveDC(hdc);
            Rect pageIn = page;
            pageIn.Inflate(-DpiScale(hdc, 2), -DpiScale(hdc, 2));
            HRGN clip = CreateRoundRectRgn(pageIn.x, pageIn.y, pageIn.x + pageIn.dx, pageIn.y + pageIn.dy, DpiScale(hdc, 8),
                                           DpiScale(hdc, 8));
            ExtSelectClipRgn(hdc, clip, RGN_AND);
            thumbImg->Blit(hdc, pageIn);
            RestoreDC(hdc, savedDC);
            DeleteObject(clip);
        }

        DrawMaybeHighlightedTextArgs hlArgs(l.filterWords, l.highlighted);
        hlArgs.hdc = hdc;
        hlArgs.rc = ToRECT(rect);
        hlArgs.text = path::GetBaseNameTemp(fs->filePath);
        hlArgs.colBg = backgroundColor;
        hlArgs.isRtl = isRtl;
        hlArgs.drawFmt = DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX | (isRtl ? DT_RIGHT : DT_LEFT);
        DrawMaybeHighlightedText(hlArgs);

        GetFileStateIcon(fs);
        int x = isRtl ? page.x + page.dx - DpiScale(hdc, 16) : page.x;
        ImageList_Draw(fs->himl, fs->iconIdx, hdc, x, rect.y, ILD_TRANSPARENT);
    }
    SelectClipRgn(hdc, nullptr);

    if (HasPermission(Perm::SavePreferences | Perm::DiskAccess) && SettingsRememberOpenedFiles()) {
        Rect rcFreqRead = DrawHideFrequentlyReadLink(win->hwndCanvas, hdc, _TRA("Show frequently read"));
        auto sl = new StaticLink(rcFreqRead, kLinkShowList);
        win->staticLinks.Append(sl);
    }

    if (l.tip) {
        COLORREF tipBgCol = darkUi ? AccentColor(panelCol, 10) : RGB(245, 248, 252);
        FillRect(hdc, l.rcTip, tipBgCol);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, textColor);
        for (auto& w : l.tip->words) {
            Point pt(w.x, w.y);
            SetTextColor(hdc, w.isLink ? accent : textColor);
            HdcDrawText(hdc, w.text, pt, DT_LEFT | DT_NOCLIP, fontTip);
        }

        AutoDeletePen penTipLinkLine(CreatePen(PS_SOLID, 1, accent));
        SelectObject(hdc, penTipLinkLine);
        for (auto& link : l.tip->links) {
            auto& first = l.tip->words[link.firstWord];
            auto& last = l.tip->words[link.lastWord];
            int underlineY = first.y + first.dy - 3;
            int x1 = first.x;
            int x2 = last.x + last.dx;
            DrawLine(hdc, Rect(x1, underlineY, x2 - x1, 0));
        }
    }
}

void DrawHomePage(MainWindow* win, HDC /*hdc*/) {
    // Muestra el WebView moderno en lugar del renderizado nativo
    CreateHomePageWebView(win);
}

void HomePageOnVScroll(MainWindow* win, WPARAM wp) {
    USHORT msg = LOWORD(wp);
    HDC hdc = GetDC(win->hwndCanvas);
    int lineDy = kThumbnailDy + kThumbsSpaceBetweenY;
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
    if (newScrollY < 0) {
        newScrollY = 0;
    }
    if (newScrollY != win->homePageScrollY) {
        win->homePageScrollY = newScrollY;
        InvalidateRect(win->hwndCanvas, nullptr, FALSE);
    }
}

void HomePageOnMouseWheel(MainWindow* win, int delta) {
    Rect rc = ClientRect(win->hwndCanvas);
    HDC hdc = GetDC(win->hwndCanvas);
    int thumbsRowDy = kThumbnailDy + kThumbsSpaceBetweenY;
    ReleaseDC(win->hwndCanvas, hdc);

    int scrollBy = thumbsRowDy / 3;
    if (delta > 0) {
        scrollBy = -scrollBy;
    }
    int newScrollY = win->homePageScrollY + scrollBy;
    if (newScrollY < 0) {
        newScrollY = 0;
    }
    if (newScrollY != win->homePageScrollY) {
        win->homePageScrollY = newScrollY;
        InvalidateRect(win->hwndCanvas, nullptr, FALSE);
    }
}
