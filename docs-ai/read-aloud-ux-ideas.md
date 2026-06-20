# Read Aloud UX ideas for SumatraPDF

UX-focused analysis of the Read Aloud feature (pre-release 3.7+) and proposals for improving discoverability, controls, and listening workflows. No implementation details.

*Last updated after continuous reading, highlight, menu/palette work (master, June 2026).*

## Current experience (as implemented today)

### Entry points

| Where | What users see |
|-------|----------------|
| **Toolbar** | Speak button (dropdown) — tooltip switches between Read Aloud / Pause Reading / Continue Reading |
| **Main menu** | **Read Aloud (TTS)** submenu after Selection |
| **Context menu** | **Read Aloud (TTS)** after Document; includes **Start Reading From Cursor Position** (right-click only) |
| **Command palette** (`Ctrl+K`) | Read Aloud; Pause Reading; Continue Reading; Start Reading From Here; Start Reading Selection |

All three menus share one builder: transport items, explicit start commands, and a **Voice →** submenu (System default + installed voices).

### What works well

- **Multiple explicit start scopes** — selection, first visible line in viewport, cursor position (context menu), plus a smart default on the toolbar / `CmdReadAloud` (selection if present, else viewport).
- **Continuous listening** — viewport / cursor / document builds read from the start point through the **end of the document**, with 1 KB TTS chunks chained automatically for faster speech start.
- **Word follow-along** — current spoken word is highlighted on the canvas (reuses selection color; timer-driven repaint).
- **Resume** — pause remembers position; Continue Reading picks up in the same tab.
- **Discoverability improved** — no longer palette-only; menus and toolbar expose the feature.
- **Consistent naming** — palette verbs match menu items (Pause Reading, Continue Reading, Start Reading …).
- **Localized menu strings** — `_TRA` / `_TRN` on menu labels.
- **Sensible text cleanup** — soft hyphens and line breaks normalized before speech.
- **Backend choice** — WinRT Speech Synthesis with SAPI fallback.

### Remaining pain points

| Issue | Why it hurts |
|-------|----------------|
| **No playback chrome** | While reading, nothing persistent shows *what* is playing, *which file/tab*, or *how far* through the document. Highlight helps on-page but is easy to miss. |
| **Toolbar button still triple-duty** | One click = start *or* pause *or* continue. Users must read the tooltip to know the next action. |
| **“From Here” may need a tooltip** | **Start Reading From Here** starts at the **first visible text line in the viewport** and reads to the document end — correct label, but first-time users may still want a one-line explanation in help. |
| **Two mental models for “start”** | `CmdReadAloud` (smart: selection → else viewport) vs explicit menu items (viewport-only, selection-only, cursor). Power users benefit; casual users may not know which to pick. |
| **No Stop, only Pause** | Pause retains resume state; there is no explicit **Stop** that ends the session and clears position. Starting a new scope while paused can feel ambiguous. |
| **Global audio, local UI** | TTS is app-global; toolbar on any window shows Pause while audio may be from another tab. `gReadAloudSourceTab` exists internally but is not surfaced. |
| **Silent permission failure** | Copy-restricted PDFs block read aloud with **no user message** when `Perm::CopySelection` is denied. |
| **Voice not remembered** | Voice choice is session-only (`gTtsVoiceId`), not in Advanced Options. |
| **Highlight not configurable** | Follow-along always uses selection color; no toggle, no dedicated read-aloud color, no setting to disable. |
| **No auto-scroll** | Long-document listening does not scroll the viewport to keep the spoken word in view. |
| **Scanned PDF dead-end** | “No text available to read aloud” toast with no OCR / next-step affordance. |
| **User docs lag code** | `Accessibility-and-Text-to-Speech.md` still describes page-only scope and old palette-only discovery. |

## What changed since the first UX pass (addressed)

