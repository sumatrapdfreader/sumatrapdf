// The floating selection toolbar is a WS_POPUP in screen coordinates, so it
// must be moved with the frame (WM_MOVE). Without that it stays on the old
// screen spot while / after the window is dragged.
//
// Run: bun tests/selection-toolbar-move.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";
import {
  getWindowRect,
  postMessage,
  setWindowPos,
  sleep,
  SWP_NOACTIVATE,
  SWP_NOZORDER,
  WM_CHAR,
  WM_KEYDOWN,
  WM_KEYUP,
} from "./winapi.ts";

const VK_END = 0x23;

const LINE = "The quick brown fox jumps over the lazy dog";

function makeTextPdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const body: Record<number, Buffer> = {};
  body[1] = enc("<< /Type /Catalog /Pages 2 0 R >>");
  body[2] = enc("<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
  body[6] = enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>");
  const stream = `BT /F1 18 Tf 72 700 Td (${LINE}) Tj ET`;
  body[10] = enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`);
  body[3] = enc(
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
      `/Resources << /Font << /F1 6 0 R >> >> /Contents 10 0 R >>`,
  );
  const maxN = 12;
  const parts: Buffer[] = [enc("%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0]!.length;
  for (let n = 1; n <= maxN; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj\n`), body[n] ?? enc("null"), enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  let xref = `xref\n0 ${maxN + 1}\n0000000000 65535 f \n`;
  for (let n = 1; n <= maxN; n++) {
    xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

function pressVKey(hwnd: number, vk: number): void {
  postMessage(hwnd, WM_KEYDOWN, vk, 0);
  postMessage(hwnd, WM_KEYUP, vk, 0);
}

function writeAppData(dir: string): string {
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    ["RestoreSession = false", "CheckForUpdates = false", "ShowToc = false", "SelectionToolbar = true", ""].join("\n"),
  );
  return dir;
}

function toolbarVisible(raw: string): boolean {
  return /^visible=1$/m.test(raw);
}

function parsePlaced(raw: string): { x: number; y: number; dx: number; dy: number } {
  const m = /^placed=(-?\d+),(-?\d+),(-?\d+),(-?\d+)$/m.exec(raw);
  if (!m) {
    throw new Error(`selection-toolbar-move: no placed= in:\n${raw}`);
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

async function toolbarDump(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestSelectionToolbar, []))[1] ?? "");
}

async function waitForToolbarVisible(client: ControlClient): Promise<string> {
  const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
  let raw = "";
  while (Date.now() < deadline) {
    raw = await toolbarDump(client);
    if (toolbarVisible(raw)) {
      return raw;
    }
    await sleep(40);
  }
  throw new Error(`selection-toolbar-move: selection toolbar did not appear\n${raw}`);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("selection-toolbar-move.pdf");
  writeFileSync(pdf, makeTextPdf());
  const dir = writeAppData(tmpPath("selection-toolbar-move-appdata"));

  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    sendCommandSync(frame, cmdId("CmdSelectTextViaKeyboard"));
    const startDeadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
    let dump = "";
    while (Date.now() < startDeadline) {
      dump = String((await client.request(ControlCommand.TestSelectTextKeyboard, []))[1] ?? "");
      if (/active=1/.test(dump)) {
        break;
      }
      await sleep(25);
    }
    if (!/active=1/.test(dump)) {
      throw new Error(`selection-toolbar-move: keyboard selection did not start\n${dump}`);
    }

    postMessage(frame, WM_CHAR, "v".charCodeAt(0), 0);
    while (Date.now() < startDeadline) {
      dump = String((await client.request(ControlCommand.TestSelectTextKeyboard, []))[1] ?? "");
      if (/visual=1/.test(dump)) {
        break;
      }
      await sleep(25);
    }
    if (!/visual=1/.test(dump)) {
      throw new Error(`selection-toolbar-move: visual mode did not start\n${dump}`);
    }

    pressVKey(frame, VK_END);
    const raw0 = await waitForToolbarVisible(client);
    const tb0 = parsePlaced(raw0);
    const frame0 = getWindowRect(frame);

    // several SetWindowPos calls, each generating WM_MOVE: this is the "while
    // moving" path (live drag) as well as the "after" snap at the last step
    const steps = [
      { dx: -48, dy: 0 },
      { dx: -48, dy: 36 },
      { dx: 0, dy: 36 },
    ];
    let originX = frame0.left;
    let originY = frame0.top;
    const width = frame0.right - frame0.left;
    const height = frame0.bottom - frame0.top;
    for (const step of steps) {
      originX += step.dx;
      originY += step.dy;
      if (!setWindowPos(frame, originX, originY, width, height, SWP_NOZORDER | SWP_NOACTIVATE)) {
        throw new Error(`selection-toolbar-move: SetWindowPos failed at ${originX},${originY}`);
      }
      const raw = await toolbarDump(client);
      if (!toolbarVisible(raw)) {
        throw new Error(`selection-toolbar-move: toolbar hid after move to ${originX},${originY}\n${raw}`);
      }
      const tb = parsePlaced(raw);
      const frameNow = getWindowRect(frame);
      const frameDx = frameNow.left - frame0.left;
      const frameDy = frameNow.top - frame0.top;
      const tbDx = tb.x - tb0.x;
      const tbDy = tb.y - tb0.y;
      if (tbDx !== frameDx || tbDy !== frameDy) {
        throw new Error(
          `selection-toolbar-move: toolbar did not follow the frame ` +
            `(frame ${frameDx},${frameDy} toolbar ${tbDx},${tbDy}) ` +
            `at ${originX},${originY}\n${raw}`,
        );
      }
      if (tb.dx !== tb0.dx || tb.dy !== tb0.dy) {
        throw new Error(
          `selection-toolbar-move: toolbar size changed ${tb.dx},${tb.dy} vs ${tb0.dx},${tb0.dy}\n${raw}`,
        );
      }
    }

    console.log("selection-toolbar-move: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
