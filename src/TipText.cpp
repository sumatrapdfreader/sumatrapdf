/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// A small markup with links, keyboard shortcuts and bold runs, parsed into a
// VirtRichText: a virtual control that wraps, paints and hit-tests itself.
// Shared by SumatraPDF's home page and notifications, and by other apps in the
// family. What it needs from the app is a CommandsContext (see TipText.h) and a
// way to open a url.

#include "base/Base.h"
#include "base/Dpi.h"
#include "base/Win.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/PlatformFont.h"
#include "wingui/Gfx.h"
#include "wingui/VirtWnd.h"

#include "TipText.h"

void (*gTipOpenUrl)(Str url) = nullptr;
CommandsContext* gCommandsContext = nullptr;

static Kind kindVirtRichText = "virtRichText";

VirtRichText::VirtRichText() {
    kind = kindVirtRichText;
}

VirtRichText::~VirtRichText() {
    Reset();
}

void VirtRichText::Reset() {
    TipWord* w = words.next;
    while (w) {
        TipWord* next = w->next;
        str::Free(w->text);
        delete w;
        w = next;
    }
    TipLink* l = links.next;
    while (l) {
        TipLink* next = l->next;
        str::Free(l->cmd);
        delete l;
        l = next;
    }
    words.next = nullptr;
    links.next = nullptr;
    lastWord = nullptr;
    lastLink = nullptr;
    totalDx = 0;
    totalDy = 0;
    layoutDx = -1;
}

int TipWordCount(VirtRichText* tip) {
    int n = 0;
    for (TipWord* w = tip->words.next; w; w = w->next) {
        n++;
    }
    return n;
}

int TipLinkCount(VirtRichText* tip) {
    int n = 0;
    for (TipLink* l = tip->links.next; l; l = l->next) {
        n++;
    }
    return n;
}

// appends at the end, so words and links stay in source order
static TipWord* AppendTipWord(VirtRichText& tip, Str text) {
    auto* w = new TipWord();
    str::ReplaceWithCopy(&w->text, text);
    if (tip.lastWord) {
        tip.lastWord->next = w;
    } else {
        tip.words.next = w;
    }
    tip.lastWord = w;
    return w;
}

static TipLink* AppendTipLink(VirtRichText& tip, Str cmd) {
    auto* l = new TipLink();
    str::ReplaceWithCopy(&l->cmd, cmd);
    if (tip.lastLink) {
        tip.lastLink->next = l;
    } else {
        tip.links.next = l;
    }
    tip.lastLink = l;
    return l;
}

// drops the link appended last; the parser adds it before it knows whether the
// link text produced any words
static void RemoveLastTipLink(VirtRichText& tip) {
    TipLink* link = tip.lastLink;
    if (!link) {
        return;
    }
    TipLink* prev = nullptr;
    for (TipLink* l = tip.links.next; l && l != link; l = l->next) {
        prev = l;
    }
    if (prev) {
        prev->next = nullptr;
    } else {
        tip.links.next = nullptr;
    }
    tip.lastLink = prev;
    str::Free(link->cmd);
    delete link;
}

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
    if (str::StartsWith(cmdStart, StrL("http://")) || str::StartsWith(cmdStart, StrL("https://"))) {
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

static void AppendTipWordsFromText(VirtRichText& tip, Str text, bool isLink, TipLink* link, bool isBold = false) {
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
        TipWord* w = AppendTipWord(tip, Str(text.s + wordStart, i - wordStart));
        w->isLink = isLink;
        w->isBold = isBold;
        w->link = link;
    }
}

// the first word emitted after `prev` (the list tail before the token was
// parsed), or null when the token emitted nothing
static TipWord* FirstWordAfter(VirtRichText& tip, TipWord* prev) {
    return prev ? prev->next : tip.words.next;
}

// mark the first word emitted for a token as having no space before it, so the
// layout draws it flush against the preceding word (e.g. "**foo**:" -> "foo:")
static void SetNoSpaceBefore(VirtRichText& tip, TipWord* prev, bool noSpace) {
    TipWord* first = FirstWordAfter(tip, prev);
    if (noSpace && first) {
        first->noSpaceBefore = true;
    }
}

