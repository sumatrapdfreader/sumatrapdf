# OCR UX ideas for SumatraPDF

UX-focused proposal for adding OCR to SumatraPDF. Fits existing patterns: lightweight reader, command palette for newer features, notifications for feedback, copy/find/read-aloud as the main text workflows. No implementation details.

## Design principle

OCR in SumatraPDF should not feel like a separate “OCR app.” It should **unlock text the user already expects**: select, copy, find, read aloud, and screen-reader access. The mental model is **“recognize text”**, not “run OCR.”

Results should behave like normal PDF text: invisible overlay, I-beam cursor, existing shortcuts unchanged. No dedicated OCR panel or correction editor — that’s out of character for SumatraPDF.

## What to offer (three scopes)

| Scope | User intent | When to surface |
|-------|-------------|-----------------|
| **Selection / region** | “Copy this paragraph from a scan” | User drags a rectangle on a page with no text |
| **Current page** | “I need this one page searchable/copyable” | Context menu, palette, or after a failed copy on one page |
| **Whole document** | “Make this scanned PDF usable” | Palette, Edit menu, or prompt on open — with a clear time warning |

**Default recommendation:** ship **page** and **selection** first; **whole document** as an explicit, confirmed action (progress UI, cancel). Region OCR is especially valuable because scans are often mixed (cover + text pages) and full-doc OCR is slow.

**Session-only by default:** recognized text lives for this open document until close. No “Save OCR’d PDF” in v1 — keeps the reader/editor boundary clean. Export-to-text-file can be a later, optional command.

## When to suggest it (progressive disclosure)

Don’t auto-OCR on open (CPU, privacy, wrong language). **Detect** likely scanned content (`HasTextForPage` is already the signal) and nudge at the moment of need:

1. **On open (once per document):** a dismissible notification:
   *“This document has little selectable text. Recognize text to enable search and copy?”*
   Actions: **Recognize on this page** · **Whole document…** · **Don’t ask again for this file**

2. **When Find is used** on a no-text page: enable Find but show in the find bar: *“No text on this page — [Recognize text]”*

3. **When Copy / Select All** yields only an image (today copy falls back to image):
   *“No text here. Recognize text on this page?”* with the same action link

4. **Read Aloud** already says “No text available…” — extend that path with **Recognize text, then read** (one combined flow, not two separate concepts)

These moments teach the feature without adding permanent chrome.

## Where to put it in the UI

### Primary (discoverable)

- **Command palette** (`Ctrl+K`): **“Recognize Text”** with sub-actions or a second step: *This page* / *Selection* / *Whole document…*
  Same discovery pattern as Read Aloud and Advanced Options.

- **Edit menu** (or **View** if Edit is crowded): one item **“Recognize Text”** → submenu matching the three scopes. Keeps the menu bar honest for users who don’t use the palette.

### Contextual (high intent)

- **Right-click on page** (no text under cursor): **“Recognize Text on This Page”** — only when the page has no text layer.

- **Right-click on rectangular selection** (image-only area): **“Recognize Text in Selection”** — sits near Copy, above or below it.

- **Find toolbar / find box** — inline link when search can’t run on current page (mirrors how image collections already disable Find in the menu).

### Do *not* add (v1)

- Toolbar button (too niche for permanent space; Read Aloud earned its slot as accessibility)
- Status-bar mode toggle
- Separate “OCR layer” preview

## Progress and feedback

Use the same family as printing notifications:

- Non-modal toast: *“Recognizing text… page 3 of 47”* with **Cancel**
- On completion: *“Text recognized on 12 pages. You can now search and copy.”* (auto-dismiss)
- On failure: short message + retry; no modal error storm

Optional subtle cue: after OCR, pages could show a one-time tooltip on first successful text selection — *“Recognized text (session only)”* — so power users know it isn’t embedded in the file.

## Settings (Advanced Options)

Keep OCR mostly invisible; expose only what affects behavior:

- **OCR languages** (multi-select, default from system/UI language)
- **Prompt when opening scanned PDFs** (on / off)
- **Recognize text in background while reading** (off by default) — only if whole-document OCR exists; otherwise skip

No tessdata paths or engine knobs in the main UI.

## How it plugs into existing features

After recognition, **nothing new to learn**:

| Existing feature | Behavior after OCR |
|------------------|-------------------|
| `Ctrl+F` / Find | Works across recognized pages |
| `Ctrl+C` / Copy | Text, not image fallback |
| `Ctrl+A` / Select All | Page text as usual |
| Read Aloud | Same commands; no special “OCR read” mode |
| Screen reader / UIA | Benefits from text layer like a born-digital PDF |
| Image collections (CBZ, etc.) | Optional: per-image recognize, or leave disabled with a clear “not supported” — same as Find today |

**Mixed PDFs:** only run OCR on pages without text; don’t re-OCR pages that already have a layer.

## What to defer

- In-app OCR correction / proofreading UI
- Embedding text into the PDF and Save
- Batch folder OCR
- Command-line OCR flags (unless power users ask later)
- “OCR to HTML/SVG” export (MuPDF tool territory, not reader UX)

## Suggested v1 story (one sentence for users)

*“If a scan has no selectable text, choose Recognize Text — then search, copy, and read aloud work like a normal PDF, for this session.”*

That stays aligned with SumatraPDF: small surface area, palette + contextual prompts, and making existing commands work instead of shipping a new subsystem.

## Related existing behavior

- `HasTextForPage()` — no glyph hit-testing when page has no text (`DisplayModel.cpp`)
- Image collections — Find disabled (`Menu.cpp`); selection/copy limited (`Selection.cpp`, `SearchAndDDE.cpp`)
- Read aloud — shows “No text available…” when page lacks text (`SumatraPDF.cpp`)
- Copy on restricted/no-text pages — may copy as image with notification (`Selection.cpp`)
- MuPDF has tessocr integration (`mupdf/source/fitz/tessocr.cpp`); tool-draw docs mention OCR output formats but mark some as disabled