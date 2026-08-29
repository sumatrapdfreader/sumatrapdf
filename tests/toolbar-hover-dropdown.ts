// Resting the mouse on a toolbar button can open a drop-down, after the delay a
// tooltip takes, and the tooltip gives way to it. The Edit PDF toolbar's Save
// button uses it for the three ways to end an editing session, each row showing
// its keyboard shortcut; Save to a new PDF is no longer its own button.
//
// Run: bun tests/toolbar-hover-dropdown.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import {
  captureWindowToPng,
  clientToScreen,
  findTopWindow,
  getWindowRect,
  isWindowVisible,
  packCoords,
  sendMessage,
  setCursorPos,
  sleep,
  WM_COMMAND,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { clickAt, findChildByClass, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const TOOLBAR_CLASS = "SUMATRA_VIRT_TOOLBAR";
const MENU_CLASS = "SumatraToolbarHoverMenu";

type Btn = { cmd: number; hidden: boolean; x: number; y: number; dx: number; dy: number };

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    "<< /Type /Annot /Subtype /Square /P 3 0 R /Rect [72 600 220 700] /C [1 0 0] >>",
  ]);
}

async function annotButtons(client: ControlClient): Promise<Btn[]> {
  const raw = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
  const res: Btn[] = [];
  const re = /annotation-idx=\d+ cmd=(\d+) hidden=(\d) enabled=\d rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/g;
  let m: RegExpExecArray | null;
  while ((m = re.exec(raw)) !== null) {
    const x = +m[3]!;
    const y = +m[4]!;
    res.push({ cmd: +m[1]!, hidden: m[2] === "1", x, y, dx: +m[5]! - x, dy: +m[6]! - y });
  }
  return res;
}

async function waitAnnotButton(client: ControlClient, cmd: number, what: string): Promise<Btn> {
  const deadline = Date.now() + 8000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const b = (await annotButtons(client)).find((v) => v.cmd === cmd && !v.hidden && v.dx > 0);
    if (b) {
      return b;
    }
    if (Date.now() > deadline) {
      throw new Error(`toolbar-hover-dropdown: ${what}`);
    }
    await sleep(50);
  }
}

async function annotTip(client: ControlClient, cmd: number): Promise<string | null> {
  const raw = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
  const m = new RegExp(`annotation-idx=\\d+ cmd=${cmd} .* tip=(.*)$`, "m").exec(raw);
  return m ? m[1]!.trim() : null;
}

async function annotCount(client: ControlClient): Promise<number> {
  const raw = String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
  return +(/annotations=(\d+)/.exec(raw)?.[1] ?? -1);
}

async function waitAnnotCount(client: ControlClient, want: number, what: string): Promise<void> {
  const deadline = Date.now() + 6000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const n = await annotCount(client);
    if (n === want) {
      return;
    }
    if (Date.now() > deadline) {
      throw new Error(`toolbar-hover-dropdown: ${what} (annotations=${n}, want ${want})`);
    }
    await sleep(50);
  }
}

async function waitMenu(pid: number, want: boolean, what: string): Promise<number> {
  const deadline = Date.now() + 6000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const h = findTopWindow(pid, MENU_CLASS);
    const shown = h !== 0 && isWindowVisible(h);
    if (shown === want) {
      return h;
    }
    if (Date.now() > deadline) {
      throw new Error(`toolbar-hover-dropdown: ${what}`);
    }
    await sleep(50);
  }
}

// rest the mouse on a toolbar button: the real cursor has to be there (the
// drop-down reads it) and the toolbar has to see a move
function hoverToolbar(toolbar: number, x: number, y: number): void {
  const s = clientToScreen(toolbar, x, y);
  setCursorPos(s.x, s.y);
  sendMessage(toolbar, WM_MOUSEMOVE, 0, packCoords(x, y));
}

