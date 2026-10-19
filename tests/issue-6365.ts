// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6365
// Right-click on a row of the Annotations window shows Copy Comment / Edit /
// Delete; Copy Comment copies the note, Delete removes the annotation and Edit
// starts editing its note.
//
// Run: bun tests/issue-6365.ts [--no-build] [-ng]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { assemblePdf, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util.ts";
import {
  clientToScreen,
  emptyClipboard,
  findTopWindow,
  getClipboardText,
  getPopupMenuHandle,
  postMessage,
  readMenuTree,
  sleep,
  VK_DOWN,
  VK_ESCAPE,
  VK_RETURN,
  WM_KEYDOWN,
} from "./winapi.ts";
import { killAndWait, launchControlled, openContextMenu, waitForContextMenu } from "./win-automation.ts";

const FLOAT_CLASS = "SUMATRA_ANNOT_FILTER_WND";
const NOTE = "first note";
const MENU = ["Copy Comment", "Edit Note", "Delete Annotation"];

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R 5 0 R] >>",
    `<< /Type /Annot /Subtype /Text /P 3 0 R /Rect [72 700 92 720] /T (kjk) /Contents (${NOTE}) >>`,
    "<< /Type /Annot /Subtype /Text /P 3 0 R /Rect [72 600 92 620] /T (kjk) /Contents (second note) >>",
  ]);
}

type State = { raw: string; visible: boolean; nAll: number; itemDy: number; listX: number; listY: number };

async function state(client: ControlClient): Promise<State> {
  const res = await client.request(ControlCommand.TestAnnotFilter, ["paint", 0, 0]);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0) {
    throw new Error(`issue-6365: TestAnnotFilter: ${raw.trim()}`);
  }
  // orig: itemDy / listY in client pixels; ng: rowDy / listRect in dips
  const rc = /listRect=(-?\d+),(-?\d+),/.exec(raw);
  return {
    raw,
    visible: /floatVisible=1/.test(raw),
    nAll: +(/nAll=(\d+)/.exec(raw)?.[1] ?? -1),
    itemDy: +(/(?:itemDy|rowDy)=(\d+)/.exec(raw)?.[1] ?? 0),
    listX: rc ? +rc[1]! : 0,
    listY: rc ? +rc[2]! : +(/listY=(\d+)/.exec(raw)?.[1] ?? 0),
  };
}

async function waitFor(client: ControlClient, what: string, pred: (s: State) => boolean): Promise<State> {
  const deadline = Date.now() + 6000 * SLOW_BUILD_FACTOR;
  let s = await state(client);
  while (!pred(s)) {
    if (Date.now() > deadline) {
      throw new Error(`issue-6365: ${what}; state ${s.raw}`);
    }
    await sleep(50);
    s = await state(client);
  }
  return s;
}

// opens the first row's menu and returns its labels; pick() chooses an item
// with the keyboard, click() with the mouse
interface RowMenu {
  open(): Promise<string[]>;
  pick(pos: number): Promise<void>;
  click(pos: number): Promise<void>;
  close(): void;
}

// orig: a Win32 menu (TrackPopupMenu) over the window's VirtListBox
function origMenu(pid: number, client: ControlClient): RowMenu {
  let popup = 0;
  return {
    async open() {
      const s = await waitFor(client, "the list never laid out", (st) => st.itemDy > 0);
      const hwnd = findTopWindow(pid, FLOAT_CLASS);
      if (!hwnd) {
        throw new Error("issue-6365: no Annotations window");
      }
      const pt = clientToScreen(hwnd, 24, s.listY + Math.floor(s.itemDy / 2));
      openContextMenu(hwnd, pt.x, pt.y);
      popup = await waitForContextMenu(3000);
      if (!popup) {
        throw new Error("issue-6365: right-click on a row showed no menu");
      }
      const hmenu = getPopupMenuHandle(popup);
      return hmenu ? readMenuTree(hmenu).map((it) => it.text) : [];
    },
    async pick(pos: number) {
      for (let i = 0; i <= pos; i++) {
        postMessage(popup, WM_KEYDOWN, VK_DOWN, 0);
      }
      postMessage(popup, WM_KEYDOWN, VK_RETURN, 0);
      await sleep(150);
    },
    // posted mouse input can't drive a Win32 menu here; keys pick the same item
    async click(pos: number) {
      await this.pick(pos);
    },
    close() {
      if (popup) {
        postMessage(popup, WM_KEYDOWN, VK_ESCAPE, 0);
      }
    },
  };
}

