// Ad-hoc: EBookUI.FontName naming a font we can't load must produce a warning
// notification (issue #4600), and must not for a PDF.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { makeMinimalPdf, tmpPath } from "./util.ts";
import { makeFixtures } from "./ad-hoc-issue-4600.ts";
import { captureWindowToPng, killAndWait, killProcessesNamed, launchSumatra, waitForFrame } from "./win-automation.ts";
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
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  makeFixtures();
  await killProcessesNamed("SumatraPDF.exe");
  const epub = join(tmpPath("epub-font"), "none.epub");
  const pdf = tmpPath("issue-4600.pdf");
  writeFileSync(pdf, makeMinimalPdf("font test"));
  await shot(epub, "NoSuchFontXYZ", "epub-missing");
  await shot(epub, "Arial", "epub-ok");
  await shot(pdf, "NoSuchFontXYZ", "pdf-missing");
  // the notification has to name the font that actually failed: here the
  // global one is fine and the document's own override is the bad one
  await shotPerFile(epub, "Arial", "NoSuchPerFileFont", "epub-perfile-missing");
}

// like shot(), but the bad font name is in this document's FileStates entry
async function shotPerFile(doc: string, globalFont: string, fileFont: string, tag: string): Promise<void> {
  const appdata = tmpPath("issue-4600-notif-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    [
      `EBookUI [`,
      `\tFontName = ${globalFont}`,
      `]`,
      `RestoreSession = false`,
      `FileStates [`,
      `\t[`,
      `\t\tFilePath = ${doc}`,
      `\t\tEBookUI [`,
      `\t\t\tFontName = ${fileFont}`,
      `\t\t]`,
      `\t]`,
      `]`,
      ``,
    ].join("\n"),
  );
  const proc = launchSumatra(["-appdata", appdata, "-view", "single page", "-zoom", "fit page", doc]);
  try {
    const frame = await waitForFrame(proc.pid!);
    showWindow(frame, SW_RESTORE);
    moveWindow(frame, 40, 40, 900, 700);
    setForegroundWindow(frame);
    await sleep(2500);
    captureWindowToPng(frame, join(tmpPath("epub-font"), `notif-${tag}.png`));
    console.log(`  wrote notif-${tag}.png (per-file FontName='${fileFont}')`);
  } finally {
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  const { runStandalone } = await import("./util.ts");
  await runStandalone(testit);
}
