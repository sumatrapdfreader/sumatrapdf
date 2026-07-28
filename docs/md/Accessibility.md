# Accessibility

SumatraPDF provides several accessibility-related features. Maturity varies by feature.

## Read Aloud

**Read Aloud (TTS)** — read document text with Windows text-to-speech, word highlight, playback bar, and pause / continue / stop — is documented in [Read Aloud (TTS)](Read-Aloud-TTS.md).

## Screen readers (UI Automation)

SumatraPDF exposes a [UI Automation](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-uiautomationoverview) tree so screen readers (Microsoft Narrator, NVDA, and other UIA clients) can access document text on the canvas.

### Microsoft Narrator

1. Start **Narrator** (Windows built-in, `Win+Ctrl+Enter`).
2. Open a document in SumatraPDF.
3. Move focus to the document canvas and navigate / select text — Narrator should read document content via the UIA text pattern.

**Supported document types for UIA:** PDF, XPS, DjVu (engines that expose extractable page text).

**Known issue:** Narrator sometimes stops reading until you switch focus to another window and back.

### Other screen readers (NVDA, JAWS, …)

Any client that uses Windows UI Automation can query the same document tree. NVDA and JAWS are not exhaustively tested for every document; feedback welcome in [discussions](https://github.com/sumatrapdfreader/sumatrapdf/discussions) or issues.

**Tips:**

- Prefer documents with real text (not scan-only image PDFs). Scanned pages need OCR elsewhere.
- Standard UI (menus, toolbar, TOC, bookmarks) uses native Windows controls and is generally readable by screen readers even without canvas UIA.
- Built-in [Read Aloud](Read-Aloud-TTS.md) is a separate, app-driven TTS path — useful when you want continuous reading with word highlight, but it is not a full screen-reader substitute.

### Plugin

Accessibility is **not** enabled in the SumatraPDF browser plugin.

## Keyboard-only use

Many actions are available without a mouse — see [Keyboard shortcuts](Keyboard-shortcuts.md), [Command Palette](Command-Palette.md), and [Finding text](Finding-text.md) (important for users who cannot use function keys without `Fn`).

## See also

- [Read Aloud (TTS)](Read-Aloud-TTS.md)
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
