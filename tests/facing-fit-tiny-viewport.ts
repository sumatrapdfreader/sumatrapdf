// Regression test: in facing view with Fit Page, a canvas narrower than the
// window margins has no fit zoom. GetZoomReal() returned 0 and painting hit
// ReportIf(zoom <= 0) in EngineMupdf::Transform (crash 2026-10-01-07-12-743f).
//
// Run: bun tests/facing-fit-tiny-viewport.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { assemblePdf, runStandalone, tmpPath } from "./util";
import { killAndWait, launchControlled } from "./win-automation";
import {
  getWindowRect,
  isZoomed,
  moveWindow,
  setWindowPos,
  showWindow,
  sleep,
  SW_RESTORE,
  SWP_NOACTIVATE,
  SWP_NOSENDCHANGING,
  SWP_NOZORDER,
} from "./winapi";

function makePdf(nPages: number): string {
  const objs: string[] = [];
  objs.push(`<< /Type /Catalog /Pages 2 0 R >>`);
  const kids = Array.from({ length: nPages }, (_, i) => `${3 + i} 0 R`).join(" ");
  objs.push(`<< /Type /Pages /Count ${nPages} /Kids [${kids}] >>`);
  for (let i = 0; i < nPages; i++) {
    objs.push(`<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>`);
  }
  return assemblePdf(objs);
}

const SETTINGS = [
  `DefaultDisplayMode = facing`,
  `DefaultZoom = fit page`,
  `RestoreSession = false`,
  `ShowStartPage = false`,
  `CheckForUpdates = false`,
  ``,
].join("\n");

// frame width that leaves the canvas a few pixels wide, less than the margins
const kTinyFrameDx = 8;

export async function testit(): Promise<void> {
  const pdf = tmpPath("facing-fit-tiny-viewport.pdf");
  writeFileSync(pdf, makePdf(6), "latin1");

  const appdata = tmpPath("facing-fit-tiny-viewport-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), SETTINGS);

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    if (isZoomed(frame)) {
      showWindow(frame, SW_RESTORE);
    }
    const r = getWindowRect(frame);
    const dy = r.bottom - r.top;

    // SWP_NOSENDCHANGING skips the frame's min track size, so the canvas can
    // get as narrow as a squeezed or DPI-changed window leaves it
    setWindowPos(frame, r.left, r.top, kTinyFrameDx, dy, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);

    // repaints the canvas; not idle at this size, so a timeout is expected
    await client.waitForRenderIdle(1000).catch(() => {});
    await Promise.race([proc.exited, sleep(500)]);
    if (proc.exitCode !== null) {
      throw new Error(`SumatraPDF exited with code ${proc.exitCode} painting a too-narrow facing / fit page view`);
    }

    moveWindow(frame, r.left, r.top, r.right - r.left, dy);
    await client.waitForRenderIdle();
  } finally {
    await client.quit().catch(() => {});
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
