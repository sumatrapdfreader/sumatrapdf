// Regression test for issue #6030: CHM in the PDF-style view
// (ChmUI.UseFixedPageUI) must be recolored with FixedPageUI.TextColor /
// BackgroundColor. Recolor was narrowed to MuPDF+DjVu, so CHM pages stayed white.
//
// Reuses the small tests/issue-2737.chm fixture.
//
// Run: bun tests/issue-6030.ts [--no-build]   (or via tests/run-almost-all.ts)

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";
import { captureWindowPixels } from "./winapi.ts";
import { findCanvas, waitForFrame } from "./win-automation.ts";

const CHM = join(import.meta.dir, "issue-2737.chm");

// Distinctive dark page background (not a typical Light-theme chrome color).
const BG_R = 0x1a;
const BG_G = 0x3a;
const BG_B = 0x5c;

function countNear(data: Uint8Array, r: number, g: number, b: number, slop: number): number {
  let n = 0;
  for (let i = 0; i < data.length; i += 4) {
    const pb = data[i]!;
    const pg = data[i + 1]!;
    const pr = data[i + 2]!;
    if (Math.abs(pr - r) <= slop && Math.abs(pg - g) <= slop && Math.abs(pb - b) <= slop) {
      n++;
    }
  }
  return n;
}

export async function testit(): Promise<void> {
  const appData = tmpPath("issue-6030-appdata");
  rmSync(appData, { recursive: true, force: true });
  mkdirSync(appData, { recursive: true });
  writeFileSync(
    join(appData, "SumatraPDF-settings.txt"),
    [
      "UiLanguage = en",
      "CheckForUpdates = false",
      "RestoreSession = false",
      "Theme = Light",
      "DocumentColorsFollowTheme = off",
      "FixedPageUI [",
      "\tTextColor = #e8e0d0",
      "\tBackgroundColor = #1a3a5c",
      "]",
      "ChmUI [",
      "\tUseFixedPageUI = true",
      "]",
      "",
    ].join("\n"),
  );

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      await client.waitForRenderIdle(30000);
      await client.setNotificationsEnabled(false);
      const canvas = findCanvas(frame);
      if (!canvas) {
        throw new Error("issue-6030: no canvas");
      }
      const cap = captureWindowPixels(canvas);
      if (!cap) {
        throw new Error("issue-6030: capture failed");
      }
      const dark = countNear(cap.data, BG_R, BG_G, BG_B, 18);
      const white = countNear(cap.data, 255, 255, 255, 8);
      if (dark < 200) {
        throw new Error(
          `issue-6030: CHM page not recolored to FixedPageUI.BackgroundColor (dark=${dark} white=${white} ${cap.w}x${cap.h})`,
        );
      }
      console.log(`  chm recolor: dark=${dark} white=${white} ${cap.w}x${cap.h} ✓`);
    },
    ["-appdata", appData, CHM],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
