// The Custom Zoom dialog (Ctrl+Y) is an edit field over a list of the zoom
// levels, not an editable combo box. Up / Down move the list while the focus is
// still in the field, and the field follows, the way the combo box did; what
// the field says is what the dialog zooms to. The list shows every level, has
// no separator row in it, and its ends are ends: the arrows stop there.
//
// Run: bun tests/custom-zoom-dialog.ts [--no-build]   (or via tests/run-almost-all.ts)

import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, ROOT, cmdId, runStandalone, SLOW_BUILD_FACTOR, writeAppdata } from "./util.ts";
import {
  enumWindows,
  findChildWindow,
  getClassName,
  getWindowPid,
  getWindowRect,
  getControlText,
  getWindowText,
  getWorkArea,
  isWindowVisible,
  postMessage,
  sleep,
  VK_DOWN,
  VK_RETURN,
  VK_UP,
  WM_KEYDOWN,
  WM_KEYUP,
} from "./winapi.ts";
import { sendCommand, waitForFrame } from "./win-automation.ts";

const DIALOG_CLASS = "SumatraWgDefaultWinClass";

// every level the dialog offers, in the order the list has them
const ZOOM_LEVELS = [
  "Fit Page",
  "Fit Width",
  "Fit Height",
  "Fit by Orientation",
  "Fit Content",
  "Shrink To Fit",
  "6400%",
  "3200%",
  "1600%",
  "800%",
  "400%",
  "200%",
  "150%",
  "125%",
  "100%",
  "50%",
  "25%",
  "12.5%",
  "8.33%",
];

function findDialog(pid: number, frame: number): number {
  let res = 0;
  enumWindows((hwnd) => {
    if (hwnd !== frame && getWindowPid(hwnd) === pid && getClassName(hwnd) === DIALOG_CLASS && isWindowVisible(hwnd)) {
      res = hwnd;
      return false;
    }
    return true;
  });
  return res;
}

async function waitForDialog(pid: number, frame: number): Promise<number> {
  const deadline = Date.now() + 5000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const hwnd = findDialog(pid, frame);
    if (hwnd) {
      return hwnd;
    }
    if (Date.now() > deadline) {
      throw new Error("custom-zoom-dialog: the dialog did not open");
    }
    await sleep(50);
  }
}

async function waitForDialogGone(pid: number, frame: number): Promise<void> {
  const deadline = Date.now() + 5000 * SLOW_BUILD_FACTOR;
  for (;;) {
    if (!findDialog(pid, frame)) {
      return;
    }
    if (Date.now() > deadline) {
      throw new Error("custom-zoom-dialog: the dialog did not close");
    }
    await sleep(50);
  }
}

// a key the dialog's own handler sees: it reads them off the message queue of
// whatever has the focus, which is the edit field
async function pressKey(hwnd: number, vkey: number): Promise<void> {
  postMessage(hwnd, WM_KEYDOWN, vkey, 0);
  postMessage(hwnd, WM_KEYUP, vkey, 0);
  await sleep(150 * SLOW_BUILD_FACTOR);
}

async function editText(edit: number, want: string, what: string): Promise<void> {
  const deadline = Date.now() + 3000 * SLOW_BUILD_FACTOR;
  let got = "";
  for (;;) {
    got = getControlText(edit);
    if (got === want) {
      return;
    }
    if (Date.now() > deadline) {
      throw new Error(`custom-zoom-dialog: ${what} (the field says "${got}", want "${want}")`);
    }
    await sleep(50);
  }
}

// hold an arrow key down until the list stops moving, and report every level
// it stopped on along the way
async function walk(edit: number, vkey: number): Promise<string[]> {
  const seen: string[] = [getControlText(edit)];
  for (let i = 0; i < 40; i++) {
    await pressKey(edit, vkey);
    const now = getControlText(edit);
    if (now === seen[seen.length - 1]) {
      // the end of the list: it stops there rather than wrapping round
      return seen;
    }
    seen.push(now);
  }
  throw new Error(`custom-zoom-dialog: the list never stopped moving (${seen.join(" ")})`);
}

async function zoomLabel(client: ControlClient): Promise<string> {
  const raw = String((await client.request(ControlCommand.TestDisplayMode, ["get"]))[1] ?? "");
  return /zoom=(.+)$/m.exec(raw)?.[1]?.trim() ?? "";
}

