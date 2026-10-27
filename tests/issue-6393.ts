// issue #6393: markdown opened at 1000% on the IE backend (Windows 7, Windows
// Sandbox: no WebView2). The default "fit page" zoom (-1) reached the browser
// as a zoom percent; IE clamps it to its maximum, WebView2 ignores it.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { runStandalone, tmpPath } from "./util.ts";
import { findCanvas, launchControlled, killAndWait } from "./win-automation.ts";
import { captureWindowPixels, sleep } from "./winapi.ts";

// "MMMM" is ~11px tall at 100%, ~110px at 1000%
const kMinTextHeight = 6;
const kMaxTextHeight = 40;
// skip the scrollbars IE adds once the page overflows
const kScrollbarMargin = 40;

// height of the band of rows holding dark (text) pixels, 0 if none
function textHeight(canvas: number): number {
  const px = captureWindowPixels(canvas);
  if (!px) {
    return 0;
  }
  let top = -1;
  let bottom = -1;
  for (let y = 0; y < px.h - kScrollbarMargin; y++) {
    for (let x = 0; x < px.w - kScrollbarMargin; x++) {
      const i = (y * px.w + x) * 4;
      // average, not per channel: ClearType tints glyph edges
      if (px.data[i] + px.data[i + 1] + px.data[i + 2] < 3 * 128) {
        if (top < 0) {
          top = y;
        }
        bottom = y;
        break;
      }
    }
  }
  return top < 0 ? 0 : bottom - top + 1;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6393");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "a.md"), "MMMM\n");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    ["RestoreSession = false", "ShowStartPage = false", "ShowToc = false", ""].join("\n"),
  );

  // -html-backend ie: the path machines without WebView2 take
  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-html-backend",
    "ie",
    join(dir, "a.md"),
  ]);
  try {
    await client.waitForRenderIdle();
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("issue-6393: no canvas");
    }

    // wait for the page to paint, then for the size to settle
    const deadline = Date.now() + 8000;
    let prev = -1;
    let h = 0;
    for (;;) {
      h = textHeight(canvas);
      if (h >= kMinTextHeight && h === prev) {
        break;
      }
      if (Date.now() > deadline) {
        throw new Error(`issue-6393: text didn't render (height ${h})`);
      }
      prev = h;
      await sleep(250);
    }
    if (h > kMaxTextHeight) {
      throw new Error(`issue-6393: text is ${h}px tall, expected <= ${kMaxTextHeight} (zoomed in)`);
    }
    console.log(`issue-6393: text ${h}px tall`);
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-6393: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