| Original gap | Status |
|--------------|--------|
| Hard to discover (palette-only) | **Fixed** — menubar, context menu, toolbar dropdown |
| Page-only, no “keep going” | **Fixed** — `ReadAloudHighlightBuildFromDocument` reads through last page |
| No follow-along highlight | **Partially fixed** — word highlight ships; no user toggle yet |
| Dropdown strings not localized | **Fixed** |
| Palette naming inconsistent | **Fixed** — Pause/Continue/Start Reading … |
| Voice list cluttering main menu | **Fixed** — **Voice** submenu in all contexts |
| Read from cursor / viewport | **Fixed** — viewport start + context-menu cursor item |

## Design principle (unchanged)

Treat Read Aloud as **listening mode** layered on normal reading — not a hidden palette command. Users should see: *what* is reading, *how* to control it, and *how far* through the content they are.

Keep SumatraPDF’s minimal chrome: one lightweight playback strip still beats a modal or a permanent toolbar widget.

## Recommended improvements (by priority)

### 1. Add a small Read Aloud bar (highest impact — still missing)

When speech starts, show a **thin non-modal bar** above the canvas (same family as notifications / print progress):

```
🔊 Reading · Annual Report 2024.pdf · page 12     ⏸  ⏹  ⚙
```

- **Pause / Resume** — explicit buttons; stop overloading the toolbar icon.
- **Stop** — ends playback and clears resume state (distinct from pause).
- **Scope label** — “Selection”, “From viewport”, “From cursor”, or page estimate.
- **Source tab** — filename when reading continues after tab switch.
- **Progress hint** — optional “page 12 of 240” derived from highlight map / spoken offset.

Dismiss on stop or natural end. Toolbar speak button remains a **shortcut into** this bar, not the only control surface.

### 2. Clarify start commands in help (rename done)

Viewport start is labeled **Start Reading From Here** everywhere (menus, palette). Remaining polish:

| Label | Behavior |
|-------|----------|
| Start Reading From Here | First visible text line → end of document |
| Start Reading From Cursor Position | Right-click position → end of document |
| Start Reading Selection | Selection only (no auto-continue past selection) |
| Read Aloud (toolbar / palette) | Smart start: selection if present, else viewport → end |

Update `Accessibility-and-Text-to-Speech.md` and tooltips so first-time users understand “here” = visible viewport text.

### 3. Split toolbar click from dropdown (partially done)

| Action | Control today | Target |
|--------|---------------|--------|
| Start / pause / resume | Main speak button | Keep, but add playback bar buttons |
| Explicit scopes | Dropdown + menus + palette | **Done** |
| Voice | Voice submenu | **Done** |

Remaining work: dropdown should not be the only place users learn that multiple start modes exist — playback bar scope label + one-line help text would help.

### 4. Put Read Aloud closer to Copy (optional polish)

Context menu today: **Read Aloud (TTS)** is a sibling submenu after Document, not inline near **Copy**.

Consider:

- A single **Start Reading Selection** item directly under Copy when text is selected (disabled otherwise).
- Keep the full **Read Aloud (TTS)** submenu for voice + other scopes.

Matches how Translate sits in the selection flow.

### 5. Playback settings and highlight control

In **Advanced Options**:

| Setting | Purpose |
|---------|---------|
| Read aloud voice | Persist choice across sessions |
| Speech rate | Slow / Normal / Fast if backend exposes it |
| Highlight while reading | On/off (default on) |
| Highlight color | Separate from selection color |
| Auto-scroll to spoken word | On/off for long documents |

### 6. Clearer empty and blocked states

Replace bare toasts with actionable messages:

- *“No text on this page.”* → **[Recognize text]** (when OCR exists) · **Read next page with text**
- *“This document doesn’t allow copying text.”* — when copy permission blocks TTS (today: silent)
- *“No speech voices installed.”* → link to Windows **Settings → Speech**

Keep toasts short; one primary action per message.

### 7. Tab and document lifecycle

Define explicit rules and reflect them in the playback bar:

| Event | Suggested behavior |
|-------|-------------------|
| Switch tab while reading | Keep playing; bar shows source file; ⏹ stops |
| Close tab that’s reading | Stop + confirm if mid-document |
| Open new file in same tab | Stop (already resets on unload) |
| Start read on tab B while A is playing | Ask: “Stop reading *A* and start *B*?” |

