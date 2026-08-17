// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5069
//
// Two things about the mouse wheel on a page zoomed past the window:
//
// 1. RememberViewOffsetOnPageTurn used to apply to the page turn the wheel does
//    when it runs off the bottom of a page. That offset is the bottom (that is
//    how you got there), so the new page opened at its bottom, skipping its top
//    - and since the view was still at the bottom, every further notch flipped
//    another page. Reaching a page edge by scrolling must open the next page at
//    its top; the remembered offset is for explicit page turns (N / P, click to
//    turn), not for scrolling off the edge.
//
// 2. MouseWheelTurnsPage makes one notch a page turn instead of a scroll, and
//    composes with RememberViewOffsetOnPageTurn: the view stays parked and the
//    pages move under it. Alt + wheel still scrolls, which is how the test
//    parks the view in the first place.
//
// Run:  bun tests/issue-5069.ts [--no-build]

import { writeFileSync, mkdirSync, rmSync } from "node:fs";
import { runStandalone, tmpPath } from "./util.ts";
import { ControlClient, ControlCommand } from "./control.ts";
import { launchControlled, findCanvas, killAndWait } from "./win-automation.ts";
import { sendMessage, getScrollPos, sleep, SB_VERT } from "./winapi.ts";

const WM_MOUSEWHEEL = 0x020a;
// one notch down / up, and one notch down with Alt held (MK_ALT is 0x20)
const WHEEL_DOWN = 0xff880000n;
const WHEEL_UP = 0x00780000n;
const WHEEL_DOWN_ALT = 0xff880020n;

const PAGE_COUNT = 6;

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
      throw new Error("issue-5069: could not read the current page");
    }
    await sleep(100);
  }
}

async function wheel(canvas: number, wp: bigint): Promise<void> {
  sendMessage(canvas, WM_MOUSEWHEEL, wp, 0n);
  await sleep(130);
}

type Session = {
  client: ControlClient;
  canvas: number;
  close: () => Promise<void>;
};

async function openZoomed(name: string, settings: string[]): Promise<Session> {
  const pdfPath = tmpPath("issue-5069.pdf");
  writeFileSync(pdfPath, buildPdf());
  const appdata = tmpPath(`issue-5069-${name}-appdata`);
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    `${appdata}/SumatraPDF-settings.txt`,
    ["RestoreSession = false", "SmoothScroll = false", ...settings, ""].join("\n"),
  );

  // zoomed to 200% in a small window, so the page is much taller than the view
  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-window-pos",
    "700x500@100x100",
    "-view",
    "single page",
    "-zoom",
    "200",
    pdfPath,
  ]);
  await client.waitForRenderIdle();
  await sleep(300);
  return {
    client,
    canvas: findCanvas(frame),
    close: async () => {
      client.close();
      await killAndWait(proc);
    },
  };
}

// Alt + wheel scrolls by half a page and never turns a page, so it is a way to
// move within the page whatever the wheel is set to do
async function altScrollToBottom(canvas: number): Promise<number> {
  let y = getScrollPos(canvas, SB_VERT);
  for (let i = 0; i < 30; i++) {
    await wheel(canvas, WHEEL_DOWN_ALT);
    const next = getScrollPos(canvas, SB_VERT);
    if (next === y) {
      return y;
    }
    y = next;
  }
  return y;
}

// scrolling off the bottom of a page opens the next one at its top
async function testEdgeFlipStartsAtTop(): Promise<void> {
  const s = await openZoomed("edge", ["RememberViewOffsetOnPageTurn = true"]);
  try {
    const bottom = await altScrollToBottom(s.canvas);
    if (bottom <= 0) {
      throw new Error(`issue-5069: expected the zoomed page to scroll, scrollY stayed ${bottom}`);
    }
    if ((await currentPage(s.client)) !== 1) {
      throw new Error("issue-5069: Alt + wheel should scroll within page 1, not turn pages");
    }
    await wheel(s.canvas, WHEEL_DOWN);
    const page = await currentPage(s.client);
    const y = getScrollPos(s.canvas, SB_VERT);
    console.log(`issue-5069 edge flip: bottom of page 1 was scrollY=${bottom}, one notch -> page=${page} scrollY=${y}`);
    if (page !== 2) {
      throw new Error(`issue-5069: wheeling past the bottom of page 1 should go to page 2, got page ${page}`);
    }
    if (y !== 0) {
      throw new Error(
        `issue-5069: page 2 should open at its top after scrolling off page 1, got scrollY=${y} ` +
          `(the remembered view offset must not apply to a page turn caused by scrolling)`,
      );
    }
  } finally {
    await s.close();
  }
}

// MouseWheelTurnsPage + RememberViewOffsetOnPageTurn: the view stays put and
// one notch moves a whole page, in both directions
async function testWheelTurnsPage(): Promise<void> {
  const s = await openZoomed("wheel", ["RememberViewOffsetOnPageTurn = true", "MouseWheelTurnsPage = true"]);
  try {
    // park the view partway down page 1 (not at the top, not at the bottom)
    for (let i = 0; i < 2; i++) {
      await wheel(s.canvas, WHEEL_DOWN_ALT);
    }
    const parked = getScrollPos(s.canvas, SB_VERT);
    if (parked <= 0) {
      throw new Error(`issue-5069: Alt + wheel should still scroll when MouseWheelTurnsPage is on (scrollY=${parked})`);
    }
    if ((await currentPage(s.client)) !== 1) {
      throw new Error("issue-5069: Alt + wheel should not turn a page");
    }

    await wheel(s.canvas, WHEEL_DOWN);
    let page = await currentPage(s.client);
    let y = getScrollPos(s.canvas, SB_VERT);
    console.log(
      `issue-5069 wheel turns page: parked at scrollY=${parked}, one notch down -> page=${page} scrollY=${y}`,
    );
    if (page !== 2 || y !== parked) {
      throw new Error(
        `issue-5069: one notch down should be page 2 at scrollY=${parked}, got page ${page} scrollY=${y}`,
      );
    }

    await wheel(s.canvas, WHEEL_UP);
    page = await currentPage(s.client);
    y = getScrollPos(s.canvas, SB_VERT);
    console.log(`issue-5069 wheel turns page: one notch up -> page=${page} scrollY=${y}`);
    if (page !== 1 || y !== parked) {
      throw new Error(`issue-5069: one notch up should be page 1 at scrollY=${parked}, got page ${page} scrollY=${y}`);
    }
  } finally {
    await s.close();
  }
}

export async function testit(): Promise<void> {
  await testEdgeFlipStartsAtTop();
  await testWheelTurnsPage();
}

if (import.meta.main) {
  await runStandalone(testit);
}
