// Find Annotation (issue #6086): the annotation filter used to be a search box
// on the main toolbar that crowded out the other buttons. It now lives only in
// the floating Annotations window, which a button near the end of the Edit PDF
// toolbar opens and closes.
//
// Covers: the toggle, the list and its keyboard nav, Esc, click / double-click,
// multi-select and Delete.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control";
import { assemblePdf, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util";
import {
  captureWindowToPng,
  enumChildWindows,
  findTopWindow,
  getClassName,
  getClientRect,
  getFocusedHwnd,
  getWindowPid,
  isWindowVisible,
  packCoords,
  sendMessage,
  sendText,
  sleep,
  VK_DELETE,
  VK_DOWN,
  WM_KEYDOWN,
  WM_LBUTTONDBLCLK,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  MK_CONTROL,
  MK_LBUTTON,
} from "./winapi";
import {
  clickAt,
  findChildByClass,
  killAndWait,
  launchControlled,
  pressEscape,
  pressKey,
  sendCommand,
} from "./win-automation";

const TOOLBAR_CLASS = "SUMATRA_VIRT_TOOLBAR";
const FLOAT_CLASS = "SUMATRA_ANNOT_FILTER_WND";
const CANVAS_CLASS = "SUMATRA_PDF_CANVAS";

type FilterState = {
  floatVisible: boolean;
  nAll: number;
  nVisible: number;
  sel: number;
  deleteEnabled: boolean;
  discardEnabled: boolean;
  saveEnabled: boolean;
  nSel: number;
  itemDy: number;
  listY: number;
  raw: string;
};

function makePdf(): string {
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R 5 0 R] >>",
    "<< /Type /Annot /Subtype /Highlight /P 3 0 R /Rect [72 680 220 710] " +
      "/QuadPoints [72 710 220 710 72 680 220 680] /C [1 1 0] /CA 0.5 " +
      "/Contents (alpha unique comment) >>",
    "<< /Type /Annot /Subtype /Text /P 3 0 R /Rect [72 50 92 70] /C [1 0 0] " + "/Contents (beta other note) >>",
  ];
  return assemblePdf(objs);
}

function parseFilter(dump: string): FilterState | null {
  const m =
    /annotFilter floatVisible=(\d+) floatRect=(-?\d+),(-?\d+),(\d+),(\d+) nAll=(\d+) nVisible=(\d+) sel=(-?\d+)/.exec(
      dump,
    );
  if (!m) {
    return null;
  }
  return {
    floatVisible: m[1] === "1",
    nAll: +m[6]!,
    nVisible: +m[7]!,
    sel: +m[8]!,
    deleteEnabled: /deleteEnabled=1/.test(dump),
    discardEnabled: /discardEnabled=1/.test(dump),
    saveEnabled: /saveEnabled=1/.test(dump),
    nSel: +(/nSel=(-?\d+)/.exec(dump)?.[1] ?? -1),
    itemDy: +(/itemDy=(-?\d+)/.exec(dump)?.[1] ?? 0),
    listY: +(/listY=(-?\d+)/.exec(dump)?.[1] ?? 0),
    raw: dump,
  };
}

async function toolbarDump(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestToolbarButtons, []);
  return String(res[1] ?? "");
}

async function waitFilter(
  client: ControlClient,
  pred: (s: FilterState) => boolean,
  timeoutMs = 4000,
): Promise<FilterState> {
  // an asan build is slow enough that the un-scaled waits ran out
  const deadline = Date.now() + timeoutMs * SLOW_BUILD_FACTOR;
  let last: FilterState | null = null;
  let raw = "";
  while (Date.now() < deadline) {
    raw = await toolbarDump(client);
    last = parseFilter(raw);
    if (last && pred(last)) {
      return last;
    }
    await sleep(50);
  }
  // the arrow function's source names the condition that never came true
  throw new Error(`annot-filter-toolbar: filter state never matched: ${pred}\n${raw}`);
}

// The list caret moves as soon as the arrow key lands, but selecting that
// annotation on the page is debounced (kSelectionDebounceMs). Poll for it: a
// fixed wait just past the debounce is not enough on a slow (asan) build.
async function waitPageSelected(client: ControlClient, what: string): Promise<string> {
  const deadline = Date.now() + 5000 * SLOW_BUILD_FACTOR;
  let markup = "";
  for (;;) {
    markup = String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
    if (/state selected=1 /.test(markup)) {
      return markup;
    }
    if (Date.now() > deadline) {
      throw new Error(`annot-filter-toolbar: ${what}\n${markup}`);
    }
    await sleep(50);
  }
}

