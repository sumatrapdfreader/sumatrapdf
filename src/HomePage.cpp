/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/Dpi.h"
#include "base/File.h"
#include "base/Win.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/VirtWnd.h"

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
#include "TipText.h"
#include "HomePage.h"
#include "Translations.h"
#include "Version.h"
#include "Theme.h"
#include "AppSettings.h"
#include "DarkModeSubclass.h"
#include "SvgIcons.h"

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
You can preview where a citation, figure or footnote link points by hovering it - enable in [advanced settings](CmdAdvancedSettings) via CitationHoverDelay.
)tips");

static Str sumatraPromos = StrL(R"promos(Try [Edna](https://edna.arslexis.io): a note taking web app for power users.
Try [MarkLexis](https://marklexis.arslexis.io): a bookmarking web application.
)promos");

static Str promoFromServer;

static bool IsTipWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void AdvanceTipText(Str& s, int n = 1) {
    ReportIf(n < 0 || n > s.len);
    if (n >= s.len) {
        s = {};
        return;
    }
    s.s += n;
    s.len -= n;
}

static void SkipTipWhitespace(Str& s) {
    while (len(s) > 0 && IsTipWhitespace(s.s[0])) {
        AdvanceTipText(s);
    }
}

// closing ']' for the '[' at (textStart - 1); supports nested brackets in link text
static Str FindMarkdownLinkTextEnd(Str textStart) {
    int depth = 1;
    for (int i = 0; i < textStart.len; i++) {
        char c = textStart.s[i];
        if (c == '[') {
            depth++;
        } else if (c == ']') {
            depth--;
            if (depth == 0) {
                return Str(textStart.s + i, textStart.len - i);
            }
        }
    }
    return {};
}

// closing ')' for the '(' before cmdStart; balances parens in http(s) targets
static Str FindMarkdownLinkCmdEnd(Str cmdStart) {
    if (str::StartsWith(cmdStart, "http://") || str::StartsWith(cmdStart, "https://")) {
        int depth = 0;
        for (int i = 0; i < cmdStart.len; i++) {
            char c = cmdStart.s[i];
            if (c == '(') {
                depth++;
            } else if (c == ')') {
                if (depth > 0) {
                    depth--;
                } else {
                    return Str(cmdStart.s + i, cmdStart.len - i);
                }
            }
        }
        return {};
    }
    return str::SliceFromChar(cmdStart, ')');
}

static void AppendTipWordsFromText(ParsedTip& tip, Str text, bool isLink, int linkIdx) {
    int i = 0;
    while (i < text.len) {
        while (i < text.len && IsTipWhitespace(text.s[i])) {
            i++;
        }
        if (i >= text.len) {
            break;
        }
        int wordStart = i;
        while (i < text.len && !IsTipWhitespace(text.s[i])) {
            i++;
        }
        TipWord w;
        str::ReplaceWithCopy(&w.text, Str(text.s + wordStart, i - wordStart));
        w.isLink = isLink;
        w.linkIdx = linkIdx;
        tip.words.Append(w);
    }
}

// resolve (Key/CmdXxx) to keyboard shortcut string
static TempStr ResolveKeyShortcutTemp(Str cmdName) {
    int cmdId = GetCommandIdByName(cmdName);
    if (cmdId <= 0) {
        return str::DupTemp(cmdName);
    }
    TempStr accel = AppendAccelKeyToMenuStringTemp("", cmdId);
    if (!accel || !*accel.s) {
        return str::DupTemp(cmdName);
    }
    // AppendAccelKeyToMenuStringTemp prepends \t, skip it
    if (accel.s[0] == '\t') {
        return Str(accel.s + 1);
    }
    return accel;
}

// resolve link command to a URL for StaticLink target
static TempStr ResolveLinkCmdTemp(Str cmd) {
    if (str::StartsWith(cmd, "https://") || str::StartsWith(cmd, "http://")) {
        return str::DupTemp(cmd);
    }
    if (str::StartsWith(cmd, "Help/")) {
        // cmd is a non-NUL-terminated view into the tip line, so %s must get a
        // zero-terminated copy of exactly the remainder -- otherwise it reads
        // past the link, pulling in trailing chars like ")."
        return fmt("https://www.sumatrapdfreader.org/docs/%s", Str(CStrTemp(Str(cmd.s + 5, cmd.len - 5))));
    }
    // Cmd* - use as-is, will be resolved to command ID on click
    return str::DupTemp(cmd);
}

void ParseTip(ParsedTip& tip, Str s) {
    if (!s) {
        return;
    }
    str::Builder expanded;
    Str sp = s;
    // first pass: expand (Key/CmdXxx) to shortcut strings (only for real commands)
    while (len(sp) > 0) {
        if (sp.s[0] == '(' && sp.len > 5 && str::StartsWith(Str(sp.s + 1, sp.len - 1), "Key/")) {
            int end = str::IndexOfChar(sp, ')');
            if (end >= 0) {
                Str cmdName(sp.s + 5, end - 5); // skip "(Key/"
                if (GetCommandIdByName(cmdName) > 0) {
                    TempStr shortcut = ResolveKeyShortcutTemp(cmdName);
                    expanded.Append(shortcut);
                    AdvanceTipText(sp, end + 1);
                    continue;
                }
            }
        }
        expanded.AppendChar(sp.s[0]);
        AdvanceTipText(sp);
    }

    // second pass: split into words, detecting [text](link) markdown links
    Str p = ToStr(expanded);
    while (len(p) > 0) {
        SkipTipWhitespace(p);
        if (len(p) == 0) {
            break;
        }

        if (p.s[0] == '[') {
            Str textStart(p.s + 1, p.len - 1);
            Str textEnd = FindMarkdownLinkTextEnd(textStart);
            if (textEnd && textEnd.len > 1 && textEnd.s[1] == '(') {
                Str cmdStart(textEnd.s + 2, textEnd.len - 2);
                Str cmdEnd = FindMarkdownLinkCmdEnd(cmdStart);
                if (cmdEnd) {
                    if (textEnd.s > textStart.s) {
                        Str linkCmd(cmdStart.s, (int)(cmdEnd.s - cmdStart.s));
                        Str linkText(textStart.s, (int)(textEnd.s - textStart.s));

                        TipLink link;
                        str::ReplaceWithCopy(&link.cmd, ResolveLinkCmdTemp(linkCmd));
                        link.firstWord = len(tip.words);
                        AppendTipWordsFromText(tip, linkText, true, len(tip.links));

                        if (link.firstWord < len(tip.words)) {
                            link.lastWord = len(tip.words) - 1;
                            tip.links.Append(link);
                            AdvanceTipText(p, (int)(cmdEnd.s - p.s) + 1);
                            continue;
                        }
                        str::Free(link.cmd);
                    } else {
                        // empty [text]: treat the whole markup as literal text
                        TipWord w;
                        str::ReplaceWithCopy(&w.text, Str(p.s, (int)(cmdEnd.s - p.s) + 1));
                        tip.words.Append(w);
                        AdvanceTipText(p, (int)(cmdEnd.s - p.s) + 1);
                        continue;
                    }
                }
            }
            // not a valid [text](link) — fall through (e.g. "[CIW]" in a filename)
        }

        // regular word; '[' is allowed unless it starts a complete [text](link)
        int wordStart = 0;
        int i = 0;
        while (i < p.len && !IsTipWhitespace(p.s[i])) {
            if (p.s[i] == '[') {
                Str textStart(p.s + i + 1, p.len - i - 1);
                Str textEnd = FindMarkdownLinkTextEnd(textStart);
                if (textEnd && textEnd.len > 1 && textEnd.s[1] == '(' &&
                    FindMarkdownLinkCmdEnd(Str(textEnd.s + 2, textEnd.len - 2))) {
                    break;
                }
            }
            i++;
        }
        if (i > wordStart) {
            TipWord w;
            str::ReplaceWithCopy(&w.text, Str(p.s + wordStart, i - wordStart));
            tip.words.Append(w);
        }
        if (i < p.len) {
            AdvanceTipText(p, i);
        } else {
            break;
        }
    }
}

void MeasureTipWords(ParsedTip& tip, HDC hdc, HFONT font) {
    uint fmt = DT_LEFT | DT_NOCLIP | DT_NOPREFIX | DT_SINGLELINE;
    for (auto& w : tip.words) {
        Size sz = HdcMeasureText(hdc, w.text, fmt, font);
        w.dx = sz.dx;
        w.dy = sz.dy;
    }
}

void LayoutTip(ParsedTip& tip, int areaWidth, int startX, int startY) {
    int x = startX;
    int y = startY;
    int lineHeight = 0;
    int spaceWidth = 4; // approximate space between words
    int maxX = startX;
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
        if (x - spaceWidth > maxX) {
            maxX = x - spaceWidth;
        }
        if (w.dy > lineHeight) {
            lineHeight = w.dy;
        }
    }
    tip.totalDx = maxX - startX;
    tip.totalDy = (y - startY) + lineHeight;
}

