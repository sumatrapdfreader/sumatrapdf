# Merging from sumatrapdf-plus fork

Tracking selective merges from https://github.com/dengxibo/sumatrapdf-plus (a
Chinese-community fork "Sumatra PDF Plus", GPLv3, same license as upstream).

## Fork layout / state

- Remote is added locally as `plus` (`git remote add plus https://github.com/dengxibo/sumatrapdf-plus.git`).
- All work is on `plus/main`. Branches `plus/sumatrapdf-plus`, `plus/feature/read-aloud-tts`,
  `plus/feature/smart-dark-preview` are stale ancestors of `main` — ignore them.
- **Fork point:** `2854c78d6` ("re-register OpenWithProgids at startup", 2026-05-21).
- Fork scope (as of last review): **89 commits, 193 files, +61,677 / −20,075 lines**
  (a chunk of that is machine-generated translations and a whitespace-only
  `mupdf/source/fitz/font.c` reformat).
- Our master has **~1,600 commits** since the fork point, so a plain `git merge` is
  not viable. We port selectively (see "Merge process" below).
- They do use our code-gen system (`cmd/gen-settings.ts`, gen-commands) — good; but
  their base predates recent gen-code.ts changes, so **regenerate, never take their
  generated `Settings.h/cpp`, `Commands.h/cpp` diffs verbatim**.

**Last reviewed commit on `plus/main`: `37bba7a39`** ("Release 3.7.8", 2026-07-10).
Update this marker after every review pass.

## Summary of fork changes, by feature

### A. Read Aloud (TTS) — largest feature
Windows SAPI text-to-speech with word-by-word highlighting synced to speech; works on
PDF/EPUB/MOBI. Start from top of page / cursor / selection; pause/continue; speed
presets 0.25×–2.0×; separate Chinese/English rates; "smart bilingual" voice switching
(auto-picks a Chinese voice for CJK runs and an English voice otherwise), both local
SAPI and online voices; toolbar button with dropdown, menu, context menu; double-click
on blank area toggles read-aloud. Recommends NaturalVoiceSAPIAdapter for natural voices.
- New files: `src/TextToSpeech.{cpp,h}`, `src/ReadAloudHighlight.{cpp,h}`,
  `docs/md/Read-Aloud-TTS.md`, `tools/gen_read_aloud_trans.py`
- Integration spread through `SumatraPDF.cpp`, `Toolbar.cpp`, `Menu.cpp`, `Canvas.cpp`,
  `DisplayModel.cpp`, `TextSelection.cpp`
- Settings: `ReadAloudVoiceId`, `ReadAloudSpeakingRate` (legacy),
  `ReadAloudSpeakingRateZh/En`, `ReadAloudSmartVoiceZh/En`, `ReadAloudSmartOnlineVoiceZh/En`
- Commands: `CmdReadAloud`, `CmdReadAloudFromCursor`, `CmdReadAloudFromTopPage`,
  `CmdReadAloudSelection`, `CmdPauseReadAloud`, `CmdContinueReadAloud`, `CmdStopReadAloud`
- Key commits: `95812be62`, `95d695b1d`, `4aa346bf5`, `48753162f`, `b4d2fdd06`,
  `4a6a9ece3`, `5a6c285e4`, `2d3684974`, `270f677d3`, `4e5363143` (partly)
- Note: we have our own ideas in `ai/read-aloud-ux-ideas.md` — compare before porting.

### B. Offline dictionary / word lookup
Double-click a word (or "Look Up Selection") to show a floating dictionary popup from
offline dictionaries (`SumatraDict.*` `.idx`/`.dat` files in `{exedir}/dict` or
`OfflineDictionaryPath`); phonetic transcription for multi-sense entries; optional
pronunciation audio; "Search with Youdao Dict" command.
- New files: `src/WordLookup.{cpp,h}` (~2,400 lines), `src/LookupAudio.{cpp,h}`,
  `src/FloatingPopupStyle.{cpp,h}`, dict import scripts `cmd/import-oaldpex-dict.ts`,
  `cmd/import-xdhy7-dict.ts`, `tools/gen_lookup_*_trans.py`
- Settings: `OfflineDictionaryPath`, `EnableDoubleClickWordLookup`
- Commands: `CmdLookupSelection`, `CmdToggleDoubleClickWordLookup`,
  `CmdSearchSelectionWithYoudaoDict`
- Key commits: `f1722fbb3`, `399d52b45`, `649b31371`, `65b413ab1` (audio optional)
- Note: dictionary *data* is not in the repo (users supply it); import scripts convert
  OALD / xdhy7 sources.

