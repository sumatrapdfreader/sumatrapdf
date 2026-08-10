/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Supplied by the app: its command table and the key each command is bound to.
// Both SumatraPDF and other apps have their own, with these signatures.
// (also declared in Commands.h / Accelerators.h; repeated here so TipText.cpp
// doesn't depend on SumatraPDF's headers)
int GetCommandIdByName(Str cmdName);                            // NOLINT(readability-redundant-declaration)
TempStr AppendAccelKeyToMenuStringTemp(TempStr str, int cmdId); // NOLINT(readability-redundant-declaration)

// how the app opens a url link; without it, url links do nothing
extern void (*gTipOpenUrl)(Str url);

struct TipLink;

// a word in a parsed tip; can be part of a link. Node of the intrusive list
// rooted at ParsedTip::words
struct TipWord {
    TipWord* next = nullptr;
    Str text; // owned
    int dx = 0;
    int dy = 0;
    int x = 0;
    int y = 0;
    bool isLink = false;
    bool isBold = false;
    // (Kbd/...) — drawn as a key-cap like the keyboard-shortcuts help sheet
    bool isKbd = false;
    // no inter-word space before this word: it abutted the previous token in the
    // source with no whitespace, e.g. the ':' in "**foo**:" (issue: bold ran into
    // following punctuation with a stray space)
    bool noSpaceBefore = false;
    TipLink* link = nullptr; // the link this word belongs to, if any
};

// node of the intrusive list rooted at ParsedTip::links
struct TipLink {
    TipLink* next = nullptr;
    Str cmd; // owned, resolved target (url or "Cmd...")
    TipWord* firstWord = nullptr;
    TipWord* lastWord = nullptr; // inclusive
};

// `words` and `links` are root nodes of intrusive lists: the parsed content
// starts at words.next / links.next, the roots themselves hold nothing. The
// nodes are owned and freed by Reset()
struct ParsedTip {
    TipWord words;
    TipLink links;
    int totalDx = 0; // computed by LayoutTip
    int totalDy = 0; // computed by LayoutTip

    // where to append next, so parsing doesn't walk the list for every word
    TipWord* lastWord = nullptr;
    TipLink* lastLink = nullptr;

    void Reset();

    ~ParsedTip() { Reset(); }
};

int TipWordCount(ParsedTip& tip);
int TipLinkCount(ParsedTip& tip);
TempStr TipPlainTextTemp(ParsedTip& tip);
// true if the tip needs the per-word rich draw path: it has links or any bold
// run (a plain message can take the cheaper single DrawText path)
bool TipHasRichContent(ParsedTip& tip);

void ParseTip(ParsedTip& tip, Str s);
void AddTipPlainText(ParsedTip& tip, Str text);
void MeasureTipWords(ParsedTip& tip, HDC hdc, HFONT font);
void LayoutTip(ParsedTip& tip, int areaWidth, int startX, int startY);
// bgCol is the tip background (used for key-cap fill/border); pass the same
// color the tip is painted on so caps match light/dark themes
void DrawTipWords(HDC hdc, ParsedTip& tip, HFONT font, COLORREF textCol, COLORREF linkCol, COLORREF bgCol);
TipLink* HitTestTipLink(ParsedTip& tip, int x, int y);
void ExecuteTipLink(HWND hwnd, Str cmd);
