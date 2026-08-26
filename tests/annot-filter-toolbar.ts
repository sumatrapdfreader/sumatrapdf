// Edit PDF toolbar filter: cue, dropdown list, keyboard nav, Esc, click / double-click.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util";
import {
  captureWindowToPng,
  enumChildWindows,
  findTopWindow,
  getClassName,
  getClientRect,
  getFocusedHwnd,
  getWindowPid,
  getWindowRect,
  isWindowVisible,
  packCoords,
  sendMessage,
  sendText,
  sleep,
  VK_DOWN,
  WM_KEYDOWN,
  WM_LBUTTONDBLCLK,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  MK_LBUTTON,
} from "./winapi";
import { clickAt, findChildByClass, killAndWait, launchControlled, pressEscape, sendCommand } from "./win-automation";

const TOOLBAR_CLASS = "SUMATRA_VIRT_TOOLBAR";
const LIST_CLASS = "SumatraAnnotFilterList";
const CANVAS_CLASS = "SUMATRA_PDF_CANVAS";

type FilterState = {
  hidden: boolean;
  rect: { x: number; y: number; dx: number; dy: number };
  listVisible: boolean;
  nAll: number;
  nVisible: number;
  sel: number;
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
    /annotFilter hidden=(\d+) rect=(-?\d+),(-?\d+),(\d+),(\d+) listVisible=(\d+) nAll=(\d+) nVisible=(\d+) sel=(-?\d+)/.exec(
      dump,
    );
  if (!m) {
    return null;
  }
  return {
    hidden: m[1] === "1",
    rect: { x: +m[2]!, y: +m[3]!, dx: +m[4]!, dy: +m[5]! },
    listVisible: m[6] === "1",
    nAll: +m[7]!,
    nVisible: +m[8]!,
    sel: +m[9]!,
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
  const deadline = Date.now() + timeoutMs;
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
  throw new Error(`annot-filter-toolbar: filter state never matched\n${raw}`);
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
    sendCommand(frame, cmdId("CmdTogglePdfAnnotationsToolbar"));

    let st = await waitFilter(client, (s) => !s.hidden);
    if (st.rect.dx < 40 || st.rect.dy < 10) {
      throw new Error(`annot-filter-toolbar: filter rect too small ${JSON.stringify(st.rect)}`);
    }
    st = await waitFilter(client, (s) => s.nAll >= 2, 8000);

    const toolbar = findChildByClass(frame, TOOLBAR_CLASS);
    if (!toolbar) {
      throw new Error("annot-filter-toolbar: no toolbar");
    }
    captureWindowToPng(toolbar, join(dir, "toolbar.png"));

    if (st.rect.y > 40) {
      throw new Error(`annot-filter-toolbar: filter should be on the main toolbar row, got y=${st.rect.y}`);
    }

    let filterEdit = 0;
    let filterEditX = -1;
    enumChildWindows(toolbar, (hwnd) => {
      if (getClassName(hwnd) !== "Edit" || !isWindowVisible(hwnd)) {
        return true;
      }
      const wr = getWindowRect(hwnd);
      if (wr.left > filterEditX) {
        filterEditX = wr.left;
        filterEdit = hwnd;
      }
      return true;
    });
    if (!filterEdit) {
      throw new Error("annot-filter-toolbar: no filter Edit hwnd");
    }
    const editRc = getClientRect(filterEdit);
    await clickAt(filterEdit, Math.floor(editRc.right / 2), Math.floor(editRc.bottom / 2), 200);
    st = await waitFilter(client, (s) => s.listVisible && s.nVisible >= 2);

    const pid = getWindowPid(frame);
    const list = findTopWindow(pid, LIST_CLASS);
    if (!list || !isWindowVisible(list)) {
      throw new Error("annot-filter-toolbar: dropdown list window not visible");
    }
    captureWindowToPng(list, join(dir, "list.png"));

    const edit = getFocusedHwnd(frame);
    if (!edit || getClassName(edit) !== "Edit") {
      throw new Error(
        `annot-filter-toolbar: filter edit did not take focus (class=${edit ? getClassName(edit) : "none"})`,
      );
    }

    sendMessage(edit, WM_KEYDOWN, VK_DOWN, 0);
    st = await waitFilter(client, (s) => s.sel >= 0);
    await sleep(400);
    const markup = String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
    if (!/state selected=1 /.test(markup)) {
      throw new Error(`annot-filter-toolbar: arrow did not select annotation on the page\n${markup}`);
    }

    sendText(edit, "unique");
    st = await waitFilter(client, (s) => s.nVisible === 1 && s.listVisible);

    await pressEscape(edit);
    st = await waitFilter(client, (s) => s.nVisible >= 2 && s.listVisible);

    await pressEscape(edit);
    st = await waitFilter(client, (s) => !s.listVisible);
    const focused = getFocusedHwnd(frame);
    if (!focused || getClassName(focused) !== CANVAS_CLASS) {
      throw new Error(
        `annot-filter-toolbar: Esc with empty filter did not focus canvas (class=${focused ? getClassName(focused) : "none"})`,
      );
    }

    await clickAt(filterEdit, Math.floor(editRc.right / 2), Math.floor(editRc.bottom / 2), 200);
    st = await waitFilter(client, (s) => s.listVisible);
    const list2 = findTopWindow(pid, LIST_CLASS);
    if (!list2) {
      throw new Error("annot-filter-toolbar: list did not reopen");
    }
    const lp = packCoords(12, 12);
    sendMessage(list2, WM_LBUTTONDOWN, MK_LBUTTON, lp);
    sendMessage(list2, WM_LBUTTONUP, 0, lp);
    st = await waitFilter(client, (s) => s.sel >= 0);
    sendMessage(list2, WM_LBUTTONDBLCLK, MK_LBUTTON, lp);
    st = await waitFilter(client, (s) => !s.listVisible);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
