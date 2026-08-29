// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6095
//
// Following an internal EPUB link left the target line clipped by the top of
// the view. MuPDF resolves an anchor to the baseline of its first content, and
// scrolling that to the top of the window cuts off everything the glyphs draw
// above the baseline.
//
// issue-6095.epub is the reporter's book with the cover, the fonts and the
// chapters the bug does not need taken out, plus a table-of-contents entry
// pointing at footnote 45 so the jump can be made without hunting for a
// superscript to click. Its stylesheet is the book's: line-height 1.5em is what
// makes the clipping wide enough to see, which is why the reporter found that
// IgnoreDocumentCSS hid the bug.
//
// Run: bun tests/issue-6095.ts [--no-build]   (or via tests/run-almost-all.ts)

import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, ROOT, runStandalone, SLOW_BUILD_FACTOR, writeAppdata } from "./util.ts";
import { captureWindowPixels, sleep } from "./winapi.ts";
import { findCanvas, waitForFrame } from "./win-automation.ts";

// the third TOC entry is annotations.xhtml#annotation-45
const TOC_DEST = 3;
// how many rows at the top of the view the check looks at, and how much ink is
// too much in them. A clipped jump leaves the cut-off tops of a whole line of
// text there; a correct one leaves at most a descender or two of the line above
const TOP_ROWS = 3;
const MAX_TOP_INK = 60;

type Cap = { data: Uint8Array; w: number; h: number };

// dark pixels in the first TOP_ROWS rows of the canvas
function inkAtTop(cap: Cap): number {
  let n = 0;
  for (let y = 0; y < TOP_ROWS && y < cap.h; y++) {
    for (let x = 0; x < cap.w; x++) {
      const i = (y * cap.w + x) * 4;
      const lum = (0.2126 * cap.data[i + 2]! + 0.7152 * cap.data[i + 1]! + 0.0722 * cap.data[i]!) / 255;
      if (lum < 0.5) {
        n++;
      }
    }
  }
  return n;
}

async function followToc(client: ControlClient, destNo: number): Promise<void> {
  const res = await client.request(ControlCommand.TestDestZoomNav, [destNo, 0]);
  const raw = String(res[1] ?? "").trim();
  if (res[0] !== 0 || !/^OK /.test(raw)) {
    throw new Error(`issue-6095: could not follow the TOC destination: ${raw}`);
  }
}

export async function testit(): Promise<void> {
  const appdata = writeAppdata(
    "issue-6095",
    ["UiLanguage = en", "RestoreSession = false", "ShowStartPage = false", "CheckForUpdates = false"].join("\n"),
  );
  const epub = join(ROOT, "tests", "issue-6095.epub");

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      await client.waitForRenderIdle(30000);
      await client.setNotificationsEnabled(false);

      await followToc(client, TOC_DEST);
      await sleep(500 * SLOW_BUILD_FACTOR);
      await client.waitForRenderIdle(30000);

      const canvas = findCanvas(frame);
      if (!canvas) {
        throw new Error("issue-6095: no canvas");
      }
      const cap = captureWindowPixels(canvas);
      if (!cap) {
        throw new Error("issue-6095: capture failed");
      }
      const ink = inkAtTop(cap);
      if (ink > MAX_TOP_INK) {
        throw new Error(
          `issue-6095: the target line is clipped by the top of the view (${ink} dark pixels in the first ` +
            `${TOP_ROWS} rows of ${cap.w}x${cap.h})`,
        );
      }
    },
    // a fixed window: the book reflows to its width, and the assertion below is
    // about where one line lands
    ["-appdata", appdata, "-window-pos", "1000x900@40x40", "-view", "continuous", epub],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
