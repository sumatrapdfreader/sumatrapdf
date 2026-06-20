# Read Aloud UX ideas for SumatraPDF

UX-focused analysis of the Read Aloud feature (pre-release 3.7+) and proposals for improving discoverability, controls, and listening workflows. No implementation details.

## Current experience (as implemented)

### What works well

- **Sensible default scope:** selection if present, otherwise current page — matches how people expect “read this” to work.
- **Text cleanup:** soft hyphens and line breaks are normalized before speech, which helps PDF wrapping sound natural.
- **Resume:** pause remembers word position; continue picks up where you left off.
- **Toolbar affordance:** speak / pause icon and tooltip reflect state; dropdown exposes page vs selection and voice list.
- **Backend choice:** Windows Speech Synthesis with SAPI fallback is the right platform fit.

### Where users struggle

| Issue | Why it hurts |
|-------|----------------|
| **Hard to discover** | Documented path is `Ctrl+K` → “read aloud”. No menu item, no shortcut, not on the selection context menu (unlike Translate). |
| **One button, three jobs** | Main toolbar click = start *or* pause *or* continue depending on state. Users must read the tooltip to know what will happen. |
| **Controls split across two UIs** | Transport (play/pause) is the main button; scope (page/selection) and voice are in the dropdown. That coupling is unintuitive. |
| **No playback chrome** | While reading, nothing on the page shows *what* is being read or *where* you are. `TtsGetSpokenPosUtf8()` exists only for resume, not display. |
| **Page-only, no “keep going”** | Stops at end of page/selection. No “read from here”, “read rest of document”, or auto-advance to next page — common for long-form listening. |
| **Global audio, local UI** | TTS is app-global; toolbar shows “Pause” on any window while audio may be from another tab. No “Reading: *filename*” indicator. |
| **Silent failures** | No text → 2s toast. Copy-restricted PDFs block read aloud with **no message** (`HasPermission(Perm::CopySelection)` returns early). |
| **Voice not remembered** | Voice choice is session-only (`gTtsVoiceId`), not in settings. |
| **Dropdown strings not localized** | “Read current page”, “Continue reading”, etc. are hardcoded English in `ShowTtsVoiceMenu`. |
| **Scanned PDFs dead-end** | “No text available to read aloud” with no next step (natural tie-in to OCR UX). |

## Design principle

Treat Read Aloud as **listening mode** layered on normal reading — not a hidden palette command. Users should see: *what* is reading, *how* to control it, and *how far* through the content they are.

Keep SumatraPDF’s minimal chrome: one lightweight playback strip beats a modal or a permanent toolbar widget.

## Recommended improvements (by priority)

### 1. Add a small Read Aloud bar (highest impact)

When speech starts, show a **thin non-modal bar** above the canvas (same family as notifications / print progress):

```
🔊 Reading page 12 · "Annual Report 2024.pdf"     ⏸  ⏹  ⚙
```

- **Pause / Resume** — explicit buttons; stop overloading the toolbar icon.
- **Stop** — ends playback and clears resume state (distinct from pause).
- **Scope label** — “Selection”, “Page 12”, or “From page 12”.
- **Source tab** — filename when reading continues after tab switch.

Dismiss on stop or natural end. Toolbar speak button can remain as a **shortcut into** this bar, not the only control surface.

### 2. Split toolbar click from dropdown

| Action | Control |
|--------|---------|
| Start / pause / resume | Main speak button (or playback bar) |
| Read selection / current page / from here | Dropdown only, or playback bar menu |
| Voice | Submenu “Voice →” or ⚙ on playback bar |

Dropdown opening should **not** be the only place to pick scope. Today scope is hidden behind a caret most users never click.

### 3. Put Read Aloud where users already act on text

- **Selection context menu** — “Read Aloud” at the top (near Copy), disabled when no text.
- **Edit menu** — “Read Aloud” submenu: Selection / Current Page / From Current Page…
- **Optional shortcut** — e.g. `Ctrl+Shift+S` (speech); bindable via custom shortcuts.

Palette stays for discovery; menus and context put it next to Copy and Translate.

### 4. Add “read more than one page”

Most requested gap for document listening:

