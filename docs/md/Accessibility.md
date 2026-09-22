# Accessibility

SumatraPDF provides several accessibility-related features.
## Read Aloud

**Read Aloud (TTS)** — read text with Windows text-to-speech, word highlight, playback bar, and pause / continue / stop. Documented in [Read Aloud (TTS)](Read-Aloud.md).

## Screen readers (UI Automation)

SumatraPDF exposes a [UI Automation](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-uiautomationoverview) tree so screen readers (Microsoft Narrator, NVDA, and other UIA clients) can access document text.

### Microsoft Narrator

1. Start **Narrator** (Windows built-in, `Win+Ctrl+Enter`).
2. Open a document in SumatraPDF.
3. Move focus to the document canvas and navigate / select text — Narrator should be able to read document content.

**Ver 3.7+:**  added reading through the document character by character, word by word, line by line and page by page. Before 3.7 the text range never advanced, so a screen reader would repeat the first line (or read nothing) even though the text was there. Reading the text under the mouse pointer or finger (Narrator's mouse mode, touch exploration) works too, and screen readers can now locate the text they read on screen — so Narrator can highlight it.

**Supported document types for UI Automatoin:** PDF, XPS, DjVu (documents that contain text we can extract).

**Known issue:** Narrator sometimes stops reading until you switch focus to another window and back.

### Other screen readers (NVDA, JAWS, …)

Any client that uses Windows UI Automation should work, including NVDA and JAWS.

**Notes:**

- Some PDF / DjVu documents are scanned pages, not text, so text reading will not work.  
- Built-in [Read Aloud](Read-Aloud.md) is a separate, app-driven text-to-speech path. 

### Embedded (plugin) mode

UI Automation is **not** exposed when SumatraPDF runs in plugin mode, i.e. when it's started with the `-plugin <parent HWND>` command-line option and embedded as a frameless child window inside another application. The standalone SumatraPDF window is not affected.

## Keyboard-only use

Many actions are available without a mouse — see [Keyboard shortcuts](Keyboard-shortcuts.md), [Command Palette](Command-Palette.md), and [Finding text](Finding-text.md) (important for users who cannot use function keys without `Fn`).

## See also

- [Read Aloud (TTS)](Read-Aloud.md)
- [FAQ](FAQ.md)
- [Commands](Commands.md)