### C. Ask AI (Doubao / DeepSeek / ChatGPT)
Select text → "Ask AI" opens the provider's web chat in the default browser and pastes
the selection (clipboard + simulated paste; several commits fight with cold-start/paste
targeting per provider). Provider picker; can be fully hidden.
- Settings: `AiChatProvider` (doubao/deepseek/chatgpt), `EnableAskAI`,
  `AiChatUseDeepSeekInsteadOfDoubao` (deprecated migration shim)
- Commands: `CmdAnalyzeSelectionWithDoubao`
- Key commits: `df38d716d`, `440ed7c7c`, `c578c0cf4`, `ead418671`, `39df9ba19`, `4c805377d`
- Assessment: China-centric providers, brittle browser-paste automation.

### D. Smart PDF dark mode
A real dark-mode *rendering* pipeline for PDFs (not just invert): intercepts drawing via
a custom fitz device, recolors text/vector content using OKLab color math, classifies
images (photos vs. line-art/scans) to preserve photos and page-1 covers while darkening
scanned/line art; per-document color mode Auto / Black (full dark) / Light (original)
with toolbar buttons; render + engine caches; unit tests included. "Legacy" inversion
kept as default renderer initially.
- New files (~3,500 lines): `src/PdfDarkMode.h`, `PdfDarkModeAnalysis.cpp`,
  `PdfDarkModeCache.cpp`, `PdfDarkModeColor.cpp`, `PdfDarkModeDevice.cpp`,
  `PdfDarkModeEngineCache.cpp`, `PdfDarkModeImageBgBlend.cpp`,
  `PdfDarkModeImageClassifier.cpp`, `PdfDarkModeImageRules.cpp`,
  `PdfDarkModeImageStats.cpp`, `PdfDarkModeInternal.h`, `PdfDarkModeNoOp.cpp`,
  `PdfDarkModeOklab.cpp`, `PdfDarkModeProfile.cpp`, `PdfDarkModeScanProcess.cpp`,
  unit tests `src/utils/tests/PdfDarkMode{ImageClassifier,Oklab}_ut.cpp`
- Touches: `EngineMupdf.cpp` (+2,800 lines), `RenderCache.cpp`, `libmupdf.def`
  (new exported MuPDF symbols), PdfFilter/PdfPreview stubs (`PdfDarkModeNoOp.cpp`,
  `EngineNotifyNoOp.cpp`) to fix DLL link
- Settings: `PdfDocumentColorMode` (auto/black/light, persisted per session 3.7.x)
- Commands: `CmdSetPdfDocumentColorMode{Auto,Black,Light}`, `CmdTogglePreservePdfImages`
- Key commits: `67fe3f0e3`, `e8c329473`, `b68a0723e`, `c225f86f4`, `65cd4d5de`,
  `171cb47a2`, `50a66cebb`, `e4e78bcb8` (light-mode perf)

### E. CAD / engineering-drawing PDF enhancement
Auto-detects CAD-style PDFs (hairline-heavy vector drawings, incl. WPS hairline and
screenshot PDFs) and thickens hairlines when rendering, with a toggle command.
- New files: `src/PdfCadDetect.{cpp,h}`, `src/PdfCadEnhanceDevice.{cpp,h}`,
  `src/PdfCadEnhanceNoOp.cpp`
- Command: `CmdToggleEngineeringDrawingEnhance`
- Key commits: `735894b11`, `b058ba496`, `38006d0e8`

### F. Themes & window chrome
New named themes: **Light-Warm** (eye-care), **Light-White**, **Dark-Dracula**,
**Dark-Black** (plus System). One-click light/dark toggle remembering
`LastDarkTheme`/`LastLightTheme`. Dark-mode fixes throughout the chrome: canvas + TOC
scrollbar theming, menu check marks, maximized frame edges, toolbar separators,
tab UI. Windows 11-style caption glyphs drawn from generated geometry
(`src/CaptionGlyphs.{cpp,h}`, `tools/gen_caption_glyphs.py`) with DPI-aware layout and
interaction-state polish.
- Touches: `Theme.cpp` (+400), `Caption.cpp`, `Toolbar.cpp`, `TableOfContents.cpp`,
  `Menu.cpp`, `TabsCtrl.cpp`, `SvgIcons.cpp`