// index of the ')' that matches s[0]=='(', or -1
static int MatchingCloseParen(Str s) {
    if (len(s) == 0 || s.s[0] != '(') {
        return -1;
    }
    int depth = 0;
    for (int i = 0; i < s.len; i++) {
        char c = s.s[i];
        if (c == '(') {
            depth++;
        } else if (c == ')') {
            depth--;
            if (depth == 0) {
                return i;
            }
        }
    }
    return -1;
}

// true if s starts with "(prefix/" (prefix e.g. "Key" or "Kbd")
static bool StartsWithParenPrefix(Str s, Str prefix) {
    // "(" + prefix + "/"
    if (len(s) < 2 + len(prefix) + 1) {
        return false;
    }
    if (s.s[0] != '(') {
        return false;
    }
    if (!str::StartsWith(Str(s.s + 1, s.len - 1), prefix)) {
        return false;
    }
    return s.s[1 + len(prefix)] == '/';
}

// emit (Kbd/...) content as one or more key-cap words (", "-separated, like
// the keyboard help sheet when a command has multiple bindings)
static void AppendKbdWords(VirtRichText& tip, Str content, bool noSpace) {
    // trim leading/trailing whitespace on the whole content
    while (len(content) > 0 && IsTipWhitespace(content.s[0])) {
        content.s++;
        content.len--;
    }
    while (len(content) > 0 && IsTipWhitespace(content.s[content.len - 1])) {
        content.len--;
    }
    if (len(content) == 0) {
        return;
    }
    StrVec toks;
    Split(&toks, content, StrL(", "));
    int n = len(toks);
    bool any = false;
    for (int i = 0; i < n; i++) {
        Str t = toks.At(i);
        while (len(t) > 0 && IsTipWhitespace(t.s[0])) {
            t.s++;
            t.len--;
        }
        while (len(t) > 0 && IsTipWhitespace(t.s[t.len - 1])) {
            t.len--;
        }
        if (len(t) == 0) {
            continue;
        }
        TipWord* w = AppendTipWord(tip, t);
        w->isKbd = true;
        if (!any) {
            w->noSpaceBefore = noSpace;
            any = true;
        }
    }
    if (!any) {
        TipWord* w = AppendTipWord(tip, content);
        w->isKbd = true;
        w->noSpaceBefore = noSpace;
    }
}

// resolve a link command to the target a tip link stores
static TempStr ResolveLinkCmdTemp(Str cmd) {
    if (str::StartsWith(cmd, StrL("https://")) || str::StartsWith(cmd, StrL("http://"))) {
        return str::DupTemp(cmd);
    }
    if (str::TrimPrefix(cmd, StrL("Help/"))) {
        // cmd is a non-NUL-terminated view into the tip line, so %s must get a
        // zero-terminated copy of exactly the remainder -- otherwise it reads
        // past the link, pulling in trailing chars like ")."
        return fmt("https://www.sumatrapdfreader.org/docs/%s", cmd);
    }
    // Cmd* - use as-is, will be resolved to command ID on click
    return str::DupTemp(cmd);
}

// Text from outside the app -- a clipboard string, a file name -- can contain
// anything the markup uses, so this adds it as plain words with nothing
// interpreted.
// adds text with no markup interpreted, for strings from outside the app
void VirtRichText::AddPlainText(Str text) {
    AppendTipWordsFromText(*this, text, false, nullptr);
    layoutDx = -1;
}

// The app decides what a command name means; without a context (Key/...) is
// left as literal text and command links do nothing.
static TempStr CommandShortcutTemp(Str cmdName) {
    if (!gCommandsContext) {
        return {};
    }
    return gCommandsContext->GetCommandShortcutTemp(cmdName);
}

