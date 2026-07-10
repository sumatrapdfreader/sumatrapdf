---
name: verify
description: Build, launch and drive SumatraPDF on Windows to verify a change end-to-end (GUI automation, screenshots, log capture).
---

# Verifying SumatraPDF changes

## Build & launch

- Build: `bun ./cmd/build.ts` → `out/dbg64/SumatraPDF-dll.exe` (NOT `SumatraPDF.exe`, which is a stale static target).
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
- Sample PDF in-repo: `ext/zlib/zlib.3.pdf`. Bug repros live in `C:\Users\kjk\OneDrive\!sumatra\bugs\`.
- Synthetic PDFs are easy to hand-generate from a bun script (compute xref offsets from string lengths) when a specific content shape is needed (e.g. metadata, hairline strokes).

## Gotchas

- Unit tests: `bun cmd/run-unit-tests.ts -dbg` (but verification = driving the app, not tests).
- PdfFilter/PdfPreview link mupdf through `src/libmupdf.def`; new `fz_*`/`pdf_*` calls in code they compile need exports added there.
- New `src/*.cpp` that include mupdf headers before `base/Base.h` must be added to the PCH opt-out list in `premake5.lua` (`setup_base_pch`), or every symbol from those headers is "undeclared" (PCH skips everything before `#include "base/Base.h"`).
