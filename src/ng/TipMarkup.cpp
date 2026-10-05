/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "Commands.h"
#include "ShortcutParse.h"
#include "Accelerators.h"
#include "TipMarkup.h"

// orig's ResolveLinkCmdTemp
static TempStr ResolveLinkCmdTemp(Str cmd) {
    if (str::StartsWith(cmd, StrL("https://")) || str::StartsWith(cmd, StrL("http://"))) {
        return str::DupTemp(cmd);
    }
    if (str::TrimPrefix(cmd, StrL("Help/"))) {
        return fmt("https://www.sumatrapdfreader.org/docs/%s", cmd);
    }
    // Cmd* - used as-is, resolved to a command id on click
    return str::DupTemp(cmd);
}

// the shortcut a (Key/CmdXxx) fragment stands for, orig's
// SumatraCommandsContext::GetCommandShortcutTemp
static TempStr CommandShortcutTemp(Str cmdName) {
    int cmdId = GetCommandIdByName(cmdName);
    if (cmdId <= 0) {
        return {}; // not a command: the markup stays literal text
    }
    TempStr accel = AppendAccelKeyToMenuStringTemp(StrL(""), cmdId);
    if (len(accel) == 0 || !*accel.s) {
        return str::DupTemp(cmdName); // a command, but unbound
    }
    // AppendAccelKeyToMenuStringTemp prepends '\t', skip it
    if (accel.s[0] == '\t') {
        return str::DupTemp(Str(accel.s + 1, accel.len - 1));
    }
    return accel;
}

// index of the ')' closing the '(' at `start`, or -1
static int MatchingCloseParen(Str s, int start) {
    int depth = 0;
    for (int i = start; i < s.len; i++) {
        if (s.s[i] == '(') {
            depth++;
        } else if (s.s[i] == ')') {
            depth--;
            if (depth == 0) {
                return i;
            }
        }
    }
    return -1;
}

static void AddSpan(Vec<TipSpan>& out, TipSpanKind kind, Str text, Str target) {
    if (len(text) == 0) {
        return;
    }
    TipSpan sp;
    sp.kind = kind;
    sp.text = str::Dup(text);
    sp.target = str::Dup(target);
    VecAppend(out, sp);
}

void TipSpansParse(Vec<TipSpan>& out, Str line) {
    int i = 0;
    int runStart = 0;
    while (i < line.len) {
        char c = line.s[i];
        if (c == '[') {
            Str rest = Str(line.s + i, line.len - i);
            int close = str::IndexOfChar(rest, ']');
            bool isLink = close > 0 && (i + close + 1) < line.len && line.s[i + close + 1] == '(';
            int end = -1;
            if (isLink) {
                Str afterOpen = Str(line.s + i + close + 2, line.len - i - close - 2);
                end = str::IndexOfChar(afterOpen, ')');
            }
            if (end >= 0) {
                AddSpan(out, TipSpanKind::Text, Str(line.s + runStart, i - runStart), {});
                Str target = ResolveLinkCmdTemp(Str(line.s + i + close + 2, end));
                AddSpan(out, TipSpanKind::Link, Str(line.s + i + 1, close - 1), target);
                i = i + close + 2 + end + 1;
                runStart = i;
                continue;
            }
        }
        if (c == '(' && str::StartsWith(Str(line.s + i, line.len - i), StrL("(Key/"))) {
            Str rest = Str(line.s + i, line.len - i);
            int close = str::IndexOfChar(rest, ')');
            if (close > 0) {
                Str cmdName = Str(line.s + i + 5, close - 5);
                TempStr shortcut = CommandShortcutTemp(cmdName);
                if (len(shortcut) > 0) {
                    AddSpan(out, TipSpanKind::Text, Str(line.s + runStart, i - runStart), {});
                    AddSpan(out, TipSpanKind::Kbd, shortcut, {});
                    i += close + 1;
                    runStart = i;
                    continue;
                }
            }
        }
        // (Kbd/text): a key cap of whatever is inside, and what is inside may
        // itself be a (Key/CmdXxx). orig nests VirtRichText runs for this
        if (c == '(' && str::StartsWith(Str(line.s + i, line.len - i), StrL("(Kbd/"))) {
            int close = MatchingCloseParen(line, i);
            if (close > 0) {
                Str inner = Str(line.s + i + 5, close - i - 5);
                Vec<TipSpan> innerSpans;
                TipSpansParse(innerSpans, inner);
                TempStr capText = TipSpansPlainTextTemp(innerSpans);
                TipSpansFree(innerSpans);
                if (len(capText) > 0) {
                    AddSpan(out, TipSpanKind::Text, Str(line.s + runStart, i - runStart), {});
                    AddSpan(out, TipSpanKind::Kbd, capText, {});
                    i = close + 1;
                    runStart = i;
                    continue;
                }
            }
        }
        // **bold text**
        if (c == '*' && i + 3 < line.len && line.s[i + 1] == '*') {
            Str after = Str(line.s + i + 2, line.len - i - 2);
            int end = str::IndexOf(after, StrL("**"));
            if (end >= 0) {
                AddSpan(out, TipSpanKind::Text, Str(line.s + runStart, i - runStart), {});
                AddSpan(out, TipSpanKind::Bold, Str(after.s, end), {});
                i = i + 2 + end + 2;
                runStart = i;
                continue;
            }
        }
        // a standalone '*' separates items; orig draws it as a middle dot
        if (c == '*' && (i + 1 >= line.len || line.s[i + 1] == ' ')) {
            AddSpan(out, TipSpanKind::Text, Str(line.s + runStart, i - runStart), {});
            AddSpan(out, TipSpanKind::Text, StrL("\xc2\xb7"), {});
            i++;
            runStart = i;
            continue;
        }
        if (c == '`') {
            Str rest = Str(line.s + i + 1, line.len - i - 1);
            int close = str::IndexOfChar(rest, '`');
            if (close >= 0) {
                AddSpan(out, TipSpanKind::Text, Str(line.s + runStart, i - runStart), {});
                AddSpan(out, TipSpanKind::Code, Str(line.s + i + 1, close), {});
                i = i + close + 2;
                runStart = i;
                continue;
            }
        }
        i++;
    }
    AddSpan(out, TipSpanKind::Text, Str(line.s + runStart, line.len - runStart), {});
}

void TipSpansAddLink(Vec<TipSpan>& out, Str text, Str cmd) {
    AddSpan(out, TipSpanKind::Link, text, cmd);
}

TempStr TipSpansPlainTextTemp(const Vec<TipSpan>& spans) {
    str::Builder b(GetTempArena());
    for (const TipSpan& sp : spans) {
        b.Append(sp.text);
    }
    return ToStr(b);
}

bool TipSpansHaveRichContent(const Vec<TipSpan>& spans) {
    for (const TipSpan& sp : spans) {
        if (sp.kind != TipSpanKind::Text) {
            return true;
        }
    }
    return false;
}

void TipSpansFree(Vec<TipSpan>& spans) {
    for (TipSpan& sp : spans) {
        str::Free(sp.text);
        str::Free(sp.target);
    }
    VecReset(spans);
}