static void ParseTipInto(VirtRichText& tip, Str s) {
    if (!s) {
        return;
    }
    str::Builder expanded;
    Str sp = s;
    // first pass: expand (Key/CmdXxx) to shortcut strings (only for real commands).
    // Uses balanced parens so nesting works: (Kbd/(Key/CmdFoo)) → (Kbd/Ctrl + …)
    while (len(sp) > 0) {
        if (StartsWithParenPrefix(sp, StrL("Key"))) {
            int end = MatchingCloseParen(sp);
            if (end > 5) {
                Str cmdName(sp.s + 5, end - 5); // skip "(Key/"
                TempStr shortcut = CommandShortcutTemp(cmdName);
                if (shortcut.s) {
                    expanded.Append(shortcut);
                    AdvanceTipText(sp, end + 1);
                    continue;
                }
            }
        }
        expanded.AppendChar(sp.s[0]);
        AdvanceTipText(sp);
    }

    // second pass: split into words, detecting [text](link), (Kbd/...), **bold**
    Str p = ToStr(expanded);
    while (len(p) > 0) {
        const char* beforeWs = p.s;
        SkipTipWhitespace(p);
        if (len(p) == 0) {
            break;
        }
        // this token abuts the previous with no whitespace between them (e.g. the
        // ':' right after "**foo**"), so it draws with no space before it
        TipWord* prevWord = tip.lastWord;
        bool noSpace = (p.s == beforeWs) && (prevWord != nullptr);

        // (Kbd/shortcut text) — key-cap(s); content already has (Key/...) expanded
        if (StartsWithParenPrefix(p, StrL("Kbd"))) {
            int end = MatchingCloseParen(p);
            if (end > 5) {
                Str content(p.s + 5, end - 5); // skip "(Kbd/"
                AppendKbdWords(tip, content, noSpace);
                AdvanceTipText(p, end + 1);
                continue;
            }
        }

        // **bold text**
        if (p.len >= 4 && p.s[0] == '*' && p.s[1] == '*') {
            Str after(p.s + 2, p.len - 2);
            int end = str::IndexOf(after, StrL("**"));
            if (end >= 0) {
                Str boldText(after.s, end);
                AppendTipWordsFromText(tip, boldText, false, nullptr, true);
                SetNoSpaceBefore(tip, prevWord, noSpace);
                AdvanceTipText(p, 2 + end + 2);
                continue;
            }
        }

        // a standalone '*' (not the '**' that starts bold) is a bullet used to
        // separate items visually; render it as a middle dot
        if (p.s[0] == '*' && (p.len == 1 || IsTipWhitespace(p.s[1]))) {
            TipWord* w = AppendTipWord(tip, StrL("\xc2\xb7")); // U+00B7 MIDDLE DOT
            w->noSpaceBefore = noSpace;
            AdvanceTipText(p, 1);
            continue;
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

                        TipLink* link = AppendTipLink(tip, ResolveLinkCmdTemp(linkCmd));
                        AppendTipWordsFromText(tip, linkText, true, link);
                        link->firstWord = FirstWordAfter(tip, prevWord);
                        link->lastWord = tip.lastWord;

                        if (link->firstWord) {
                            SetNoSpaceBefore(tip, prevWord, noSpace);
                            AdvanceTipText(p, (int)(cmdEnd.s - p.s) + 1);
                            continue;
                        }
                        // the link text was empty, so it produced no words
                        RemoveLastTipLink(tip);
                    } else {
                        // empty [text]: treat the whole markup as literal text
                        TipWord* w = AppendTipWord(tip, Str(p.s, (int)(cmdEnd.s - p.s) + 1));
                        w->noSpaceBefore = noSpace;
                        AdvanceTipText(p, (int)(cmdEnd.s - p.s) + 1);
                        continue;
                    }
                }
            }
            // not a valid [text](link) — fall through (e.g. "[CIW]" in a filename)
        }

        // regular word; stop at '[', '**', or '(Kbd/' so those stay separate tokens
        int wordStart = 0;
        int i = 0;
        while (i < p.len && !IsTipWhitespace(p.s[i])) {
            if (p.s[i] == '*' && i + 1 < p.len && p.s[i + 1] == '*') {
                break; // start of **bold**
            }
            if (p.s[i] == '(' && StartsWithParenPrefix(Str(p.s + i, p.len - i), StrL("Kbd"))) {
                break;
            }
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
            TipWord* w = AppendTipWord(tip, Str(p.s + wordStart, i - wordStart));
            w->noSpaceBefore = noSpace;
        }
        if (i < p.len) {
            AdvanceTipText(p, i);
        } else {
            break;
        }
    }
}

