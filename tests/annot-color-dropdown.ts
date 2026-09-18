// The color chips of a selected annotation open the same color drop-down the
// Edit PDF toolbar's markup buttons have, and the color they apply carries its
// own opacity, so an annotation with a color chip has no opacity chip.
// A shape has two of those chips, and its interior can be left out entirely.
//
// Every Edit PDF toolbar button that creates a colored annotation has that
// drop-down too, and picking a color there sets the color of the next one made.
// Redact is the exception: its color is the box that covers the text.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  clientToScreen,
  enumWindows,
  findTopWindow,
  getWindowOwner,
  getWindowPid,
  getWindowRect,
  getWindowText,
  isWindowAbove,
  isWindowVisible,
  MK_RBUTTON,
  packCoords,
  postMessage,
  sendMessage,
  setCursorPos,
  sleep,
  VK_ESCAPE,
  WM_COMMAND,
  WM_KEYDOWN,
  WM_MOUSEMOVE,
  WM_RBUTTONDOWN,
  WM_RBUTTONUP,
} from "./winapi.ts";
import {
  clickAt,
  findCanvas,
  findChildByClass,
  killAndWait,
  launchControlled,
  pressEscape,
  sendCommand,
} from "./win-automation.ts";

const TOOLBAR_CLASS = "SumatraAnnotEditToolbar";
const MAIN_TOOLBAR_CLASS = "SUMATRA_VIRT_TOOLBAR";
const HOVER_MENU_CLASS = "SumatraToolbarHoverMenu";
const POPUP_CLASS = "SumatraAnnotColorPopup";
// the two preset colors the test picks from: translucent red, opaque green
const PRESETS = "#80ff0000 #00ff00";
// ink's own colors, with the alpha it paints them at
const INK_PRESETS = "#66ff0000 #4000ff00";
// Annotations.InkColors when not set; the first is the default ink color
const INK_DEFAULT_PRESETS = "#66ffff00* #668bf05d #6699defa #66f199d2 #66e24745";
const COLOR_DIALOG_TITLE = "Annotation Colors";
const PICKED_COLOR = "#ff0000";
const PICKED_OPACITY = 0x80;

type Rect = { x: number; y: number; dx: number; dy: number };

function parseRect(m: RegExpExecArray | null): Rect {
  if (!m) {
    return { x: 0, y: 0, dx: 0, dy: 0 };
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

function writeSettings(appdata: string): void {
  const nl = String.fromCharCode(10);
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    [
      "UiLanguage = en",
      "RestoreSession = false",
      "ShowStartPage = false",
      "CheckForUpdates = false",
      "Annotations [",
      `\tPresetColors = ${PRESETS}`,
      "]",
      "",
    ].join(nl),
  );
}

async function markupDump(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0) {
    throw new Error(`annot-color-dropdown: could not read markup state\n${raw}`);
  }
  return raw;
}

async function toolbarDump(client: ControlClient): Promise<string> {
  const raw = await markupDump(client);
  const m = /annotEditToolbar .*/.exec(raw);
  if (!m) {
    throw new Error(`annot-color-dropdown: could not read toolbar state\n${raw}`);
  }
  return m[0]!;
}

// the selected annotation's colors and opacity
async function selectedColor(client: ControlClient): Promise<{ color: string; interior: string; opacity: number }> {
  const res = await client.request(ControlCommand.TestAnnotEditorLayout, [0, 0]);
  const raw = String(res[1] ?? "").trim();
  const m = / color=(\S+) interiorColor=(\S+) opacity=(\d+)/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`annot-color-dropdown: could not read annotation colors: ${raw}`);
  }
  return { color: m[1]!, interior: m[2]!, opacity: +m[3]! };
}

function chipNames(dump: string): string[] {
  return (/ items=(\S+)/.exec(dump)?.[1] ?? "").split(",");
}

