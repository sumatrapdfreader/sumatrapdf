// Fit Content: one wheel notch flips the page in every non-continuous view
// (single page, facing, book view), not only in single page. The continuous
// views keep scrolling.
//
// Run: bun tests/issue-5407.ts [--no-build]

import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { assemblePdf, runStandalone, tmpPath } from "./util.ts";
import { clientToScreen, getScrollPos, packCoords, SB_VERT, sendMessage, sleep } from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled } from "./win-automation.ts";

const WM_MOUSEWHEEL = 0x020a;
const WHEEL_NOTCH = 120;
const N_PAGES = 6;

// pages with a small block of text inside wide margins, so Fit Content zooms
// in past the page and the page needs a scrollbar
function makePdf(): string {
  const objs = ["<< /Type /Catalog /Pages 2 0 R >>", "", "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>"];
  const kids: string[] = [];
  for (let i = 0; i < N_PAGES; i++) {
    const content = `BT /F1 12 Tf 250 420 Td (Page ${i + 1}) Tj 0 -20 Td (some text here) Tj ET`;
    const pageNo = objs.length + 1;
    objs.push(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> /Contents ${pageNo + 1} 0 R >>`,
    );
    objs.push(`<< /Length ${content.length} >>\nstream\n${content}\nendstream`);
    kids.push(`${pageNo} 0 R`);
  }
  objs[1] = `<< /Type /Pages /Count ${N_PAGES} /Kids [${kids.join(" ")}] >>`;
  return assemblePdf(objs);
}

let PDF = "";

// [view, flips]
const CASES: [string, boolean][] = [
  ["single page", true],
  ["facing", true],
  ["book view", true],
  ["continuous", false],
];

async function wheelOnce(view: string): Promise<{ before: number; after: number; scrolled: boolean }> {
  const { proc, client, frame } = await launchControlled(["-view", view, "-zoom", "fit content", PDF]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const page = async () => {
      const raw = String((await client.request(ControlCommand.TestCurrentTab, []))[1] ?? "");
      const m = /page=(\d+)/.exec(raw);
      if (!m) {
        throw new Error(`issue-5407: no page in '${raw}'`);
      }
      return +m[1]!;
    };
    const canvas = findCanvas(frame);
    const before = await page();
    const y0 = getScrollPos(canvas, SB_VERT);
    const pt = clientToScreen(canvas, 200, 200);
    sendMessage(canvas, WM_MOUSEWHEEL, BigInt((-WHEEL_NOTCH & 0xffff) << 16), packCoords(pt.x, pt.y));
    // a page turn or scroll is done when the message returns; give the paint a moment
    await sleep(200);
    return { before, after: await page(), scrolled: getScrollPos(canvas, SB_VERT) !== y0 };
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-5407");
  mkdirSync(dir, { recursive: true });
  PDF = join(dir, "pages.pdf");
  writeFileSync(PDF, makePdf(), "latin1");
  for (const [view, flips] of CASES) {
    const r = await wheelOnce(view);
    const flipped = r.after > r.before;
    if (flips && !flipped) {
      throw new Error(`issue-5407: ${view}: a wheel notch didn't flip the page (${JSON.stringify(r)})`);
    }
    if (!flips && flipped) {
      throw new Error(
        `issue-5407: ${view}: a wheel notch flipped the page instead of scrolling (${JSON.stringify(r)})`,
      );
    }
  }
  console.log("issue-5407: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
