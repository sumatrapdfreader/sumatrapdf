// #6534: the box a free text is edited in hid the page under it with solid
// white. For an annotation without a background it must show the page through
// (the annotation itself is left out while it's edited, so its text isn't
// doubled). CmdEditAnnotationText (context menu: Edit Text) opens that box for
// the selected annotation.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import type { ControlClient } from "./control.ts";
import { ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util.ts";
import {
  captureWindowPixels,
  clientToScreen,
  enumChildWindows,
  getClassName,
  getControlText,
  MK_LBUTTON,
  packCoords,
  sendMessage,
  setCursorPos,
  sleep,
  VK_ESCAPE,
  WM_KEYDOWN,
  WM_KEYUP,
  WM_LBUTTONDBLCLK,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

type Box = { x: number; y: number; dx: number; dy: number };
type State = { raw: string; screen: Box; edit: boolean; editRect: Box; selected: boolean };

async function state(client: ControlClient): Promise<State> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkupAnnots, []);
    const raw = String(res[1] ?? "");
    const ft = /type=FreeText[^\n]*?screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
    const ed = /freeTextEdit active=(\d) rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
    const sel = /state selected=(\d)/.exec(raw);
    if (res[0] === 0 && ft && ed && sel) {
      return {
        raw,
        screen: { x: +ft[1]!, y: +ft[2]!, dx: +ft[3]!, dy: +ft[4]! },
        edit: ed[1] === "1",
        editRect: { x: +ed[2]!, y: +ed[3]!, dx: +ed[4]!, dy: +ed[5]! },
        selected: sel[1] === "1",
      };
    }
    if (Date.now() > deadline) throw new Error(`issue-6534: can't read the annotation state\n${raw}`);
    await sleep(50);
  }
}

async function waitFor(client: ControlClient, pred: (s: State) => boolean, what: string): Promise<State> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const s = await state(client);
    if (pred(s)) return s;
    if (Date.now() > deadline) throw new Error(`issue-6534: ${what}\n${s.raw}`);
    await sleep(40);
  }
}

async function doubleClick(client: ControlClient, canvas: number, x: number, y: number): Promise<void> {
  if (USE_NG) {
    await client.request(ControlCommand.TestInput, ["click", x, y, 0, 0]);
    await client.request(ControlCommand.TestInput, ["click", x, y, 0, 0]);
    return;
  }
  const s = clientToScreen(canvas, x, y);
  setCursorPos(s.x, s.y);
  const pt = packCoords(x, y);
  sendMessage(canvas, WM_MOUSEMOVE, 0, pt);
  sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, pt);
  sendMessage(canvas, WM_LBUTTONUP, 0, pt);
  sendMessage(canvas, WM_LBUTTONDBLCLK, MK_LBUTTON, pt);
  sendMessage(canvas, WM_LBUTTONUP, 0, pt);
}

// share of the box's inner pixels that show the page's gray square
async function grayShare(client: ControlClient, canvas: number, box: Box): Promise<number> {
  let share = 0;
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  do {
    await client.waitForRenderIdle();
    const px = captureWindowPixels(canvas)!;
    let gray = 0;
    let n = 0;
    for (let y = box.y + 4; y < box.y + box.dy - 4; y++) {
      for (let x = box.x + 4; x < box.x + box.dx - 4; x++) {
        const i = (y * px.w + x) * 4;
        const [b, g, r] = [px.data[i]!, px.data[i + 1]!, px.data[i + 2]!];
        n++;
        if (Math.abs(r - 0x80) < 24 && Math.abs(g - 0x80) < 24 && Math.abs(b - 0x80) < 24) gray++;
      }
    }
    share = n > 0 ? gray / n : 0;
    if (share > 0.5) return share;
    await sleep(100);
  } while (Date.now() < deadline);
  return share;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6534");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "freetext.pdf");
  // a gray square in the page content, a borderless free text without a
  // background over it
  const stream = "0.5 g 60 300 400 300 re f";
  writeFileSync(
    pdf,
    assemblePdf([
      "<< /Type /Catalog /Pages 2 0 R >>",
      "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 5 0 R /Annots [4 0 R] >>",
      "<< /Type /Annot /Subtype /FreeText /P 3 0 R /Rect [100 400 400 500] /Contents (word) /DA (/Helv 12 Tf 1 0 0 rg) /BS << /W 0 >> >>",
      `<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`,
    ]),
    "latin1",
  );
  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const canvas = findCanvas(frame);
    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));
    await client.waitForRenderIdle();
    await sleep(200);
    const before = await state(client);

    const sc = before.screen;
    await doubleClick(client, canvas, sc.x + Math.floor(sc.dx / 2), sc.y + Math.floor(sc.dy / 2));
    const s = await waitFor(client, (st) => st.edit, "double-click did not edit the free text");
    const share = await grayShare(client, canvas, s.editRect);
    if (share < 0.5) {
      throw new Error(`issue-6534: the edit box hides the page (${Math.round(share * 100)}% of it shows the page)`);
    }

    // close it, then open it again from the command
    let target = frame;
    if (!USE_NG) {
      enumChildWindows(canvas, (h) => {
        if (getClassName(h) !== "Edit" || !getControlText(h).startsWith("word")) return true;
        target = h;
        return false;
      });
    }
    sendMessage(target, WM_KEYDOWN, VK_ESCAPE, 0);
    sendMessage(target, WM_KEYUP, VK_ESCAPE, 0);
    await waitFor(client, (st) => !st.edit && st.selected, "Esc did not close the edit box, leaving it selected");
    sendCommandSync(frame, cmdId("CmdEditAnnotationText"));
    await waitFor(client, (st) => st.edit, "CmdEditAnnotationText did not edit the selected free text");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
