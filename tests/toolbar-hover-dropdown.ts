// Resting the mouse on a toolbar button can open a drop-down, after the delay a
// tooltip takes, and the tooltip gives way to it. The Edit PDF toolbar's Save
// button uses it for the three ways to end an editing session, each row showing
// its keyboard shortcut; Save to a new PDF is no longer its own button. The Zoom
// In / Zoom Out buttons use it for the zoom levels, as one compact row with the
// level in use boxed and sitting under the button.
//
// Run: bun tests/toolbar-hover-dropdown.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import {
  captureWindowToPng,
  getWorkArea,
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

// what the zoom drop-down lists: the levels the zoom buttons step through
// (DisplayModel's defaultZoomLevels), smallest first, with the two fit modes
// where 100% is
const ZOOM_LEVELS = [
  "8.33%",
  "12.5%",
  "18%",
  "25%",
  "33.33%",
  "50%",
  "66.67%",
  "75%",
  "100%",
  "Fit Page",
  "Fit Width",
  "125%",
  "150%",
  "200%",
  "300%",
  "400%",
  "600%",
  "800%",
  "1000%",
  "1200%",
  "1600%",
  "2000%",
  "2400%",
  "3200%",
  "4800%",
  "6400%",
];

// ZoomLevels in the settings replaces the levels, for the buttons and the strip
// alike; the fit modes stay
const CUSTOM_ZOOM_LEVELS = [50, 75, 100, 150, 300];
const CUSTOM_ZOOM_STRIP = ["50%", "75%", "100%", "Fit Page", "Fit Width", "150%", "300%"];

// a window away from the right edge of the screen: the strip runs to the right
// of the zoom buttons, and one that would hang off the monitor is slid back on,
// which is right but would move the level in use out from under the button
const WINDOW_POS = "1100x900@40x40";

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

async function mainButtons(client: ControlClient): Promise<Btn[]> {
  const raw = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
  const res: Btn[] = [];
  const re = /^idx=\d+ cmd=(\d+) hidden=(\d) rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/gm;
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

// keep resting the mouse on a button until its drop-down is up: something else
// on the machine can yank the cursor away, and the drop-down reads where it
// actually is, so a single move is not enough to rely on
async function hoverUntilMenu(toolbar: number, pid: number, x: number, y: number, what: string): Promise<number> {
  const deadline = Date.now() + 8000 * SLOW_BUILD_FACTOR;
  for (;;) {
    hoverToolbar(toolbar, x, y);
    const h = findTopWindow(pid, MENU_CLASS);
    if (h !== 0 && isWindowVisible(h)) {
      return h;
    }
    if (Date.now() > deadline) {
      throw new Error(`toolbar-hover-dropdown: ${what}`);
    }
    await sleep(100);
  }
}

async function zoomLabel(client: ControlClient): Promise<string> {
  const raw = String((await client.request(ControlCommand.TestDisplayMode, ["get"]))[1] ?? "");
  return /zoom=(.+)$/m.exec(raw)?.[1]?.trim() ?? "";
}

async function waitZoom(client: ControlClient, want: string, what: string): Promise<void> {
  const deadline = Date.now() + 6000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const z = await zoomLabel(client);
    if (z === want) {
      return;
    }
    if (Date.now() > deadline) {
      throw new Error(`toolbar-hover-dropdown: ${what} (zoom=${z}, want ${want})`);
    }
    await sleep(50);
  }
}

type Item = { cmd: number; current: boolean; x: number; y: number; x2: number; y2: number; text: string };

// what the drop-down that is up is showing, in screen coordinates
async function dropdownItems(client: ControlClient): Promise<Item[]> {
  const raw = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
  const re = /^dropdown-item idx=\d+ cmd=(\d+) current=(\d) rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+) text=(.*)$/gm;
  const res: Item[] = [];
  let m: RegExpExecArray | null;
  while ((m = re.exec(raw)) !== null) {
    res.push({
      cmd: +m[1]!,
      current: m[2] === "1",
      x: +m[3]!,
      y: +m[4]!,
      x2: +m[5]!,
      y2: +m[6]!,
      text: m[7]!.trim(),
    });
  }
  return res;
}

