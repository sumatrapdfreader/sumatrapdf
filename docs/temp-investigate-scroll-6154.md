# Investigate sticky scroll at 180%+ zoom (#6154)

Temp notes for an agent on another machine. Delete when done.

Issue: https://github.com/sumatrapdfreader/sumatrapdf/issues/6154  
The useful comment: https://github.com/sumatrapdfreader/sumatrapdf/issues/6154#issuecomment-5591969544

## Goal

Reproduce (or rule out) sticky / not-smooth vertical scrolling at ~180% zoom and above. Reporter says it happens with two-finger touchpad **and** with the vertical scrollbar, on a light PDF.

Do not "fix" until you have seen it, or have a measurement that shows zoom-dependent hitch that a human would call sticky.

## Reporter setup (from the comment + settings file)

- Pre-release SumatraPDF, Windows 11 Pro, Best Performance
- 3K OLED, **Scaling 250%**, maximized window `0 0 2880 1800` (`WindowState = 3`)
- Scroll: Precision touchpad two-finger, later also native vertical scrollbar
- Sample: [lorem.pdf](https://github.com/user-attachments/files/31962008/lorem.pdf) (3 pages, ~403 KB)
- Settings: [SumatraPDF-settings.txt](https://github.com/user-attachments/files/31977608/SumatraPDF-settings.txt)

Keep copies (already downloaded here):

- `C:\Users\kjk\OneDrive\!sumatra\bugs\bug-6154-lorem.pdf`
- `C:\Users\kjk\OneDrive\!sumatra\bugs\bug-6154-SumatraPDF-settings.txt`

If those paths are missing, download the two GitHub attachments again.

Relevant settings (not the whole file):

```
DefaultDisplayMode = automatic
DefaultZoom = fit content
SmoothScroll = true
ScrollLineAmount = 16
Scrollbars = hidden
MouseWheelTurnsPage = false
ZoomIncrement = 0.01
CustomScreenDPI = 0
WindowPos = 0 0 2880 1800
WindowState = 3
```

`FixedPageUI.UseOverlayScrollbar = true` is leftover; live scrollbar mode is `Scrollbars = hidden`. File history for `lorem.pdf` had `Zoom = 250.917` (matches the tiny `ZoomIncrement`).

## What the first machine could not do

Ran on a **96 DPI ~1920×1200** box, **no interactive desktop**, **no real touchpad**. Injected `SendInput` is dropped; posted `WM_MOUSEWHEEL` / `WM_VSCROLL` work. You cannot *feel* stickiness there.

If your machine has 250% scaling, a touchpad, and a real desktop, **manual repro first**. That is the missing piece.

## Manual repro (do this first on a 250% / 3K box)

1. Build: `bun cmd/build.ts -dbg` → `out/dbg64/SumatraPDF.exe`
2. Copy reporter settings into a throwaway dir, then:

```
out\dbg64\SumatraPDF.exe -for-testing -appdata <throwaway> -log -log-to-file <log> -zoom 180 <lorem.pdf>
```

Maximize. Two-finger scroll. Repeat at 100%, 180%, 250%. Then set `Scrollbars = windows` and drag the thumb / click the track.

3. Also try `SmoothScroll = false`. If stickiness vanishes, it is the animation + tile paint, not input.

What "sticky" means here: hitch / catch-up while moving, not "smooth-scroll eases for ~500 ms" (that is `kSmoothScrollRate = 15` in `src/Canvas.cpp` and happens at every zoom).

Log: `Slow rendering: N ms` fires only if a tile takes **>300 ms** (`src/RenderCache.cpp`). Absence of that line does not mean scrolling felt fine.

## Why 180% on 250% DPI is special

`zoomReal = zoomVirtual * 0.01 * (screenDPI / 72)`.

| Display | Zoom % | zoomReal | notes |
|---|---|---|---|
| 96 DPI | 180 | 2.4 | still cheap |
| 240 DPI (250% scale) | 100 | 3.33 | |
| 240 DPI | **180** | **6.0** | reporter threshold |
| 240 DPI | 250 | 8.33 | their saved zoom |

`RenderCache::GetTileRes` (`src/RenderCache.cpp`) splits a page when it is larger than `maxTileSize` (initially `SM_CXSCREEN × SM_CYSCREEN`). `res` goes 0 → 1 → 2 as `factorAvg` grows. On the 96 DPI box, `CustomScreenDPI = 240` + 180% was the first case with `res=2`.

Hypothesis: at 180% × 250% DPI on a 2880×1800 window, each new tile is ~screen-sized; scrolling into uncached tiles hitchs. Once cached, jumps are cheap. Matches "even light PDFs" and "scrollbar too" (both request new tiles).

`CustomScreenDPI` only fakes the **zoom math**, not Windows DPI, window pixel size, or touchpad. Prefer a real 250% display.

## Measurements already taken (96 DPI machine, 2026-09-11)

Posted input, full work area, `lorem.pdf`, continuous, `WaitRenderIdle` then scroll.

| Case | First idle | After scroll |
|---|---|---|
| 96 DPI, 100–250%, wheel 120 / line / page | 8–68 ms | moved immediately |
| 96 DPI, 250%, 30-delta "touchpad" wheel | same | same as 120-delta |
| `CustomScreenDPI=240`, 100% page-down | 42 ms | instant |
| `CustomScreenDPI=240`, **180%** (`zoomR=6`, `res=2`) | **~27 s** to first idle | then page-down instant |
| `CustomScreenDPI=240`, 250% (`zoomR=8.3`, `res=2`) | 447 ms | instant |
| SmoothScroll on, 8× wheel 120 | settle ~545 ms **at every zoom** | that is the ease, not a hitch |
| SmoothScroll off, page-down | settle ~85 ms | |

Wheel and scrollbar line/page did **not** get slower at 180%+ once tiles were ready. The 27 s first-idle at 180%/240 is suspicious but was also inflated by `WaitRenderIdle` (it `InvalidateRect`+`UpdateWindow` every poll — see below).

Earlier 2026-09-08 probe on the same box (`tests/tmp/6154-probe.ts`, 96 DPI, 100 vs 200%) also saw no zoom-dependent wheel hitch.

## Code to read

- Wheel / smooth scroll: `src/Canvas.cpp` `CanvasOnMouseWheel`, `OnVScroll`, `StartOrUpdateSmoothScrollY` (`kSmoothScrollRate = 15`)
- Tile split: `src/RenderCache.cpp` `GetTileRes`, slow log `durMs > 300`
- Idle snapshot (do not treat as "user scroll cost"): `src/SumatraControl.cpp` `SnapshotRenderIdle` — invalidates twice per poll
- DPI for zoom: `src/SumatraPDF.cpp` `gSettings->customScreenDPI` / `DpiGetForHwnd`
- Overlay vs hidden: `ScrollbarsUseOverlay()` / `ScrollbarsAreHidden()` in `src/SumatraPDF.cpp`

## Instrumented re-run (if you cannot feel it)

Do **not** register a suite test. Write `tests/tmp/repro-6154.ts` (gitignored) or run from a scratch file.

Launch with `-for-testing -dbg-control -appdata <dir> -log -log-to-file <log>`. Helpers: `tests/win-automation.ts` `launchControlled`, `tests/control.ts` `waitForRenderIdle` / `TestDpi`, `tests/winapi.ts` `getScrollInfo` / `postMessage`.

Window pos format is `WIDTHxHEIGHT@XxY` (e.g. `1872x1200@0x0`), not `x,y,dx,dy`. Use the **work area**, not the default right-half test window — small viewport understates tile cost.

Compare:

1. Native DPI, zoom 100 / 180 / 250
2. If native DPI is 96, also `CustomScreenDPI = 240` at those zooms (math-only stand-in)
3. Input: `WM_MOUSEWHEEL` delta 120 (mouse), delta 30 (precision pad), `WM_VSCROLL` `SB_LINEDOWN` / `SB_PAGEDOWN`
4. `SmoothScroll` true vs false
5. `Scrollbars = hidden` vs `windows`

Log from `waitForRenderIdle`: `zoomV=… zoomR=… res=… vp=WxH ready=… tile=… cache=…`. `res` jumping 0→2 at the reporter's 180% is the signal.

Time **scroll start → first `nPos` change** and **time until `WaitRenderIdle` after a page-sized jump into uncached tiles**. Do not wait-for-idle *before* the jump if you want to see hitch; that pre-warms the cache.

### Pitfalls

- `WaitRenderIdle` paints every poll. A long first-idle can be the probe starving render threads, not the user's hitch. Prefer log timestamps / `Slow rendering` / CPU, or sample `GetScrollInfo` during a burst of wheel messages without waiting idle first.
- Posted `WM_VSCROLL` `SB_THUMBTRACK` does nothing useful: `OnVScroll` reads `si.nTrackPos` from `GetScrollInfo`, which posting does not set. Use `SB_PAGEDOWN` or drive the overlay/native thumb with mouse messages if the desktop allows it.
- Hidden scrollbars: `GetScrollInfo` on the canvas can still move (`nPos`); overlay mode updates via `OverlayScrollbarGetInfo`.
- `-for-testing` skips session restore and does not save settings. Portable `out/dbg64/SumatraPDF-settings.txt` can still **load**. Use `-appdata`.
- Command ids: `cmdId("Cmd…")`, never hardcode.

## What would count as a repro

Any of:

- You feel hitch at ≥180% on 250% scaling, gone or much less at 100%, with touchpad **or** scrollbar
- Tile render of visible pages regularly >~50–100 ms at that zoom, and scroll motion stalls until the tile lands
- `res` / `zoomR` at 180%×250% DPI clearly larger than at 100%, and scrolling into a new tile row hitches

If it is smooth at 250% on a 250% 3K box with their settings, say so; then the report is machine-specific (GPU, touchpad driver, OLED) and not a general tile bug.

## Do not

- Run `tests/run-almost-all.ts` / `run-all.ts`
- Commit a fix or a suite test from this doc
- Treat 545 ms smooth-scroll settle as the bug
