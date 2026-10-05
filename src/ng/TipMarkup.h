/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig parses the tip markup into a VirtRichText (gui/win/TipText.cpp),
// which draws itself with Gfx. Here the same markup becomes a list of spans
// the gpui side turns into elements: the home page's tip band (step 9b) and
// the notifications (step 14a).

enum class TipSpanKind {
    Text,
    Link,
    Kbd,
    Code,
    Bold,
};

// text and target are owned; free the whole list with TipSpansFree()
struct TipSpan {
    TipSpanKind kind = TipSpanKind::Text;
    Str text;
    Str target; // for Link: a command name or a url
};

// orig's ParseTipInto(): appends the spans of `markup` to `out`
void TipSpansParse(Vec<TipSpan>& out, Str markup);
// orig's VirtRichText::AddPlainLink(): text that is a link but is not parsed,
// so text from outside the app can't inject a command (GHSA-2wv2-qm2f-vmxh)
void TipSpansAddLink(Vec<TipSpan>& out, Str text, Str cmd);
// orig's VirtRichText::PlainTextTemp()
TempStr TipSpansPlainTextTemp(const Vec<TipSpan>& spans);
bool TipSpansHaveRichContent(const Vec<TipSpan>& spans);
void TipSpansFree(Vec<TipSpan>& spans);
