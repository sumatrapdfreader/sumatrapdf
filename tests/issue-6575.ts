// #6575: CmdSetDocumentColorsFollowTheme takes off / smart / legacy / toggle,
// so a shortcut switches the mode without the dialog. toggle goes off <-> smart.
// The mode is applied at once and saved.

import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import type { ControlClient } from "./control.ts";
import { makeTopBottomPdf, runStandalone, tmpPath, writeAppdata } from "./util.ts";
import { findCanvas, killAndWait, launchControlled } from "./win-automation.ts";
import { captureWindowPixels, sendCopyDataW, sleep } from "./winapi.ts";

const kCopyDataDdeW = 0x44646557;
const kTag = "issue-6575";

async function dde(client: ControlClient, frame: number, cmd: string): Promise<void> {
  if (!sendCopyDataW(frame, kCopyDataDdeW, cmd)) {
    throw new Error(`${kTag}: DDE ${cmd} not handled`);
  }
  await client.waitForRenderIdle();
}

// with the dark theme a page following it is dark; the canvas center is blank page
function pageIsDark(frame: number): boolean {
  const px = captureWindowPixels(findCanvas(frame));
  if (!px) {
    throw new Error(`${kTag}: no canvas pixels`);
  }
  const i = (Math.floor(px.h / 2) * px.w + Math.floor(px.w / 2)) * 4;
  return px.data[i]! + px.data[i + 1]! + px.data[i + 2]! < 3 * 128;
}

// pages are rendered again after the change: poll
async function expectMode(client: ControlClient, frame: number, cmd: string, dark: boolean) {
  await dde(client, frame, cmd);
  for (let i = 0; i < 40 && pageIsDark(frame) !== dark; i++) {
    await sleep(100);
  }
  if (pageIsDark(frame) !== dark) {
    throw new Error(`${kTag}: after ${cmd} the page should be ${dark ? "dark" : "light"}`);
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath(`${kTag}.pdf`);
  writeFileSync(pdf, makeTopBottomPdf(1));
  const appdata = writeAppdata(
    `${kTag}-appdata`,
    "RestoreSession = false\nTheme = dark\nDocumentColorsFollowTheme = off",
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf], { saveSettings: true });
  try {
    await client.waitForRenderIdle();
    if (pageIsDark(frame)) {
      throw new Error(`${kTag}: off at start should be a light page`);
    }
    await expectMode(client, frame, "[CmdSetDocumentColorsFollowTheme smart]", true);
    await expectMode(client, frame, "[CmdSetDocumentColorsFollowTheme off]", false);
    await expectMode(client, frame, "[CmdSetDocumentColorsFollowTheme toggle]", true);
    await expectMode(client, frame, "[CmdSetDocumentColorsFollowTheme toggle]", false);
    await expectMode(client, frame, "[CmdSetDocumentColorsFollowTheme legacy]", true);
    // an unknown value changes nothing
    await expectMode(client, frame, "[CmdSetDocumentColorsFollowTheme bogus]", true);
    await client.quit();
  } catch (e) {
    await killAndWait(proc);
    throw e;
  }
  await proc.exited;

  const saved = readFileSync(join(appdata, "SumatraPDF-settings.txt"), "utf8");
  if (!/^DocumentColorsFollowTheme = legacy$/m.test(saved)) {
    throw new Error(`${kTag}: the mode wasn't saved:\n${saved}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
