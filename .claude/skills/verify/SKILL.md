---
name: verify
description: Build, launch and drive SumatraPDF on Windows to verify a change end-to-end (GUI automation, screenshots, log capture).
---

# Verifying SumatraPDF changes

## Build & launch

- Build: `bun ./cmd/build.ts -debug` → `out/dbg64/SumatraPDF.exe` (the exe `tests/util.ts` EXE points at; `SumatraPDF-static.exe` is the static target and is usually stale).
- If new source files were added to `premake5.files.lua` / `premake5.lua`, run `bun cmd/premake.ts` first to regenerate `vs2022/*.vcxproj`, or the build fails with stale projects / link errors.
- Always launch with `-for-testing` (fresh instance, no session restore, doesn't touch user settings).
- Capture engine/app logs: `-log -log-to-file <path>` (collects `logf`/`logfa` output).

## Driving the GUI

Injected SendInput is dropped on this machine; post window messages instead (see project memory `env-gui-automation`). Reuse the checked-in helpers from a bun script:

```ts
const { launchSumatra, waitForFrame, findCanvas, sendCommand } = await import("<root>/tests/win-automation.ts");
const { captureWindowToPng, sleep, postMessage } = await import("<root>/tests/winapi.ts");
const { cmdId } = await import("<root>/tests/util.ts");

const proc = launchSumatra(["-log", "-log-to-file", logPath, pdfPath]);
const frame = await waitForFrame(proc.pid!);
await sleep(2500); // first render
const canvas = findCanvas(frame);
captureWindowToPng(canvas, "before.png"); // works on occluded windows (PrintWindow)
sendCommand(frame, cmdId("CmdSomething")); // NEVER hardcode command ids
await sleep(2000);
captureWindowToPng(canvas, "after.png");
postMessage(frame, 0x0010 /*WM_CLOSE*/, 0, 0);
```

- Compare before/after PNGs (byte diff is a good first signal; Read the PNGs to eyeball).
- Repeating an action back to the same state reproduces byte-identical captures — useful as a determinism control.
- Sample PDF in-repo: `ext/a-zlib/zlib.3.pdf`. Bug repros live in `C:\Users\kjk\OneDrive\!sumatra\bugs\`.
- Synthetic PDFs are easy to hand-generate from a bun script (compute xref offsets from string lengths) when a specific content shape is needed (e.g. metadata, hairline strokes).

## Gotchas

- `captureWindowToPng` (PrintWindow) does NOT capture the custom caption row's
  buttons (painted via `BeginPaint` directly on the frame DC) nor native
  scrollbars (non-client area) — those regions come out blank. Screen capture
  (`CopyFromScreen`) is also unavailable in this environment (blank desktop).
  For non-client drawing use `captureWindowDCToPng` / `captureWindowDCRegionToPng`
  instead: they BitBlt from `GetWindowDC`, i.e. what is actually on screen for
  that window (works for background windows, not minimized ones). The Region
  variant takes a window-relative sub-rect plus a zoom factor — a 17px scrollbar
  strip is unreadable in a full-window PNG, so crop it and magnify 3x. If neither
  works, dump the drawing to a bitmap via a temporary harness (render into a
  `CreateDIBSection` DC, save with `HBITMAPToBmpFormat` + `file::WriteFile`,
  view, then remove the harness).

- On this machine (OS in dark mode) `captureWindowToPng` (PrintWindow with
  PW_RENDERFULLCONTENT) NONDETERMINISTICALLY renders WindowBase dialogs with the
  OS dark theme — dark background, dark combos/checkboxes, dark caption — even
  when the app theme is Light and the screen shows the dialog correctly light.
  Across runs it can come out fully dark, fully light, or mixed, so it proves
  nothing about theming. For any color/theme assertion on a dialog use
  `captureWindowDCToPng` (BitBlts the window's real surface); PrintWindow is
  fine for layout/text content.

- All WindowBase dialogs share one window class, `SumatraWgDefaultWinClass`,
  and a hidden find-bar strip of that class exists from startup — finding a
  dialog by class alone (`waitForTopWindow`) can return that ~431x38 strip
  (captures as a black band). Match by class AND `getWindowText(hwnd)` against
  the dialog's `args.title`. Also: `CmdGoToPage` doesn't open its dialog while
  the toolbar is visible — it focuses the toolbar's page box instead.

- Reading a control's text cross-process needs `getControlText` (WM_GETTEXT);
  `GetWindowTextW` only returns captions for windows of another process, so
  `getWindowText`/`getWindowTextFull` come back empty for child controls.
  Text assertions beat pixel comparison when the change is textual.

- `out/dbg64/SumatraPDF-settings.txt` exists (portable mode): the dbg build **loads** it even under `-for-testing` (which only prevents saving). Stale values there change app behavior in tests — e.g. a non-default `PdfDocumentColorMode` silently alters rendering. Check it when the app behaves unexpectedly at startup; it's written only by non-`-for-testing` (manual) launches.

- Unit tests: `bun cmd/run-unit-tests.ts -dbg` (but verification = driving the app, not tests).
- PdfFilter/PdfPreview link mupdf through `src/libsumatrapdf.def`; new `fz_*`/`pdf_*` calls in code they compile need exports added there.
- New `src/*.cpp` that include mupdf headers before `base/Base.h` must be added to the PCH opt-out list in `premake5.lua` (`setup_base_pch`), or every symbol from those headers is "undeclared" (PCH skips everything before `#include "base/Base.h"`).
