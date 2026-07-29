# macOS Port Progress

Last updated: 2026-07-08.

## Completed

- `46654fb25` — Ported `TextSelection.cpp` and `TextSearch.cpp` enough for macOS:
  removed Win32/wingui include dependencies, preserved Windows character
  classification/case folding under `#if OS_WIN`, added POSIX fallbacks, and
  added both files to `PORTABLE_COMPILE_SOURCES` and `MAC_APP_SOURCES`.
- Verified with `bun cmd/build-mac.ts -debug`; mac `test_util`,
  `test_engines`, portable compile checks, and `SumatraPDF.app` all built.

## Deferred

- `PdfSync.cpp` / SyncTeX forward-inverse search is intentionally deferred. It is
  not needed for the near-term native mac viewer milestones: open, render,
  scroll, zoom, selection, and text search. The file also pulls in external
  SyncTeX C sources and Windows-specific local-codepage/path handling, so port
  it later only when mac TeX integration becomes a feature target.

## Next Candidates

- Continue Phase 1 with document-model files that directly support the viewer:
  `Annotation`, then `DisplayModel` once its Windows UI include dependencies are
  separated.
- Keep new mac build additions small and verify each slice with
  `bun cmd/build-mac.ts -debug`.