// ng: a gpui popup in the "annotlist" tool window
function ngMenu(client: ControlClient): RowMenu {
  let at = { x: 0, y: 0 };
  const input = (kind: string, a: number, b: number, c: number) =>
    client.request(ControlCommand.TestToolWindow, ["input", "annotlist", kind, a, b, c, 0]);
  const popupOpen = async () =>
    / popup=1/.test(String((await client.request(ControlCommand.TestToolWindow, ["state", "annotlist"]))[1] ?? ""));
  return {
    async open() {
      const s = await waitFor(client, "the list never laid out", (st) => st.itemDy > 0 && st.listY > 0);
      at = { x: s.listX + 24, y: s.listY + Math.floor(s.itemDy / 2) };
      await input("click", at.x, at.y, 1);
      const deadline = Date.now() + 3000 * SLOW_BUILD_FACTOR;
      while (!(await popupOpen())) {
        if (Date.now() > deadline) {
          throw new Error("issue-6365: right-click on a row showed no menu");
        }
        await sleep(40);
      }
      const m = /menu=([^\n]*)/.exec((await state(client)).raw);
      return m && m[1] ? m[1].split("|") : [];
    },
    async pick(pos: number) {
      for (let i = 0; i <= pos; i++) {
        await input("key", VK_DOWN, 0, 0);
      }
      await input("key", VK_RETURN, 0, 0);
      await sleep(150);
    },
    // the menu opens with its top left at the click; rows are 28 dips apart
    async click(pos: number) {
      await input("click", at.x + 90, at.y + 18 + pos * 28, 0);
      await sleep(150);
    },
    close() {},
  };
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6365");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "annots.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const { proc, client } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdToggleEditPDF"]);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdFindAnnotation"]);
    const s = await waitFor(client, "the Annotations window never opened", (st) => st.visible && st.nAll >= 0);
    if (s.nAll !== 2) {
      throw new Error(`issue-6365: nAll=${s.nAll}, want 2`);
    }
    const menu = USE_NG ? ngMenu(client) : origMenu(proc.pid!, client);

    const labels = await menu.open();
    if (JSON.stringify(labels) !== JSON.stringify(MENU)) {
      menu.close();
      throw new Error(`issue-6365: menu ${JSON.stringify(labels)}, want ${JSON.stringify(MENU)}`);
    }
    emptyClipboard();
    await menu.click(0);
    const copied = await getClipboardText(2000);
    if (copied !== NOTE) {
      throw new Error(`issue-6365: Copy Comment copied '${copied}', want '${NOTE}'`);
    }

    await menu.open();
    await menu.pick(2);
    await waitFor(client, "Delete Annotation did not delete", (st) => st.nAll === 1);

    await menu.open();
    await menu.pick(1);
    const deadline = Date.now() + 3000 * SLOW_BUILD_FACTOR;
    for (;;) {
      const res = await client.request(ControlCommand.TestMarkupAnnots, []);
      if (/annotEditToolbar visible=1 [^\n]*editing=1/.test(String(res[1] ?? ""))) {
        break;
      }
      if (Date.now() > deadline) {
        throw new Error(`issue-6365: Edit Note did not start editing:\n${String(res[1] ?? "")}`);
      }
      await sleep(50);
    }

    await client.quit();
  } finally {
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
