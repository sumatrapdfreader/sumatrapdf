# Accessibility

SumatraPDF provides several accessibility-related features. Maturity varies by feature.

## Read Aloud

**Read Aloud (TTS)** — read document text with Windows text-to-speech, word highlight, playback bar, and pause / continue / stop — is documented in [Read Aloud (TTS)](Read-Aloud.md).

## Screen readers (UI Automation)

SumatraPDF exposes a [UI Automation](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-uiautomationoverview) tree so screen readers (Microsoft Narrator, NVDA, and other UIA clients) can access document text on the canvas.

### Microsoft Narrator

1. Start **Narrator** (Windows built-in, `Win+Ctrl+Enter`).
2. Open a document in SumatraPDF.
3. Move focus to the document canvas and navigate / select text — Narrator should read document content via the UIA text pattern.

**ver 3.7+:** reading through the document character by character, word by word, line by line and page by page works. Before 3.7 the text range never advanced, so a screen reader would repeat the first line (or read nothing) even though the text was there. Reading the text under the mouse pointer or finger (Narrator's mouse mode, touch exploration) works too, and screen readers can now locate the text they read on screen — so Narrator can highlight it.

**Supported document types for UIA:** PDF, XPS, DjVu (engines that expose extractable page text).

**Known issue:** Narrator sometimes stops reading until you switch focus to another window and back.

### Other screen readers (NVDA, JAWS, …)

Any client that uses Windows UI Automation can query the same document tree. NVDA and JAWS are not exhaustively tested for every document; feedback welcome in [discussions](https://github.com/sumatrapdfreader/sumatrapdf/discussions) or issues.

**Tips:**

- Prefer documents with real text (not scan-only image PDFs). Scanned pages need OCR elsewhere.
- Standard UI (menus, toolbar, TOC, bookmarks) uses native Windows controls and is generally readable by screen readers even without canvas UIA.
- Built-in [Read Aloud](Read-Aloud.md) is a separate, app-driven TTS path — useful when you want continuous reading with word highlight, but it is not a full screen-reader substitute.

### Plugin

Accessibility is **not** enabled in the SumatraPDF browser plugin.

## Keyboard-only use

Many actions are available without a mouse — see [Keyboard shortcuts](Keyboard-shortcuts.md), [Command Palette](Command-Palette.md), and [Finding text](Finding-text.md) (important for users who cannot use function keys without `Fn`).

## See also

- [Read Aloud (TTS)](Read-Aloud.md)
- [FAQ](FAQ.md)
- [Commands](Commands.md)

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
