// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6039
//
// Page Grid was painted only for fixed-page documents even though EPUBs use
// the same paginated canvas and page-coordinate transforms.

import { mkdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { withControlledSumatra } from "./control.ts";
import { EXE, ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import { captureWindowPixels, sleep } from "./winapi.ts";
import { findCanvas, sendCommand, waitForFrame } from "./win-automation.ts";

const EPUB = join(ROOT, "tests", "issue-5846.epub");

function countGridColor(data: Uint8Array): number {
  let n = 0;
  for (let i = 0; i < data.length; i += 4) {
    const b = data[i]!;
    const g = data[i + 1]!;
    const r = data[i + 2]!;
    if (Math.abs(r - 128) <= 8 && Math.abs(g - 128) <= 8 && Math.abs(b - 255) <= 8) {
      n++;
    }
  }
  return n;
}

export async function testit(): Promise<void> {
  const appdata = tmpPath("issue-6039-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      await client.waitForRenderIdle(30_000);
      await client.setNotificationsEnabled(false);
      const canvas = findCanvas(frame);
      if (!canvas) {
        throw new Error("issue-6039: no EPUB canvas");
      }
      const before = captureWindowPixels(canvas);
      if (!before) {
        throw new Error("issue-6039: initial capture failed");
      }
      const nBefore = countGridColor(before.data);

      sendCommand(frame, cmdId("CmdTogglePageGrid"));
      await client.waitForRenderIdle(30_000);
      const deadline = Date.now() + 1000;
      let nAfter = nBefore;
      while (Date.now() < deadline) {
        const after = captureWindowPixels(canvas);
        if (!after) {
          throw new Error("issue-6039: grid capture failed");
        }
        nAfter = countGridColor(after.data);
        if (nAfter >= nBefore + 40) {
          break;
        }
        await sleep(25);
      }
      if (nAfter < nBefore + 40) {
        throw new Error(`issue-6039: EPUB page grid did not show (grid pixels before=${nBefore} after=${nAfter})`);
      }
      console.log(`  EPUB grid pixels ${nBefore} -> ${nAfter} ✓`);
    },
    ["-appdata", appdata, EPUB],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