void DrawTipWords(HDC hdc, ParsedTip& tip, HFONT font, COLORREF textCol, COLORREF linkCol) {
    uint fmt = DT_LEFT | DT_NOCLIP | DT_NOPREFIX | DT_SINGLELINE;
    for (auto& w : tip.words) {
        Point pt = {w.x, w.y};
        SetTextColor(hdc, w.isLink ? linkCol : textCol);
        HdcDrawText(hdc, w.text, pt, fmt, font);
    }
    // underline each link
    HPEN pen = CreatePen(PS_SOLID, 1, linkCol);
    HGDIOBJ prevPen = SelectObject(hdc, pen);
    for (auto& link : tip.links) {
        auto& first = tip.words[link.firstWord];
        auto& last = tip.words[link.lastWord];
        int underlineY = first.y + first.dy - 3;
        int x1 = first.x;
        int x2 = last.x + last.dx;
        DrawLine(hdc, Rect(x1, underlineY, x2 - x1, 0));
    }
    SelectObject(hdc, prevPen);
    DeleteObject(pen);
}

int HitTestTipLink(ParsedTip& tip, int x, int y) {
    for (auto& w : tip.words) {
        if (!w.isLink) {
            continue;
        }
        Rect wr = {w.x, w.y, w.dx, w.dy};
        if (wr.Contains(Point(x, y))) {
            return w.linkIdx;
        }
    }
    return -1;
}

void ExecuteTipLink(HWND hwnd, Str cmd) {
    if (len(cmd) == 0) {
        return;
    }
    if (str::StartsWith(cmd, "Cmd")) {
        int cmdId = GetCommandIdByName(cmd);
        if (cmdId > 0) {
            HwndSendCommand(hwnd, cmdId);
        }
        return;
    }
    if (str::StartsWith(cmd, "http://") || str::StartsWith(cmd, "https://")) {
        // documentation links open in the embedded manual browser
        if (!MaybeLaunchDocumentation(cmd)) {
            SumatraLaunchBrowser(cmd);
        }
    }
}

bool TipHasLinks(ParsedTip& tip) {
    return len(tip.links) > 0;
}

TempStr TipPlainTextTemp(ParsedTip& tip) {
    str::Builder sb;
    for (int i = 0; i < len(tip.words); i++) {
        if (i > 0) {
            sb.AppendChar(' ');
        }
        sb.Append(tip.words[i].text);
    }
    return ToStrTemp(sb);
}

// must fit all non-empty lines in sumatraTips / sumatraPromos
constexpr int kMaxHomeTips = 16;
constexpr int kMaxHomePromos = 8;

static ParsedTip gParsedTipsStorage[kMaxHomeTips];
static int gParsedTipCount = 0;
static ParsedTip gParsedPromosStorage[kMaxHomePromos];
static int gParsedPromoCount = 0;
static bool gTipsParsed = false;
static bool gSelectedIsPromo = false;
static int gSelectedTipIdx = -1;

static void ResetHomeCloseBtn();

