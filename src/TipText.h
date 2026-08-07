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

// a word in a parsed tip; can be part of a link
struct TipWord {
    Str text; // owned
    int dx = 0;
    int dy = 0;
    int x = 0;
    int y = 0;
    bool isLink = false;
    bool isBold = false;
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

TempStr TipPlainTextTemp(ParsedTip& tip);
bool TipHasLinks(ParsedTip& tip);

void ParseTip(ParsedTip& tip, Str s);
void AddTipPlainText(ParsedTip& tip, Str text);
void MeasureTipWords(ParsedTip& tip, HDC hdc, HFONT font);
void LayoutTip(ParsedTip& tip, int areaWidth, int startX, int startY);
void DrawTipWords(HDC hdc, ParsedTip& tip, HFONT font, COLORREF textCol, COLORREF linkCol);
int HitTestTipLink(ParsedTip& tip, int x, int y);
void ExecuteTipLink(HWND hwnd, Str cmd);
