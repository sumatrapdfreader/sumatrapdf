# Accessibility and Text-to-Speech

SumatraPDF provides several accessibility-related features. Maturity varies by feature.

## Read Aloud (pre-release 3.7+)

Read document text using Windows text-to-speech. You can start from a text selection, from the first visible text in the viewport, or (from the context menu) from the position where you right-clicked.

### How to use

1. Open a document with selectable text (PDF, EPUB, etc.).
2. Start reading from one of these places:
   - **Toolbar** — Read Aloud button (click to start / pause / continue; dropdown for scope and voice)
   - **Main menu** — **Read Aloud (TTS)** (after Selection)
   - **Context menu** — **Read Aloud (TTS)** (after Document)
   - **Command palette** (`Ctrl + K`) — e.g. **Start Reading From Top**, **Start Reading Selection**, **Read Aloud**
3. While speaking, a **playback bar** at the bottom of the canvas shows the document name, current page, and start scope, with **Pause** / **Resume** and **Stop** buttons. The current word is also highlighted on the page. **Pause Reading**, **Continue Reading**, and **Stop Reading** are in the Read Aloud menus and palette.

### Start scopes

| Command | Behavior |
|---------|----------|
| **Read Aloud** (toolbar / palette) | Selection if present, otherwise first visible text in the viewport; continues through the rest of the document |
| **Start Reading From Top** | First visible text in the viewport → end of document |
| **Start Reading From Cursor Position** | Context menu only — right-click position → end of document |
| **Start Reading Selection** | Selected text only |

### Voice

Open **Voice** in any Read Aloud menu to pick **System default** or an installed Windows voice. Your choice is remembered in `ReadAloudVoiceId` in [Advanced options](Advanced-options-settings.md) (`SumatraPDF-settings.txt`). Leave it empty for the system default.

### Limitations

- Uses Windows speech voices installed on your system (WinRT Speech Synthesis with SAPI fallback).
- EPUB/complex layouts may read in an order that does not match visual layout.
- Copy-restricted documents cannot be read aloud.
- Does not replace a full screen-reader experience for blind users.

## Screen readers (UI Automation)

SumatraPDF exposes an experimental [UI Automation](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-uiautomationoverview) tree so screen readers can access document text.

### Microsoft Narrator

1. Start **Narrator** (Windows built-in).
2. Open a document in SumatraPDF.
3. Select text — Narrator should read the selection.

**Supported document types for UIA:** PDF, XPS, DjVu.

**Known issue:** Narrator sometimes stops reading until you switch focus to another window and back.

### Other screen readers

NVDA and other clients are not officially tested. Feedback welcome in [discussions](https://github.com/sumatrapdfreader/sumatrapdf/discussions).

### Plugin

Accessibility is **not** enabled in the SumatraPDF browser plugin.

## Keyboard-only use

Many actions are available without a mouse — see [Keyboard shortcuts](Keyboard-shortcuts.md), [Command Palette](Command-Palette.md), and [Finding text](Finding-text.md) (important for users who cannot use function keys without `Fn`).

## See also

- [FAQ](FAQ.md)
- [Commands](Commands.md) — Read Aloud commands
- [Advanced options](Advanced-options-settings.md) — `ReadAloudVoiceId`
- [Version history](Version-history.md) — 3.7 Read Aloud entry

# Technical documentation

For SumatraPDF developers — UIAutomation element structure when a document is loaded:

```
Window
 |-> FragmentRoot
     Name: "Canvas"
     ControlType: UIA_CustomControlTypeId
      |
      |-> Fragment
          Name: [filename]
          ControlType: UIA_DocumentControlTypeId 
          NativeWindowHandle: 0
          Patterns: ITextProvider
            |
            | -> Fragment
            |    Name: "Page 1"
            |    Patterns: IValueProvider
            -
            -
            |
            | -> Fragment
                 Name: "Page n"
                 Patterns: IValueProvider
```

When no document is loaded:

```
Window
 |-> FragmentRoot
     Name: "Canvas"
     ControlType: UIA_CustomControlTypeId
      |
      |-> Fragment
          Name: "Start Page"
```