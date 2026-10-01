// The stamp's edit toolbar shows its type as text in the Icon chip. It used to
// be cut to the first 2 letters in a square chip; the chip must fit the name,
// so a long name makes the toolbar wider than a short one.
//
// Run: bun tests/stamp-edit-toolbar-name.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { findTopWindow, getWindowRect, isWindowVisible, sleep } from "./winapi.ts";
import { clickAt, findCanvas, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const TOOLBAR_CLASS = "SumatraAnnotEditToolbar";

// width of the edit toolbar with a stamp named `name` selected
async function toolbarDx(name: string): Promise<number> {
  const pdf = tmpPath(`stamp-edit-toolbar-${name}.pdf`);
  writeFileSync(
    pdf,
    assemblePdf([
      "<< /Type /Catalog /Pages 2 0 R >>",
      "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
      `<< /Type /Annot /Subtype /Stamp /P 3 0 R /Rect [200 380 420 460] /Name /${name} >>`,
    ]),
    "latin1",
  );
  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300);
    const res = await client.request(ControlCommand.TestMarkupAnnots, []);
    const m = /type=Stamp[^\n]*screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(String(res[1] ?? ""));
    if (!m) {
      throw new Error(`stamp-edit-toolbar-name: no stamp\n${res[1]}`);
    }
    await clickAt(findCanvas(frame), +m[1]! + Math.floor(+m[3]! / 2), +m[2]! + Math.floor(+m[4]! / 2));
    const deadline = Date.now() + 5000;
    for (;;) {
      const tb = findTopWindow(proc.pid!, TOOLBAR_CLASS);
      if (tb && isWindowVisible(tb)) {
        const r = getWindowRect(tb);
        return r.right - r.left;
      }
      if (Date.now() > deadline) {
        throw new Error(`stamp-edit-toolbar-name: no edit toolbar for ${name}`);
      }
      await sleep(50);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const short = await toolbarDx("Draft");
  const long = await toolbarDx("NotForPublicRelease");
  // "NotForPublicRelease" is ~14 characters longer: far more than 40px
  if (long < short + 40) {
    throw new Error(`stamp-edit-toolbar-name: the chip doesn't fit the name: Draft ${short}px, long ${long}px`);
  }
  console.log("stamp-edit-toolbar-name: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
