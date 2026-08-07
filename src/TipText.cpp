/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Parsing, laying out and drawing "tip" text: a small markup with links,
// keyboard shortcuts and bold runs. Shared by SumatraPDF's home page and
// notifications, and by other apps in the family. What it needs from the app
// is its command table (see TipText.h) and a way to open a url.

#include "base/Base.h"
#include "base/Win.h"

#include "Commands.h"
#include "TipText.h"

void (*gTipOpenUrl)(Str url) = nullptr;

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

static void AppendTipWordsFromText(ParsedTip& tip, Str text, bool isLink, int linkIdx, bool isBold = false) {
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
        w.isBold = isBold;
        w.linkIdx = linkIdx;
        tip.words.Append(w);
    }
}

// mark the first word emitted for a token as having no space before it, so the
// layout draws it flush against the preceding word (e.g. "**foo**:" -> "foo:")
static void SetNoSpaceBefore(ParsedTip& tip, int firstWordIdx, bool noSpace) {
    if (noSpace && firstWordIdx >= 0 && firstWordIdx < len(tip.words)) {
        tip.words[firstWordIdx].noSpaceBefore = true;
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
void AddTipPlainText(ParsedTip& tip, Str text) {
    AppendTipWordsFromText(tip, text, false, -1);
}

void ParseTip(ParsedTip& tip, Str s) {
    if (!s) {
        return;
    }
    str::Builder expanded;
    Str sp = s;
    // first pass: expand (Key/CmdXxx) to shortcut strings (only for real commands)
    while (len(sp) > 0) {
        if (sp.s[0] == '(' && sp.len > 5 && str::StartsWith(Str(sp.s + 1, sp.len - 1), StrL("Key/"))) {
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
        const char* beforeWs = p.s;
        SkipTipWhitespace(p);
        if (len(p) == 0) {
            break;
        }
        // this token abuts the previous with no whitespace between them (e.g. the
        // ':' right after "**foo**"), so it draws with no space before it
        int firstWordIdx = len(tip.words);
        bool noSpace = (p.s == beforeWs) && firstWordIdx > 0;

        // **bold text**
        if (p.len >= 4 && p.s[0] == '*' && p.s[1] == '*') {
            Str after(p.s + 2, p.len - 2);
            int end = str::IndexOf(after, StrL("**"));
            if (end >= 0) {
                Str boldText(after.s, end);
                AppendTipWordsFromText(tip, boldText, false, -1, true);
                SetNoSpaceBefore(tip, firstWordIdx, noSpace);
                AdvanceTipText(p, 2 + end + 2);
                continue;
            }
        }

        // a standalone '*' (not the '**' that starts bold) is a bullet used to
        // separate items visually; render it as a middle dot
        if (p.s[0] == '*' && (p.len == 1 || IsTipWhitespace(p.s[1]))) {
            TipWord w;
            str::ReplaceWithCopy(&w.text, StrL("\xc2\xb7")); // U+00B7 MIDDLE DOT
            w.noSpaceBefore = noSpace;
            tip.words.Append(w);
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

                        TipLink link;
                        str::ReplaceWithCopy(&link.cmd, ResolveLinkCmdTemp(linkCmd));
                        link.firstWord = len(tip.words);
                        AppendTipWordsFromText(tip, linkText, true, len(tip.links));

                        if (link.firstWord < len(tip.words)) {
                            link.lastWord = len(tip.words) - 1;
                            tip.links.Append(link);
                            SetNoSpaceBefore(tip, firstWordIdx, noSpace);
                            AdvanceTipText(p, (int)(cmdEnd.s - p.s) + 1);
                            continue;
                        }
                        str::Free(link.cmd);
                    } else {
                        // empty [text]: treat the whole markup as literal text
                        TipWord w;
                        str::ReplaceWithCopy(&w.text, Str(p.s, (int)(cmdEnd.s - p.s) + 1));
                        w.noSpaceBefore = noSpace;
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
            if (p.s[i] == '*' && i + 1 < p.len && p.s[i + 1] == '*') {
                break; // start of **bold**
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
            TipWord w;
            str::ReplaceWithCopy(&w.text, Str(p.s + wordStart, i - wordStart));
            w.noSpaceBefore = noSpace;
            tip.words.Append(w);
        }
        if (i < p.len) {
            AdvanceTipText(p, i);
        } else {
            break;
        }
    }
}

static HFONT CreateBoldFontFrom(HFONT font) {
    if (!font) {
        return nullptr;
    }
    LOGFONTW lf{};
    if (GetObjectW(font, sizeof(lf), &lf) == 0) {
        return nullptr;
    }
    lf.lfWeight = FW_BOLD;
    return CreateFontIndirectW(&lf);
}

void MeasureTipWords(ParsedTip& tip, HDC hdc, HFONT font) {
    uint fmt = DT_LEFT | DT_NOCLIP | DT_NOPREFIX | DT_SINGLELINE;
    HFONT boldFont = nullptr;
    for (auto& w : tip.words) {
        if (w.isBold && !boldFont) {
            boldFont = CreateBoldFontFrom(font);
        }
        HFONT use = (w.isBold && boldFont) ? boldFont : font;
        Size sz = HdcMeasureText(hdc, w.text, fmt, use);
        w.dx = sz.dx;
        w.dy = sz.dy;
    }
    if (boldFont) {
        DeleteObject(boldFont);
    }
}

// lays out words within areaWidth (wrapping); sets per-word x/y and tip.totalDx/totalDy
void LayoutTip(ParsedTip& tip, int areaWidth, int startX, int startY) {
    int x = startX;
    int y = startY;
    int lineHeight = 0;
    int spaceWidth = 4; // approximate space between words
    int maxX = startX;
    for (auto& w : tip.words) {
        // space goes before the word, so words abutting the previous token
        // (w.noSpaceBefore, e.g. the ':' in "**foo**:") draw flush against it
        int space = (x > startX && !w.noSpaceBefore) ? spaceWidth : 0;
        if (x > startX && x + space + w.dx > startX + areaWidth) {
            // wrap to next line
            x = startX;
            y += lineHeight + 2;
            lineHeight = 0;
            space = 0;
        }
        x += space;
        w.x = x;
        w.y = y;
        x += w.dx;
        maxX = std::max(x, maxX);
        lineHeight = std::max(w.dy, lineHeight);
    }
    tip.totalDx = maxX - startX;
    tip.totalDy = (y - startY) + lineHeight;
}

// draws the words (link words in linkCol, underlined; others in textCol)
void DrawTipWords(HDC hdc, ParsedTip& tip, HFONT font, COLORREF textCol, COLORREF linkCol) {
    uint fmt = DT_LEFT | DT_NOCLIP | DT_NOPREFIX | DT_SINGLELINE;
    HFONT boldFont = nullptr;
    for (auto& w : tip.words) {
        if (w.isBold && !boldFont) {
            boldFont = CreateBoldFontFrom(font);
        }
        Point pt = {w.x, w.y};
        SetTextColor(hdc, w.isLink ? linkCol : textCol);
        HFONT use = (w.isBold && boldFont) ? boldFont : font;
        HdcDrawText(hdc, w.text, pt, fmt, use);
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
        HdcDrawLine(hdc, Rect(x1, underlineY, x2 - x1, 0));
    }
    SelectObject(hdc, prevPen);
    if (boldFont) {
        DeleteObject(boldFont);
    }
    DeleteObject(pen);
}

// returns index into tip.links of the link at (x, y) in layout coords, or -1
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

// runs a link target: "Cmd..." sends the command to hwnd, a url goes to gTipOpenUrl.
// Cmd targets may include arguments (e.g. "CmdFixDefaultApp .pdf"); those go through
// CreateCommandFromDefinition so FrameOnCommand sees a CustomCommand with args.
void ExecuteTipLink(HWND hwnd, Str cmd) {
    if (len(cmd) == 0) {
        return;
    }
    if (str::StartsWith(cmd, StrL("Cmd"))) {
        CustomCommand* custom = CreateCommandFromDefinition(cmd);
        if (custom) {
            HwndSendCommand(hwnd, custom->id);
            return;
        }
        int cmdId = GetCommandIdByName(cmd);
        if (cmdId > 0) {
            HwndSendCommand(hwnd, cmdId);
        }
        return;
    }
    if (str::StartsWith(cmd, StrL("http://")) || str::StartsWith(cmd, StrL("https://"))) {
        if (gTipOpenUrl) {
            gTipOpenUrl(cmd);
        }
    }
}

bool TipHasRichContent(ParsedTip& tip) {
    if (len(tip.links) > 0) {
        return true;
    }
    for (TipWord& w : tip.words) {
        if (w.isBold) {
            return true;
        }
    }
    return false;
}

// reconstructs the plain (link markup removed, Key/ expanded) text from a parse
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