- Settings: `LastDarkTheme`, `LastLightTheme`; command `CmdToggleLightDarkTheme`
- Key commits: `36468333a`, `e8c329473`, `81defc78b`, `171cb47a2`, `597f0c47d`,
  `1cd0c85d6`→`b0002c393` (caption series), `c4018ec06`, `0e493c658`, `2183086c1`,
  `5491f5520`
- Caution: our Theme.cpp/dark-mode code has evolved a lot since their base; expect
  heavy conflicts. Port ideas, not diffs.

### G. Ebook (EPUB/MOBI/AZW3) engine work
Real changes inside vendored `mupdf/source/html/*` (~1,000–1,700 changed lines per file
even ignoring whitespace) plus Sumatra-side ebook code: faster loading of large ebooks,
NCX table of contents for MOBI/AZW3 and EPUB, progressive TOC loading (with graying-out
of not-yet-reachable in-book links), better Chinese mixed text/image layout, fixes for
garbled MOBI TOC from corrupt NCX/CNCX labels, blank MOBI picture-book pages
(self-closing `<img>` + missing image records), Bookworm EPUB comic layout, EPUB/MOBI
dark-theme switching artifacts.
- Touches: `mupdf/source/html/{html-layout,html-parse,css-apply,epub-doc,html-font}.c`,
  `src/MobiDoc.cpp` (+1,188), `src/EngineEbook.cpp` (+1,370), `src/HtmlFormatter.cpp`,
  `src/EbookFormatter.cpp`, new `src/EbookTypography.h`, `ext/mupdf_load_system_font.c`,
  `src/libmupdf.def`
- Settings: per-doc `LayoutDx/LayoutDy` (flowed ebook page size)
- Key commits: `937dc3454`, `069eb2f23`, `243092d1b`, `7091e1684`, `52239c27a`
  (garbled TOC), `0a97bc1a0`, `65b413ab1`, `c111312bc`, `d8ac31deb`
- Caution: this is the riskiest area to port — vendored mupdf may have moved upstream;
  diff their mupdf changes against *our current* mupdf, not the fork base.
- Note: `mupdf/source/fitz/font.c` diff is whitespace/line-endings only — ignore it.

### H. Selection toolbar
Floating mini-toolbar after selecting text in a PDF: highlight, underline, strikeout,
Ask AI. Also fixed annotation menu visibility on non-PDF files.
- New files: `src/SelectionToolbar.{cpp,h}`; setting `SelectionToolbar` (bool)
- Key commits: `db0b32b7a`, `f1722fbb3`

### I. Home page: list view + tips
`HomePageViewMode` setting: `thumbnails` (current grid) or `list` (filename + path).
`ShowTips` toggle. Thumbnail bookkeeping fields (`ThumbnailDarkTheme`,
`ThumbnailBlankKnown`, `ThumbnailIsBlank` — runtime-only) for dark-theme-aware
thumbnails and hiding blank ones. `HomePage.cpp` +756 lines.
- Key commit: `0435f1ae6` (part of the 3.7.4 release commit), `e4e78bcb8`

### J. GitHub-based auto-update
Rewired update checks to their repo (`raw.githubusercontent.com/dengxibo/...` with
jsDelivr fallback), `update-check.txt` in repo root, GitHub Releases downloads, download
speed/size progress UI, portable self-update that can replace the running exe,
update check on first launch, fix for manual check hanging.
- Touches: `src/UpdateCheck.cpp` (+160/−66), `update-check.txt`
- Key commits: `4e5363143`, `56aefc51a`, `17612a1e0`, `60a8b3462`, `077e00776`,
  `bc79cf9e6`, `9938530ae`
- Assessment: URLs are theirs — **do not merge as-is**. Possibly worth porting:
  download progress UI, portable self-replace fix, hang fix — as ideas.

### K. Standalone bug fixes — verified against master 2026-07-10 (`04726daed`)

Each fork fix was compared against current master to see if the bug still exists here.

**Still applicable (bug present in master):**
- `5491f5520` — **TOC scrollbar mouse capture stuck during tree reload.** Master's
  `TreeView::Clear()` / `SetTreeModel()` call `TreeView_DeleteAllItems` with no
  capture guard; TOC filter/reload paths (`ClearTocBox`, `LoadTocTree`,
  `ApplyTocFilter`) hit exactly that. Port ONLY the `src/wingui/TreeView.cpp` hunk
  (`CancelInProgressInteraction` helper sending `WM_CANCELMODE` + 2 call sites);
  the `TableOfContents.cpp` hunk targets fork-only `ReCreateTocTreeView`. Small, clean.
