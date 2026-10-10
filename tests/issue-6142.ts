// issue #6142: mouse wheel did not scroll a document taller than the window;
// dragging the scrollbar still worked. Two surfaces:
//   - fixed-page (PDF): wheel on the canvas must move the vertical scrollbar
//   - markdown WebView2: the canvas subclasses WM_MOUSEWHEEL and used to
//     swallow it instead of forwarding it to the browser, so a wheel that
//     lands on the parent (the usual routing when the webview isn't focused)
//     did nothing.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { runStandalone, tmpPath } from "./util.ts";
import { findCanvas, launchControlled, killAndWait, ensureModifierKeysUp } from "./win-automation.ts";
import {
  clientToScreen,
  getClientRect,
  getScrollInfo,
  packCoords,
  sendMessage,
  setCursorPos,
  sleep,
} from "./winapi.ts";

const WM_MOUSEWHEEL = 0x020a;
const WHEEL_DELTA = 120;
const PAGE_COUNT = 3;

function buildPdf(): Buffer {
  const objs: string[] = [];
  objs[1] = "<< /Type /Catalog /Pages 2 0 R >>";
  objs[3] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";
  const kids: number[] = [];
  let objNum = 4;
  for (let page = 1; page <= PAGE_COUNT; page++) {
    const pageNum = objNum++;
    const contentNum = objNum++;
    kids.push(pageNum);
    const content = `BT /F1 24 Tf 72 720 Td (page ${page} top) Tj ET BT /F1 24 Tf 72 72 Td (page ${page} bottom) Tj ET`;
    objs[pageNum] =
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
      `/Resources << /Font << /F1 3 0 R >> >> /Contents ${contentNum} 0 R >>`;
    objs[contentNum] = `<< /Length ${content.length} >>\nstream\n${content}\nendstream`;
  }
  objs[2] = `<< /Type /Pages /Kids [${kids.map((k) => `${k} 0 R`).join(" ")}] /Count ${PAGE_COUNT} >>`;
  const maxN = objNum - 1;
  let pdf = "%PDF-1.5\n";
  const offsets: number[] = [];
  for (let i = 1; i <= maxN; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${i} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefPos = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${maxN + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    pdf += off.toString().padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${xrefPos}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

async function waitScrollable(canvas: number, timeoutMs = 8000): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const si = getScrollInfo(canvas);
    if (si.max > (si.page | 0)) {
      return;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6142: canvas is not vertically scrollable: ${JSON.stringify(si)}`);
    }
    await sleep(40);
  }
}

async function wheelDown(canvas: number, notches: number): Promise<void> {
  await ensureModifierKeysUp();
  // mac treats a wheel lParam of 0 as the canvas center. A screen point is
  // not a frame-client point, so it misses the document.
  let lp = 0;
  if (!IS_MAC) {
    const cr = getClientRect(canvas);
    const mid = clientToScreen(canvas, Math.floor(cr.right / 2), Math.floor(cr.bottom / 2));
    setCursorPos(mid.x, mid.y);
    lp = packCoords(mid.x, mid.y);
  }
  const wp = (-WHEEL_DELTA << 16) >>> 0;
  for (let i = 0; i < notches; i++) {
    sendMessage(canvas, WM_MOUSEWHEEL, wp, lp);
  }
}

async function waitScrolledFrom(canvas: number, before: number, timeoutMs = 2000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  let last = before;
  while (Date.now() < deadline) {
    const pos = getScrollInfo(canvas).pos;
    if (pos > before) {
      return pos;
    }
    last = pos;
    await sleep(40);
  }
  return last;
}

async function testPdfWheel(): Promise<void> {
  const dir = tmpPath("issue-6142-pdf");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "issue-6142.pdf");
  writeFileSync(pdf, buildPdf());
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    [
      "RestoreSession = false",
      "ShowStartPage = false",
      "CheckForUpdates = false",
      "SmoothScroll = true",
      "MouseWheelTurnsPage = false",
      "",
    ].join("\n"),
  );

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-view",
    "continuous",
    "-zoom",
    "150",
    pdf,
  ]);
  try {
    await client.waitForRenderIdle();
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("issue-6142: no canvas");
    }
    await waitScrollable(canvas);

    const before = getScrollInfo(canvas).pos;
    await wheelDown(canvas, 6);
    const after = await waitScrolledFrom(canvas, before);
    if (after <= before) {
      throw new Error(`issue-6142: PDF wheel did not scroll (pos ${before} -> ${after})`);
    }
    console.log(`issue-6142: PDF wheel scrolled ${before} -> ${after}`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

async function mdScrollY(client: ControlClient): Promise<number> {
  const deadline = Date.now() + 8000;
  let last = "";
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkdownTocNavigate, [0, 0]);
    const output = String(res[1] ?? "").trim();
    last = output;
    const m = /^OK scrollX=\d+ scrollY=(-?\d+)/.exec(output);
    if (res[0] === 0 && m && Number(m[1]) >= 0) {
      return Number(m[1]);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6142: markdown never reported scroll: ${last}`);
    }
    await sleep(80);
  }
}

async function testMarkdownWheel(): Promise<void> {
  const dir = tmpPath("issue-6142-md");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const paragraphs = Array.from({ length: 80 }, (_, i) => `Paragraph ${i + 1}: enough text to overflow the window.`);
  const md = join(dir, "long.md");
  writeFileSync(md, ["# Start", ...paragraphs].join("\n\n"));
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    ["MarkdownUI [", "\tUseFixedPageUI = false", "]", "RestoreSession = false", "ShowStartPage = false", ""].join("\n"),
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, md]);
  try {
    await client.waitForRenderIdle();
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("issue-6142: no canvas");
    }
    const before = await mdScrollY(client);
    // send to the canvas parent: WebView2 often isn't the focused hwnd, so the
    // wheel lands here and must be forwarded into the browser
    await wheelDown(canvas, 8);
    const deadline = Date.now() + 4000;
    let after = before;
    while (Date.now() < deadline) {
      after = await mdScrollY(client);
      if (after > before + 20) {
        break;
      }
      await sleep(80);
    }
    if (after <= before + 20) {
      throw new Error(`issue-6142: markdown wheel did not scroll (scrollY ${before} -> ${after})`);
    }
    console.log(`issue-6142: markdown wheel scrolled ${before} -> ${after}`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  await testPdfWheel();
  await testMarkdownWheel();
  console.log("issue-6142: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