| Command | Behavior |
|---------|----------|
| **Read from here** | Current page → end of document, auto-advance pages |
| **Read document** | Whole document (confirm if large) |

Playback bar shows **Page 12 of 240**. On page turn, scroll to keep the reading position visible (optional setting).

Pause/resume should work across page boundaries (store page + char offset, not just string offset in one page’s text).

### 5. Visual follow-along (optional but high delight)

Use existing word-position tracking to **soft-highlight** the sentence or line being spoken — similar to karaoke, not a second selection color. Toggle in Advanced Options: “Highlight text while reading aloud” (default on).

Even without perfect EPUB layout order, some feedback beats a static page.

### 6. Clearer empty and blocked states

Replace bare toasts with actionable messages:

- *“No text on this page.”* → **[Recognize text]** (if OCR exists) · **Read next page with text**
- *“This document doesn’t allow copying text.”* — when copy permission blocks TTS
- *“No speech voices installed.”* → link to Windows **Settings → Speech**

Keep toasts short; one primary action per message.

### 7. Persist voice (and maybe rate)

In **Advanced Options**:

- **Read aloud voice** — dropdown of installed voices; default “System”
- **Speech rate** — slider or presets (Slow / Normal / Fast) if the backend exposes it

Remember per user, not per session.

### 8. Tab and document lifecycle

Define explicit rules and reflect them in UI:

| Event | Suggested behavior |
|-------|-------------------|
| Switch tab while reading | Keep playing; playback bar shows source file; ⏹ stops |
| Close tab that’s reading | Stop + confirm if mid-document |
| Open new file in same tab | Stop (already resets on unload) |
| Start read on tab B while A is playing | Ask: “Stop reading *A* and start *B*?” |

Avoid a global pause button on the wrong tab with no context.

### 9. Unify command naming in palette

Palette entries today: “Read Aloud”, “Pause Read Aloud”, “Continue Read Aloud” — three names for one mode. Prefer:

- **Read Aloud** (starts or shows playback bar)
- **Pause Reading** / **Resume Reading** — only enabled in the right state
- Or a single **Toggle Read Aloud** with dynamic label

Same verbs as the playback bar and toolbar tooltip.

## What to defer

- Full audiobook features (bookmarks, sleep timer, MP3 export)
- Neural / cloud voices (stay on Windows installed voices)
- Per-paragraph voice or SSML editor
- Replacing Narrator / NVDA for blind users (document that in help)

## Suggested v1 story (one sentence for users)

*“Select text or open a page, click Read Aloud (or right-click → Read Aloud), and use the playback bar to pause, stop, or listen through the rest of the document — with an optional highlight showing where you are.”*

## Quick wins vs larger lifts

| Quick wins | Larger lifts |
|------------|----------------|
| Context menu + Edit menu entry | Playback bar |
| Shortcut + palette label cleanup | Multi-page / read-from-here |
| Notifications for copy-blocked / no voices | Follow-along highlight |
| Persist voice in settings | Page-aware resume across pages |
| Localize dropdown strings | OCR integration on no-text path |

The biggest gap versus user expectations is not voice quality — it’s **visibility and control**: people don’t know the feature exists, can’t tell what’s playing, and can’t listen past one page without manual restarts.

## Related existing behavior

- **Commands:** `CmdReadAloud`, `CmdPauseReadAloud`, `CmdContinueReadAloud` — palette-only, no default keyboard shortcut (`Commands.cpp`, `cmd/gen-commands.ts`)
- **Toolbar:** speak button with dropdown for scope + voice (`Toolbar.cpp`, `ShowTtsVoiceMenu` in `SumatraPDF.cpp`)
- **Scope:** selection if present, else current page (`ReadAloudInTab`)
- **Resume:** `readAloudText` + `readAloudResumePos` on `WindowTab`; pause via `ReadAloudStopRememberPos`
- **Stops on:** tab close, window close, document unload (`StopReadAloudIfSourceTab`, `ResetReadAloudStateForTab`)
- **Copy permission:** read aloud silently no-ops when `Perm::CopySelection` is denied
- **TTS:** `TextToSpeech.cpp` — WinRT Speech Synthesis with SAPI fallback; voice id session-only
- **Docs:** `docs/md/Accessibility-and-Text-to-Speech.md`, version history 3.7 entry