// clicks the named chip, which opens its color drop-down, and returns the
// screen rects of the swatches in it
async function openChipDropdown(client: ControlClient, pid: number, kind: string): Promise<Rect[]> {
  const dump = await toolbarDump(client);
  if (!chipNames(dump).includes(kind)) {
    throw new Error(`annot-color-dropdown: no ${kind} chip: ${dump}`);
  }
  const placed = parseRect(/ placed=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(dump));
  const chip = parseRect(new RegExp(`[=;]${kind}:(-?\\d+),(-?\\d+),(\\d+),(\\d+)`).exec(dump));
  const tbHwnd = findTopWindow(pid, TOOLBAR_CLASS);
  if (!tbHwnd) {
    throw new Error("annot-color-dropdown: annotation toolbar window not found");
  }
  await clickAt(tbHwnd, chip.x - placed.x + Math.floor(chip.dx / 2), chip.y - placed.y + Math.floor(chip.dy / 2));
  await sleep(500);

  const popup = findTopWindow(pid, POPUP_CLASS);
  if (!popup || !isWindowVisible(popup)) {
    throw new Error(
      `annot-color-dropdown: the ${kind} drop-down did not open, chip at ${JSON.stringify(chip)} of ${dump}`,
    );
  }
  const line = /annotColorPopup .*/.exec(await markupDump(client))?.[0] ?? "";
  if (!/annotColorPopup visible=1/.test(line)) {
    throw new Error(`annot-color-dropdown: the ${kind} drop-down is not in the dump: ${line}`);
  }
  const swatches: Rect[] = [];
  const re = /[=;]([^:;]+):(-?\d+),(-?\d+),(\d+),(\d+):[01]/g;
  for (const m of line.matchAll(re)) {
    swatches.push({ x: +m[2]!, y: +m[3]!, dx: +m[4]!, dy: +m[5]! });
  }
  if (swatches.length === 0) {
    throw new Error(`annot-color-dropdown: no swatches in the ${kind} drop-down: ${line}`);
  }
  return swatches;
}

// the drop-down's visible color dialog, if it is up
function findColorDialog(pid: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) !== pid || !isWindowVisible(hwnd)) {
      return true;
    }
    if (getWindowText(hwnd) !== COLOR_DIALOG_TITLE) {
      return true;
    }
    found = hwnd;
    return false;
  });
  return found;
}

// The button at the right end of the drop-down opens the color dialog. It has
// to end up in front of the main window, which it only does if it is owned by
// it, and the drop-down has to be gone.
async function checkEditColors(pid: number, frame: number, swatches: Rect[]): Promise<void> {
  const popup = findTopWindow(pid, POPUP_CLASS);
  const r = getWindowRect(popup);
  const last = swatches[swatches.length - 1]!;
  const x = last.x + last.dx + Math.floor((r.right - (last.x + last.dx)) / 2);
  await clickAt(popup, x - r.left, last.y + Math.floor(last.dy / 2) - r.top);
  await sleep(800);

  const dlg = findColorDialog(pid);
  if (!dlg) {
    throw new Error("annot-color-dropdown: Edit colors did not open the color dialog");
  }
  if (getWindowOwner(dlg) !== frame || !isWindowAbove(dlg, frame)) {
    throw new Error("annot-color-dropdown: the color dialog is not in front of the main window");
  }
  const stillUp = findTopWindow(pid, POPUP_CLASS);
  if (stillUp && isWindowVisible(stillUp)) {
    throw new Error("annot-color-dropdown: the drop-down stayed up under the color dialog");
  }
  postMessage(dlg, WM_KEYDOWN, VK_ESCAPE, 0);
  await sleep(400);
  if (findColorDialog(pid)) {
    throw new Error("annot-color-dropdown: the color dialog did not close");
  }
}

// picks one of the swatches of the open drop-down
async function pickSwatch(client: ControlClient, pid: number, swatches: Rect[], idx: number): Promise<void> {
  const popup = findTopWindow(pid, POPUP_CLASS);
  const r = getWindowRect(popup);
  const sw = swatches[idx]!;
  await clickAt(popup, sw.x - r.left + Math.floor(sw.dx / 2), sw.y - r.top + Math.floor(sw.dy / 2));
  await sleep(600);
  await client.waitForRenderIdle();
  const stillUp = findTopWindow(pid, POPUP_CLASS);
  if (stillUp && isWindowVisible(stillUp)) {
    throw new Error("annot-color-dropdown: the drop-down stayed up after picking a color");
  }
}