- `231e4124e` — **bin2coff.c buffer overflow** (`strcpy` → bounded `memcpy`). Our
  `tools/bin2coff.c` still has the unbounded `strcpy`s; bin2coff.exe still embeds
  fonts in the mupdf build. Low severity (inputs are build-controlled labels), fix is
  harmless. Their `tests/test_invariant_bin2coff.c` doesn't fit our test conventions —
  skip or adapt.
- `8cc39e227` (EngineMupdf.cpp part only) — **junk PDF page labels from scanned/OCR
  ebooks** (`.pdg` filenames / per-page prefix-only labels shown instead of page
  numbers). Master's `BuildPageLabelVec` (`src/EngineMupdf.cpp:1846`) and
  `GetPageLabeTemp` (`:4678`) have none of the three guards. Won't apply cleanly —
  master refactored page labels to `Str` (`PageLabelInfo::type` is `Str`, `prefix` is
  `pdf_obj*`); port by hand: prefix-only check after the trivial-default early return,
  `.pdg` check before final `return labels;`, per-label fallback in `GetPageLabeTemp`.

**Partially applicable (core fixed here; small residual pieces):**
- `ac8a2f73b` — **crash closing tabs during background rendering.** Core fz_context
  teardown race is ALREADY FIXED in master: `RenderCache::CancelRendering`
  (`src/RenderCache.cpp:770`) already waits for in-flight renders (fork re-implemented
  an equivalent wait), plus master has `pauseRendering` defenses the fork base lacked.
  Genuinely missing, port by hand if wanted: (a) `IsValidZoom` early-return guard in
  `DisplayModel::SetViewPortSize`; (b) in `ReplaceDocumentInCurrentTab` relayout the
  new doc *before* `delete prevCtrl` (master deletes at `SumatraPDF.cpp:1738` before
  relayout at `:1760`).
- `edebf7dbe` — **crash opening file with tabs disabled.** Structurally prevented in
  master: our async load creates the tab *after* load in `LoadDocumentFinish`
  (fork used a shell-tab-first model needing all those guards; those functions don't
  exist here). Optional hardening only: guard `win->CurrentTab()` in
  `UpdateUiForCurrentTab` (`SumatraPDF.cpp:1581`); make `ReportIf(!tab)` in
  `ReplaceDocumentInCurrentTab` (`:1614`) an early return; null-guard
  `SetFrameTitleForTab`.
- `1369facda` + `21ef059f7` — **multi-monitor per-monitor-DPI UI fonts.** Master
  already handles DPI *detection* + geometry (`src/base/Dpi_win.cpp` prefers
  `GetDpiForWindow`/`GetDpiForMonitor`; `OnDpiChanged` at `SumatraPDF.cpp:5160`
  rebuilds menu/toolbar/find bar) — the fork's Dpi.cpp rewrite is superseded. But the
  **font half is a real master bug**: `GetAppMenuFontSize()` (`AppSettings.cpp:792`)
  sizes fonts from system DPI via `SystemParametersInfoW`; font cache
  (`src/base/Win.cpp:1871`) has no DPI in its key; `ResetCachedFonts()` never runs on
  DPI change; toolbar text (`Toolbar.cpp:1459`) and tabs (`Tabs.cpp:62`) keep
  primary-monitor font size on other monitors. Porting = re-implementation, not
  cherry-pick (~120 lines of per-DPI font helpers in `Win.cpp`/`WinDynCalls`
  incl. `SystemParametersInfoForDpi`, DPI-keyed font cache, then rewire
  `AppSettings.cpp` font getters + `OnDpiChanged`). Medium effort/risk.

**Not applicable:**
- `c7a20f168` — compiler warnings: both are in fork-only code (`PreparePageNavigation`,
  their ebook changes). N/A.
- `2183086c1` — dark-mode TOC scrollbar staying white: master's recursive
  `EnumChildWindows` dark-mode pass (`DarkMode::setChildCtrlsSubclassAndTheme`,
  `SumatraPDF.cpp:2198/2345`) already themes the nested trees; symptom can't occur here.
- `069eb2f23` — "TOC loading optimization": actually a mix of fork branding
  (HomePage links) and MobiDoc work — reclassified into group G (ebooks), not a
  standalone fix. Also accidentally commits a stray `debug-76c24b.log`.

