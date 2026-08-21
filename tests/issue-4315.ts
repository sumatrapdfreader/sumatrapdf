// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/4315
//
// KF8 / AZW3 fixed-layout books store pages as JPEGs in the PDB and leave the
// PalmDoc HTML empty. EngineMobi must still show those images as pages.
//
// Fixture: tests/issue-4315.azw3 (two solid-color JPEG pages).
// Regenerate with: bun tests/issue-4315-make.ts
//
// Run: bun tests/issue-4315.ts [--no-build]

import { existsSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { captureWindowPixels, captureWindowToPng } from "./winapi.ts";
import { findCanvas, sendCommandSync, waitForFrame } from "./win-automation.ts";
import { cmdId, EXE, runStandalone, tmpPath } from "./util.ts";

const AZW3 = join(import.meta.dir, "issue-4315.azw3");

function meanRgb(shot: { w: number; h: number; data: Uint8Array }): { r: number; g: number; b: number } {
  let r = 0,
    g = 0,
    b = 0,
    n = 0;
  const x0 = Math.floor(shot.w * 0.3);
  const x1 = Math.floor(shot.w * 0.7);
  const y0 = Math.floor(shot.h * 0.12);
  const y1 = Math.floor(shot.h * 0.55);
  for (let y = y0; y < y1; y += 2) {
    for (let x = x0; x < x1; x += 2) {
      const i = (y * shot.w + x) * 4;
      b += shot.data[i]!;
      g += shot.data[i + 1]!;
      r += shot.data[i + 2]!;
      n++;
    }
  }
  n = Math.max(n, 1);
  return { r: r / n, g: g / n, b: b / n };
}

export async function testit(): Promise<void> {
  if (!existsSync(AZW3)) {
    throw new Error(`issue-4315: fixture not found: ${AZW3}`);
  }

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      if (!frame) {
        throw new Error("issue-4315: no frame");
      }
      await client.request(ControlCommand.SetNotificationsEnabled, [0]);
      await client.waitForRenderIdle();
      sendCommandSync(frame, cmdId("CmdZoomFitPage"));
      await client.waitForRenderIdle();

      const canvas = findCanvas(frame);
      if (!canvas) {
        throw new Error("issue-4315: no canvas");
      }
      captureWindowToPng(canvas, tmpPath("issue-4315-p1.png"));
      const p1 = captureWindowPixels(canvas);
      if (!p1) {
        throw new Error("issue-4315: capture p1 failed");
      }
      const c1 = meanRgb(p1);
      if (c1.r < 120 || c1.r - c1.b < 40) {
        throw new Error(`issue-4315: page 1 should be red, got r=${c1.r.toFixed(0)} b=${c1.b.toFixed(0)}`);
      }

      sendCommandSync(frame, cmdId("CmdGoToNextPage"));
      await client.waitForRenderIdle();
      captureWindowToPng(canvas, tmpPath("issue-4315-p2.png"));
      const p2 = captureWindowPixels(canvas);
      if (!p2) {
        throw new Error("issue-4315: capture p2 failed");
      }
      const c2 = meanRgb(p2);
      if (c2.b < 120 || c2.b - c2.r < 40) {
        throw new Error(`issue-4315: page 2 should be blue, got r=${c2.r.toFixed(0)} b=${c2.b.toFixed(0)}`);
      }
    },
    [AZW3],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
