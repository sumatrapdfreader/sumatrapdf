// With FitWidthPercent < 100 Fit Width can be smaller than Fit Page. Zoom in /
// out assumed Fit Page <= Fit Width and fired ReportIf in
// DisplayModel::GetNextZoomStep (crash 2026-11-20-05-38-cb8d). The zoom steps
// must also stay monotonic.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { withControlledSumatra } from "./control.ts";
import { sendCommandSync, waitForFrame } from "./win-automation.ts";
import { cmdId, EXE, makeTopBottomPdf, runStandalone, tmpPath } from "./util.ts";

function parseZoomR(info: string): number {
  const m = /zoomR=([\d.]+)/.exec(info);
  if (!m) {
    throw new Error(`fit-width-percent-zoom-step: no zoomR in '${info}'`);
  }
  return parseFloat(m[1]);
}

export async function testit(): Promise<void> {
  if (process.platform !== "win32") return;
  const dir = tmpPath("fit-width-percent-zoom-step");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, makeTopBottomPdf(2));
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    "RestoreSession = false\nFitWidthPercent = 20\nZoomIncrement = 0\n",
  );

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      if (!frame) {
        throw new Error("fit-width-percent-zoom-step: no frame");
      }
      await client.setNotificationsEnabled(false);
      for (const [cmd, dir] of [
        ["CmdZoomIn", 1],
        ["CmdZoomOut", -1],
      ] as const) {
        const zooms = [parseZoomR(await client.waitForRenderIdle())];
        for (let i = 0; i < 8; i++) {
          sendCommandSync(frame, cmdId(cmd));
          const zoom = parseZoomR(await client.waitForRenderIdle());
          const prev = zooms[zooms.length - 1]!;
          if ((zoom - prev) * dir < -0.0001) {
            throw new Error(`fit-width-percent-zoom-step: ${cmd} went the wrong way: ${zooms.join(" -> ")} -> ${zoom}`);
          }
          zooms.push(zoom);
        }
        console.log(`fit-width-percent-zoom-step: ${cmd} ${zooms.join(" -> ")}`);
      }
    },
    ["-appdata", dir, "-view", "continuous", "-zoom", "fit page", pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