### L. Branding / fork-only — do not merge
README rewrites (bilingual), `readme.txt` Chinese user guide, `docs/github-publish-zh.md`,
app renamed "Sumatra PDF Plus", their 3.7.x version numbering and release commits,
`update-check.txt`, `fonts/README.txt`, machine-generated translation blobs in
`translations/*.txt` (+11k lines, generated by their `tools/gen_*_trans.py` scripts —
if we port a feature, run our own translation flow instead), `vs2022/*` project file
additions (we generate via premake).

## Merge process (for every future sync)

1. **Fetch:** `git fetch plus --no-tags`
2. **What's new:** `git log --date=short --format='%h %ad %s' <last-reviewed>..plus/main`
   where `<last-reviewed>` is the marker near the top of this file.
3. **Categorize** new commits into the feature groups above (add new groups if needed)
   and update the group descriptions. Useful commands:
   - Full feature diff: `git diff <fork-point>..plus/main -- <files>` (fork point
     `2854c78d6` — stable, never changes)
   - Real-change check on vendored code: add `--ignore-all-space`
   - One file's history: `git log --oneline plus/main -- <file>`
4. **Present to Krzysztof for review**; record his decision per feature/commit in the
   Decision log below. Nothing is merged without an explicit decision.
5. **Port approved items:**
   - Prefer `git cherry-pick -x <sha>` when it applies cleanly (`-x` records origin).
   - Otherwise port manually from the feature diff. Attribute in the commit message:
     `Ported from dengxibo/sumatrapdf-plus <sha(s)>`.
   - Settings/commands: re-add through `cmd/gen-settings.ts` / `cmd/gen-commands.ts`
     and run `bun cmd/gen-code.ts` — never take their generated files.
   - clang-format only our `src/` files; keep `mupdf/`/`ext/` edits minimal-style.
   - Skip their translations blobs; add new strings through our translation flow.
   - Build (`bun ./cmd/build.ts`) and run unit tests (`bun cmd/run-unit-tests.ts -dbg`).
6. **Update this file:** bump the "Last reviewed" marker, log decisions and merge
   commits in the Decision log.

### Gotchas

- The fork's base (`2854c78d6`, 2026-05-21) is ~1,600 commits behind master. Never
  `git merge plus/main`. Cherry-picks will often conflict in `SumatraPDF.cpp`,
  `Toolbar.cpp`, `Menu.cpp`, `Theme.cpp` — porting by feature diff is usually easier
  than commit-by-commit for the big features.
- Features are interleaved in shared files and in their "Release 3.7.x" commits; a
  single fork commit often touches several features.
- Any fix in group K may already exist in master in a different form — verify before
  porting.
- Their `libmupdf.def` / premake file lists differ; regenerate ours rather than merging.

## Decision log

