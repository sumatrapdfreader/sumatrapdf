// Ad-hoc: EBookUI.FontName naming a font we can't load must produce a warning
// notification (issue #4600), and must not for a PDF.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { makeMinimalPdf, tmpPath } from "./util.ts";
import { makeFixtures } from "./ad-hoc-issue-4600.ts";
import { captureWindowToPng, launchSumatra, waitForFrame } from "./win-automation.ts";
import { moveWindow, setForegroundWindow, showWindow, sleep, SW_RESTORE } from "./winapi.ts";

async function shot(doc: string, fontName: string, tag: string): Promise<void> {
  const appdata = tmpPath("issue-4600-notif-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    [`EBookUI [`, `\tFontName = ${fontName}`, `]`, `RestoreSession = false`, ``].join("\n"),
  );
  const proc = launchSumatra(["-appdata", appdata, "-view", "single page", "-zoom", "fit page", doc]);
  try {
    const frame = await waitForFrame(proc.pid!);
    showWindow(frame, SW_RESTORE);
    moveWindow(frame, 40, 40, 900, 700);
    setForegroundWindow(frame);
    await sleep(2500);
    captureWindowToPng(frame, join(tmpPath("epub-font"), `notif-${tag}.png`));
    console.log(`  wrote notif-${tag}.png`);
  } finally {
    proc.kill();
    await sleep(300);
  }
}

export async function testit(): Promise<void> {
  makeFixtures();
  Bun.spawnSync(["taskkill", "/F", "/IM", "SumatraPDF.exe"]);
  await sleep(300);
  const epub = join(tmpPath("epub-font"), "none.epub");
  const pdf = tmpPath("issue-4600.pdf");
  writeFileSync(pdf, makeMinimalPdf("font test"));
  await shot(epub, "NoSuchFontXYZ", "epub-missing");
  await shot(epub, "Arial", "epub-ok");
  await shot(pdf, "NoSuchFontXYZ", "pdf-missing");
}

if (import.meta.main) {
  const { runStandalone } = await import("./util.ts");
  await runStandalone(testit);
}
