// Merge PDF (CmdMergePDF): the dialog's grid of pages. A page's corner button
// removes it, dragging moves it, a rubber band selects pages and Delete removes
// (then restores) them, another PDF's pages are added where the Add PDF
// question says (after the selected page by default), and the result is saved
// as a new PDF (opened in a tab) and over the open document (reloaded). Each
// page of the test PDFs has a unique width (601, 602, ... / 701, 702) so the
// page order can be read back; the bookmark must follow its page.
//
// Run: bun tests/issue-6070.ts [--no-build]

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, makePdf, runStandalone, tmpPath } from "./util.ts";
import {
  clientToScreen,
  enumWindows,
  findChildWindow,
  getControlText,
  getWindowPid,
  getWindowText,
  isWindowVisible,
  MK_CONTROL,
  MK_LBUTTON,
  packCoords,
  postMessage,
  sendMessage,
  sendText,
  setCursorPos,
  sleep,
  VK_DELETE,
  VK_ESCAPE,
  VK_RETURN,
  WM_KEYDOWN,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

type Rect = { x: number; y: number; dx: number; dy: number };
type Merge = {
  hwnd: number;
  // e.g. "0:1s,0:2r,1:1": source:page, r: removed, s: selected
  items: string;
  canSave: boolean;
  rects: Map<number, { page: Rect; btn: Rect }>;
  raw: string;
};

function parseRect(s: string): Rect {
  const [x, y, dx, dy] = s.split(",").map(Number);
  return { x: x!, y: y!, dx: dx!, dy: dy! };
}

async function merge(client: ControlClient, action = "", arg = "", n = 0): Promise<Merge> {
  const res = await client.request(ControlCommand.TestMergePdf, [action, arg, n]);
  const raw = String(res[1] ?? "");
  const m = /hwnd=(\d+) focused=\d items=(\S*) canSave=(\d) rects=(\S*)/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`issue-6070: TestMergePdf ${action}: ${raw}`);
  }
  const rects = new Map<number, { page: Rect; btn: Rect }>();
  for (const part of m[4]!.split(";").filter(Boolean)) {
    const [idx, page, btn] = part.split(":");
    rects.set(+idx!, { page: parseRect(page!), btn: parseRect(btn!) });
  }
  return { hwnd: +m[1]!, items: m[2]!, canSave: m[3] === "1", rects, raw };
}

// save / saveas: close the dialog, so there is nothing to report but the outcome
async function save(client: ControlClient, action: string, path = ""): Promise<void> {
  const res = await client.request(ControlCommand.TestMergePdf, [action, path, 0]);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0 || raw !== "OK saved=1") {
    throw new Error(`issue-6070: ${action}: ${raw}`);
  }
}

async function waitFor(what: string, f: () => Promise<boolean>) {
  const deadline = Date.now() + 5000;
  while (!(await f())) {
    if (Date.now() > deadline) {
      throw new Error(`issue-6070: ${what}`);
    }
    await sleep(100);
  }
}

async function wantItems(client: ControlClient, items: string, what: string) {
  let s: Merge | undefined;
  await waitFor(`${what}: want items=${items}`, async () => {
    s = await merge(client);
    return s.items === items;
  }).catch((e) => {
    throw new Error(`${e.message}: ${s?.raw}`);
  });
}

// widths and bookmarks of the current tab's document
async function pageState(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestPageInfo, []);
  const m = /widths=([\d,]+) toc=(\S*)/.exec(String(res[1] ?? ""));
  return m ? `${m[1]} ${m[2]}` : "";
}

function mouse(hwnd: number, msg: number, mk: number, pt: { x: number; y: number }) {
  // SetCapture injects a WM_MOUSEMOVE at the real cursor: keep it where we are
  const screen = clientToScreen(hwnd, pt.x, pt.y);
  setCursorPos(screen.x, screen.y);
  sendMessage(hwnd, msg, mk, packCoords(pt.x, pt.y));
}

// the "where to insert" question Add PDF... asks
async function waitForInsertQuestion(pid: number): Promise<number> {
  let found = 0;
  await waitFor("Add PDF didn't ask where to insert", async () => {
    enumWindows((hwnd) => {
      if (getWindowPid(hwnd) === pid && isWindowVisible(hwnd) && getWindowText(hwnd) === "Add PDF") {
        found = hwnd;
        return false;
      }
      return true;
    });
    return found !== 0;
  });
  return found;
}

function center(r: Rect): { x: number; y: number } {
  return { x: r.x + Math.floor(r.dx / 2), y: r.y + Math.floor(r.dy / 2) };
}

