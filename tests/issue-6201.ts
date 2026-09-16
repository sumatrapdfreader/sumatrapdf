// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6201
//
// ScrollEdgeTurnsPage = false keeps the wheel on the current page: scrolling a
// zoomed-in page in single page view stops at its bottom instead of going to
// the next page. Default (true) is the old behaviour, which tests/issue-5069.ts
// covers.
//
// Run:  bun tests/issue-6201.ts [--no-build]

import { writeFileSync, mkdirSync, rmSync } from "node:fs";
import { runStandalone, tmpPath } from "./util.ts";
import { ControlClient, ControlCommand } from "./control.ts";
import { launchControlled, findCanvas, killAndWait, ensureModifierKeysUp } from "./win-automation.ts";
import { sendMessage, getScrollPos, sleep, SB_VERT } from "./winapi.ts";

const WM_MOUSEWHEEL = 0x020a;
const WHEEL_DOWN = 0xff880000n;
const WHEEL_UP = 0x00780000n;

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

async function currentPage(client: ControlClient): Promise<number> {
  const deadline = Date.now() + 10_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestFavoriteNav, ["page", 0]);
    const m = /OK page=(\d+)/.exec(String(res[1] ?? ""));
    if (m) {
      return +m[1]!;
    }
    if (Date.now() > deadline) {
      throw new Error("issue-6201: could not read the current page");
    }
    await sleep(100);
  }
}

async function wheel(canvas: number, wp: bigint): Promise<void> {
  await ensureModifierKeysUp();
  sendMessage(canvas, WM_MOUSEWHEEL, wp, 0n);
  await sleep(130);
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-6201.pdf");
  writeFileSync(pdfPath, buildPdf());
  const appdata = tmpPath("issue-6201-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    `${appdata}/SumatraPDF-settings.txt`,
    ["RestoreSession = false", "SmoothScroll = false", "ScrollEdgeTurnsPage = false", ""].join("\n"),
  );

  // 200%: the page is taller than the window, so there is something to scroll
  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-view",
    "single page",
    "-zoom",
    "200",
    pdfPath,
  ]);
  const canvas = findCanvas(frame);
  try {
    await client.waitForRenderIdle();
    await sleep(300);

    // wheel down until the page stops moving: it must stay on page 1
    let y = getScrollPos(canvas, SB_VERT);
    let bottom = y;
    for (let i = 0; i < 40; i++) {
      await wheel(canvas, WHEEL_DOWN);
      const page = await currentPage(client);
      if (page !== 1) {
        throw new Error(`issue-6201: wheel left page 1 for page ${page} with ScrollEdgeTurnsPage = false`);
      }
      const next = getScrollPos(canvas, SB_VERT);
      if (next === y) {
        bottom = next;
        break;
      }
      y = next;
      bottom = next;
    }
    if (bottom <= 0) {
      throw new Error(`issue-6201: expected the zoomed page to scroll, scrollY stayed ${bottom}`);
    }

    // and back up: still page 1, back at the top
    for (let i = 0; i < 40; i++) {
      await wheel(canvas, WHEEL_UP);
      const page = await currentPage(client);
      if (page !== 1) {
        throw new Error(`issue-6201: wheeling up left page 1 for page ${page}`);
      }
      if (getScrollPos(canvas, SB_VERT) === 0) {
        break;
      }
    }
    if (getScrollPos(canvas, SB_VERT) !== 0) {
      throw new Error("issue-6201: wheeling up did not reach the top of page 1");
    }
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(appdata, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
