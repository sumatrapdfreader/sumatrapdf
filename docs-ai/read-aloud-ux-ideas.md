# Read Aloud UX ideas for SumatraPDF

UX-focused analysis of the Read Aloud feature (pre-release 3.7+) and proposals for improving discoverability, controls, and listening workflows. No implementation details.

*Last updated after playback bar, Stop Reading, auto-scroll, and menu polish (master, June 2026).*

## Current experience (as implemented today)

### Entry points

| Where | What users see |
|-------|----------------|
| **Toolbar** | Speak button — main click = smart start / pause / continue (tooltip reflects next action); dropdown arrow = full Read Aloud submenu |
| **Main menu** | **Read Aloud (TTS)** submenu after Selection |
| **Context menu** | **Read Aloud (TTS)** after Document; includes **Start Reading From Cursor Position** (right-click only) |
| **Command palette** (`Ctrl+K`) | Read Aloud; Pause Reading; Continue Reading; Stop Reading; Start Reading From Top; Start Reading Selection |
| **Playback bar** | Thin strip at the **bottom of the canvas** while a session is active (speaking or paused) |

All three menus share one builder (`BuildReadAloudMenuItems`): when speaking → **Pause Reading** + **Stop Reading**; when paused → **Continue Reading** + **Stop Reading**; then explicit start commands and **Voice →** (System default + installed voices).

### Playback bar (shipped)

While a read-aloud session is active (speaking or paused):

```
[Pause] [Stop]   Reading · Annual Report.pdf · page 12 of 240 · Smart start
```

- **Pause / Resume** — left-side buttons; label switches to Resume when paused.
- **Stop** — ends the session and clears resume state (distinct from pause).
- **Status line** — document name, **page X of Y** (from highlight map + spoken offset), and **scope** label: Smart start, From top, From cursor, or Selection.
- **Placement** — bottom of canvas, full width minus margin; relayouts on resize.
- **Visibility** — shown on the window that owns the session; hidden on other SumatraPDF windows while audio may still play there.

The bar is the primary **session control** surface. Menus and palette duplicate transport actions for keyboard-first users.

### What works well

- **Multiple explicit start scopes** — selection, first visible text in viewport (“From Top”), cursor position (context menu), plus smart default on toolbar / `CmdReadAloud` (selection if present, else viewport).
- **Continuous listening** — viewport / cursor / smart builds read from the start point through the **end of the document**; selection reads selection only. 1 KB TTS chunks chain automatically.
- **Word follow-along** — current spoken word highlighted on the canvas (selection color; timer-driven repaint).
- **Auto-scroll** — viewport follows the spoken word while reading; **stops permanently** if the user scrolls the highlight fully off-screen (respects manual navigation).
- **Resume** — pause remembers position; Continue Reading picks up in the same tab.
- **Stop** — explicit end-of-session in playback bar, all three menus, and command palette (`CmdStopReadAloud`).
- **Session chrome** — playback bar answers “what is playing, from where, how far” at a glance.
- **Discoverability** — menubar, context menu, toolbar dropdown, palette, and playback bar.
- **Consistent naming** — Pause Reading, Continue Reading, Stop Reading, Start Reading From Top (renamed from “From Here”).
- **Localized menu strings** — `_TRA` / `_TRN` on menu labels.
- **Voice remembered** — `ReadAloudVoiceId` advanced setting persists Voice submenu choice.
- **Sensible text cleanup** — soft hyphens and line breaks normalized before speech.
- **Backend choice** — WinRT Speech Synthesis with SAPI fallback.
- **User docs updated** — `Accessibility-and-Text-to-Speech.md` describes continuous reading, playback bar, menus, and scopes.

### Remaining pain points