static int ParseTipsFromString(Str src, Str prefix, ParsedTip* buffer, int bufferCap) {
    StrVec lines;
    Split(&lines, src, "\n");
    int n = 0;
    for (int i = 0; i < len(lines); i++) {
        Str line = lines[i];
        if (!str::IsEmptyOrWhiteSpace(line)) {
            n++;
        }
    }
    if (n == 0) {
        return 0;
    }
    ReportIf(n > bufferCap);
    int count = 0;
    for (int i = 0; i < len(lines); i++) {
        Str line = lines[i];
        if (str::IsEmptyOrWhiteSpace(line)) {
            continue;
        }
        if (prefix) {
            TempStr prefixed = str::JoinTemp(prefix, line);
            ParseTip(buffer[count], prefixed);
        } else {
            ParseTip(buffer[count], line);
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
    if (gTipsParsed) {
        return;
    }
    gParsedTipCount = ParseTipsFromString(sumatraTips, "Tip: ", gParsedTipsStorage, kMaxHomeTips);
    gParsedPromoCount = ParseTipsFromString(sumatraPromos, {}, gParsedPromosStorage, kMaxHomePromos);
    gTipsParsed = true;
    PickRandomTipOrPromo();
}

void FreeHomePageTips() {
    if (gTipsParsed) {
        for (int i = 0; i < gParsedTipCount; i++) {
            gParsedTipsStorage[i].Reset();
        }
        for (int i = 0; i < gParsedPromoCount; i++) {
            gParsedPromosStorage[i].Reset();
        }
        gParsedTipCount = 0;
        gParsedPromoCount = 0;
        gTipsParsed = false;
    }
    str::Free(promoFromServer);
    ResetHomeCloseBtn();
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

static const Str kSumatraTxtFont = StrL("Arial Black");
constexpr int kSumatraTxtFontSize = 24;

static const Str kVersionTxtFont = StrL("Arial Black");
constexpr int kVersionTxtFontSize = 12;

#define LAYOUT_LTR 0

static ATOM gAtomAbout;
static HWND gHwndAbout;
static Tooltip* gAboutTooltip = nullptr;
static Str gClickedURL;

struct AboutLayoutInfoEl {
    /* static data, must be provided */
    Str leftTxt;
    Str rightTxt;
    Str url;

    /* data calculated by the layout */
    Rect leftPos;
    Rect rightPos;
};

static AboutLayoutInfoEl gAboutLayoutInfo[] = {
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

static Vec<StaticLink*> gStaticLinks;

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

constexpr COLORREF kCol1 = RGB(196, 64, 50);
constexpr COLORREF kCol2 = RGB(227, 107, 35);
constexpr COLORREF kCol3 = RGB(93, 160, 40);
constexpr COLORREF kCol4 = RGB(69, 132, 190);
constexpr COLORREF kCol5 = RGB(112, 115, 207);

static void DrawSumatraVersion(HDC hdc, Rect rect) {
    uint fmt = DT_LEFT | DT_NOCLIP;
    HFONT fontSumatraTxt = CreateSimpleFont(hdc, kSumatraTxtFont, kSumatraTxtFontSize);
    HFONT fontVersionTxt = CreateSimpleFont(hdc, kVersionTxtFont, kVersionTxtFontSize);

    SetBkMode(hdc, TRANSPARENT);

    Str txt = kAppName;
    Size txtSize = HdcMeasureText(hdc, txt, fmt, fontSumatraTxt);
    Rect mainRect(rect.x + (rect.dx - txtSize.dx) / 2, rect.y + (rect.dy - txtSize.dy) / 2, txtSize.dx, txtSize.dy);

    // draw SumatraPDF in colorful way
    Point pt = mainRect.TL();
    // colorful version
    static COLORREF cols[] = {kCol1, kCol2, kCol3, kCol4, kCol5, kCol5, kCol4, kCol3, kCol2, kCol1};
    char buf[2] = {};
    for (int i = 0; i < len(kAppName); i++) {
        SetTextColor(hdc, cols[i % dimofi(cols)]);
        buf[0] = kAppName[i];
        HdcDrawText(hdc, buf, pt, fmt, fontSumatraTxt);
        txtSize = HdcMeasureText(hdc, buf, fmt, fontSumatraTxt);
        pt.x += txtSize.dx;
    }

    SetTextColor(hdc, ThemeWindowTextColor());
    int x = mainRect.x + mainRect.dx + DpiScale(hdc, kInnerPadding);
    int y = mainRect.y;

    TempStr ver = GetAppVersionTemp();
    Point p = {x, y};
    HdcDrawText(hdc, ver, p, fmt, fontVersionTxt);
    p.y += DpiScale(hdc, 13);
    if (gIsPreReleaseBuild) {
        HdcDrawText(hdc, "Pre-release", p, fmt);
    }
}

// draw on the bottom right
static Rect DrawHideFrequentlyReadLink(HWND hwnd, HDC hdc, Str txt) {
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

    /* calculate minimal top box size */
    Size sz = HdcMeasureText(hdc, kAppName, fontSumatraTxt);
    sz.dy = sz.dy + DpiScale(hdc, kAboutBoxMarginDy * 2);

    /* consider version and version-sub strings */
    TempStr ver = GetAppVersionTemp();
    Size txtSize = HdcMeasureText(hdc, ver, fontVersionTxt);
    int minWidth = txtSize.dx + DpiScale(hdc, 8);
    int dx = std::max(txtSize.dx, minWidth);
    sz.dx += 2 * (dx + DpiScale(hdc, kInnerPadding));
    return sz;
}

static TempStr TrimGitTemp(Str s) {
    if (gitCommidId && str::EndsWith(s, gitCommidId)) {
        int sLen = len(s);
        int gitLen = len(gitCommidId);
        return str::DupTemp(Str(s.s, sLen - gitLen - 7));
    }
    return s;
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
    FillRect(hdc, &rcLogoBg, bgBrush);
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
        HdcDrawText(hdc, el->leftTxt, pos, fmt);
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
        TempStr s = TrimGitTemp(el->rightTxt);
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
        Size txtSize = HdcMeasureText(hdc, el->leftTxt, fmt, fontLeftTxt);
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
        TempStr s = TrimGitTemp(el->rightTxt);
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
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        maxLen = std::max(maxLen, len(el->leftTxt));
    }
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        for (int i = maxLen - len(el->leftTxt); i > 0; i--) {
            info.AppendChar(' ');
        }
        info.Append(fmt("%s: %s\r\n", el->leftTxt, el->url ? el->url.s : el->rightTxt));
    }
    CopyTextToClipboard(ToStr(info));
}

TempStr GetStaticLinkAtTemp(Vec<StaticLink*>& staticLinks, int x, int y, StaticLink** linkOut) {
    if (!CanAccessDisk()) {
        return {};
    }

    Point pt(x, y);
    for (int i = 0; i < len(staticLinks); i++) {
        if (staticLinks[i]->rect.Contains(pt)) {
            auto link = staticLinks[i];
            if (linkOut) {
                *linkOut = link;
            }
            return str::DupTemp(link->target);
        }
    }

    return {};
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
    TempStr url;
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
#define kHomeListThumbDx DpiScale(hdc, 38)
#define kHomeListThumbDy DpiScale(hdc, 50)
#define kHomeListRowDy DpiScale(hdc, 58)
#define kHomeListRowGapDx DpiScale(hdc, 8)

struct ThumbnailLayout {
    Rect rcPage;
    Size szThumb;
    Rect rcText;
    Rect rcListRow;
    Rect rcListThumb;
    Rect rcListFileName;
    Rect rcListSize;
    Rect rcListRemove;
    Rect rcListPin;
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
    Rect rcIconListView;
    Rect rcIconThumbnailView;

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

    // tip layout
    Rect rcTip;               // background rect for tip area
    ParsedTip* tip = nullptr; // points into gParsedTipsStorage or gParsedPromosStorage, not owned

    ~HomePageLayout();
};

HomePageLayout::~HomePageLayout() {
    delete freqRead;
    delete openDoc;
}

constexpr int kOpenDocumentYShift = 7;
constexpr int kThumbsMiddleMargin = 32;
constexpr int kSearchEditDy = 28;
constexpr int kHeaderSearchGapY = 12;
constexpr int kSearchThumbnailsGapY = 12;

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
    Edit_SetCueBannerText(win->hwndHomeSearch, L"search files (Ctrl + F)");
    // add left/right padding so text doesn't overlap the border
    int margin = DpiScale(win->hwndCanvas, 6);
    SendMessage(win->hwndHomeSearch, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(margin, margin));
    // restore the query from before the edit control was destroyed
    // (e.g. by switching to a document tab and back)
    if (len(win->homeSearchQuery) > 0) {
        HwndSetText(win->hwndHomeSearch, win->homeSearchQuery);
    }
}

void HomePageDestroySearch(MainWindow* win) {
    if (win->hwndHomeSearch) {
        TempStr query = HwndGetTextTemp(win->hwndHomeSearch);
        str::ReplaceWithCopy(&win->homeSearchQuery, query);
        DestroyWindow(win->hwndHomeSearch);
        win->hwndHomeSearch = nullptr;
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

// thumbnail tooltip: the file path, then two spaces and a human-readable size
static TempStr HomeThumbTooltipTemp(Str path) {
    i64 size = file::GetSize(path);
    if (size < 0) {
        return str::DupTemp(path);
    }
    return fmt("%s  %s", path, str::FormatSizeShortTemp(size, nullptr));
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
    HFONT fontText = CreateSimpleFont(hdc, "MS Shell Dlg", 14);
    HFONT hdrFont = CreateSimpleFont(hdc, "MS Shell Dlg", 24);

    Size sz = CalcSumatraVersionSize(hdc);
    {
        Rect& r = l.rcAppWithVer;
        r.x = rc.dx - sz.dx - 3;
        r.y = 0;
        r.SetSize(sz);
    }

    l.rcLine = {0, sz.dy, rc.dx, 0};

    // --- Pre-compute thumbnail grid x offset so header can align with it ---
    // use unfiltered count so layout stays stable when search filters results
    int nFilesForLayout = len(allFileStates);
    int colsForLayout =
        (rc.dx - kThumbsMarginLeft - kThumbsMarginRight + kThumbsSpaceBetweenX) / (kThumbnailDx + kThumbsSpaceBetweenX);
    int thumbsColsForLayout = std::max(colsForLayout, 1);
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

    // --- Step 1: layout header at the top ---
    l.himlOpen = (HIMAGELIST)SendMessageW(win->hwndToolbar, TB_GETIMAGELIST, 0, 0);
    Rect rcIconView(0, 0, 0, 0);
    ImageList_GetIconSize(l.himlOpen, &rcIconView.dx, &rcIconView.dy);

    Str txt = _TRA("Recently Opened");
    if (gGlobalPrefs->homePageSortByFrequentlyRead) {
        txt = _TRA("Frequently Read");
    }
    VirtWndText* hdr = new VirtWndText(hwnd, txt, hdrFont);
    l.freqRead = hdr;
    hdr->isRtl = isRtl;
    Size txtSize = hdr->GetIdealSize(true);

    int hdrY = DpiScale(hdc, 8);
    int iconGap = DpiScale(hdc, 4);
    int titleGap = DpiScale(hdc, 8);
    int viewIconsDx = 2 * rcIconView.dx + iconGap;
    Rect rcHdr(thumbsStartX + viewIconsDx + titleGap, hdrY, txtSize.dx, txtSize.dy);
    l.rcIconThumbnailView = {thumbsStartX, rcHdr.y + (rcHdr.dy - rcIconView.dy) / 2, rcIconView.dx, rcIconView.dy};
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
    win->staticLinks.Append(new StaticLink(l.rcIconListView, kLinkHomeListView, _TRA("Show as list")));
    win->staticLinks.Append(new StaticLink(l.rcIconThumbnailView, kLinkHomeThumbnailView, _TRA("Show as thumbnails")));

    /* "Open a document" link next to header */
    Rect rcIconOpen(0, 0, 0, 0);
    ImageList_GetIconSize(l.himlOpen, &rcIconOpen.dx, &rcIconOpen.dy);

    txt = _TRA("Open a document...");
    auto openDoc = new VirtWndText(hwnd, txt, fontText);
    openDoc->isRtl = isRtl;
    openDoc->withUnderline = true;
    txtSize = openDoc->GetIdealSize(true);

    int openDocSpacing = DpiScale(hdc, 16);
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

    rcOpenDoc = rcOpenDoc.Union(rcIconOpen);
    rcOpenDoc.Inflate(10, 10);
    l.openDoc = openDoc;
    auto sl = new StaticLink(rcOpenDoc, kLinkOpenFile);
    win->staticLinks.Append(sl);

    int headerBottomY = rcHdr.y + rcHdr.dy;

    // --- Position search edit below header ---
    EnsureHomeSearchCreated(win);
    int searchEditDy = DpiScale(hdc, kSearchEditDy);
    int headerSearchGap = DpiScale(hdc, kHeaderSearchGapY);
    int searchThumbsGap = DpiScale(hdc, kSearchThumbnailsGapY);
    {
        int borderDx = thumbsContentWidth * 3 / 4;
        if (borderDx < DpiScale(hdc, 200)) {
            borderDx = DpiScale(hdc, 200);
        }
        int borderX = thumbsStartX + (thumbsContentWidth - borderDx) / 2;
        int borderY = headerBottomY + headerSearchGap;
        int borderDy = searchEditDy + 2; // 1px border on each side
        l.rcSearchBorder = {borderX, borderY, borderDx, borderDy};
        // measure font height so we can vertically center the edit
        HFONT editFont = (HFONT)SendMessage(win->hwndHomeSearch, WM_GETFONT, 0, 0);
        TEXTMETRIC tm;
        HFONT oldFont = (HFONT)SelectObject(hdc, editFont);
        GetTextMetrics(hdc, &tm);
        SelectObject(hdc, oldFont);
        int fontDy = tm.tmHeight + tm.tmExternalLeading + 2; // +2 for caret padding
        int editDy = std::min(fontDy, searchEditDy);
        int editY = borderY + 1 + (searchEditDy - editDy) / 2;
        MoveWindow(win->hwndHomeSearch, borderX + 1, editY, borderDx - 2, editDy, TRUE);
    }
    // border is 1px top + 1px bottom = 2px
    int searchAreaDy = headerSearchGap + searchEditDy + 2 + searchThumbsGap;
    headerBottomY += searchAreaDy;

    // --- Step 2: calculate tip area at the bottom (before thumbnails) ---
    int tipHeight = 0;
    HFONT fontTip = CreateSimpleFont(hdc, "MS Shell Dlg", 16);
    ParsedTip* tip = nullptr;
    if (gGlobalPrefs->showTips && gSelectedTipIdx >= 0) {
        if (gSelectedIsPromo && gSelectedTipIdx < gParsedPromoCount) {
            tip = &gParsedPromosStorage[gSelectedTipIdx];
        } else if (!gSelectedIsPromo && gSelectedTipIdx < gParsedTipCount) {
            tip = &gParsedTipsStorage[gSelectedTipIdx];
        }
    }
    if (tip) {
        MeasureTipWords(*tip, hdc, fontTip);
        int tipPadding = DpiScale(hdc, 8);
        // do a preliminary layout to get the height (use thumbnails content width)
        LayoutTip(*tip, thumbsContentWidth, 0, 0);
        tipHeight = tip->totalDy + 2 * tipPadding;
    }

    // --- Step 3: middle area for thumbnails/list ---
    // content starts directly after headerBottomY (which includes kSearchThumbnailsGapY)
    int thumbsTopY = headerBottomY;
    int thumbsBottomY = rc.dy - tipHeight - kThumbsMiddleMargin;
    int thumbsVisibleDy = std::max(0, thumbsBottomY - thumbsTopY);

    l.rcThumbsArea = {0, thumbsTopY, rc.dx, thumbsVisibleDy};

    int nFiles = len(fileStates);
    bool showList = gGlobalPrefs->homePageShowList;
    int thumbsRows = 0;
    int thumbsContentDy = 0;
    if (showList) {
        thumbsRows = nFiles;
        thumbsContentDy = nFiles * kHomeListRowDy;
    } else {
        thumbsRows = (nFiles + thumbsColsForLayout - 1) / thumbsColsForLayout;
        if (thumbsRows > 0) {
            thumbsContentDy = thumbsRows * (kThumbnailDy + kThumbsSpaceBetweenY) - kThumbsSpaceBetweenY;
        }
    }

    int scrollY = win->homePageScrollY;
    int maxScrollY = std::max(0, thumbsContentDy - thumbsVisibleDy);
    if (scrollY > maxScrollY) {
        scrollY = maxScrollY;
        win->homePageScrollY = scrollY;
    }
    l.totalContentDy = thumbsContentDy;
    l.thumbsVisibleDy = thumbsVisibleDy;

    Point ptOff(thumbsStartX, thumbsTopY - scrollY);

    if (showList) {
        int listX = thumbsStartX;
        if (isRtl) {
            listX = rc.dx - thumbsStartX - thumbsContentWidth;
        }
        int listIconDx = l.rcIconListView.dx;
        int listIconGap = DpiScale(hdc, 6);
        int listSizeDx = DpiScale(hdc, 64);
        for (int row = 0; row < nFiles; row++) {
            ThumbnailLayout& thumb = *l.thumbnails.AppendBlanks(1);
            FileState* fs = fileStates[row];
            thumb.fs = fs;
            Rect rcRow(listX, ptOff.y + row * kHomeListRowDy, thumbsContentWidth, kHomeListRowDy);
            thumb.rcListRow = rcRow;
            Rect rcThumb(rcRow.x, rcRow.y + (rcRow.dy - kHomeListThumbDy) / 2, kHomeListThumbDx, kHomeListThumbDy);
            Rect rcPin(rcRow.x + rcRow.dx - listIconDx, rcRow.y + (rcRow.dy - listIconDx) / 2, listIconDx, listIconDx);
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
            if (rcFileName.dx < 0) {
                rcFileName.dx = 0;
            }
            thumb.rcListThumb = rcThumb;
            thumb.rcListPin = rcPin;
            thumb.rcListRemove = rcRemove;
            thumb.rcListSize = rcSize;
            thumb.rcListFileName = rcFileName;
            RenderedBitmap* thumbImg = LoadThumbnail(fs);
            if (thumbImg) {
                thumb.szThumb = thumbImg->GetSize();
            }
            Str path = fs->filePath;
            Rect slRect = rcRow.Intersect(l.rcThumbsArea);
            if (!slRect.IsEmpty()) {
                TempStr removeTarget = str::JoinTemp(kLinkHomeRemoveFilePrefix, path);
                TempStr pinTarget = str::JoinTemp(kLinkHomePinFilePrefix, path);
                Str pinTip = fs->isPinned ? _TRA("Unpin") : _TRA("Pin");
                win->staticLinks.Append(new StaticLink(rcRemove.Intersect(l.rcThumbsArea), removeTarget,
                                                       _TRA("Remove from Frequently Read")));
                win->staticLinks.Append(new StaticLink(rcPin.Intersect(l.rcThumbsArea), pinTarget, pinTip));
                thumb.sl = new StaticLink(slRect, path, HomeThumbTooltipTemp(path));
                win->staticLinks.Append(thumb.sl);
            }
        }
    } else {
        for (int row = 0; row < thumbsRows; row++) {
            for (int col = 0; col < thumbsColsForLayout; col++) {
                if (row * thumbsColsForLayout + col >= nFiles) {
                    // no more files to display
                    thumbsRows = col > 0 ? row + 1 : row;
                    break;
                }
                ThumbnailLayout& thumb = *l.thumbnails.AppendBlanks(1);
                FileState* fs = fileStates[row * thumbsColsForLayout + col];
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
                Str path = fs->filePath;
                Rect slRect = rcText.Union(rcPage).Intersect(l.rcThumbsArea);
                if (!slRect.IsEmpty()) {
                    thumb.sl = new StaticLink(slRect, path, HomeThumbTooltipTemp(path));
                    win->staticLinks.Append(thumb.sl);
                }
            }
        }
    }

    // layout tip at the bottom
    if (tip) {
        Rect rcClient = ClientRect(win->hwndCanvas);
        int tipPadding = DpiScale(hdc, 8);

        int tipY = rcClient.dy - tipHeight;
        // background spans full window width
        l.rcTip = {0, tipY, rcClient.dx, tipHeight};
        l.tip = tip;

        // text area aligned with thumbnails
        int tipStartX = thumbsStartX;
        int tipStartY = tipY + tipPadding;
        LayoutTip(*tip, thumbsContentWidth, tipStartX, tipStartY);

        // register tip links; per-link rects first so they take priority in hit testing
        for (auto& link : tip->links) {
            // compute bounding rect of all words in this link
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
        // tip background: clicking outside of links picks another tip
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
    WCHAR* filePathW = CWStrTemp(fs->filePath);
    fs->himl = (HIMAGELIST)SHGetFileInfoW(filePathW, 0, &sfi, sizeof(sfi), flags);
    fs->iconIdx = sfi.iIcon;
}

// --- Close (✕) button for Frequently Read thumbnails (issue #283, #5745) ---
//
// Drawn directly onto the home-page canvas (over the top-right corner of the
// thumbnail under the mouse) rather than as a separate top-level window. The
// separate window could be left behind, drawing stray crosses over a document
// (#5745). Styled like the tab close button (gray X on a white circle; red
// circle + white X on hover).
//
// To keep updates cheap, the glyph is painted/erased in just its own rect: the
// double-buffer (win->buffer) holds the page without the button, so erasing is
// a small BitBlt of that area back to the window. A full home-page repaint
// resets the button (it reappears on the next mouse move).

struct HomeCloseBtn {
    MainWindow* win = nullptr; // window the button currently belongs to
    Str filePath;              // file removed when the button is clicked
    Rect rc;                   // button rect (canvas client coords)
    Rect thumbRc;              // thumbnail rect (canvas client coords)
    bool isHover = false;
    bool visible = false;
};
static HomeCloseBtn gHomeCloseBtn;
// where the glyph is currently painted on the window, so we can erase exactly
// that area (empty when nothing is painted)
static Rect gHomeCloseBtnPaintedRc;

static void DrawHomeCloseGlyph(HDC hdc, const Rect& rc, bool isHover) {
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    u8 a = isHover ? 255 : 215;
    int w = rc.dx;
    int h = rc.dy;
    if (isHover) {
        COLORREF bg = kColCloseXHoverBg; // runtime var so GetXValue() isn't a constant cast
        Gdiplus::SolidBrush br(Gdiplus::Color(a, GetRValue(bg), GetGValue(bg), GetBValue(bg)));
        g.FillEllipse(&br, rc.x, rc.y, w - 1, h - 1);
    } else {
        // white circle so the gray X stays visible on any thumbnail background
        Gdiplus::SolidBrush br(Gdiplus::Color(a, 255, 255, 255));
        g.FillEllipse(&br, rc.x, rc.y, w - 1, h - 1);
    }
    COLORREF xcol = isHover ? kColCloseXHover : kColCloseX;
    Gdiplus::Pen pen(Gdiplus::Color(a, GetRValue(xcol), GetGValue(xcol), GetBValue(xcol)), 2.0f);
    int pad = w / 3;
    g.DrawLine(&pen, rc.x + pad, rc.y + pad, rc.x + w - pad, rc.y + h - pad);
    g.DrawLine(&pen, rc.x + w - pad, rc.y + pad, rc.x + pad, rc.y + h - pad);
}

// erase whatever glyph is currently on the window by blitting the button-free
// page back from the double-buffer
static void EraseHomeCloseGlyph(MainWindow* win) {
    Rect& pr = gHomeCloseBtnPaintedRc;
    if (pr.IsEmpty()) {
        return;
    }
    if (win && win->buffer) {
        HDC hdc = GetDC(win->hwndCanvas);
        HDC bufDC = win->buffer->GetDC();
        if (hdc && bufDC) {
            BitBlt(hdc, pr.x, pr.y, pr.dx, pr.dy, bufDC, pr.x, pr.y, SRCCOPY);
        }
        if (hdc) {
            ReleaseDC(win->hwndCanvas, hdc);
        }
    } else {
        // no buffer to restore from: fall back to invalidating the area
        if (win) {
            RECT r = ToRECT(pr);
            InvalidateRect(win->hwndCanvas, &r, FALSE);
        }
    }
    pr = {};
}

// re-paint the button: erase the previous glyph, then draw the current one
static void RepaintHomeCloseBtn(MainWindow* win) {
    EraseHomeCloseGlyph(win);
    HomeCloseBtn& b = gHomeCloseBtn;
    if (!b.visible || !win) {
        return;
    }
    HDC hdc = GetDC(win->hwndCanvas);
    if (hdc) {
        DrawHomeCloseGlyph(hdc, b.rc, b.isHover);
        ReleaseDC(win->hwndCanvas, hdc);
        gHomeCloseBtnPaintedRc = b.rc;
    }
}

// clear state without touching the window (used by a full home-page repaint,
// which redraws everything anyway)
static void ResetHomeCloseBtn() {
    HomeCloseBtn& b = gHomeCloseBtn;
    b.visible = false;
    b.isHover = false;
    str::Free(b.filePath);
    b.filePath = {};
    gHomeCloseBtnPaintedRc = {};
}

void HomePageHideCloseButton() {
    HomeCloseBtn& b = gHomeCloseBtn;
    if (!b.visible && gHomeCloseBtnPaintedRc.IsEmpty()) {
        return;
    }
    MainWindow* win = b.win;
    EraseHomeCloseGlyph(win);
    b.visible = false;
    b.isHover = false;
    str::Free(b.filePath);
    b.filePath = {};
}

// compute the button rect (canvas client coords) for a thumbnail link
static Rect HomeCloseBtnRectForThumb(MainWindow* win, const Rect& thumb) {
    int sz = DpiScale(win->hwndCanvas, 18);
    int margin = DpiScale(win->hwndCanvas, 5);
    int bx = IsUIRtl() ? (thumb.x + margin) : (thumb.x + thumb.dx - sz - margin);
    int by = thumb.y + margin;
    return Rect(bx, by, sz, sz);
}

void HomePageUpdateCloseButton(MainWindow* win, int x, int y) {
    if (!win || !CanAccessDisk() || gGlobalPrefs->homePageShowList) {
        HomePageHideCloseButton();
        return;
    }
    HomeCloseBtn& b = gHomeCloseBtn;
    Point pt(x, y);

    // already showing a button: update hover state as the mouse moves over /
    // off the glyph, but keep it while the mouse stays on the same thumbnail
    if (b.visible) {
        bool overBtn = b.rc.Contains(pt);
        if (overBtn != b.isHover) {
            b.isHover = overBtn;
            RepaintHomeCloseBtn(win);
        }
        if (b.thumbRc.Contains(pt)) {
            return;
        }
    }

    StaticLink* link = nullptr;
    TempStr target = GetStaticLinkAtTemp(win->staticLinks, x, y, &link);
    // a thumbnail link's target is an absolute file path; everything else (a
    // "<...>" command, a "Cmd..." tip link, a URL) is not, so it gets no button
    bool isThumb = len(target) > 0 && link && path::IsAbsolute(target);
    if (!isThumb) {
        HomePageHideCloseButton();
        return;
    }
    if (b.visible && b.filePath && str::Eq(b.filePath, target)) {
        return; // same thumbnail, nothing to do
    }
    b.win = win;
    b.thumbRc = link->rect;
    b.rc = HomeCloseBtnRectForThumb(win, link->rect);
    str::ReplaceWithCopy(&b.filePath, target);
    b.isHover = b.rc.Contains(pt);
    b.visible = true;
    RepaintHomeCloseBtn(win);
}

// called from a left-button click; if the click is on the close button, remove
// the file and return true so the caller doesn't also open the thumbnail
bool HomePageOnCloseButtonClick(MainWindow* win, int x, int y) {
    HomeCloseBtn& b = gHomeCloseBtn;
    if (!b.visible || !b.rc.Contains(Point(x, y))) {
        return false;
    }
    TempStr path = str::DupTemp(b.filePath);
    HomePageHideCloseButton();
    if (win && len(path) > 0) {
        ForgetFileFromFrequentlyRead(win, path);
    }
    return true;
}

void HomePageOnCanvasMouseLeave() {
    // the button is part of the canvas now, so leaving the canvas always hides it
    HomePageHideCloseButton();
}

static void DrawHomeViewButton(HDC hdc, HIMAGELIST himl, Rect r, TbIcon icon, bool selected) {
    if (selected) {
        FillRect(hdc, r, ThemeControlBackgroundColor());
        HBRUSH br = CreateSolidBrush(AccentColor(ThemeControlBackgroundColor(), 40));
        RECT rr = ToRECT(r);
        FrameRect(hdc, &rr, br);
        DeleteObject(br);
    }
    ImageList_Draw(himl, (int)icon, hdc, r.x, r.y, ILD_NORMAL);
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
    Rect r(dst.x + (dst.dx - dx) / 2, dst.y + (dst.dy - dy) / 2, dx, dy);
    return r;
}

static TempStr FileSizeForHomeListTemp(Str path) {
    i64 size = file::GetSize(path);
    if (size < 0) {
        return str::DupTemp("");
    }
    return str::FormatSizeShortTemp(size, nullptr);
}

static void DrawHomeListRow(HomePageLayout& l, const ThumbnailLayout& thumb, HFONT fontText, COLORREF backgroundColor,
                            bool isRtl) {
    HDC hdc = l.hdc;
    FileState* fs = thumb.fs;
    Rect row = thumb.rcListRow;
    if (row.Intersect(l.rcThumbsArea).IsEmpty()) {
        return;
    }

    COLORREF lineCol = AccentColor(ThemeMainWindowBackgroundColor(), 30);
    ScopedSelectObject pen(hdc, CreatePen(PS_SOLID, 1, lineCol), true);
    DrawLine(hdc, Rect(row.x, row.y + row.dy - 1, row.dx, 0));

    RenderedBitmap* thumbImg = LoadThumbnail(fs);
    Rect thumbBox = thumb.rcListThumb;
    if (thumbImg) {
        Rect thumbDst = FitRectInRect(thumbImg->GetSize(), thumbBox);
        thumbImg->Blit(hdc, thumbDst);
    }
    RoundRect(hdc, thumbBox.x, thumbBox.y, thumbBox.x + thumbBox.dx, thumbBox.y + thumbBox.dy, 4, 4);

    Str path = fs->filePath;
    TempStr fileName = path::GetBaseNameTemp(path);
    UINT nameFmt = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX | (isRtl ? DT_RIGHT : DT_LEFT);
    SelectObject(hdc, fontText);
    {
        RECT rcText = ToRECT(thumb.rcListFileName);
        DrawMaybeHighlightedText(hdc, rcText, fileName, l.filterWords, l.highlighted, backgroundColor, isRtl, false,
                                 nameFmt);
    }

    TempStr fileSize = FileSizeForHomeListTemp(path);
    SetTextColor(hdc, ThemeWindowTextColor());
    RECT rcSize = ToRECT(thumb.rcListSize);
    UINT sizeFmt = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX | (isRtl ? DT_LEFT : DT_RIGHT);
    DrawTextA(hdc, fileSize.s, -1, &rcSize, sizeFmt);

    ImageList_Draw(l.himlOpen, (int)TbIcon::Close, hdc, thumb.rcListRemove.x, thumb.rcListRemove.y, ILD_NORMAL);
    if (fs->isPinned) {
        FillRect(hdc, thumb.rcListPin, ThemeControlBackgroundColor());
    }
    ImageList_Draw(l.himlOpen, (int)TbIcon::Pin, hdc, thumb.rcListPin.x, thumb.rcListPin.y, ILD_NORMAL);
}

static void DrawHomePageLayout(HomePageLayout& l) {
    bool isRtl = IsUIRtl();
    auto hdc = l.hdc;
    auto win = l.win;
    auto textColor = ThemeWindowTextColor();
    auto backgroundColor = ThemeMainWindowBackgroundColor();

    {
        Rect rc = ClientRect(win->hwndCanvas);
        auto color = ThemeMainWindowBackgroundColor();
        FillRect(hdc, rc, color);
    }

    // draw search edit border and background on the canvas
    {
        COLORREF bgCol = ThemeControlBackgroundColor();
        const Rect& sb = l.rcSearchBorder;
        RECT rcBorder = {sb.x, sb.y, sb.x + sb.dx, sb.y + sb.dy};
        // fill interior with control background so padding matches the edit
        HBRUSH brBg = CreateSolidBrush(bgCol);
        FillRect(hdc, &rcBorder, brBg);
        DeleteObject(brBg);
        // draw border frame
        COLORREF borderCol = AccentColor(bgCol, 40);
        HBRUSH brBorder = CreateSolidBrush(borderCol);
        FrameRect(hdc, &rcBorder, brBorder);
        DeleteObject(brBorder);
    }

    if (false) {
        const Rect& r = l.rcAppWithVer;
        DrawSumatraVersion(hdc, r);
    }

    auto color = ThemeWindowTextColor();
    if (false) {
        ScopedSelectObject pen(hdc, CreatePen(PS_SOLID, 1, color), true);
        DrawLine(hdc, l.rcLine);
    }
    HFONT fontText = CreateSimpleFont(hdc, "MS Shell Dlg", 14);

    AutoDeletePen penThumbBorder(CreatePen(PS_SOLID, kThumbsBorderDx, color));
    color = ThemeWindowLinkColor();
    AutoDeletePen penLinkLine(CreatePen(PS_SOLID, 1, color));

    SelectObject(hdc, penThumbBorder);
    SetBkMode(hdc, TRANSPARENT);
    color = ThemeWindowTextColor();
    SetTextColor(hdc, color);

    DrawHomeViewButton(hdc, l.himlOpen, l.rcIconThumbnailView, TbIcon::HomeThumbnails, !gGlobalPrefs->homePageShowList);
    DrawHomeViewButton(hdc, l.himlOpen, l.rcIconListView, TbIcon::HomeList, gGlobalPrefs->homePageShowList);
    l.freqRead->Paint(hdc);
    SelectObject(hdc, GetStockBrush(NULL_BRUSH));

    // clip thumbnails to the middle area
    {
        const Rect& ta = l.rcThumbsArea;
        HRGN thumbsClip = CreateRectRgn(ta.x, ta.y, ta.x + ta.dx, ta.y + ta.dy);
        SelectClipRgn(hdc, thumbsClip);
        DeleteObject(thumbsClip);
    }

    for (const ThumbnailLayout& thumb : l.thumbnails) {
        FileState* fs = thumb.fs;
        if (gGlobalPrefs->homePageShowList) {
            DrawHomeListRow(l, thumb, fontText, backgroundColor, isRtl);
            continue;
        }
        const Rect& page = thumb.rcPage;

        RenderedBitmap* thumbImg = LoadThumbnail(fs);
        if (thumbImg) {
            int savedDC = SaveDC(hdc);
            HRGN clip = CreateRoundRectRgn(page.x, page.y, page.x + page.dx, page.y + page.dy, 10, 10);
            ExtSelectClipRgn(hdc, clip, RGN_AND);
            // note: we used to invert bitmaps in dark theme but that doesn't
            // make sense for thumbnails
            thumbImg->Blit(hdc, page);
            RestoreDC(hdc, savedDC);
            DeleteObject(clip);
        }
        RoundRect(hdc, page.x, page.y, page.x + page.dx, page.y + page.dy, 10, 10);

        const Rect& rect = thumb.rcText;
        Str path = fs->filePath;
        TempStr fileName = path::GetBaseNameTemp(path);
        UINT fmt = DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX | (isRtl ? DT_RIGHT : DT_LEFT);

        SelectObject(hdc, fontText);
        {
            RECT rcText = {rect.x, rect.y, rect.x + rect.dx, rect.y + rect.dy};
            DrawMaybeHighlightedText(hdc, rcText, fileName, l.filterWords, l.highlighted, backgroundColor, isRtl, false,
                                     fmt);
        }

        GetFileStateIcon(fs);
        int x = isRtl ? page.x + page.dx - DpiScale(hdc, 16) : page.x;
        ImageList_Draw(fs->himl, fs->iconIdx, hdc, x, rect.y, ILD_TRANSPARENT);
    }

    // restore full clip region
    SelectClipRgn(hdc, nullptr);

    color = ThemeWindowLinkColor();
    SetTextColor(hdc, color);
    SelectObject(hdc, penLinkLine);

    int x = l.rcIconOpen.x;
    int y = l.rcIconOpen.y;
    int openIconIdx = 0;
    ImageList_Draw(l.himlOpen, openIconIdx, hdc, x, y, ILD_NORMAL);

    l.openDoc->Paint(hdc);

    if (false) {
        Rect rcFreqRead = DrawHideFrequentlyReadLink(win->hwndCanvas, hdc, _TRA("Hide frequently read"));
        auto sl = new StaticLink(rcFreqRead, kLinkHideList);
        win->staticLinks.Append(sl);
    }

    // draw tip at the bottom
    if (l.tip) {
        COLORREF tipBgCol = ThemeControlBackgroundColor();
        FillRect(hdc, l.rcTip, tipBgCol);

        HFONT fontTip = CreateSimpleFont(hdc, "MS Shell Dlg", 16);
        COLORREF textCol = ThemeWindowTextColor();
        COLORREF linkCol = ThemeWindowLinkColor();
        DrawTipWords(hdc, *l.tip, fontTip, textCol, linkCol);
    }
}

void DrawHomePage(MainWindow* win, HDC hdc) {
    HWND hwnd = win->hwndFrame;
    // any home-page repaint (scroll, resize, filter) invalidates thumbnail
    // positions and rewrites the buffer, so drop the close button without
    // touching the window; it reappears on the next hover
    ResetHomeCloseBtn();
    DeleteVecMembers(win->staticLinks);

    HomePageLayout l;
    l.rc = ClientRect(win->hwndCanvas);
    l.hdc = hdc;
    l.hwnd = hwnd;
    l.win = win;
    LayoutHomePage(l);

    DrawHomePageLayout(l);

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

void HomePageOnVScroll(MainWindow* win, WPARAM wp) {
    USHORT msg = LOWORD(wp);
    HDC hdc = GetDC(win->hwndCanvas);
    int lineDy = gGlobalPrefs->homePageShowList ? kHomeListRowDy : kThumbnailDy + kThumbsSpaceBetweenY;
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
    int thumbsRowDy = gGlobalPrefs->homePageShowList ? kHomeListRowDy : kThumbnailDy + kThumbsSpaceBetweenY;
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
