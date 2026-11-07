// Edit Favorites (CmdEditFavorites): the favorites dialog without the name for a
// new favorite. Move a favorite down, rename it, delete another; the new order
// sticks (FavoritesManualOrder). Add Favorite is the same dialog plus the name.
//
// Run: bun tests/edit-favorites.ts [-ng] [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, pressKey, sendCommand } from "./win-automation.ts";
import {
  captureWindowToPng,
  enumChildWindows,
  enumWindows,
  getClassName,
  getWindowPid,
  getWindowText,
  isWindowVisible,
  packCoords,
  postMessage,
  sendText,
  sleep,
  VK_DOWN,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";

const kPages = 3;
const kWaitMs = 5000;
const MK_LBUTTON = 1;
// ng's tool window name (DlgWindowSpec.name)
const kNgWindow = "addfavorite";
// gpui's accessibility role for a text field
const kRoleTextField = 17;

function makePdf(): string {
  const objs: string[] = ["<< /Type /Catalog /Pages 2 0 R >>"];
  const kids = Array.from({ length: kPages }, (_, i) => `${i + 3} 0 R`).join(" ");
  objs.push(`<< /Type /Pages /Count ${kPages} /Kids [${kids}] >>`);
  for (let i = 0; i < kPages; i++) {
    objs.push("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>");
  }
  return assemblePdf(objs);
}

async function favNav(client: ControlClient, action: string, pageNo = 0): Promise<string> {
  const res = await client.request(ControlCommand.TestFavoriteNav, [action, pageNo]);
  const out = String(res[1] ?? "").trim();
  if (res[0] !== 0) {
    throw new Error(`TestFavoriteNav ${action}: ${out}`);
  }
  return out;
}

async function expectFavs(client: ControlClient, want: string, what: string): Promise<void> {
  const deadline = Date.now() + kWaitMs;
  let got = "";
  while (Date.now() < deadline) {
    got = await favNav(client, "list");
    if (got === want) {
      return;
    }
    await sleep(50);
  }
  throw new Error(`edit-favorites: ${what}: '${got}', want '${want}'`);
}

// what the scenario does to the dialog
type Driver = {
  open(cmd: string, title: string): Promise<void>;
  selectRow(idx: number): Promise<void>;
  click(label: string): Promise<void>;
  setName(text: string): Promise<void>;
  nameFields(): Promise<number>;
};

// --- orig: a window with Edit controls and VirtCtrls -------------------------

async function waitForDialog(pid: number, title: string): Promise<number> {
  const deadline = Date.now() + kWaitMs;
  while (Date.now() < deadline) {
    let found = 0;
    enumWindows((h) => {
      if (getWindowPid(h) === pid && isWindowVisible(h) && getWindowText(h) === title) {
        found = h;
        return false;
      }
      return true;
    });
    if (found) {
      return found;
    }
    await sleep(50);
  }
  throw new Error(`edit-favorites: no '${title}' window`);
}

function clickDlg(dlg: number, x: number, y: number): void {
  const lp = packCoords(x, y);
  postMessage(dlg, WM_MOUSEMOVE, 0, lp);
  postMessage(dlg, WM_LBUTTONDOWN, MK_LBUTTON, lp);
  postMessage(dlg, WM_LBUTTONUP, 0, lp);
}

function origDriver(client: ControlClient, frame: number, pid: number, dir: string): Driver {
  let dlg = 0;
  const virt = async () => client.virtTree(getClassName(dlg));
  const edits = () => {
    const res: number[] = [];
    enumChildWindows(dlg, (h) => {
      if (getClassName(h) === "Edit") {
        res.push(h);
      }
      return true;
    });
    return res;
  };
  return {
    async open(cmd, title) {
      sendCommand(frame, cmdId(cmd));
      dlg = await waitForDialog(pid, title);
      captureWindowToPng(dlg, join(dir, `${cmd}.png`));
    },
    // click the first row, which gives the list the keys, then go down
    async selectRow(idx) {
      const list = (await virt()).find((r) => r.kind.includes("List"))?.rect;
      if (!list) {
        throw new Error("edit-favorites: no list");
      }
      clickDlg(dlg, list.x + (list.dx >> 1), list.y + 4);
      await sleep(200);
      for (let i = 0; i < idx; i++) {
        await pressKey(dlg, VK_DOWN);
      }
    },
    async click(label) {
      const btn = (await virt()).find((r) => r.text === label)?.rect;
      if (!btn) {
        throw new Error(`edit-favorites: no '${label}' button`);
      }
      clickDlg(dlg, btn.x + (btn.dx >> 1), btn.y + (btn.dy >> 1));
      await sleep(200);
    },
    // the selected favorite's name is the last field
    async setName(text) {
      const all = edits();
      if (all.length === 0) {
        throw new Error("edit-favorites: no name field");
      }
      sendText(all[all.length - 1]!, text);
    },
    nameFields: async () => edits().length,
  };
}

// --- ng: a gpui tool window, driven through the control channel -------------

type Box = { x: number; y: number; dx: number; dy: number };

function parseBoxes(layout: string, prefix: string): Box[] {
  const res: Box[] = [];
  for (const line of layout.split("\n")) {
    if (!line.startsWith(prefix)) {
      continue;
    }
    const m = /rect=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(line);
    if (m) {
      res.push({ x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! });
    }
  }
  return res;
}

function ngDriver(client: ControlClient): Driver {
  const req = async (...args: (string | number)[]) =>
    String((await client.request(ControlCommand.TestToolWindow, args))[1] ?? "");
  const layout = () => req("layout", kNgWindow, "", 0, 0, 0, 0);
  const clickBox = async (b: Box) => {
    await req("input", kNgWindow, "click", b.x + (b.dx >> 1), b.y + (b.dy >> 1), 0, 0);
    await sleep(150);
  };
  const textFields = async () => {
    const lines = (await layout()).split("\n").filter((l) => l.includes(` role=${kRoleTextField} `));
    return parseBoxes(lines.join("\n"), "node ");
  };
  return {
    async open(cmd, title) {
      await client.request(ControlCommand.TestInvokeCommand, [cmd]);
      const deadline = Date.now() + kWaitMs;
      while (!(await layout()).startsWith("OK ")) {
        if (Date.now() > deadline) {
          throw new Error(`edit-favorites: no '${title}' window`);
        }
        await sleep(100);
      }
    },
    // the list's rows are its click targets, in order
    async selectRow(idx) {
      const row = parseBoxes(await layout(), "hit ")[idx];
      if (!row) {
        throw new Error(`edit-favorites: no row ${idx}`);
      }
      await clickBox(row);
    },
    async click(label) {
      const lines = (await layout()).split("\n").filter((l) => l.includes(`label='${label}'`));
      const btn = parseBoxes(lines.join("\n"), "node ")[0];
      if (!btn) {
        throw new Error(`edit-favorites: no '${label}' button`);
      }
      await clickBox(btn);
    },
    async setName(text) {
      const all = await textFields();
      const field = all[all.length - 1];
      if (!field) {
        throw new Error("edit-favorites: no name field");
      }
      await clickBox(field);
      for (const ch of text) {
        await req("input", kNgWindow, "char", ch.codePointAt(0)!, 0, 0, 0);
      }
    },
    nameFields: async () => (await textFields()).length,
  };
}

export async function testit(): Promise<void> {
  const dir = tmpPath("edit-favorites");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), "CheckForUpdates = false\nRestoreSession = false\n");

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    for (let p = 1; p <= kPages; p++) {
      await favNav(client, "add", p);
    }
    await expectFavs(client, "OK manual=0 -@1,-@2,-@3", "added");

    const d = USE_NG ? ngDriver(client) : origDriver(client, frame, proc.pid, dir);
    await d.open("CmdEditFavorites", "Edit Favorites");
    if ((await d.nameFields()) !== 1) {
      throw new Error("edit-favorites: Edit Favorites should only have the selected favorite's name");
    }

    // rows: doc.pdf, Page 1, Page 2, Page 3
    await d.selectRow(1);
    await d.click("Move Down");
    await expectFavs(client, "OK manual=1 -@2,-@1,-@3", "Move Down");

    await d.setName("Intro");
    await d.click("Rename");
    await expectFavs(client, "OK manual=1 -@2,Intro@1,-@3", "Rename");

    await d.selectRow(3);
    await d.click("Delete");
    await expectFavs(client, "OK manual=1 -@2,Intro@1", "Delete");

    // with the manual order, a new favorite goes at the end
    await d.click("Close");
    await favNav(client, "add", 3);
    await expectFavs(client, "OK manual=1 -@2,Intro@1,-@3", "added after manual order");

    await d.open("CmdFavoriteAdd", "Add Favorite");
    const n = await d.nameFields();
    if (n !== 2) {
      throw new Error(`edit-favorites: Add Favorite has ${n} text fields, want 2`);
    }
    await d.click("Cancel");
    console.log("edit-favorites: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