| Issue | Why it hurts |
|-------|----------------|
| **Toolbar button still triple-duty** | One click = start *or* pause *or* continue. Tooltip helps but users must read it; no Stop on main click. Icon switches to “stop speaking” glyph while the action is still **Pause** — visual mismatch. |
| **Toolbar vs session tab** | `CmdReadAloud` uses **current tab** for continue/smart start; audio and highlight follow **source tab**. Paused on tab A, switch to tab B → toolbar offers “Read Aloud” (new start) while the bar still shows tab A — easy to orphan a paused session or start conflicting audio. |
| **Two mental models for “start”** | Smart (`CmdReadAloud`) vs explicit menu items (viewport-only, selection-only, cursor). Power users benefit; casual users may not know which to pick. Scope label on the bar helps *after* start, not before. |
| **“From Top” wording** | Menu says **Start Reading From Top**; bar says **From top**. Same behavior (first visible text → end) but labels differ slightly. |
| **Global audio, per-window bar** | TTS is app-global; playback bar only appears on the session window. Another window has no bar — only a changing toolbar tooltip if that window is focused. |
| **Tab switch within one window** | Bar stays visible at the bottom while the canvas shows another tab’s pages. Filename on the bar clarifies the source, but highlight is on the source tab’s page (not visible) — listening without follow-along until user returns to that tab. |
| **Starting a new scope while paused** | Explicit start commands stop active TTS but may not fully replace an existing paused session on another tab — edge case feels ambiguous without a confirm dialog. |
| **Silent permission failure** | Copy-restricted PDFs block read aloud with **no user message** when `Perm::CopySelection` is denied. |
| **Highlight not configurable** | Follow-along always uses selection color; no toggle, dedicated read-aloud color, or disable. |
| **No speech rate control** | Windows voices speak at system rate only; no in-app slow/normal/fast. |
| **Playback bar has no Voice / settings** | Voice changes require opening a menu or palette; no ⚙ on the bar (deferred in original mock). |
| **Scanned PDF dead-end** | “No text available to read aloud” toast with no OCR / next-step affordance. |
| **Palette Stop always listed?** | `CmdStopReadAloud` lacks palette context gating (unlike Pause/Continue); may appear when no session is active. |
| **No default keyboard shortcut** | Everything reachable via palette/menus but nothing bound in default accelerators for “start reading” or “pause”. |
| **Context menu placement** | **Read Aloud (TTS)** is a sibling submenu after Document, not inline near **Copy** / selection actions. |

## What changed since the first UX pass

| Original gap | Status |
|--------------|--------|
| Hard to discover (palette-only) | **Fixed** — menubar, context menu, toolbar dropdown |
| Page-only, no “keep going” | **Fixed** — reads through last page |
| No follow-along highlight | **Shipped** — word highlight; toggle/color still missing |
| No playback chrome | **Fixed** — playback bar with page progress and scope |
| No Stop, only Pause | **Fixed** — Stop in bar, menus, palette |
| No auto-scroll | **Fixed** — follows spoken word; user scroll-away disables |
| Dropdown strings not localized | **Fixed** |
| Palette naming inconsistent | **Fixed** |
| Voice list cluttering main menu | **Fixed** — **Voice** submenu |
| Read from cursor / viewport | **Fixed** — viewport + context-menu cursor item |
| Voice not remembered | **Fixed** — `ReadAloudVoiceId` |
| “From Here” label unclear | **Improved** — renamed **Start Reading From Top**; help table updated |
| User docs lag code | **Fixed** — Accessibility page brought current |
| Cursor start disabled wrongly | **Fixed** — same text-hit logic as I-beam cursor |

## Design principle (unchanged)

Treat Read Aloud as **listening mode** layered on normal reading — not a hidden palette command. Users should see: *what* is reading, *how* to control it, and *how far* through the content they are.

The playback bar delivers this with minimal chrome. Next polish should reduce **toolbar ambiguity** and **cross-tab confusion**, not add modals or permanent toolbar widgets.

## Recommended improvements (by priority)

### 1. Align toolbar with session model (highest impact remaining)

The playback bar fixed visibility; the toolbar is now the main source of confusion.

| Proposal | Rationale |
|----------|-----------|
| **Main click = pause/resume only when a session exists** on current or global session tab; otherwise smart start | Avoid surprise “new start” over a paused session on another tab |
| **Separate Stop** on toolbar (split button, long-press, or right-click) **or** rely on bar/menus only but fix the icon | Today’s stop-speaking icon implies Stop; action is Pause |
| **Toolbar tooltip shows source** when session tab ≠ current tab | e.g. “Continue Reading · Report.pdf” |
| **Disable smart start** while any session is paused, with “Stop or Continue first” toast | Prevents orphaned `readAloudText` on inactive tabs |

