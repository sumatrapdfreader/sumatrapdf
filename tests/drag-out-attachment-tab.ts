// Dragging a tab out of its window reloads the document by path in the new
// window. A tab opened from an embedded attachment has no path: the drag closed
// it and fired the empty-path debug report in CreateEngineFromFile()
// (crash 2026-10-13-12-51-c0c4). Such a tab now stays where it is.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { assemblePdf, pollUntil, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import {
  clientToScreen,
  getClientRect,
  getWindowText,
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

    const tabs = findChildByClass(frame, TABS_CLASS);
    if (!tabs) {
      throw new Error("drag-out-attachment-tab: no tab bar");
    }
    const y = Math.floor(getClientRect(tabs).bottom / 2);
    const x = await findAttachmentTabX(frame, tabs, y);

    // press on the tab, pull it down past the drag threshold, drop below the bar
    const dropY = y + 300;
    const at = clientToScreen(tabs, x, y);
    setCursorPos(at.x, at.y);
    postMessage(tabs, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(x, y));
    postMessage(tabs, WM_MOUSEMOVE, MK_LBUTTON, packCoords(x, y + 40));
    postMessage(tabs, WM_MOUSEMOVE, MK_LBUTTON, packCoords(x, dropY));
    postMessage(tabs, WM_LBUTTONUP, 0, packCoords(x, dropY));
    await sleep(300 * SLOW_BUILD_FACTOR);

    // a debug report under -for-testing ends the process
    if (proc.exitCode !== null) {
      throw new Error(`drag-out-attachment-tab: the app exited with ${proc.exitCode} after the drag`);
    }
    await client.waitForRenderIdle();
    const title = getWindowText(frame);
    if (!title.includes(ATTACHMENT)) {
      throw new Error(`drag-out-attachment-tab: the attachment tab is gone, title='${title}'`);
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