| Date | Feature / commits | Decision | Result / notes |
|------|-------------------|----------|----------------|
| _(pending)_ | A. Read Aloud (TTS) | — | |
| _(pending)_ | B. Offline dictionary lookup | — | analyzed 2026-07-11; implementation notes + port plan in [dict-support.md](dict-support.md) (recommendation: port UI/plumbing, replace their bespoke SumatraDict/MDX-converter data layer with a native StarDict reader) |
| _(pending)_ | C. Ask AI | — | |
| 2026-07-11 | D. Smart PDF dark mode | **merged** | as `86e5cb24c`; ported all 15 `PdfDarkMode*` modules + 2 unit-test files by hand (Vec API `.Size()/.at()` → `len()/[]`, `utils/`→`base/` includes, no `#pragma once`, PCH opt-outs). KEY FINDING: the fork ships with the object-level OKLab renderer compile-time OFF (`kPdfDarkModeRenderer = LegacyBitmapPostProcess` in PdfDarkModeColor.cpp) — the live feature is per-doc color mode + classifier-driven image preservation in the bitmap recolor; we kept it identically dormant (wired into RenderPage but unreachable). SEMANTIC ADAPTATION: fork keys dark mode off their dark themes (feature F recolors pages per theme); master's themes never touch page colors, so ours keys off the effective page colors (`FixedPageUI.InvertColors` / custom dark colors) — with invert on, `auto` preserves photos (new default), `black` = old invert-everything, `light` = don't invert PDFs. Integration: `RenderPageArgs.darkProfile` + `darkModeEpoch` (request/entry staleness) in RenderCache, `UpdateBitmapColors` extended with linkColor+skipRects, `GetBitmapRecolorSkipRects` (content-stream image collection + classifier gating) in EngineMupdf, `PdfDarkModeNoOp.cpp` for filter/preview/test_engines/mac, `fz_colorspace_n` added to libmupdf.def. Setting `PdfDocumentColorMode` (auto/black/light) + 4 commands. Verified end-to-end with a synthetic photo PDF (screenshots: photo preserved in auto, inverted in black, original in light; deterministic re-renders). Fork's RenderCache non-blocking CancelRendering + UpdateAfterThemeChange reorder (735894b11) still not taken — master's CancelRendering was already reworked. |
| 2026-07-10 | E. CAD line enhancement (`735894b11`, `b058ba496`, `38006d0e8`) | **merged** | as `211f989d1`; ported by hand against master (fork base predates Pixmap/PCH/Str refactors). Key adaptations: `EngineeringDrawingEnhance` mode pushed via setter from `LoadSettings` (gDisableFormJavaScript pattern) instead of reading gGlobalPrefs in PdfCadDetect.cpp — so no `PdfCadEnhanceNoOp.cpp` needed; real files compile everywhere (incl. mac) with mode defaulting to Off outside the app. Fixed fork's read-after-free of analysis-device stats (stats now caller-owned) and their unguarded fz_rethrow out of DetectCadPdf. Added 12 missing fz_* exports to libmupdf.def (fork's approach was the NoOp stub instead). Skipped: their RenderCache/UpdateAfterThemeChange hunks (dark-mode toolbar fix, belongs to D). Setting editable in Advanced Settings dialog (enum values added). |
| _(pending)_ | F. Themes & chrome | — | |
| _(pending)_ | G. Ebook engine work | — | |
| 2026-07-10 | H. Selection toolbar (`db0b32b7a` + follow-up polish) | **merged** | as `dba10007e`; ported `SelectionToolbar.{cpp,h}` by hand; folded the shared `FloatingPopupStyle.{cpp,h}` helpers into it (they only exist to share chrome with WordLookup, feature B). Buttons rewritten on top of master's `CommandAvailability` (`GetCommandVisibility`, `CommandSurface::Toolbar`): Copy, Read Aloud, Highlight, Underline, Squiggly, Strike Out; dropped fork's Ask AI / Look Up buttons (features C/B). Theme colors mapped to master's Theme.h (`IsLightColor(ThemeWindowBackgroundColor())` for dark detection). Setting `Annotations.SelectionToolbar` (bool, default true). Skipped unrelated hunks bundled in `db0b32b7a`: Menu.cpp UINT_PTR fix (master already fixed via CommandAvailability refactor), `EngineMupdfGetAnnotations` quick-load/no-pagesLock change (master's locking was rewritten; possible perf win for large docs — evaluate separately), `HwndSetIcon` ICON_SMALL addition (unrelated; could take separately). |
| _(pending)_ | I. Home page list view | — | |
| _(pending)_ | J. Auto-update changes | — | fork-specific URLs; ideas only |
| 2026-07-10 | K: TOC scrollbar capture stuck (`5491f5520`) | **merged** | ported TreeView.cpp hunk only, as `a1b40ef1f` |
| 2026-07-10 | K: junk .pdg PDF page labels (`8cc39e227` part) | **merged** | hand-ported to Str-based page labels, as `dc36c20ce` |
| 2026-07-10 | K: bin2coff strcpy overflow (`231e4124e`) | **merged** | clean cherry-pick, as `dbc91d370`; skipped their C test file (doesn't fit our test conventions) |
| 2026-07-10 | K: render-teardown hardening (`ac8a2f73b` residual parts) | **merged** | IsValidZoom guard + relayout-before-delete, as `622dcb8d0`; core race already fixed in master; skipped their WaitForRenderingComplete (duplicate) and CloseTab pauseRendering (covered by CloseWindow/~DisplayModel) |
| 2026-07-10 | K: multi-monitor DPI fonts (`21ef059f7`+`1369facda`) | **merged** | re-implemented against master's font cache, as `1c1fccdec`; per-DPI font cache + SystemParametersInfoForDpi + WM_DPICHANGED re-apply; skipped their Dpi.cpp rewrite (master's Dpi_win.cpp already better) |
| 2026-07-10 | K: no-tabs open crash (`edebf7dbe`) | **skip** | structurally prevented by master's load-then-create-tab design; optional guards not taken |
| 2026-07-10 | K: compiler warnings (`c7a20f168`) | **skip** | warnings are in fork-only code |
| 2026-07-10 | K: dark TOC scrollbar (`2183086c1`) | **skip** | master's recursive dark-mode pass already themes nested trees |
| 2026-07-10 | L. Branding / fork-only | **skip** | readmes, renames, versioning, translations blobs, vs2022 files |
