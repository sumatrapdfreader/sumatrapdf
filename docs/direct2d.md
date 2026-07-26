# Direct2D for custom painting

Estimate of converting custom GDI/GDI+ painting to Direct2D for consistency,
with pros/cons and recommended approach. (Snapshot of codebase analysis as of
2026-07.)

## Scope (what “all custom GDI/GDI+” actually means)

Important split:

| Layer | Today | D2D conversion? |
|-------|--------|------------------|
| **Page pixels** | MuPDF → `Pixmap` → blit to `HDC` (`RenderCache`) | Optional: blit via D2D/bitmap, **not** re-render PDF with D2D |
| **Chrome / overlays** | GDI + GDI+ (`Canvas`, selection, home, tabs, caption, scrollbars, notifications, selection toolbar, …) | **Yes — this is the bulk** |
| **Common controls** | Win32 buttons/edits + darkmodelib theming | Usually **leave** or only wrap; full D2D rewrite of every control is a different project |
| **mui text** | `ITextRender` (GDI / GDI+ / HDC); TODO for DirectDraw-ish path | Natural insertion point for DWrite/D2D text |

Rough inventory (order of magnitude):

- **~420 `Gdiplus::` refs** in **~38 files** under `src/`
- Heavy custom painters include: `Canvas`, `HomePage`, `TabsCtrl`, `OverlayScrollbar`,
  `CaptionGlyphs`, `Selection` / `SelectionToolbar`, `Notifications`,
  `ReadAloudPlaybackBar`, `ImageSaveCropResize`, `base/GdiPlus`, `mui/*`,
  installer chrome, etc.
- Page path is already raster; D2D there is mainly **composition**, not layout.

There is **no Direct2D layer** in the tree today.

### Notable related code

- `src/mui/TextRender.h` — `ITextRender` with GDI / GDI+ / HDC backends; comment
  `// TODO: implement TextRenderDirectDraw`
- `src/base/GdiPlus.*` — measure text, pixmap/bitmap conversion, image load helpers
- `RenderCache` — `BlitPixmap` / region blit to `HDC` after MuPDF render

## Effort estimate

Assumes a thin **`D2dContext`** (factory, per-window `ID2D1HwndRenderTarget` or
DC/DXGI target, brushes, DPI), **DirectWrite** for text you care about, and
incremental migration—not a big-bang rewrite.

Estimates are **person-weeks** for one strong Win32 engineer.

| Phase | Work | Est. |
|-------|------|------|
| **0. Foundation** | D2D/DWrite init, device-lost, DPI, HDC interop, theme color helpers, debug toggles | **1.5–3 w** |
| **1. Easy chrome** | Selection toolbar, notifications, playback bar, simple overlays (rounded rects, fills, lines) | **1–2 w** |
| **2. Medium chrome** | Overlay scrollbars, caption glyphs, tab strip painting, find chrome custom bits | **2–4 w** |
| **3. Hard UI** | `Canvas` overlays (selection, find, focus, links), `HomePage` list/thumbs painting | **3–6 w** |
| **4. Text pipeline** | DWrite backend for `ITextRender` (or new API); measure/draw parity with GDI/GDI+ | **2–5 w** |
| **5. Image tools** | `ImageSaveCropResize` and other heavy GDI+ editors | **2–4 w** (optional / later) |
| **6. Page blit** | `BlitPixmap` → D2D bitmap (optional consistency) | **0.5–1.5 w** |
| **7. Polish** | HiDPI multi-monitor, darkmode, printing edges, perf, regressions | **2–4 w** |
| **Total “consistent custom paint on D2D”** | Chrome + overlays + text + foundation | **~12–25 person-weeks** |
| **+ image tools + full GDI+ purge** | Drop GDI+ for loaders/encoders too | **+4–10 w** (often not worth it) |

**Calendar:** ~3–6 months part-time alongside features; ~2–3 months if focused
full-time with good tests/screenshots.

**Not in the estimate:** rewriting every themed Win32 control, WebView2, installer
in isolation, or PDF engine to D2D.

## Pros

- **One composition model** for custom UI (antialiasing, transforms, opacity,
  rounded clips without GDI region footguns).
- **Better HiDPI / multi-monitor** behavior if you size render targets in DIPs
  and rebuild on DPI change.
- **Cleaner overlays** on page bitmaps (selection, find, scrollbars) with less
  HDC lock gymnastics (`Graphics::GetHDC` pain is already known in `TextRender`).
- **DWrite** can improve text for UI chrome (subpixel, fallbacks)—if you invest
  in the text path.
- Aligns with modern Windows; GDI+ is legacy and slower for many 2D ops.
- Forces a **real paint abstraction** (you already half-have this in `ITextRender`).

## Cons

- **Large, boring migration** with high regression risk (every dialog/chrome edge case).
- **Interop tax:** still need GDI for many Win32 controls, print, some image codecs,
  darkmodelib—hybrid forever unless you go full custom UI.
- **Device lost / DPI / session** (RDP, sleep, adapter change) must be handled carefully.
- **Font / metric drift** if measure stays GDI and draw goes D2D (or vice versa)—breaks
  hit-testing, tab widths, selection toolbar layout.
- **GDI+ used for more than paint:** image load/orientation/encode (`GdiPlus.cpp`,
  thumbs, crop tool). “All GDI+” ≠ “all paint.”
- **Page pipeline is MuPDF**, not GDI—D2D doesn’t simplify document fidelity.
- **darkmodelib** already subclasses and paints borders; stacking D2D parents can fight it.
- Opportunity cost vs. features (EPUB progressive, etc.).

## Practical recommendation

1. **Don’t aim for “zero GDI”.** Aim for: *custom chrome and overlays draw through
   one D2D helper; pages stay MuPDF pixmaps.*
2. **Start with a façade** (`DrawContext` over HDC *or* D2D) and migrate 1–2 surfaces
   (selection toolbar + notifications) before Canvas.
3. **Do text deliberately:** either full DWrite measure+draw for that surface, or
   keep GDI measure temporarily and accept limits.
4. **Leave** common controls + darkmodelib; **optional** page blit via D2D later.
5. **Skip or defer** `ImageSaveCropResize` and GDI+ image codecs unless you’re
   killing GDI+ for packaging reasons.

## Bottom line

A **consistent custom-paint stack on Direct2D** is a **medium–large project
(~3–6 engineer-months)**, not a refactor weekend. Payoff is real for overlay/chrome
quality and architecture; full GDI/GDI+ elimination is much larger and lower ROI
while Sumatra remains a Win32 + darkmodelib + MuPDF app.
