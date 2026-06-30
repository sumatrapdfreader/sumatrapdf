/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Shared "tip" text: a small markup understood by the home page tips and by
// notifications. Supports:
//   [text](Cmd...)      a link that runs a command on click
//   [text](Help/Page)   a link that opens a docs page in the browser
//   [text](https://..)  a link that opens a url in the browser
//   (Key/Cmd...)        expanded inline to the command's keyboard shortcut
// note: include Base.h before this

// a word in a parsed tip; can be part of a link
struct TipWord {
    Str text; // owned
    int dx = 0;
    int dy = 0;
    int x = 0;
    int y = 0;
    bool isLink = false;
    int linkIdx = -1; // index into ParsedTip::links
};

struct TipLink {
    Str cmd; // owned, resolved target (url or "Cmd...")
    int firstWord = 0;
    int lastWord = 0; // inclusive
};

struct ParsedTip {
    Vec<TipWord> words;
    Vec<TipLink> links;
    int totalDx = 0; // computed by LayoutTip
    int totalDy = 0; // computed by LayoutTip

    void Reset() {
        for (auto& w : words) {
            str::Free(w.text);
        }
        for (auto& l : links) {
            str::Free(l.cmd);
        }
        words.Reset();
        links.Reset();
        totalDx = 0;
        totalDy = 0;
    }

    ~ParsedTip() { Reset(); }
};

// reconstructs the plain (link markup removed, Key/ expanded) text from a parse
TempStr TipPlainTextTemp(ParsedTip& tip);
bool TipHasLinks(ParsedTip& tip);

void ParseTip(ParsedTip& tip, Str s);
void MeasureTipWords(ParsedTip& tip, HDC hdc, HFONT font);
// lays out words within areaWidth (wrapping); sets per-word x/y and tip.totalDx/totalDy
void LayoutTip(ParsedTip& tip, int areaWidth, int startX, int startY);
// draws the words (link words in linkCol, underlined; others in textCol)
void DrawTipWords(HDC hdc, ParsedTip& tip, HFONT font, COLORREF textCol, COLORREF linkCol);
// returns index into tip.links of the link at (x, y) in layout coords, or -1
int HitTestTipLink(ParsedTip& tip, int x, int y);
// runs a link target: "Cmd..." sends the command to hwnd, a url opens the browser
void ExecuteTipLink(HWND hwnd, Str cmd);