export async function testit(): Promise<void> {
  const dir = tmpPath("toolbar-hover-dropdown");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "annots.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata);
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
    const pid = proc.pid!;
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300);

    const save = await waitAnnotButton(client, cmdId("CmdSaveAnnotations"), "no Save button on the Edit PDF toolbar");
    if ((await annotButtons(client)).some((b) => b.cmd === cmdId("CmdSaveAnnotationsNewFile"))) {
      throw new Error("toolbar-hover-dropdown: Save to a new PDF is still its own toolbar button");
    }

    const toolbar = findChildByClass(frame, TOOLBAR_CLASS);
    if (!toolbar) {
      throw new Error("toolbar-hover-dropdown: no toolbar");
    }
    if (findTopWindow(pid, MENU_CLASS)) {
      throw new Error("toolbar-hover-dropdown: the drop-down was up before anything was hovered");
    }

    // something to save, so the rows are live
    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotText"), packCoords(150, 300));
    await waitAnnotCount(client, 2, "could not create an annotation to save");

    // creating it may have relaid out the row, so re-read where Save sits
    const save2 = await waitAnnotButton(client, cmdId("CmdSaveAnnotations"), "Save button vanished");
    const cx = save2.x + Math.floor(save2.dx / 2);
    const cy = save2.y + Math.floor(save2.dy / 2);
    hoverToolbar(toolbar, cx, cy);
    const menu = await waitMenu(pid, true, "resting on Save did not open the drop-down");
    await sleep(200);
    captureWindowToPng(menu, join(dir, "dropdown.png"));

    // directly under the button (it slides left when its width would run off
    // the right edge of the screen, so only the button's centre has to be in it)
    const mr = getWindowRect(menu);
    const below = clientToScreen(toolbar, save2.x, save2.y + save2.dy);
    const centreX = below.x + Math.floor(save2.dx / 2);
    if (Math.abs(mr.top - below.y) > 2 || mr.left > centreX || mr.right < centreX) {
      throw new Error(
        `toolbar-hover-dropdown: drop-down is not under the button ${JSON.stringify(mr)} vs ${JSON.stringify(below)}`,
      );
    }
    const dy = mr.bottom - mr.top;
    if (mr.right - mr.left < save2.dx || dy < save2.dy) {
      throw new Error(`toolbar-hover-dropdown: drop-down looks too small ${JSON.stringify(mr)}`);
    }

    // the button gives up its tooltip while the drop-down is up, so nothing
    // brings the bubble back over it
    if (await annotTip(client, cmdId("CmdSaveAnnotations"))) {
      throw new Error("toolbar-hover-dropdown: the button kept its tooltip under the drop-down");
    }

    // the third row is Discard changes: clicking it runs that command, which
    // proves there are three rows and that a click reaches the right one
    const rowDy = Math.floor(dy / 3);
    const rowY = 2 * rowDy + Math.floor(rowDy / 2);
    // a row lights up under the mouse
    setCursorPos(mr.left + 40, mr.top + rowY);
    sendMessage(menu, WM_MOUSEMOVE, 0, packCoords(40, rowY));
    await clickAt(menu, 20, rowY, 300);
    await waitMenu(pid, false, "clicking a row did not close the drop-down");
    await waitAnnotCount(client, 1, "the third row did not discard the changes");
    if (!(await annotTip(client, cmdId("CmdSaveAnnotations")))) {
      throw new Error("toolbar-hover-dropdown: the button did not get its tooltip back");
    }

    // moving the mouse off it closes it too
    hoverToolbar(toolbar, cx, cy);
    await waitMenu(pid, true, "the drop-down did not open a second time");
    const away = clientToScreen(toolbar, 4, 4);
    setCursorPos(away.x, away.y - 200);
    sendMessage(toolbar, WM_MOUSEMOVE, 0, packCoords(4, 4));
    await waitMenu(pid, false, "moving the mouse away did not close the drop-down");
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("toolbar-hover-dropdown: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
