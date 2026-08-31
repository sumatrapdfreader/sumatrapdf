// #6112: MuPDF file-attachment icons in the property row / icon menu were
// drawn upside-down (the paths are Open Iconic y-down, not PDF y-up).
//
// Run: bun tests/issue-6112.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { captureWindowPixels, findTopWindow, packCoords, sendMessage, sleep, WM_COMMAND } from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const TOOLBAR_CLASS = "SumatraAnnotEditToolbar";

function makeBlankPdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
  ]);
}

async function toolbarDump(client: ControlClient): Promise<string> {
  const deadline = Date.now() + 5_000;
  let raw = "";
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkupAnnots, []);
    raw = String(res[1] ?? "");
    const m = /annotEditToolbar .*/.exec(raw);
    if (res[0] === 0 && m && /visible=1/.test(m[0]!)) {
      return m[0]!;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6112: annotation toolbar did not appear\n${raw}`);
    }
    await sleep(40);
  }
}

function parseRect(m: RegExpExecArray | null): { x: number; y: number; dx: number; dy: number } {
  if (!m) {
    return { x: 0, y: 0, dx: 0, dy: 0 };
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

function isInk(cap: { w: number; h: number; data: Uint8Array }, x: number, y: number): boolean {
  if (x < 0 || y < 0 || x >= cap.w || y >= cap.h) {
    return false;
  }
  const i = (y * cap.w + x) * 4;
  return cap.data[i]! + cap.data[i + 1]! + cap.data[i + 2]! < 420;
}

// Point-up star (and a PushPin head): ink in the top band sits near center.
// Upside-down, the top band is two side points and the center is empty.
function topBandCenterInk(
  cap: { w: number; h: number; data: Uint8Array },
  r: { x: number; y: number; dx: number; dy: number },
): { center: number; sides: number } {
  const pad = Math.max(2, Math.floor(Math.min(r.dx, r.dy) * 0.12));
  const x0 = r.x + pad;
  const x1 = r.x + r.dx - pad;
  const y0 = r.y + pad;
  const y1 = r.y + Math.floor(r.dy * 0.38);
  const mid = (x0 + x1) / 2;
  const half = (x1 - x0) / 2;
  let center = 0;
  let sides = 0;
  for (let y = y0; y < y1; y++) {
    for (let x = x0; x < x1; x++) {
      if (!isInk(cap, x, y)) {
        continue;
      }
      if (Math.abs(x + 0.5 - mid) <= half * 0.4) {
        center++;
      } else {
        sides++;
      }
    }
  }
  return { center, sides };
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6112");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "a.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdf, makeBlankPdf(), "latin1");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n",
  );

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-view",
    "single page",
    "-zoom",
    "fit page",
    pdf,
  ]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    findCanvas(frame);
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300);

    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotFileAttachment"), packCoords(150, 300));
    await sleep(400);
    await client.waitForRenderIdle();

    const dump = await toolbarDump(client);
    const iconName = / iconName=(\S+)/.exec(dump)?.[1] ?? "";
    if (iconName !== "PushPin") {
      throw new Error(`issue-6112: icon chip should be PushPin, got ${iconName || "(empty)"}: ${dump}`);
    }
    const placed = parseRect(/ placed=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(dump));
    const icon = parseRect(/[=;]icon:(-?\d+),(-?\d+),(\d+),(\d+)/.exec(dump));
    if (icon.dx < 8) {
      throw new Error(`issue-6112: no icon chip: ${dump}`);
    }
    const tbHwnd = findTopWindow(proc.pid!, TOOLBAR_CLASS);
    if (!tbHwnd) {
      throw new Error("issue-6112: property row window not found");
    }
    const cap = captureWindowPixels(tbHwnd);
    if (!cap) {
      throw new Error("issue-6112: could not capture property row");
    }
    const local = { x: icon.x - placed.x, y: icon.y - placed.y, dx: icon.dx, dy: icon.dy };
    const { center, sides } = topBandCenterInk(cap, local);
    if (center <= sides) {
      throw new Error(`issue-6112: icon looks upside-down (topCenter=${center} topSides=${sides})`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-6112: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
