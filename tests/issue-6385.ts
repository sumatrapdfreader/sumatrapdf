// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6385
//
// A wider border grows a free text's /Rect, but the selection kept the old
// bounds. And a styled (bold) free text is laid out by MuPDF's HTML path,
// which padded the right side twice as much as the left, so centered text
// sat left of center by the border width.
//
// Run: bun tests/issue-6385.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, makeBlankPdf, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import {
  captureWindowPixels,
  enumChildWindows,
  findTopWindow,
  getClassName,
  getWindowRect,
  MK_LBUTTON,
  packCoords,
  sendMessage,
  sleep,
  VK_ESCAPE,
  WM_KEYDOWN,
  WM_KEYUP,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { clickAt, findCanvas, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const CREATE = "CmdCreateAnnotFreeText textsize=6 bgcolor=#ff0000 borderwidth=0 alignment=center";
const BORDER_MAX = 12;

type Rect = { x: number; y: number; dx: number; dy: number };

async function markupDump(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
}

async function selectedRect(client: ControlClient): Promise<Rect> {
  const raw = String((await client.request(ControlCommand.TestAnnotEditorLayout, [0, 0]))[1] ?? "");
  const m = / annotRect=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(raw);
  if (!m) {
    throw new Error(`issue-6385: no selected annotation: ${raw}`);
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

function chipRect(line: string, name: string): Rect {
  const m = new RegExp(`[=;]${name}:(-?\\d+),(-?\\d+),(\\d+),(\\d+)`).exec(line);
  if (!m) {
    throw new Error(`issue-6385: no ${name} chip: ${line}`);
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

async function toolbarLine(client: ControlClient): Promise<string> {
  return /annotEditToolbar .*/.exec(await markupDump(client))?.[0] ?? "";
}

// click a chip of the annotation's property row
async function clickChip(pid: number, line: string, name: string): Promise<void> {
  const placed = /placed=(-?\d+),(-?\d+)/.exec(line)!;
  const row = findTopWindow(pid, "SumatraAnnotEditToolbar");
  if (!row) {
    throw new Error("issue-6385: no annotation property row window");
  }
  const c = chipRect(line, name);
  await clickAt(row, c.x - +placed[1]! + (c.dx >> 1), c.y - +placed[2]! + (c.dy >> 1), 0);
}

// drag the Border Width slider from 0 to its right end and let go
async function dragBorderToMax(client: ControlClient, pid: number): Promise<void> {
  await clickChip(pid, await toolbarLine(client), "border");
  const thickRe = /thickness=(\d+):(-?\d+),(-?\d+),(\d+),(\d+)/;
  const line = await pollUntil(
    async () => /annotColorPopup .*/.exec(await markupDump(client))?.[0] ?? "",
    (l) => +(thickRe.exec(l)?.[4] ?? 0) > 8,
    { error: (l) => `issue-6385: the Border Width slider did not open: ${l}` },
  );
  const th = thickRe.exec(line)!;
  const sl = { x: +th[2]!, y: +th[3]!, dx: +th[4]! };
  const popup = findTopWindow(pid, "SumatraAnnotColorPopup");
  if (!popup) {
    throw new Error("issue-6385: no slider popup window");
  }
  const pr = getWindowRect(popup);
  const y = +th[3]! + (+th[5]! >> 1) - pr.top;
  const xv = (v: number) => sl.x + Math.round((v * (sl.dx - 1)) / BORDER_MAX) - pr.left;
  sendMessage(popup, WM_MOUSEMOVE, 0, packCoords(xv(0), y));
  sendMessage(popup, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(xv(0), y));
  for (let v = 1; v <= BORDER_MAX; v++) {
    sendMessage(popup, WM_MOUSEMOVE, MK_LBUTTON, packCoords(xv(v), y));
  }
  sendMessage(popup, WM_LBUTTONUP, 0, packCoords(xv(BORDER_MAX), y));
  sendMessage(popup, WM_KEYDOWN, VK_ESCAPE, 0);
  await client.waitForRenderIdle();
}

// horizontal offset of the text from the center of the area inside the border
function textCenterOffset(canvas: number, rc: Rect): { offset: number; band: number } {
  const cap = captureWindowPixels(canvas);
  if (!cap) {
    throw new Error("issue-6385: canvas capture failed");
  }
  const px = (x: number, y: number) => {
    const i = (y * cap.w + x) * 4;
    return { b: cap.data[i]!, g: cap.data[i + 1]!, r: cap.data[i + 2]! };
  };
  const isDark = (x: number, y: number) => {
    const p = px(x, y);
    return p.r < 100 && p.g < 100 && p.b < 100;
  };
  const isRed = (x: number, y: number) => {
    const p = px(x, y);
    return p.r > 200 && p.g < 80 && p.b < 80;
  };

  // the border band, scanning a row below the text, away from the handles
  const y = rc.y + Math.floor((rc.dy * 3) / 4);
  let outerL = Math.max(0, rc.x - 40);
  while (outerL < cap.w && !isDark(outerL, y)) {
    outerL++;
  }
  let innerL = outerL;
  while (innerL < cap.w && !isRed(innerL, y)) {
    innerL++;
  }
  let innerR = innerL;
  while (innerR < cap.w && isRed(innerR, y)) {
    innerR++;
  }
  const band = innerL - outerL;
  if (band < 4 || innerR - innerL < 50) {
    throw new Error(`issue-6385: no border found: outer=${outerL} inner=${innerL}..${innerR}`);
  }

  let minX = cap.w;
  let maxX = -1;
  // the top and bottom border rows are dark at the inner left edge, text rows aren't
  for (let yy = Math.max(0, rc.y - 40); yy < Math.min(cap.h, rc.y + rc.dy + 40); yy++) {
    if (isDark(innerL, yy) || !isRed(innerL, yy)) {
      continue;
    }
    for (let x = innerL; x < innerR; x++) {
      if (isDark(x, yy)) {
        minX = Math.min(minX, x);
        maxX = Math.max(maxX, x);
      }
    }
  }
  if (maxX < 0) {
    throw new Error("issue-6385: no text inside the border");
  }
  return { offset: (minX + maxX) / 2 - (innerL + innerR) / 2, band };
}

export async function testit(): Promise<void> {
  if (USE_NG) {
    console.log("issue-6385: skipped for ng (drives the Win32 property row)");
    return;
  }
  const dir = tmpPath("issue-6385");
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n",
  );
  const pdf = join(dir, "blank.pdf");
  writeFileSync(pdf, makeBlankPdf(), "latin1");

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-view",
    "single page",
    "-zoom",
    "200",
    pdf,
  ]);
  const pid = proc.pid!;
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const canvas = findCanvas(frame);
    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));

    // create a centered free text and close its in-place editor
    await client.request(ControlCommand.TestInvokeCommand, [CREATE, 150, 150]);
    const edit = await pollUntil(
      async () => {
        let h = 0;
        enumChildWindows(canvas, (hwnd) => {
          if (getClassName(hwnd) === "Edit") {
            h = hwnd;
            return false;
          }
          return true;
        });
        return h;
      },
      (h) => h !== 0,
      { error: "issue-6385: creating a free text did not open the editor" },
    );
    sendMessage(edit, WM_KEYDOWN, VK_ESCAPE, 0);
    sendMessage(edit, WM_KEYUP, VK_ESCAPE, 0);
    await pollUntil(
      () => toolbarLine(client),
      (l) => /visible=1/.test(l),
      {
        error: (l) => `issue-6385: the free text is not selected: ${l}`,
      },
    );
    await client.waitForRenderIdle();

    await clickChip(pid, await toolbarLine(client), "bold");
    await pollUntil(
      () => toolbarLine(client),
      (l) => /fontStyle=1/.test(l),
      {
        error: (l) => `issue-6385: Bold did not apply: ${l}`,
      },
    );
    await client.waitForRenderIdle();
    const before = await selectedRect(client);

    await dragBorderToMax(client, pid);
    await sleep(200);
    await client.waitForRenderIdle();
    const after = await selectedRect(client);
    const { offset, band } = textCenterOffset(canvas, after);

    // the border grows /Rect by half its width on every side
    const grow = band / 2;
    if (Math.abs(before.x - after.x - grow) > 3 || Math.abs(after.dx - before.dx - band) > 4) {
      throw new Error(
        `issue-6385: selection ${JSON.stringify(after)} did not follow the border (was ${JSON.stringify(before)}, border ${band}px)`,
      );
    }
    if (Math.abs(offset) > band / 4) {
      throw new Error(`issue-6385: centered bold text is ${offset}px off center (border ${band}px)`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("issue-6385: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
