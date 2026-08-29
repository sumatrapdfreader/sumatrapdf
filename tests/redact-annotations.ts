// Redact marks selected text or a dragged rectangle; Apply Redactions
// permanently removes the marked content from the file. Its toolbar button is
// only there while something is marked.
//
// Run: bun tests/redact-annotations.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import {
  clientToScreen,
  getClientRect,
  MK_LBUTTON,
  packCoords,
  postMessage,
  sendMessage,
  setCursorPos,
  sleep,
  WM_CHAR,
  WM_KEYDOWN,
  WM_KEYUP,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import {
  findCanvas,
  killAndWait,
  launchControlled,
  pressEscape,
  sendCommand,
  sendCommandSync,
} from "./win-automation.ts";

const VK_END = 0x23;
const LINE = "SECRETWORD KEEPWORD";

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

async function markupDump(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
}

async function waitForMarkup(client: ControlClient, pred: (raw: string) => boolean, what: string): Promise<string> {
  const deadline = Date.now() + 8000 * SLOW_BUILD_FACTOR;
  let raw = "";
  while (Date.now() < deadline) {
    raw = await markupDump(client);
    if (pred(raw)) {
      return raw;
    }
    await sleep(40);
  }
  throw new Error(`redact-annotations: ${what}\n${raw}`);
}

function parseExpanded(raw: string): string {
  const m = /^expanded=(.*)$/m.exec(raw);
  if (!m) {
    throw new Error(`redact-annotations: no expanded= in:\n${raw}`);
  }
  return m[1]!.trim();
}

async function waitForSelection(client: ControlClient, want: string): Promise<void> {
  const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
  let got = "";
  let raw = "";
  while (Date.now() < deadline) {
    raw = String((await client.request(ControlCommand.TestSelectionVars, ["${selection}"]))[1] ?? "");
    got = parseExpanded(raw);
    if (got === want) {
      return;
    }
    await sleep(25);
  }
  throw new Error(`redact-annotations: selected text is '${got}', want '${want}'\n${raw}`);
}

type Point = { x: number; y: number };

function moveMouse(canvas: number, point: Point, keys = 0): void {
  const screen = clientToScreen(canvas, point.x, point.y);
  setCursorPos(screen.x, screen.y);
  sendMessage(canvas, WM_MOUSEMOVE, keys, packCoords(point.x, point.y));
}

async function drag(canvas: number, start: Point, end: Point): Promise<void> {
  moveMouse(canvas, start);
  sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(start.x, start.y));
  await sleep(50);
  sendMessage(canvas, WM_MOUSEMOVE, MK_LBUTTON, packCoords(end.x, end.y));
}

function releaseDrag(canvas: number, end: Point): void {
  sendMessage(canvas, WM_LBUTTONUP, 0, packCoords(end.x, end.y));
}

function annotBtnHidden(dump: string, cmd: number): string | null {
  const m = new RegExp(`annotation-idx=\\d+ cmd=${cmd} hidden=(\\d)`).exec(dump);
  return m ? m[1]! : null;
}

// The Apply Redactions button is added to / removed from the annotation
// toolbar as marks come and go, so poll rather than read once. Redact is the
// reference: while the whole row is down every button in it reads as hidden.
async function waitApplyButton(client: ControlClient, want: boolean, what: string): Promise<void> {
  const apply = cmdId("CmdApplyRedactions");
  const redact = cmdId("CmdCreateAnnotRedact");
  const deadline = Date.now() + 8000 * SLOW_BUILD_FACTOR;
  let why = "the annotation toolbar never came up";
  for (;;) {
    const raw = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
    if (annotBtnHidden(raw, redact) === "0") {
      const h = annotBtnHidden(raw, apply);
      if (h !== null && (h === "0") === want) {
        return;
      }
      why = h === null ? "no Apply Redactions button in the dump" : `hidden=${h}`;
    }
    if (Date.now() > deadline) {
      throw new Error(`redact-annotations: ${what} (${why})`);
    }
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("redact-annotations");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "secret.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdf, makeTextPdf());
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

    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(200);
    await waitApplyButton(client, false, "Apply Redactions is on the toolbar with nothing marked");

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
      throw new Error(`redact-annotations: keyboard selection did not start\n${dump}`);
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
      throw new Error(`redact-annotations: visual mode did not start\n${dump}`);
    }
    pressVKey(frame, VK_END);
    await waitForSelection(client, LINE);

    sendCommandSync(frame, cmdId("CmdCreateAnnotRedact"));
    let raw = await waitForMarkup(
      client,
      (s) => /type=Redact /.test(s) && /quads=[1-9]/.test(s),
      "text redact mark was not created",
    );
    if (!/page1text=.*SECRETWORD/.test(raw)) {
      throw new Error(`redact-annotations: marking must leave the text in the file\n${raw}`);
    }
    await waitApplyButton(client, true, "Apply Redactions did not appear once text was marked");

    sendCommandSync(frame, cmdId("CmdApplyRedactions"));
    await client.waitForRenderIdle();
    raw = await waitForMarkup(
      client,
      (s) => !/type=Redact /.test(s) && /page1text=/.test(s) && !/page1text=.*SECRETWORD/.test(s),
      "apply did not remove the marked text",
    );
    await pressEscape(frame);
    await pressEscape(frame);

    const canvas = findCanvas(frame);
    const canvasRect = getClientRect(canvas);
    const start = { x: Math.floor(canvasRect.right / 2) - 40, y: Math.floor(canvasRect.bottom / 2) - 30 };
    const end = { x: start.x + 90, y: start.y + 50 };
    sendCommand(frame, cmdId("CmdCreateAnnotRedact"));
    await waitForMarkup(client, (s) => /shapePlacement active=1/.test(s), "redact placement did not start");
    await drag(canvas, start, end);
    releaseDrag(canvas, end);
    await waitForMarkup(client, (s) => /type=Redact /.test(s), "area redact mark was not created");

    sendCommandSync(frame, cmdId("CmdApplyRedactions"));
    await client.waitForRenderIdle();
    await waitForMarkup(client, (s) => !/type=Redact /.test(s), "apply did not remove the area mark");
    await waitApplyButton(client, false, "Apply Redactions stayed on the toolbar with nothing left to apply");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
