// #6166: after highlighting text, a drag on that highlight must start a text
// selection so the marked words can be copied. Creating the highlight turns
// on Edit PDF and selects the annotation; those used to eat the click.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import {
  MK_CONTROL,
  MK_LBUTTON,
  packCoords,
  postChar,
  postMessage,
  sleep,
  VK_END,
  WM_KEYDOWN,
  WM_KEYUP,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { clickAt, findCanvas, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const WORD = "selectme";

function makePdf(): string {
  const stream = `BT /F1 24 Tf 72 720 Td (${WORD} here) Tj ET`;
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R " +
      "/Resources << /Font << /F1 5 0 R >> >> >>",
    `<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`,
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>",
  ]);
}

type Markup = {
  selected: boolean;
  editToolbar: boolean;
  annotations: number;
  screen: { x: number; y: number; dx: number; dy: number } | null;
  raw: string;
};

async function markup(client: ControlClient): Promise<Markup> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  const state = /state selected=(\d+) hover=\d+ editToolbar=(\d+)/.exec(raw);
  const count = /annotations=(\d+)/.exec(raw);
  const screen = /screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
  if (res[0] !== 0 || !state || !count) {
    throw new Error(`issue-6166: could not read markup state\n${raw}`);
  }
  return {
    selected: state[1] === "1",
    editToolbar: state[2] === "1",
    annotations: +count[1]!,
    screen: screen ? { x: +screen[1]!, y: +screen[2]!, dx: +screen[3]!, dy: +screen[4]! } : null,
    raw,
  };
}

async function selectedText(client: ControlClient): Promise<string> {
  const raw = String((await client.request(ControlCommand.TestSelectionVars, ["${selection}"]))[1] ?? "");
  return /^expanded=(.*)$/m.exec(raw)?.[1] ?? "";
}

async function selectLineWithKeyboard(client: ControlClient, frame: number): Promise<void> {
  const deadline = Date.now() + 4_000 * SLOW_BUILD_FACTOR;
  sendCommandSync(frame, cmdId("CmdSelectTextViaKeyboard"));
  let dump = "";
  while (Date.now() < deadline) {
    dump = String((await client.request(ControlCommand.TestSelectTextKeyboard, []))[1] ?? "");
    if (/active=1/.test(dump)) {
      break;
    }
    await sleep(25);
  }
  if (!/active=1/.test(dump)) {
    throw new Error(`issue-6166: keyboard selection did not start\n${dump}`);
  }
  await postChar(frame, "v");
  while (Date.now() < deadline) {
    dump = String((await client.request(ControlCommand.TestSelectTextKeyboard, []))[1] ?? "");
    if (/visual=1/.test(dump)) {
      break;
    }
    await sleep(25);
  }
  if (!/visual=1/.test(dump)) {
    throw new Error(`issue-6166: visual mode did not start\n${dump}`);
  }
  postMessage(frame, WM_KEYDOWN, VK_END, 0);
  postMessage(frame, WM_KEYUP, VK_END, 0);
  await sleep(200);
}

async function dragSelect(canvas: number, x0: number, y0: number, x1: number, y1: number): Promise<void> {
  postMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(x0, y0));
  await sleep(150);
  const steps = 8;
  for (let i = 1; i <= steps; i++) {
    const x = Math.round(x0 + ((x1 - x0) * i) / steps);
    const y = Math.round(y0 + ((y1 - y0) * i) / steps);
    postMessage(canvas, WM_MOUSEMOVE, MK_LBUTTON, packCoords(x, y));
    await sleep(60);
  }
  postMessage(canvas, WM_LBUTTONUP, 0, packCoords(x1, y1));
  await sleep(300);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6166");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "highlight.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdf, makePdf(), "latin1");
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

    await selectLineWithKeyboard(client, frame);
    sendCommandSync(frame, cmdId("CmdCreateAnnotHighlight"));

    const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
    let s: Markup | undefined;
    while (Date.now() < deadline) {
      s = await markup(client);
      if (s.editToolbar && s.annotations === 1 && s.screen && s.screen.dx > 4) {
        break;
      }
      await sleep(40);
    }
    if (!s || !s.editToolbar || s.annotations !== 1 || !s.screen || s.screen.dx <= 4) {
      throw new Error(`issue-6166: highlight was not created in Edit PDF mode\n${s?.raw ?? ""}`);
    }

    const canvas = findCanvas(frame);
    const y = s.screen.y + Math.floor(s.screen.dy / 2);
    await dragSelect(canvas, s.screen.x + 2, y, s.screen.x + s.screen.dx - 2, y);

    const textDeadline = Date.now() + 3_000 * SLOW_BUILD_FACTOR;
    let text = "";
    while (Date.now() < textDeadline) {
      text = await selectedText(client);
      if (text.includes(WORD)) {
        break;
      }
      await sleep(50);
    }
    if (!text.includes(WORD)) {
      throw new Error(`issue-6166: drag on highlight did not select text (got ${JSON.stringify(text)})`);
    }

    const cr = await markup(client);
    const hx = cr.screen ? cr.screen.x + Math.floor(cr.screen.dx / 2) : s.screen.x + 4;
    const hy = cr.screen ? cr.screen.y + Math.floor(cr.screen.dy / 2) : y;
    await clickAt(canvas, hx, hy, 200, MK_CONTROL);
    const afterCtrl = await markup(client);
    if (!afterCtrl.selected || !afterCtrl.editToolbar) {
      throw new Error(`issue-6166: Ctrl+click did not select the highlight\n${afterCtrl.raw}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-6166: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