// Focus moves on its own schedule, and getFocusedHwnd() reads it across
// processes: one read right after the key can catch the old owner (or nothing).
async function waitFocusedClass(frame: number, want: string, what: string): Promise<number> {
  const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
  let cls = "none";
  for (;;) {
    const hwnd = getFocusedHwnd(frame);
    cls = hwnd ? getClassName(hwnd) : "none";
    if (hwnd && cls === want) {
      return hwnd;
    }
    if (Date.now() > deadline) {
      throw new Error(`annot-filter-toolbar: ${what} (class=${cls})`);
    }
    await sleep(50);
  }
}

function floatEditHwnd(floatWnd: number): number {
  let e = 0;
  enumChildWindows(floatWnd, (hwnd) => {
    if (getClassName(hwnd) === "Edit" && isWindowVisible(hwnd)) {
      e = hwnd;
      return false;
    }
    return true;
  });
  return e;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("annot-filter-toolbar");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "annots.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const pid = getWindowPid(frame);
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300);

    // the main toolbar no longer carries a filter box (#6086)
    const toolbar = findChildByClass(frame, TOOLBAR_CLASS);
    if (!toolbar) {
      throw new Error("annot-filter-toolbar: no toolbar");
    }
    captureWindowToPng(toolbar, join(dir, "toolbar.png"));
    let nEdits = 0;
    enumChildWindows(toolbar, (hwnd) => {
      if (getClassName(hwnd) === "Edit" && isWindowVisible(hwnd)) {
        nEdits++;
      }
      return true;
    });
    if (nEdits > 1) {
      throw new Error(`annot-filter-toolbar: toolbar still has a filter box (${nEdits} edits)`);
    }
    if (findTopWindow(pid, FLOAT_CLASS)) {
      throw new Error("annot-filter-toolbar: annotation list opened without being asked for");
    }

    // Find Annotation opens the floating list
    sendCommand(frame, cmdId("CmdFindAnnotation"));
    let st = await waitFilter(client, (s) => s.floatVisible && s.nAll >= 2, 8000);
    if (st.discardEnabled || st.saveEnabled) {
      throw new Error(`annot-filter-toolbar: save/discard should be disabled with no changes\n${st.raw}`);
    }
    const floatWnd = findTopWindow(pid, FLOAT_CLASS);
    if (!floatWnd || !isWindowVisible(floatWnd)) {
      throw new Error("annot-filter-toolbar: floating annotation list window not visible");
    }
    captureWindowToPng(floatWnd, join(dir, "floating.png"));

    const edit = floatEditHwnd(floatWnd);
    if (!edit) {
      throw new Error("annot-filter-toolbar: floating window has no Edit");
    }
    const editRc = getClientRect(edit);
    await clickAt(edit, Math.floor(editRc.right / 2), Math.floor(editRc.bottom / 2), 200);

    sendMessage(edit, WM_KEYDOWN, VK_DOWN, 0);
    st = await waitFilter(client, (s) => s.sel >= 0);
    await waitPageSelected(client, "arrow did not select annotation on the page");
    st = await waitFilter(client, (s) => s.deleteEnabled);

    sendText(edit, "unique");
    st = await waitFilter(client, (s) => s.nVisible === 1 && s.floatVisible);

    await pressEscape(edit);
    st = await waitFilter(client, (s) => s.nVisible >= 2 && s.floatVisible);
    await pressEscape(edit);
    await waitFocusedClass(frame, CANVAS_CLASS, "Esc with an empty filter did not focus the canvas");

    // click and double-click in the list
    st = await waitFilter(client, (s) => s.itemDy > 0);
    const lp = packCoords(24, st.listY + Math.floor(st.itemDy / 2));
    sendMessage(floatWnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
    sendMessage(floatWnd, WM_LBUTTONUP, 0, lp);
    st = await waitFilter(client, (s) => s.sel >= 0);
    sendMessage(floatWnd, WM_LBUTTONDBLCLK, MK_LBUTTON, lp);
    await waitPageSelected(client, "double-click did not select the annotation");

    // Ctrl+click adds to the selection; Del removes both annotations
    st = await waitFilter(client, (s) => s.nSel >= 1 && s.deleteEnabled);
    await clickAt(floatWnd, 24, st.listY + st.itemDy + Math.floor(st.itemDy / 2), 200, MK_CONTROL);
    st = await waitFilter(client, (s) => s.nSel >= 2);
    await pressKey(floatWnd, VK_DELETE, 400);
    st = await waitFilter(client, (s) => s.nAll === 0 && s.nSel === 0 && !s.deleteEnabled);

    // Find Annotation again closes it
    sendCommand(frame, cmdId("CmdFindAnnotation"));
    st = await waitFilter(client, (s) => !s.floatVisible);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
