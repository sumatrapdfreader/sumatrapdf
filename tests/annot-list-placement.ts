// The Annotations list opens on the roomier side of the main window, or on the
// right edge of the screen when maximized / fullscreen. Position is session-only.
//
// Run: bun tests/annot-list-placement.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { findTopWindow, getWindowPid, getWindowRect, getWorkArea, sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const FLOAT_CLASS = "SUMATRA_ANNOT_FILTER_WND";

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    "<< /Type /Annot /Subtype /Text /P 3 0 R /Rect [72 50 92 70] /Contents (n) >>",
  ]);
}

async function waitFloat(
  client: ControlClient,
  visible: boolean,
): Promise<{ x: number; y: number; dx: number; dy: number }> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  let raw = "";
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkupAnnots, []);
    raw = String(res[1] ?? "");
    const m = /annotFilter floatVisible=(\d+) floatRect=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(raw);
    if (res[0] === 0 && m && m[1] === (visible ? "1" : "0")) {
      return { x: +m[2]!, y: +m[3]!, dx: +m[4]!, dy: +m[5]! };
    }
    if (Date.now() > deadline) {
      throw new Error(`annot-list-placement: floatVisible=${visible ? 1 : 0} never matched\n${raw}`);
    }
    await sleep(40);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("annot-list-placement");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "a.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));
    await sleep(200);

    sendCommandSync(frame, cmdId("CmdFindAnnotation"));
    const first = await waitFloat(client, true);
    const fr = getWindowRect(frame);
    const wa = getWorkArea();
    const spaceRight = wa.right - fr.right;
    const spaceLeft = fr.left - wa.left;
    const listMid = first.x + first.dx / 2;
    const frameMid = (fr.left + fr.right) / 2;
    if (spaceRight >= spaceLeft) {
      if (listMid < frameMid) {
        throw new Error(
          `annot-list-placement: more room on the right, list was left: list=${JSON.stringify(first)} frame=${JSON.stringify(fr)}`,
        );
      }
    } else if (listMid > frameMid) {
      throw new Error(
        `annot-list-placement: more room on the left, list was right: list=${JSON.stringify(first)} frame=${JSON.stringify(fr)}`,
      );
    }

    sendCommandSync(frame, cmdId("CmdFindAnnotation"));
    await waitFloat(client, false);
    sendCommandSync(frame, cmdId("CmdFindAnnotation"));
    const again = await waitFloat(client, true);
    if (again.x !== first.x || again.y !== first.y || again.dx !== first.dx || again.dy !== first.dy) {
      throw new Error(
        `annot-list-placement: session pos not kept: first=${JSON.stringify(first)} again=${JSON.stringify(again)}`,
      );
    }

    sendCommandSync(frame, cmdId("CmdFindAnnotation"));
    await waitFloat(client, false);
    sendCommandSync(frame, cmdId("CmdToggleFullscreen"));
    await client.waitForRenderIdle();
    await sleep(200);
    sendCommandSync(frame, cmdId("CmdFindAnnotation"));
    const fs = await waitFloat(client, true);
    const fsFr = getWindowRect(frame);
    if (Math.abs(fs.x + fs.dx - fsFr.right) > 8) {
      throw new Error(
        `annot-list-placement: fullscreen list not on right edge: list=${JSON.stringify(fs)} frame=${JSON.stringify(fsFr)}`,
      );
    }
    const fsMidY = fs.y + fs.dy / 2;
    const frMidY = (fsFr.top + fsFr.bottom) / 2;
    if (Math.abs(fsMidY - frMidY) > 40) {
      throw new Error(
        `annot-list-placement: fullscreen list not y-centered: list=${JSON.stringify(fs)} frame=${JSON.stringify(fsFr)}`,
      );
    }

    const pid = getWindowPid(frame);
    if (!findTopWindow(pid, FLOAT_CLASS)) {
      throw new Error("annot-list-placement: floating window hwnd not found");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("annot-list-placement: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
