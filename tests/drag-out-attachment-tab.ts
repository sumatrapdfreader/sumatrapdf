// Dragging a tab out of its window reloads the document by path in the new
// window. A tab opened from an embedded attachment has no path: the drag closed
// it and fired the empty-path debug report in CreateEngineFromFile()
// (crash 2026-10-13-12-51-c0c4). Such a tab now stays where it is.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, pollUntil, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util.ts";
import {
  clientToScreen,
  enumWindows,
  getClientRect,
  getWindowPid,
  getWindowText,
  isWindowVisible,
  MK_LBUTTON,
  packCoords,
  postMessage,
  setCursorPos,
  sleep,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { clickAt, findChildByClass, killAndWait, launchControlled } from "./win-automation.ts";

const TABS_CLASS = "SumatraTabsCtrlClass";
const ATTACHMENT = "image002.png";
// how far below the tab bar the tab is dropped
const DROP_DY = 300;

// 1x1 red PNG
const PNG_BASE64 = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8DwHwAFBQIAX8jx0gAAAABJRU5ErkJggg==";

function makePdfWithImageAttachment(): string {
  const png = Buffer.from(PNG_BASE64, "base64").toString("latin1");
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    "<< /Type /Annot /Subtype /FileAttachment /P 3 0 R /Rect [200 400 400 600] /FS 5 0 R /Name /PushPin >>",
    `<< /Type /Filespec /F (${ATTACHMENT}) /UF (${ATTACHMENT}) /EF << /F 6 0 R >> >>`,
    `<< /Type /EmbeddedFile /Length ${png.length} /Params << /Size ${png.length} >> >>\nstream\n${png}\nendstream`,
  ]);
}

// x of a point on the attachment's tab: the tab a click there selects.
// Scans from the left so the first hit is far from the tab's close button.
async function findAttachmentTabX(frame: number, tabs: number, y: number): Promise<number> {
  const width = getClientRect(tabs).right;
  const step = 30;
  // start on the first tab (Home)
  await clickAt(tabs, step, y, 50);
  for (let x = 2 * step; x < width; x += step) {
    await clickAt(tabs, x, y, 50);
    if (getWindowText(frame).includes(ATTACHMENT)) {
      return x;
    }
  }
  throw new Error("drag-out-attachment-tab: could not find the attachment's tab");
}

async function findTabBar(frame: number): Promise<{ tabs: number; x: number; y: number }> {
  const tabs = findChildByClass(frame, TABS_CLASS);
  if (!tabs) {
    throw new Error("drag-out-attachment-tab: no tab bar");
  }
  const y = Math.floor(getClientRect(tabs).bottom / 2);
  const x = await findAttachmentTabX(frame, tabs, y);
  return { tabs, x, y };
}

// press on the tab, pull it down past the drag threshold, drop below the bar
function dragOutOrig(frame: number, { tabs, x, y }: { tabs: number; x: number; y: number }): void {
  const dropY = y + DROP_DY;
  const at = clientToScreen(tabs, x, y);
  setCursorPos(at.x, at.y);
  postMessage(tabs, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(x, y));
  postMessage(tabs, WM_MOUSEMOVE, MK_LBUTTON, packCoords(x, y + 40));
  postMessage(tabs, WM_MOUSEMOVE, MK_LBUTTON, packCoords(x, dropY));
  postMessage(tabs, WM_LBUTTONUP, 0, packCoords(x, dropY));
}

function processWindowTitles(pid: number): string[] {
  const titles: string[] = [];
  enumWindows((h) => {
    if (getWindowPid(h) === pid && isWindowVisible(h)) {
      titles.push(getWindowText(h));
    }
    return true;
  });
  return titles;
}

// ng draws its tabs: the attachment's is the frame layout node labelled with it
async function dragOutNg(client: ControlClient): Promise<void> {
  const layout = String((await client.request(ControlCommand.TestToolWindow, ["layout", "frame"]))[1] ?? "");
  const re = new RegExp(`node rect=(-?\\d+),(-?\\d+),(\\d+),(\\d+) role=\\d+ label='[^']*${ATTACHMENT}`);
  const m = re.exec(layout);
  if (!m) {
    throw new Error(`drag-out-attachment-tab: no tab labelled ${ATTACHMENT}:\n${layout.slice(0, 2000)}`);
  }
  const x = Number(m[1]) + Math.floor(Number(m[3]) / 3);
  const y = Number(m[2]) + Math.floor(Number(m[4]) / 2);
  const input = async (...args: (string | number)[]) => client.request(ControlCommand.TestInput, args);
  await input("down", x, y, 0, 0);
  await input("move", x, y + 40, 1, 0);
  await input("move", x, y + DROP_DY, 1, 0);
  await input("up", x, y + DROP_DY, 0, 0);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("drag-out-attachment-tab");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "outer.pdf");
  writeFileSync(pdf, makePdfWithImageAttachment(), "latin1");

  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    await pollUntil(
      async () => {
        const res = await client.request(ControlCommand.TestMarkupAnnots, ["open-embedded", 0, 0]);
        return { code: res[0], text: String(res[1] ?? "") };
      },
      (r) => r.code === 0 && /OK/.test(r.text),
      {
        timeoutMs: 8000 * SLOW_BUILD_FACTOR,
        error: (r) => `drag-out-attachment-tab: open-embedded failed: code=${r.code} ${r.text}`,
      },
    );
    await pollUntil(
      () => getWindowText(frame),
      (t) => t.includes(ATTACHMENT),
      {
        timeoutMs: 8000 * SLOW_BUILD_FACTOR,
        error: (t) => `drag-out-attachment-tab: expected tab ${ATTACHMENT}, title='${t}'`,
      },
    );
    await client.waitForRenderIdle();

    if (USE_NG) {
      await dragOutNg(client);
    } else {
      dragOutOrig(frame, await findTabBar(frame));
    }
    await sleep(300 * SLOW_BUILD_FACTOR);

    // a debug report under -for-testing ends the process
    if (proc.exitCode !== null) {
      throw new Error(`drag-out-attachment-tab: the app exited with ${proc.exitCode} after the drag`);
    }
    await client.waitForRenderIdle();
    // ng saves an attachment to a file before opening it, so its tab has a
    // path and moves to a window of its own
    const titles = USE_NG ? processWindowTitles(proc.pid!) : [getWindowText(frame)];
    if (!titles.some((t) => t.includes(ATTACHMENT))) {
      throw new Error(`drag-out-attachment-tab: the attachment tab is gone, titles=${JSON.stringify(titles)}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("drag-out-attachment-tab: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
