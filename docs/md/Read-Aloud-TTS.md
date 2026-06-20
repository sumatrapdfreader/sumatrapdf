# Read Aloud (TTS)

*Pre-release 3.7+*

Read document text using Windows text-to-speech. You can start from a text selection, from the first visible text in the viewport, or (from the context menu) from the position where you right-clicked.

## How to use

1. Open a document with selectable text (PDF, EPUB, etc.).
2. Start reading from one of these places:
   - **Toolbar** — Read Aloud button (click to start / pause / continue; the icon shows a speaker when idle or paused, and a pause symbol while speaking). Use the dropdown arrow for explicit start scopes and **Voice**.
   - **Main menu** — **Read Aloud (TTS)** (after Selection)
   - **Context menu** — **Read Aloud (TTS)** (after Document)
   - **Command palette** (`Ctrl + K`) — **Read Aloud**, **Start Reading From Top**, **Start Reading Selection**, **Pause Reading**, **Continue Reading**, **Stop Reading** (transport commands appear only when they apply)
3. While a session is active (speaking or paused), a **playback bar** at the bottom of the canvas shows the document name, **page X of Y**, start scope, and **Pause** / **Resume** and **Stop** buttons on the left. The current word is highlighted on the page while speaking. **Pause Reading**, **Continue Reading**, and **Stop Reading** are also in all three Read Aloud menus.

**Pause** stops speech and remembers your position so you can **Continue Reading** later. **Stop** ends the session and clears the resume position.

Switching to another tab stops reading and clears the resume position on the tab you left.

## Start scopes

| Command | Behavior |
|---------|----------|
| **Read Aloud** (toolbar / palette) | Selection if present, otherwise first visible text in the viewport; continues through the rest of the document |
| **Start Reading From Top** | First visible text in the viewport → end of document |
| **Start Reading From Cursor Position** | Context menu only — right-click position → end of document (disabled when there is no text at the cursor) |
| **Start Reading Selection** | Selected text only; does not continue past the selection |

Scope labels on the playback bar: **Smart start**, **From top**, **From cursor**, or **Selection**.

## While listening

- **Word highlight** — the spoken word is highlighted on the page (uses the selection highlight color).
- **Auto-scroll** — the viewport scrolls to keep the spoken word in view. If you scroll the highlight fully off-screen, auto-scroll stops for that session so manual navigation is respected.

## Voice

Open **Voice** in any Read Aloud menu to pick **System default** or an installed Windows voice. Your choice is remembered in `ReadAloudVoiceId` in [Advanced options](Advanced-options-settings.md) (`SumatraPDF-settings.txt`). Leave it empty for the system default.

## Limitations

- Uses Windows speech voices installed on your system (WinRT Speech Synthesis with SAPI fallback).
- EPUB/complex layouts may read in an order that does not match visual layout.
- Copy-restricted documents cannot be read aloud (no message is shown).
- Pages with no extractable text show a short *“No text available to read aloud”* notification.
- Does not replace a full screen-reader experience for blind users — see [Accessibility](Accessibility.md) for UI Automation / Narrator.

## See also

- [Commands](Commands.md) — Read Aloud commands
- [Advanced options](Advanced-options-settings.md) — `ReadAloudVoiceId`
- [Version history](Version-history.md) — 3.7 Read Aloud entry
- [Accessibility](Accessibility.md) — screen readers and other accessibility features