Avoid a global pause button on the wrong tab with no context.

### 8. Default keyboard shortcut

Bind a discoverable shortcut (e.g. `Ctrl+Shift+S` for speech) in default accelerators, documented in Keyboard shortcuts. Custom rebinding already supported via Advanced Options.

### 9. Update user-facing documentation

Bring these in line with behavior:

- `docs/md/Accessibility-and-Text-to-Speech.md` — continuous reading, menus, highlight, Voice submenu
- `docs/md/Commands.md` — new palette commands (`CmdReadAloudFromTopPage`, `CmdReadAloudSelection`)
- Version-history prose for the next release (not a stream of `add Cmd…` bullets)

## What to defer

- Full audiobook features (bookmarks, sleep timer, MP3 export)
- Neural / cloud voices (stay on Windows installed voices)
- Per-paragraph voice or SSML editor
- Replacing Narrator / NVDA for blind users (document that in help)

## Suggested user story (one sentence)

*“Open a document, use Read Aloud from the toolbar or right-click menu, pick where to start (viewport, selection, or cursor), choose a voice under Voice, and listen through the rest of the document — with the current word highlighted as it is spoken. Use Pause Reading to take a break and Continue Reading to resume.”*

## Quick wins vs larger lifts (revised)

| Quick wins | Larger lifts |
|------------|----------------|
| Refresh Accessibility help for “From Here” | Playback bar |
| Notification when copy permission blocks TTS | Auto-scroll while reading |
| Persist voice in Advanced Options | Tab-switch / multi-tab policy UI |
| Highlight on/off + color setting | OCR integration on no-text path |
| Default keyboard shortcut | Stop vs Pause in all surfaces |
| Refresh Accessibility help page | Progress indicator (page X of Y) |

The biggest **remaining** gap versus user expectations is **visibility and session control**: people can find the feature now, but still cannot tell at a glance what is playing, from which tab, or how to stop cleanly without inferring from a changing toolbar tooltip.

## Related implementation map (for developers)

- **Commands:** `CmdReadAloud`, `CmdPauseReadAloud`, `CmdContinueReadAloud`, `CmdReadAloudFromTopPage`, `CmdReadAloudSelection` — palette; no default shortcut
- **TTS menu ids:** `CmdTtsMenuReadCurrentPage`, `CmdTtsMenuReadSelection`, `CmdTtsMenuReadFromCursor`, pause/continue, `CmdTtsVoice*` — dynamic menus only
- **Menus:** `BuildReadAloudMenuItems` / `RebuildReadAloudMenu` in `SumatraPDF.cpp`; menubar + context hooks in `Menu.cpp`
- **Toolbar:** speak + dropdown (`ShowTtsVoiceMenu`); dynamic tooltip in `Toolbar.cpp`
- **Scope:** `ReadAloudInTab` (smart), `ReadAloudFromViewportTopInTab`, `ReadAloudSelectionInTab`, `ReadAloudFromCursorInTab`
- **Document text:** `ReadAloudHighlightBuildFromDocument`, `ReadAloudGetViewportStart`, `ReadAloudGetCursorStart`
- **Chunking:** `kReadAloudMaxChunkLen = 1024`, `ReadAloudSpeakChunk`, `WM_TTS_EVENT` chaining
- **Highlight:** `PaintReadAloudHighlight`, `ReadAloudHighlightTimerStart/Stop` in `ReadAloudHighlight.cpp`
- **Resume:** `readAloudText`, `readAloudResumePos`, chunk fields on `WindowTab`
- **Source tab:** `gReadAloudSourceTab` / `GetReadAloudSourceTab()`
- **Copy permission:** silent return in read-aloud entry paths when `Perm::CopySelection` denied
- **TTS:** `TextToSpeech.cpp` — WinRT + SAPI; voice id session-only
- **Docs to update:** `docs/md/Accessibility-and-Text-to-Speech.md`, `docs/md/Commands.md`