// the level in use has to end up under the middle of the button: that is what
// makes the strip worth opening from a button the mouse is already on. A strip
// this wide can run off the monitor, and one that does is slid back on, which
// is right and moves the level off the button by however much it took
function checkCurrentUnderButton(items: Item[], want: string, btnCentreX: number, menu: number): void {
  const current = items.filter((it) => it.current);
  if (current.length !== 1 || current[0]!.text !== want) {
    throw new Error(
      `toolbar-hover-dropdown: want only ${want} marked as the current zoom, got ` +
        `[${current.map((it) => it.text).join()}]`,
    );
  }
  const centre = Math.floor((current[0]!.x + current[0]!.x2) / 2);
  if (Math.abs(centre - btnCentreX) <= 4) {
    return;
  }
  const wa = getWorkArea();
  const mr = getWindowRect(menu);
  const clamped = mr.left <= wa.left + 1 || mr.right >= wa.right - 1;
  if (!clamped) {
    throw new Error(`toolbar-hover-dropdown: ${want} is at x=${centre}, not under the button at x=${btnCentreX}`);
  }
  // slid back on: it went as far as it could towards the button
  const wantLeft = mr.left <= wa.left + 1 ? wa.left : wa.right - (mr.right - mr.left);
  if (Math.abs(mr.left - wantLeft) > 4) {
    throw new Error(`toolbar-hover-dropdown: the strip is neither under ${want} nor against the screen edge`);
  }
}