// More levels than the screen has room for: the list gives rows back rather
// than growing a dialog taller than the monitor it opens on.
async function checkTallList(): Promise<void> {
  const levels: number[] = [];
  for (let z = 10; z <= 600; z += 10) {
    levels.push(z);
  }
  const appdata = writeAppdata(
    "custom-zoom-dialog-tall",
    [
      "UiLanguage = en",
      "RestoreSession = false",
      "ShowStartPage = false",
      "CheckForUpdates = false",
      `ZoomLevels = ${levels.join(" ")}`,
    ].join("\n"),
  );
  const pdf = join(ROOT, "tests", "issue-5871.pdf");

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const pid = proc.pid!;
      const frame = await waitForFrame(pid);
      await client.waitForRenderIdle(30000);
      await client.setNotificationsEnabled(false);

      sendCommand(frame, cmdId("CmdZoomCustom"));
      const dlg = await waitForDialog(pid, frame);
      const dr = getWindowRect(dlg);
      const wa = getWorkArea();
      if (dr.bottom - dr.top > wa.bottom - wa.top) {
        throw new Error(
          `custom-zoom-dialog: ${levels.length} levels made the dialog taller than the screen ` +
            `${JSON.stringify(dr)} vs ${JSON.stringify(wa)}`,
        );
      }
      // still usable: the field follows the list, which now scrolls
      const edit = findChildWindow(dlg, "Edit");
      await editText(edit, "100%", "the field did not start on the current zoom");
      // the list runs largest first, so the level under 100% is 90%
      await pressKey(edit, VK_DOWN);
      await editText(edit, "90%", "Down did not move to the next custom level");
    },
    ["-appdata", appdata, "-window-pos", "1100x900@40x40", "-zoom", "100", pdf],
  );
}

export async function testit(): Promise<void> {
  const appdata = writeAppdata(
    "custom-zoom-dialog",
    ["UiLanguage = en", "RestoreSession = false", "ShowStartPage = false", "CheckForUpdates = false"].join("\n"),
  );
  const pdf = join(ROOT, "tests", "issue-5871.pdf");

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const pid = proc.pid!;
      const frame = await waitForFrame(pid);
      await client.waitForRenderIdle(30000);
      await client.setNotificationsEnabled(false);

      sendCommand(frame, cmdId("CmdZoomCustom"));
      const dlg = await waitForDialog(pid, frame);

      const title = getWindowText(dlg);
      if (title !== "Zoom") {
        throw new Error(`custom-zoom-dialog: the title is "${title}", want "Zoom"`);
      }
      if (findChildWindow(dlg, "ComboBox")) {
        throw new Error("custom-zoom-dialog: the zoom is still a combo box");
      }
      const edit = findChildWindow(dlg, "Edit");
      if (!edit) {
        throw new Error("custom-zoom-dialog: no edit field");
      }
      // the list is drawn by the dialog, not a window of its own, so what says
      // it is there is the height it takes and the arrow keys below
      const dr = getWindowRect(dlg);
      const er = getWindowRect(edit);
      const rowDy = er.bottom - er.top;
      if (dr.bottom - dr.top < 6 * rowDy) {
        throw new Error(`custom-zoom-dialog: the dialog is too short to hold a list ${JSON.stringify(dr)}`);
      }

      // the document opens at 100%, so that is what the field starts on
      await editText(edit, "100%", "the field did not start on the current zoom");

      // Down / Up move the list and the field follows, with the focus still in
      // the field. 50% and 125% are the levels either side of 100%
      await pressKey(edit, VK_DOWN);
      await editText(edit, "50%", "Down did not move to the next level");
      await pressKey(edit, VK_UP);
      await pressKey(edit, VK_UP);
      await editText(edit, "125%", "Up did not move back up the levels");

      // Enter zooms to what the field says
      await pressKey(edit, VK_RETURN);
      await waitForDialogGone(pid, frame);
      const zoom = await zoomLabel(client);
      if (zoom !== "125") {
        throw new Error(`custom-zoom-dialog: the dialog zoomed to ${zoom}, want 125`);
      }

      // walking to both ends says what the whole list holds: every level, in
      // order, with no separator row and nothing beyond the ends
      sendCommand(frame, cmdId("CmdZoomCustom"));
      const dlg2 = await waitForDialog(pid, frame);
      const edit2 = findChildWindow(dlg2, "Edit");
      await editText(edit2, "125%", "reopening did not show the zoom it is at");
      const down = await walk(edit2, VK_DOWN);
      const up = await walk(edit2, VK_UP);
      const all = up.slice().reverse();
      if (all.join() !== ZOOM_LEVELS.join()) {
        throw new Error(`custom-zoom-dialog: the levels are [${all.join()}]`);
      }
      if (down[down.length - 1] !== "8.33%") {
        throw new Error(`custom-zoom-dialog: Down ended on ${down[down.length - 1]}, want 8.33%`);
      }

      // the walk left it on the first row, which is a level with a name rather
      // than a number: the field takes the name and so does the zoom
      await editText(edit2, "Fit Page", "the walk did not end on the first level");
      await pressKey(edit2, VK_RETURN);
      await waitForDialogGone(pid, frame);
      const zoom2 = await zoomLabel(client);
      if (zoom2 !== "fit page") {
        throw new Error(`custom-zoom-dialog: the dialog zoomed to ${zoom2}, want fit page`);
      }
    },
    ["-appdata", appdata, "-window-pos", "1100x900@40x40", "-zoom", "100", pdf],
  );

  await checkTallList();

  console.log("custom-zoom-dialog: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
