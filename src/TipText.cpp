/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Parsing, laying out and drawing "tip" text: a small markup with links,
// keyboard shortcuts and bold runs. Shared by SumatraPDF's home page and
// notifications, and by other apps in the family. What it needs from the app
// is its command table (see TipText.h) and a way to open a url.

#include "base/Base.h"
#include "base/Win.h"

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
        SkipTipWhitespace(p);
        if (len(p) == 0) {
            break;
        }

        // **bold text**
        if (p.len >= 4 && p.s[0] == '*' && p.s[1] == '*') {
            Str after(p.s + 2, p.len - 2);
            int end = str::IndexOf(after, StrL("**"));
            if (end >= 0) {
                Str boldText(after.s, end);
                AppendTipWordsFromText(tip, boldText, false, -1, true);
                AdvanceTipText(p, 2 + end + 2);
                continue;
            }
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
    if (str::StartsWith(cmd, StrL("Cmd"))) {
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
