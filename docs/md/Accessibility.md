# Accessibility

SumatraPDF provides several accessibility-related features. Maturity varies by feature.

## Read Aloud

**Read Aloud (TTS)** — read document text with Windows text-to-speech, word highlight, playback bar, and pause / continue / stop — is documented in [Read Aloud (TTS)](Read-Aloud-TTS.md).

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