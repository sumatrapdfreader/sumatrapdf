// The floating Find window reserves the full N / M status width from the total
// hit count, so moving to the last result cannot resize the search combo box.
import { writeFileSync } from "node:fs";
import { ControlCommand, type ControlClient } from "./control.ts";
import { runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import {
  enumWindows,
  enumChildWindows,
  findChildWindow,
  getClassName,
  getParentWindow,
  getWindowPid,
  getWindowRect,
  getWindowText,
  postMessage,
  sendMessage,
  sendText,
  sleep,
  VK_END,
  VK_HOME,
  VK_RETURN,
  WM_KEYDOWN,
} from "./winapi.ts";

const query = "needle";
// Keep page and hit counts at different digit widths: the reservation is based
// on hits, not pages.
const pageCount = 9;
const hitsPerPage = 111;
const hitCount = pageCount * hitsPerPage;

function buildPdf(): Buffer {
  const objects: string[] = [];
  const fontObject = 3;
  objects[1] = "<< /Type /Catalog /Pages 2 0 R >>";
  objects[fontObject] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";

  const pages: number[] = [];
  let objectNumber = 4;
  for (let page = 1; page <= pageCount; page++) {
    const pageObject = objectNumber++;
    const contentObject = objectNumber++;
    pages.push(pageObject);
    const hits = Array.from({ length: hitsPerPage }, (_, i) => {
      const x = 20 + (i % 10) * 58;
      const y = 760 - Math.floor(i / 10) * 60;
      return `1 0 0 1 ${x} ${y} Tm (${query}) Tj`;
    }).join("\n");
    const content = `BT /F1 6 Tf\n${hits}\nET`;
    objects[pageObject] =
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
      `/Resources << /Font << /F1 ${fontObject} 0 R >> >> /Contents ${contentObject} 0 R >>`;
    objects[contentObject] = `<< /Length ${content.length} >>\nstream\n${content}\nendstream`;
  }
  objects[2] = `<< /Type /Pages /Kids [${pages.map((page) => `${page} 0 R`).join(" ")}] /Count ${pages.length} >>`;

  let pdf = "%PDF-1.5\n";
  const offsets: number[] = [];
  for (let i = 1; i < objectNumber; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${i} 0 obj\n${objects[i]}\nendobj\n`;
  }
  const xrefOffset = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${objectNumber}\n0000000000 65535 f \n`;
  for (const offset of offsets) {
    pdf += `${offset.toString().padStart(10, "0")} 00000 n \n`;
  }
  pdf += `trailer\n<< /Size ${objectNumber} /Root 1 0 R >>\nstartxref\n${xrefOffset}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

async function waitForSelection(client: ControlClient, expected: number, timeoutMs = 30_000): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  let raw = "";
  while (Date.now() < deadline) {
    const response = await client.request(ControlCommand.TestFindResultsOrder, [query, 1]);
    raw = String(response[1] ?? "").trim();
    const match = /sel=(-?\d+)/.exec(raw);
    if (Number(response[0]) === 0 && match && Number(match[1]) === expected) {
      return;
    }
    await sleep(50);
  }
  throw new Error(`find-window-layout: selected result did not reach ${expected}: ${raw}`);
}

async function waitForPage(client: ControlClient, expected: number, timeoutMs = 8000): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  let raw = "";
  while (Date.now() < deadline) {
    const response = await client.request(ControlCommand.TestFavoriteNav, ["page", 0]);
    raw = String(response[1] ?? "").trim();
    // the reply carries the scroll position too ("OK page=N y=N"), so match
    // the page rather than the whole line
    const m = /OK page=(\d+)/.exec(raw);
    if (m && Number(m[1]) === expected) {
      return;
    }
    await sleep(50);
  }
  throw new Error(`find-window-layout: page did not reach ${expected}: ${raw}`);
}

function findWindowByTitle(pid: number, title: string): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) === pid && getWindowText(hwnd) === title) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

function findDirectChild(parent: number, className: string): number {
  let found = 0;
  enumChildWindows(parent, (hwnd) => {
    if (getParentWindow(hwnd) === parent && getClassName(hwnd) === className) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

async function waitForEditSelection(edit: number, expected: number, timeoutMs = 2000): Promise<void> {
  const EM_GETSEL = 0x00b0;
  const deadline = Date.now() + timeoutMs;
  let selection = -1;
  while (Date.now() < deadline) {
    selection = Number(sendMessage(edit, EM_GETSEL, 0, 0)) & 0xffff;
    if (selection === expected) {
      return;
    }
    await sleep(25);
  }
  throw new Error(`find-window-layout: page-range caret stayed at ${selection}, expected ${expected}`);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("find-window-layout.pdf");
  writeFileSync(pdf, buildPdf());

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await waitForSelection(client, 0);

    const findWindow = findWindowByTitle(getWindowPid(frame), "Find");
    const combo = findWindow ? findChildWindow(findWindow, "ComboBox") : 0;
    const searchEdit = combo ? findChildWindow(combo, "Edit") : 0;
    const pagesEdit = findWindow ? findDirectChild(findWindow, "Edit") : 0;
    if (!combo || !searchEdit || !pagesEdit) {
      throw new Error("find-window-layout: find controls not found");
    }
    const initial = getWindowRect(combo);

    // Home in the page-range edit must move its caret, not the result selection.
    const EM_SETSEL = 0x00b1;
    sendText(pagesEdit, "1-9");
    sendMessage(pagesEdit, EM_SETSEL, 3, 3);
    postMessage(pagesEdit, WM_KEYDOWN, VK_HOME, 0);
    await waitForEditSelection(pagesEdit, 0);

    // Enter flushes the page-range edit's pending search. Wait for that scan
    // before testing navigation so it cannot reset the selection afterwards.
    postMessage(searchEdit, WM_KEYDOWN, VK_RETURN, 0);
    await sleep(50);
    await waitForSelection(client, 0);
    // The first End collapses the initial select-all to the text boundary; the
    // second uses the floating Find window's two-press result navigation.
    postMessage(searchEdit, WM_KEYDOWN, VK_END, 0);
    postMessage(searchEdit, WM_KEYDOWN, VK_END, 0);
    await waitForSelection(client, hitCount - 1);
    await waitForPage(client, pageCount);
    const after = getWindowRect(combo);
    const initialWidth = initial.right - initial.left;
    const afterWidth = after.right - after.left;
    if (afterWidth !== initialWidth) {
      throw new Error(`find-window-layout: combo width changed from ${initialWidth} to ${afterWidth}`);
    }
    console.log(
      `find-window-layout: combo width stayed ${initialWidth}px for 1 / ${hitCount} and ${hitCount} / ${hitCount}`,
    );
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