// measures every word and wraps them into areaWidth. Positions are relative to
// the control's content origin, so they survive the control being moved
void VirtRichText::LayoutText(int areaWidth) {
    if (areaWidth == layoutDx) {
        return;
    }
    layoutDx = areaWidth;
    HWND hwnd = GetHwnd();
    PlatformFont* boldFont = nullptr;
    int kbdPadX = DpiScale(hwnd, 7);
    int kbdPadY = DpiScale(hwnd, 5);
    for (TipWord* w = words.next; w; w = w->next) {
        if (w->isBold && !boldFont) {
            boldFont = GetBoldPlatformFont(font);
        }
        PlatformFont* use = (w->isBold && boldFont) ? boldFont : font;
        Size sz = PlatformFontMeasureText(use, w->text);
        if (w->isKbd) {
            // key-cap padding matches KeyboardHelp's key caps
            w->dx = sz.dx + (2 * kbdPadX);
            w->dy = sz.dy + kbdPadY;
        } else {
            w->dx = sz.dx;
            w->dy = sz.dy;
        }
    }

    int startX = 0;
    int startY = 0;
    int x = startX;
    int y = startY;
    int lineHeight = 0;
    int spaceWidth = 4; // approximate space between words
    int maxX = startX;
    for (TipWord* w = words.next; w; w = w->next) {
        // space goes before the word, so words abutting the previous token
        // (noSpaceBefore, e.g. the ':' in "**foo**:") draw flush against it
        int space = (x > startX && !w->noSpaceBefore) ? spaceWidth : 0;
        if (x > startX && x + space + w->dx > startX + areaWidth) {
            // wrap to next line
            x = startX;
            y += lineHeight + 2;
            lineHeight = 0;
            space = 0;
        }
        x += space;
        w->x = x;
        w->y = y;
        x += w->dx;
        maxX = std::max(x, maxX);
        lineHeight = std::max(w->dy, lineHeight);
    }
    totalDx = maxX - startX;
    totalDy = (y - startY) + lineHeight;
}

int VirtRichText::MinIntrinsicWidth(int) {
    LayoutText(1 << 20);
    return totalDx;
}

int VirtRichText::MinIntrinsicHeight(int width) {
    LayoutText(width > 0 ? width : (1 << 20));
    return totalDy;
}

Size VirtRichText::GetIdealSize() {
    LayoutText(layoutDx > 0 ? layoutDx : (1 << 20));
    return {totalDx, totalDy};
}

Size VirtRichText::Layout(Constraints bc) {
    int dx = (bc.max.dx == Inf) ? (1 << 20) : bc.max.dx;
    LayoutText(dx);
    return bc.Constrain({totalDx, totalDy});
}

void VirtRichText::SetBounds(Rect r) {
    VirtWnd::SetBounds(r);
    Rect content = r;
    content.SubTB(padding.top, padding.bottom);
    content.SubLR(padding.left, padding.right);
    LayoutText(content.dx);
}

// draws the words (link words in linkColor, underlined; others in textColor;
// isKbd words as key-caps like the keyboard help sheet)
void VirtRichText::Paint(VirtWndPaintCtx& ctx) {
    HDC hdc = GfxHdc(ctx.gfx);
    uint fmt = DT_LEFT | DT_NOCLIP | DT_NOPREFIX | DT_SINGLELINE;
    PlatformFont* boldFont = nullptr;
    COLORREF textCol = textColor;
    COLORREF linkCol = (linkColor == kColorUnset) ? textCol : linkColor;
    COLORREF bgCol = bgColor;
    // key-cap colors: AccentColor on the background the text sits on
    if (bgCol == kColorUnset) {
        bgCol = IsLightColor(textCol) ? MkGray(0x22) : MkGray(0xf2);
    }
    COLORREF capBg = AccentColor(bgCol, 16);
    COLORREF capBorder = AccentColor(bgCol, 40);
    int rad = DpiScale(GetHwnd(), 5);
    // words are laid out at (0, 0); shift them to where we are
    int offX = ctx.content.x;
    int offY = ctx.content.y;

    for (TipWord* w = words.next; w; w = w->next) {
        if (w->isKbd) {
            HPEN pen = CreatePen(PS_SOLID, 1, capBorder);
            HBRUSH br = CreateSolidBrush(capBg);
            HGDIOBJ oldPen = SelectObject(hdc, pen);
            HGDIOBJ oldBr = SelectObject(hdc, br);
            RoundRect(hdc, offX + w->x, offY + w->y, offX + w->x + w->dx, offY + w->y + w->dy, rad, rad);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBr);
            DeleteObject(pen);
            DeleteObject(br);
            SetTextColor(hdc, textCol);
            Rect capRc{offX + w->x, offY + w->y, w->dx, w->dy};
            HdcDrawText(hdc, w->text, capRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX,
                        font ? font->GetHFont() : nullptr);
            continue;
        }
        if (w->isBold && !boldFont) {
            boldFont = GetBoldPlatformFont(font);
        }
        Point pt = {offX + w->x, offY + w->y};
        SetTextColor(hdc, w->isLink ? linkCol : textCol);
        PlatformFont* use = (w->isBold && boldFont) ? boldFont : font;
        HdcDrawText(hdc, w->text, pt, fmt, use ? use->GetHFont() : nullptr);
    }
    // underline each link
    HPEN pen = CreatePen(PS_SOLID, 1, linkCol);
    HGDIOBJ prevPen = SelectObject(hdc, pen);
    for (TipLink* link = links.next; link; link = link->next) {
        TipWord* first = link->firstWord;
        TipWord* last = link->lastWord;
        if (!first || !last) {
            continue;
        }
        int underlineY = offY + first->y + first->dy - 3;
        int x1 = offX + first->x;
        int x2 = offX + last->x + last->dx;
        HdcDrawLine(hdc, Rect(x1, underlineY, x2 - x1, 0));
    }
    SelectObject(hdc, prevPen);
    DeleteObject(pen);
}