// a second instance, this one told to use its own zoom levels
async function checkCustomZoomLevels(dir: string, pdf: string): Promise<void> {
  const appdata = join(dir, "appdata-custom");
  mkdirSync(appdata);
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n" +
      `ZoomLevels = ${CUSTOM_ZOOM_LEVELS.join(" ")}\n`,
  );
  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-window-pos",
    WINDOW_POS,
    "-zoom",
    "100",
    pdf,
  ]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const toolbar = findChildByClass(frame, TOOLBAR_CLASS);
    if (!toolbar) {
      throw new Error("toolbar-hover-dropdown: no toolbar");
    }
    const zoomIn = (await mainButtons(client)).find((b) => b.cmd === cmdId("CmdZoomIn") && !b.hidden && b.dx > 0);
    if (!zoomIn) {
      throw new Error("toolbar-hover-dropdown: no Zoom In button");
    }
    const zx = zoomIn.x + Math.floor(zoomIn.dx / 2);
    const zy = zoomIn.y + Math.floor(zoomIn.dy / 2);
    await hoverUntilMenu(toolbar, proc.pid!, zx, zy, "resting on Zoom In did not open the drop-down");
    await sleep(200);
    const items = await dropdownItems(client);
    if (items.map((it) => it.text).join() !== CUSTOM_ZOOM_STRIP.join()) {
      throw new Error(`toolbar-hover-dropdown: custom ZoomLevels give [${items.map((it) => it.text).join()}]`);
    }
    checkCurrentUnderButton(items, "100%", clientToScreen(toolbar, zx, zy).x, findTopWindow(proc.pid!, MENU_CLASS));

    // and they are real commands, not just labels
    const menu = await hoverUntilMenu(toolbar, proc.pid!, zx, zy, "the zoom drop-down closed");
    const r = getWindowRect(menu);
    const cell150 = items.find((it) => it.text === "150%")!;
    await clickAt(
      menu,
      Math.floor((cell150.x + cell150.x2) / 2) - r.left,
      Math.floor((cell150.y + cell150.y2) / 2) - r.top,
      300,
    );
    await waitZoom(client, "150", "clicking a custom level did not zoom to it");
  } finally {
    client.close();
    await killAndWait(proc);
  }
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
    "-window-pos",
    WINDOW_POS,
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
    const menu = await hoverUntilMenu(toolbar, pid, cx, cy, "resting on Save did not open the drop-down");
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
    await hoverUntilMenu(toolbar, pid, cx, cy, "the drop-down did not open a second time");
    const away = clientToScreen(toolbar, 4, 4);
    setCursorPos(away.x, away.y - 200);
    sendMessage(toolbar, WM_MOUSEMOVE, 0, packCoords(4, 4));
    await waitMenu(pid, false, "moving the mouse away did not close the drop-down");

    // --- the zoom buttons list the zoom levels
    sendCommand(frame, cmdId("CmdZoom100"));
    await waitZoom(client, "100", "could not set the zoom to 100%");

    const zoomIn = (await mainButtons(client)).find((b) => b.cmd === cmdId("CmdZoomIn") && !b.hidden && b.dx > 0);
    if (!zoomIn) {
      throw new Error("toolbar-hover-dropdown: no Zoom In button");
    }
    const zx = zoomIn.x + Math.floor(zoomIn.dx / 2);
    const zy = zoomIn.y + Math.floor(zoomIn.dy / 2);
    const btnCentreX = clientToScreen(toolbar, zx, zy).x;
    let zoomMenu = await hoverUntilMenu(toolbar, pid, zx, zy, "resting on Zoom In did not open the drop-down");
    await sleep(200);
    captureWindowToPng(zoomMenu, join(dir, "zoom-dropdown.png"));

    let items = await dropdownItems(client);
    const texts = items.map((it) => it.text);
    if (texts.join() !== ZOOM_LEVELS.join()) {
      throw new Error(`toolbar-hover-dropdown: the zoom levels are [${texts.join()}]`);
    }
    // one row, not a column: every cell has the same top and bottom
    if (items.some((it) => it.y !== items[0]!.y || it.y2 !== items[0]!.y2)) {
      throw new Error("toolbar-hover-dropdown: the zoom levels are not in a single row");
    }
    const zr = getWindowRect(zoomMenu);
    if (zr.bottom - zr.top > 2 * (items[0]!.y2 - items[0]!.y)) {
      throw new Error(`toolbar-hover-dropdown: the zoom drop-down is taller than a row ${JSON.stringify(zr)}`);
    }
    checkCurrentUnderButton(items, "100%", btnCentreX, zoomMenu);

    // clicking a level zooms straight to it. 300% is one of the levels the Zoom
    // menu has no command for, so it is also the check that those levels get a
    // command of their own
    const cell300 = items.find((it) => it.text === "300%")!;
    await clickAt(
      zoomMenu,
      Math.floor((cell300.x + cell300.x2) / 2) - zr.left,
      Math.floor((cell300.y + cell300.y2) / 2) - zr.top,
      300,
    );
    await waitMenu(pid, false, "clicking a zoom level did not close the drop-down");
    await waitZoom(client, "300", "clicking the 300% cell did not zoom to it");

    // and now that level is the one boxed, and the strip has moved so that it
    // is the one under the button
    zoomMenu = await hoverUntilMenu(toolbar, pid, zx, zy, "the zoom drop-down did not open again");
    await sleep(200);
    items = await dropdownItems(client);
    checkCurrentUnderButton(items, "300%", btnCentreX, zoomMenu);

    // stepping the zoom with the button the strip belongs to moves the box to
    // the level it lands on, without the strip itself moving out from under
    // the mouse mid-click
    const before = items.map((it) => `${it.text}@${it.x}`).join();
    sendCommand(frame, cmdId("CmdZoomIn"));
    await waitZoom(client, "400", "the Zoom In button did not step the zoom");
    items = await dropdownItems(client);
    if (items.map((it) => `${it.text}@${it.x}`).join() !== before) {
      throw new Error("toolbar-hover-dropdown: the strip moved when the zoom was stepped");
    }
    let boxed = items.filter((it) => it.current).map((it) => it.text);
    if (boxed.join() !== "400%") {
      throw new Error(`toolbar-hover-dropdown: the box did not follow the zoom, it is on [${boxed.join()}]`);
    }

    // a zoom that is none of them leaves nothing boxed
    sendCommand(frame, cmdId("CmdZoomFitContent"));
    await sleep(300 * SLOW_BUILD_FACTOR);
    boxed = (await dropdownItems(client)).filter((it) => it.current).map((it) => it.text);
    if (boxed.length !== 0) {
      throw new Error(`toolbar-hover-dropdown: a zoom off the list still boxes [${boxed.join()}]`);
    }
    sendCommand(frame, cmdId("CmdZoom200"));
    await waitZoom(client, "200", "could not put the zoom back on a listed level");

    // Zoom Out gets the same drop-down, and moving onto it from Zoom In swaps
    // straight to it rather than waiting for the delay all over again
    const zoomOut = (await mainButtons(client)).find((b) => b.cmd === cmdId("CmdZoomOut") && !b.hidden && b.dx > 0);
    if (!zoomOut) {
      throw new Error("toolbar-hover-dropdown: no Zoom Out button");
    }
    const ox = zoomOut.x + Math.floor(zoomOut.dx / 2);
    const oy = zoomOut.y + Math.floor(zoomOut.dy / 2);
    await hoverUntilMenu(toolbar, pid, ox, oy, "moving onto Zoom Out did not carry the drop-down over");
    await sleep(200);
    items = await dropdownItems(client);
    if (items.map((it) => it.text).join() !== ZOOM_LEVELS.join()) {
      throw new Error("toolbar-hover-dropdown: Zoom Out's drop-down is not the same list");
    }
    checkCurrentUnderButton(items, "200%", clientToScreen(toolbar, ox, oy).x, zoomMenu);
  } finally {
    client.close();
    await killAndWait(proc);
  }

  await checkCustomZoomLevels(dir, pdf);

  console.log("toolbar-hover-dropdown: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