function pageRect(s: Merge, idx: number): Rect {
  const r = s.rects.get(idx);
  if (!r) {
    throw new Error(`issue-6070: item ${idx} isn't visible: ${s.raw}`);
  }
  return r.page;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6070");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, makePdf(4, 601, 3, 0), "latin1");
  const pdfBytes = readFileSync(pdf);
  const other = join(dir, "other.pdf");
  writeFileSync(other, makePdf(2, 701, 0, 0), "latin1");
  const merged = join(dir, "merged.pdf");

  const { proc, client, frame } = await launchControlled(["-view", "single page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    sendCommand(frame, cmdId("CmdMergePDF"));
    // the current page starts selected
    await wantItems(client, "0:1s,0:2,0:3,0:4", "the dialog didn't open");
    let s = await merge(client);
    const hwnd = s.hwnd;

    // page 2's corner button, shown on hover, removes it
    const btn = center(s.rects.get(1)!.btn);
    mouse(hwnd, WM_MOUSEMOVE, 0, btn);
    mouse(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, btn);
    mouse(hwnd, WM_LBUTTONUP, 0, btn);
    await wantItems(client, "0:1s,0:2r,0:3,0:4", "the corner button didn't remove page 2");

    // page 4 dragged in front of page 1
    s = await merge(client);
    const from = center(pageRect(s, 3));
    const r0 = pageRect(s, 0);
    const to = { x: r0.x + Math.floor(r0.dx / 8), y: r0.y + Math.floor(r0.dy / 2) };
    mouse(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, from);
    mouse(hwnd, WM_MOUSEMOVE, MK_LBUTTON, to);
    mouse(hwnd, WM_LBUTTONUP, 0, to);
    await wantItems(client, "0:4s,0:1,0:2r,0:3", "dragging didn't move page 4 to the front");

    // Ctrl click adds a page to the selection
    s = await merge(client);
    mouse(hwnd, WM_LBUTTONDOWN, MK_LBUTTON | MK_CONTROL, center(pageRect(s, 3)));
    mouse(hwnd, WM_LBUTTONUP, MK_CONTROL, center(pageRect(s, 3)));
    await wantItems(client, "0:4s,0:1,0:2r,0:3s", "Ctrl click didn't add to the selection");

    // a rubber band from the gap above the first page to the middle of the third
    s = await merge(client);
    const bandFrom = { x: pageRect(s, 0).x - 4, y: pageRect(s, 0).y - 4 };
    const bandTo = center(pageRect(s, 2));
    mouse(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, bandFrom);
    mouse(hwnd, WM_MOUSEMOVE, MK_LBUTTON, bandTo);
    mouse(hwnd, WM_LBUTTONUP, 0, bandTo);
    await wantItems(client, "0:4s,0:1s,0:2rs,0:3", "the rubber band didn't select pages 1-3");

    // Delete removes the selected pages; with all of them removed, restores them
    postMessage(hwnd, WM_KEYDOWN, VK_DELETE, 0);
    await wantItems(client, "0:4rs,0:1rs,0:2rs,0:3", "Delete didn't remove the selected pages");
    postMessage(hwnd, WM_KEYDOWN, VK_DELETE, 0);
    await wantItems(client, "0:4s,0:1s,0:2s,0:3", "Delete didn't restore the removed pages");
    await merge(client, "remove", "2");

    // Add PDF asks where; the page starts as the selected one (3). Esc adds nothing
    await merge(client, "askpos", other);
    let ask = await waitForInsertQuestion(proc.pid);
    let edit = findChildWindow(ask, "Edit");
    if (getControlText(edit) !== "3") {
      throw new Error(`issue-6070: want page 3 in the Add PDF question, got '${getControlText(edit)}'`);
    }
    postMessage(edit, WM_KEYDOWN, VK_ESCAPE, 0);
    await waitFor("Esc didn't close the Add PDF question", async () => !isWindowVisible(ask));
    await wantItems(client, "0:4,0:1,0:2rs,0:3", "a cancelled Add PDF added pages");

    // after page 4: at the end; its first page removed
    await merge(client, "askpos", other);
    ask = await waitForInsertQuestion(proc.pid);
    edit = findChildWindow(ask, "Edit");
    sendText(edit, "4");
    postMessage(edit, WM_KEYDOWN, VK_RETURN, 0);
    await wantItems(client, "0:4,0:1,0:2r,0:3,1:1s,1:2s", "Add PDF after page 4");
    await merge(client, "remove", "4");
    await wantItems(client, "0:4,0:1,0:2r,0:3,1:1rs,1:2", "adding other.pdf");

    // Save As opens the result; the document is untouched
    await save(client, "saveas", merged);
    await waitFor("the merged PDF didn't open", async () => (await pageState(client)) === "604,601,603,702 Target:3;");
    if (!readFileSync(pdf).equals(pdfBytes)) {
      throw new Error("issue-6070: Save As changed the document");
    }

    // Save writes over the open document, which is reloaded
    await merge(client, "open");
    await merge(client, "remove", "0");
    await save(client, "save");
    await waitFor(
      "saving over the document didn't reload it",
      async () => (await pageState(client)) === "601,603,702 Target:2;",
    );
    console.log("issue-6070: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