// A highlight: one color chip, no opacity chip, and no "none" swatch (mupdf
// draws a text markup without a color in a default one, so it wouldn't mean
// "invisible").
async function testMarkup(): Promise<void> {
  const dir = tmpPath("annot-color-dropdown");
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeSettings(appdata);

  const pdf = join(process.cwd(), "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(400);

    // a highlight over the text of page 1, which leaves it selected
    await client.seedTextSelection(1);
    sendCommand(frame, cmdId("CmdCreateAnnotHighlight"));
    await sleep(600);
    await client.waitForRenderIdle();

    const dump = await toolbarDump(client);
    if (!/annotEditToolbar visible=1/.test(dump)) {
      throw new Error(`annot-color-dropdown: no annotation toolbar: ${dump}`);
    }
    const items = chipNames(dump);
    if (!items.includes("color")) {
      throw new Error(`annot-color-dropdown: no color chip: ${dump}`);
    }
    if (items.includes("opacity")) {
      throw new Error(`annot-color-dropdown: the color chip did not replace the opacity chip: ${dump}`);
    }

    let swatches = await openChipDropdown(client, proc.pid!, "color");
    if (swatches.length !== 2) {
      throw new Error(`annot-color-dropdown: a markup annotation should have no "none" swatch: ${swatches.length}`);
    }
    await checkEditColors(proc.pid!, frame, swatches);

    swatches = await openChipDropdown(client, proc.pid!, "color");
    await pickSwatch(client, proc.pid!, swatches, 0);
    const got = await selectedColor(client);
    if (got.color !== PICKED_COLOR || got.opacity !== PICKED_OPACITY) {
      throw new Error(
        `annot-color-dropdown: annotation is ${got.color}/${got.opacity}, want ${PICKED_COLOR}/${PICKED_OPACITY}`,
      );
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

// A square: a chip for the stroke and one for the interior, each with its own
// drop-down, and the interior can be cleared with the drop-down's first swatch.
async function testShape(): Promise<void> {
  const dir = tmpPath("annot-color-dropdown-shape");
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeSettings(appdata);

  const pdf = join(dir, "square.pdf");
  writeFileSync(
    pdf,
    assemblePdf([
      "<< /Type /Catalog /Pages 2 0 R >>",
      "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
      "<< /Type /Annot /Subtype /Square /P 3 0 R /Rect [72 420 192 540] /C [0 0 1] /IC [1 1 0.6] /BS << /W 2 >> >>",
    ]),
    "latin1",
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const canvas = findCanvas(frame);
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(400);

    const raw = await markupDump(client);
    const sq = parseRect(/type=Square[^\n]*screen=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(raw));
    if (sq.dx === 0) {
      throw new Error(`annot-color-dropdown: no square on the page\n${raw}`);
    }
    await clickAt(canvas, sq.x + Math.floor(sq.dx / 2), sq.y + Math.floor(sq.dy / 2));
    await sleep(400);
    await client.waitForRenderIdle();

    const items = chipNames(await toolbarDump(client));
    for (const want of ["color", "interiorColor"]) {
      if (!items.includes(want)) {
        throw new Error(`annot-color-dropdown: a square has no ${want} chip: ${items.join(",")}`);
      }
    }
    if (items.includes("opacity")) {
      throw new Error(`annot-color-dropdown: a square still has an opacity chip: ${items.join(",")}`);
    }

    // the interior takes the first preset, opacity and all
    let swatches = await openChipDropdown(client, proc.pid!, "interiorColor");
    if (swatches.length !== 3) {
      throw new Error(`annot-color-dropdown: want a "none" swatch and the two presets, got ${swatches.length}`);
    }
    await pickSwatch(client, proc.pid!, swatches, 1);
    let got = await selectedColor(client);
    if (got.interior !== PICKED_COLOR || got.opacity !== PICKED_OPACITY) {
      throw new Error(
        `annot-color-dropdown: interior is ${got.interior}/${got.opacity}, want ${PICKED_COLOR}/${PICKED_OPACITY}`,
      );
    }
    if (got.color !== "#0000ff") {
      throw new Error(`annot-color-dropdown: picking an interior color changed the stroke to ${got.color}`);
    }

    // and the first swatch takes it away again
    swatches = await openChipDropdown(client, proc.pid!, "interiorColor");
    await pickSwatch(client, proc.pid!, swatches, 0);
    got = await selectedColor(client);
    if (got.interior !== "none") {
      throw new Error(`annot-color-dropdown: the interior is ${got.interior}, want none`);
    }

    // the stroke has a drop-down of its own
    swatches = await openChipDropdown(client, proc.pid!, "color");
    await pickSwatch(client, proc.pid!, swatches, 2);
    got = await selectedColor(client);
    if (got.color !== "#00ff00" || got.interior !== "none") {
      throw new Error(`annot-color-dropdown: the square is ${got.color}/${got.interior}, want #00ff00/none`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

// the buttons that offer the preset colors, and what they make
const COLOR_BUTTONS = [
  "CmdCreateAnnotText",
  "CmdCreateAnnotFreeText",
  "CmdCreateAnnotLine",
  "CmdCreateAnnotPolyLine",
  "CmdCreateAnnotSquare",
  "CmdCreateAnnotCircle",
  "CmdCreateAnnotPolygon",
  "CmdCreateAnnotStamp",
  "CmdCreateAnnotCaret",
  "CmdCreateAnnotFileAttachment",
];

// what each of them makes annotations in when no color is set: the defaults
// Acrobat, PDF-XChange and Foxit use
const DEFAULT_COLORS: Record<string, string> = {
  CmdCreateAnnotText: "#ffff00",
  CmdCreateAnnotFreeText: "#000000",
  CmdCreateAnnotLine: "#ff0000",
  CmdCreateAnnotPolyLine: "#ff0000",
  CmdCreateAnnotSquare: "#ff0000",
  CmdCreateAnnotCircle: "#ff0000",
  CmdCreateAnnotPolygon: "#ff0000",
  CmdCreateAnnotStamp: "#ff0000",
  CmdCreateAnnotCaret: "#0000ff",
  CmdCreateAnnotFileAttachment: "#ffff00",
};

// the visible annotation button for a command, in toolbar client coords
async function annotButtonRect(client: ControlClient, cmd: number): Promise<Rect | null> {
  const raw = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
  const re = /annotation-idx=\d+ cmd=(\d+) hidden=(\d) enabled=\d rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/g;
  let m: RegExpExecArray | null;
  while ((m = re.exec(raw)) !== null) {
    if (+m[1]! === cmd && m[2] === "0") {
      const x = +m[3]!;
      const y = +m[4]!;
      return { x, y, dx: +m[5]! - x, dy: +m[6]! - y };
    }
  }
  return null;
}

// the colors the open drop-down lists, the one in use marked with a *. The ink
// button's drop-down also has a thickness slider (tests/ink-thickness.ts),
// which is not a color
async function hoverMenuColors(client: ControlClient): Promise<string[]> {
  const raw = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
  const re = /^dropdown-item idx=\d+ cmd=\d+ current=(\d) rect=[-\d,]+ text=(#\S+|none)$/gm;
  const res: string[] = [];
  let m: RegExpExecArray | null;
  while ((m = re.exec(raw)) !== null) {
    res.push(m[1] === "1" ? `${m[2]}*` : m[2]!);
  }
  return res;
}

// right-click opens the drop-down at once, without waiting for the hover delay.
// The cursor has to be on the button or the drop-down closes itself.
function rightClickToolbar(toolbar: number, x: number, y: number): void {
  const s = clientToScreen(toolbar, x, y);
  setCursorPos(s.x, s.y);
  const lp = packCoords(x, y);
  sendMessage(toolbar, WM_RBUTTONDOWN, MK_RBUTTON, lp);
  sendMessage(toolbar, WM_RBUTTONUP, 0, lp);
}

// Esc dismisses a right-click drop-down; moving the mouse away does not
async function closeHoverMenu(pid: number, frame: number): Promise<void> {
  await pressEscape(frame);
  for (let i = 0; i < 30; i++) {
    const h = findTopWindow(pid, HOVER_MENU_CLASS);
    if (!h || !isWindowVisible(h)) {
      return;
    }
    await sleep(100);
  }
  throw new Error("annot-color-dropdown: the toolbar drop-down would not close");
}

async function testToolbarButtons(): Promise<void> {
  const dir = tmpPath("annot-color-dropdown-buttons");
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeSettings(appdata);

  const pdf = join(dir, "blank.pdf");
  writeFileSync(
    pdf,
    assemblePdf([
      "<< /Type /Catalog /Pages 2 0 R >>",
      "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
    ]),
    "latin1",
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  const pid = proc.pid!;
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(600);
    const toolbar = findChildByClass(frame, MAIN_TOOLBAR_CLASS);

    // none of them has a color set, so each one's default is marked as the one
    // in use, and joins the presets when it is not one of them yet
    const presets = PRESETS.split(" ");
    for (const name of COLOR_BUTTONS) {
      const b = await annotButtonRect(client, cmdId(name));
      if (!b) {
        throw new Error(`annot-color-dropdown: no ${name} button on the Edit PDF toolbar`);
      }
      rightClickToolbar(toolbar, b.x + (b.dx >> 1), b.y + (b.dy >> 1));
      await sleep(400);
      const def = DEFAULT_COLORS[name]!;
      if (!presets.includes(def)) {
        presets.push(def);
      }
      const want = presets.map((c) => (c === def ? `${c}*` : c)).join(" ");
      const colors = await hoverMenuColors(client);
      if (colors.join(" ") !== want) {
        throw new Error(`annot-color-dropdown: ${name} offers "${colors.join(" ")}", want "${want}"`);
      }
      await closeHoverMenu(pid, frame);
    }

    // ink offers colors of its own, translucent, and its default is the first
    {
      const b = (await annotButtonRect(client, cmdId("CmdCreateAnnotInk")))!;
      rightClickToolbar(toolbar, b.x + (b.dx >> 1), b.y + (b.dy >> 1));
      await sleep(400);
      const colors = (await hoverMenuColors(client)).join(" ");
      if (colors !== INK_DEFAULT_PRESETS) {
        throw new Error(`annot-color-dropdown: CmdCreateAnnotInk offers "${colors}", want "${INK_DEFAULT_PRESETS}"`);
      }
      await closeHoverMenu(pid, frame);
    }

    // a redaction mark's color is the box that covers the text, not a choice
    const redact = await annotButtonRect(client, cmdId("CmdCreateAnnotRedact"));
    if (redact) {
      rightClickToolbar(toolbar, redact.x + (redact.dx >> 1), redact.y + (redact.dy >> 1));
      await sleep(400);
      const h = findTopWindow(pid, HOVER_MENU_CLASS);
      if (h && isWindowVisible(h)) {
        throw new Error("annot-color-dropdown: Redact should have no color drop-down");
      }
      await closeHoverMenu(pid, frame);
      // with no drop-down the right-click picked the tool; leave its mode,
      // which disables the other buttons
      await pressEscape(frame);
      await sleep(300);
    }

    // picking a color is the color the next annotation of that type is made in
    const square = (await annotButtonRect(client, cmdId("CmdCreateAnnotSquare")))!;
    rightClickToolbar(toolbar, square.x + (square.dx >> 1), square.y + (square.dy >> 1));
    await sleep(400);
    const raw = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
    const item = /^dropdown-item idx=1 cmd=\d+ current=\d rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/m.exec(raw);
    if (!item) {
      throw new Error(`annot-color-dropdown: the Square drop-down has no second swatch\n${raw}`);
    }
    const menu = findTopWindow(pid, HOVER_MENU_CLASS);
    const mr = getWindowRect(menu);
    const cx = (+item[1]! + +item[3]!) >> 1;
    const cy = (+item[2]! + +item[4]!) >> 1;
    await clickAt(menu, cx - mr.left, cy - mr.top);
    await sleep(400);

    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotSquare"), packCoords(300, 300));
    await sleep(600);
    await client.waitForRenderIdle();
    const got = await selectedColor(client);
    if (got.color !== "#00ff00") {
      throw new Error(`annot-color-dropdown: the new square is ${got.color}, want #00ff00`);
    }

    // clicking the button picks the tool: its colors go away, and resting on
    // the button does not bring them back
    const line = (await annotButtonRect(client, cmdId("CmdCreateAnnotLine")))!;
    const lx = line.x + (line.dx >> 1);
    const ly = line.y + (line.dy >> 1);
    rightClickToolbar(toolbar, lx, ly);
    await sleep(400);
    const lineMenu = findTopWindow(pid, HOVER_MENU_CLASS);
    if (!lineMenu || !isWindowVisible(lineMenu)) {
      throw new Error("annot-color-dropdown: the Line drop-down did not open");
    }
    await clickAt(toolbar, lx, ly, 300);
    await sleep(300);
    const stayClosedUntil = Date.now() + 1500;
    while (Date.now() < stayClosedUntil) {
      const s = clientToScreen(toolbar, lx, ly);
      setCursorPos(s.x, s.y);
      sendMessage(toolbar, WM_MOUSEMOVE, 0, packCoords(lx, ly));
      const h = findTopWindow(pid, HOVER_MENU_CLASS);
      if (h && isWindowVisible(h)) {
        throw new Error("annot-color-dropdown: clicking the Line button left its color drop-down up");
      }
      await sleep(100);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

// A button's color that is not one of the presets (set in the settings file by
// hand, say) is added to them, so its drop-down always shows the color in use.
async function testCurrentColorAdded(): Promise<void> {
  const dir = tmpPath("annot-color-dropdown-current");
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  const nl = String.fromCharCode(10);
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    [
      "UiLanguage = en",
      "RestoreSession = false",
      "ShowStartPage = false",
      "CheckForUpdates = false",
      "Annotations [",
      `\tPresetColors = ${PRESETS}`,
      "\tLineColor = #123456",
      `\tInkColors = ${INK_PRESETS}`,
      "]",
      "",
    ].join(nl),
  );
  const pdf = join(dir, "blank.pdf");
  writeFileSync(
    pdf,
    assemblePdf([
      "<< /Type /Catalog /Pages 2 0 R >>",
      "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
    ]),
    "latin1",
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  const pid = proc.pid!;
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(600);
    const toolbar = findChildByClass(frame, MAIN_TOOLBAR_CLASS);

    for (const [name, want] of [
      ["CmdCreateAnnotLine", `${PRESETS} #123456*`],
      // it stays in the presets, for every button; a square has no color set,
      // so its default joins them too
      ["CmdCreateAnnotSquare", `${PRESETS} #123456 #ff0000*`],
      // the highlighter makes highlights, in HighlightColor (yellow)
      ["CmdAnnotationHighlightBrush", `${PRESETS} #123456 #ff0000 #ffff00*`],
      // ink has colors of its own, translucent; its default 40% yellow joins them
      ["CmdCreateAnnotInk", `${INK_PRESETS} #66ffff00*`],
    ] as const) {
      const b = (await annotButtonRect(client, cmdId(name)))!;
      rightClickToolbar(toolbar, b.x + (b.dx >> 1), b.y + (b.dy >> 1));
      await sleep(400);
      const colors = (await hoverMenuColors(client)).join(" ");
      if (colors !== want) {
        throw new Error(`annot-color-dropdown: ${name} offers "${colors}", want "${want}"`);
      }
      await closeHoverMenu(pid, frame);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  await testMarkup();
  await testShape();
  await testToolbarButtons();
  await testCurrentColorAdded();
  console.log("annot-color-dropdown: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