### 2. Cross-tab and multi-document policy

Define rules users can predict; reflect them in bar + toolbar:

| Event | Suggested behavior |
|-------|-------------------|
| Switch tab while reading | Keep playing; bar shows source filename; optional “Switch to reading tab” link/button on bar |
| Switch tab while paused | Bar stays; Continue in menu operates on session tab, not current tab (or switch tab automatically on Continue) |
| Start read on tab B while A paused/playing | Confirm: “Stop reading *A* and start *B*?” — today partial/overlapping state is possible |
| Close tab that’s reading | Stop + confirm if mid-document |
| Second SumatraPDF window | Show compact bar or notification in focused window: “Reading in other window · Report.pdf” |

### 3. Playback bar polish (incremental)

| Addition | Notes |
|----------|-------|
| **⚙ Voice** (or chevron) on bar | Opens Voice submenu without leaving canvas — matches original mock |
| **Click filename** | Switch to session tab / scroll to spoken word |
| **Stronger button affordance** | Slightly higher contrast vs notification background; optional icons ⏸ ⏹ for scanability |
| **Unify scope strings** | Bar: “From top of page” or match menu exactly: “Start Reading From Top” → shorten to **Viewport** / **Top of view** |
| **Paused state in status** | “Paused · …” prefix so line does not look identical to active reading |

### 4. Clarify start commands for first-time users

| Label | Behavior |
|-------|----------|
| **Read Aloud** (toolbar / palette) | Smart: selection if present, else viewport top → end of document |
| **Start Reading From Top** | Viewport top → end of document |
| **Start Reading From Cursor Position** | Right-click point → end (context menu) |
| **Start Reading Selection** | Selection only; does not continue past selection |

**Tooltip on toolbar dropdown items** (one line each) would help more than renaming again. Consider a short in-app tip the first time Read Aloud runs.

### 5. Context menu: Read Aloud near Copy (optional)

When text is selected:

- Inline **Start Reading Selection** under **Copy** (disabled when no selection).
- Keep full **Read Aloud (TTS)** submenu for voice, viewport start, cursor, transport.

Mirrors how **Translate** sits in the selection flow.

### 6. Settings and highlight control

In **Advanced Options** (or playback-bar ⚙):

| Setting | Purpose |
|---------|---------|
| ~~Read aloud voice~~ | **Done** — `ReadAloudVoiceId` |
| Speech rate | Slow / Normal / Fast if backend exposes it |
| Highlight while reading | On/off (default on) |
| Highlight color | Separate from selection color |
| Auto-scroll to spoken word | On/off (default on; today always on until user scrolls away) |

### 7. Clearer empty and blocked states

| Situation | Target message |
|-----------|----------------|
| No extractable text | “No text on this page.” → **[Find text on next page]** if feasible |
| Copy permission denied | “This document doesn’t allow copying text.” (today: silent) |
| No voices installed | Link to Windows **Settings → Speech** |
| Session conflict | “Already reading *X.pdf* — Stop or Continue first.” |

Keep toasts short; one primary action each.

### 8. Command palette context

| Command | Show when |
|---------|-----------|
| Pause Reading | `TtsIsSpeaking()` |
| Continue Reading | paused session on current or global session tab |
| Stop Reading | any active session (speaking or paused) |
| Start commands | document loaded; not redundant with transport when speaking |

Hiding Stop when idle reduces palette noise.

### 9. Default keyboard shortcut

Bind a discoverable shortcut (e.g. `Ctrl+Shift+S` toggle pause/resume, or `Ctrl+Shift+R` start) in default accelerators; document in Keyboard shortcuts. Custom rebinding already supported.

### 10. Selection scope expectations

**Start Reading Selection** does not continue past the selection (by design). Users may expect “read my highlight then keep going.” Options:

- Rename to **Read Selection Only** / add subtitle in menu.
- Or add **Read Selection, Then Document** as an advanced scope (larger lift).

Call out the difference in help and first-run tip.

