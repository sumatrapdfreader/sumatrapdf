/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// needs wingui/PlatformFont.h, wingui/Gfx.h and wingui/VirtWnd.h included first

struct TipLink;

// What the markup needs to know about the app's commands. The app implements it
// and installs it as gCommandsContext, which is what keeps this code free of
// SumatraPDF's command table.
struct CommandsContext {
    virtual ~CommandsContext() = default;

    // Keyboard shortcut for a command name like "CmdOpenFile", used by
    // (Key/CmdOpenFile). Returns {} when there is no such command (the markup is
    // then left as literal text), and the command name itself when the command
    // exists but has no binding.
    virtual TempStr GetCommandShortcutTemp(Str cmdName) = 0;

    // Runs a link target that names a command. `cmd` may carry arguments, e.g.
    // "CmdFixDefaultApp .pdf".
    virtual void ExecuteCommand(HWND hwnd, Str cmd) = 0;
};

extern CommandsContext* gCommandsContext;

// how the app opens a url link; without it, url links do nothing
extern void (*gTipOpenUrl)(Str url);

// a word in the text; can be part of a link. Node of the intrusive list rooted
// at VirtRichText::words
struct TipWord {
    TipWord* next = nullptr;
    Str text; // owned
    int dx = 0;
    int dy = 0;
    // relative to the control's content origin, set by LayoutText()
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

// node of the intrusive list rooted at VirtRichText::links
struct TipLink {
    TipLink* next = nullptr;
    Str cmd; // owned, resolved target (url or "Cmd...")
    TipWord* firstWord = nullptr;
    TipWord* lastWord = nullptr; // inclusive
};

// A run of text with links, keyboard shortcuts and bold runs, as a virtual
// control: it wraps itself to the width it is given, paints itself, and handles
// clicks on its links. `words` and `links` are root nodes of intrusive lists;
// the content starts at words.next / links.next.
struct VirtRichText : VirtWnd {
    TipWord words;
    TipLink links;
    // where to append next, so parsing doesn't walk the list for every word
    TipWord* lastWord = nullptr;
    TipLink* lastLink = nullptr;

    // size of the laid-out text, computed by LayoutText()
    int totalDx = 0;
    int totalDy = 0;
    // the width the words were last laid out for
    int layoutDx = -1;

    PlatformFont* font = nullptr; // not owned
    COLORREF textColor = kColorUnset;
    COLORREF linkColor = kColorUnset;
    // the color the text is painted on; used for the key-cap fill and border
    COLORREF bgColor = kColorUnset;
    // link commands are sent to this window
    HWND hwndForCmds = nullptr;

    VirtRichText();
    ~VirtRichText() override;

    void Reset();
    void AddPlainText(Str);
    void LayoutText(int areaWidth);
    bool HasRichContent();
    TempStr PlainTextTemp();
    TipLink* LinkAt(Point ptLocal);

    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    Size Layout(Constraints bc) override;
    Size GetIdealSize() override;
    void SetBounds(Rect) override;
    void Paint(VirtWndPaintCtx&) override;
    bool OnMouseDown(VirtWndMouseEvent&) override;
    bool OnMouseUp(VirtWndMouseEvent&) override;
    bool OnSetCursor(Point ptLocal) override;
    TempStr GetTooltipTemp(Point ptLocal) override;
};

VirtRichText* ParseTip(Str s);
int TipWordCount(VirtRichText*);
int TipLinkCount(VirtRichText*);
void ExecuteTipLink(HWND hwnd, Str cmd);
