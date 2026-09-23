# Accessibility

SumatraPDF works with screen readers through Windows UI Automation, reads text aloud with built-in text-to-speech, and can be used from the keyboard alone.

**Most often you use a screen reader such as Narrator or NVDA to read document text.** At a glance:

- **Screen readers:** a [UI Automation](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-uiautomationoverview) tree for Narrator, NVDA, JAWS and other UIA clients.
- **Read Aloud (TTS):** Windows text-to-speech with word highlight, playback bar, and pause / continue / stop — see [Read Aloud (TTS)](Read-Aloud.md).
- **Keyboard-only use:** [Keyboard shortcuts](Keyboard-shortcuts.md) and the [Command Palette](Command-Palette.md).

## Read a document with a screen reader

SumatraPDF exposes a UI Automation tree so screen readers (Microsoft Narrator, NVDA, and other UIA clients) can access document text.

### Microsoft Narrator

1. Start **Narrator** (Windows built-in, `Win+Ctrl+Enter`).
2. Open a document in SumatraPDF.
3. Move focus to the document canvas and navigate / select text — Narrator should be able to read document content.

**Ver 3.7+:** added reading through the document character by character, word by word, line by line and page by page. Before 3.7 the text range never advanced, so a screen reader would repeat the first line (or read nothing) even though the text was there. Reading the text under the mouse pointer or finger (Narrator's mouse mode, touch exploration) works too, and screen readers can now locate the text they read on screen — so Narrator can highlight it.

**Known issue:** Narrator sometimes stops reading until you switch focus to another window and back.

### Other screen readers (NVDA, JAWS, …)

Any client that uses Windows UI Automation should work, including NVDA and JAWS.

### Supported documents

UI Automation supports PDF, XPS, DjVu (documents that contain text we can extract).

Note: some PDF / DjVu documents are scanned pages, not text, so text reading will not work.

## Listen with Read Aloud

**Read Aloud (TTS)** reads text with Windows text-to-speech, with word highlight, playback bar, and pause / continue / stop. It is a separate, app-driven text-to-speech path, not a screen reader. Documented in [Read Aloud (TTS)](Read-Aloud.md).

## Use SumatraPDF without a mouse

Many actions are available without a mouse — see [Keyboard shortcuts](Keyboard-shortcuts.md), [Command Palette](Command-Palette.md), and [Finding text](Finding-text.md) (important for users who cannot use function keys without `Fn`).

## Tips

- If Narrator stops reading, switch focus to another window and back.
- Use [Read Aloud](Read-Aloud.md) to listen to a document without running a screen reader.
- Use `Ctrl + K` ([Command Palette](Command-Palette.md)) to run any command by name from the keyboard.

## Embedded (plugin) mode

UI Automation is **not** exposed when SumatraPDF runs in plugin mode, i.e. when it's started with the `-plugin <parent HWND>` command-line option and embedded as a frameless child window inside another application. The standalone SumatraPDF window is not affected.

## See also

- [Read Aloud (TTS)](Read-Aloud.md) — built-in text-to-speech
- [FAQ](FAQ.md)
- [Commands](Commands.md) — command ids for keyboard shortcuts
