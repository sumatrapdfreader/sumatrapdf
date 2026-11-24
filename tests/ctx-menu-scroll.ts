// ng's page context menu is not modal, so the view can move before an item
// is picked. The pick must act on the page the menu was opened on, not on
// whatever is under that canvas point now.
//
// Run: bun tests/ctx-menu-scroll.ts [--no-build] -ng

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const kBottomInset = 20;
// SB_THUMBTRACK to this many pixels: page 1 stays in view, the point moves past it
const kScrollPx = 200;
const kThumbTrack = 5;

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R 4 0 R 5 0 R] /Count 3 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>",
  ]);
}

async function str(client: ControlClient, cmd: ControlCommand, args: (string | number)[]): Promise<string> {
  return String((await client.request(cmd, args))[1] ?? "");
}

// a point near the bottom of the canvas: the bottom of page 1 at fit page
async function canvasBottom(client: ControlClient): Promise<{ x: number; y: number }> {
  const m = /canvas=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(await str(client, ControlCommand.TestUiState, []));
  if (!m) {
    throw new Error("ctx-menu-scroll: no canvas rect");
  }
  return { x: +m[1]! + Math.floor(+m[3]! / 2), y: +m[2]! + +m[4]! - kBottomInset };
}

async function menuAt(client: ControlClient, x: number, y: number, want: string): Promise<void> {
  const menu = await str(client, ControlCommand.TestContextMenuAt, [x, y]);
  if (!menu.includes(want)) {
    throw new Error(`ctx-menu-scroll: menu lacks "${want}"\n${menu}`);
  }
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log("ctx-menu-scroll: ng only (orig's popup is modal and keeps its target), skipping");
    return;
  }
  const pdf = tmpPath("ctx-menu-scroll.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const { proc, client } = await launchControlled(["-view", "continuous", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    const { x, y } = await canvasBottom(client);

    // open the menu on page 1, scroll so page 2 is under that point, then pick
    // a command that acts at the menu's point (a stamp is created right away)
    await menuAt(client, x, y, "Add Page 1 to Favorites");
    await client.request(ControlCommand.TestInvokeCommand, ["WM_VSCROLL", kThumbTrack, kScrollPx]);
    await client.waitForRenderIdle();
    const ran = await str(client, ControlCommand.TestUiState, ["ctxcmd", cmdId("CmdCreateAnnotStamp")]);
    if (ran !== "ok") {
      throw new Error(`ctx-menu-scroll: ctxcmd failed: ${ran}`);
    }
    await client.waitForRenderIdle();
    await menuAt(client, x, y, "Add Page 2 to Favorites");

    const annots = await str(client, ControlCommand.TestMarkupAnnots, []);
    const m = /type=Stamp page=(\d+)/.exec(annots);
    if (!m || m[1] !== "1") {
      throw new Error(`ctx-menu-scroll: the stamp is not on page 1, where the menu opened\n${annots}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