// the link under a point in our own coordinates, or null
TipLink* VirtRichText::LinkAt(Point ptLocal) {
    int x = ptLocal.x - padding.left;
    int y = ptLocal.y - padding.top;
    for (TipWord* w = words.next; w; w = w->next) {
        if (!w->isLink) {
            continue;
        }
        Rect wr = {w->x, w->y, w->dx, w->dy};
        if (wr.Contains(Point(x, y))) {
            return w->link;
        }
    }
    return nullptr;
}

// a click on a link runs it; anything else bubbles up to whoever hosts us
bool VirtRichText::OnMouseDown(VirtWndMouseEvent& ev) {
    return LinkAt(ev.pt) != nullptr;
}

bool VirtRichText::OnMouseUp(VirtWndMouseEvent& ev) {
    TipLink* link = LinkAt(ev.pt);
    if (!link) {
        return false;
    }
    HWND hwnd = hwndForCmds ? hwndForCmds : GetHwnd();
    ExecuteTipLink(hwnd, link->cmd);
    return true;
}

bool VirtRichText::OnSetCursor(Point ptLocal) {
    if (!LinkAt(ptLocal)) {
        return false;
    }
    SetCursorCached(IDC_HAND);
    return true;
}

TempStr VirtRichText::GetTooltipTemp(Point ptLocal) {
    TipLink* link = LinkAt(ptLocal);
    if (!link) {
        return nullptr;
    }
    return str::DupTemp(link->cmd);
}

// runs a link target: "Cmd..." sends the command to hwnd, a url goes to gTipOpenUrl.
// Cmd targets may include arguments (e.g. "CmdFixDefaultApp .pdf"); those go through
// CreateCommandFromDefinition so FrameOnCommand sees a CustomCommand with args.
void ExecuteTipLink(HWND hwnd, Str cmd) {
    if (len(cmd) == 0) {
        return;
    }
    if (str::StartsWith(cmd, StrL("Cmd"))) {
        if (gCommandsContext) {
            gCommandsContext->ExecuteCommand(hwnd, cmd);
        }
        return;
    }
    if (str::StartsWith(cmd, StrL("http://")) || str::StartsWith(cmd, StrL("https://"))) {
        if (gTipOpenUrl) {
            gTipOpenUrl(cmd);
        }
    }
}

bool VirtRichText::HasRichContent() {
    if (links.next) {
        return true;
    }
    for (TipWord* w = words.next; w; w = w->next) {
        if (w->isBold || w->isKbd) {
            return true;
        }
    }
    return false;
}

// reconstructs the plain (link markup removed, Key/ expanded) text
TempStr VirtRichText::PlainTextTemp() {
    str::Builder sb;
    for (TipWord* w = words.next; w; w = w->next) {
        if (w != words.next) {
            sb.AppendChar(' ');
        }
        sb.Append(w->text);
    }
    return ToStrTemp(sb);
}

VirtRichText* ParseTip(Str s) {
    auto* tip = new VirtRichText();
    ParseTipInto(*tip, s);
    return tip;
}