## What to defer

- Full audiobook features (bookmarks, sleep timer, MP3 export)
- Neural / cloud voices (stay on Windows installed voices)
- Per-paragraph voice or SSML editor
- Replacing Narrator / NVDA for blind users (document limitations in help)
- Floating / undocked playback widget

## Suggested user story (one sentence)

*“Open a document, start Read Aloud from the toolbar or right-click menu, choose where to begin (viewport, selection, or cursor), pick a voice under Voice, and listen through the document with the current word highlighted and the page scrolling along — use the playback bar or menu to pause, resume, or stop, and see which page you’re on.”*

## Quick wins vs larger lifts (revised)

| Quick wins | Larger lifts |
|------------|----------------|
| Fix toolbar icon (speak vs pause vs stop semantics) | Cross-tab session policy + confirm on conflict |
| Palette gating for Stop / Continue | “Reading in other tab” bar action |
| Toast when copy permission blocks TTS | Voice ⚙ on playback bar |
| Unify bar scope label with menu wording | Speech rate in Advanced Options |
| “Paused ·” prefix on bar status line | Highlight on/off + color setting |
| Menu tooltips for start scopes | Inline **Start Reading Selection** under Copy |
| Default keyboard shortcut | OCR / next-page path on no-text |
| Bar button contrast / optional icons | **Read selection then continue** scope |

The largest **remaining** gap is **session coherence across tabs and toolbar**: users can see *what* is playing (playback bar), but the toolbar and smart-start path can still act on the wrong tab or leave orphaned pause state.

## Related implementation map (for developers)

- **Commands:** `CmdReadAloud`, `CmdPauseReadAloud`, `CmdContinueReadAloud`, `CmdStopReadAloud`, `CmdReadAloudFromTopPage`, `CmdReadAloudSelection` — palette; no default shortcut
- **TTS menu ids:** `CmdTtsMenuReadCurrentPage`, `CmdTtsMenuContinueReading`, `CmdTtsMenuReadSelection`, `CmdTtsMenuPauseReading`, `CmdTtsMenuReadFromCursor`, `CmdTtsMenuStopReading`, `CmdTtsVoice*`
- **Menus:** `BuildReadAloudMenuItems` / `RebuildReadAloudMenu` in `SumatraPDF.cpp`; menubar + context in `Menu.cpp`
- **Toolbar:** speak + dropdown (`ShowTtsVoiceMenu`); dynamic tooltip/icon in `Toolbar.cpp`
- **Playback bar:** `ReadAloudPlaybackBar.cpp` — `ReadAloudPlaybackBarUpdateSession`, `ReadAloudPlaybackPauseOrResume`, `ReadAloudPlaybackStop`
- **Scope:** `WindowTab::readAloudScope` — Smart, Viewport, Selection, Cursor; set in `ReadAloudInTab`, `ReadAloudFromViewportTopInTab`, etc.
- **Sessions:** `gReadAloudSessionTab` (persists through pause), `gReadAloudSourceTab` (active TTS)
- **Progress:** `ReadAloudGetProgressPage` in `ReadAloudHighlight.cpp`
- **Auto-scroll:** `ReadAloudUpdateAutoScroll`, `ReadAloudOnUserViewChanged` — disables on user scroll-away
- **Document text:** `ReadAloudHighlightBuildFromDocument`, `ReadAloudGetViewportStart`, `ReadAloudGetCursorStart`
- **Chunking:** `kReadAloudMaxChunkLen = 1024`, `ReadAloudSpeakChunk`, `WM_TTS_EVENT` chaining
- **Highlight:** `PaintReadAloudHighlight`, highlight timer in `ReadAloudHighlight.cpp`
- **Resume:** `readAloudText`, `readAloudResumePos`, chunk fields on `WindowTab`
- **Copy permission:** silent return in read-aloud entry paths when `Perm::CopySelection` denied
- **TTS:** `TextToSpeech.cpp` — WinRT + SAPI; `ReadAloudVoiceId` in settings
- **User docs:** `docs/md/Accessibility-and-Text-to-Speech.md`, `docs/md/Commands.md`, version-history 3.7 entry