// #6252: the citation hover popup takes the wheel.
//  - over the link: Shift + wheel scrolls it, Ctrl + wheel zooms it, and a
//    horizontal wheel scrolls it too (mouse software sends Shift + wheel as one)
//  - over the popup itself: wheel scrolls it, Ctrl + wheel zooms it
//
// A test's cursor can't hold a hover (the canvas gets WM_MOUSELEAVE at once), so
// TestRefHover opens the popup. Page 1 is one big link to page 2.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control";
import { assemblePdf, runStandalone, tmpPath } from "./util";
import { clientToScreen, getClientRect, postMessage, sleep } from "./winapi";
import { findCanvas, killAndWait, launchControlled } from "./win-automation";

const WM_MOUSEWHEEL = 0x020a;
const WM_MOUSEHWHEEL = 0x020e;
const MK_SHIFT = 0x0004;
const MK_CONTROL = 0x0008;
const WHEEL_DELTA = 120;

function makePdf(): string {
  const lines: string[] = [];
  for (let i = 0; i < 60; i++) {
    lines.push(`(Reference entry line ${i + 1}) Tj 0 -12 Td`);
  }
  const content2 = `BT /F1 10 Tf 72 740 Td ${lines.join(" ")} ET`;
  const content1 = "BT /F1 24 Tf 72 700 Td (See reference [1]) Tj ET";
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R 4 0 R 9 0 R] /Count 3 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 5 0 R >> >> " +
      "/Contents 6 0 R /Annots [8 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 5 0 R >> >> /Contents 7 0 R >>",
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
    `<< /Length ${content1.length} >>\nstream\n${content1}\nendstream`,
    `<< /Length ${content2.length} >>\nstream\n${content2}\nendstream`,
    "<< /Type /Annot /Subtype /Link /Rect [0 0 612 792] /Border [0 0 0] /Dest [4 0 R /XYZ 72 600 null] >>",
    // a page after the destination, so scrolling down never runs out
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 5 0 R >> >> /Contents 7 0 R >>",
  ]);
}

type Popup = { visible: boolean; hwnd: number; page: number; y: number; zoom: number };

// scrolling down moves to a later page or further down the same page
function below(q: Popup, p: Popup): boolean {
  return q.page > p.page || (q.page === p.page && q.y > p.y);
}

async function popupState(client: ControlClient): Promise<Popup> {
  const res = await client.request(ControlCommand.TestRefHover, []);
  const raw = String(res[1] ?? "");
  if (/visible=0/.test(raw)) {
    return { visible: false, hwnd: 0, page: 0, y: 0, zoom: 0 };
  }
  const m = /visible=1 hwnd=(-?\d+) page=(-?\d+) y=(-?\d+) zoom=(\d+)/.exec(raw);
  if (!m) {
    throw new Error(`issue-6252: TestRefHover failed: ${raw}`);
  }
  return { visible: true, hwnd: +m[1]!, page: +m[2]!, y: +m[3]!, zoom: +m[4]! };
}

async function waitFor(client: ControlClient, what: string, pred: (p: Popup) => boolean): Promise<Popup> {
  const deadline = Date.now() + 8000;
  for (;;) {
    const p = await popupState(client);
    if (pred(p)) {
      return p;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6252: ${what}; popup is ${JSON.stringify(p)}`);
    }
    await sleep(50);
  }
}

// WM_MOUSE[H]WHEEL: wParam = delta (high word) | key flags, lParam = screen point
function postWheel(hwnd: number, msg: number, delta: number, keys: number, pt: { x: number; y: number }): void {
  const wp = (delta & 0xffff) * 0x10000 + keys;
  const lp = (pt.y & 0xffff) * 0x10000 + (pt.x & 0xffff);
  postMessage(hwnd, msg, wp, lp);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6252");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    ["UiLanguage = en", "CheckForUpdates = false", "RestoreSession = false", "CitationHoverDelay = 100", ""].join("\n"),
  );

  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf], { saveSettings: true });
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const canvas = findCanvas(frame);
    const rc = getClientRect(canvas);
    const x = Math.floor((rc.right - rc.left) / 2);
    const y = Math.floor((rc.bottom - rc.top) / 3);
    const onLink = clientToScreen(canvas, x, y);
    const res = await client.request(ControlCommand.TestRefHover, ["show", x, y]);
    if (res[0] !== 0) {
      throw new Error(`issue-6252: could not show the popup: ${res[1]}`);
    }
    let p = await waitFor(client, "the popup did not open", (p) => p.visible && p.page === 2);

    // over the link: Shift + wheel down scrolls down
    postWheel(canvas, WM_MOUSEWHEEL, -WHEEL_DELTA, MK_SHIFT, onLink);
    p = await waitFor(client, "Shift + wheel on the link did not scroll", (q) => below(q, p));

    // a horizontal wheel (Shift + wheel from mouse software) scrolls: right is down
    postWheel(canvas, WM_MOUSEHWHEEL, WHEEL_DELTA, 0, onLink);
    p = await waitFor(client, "horizontal wheel on the link did not scroll", (q) => below(q, p));

    // Ctrl + wheel up zooms in
    postWheel(canvas, WM_MOUSEWHEEL, WHEEL_DELTA, MK_CONTROL, onLink);
    p = await waitFor(client, "Ctrl + wheel on the link did not zoom", (q) => q.zoom > p.zoom);

    // over the popup: plain wheel scrolls, Ctrl + wheel zooms
    postWheel(p.hwnd, WM_MOUSEWHEEL, -WHEEL_DELTA, 0, onLink);
    p = await waitFor(client, "wheel on the popup did not scroll", (q) => below(q, p));
    postWheel(p.hwnd, WM_MOUSEWHEEL, WHEEL_DELTA, MK_CONTROL, onLink);
    await waitFor(client, "Ctrl + wheel on the popup did not zoom", (q) => q.zoom > p.zoom);
    console.log("issue-6252: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
