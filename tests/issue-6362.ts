// A page that renders slowly must not hold up going to the next page
// (issue #6362). Updating the toolbar on a page change asked whether the
// document has redaction marks, which waited for the lock the render thread
// holds while it loads a page. Pages 2-5 are slow to load (one content stream
// of ~600K paths, compressed to little). After Right to page 2 the render
// cache loads page 3 ahead; Right pressed meanwhile must land at once.
//
// Run: bun tests/issue-6362.ts [--no-build]

import { writeFileSync } from "node:fs";
import { deflateSync } from "node:zlib";
import { ControlCommand, type ControlClient } from "./control.ts";
import { assemblePdf, runStandalone, tmpPath } from "./util.ts";
import { findCanvas, killAndWait, launchControlled } from "./win-automation.ts";
import { postMessage, sleep, VK_RIGHT, WM_KEYDOWN } from "./winapi.ts";

// paths on a slow page
const SLOW_PATHS = 600_000;
const SLOW_PAGES = 4;
// a page change that waited for the slow page took seconds
const MAX_NAV_MS = 500;

// page 1 is plain, the rest share one slow content stream
function makePdf(): string {
  const font = "/Font << /F1 << /Type /Font /Subtype /Type1 /BaseFont /Helvetica >> >>";
  const page = (contents: number) =>
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents ${contents} 0 R /Resources << ${font} >> >>`;
  const label = "BT /F1 72 Tf 250 400 Td (1) Tj ET";
  const slow = deflateSync(Buffer.from("0.5 0 0 rg 100 100 m 109 103 l 100 107 l f\n".repeat(SLOW_PATHS)));
  const nPages = 1 + SLOW_PAGES;
  // objects: 1 catalog, 2 pages, 3 plain content, 4 slow content, 5.. the pages
  const kids = Array.from({ length: nPages }, (_, i) => `${5 + i} 0 R`).join(" ");
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    `<< /Type /Pages /Count ${nPages} /Kids [${kids}] >>`,
    `<< /Length ${label.length} >>\nstream\n${label}\nendstream`,
    `<< /Length ${slow.length} /Filter /FlateDecode >>\nstream\n${slow.toString("latin1")}\nendstream`,
    page(3),
    ...Array.from({ length: SLOW_PAGES }, () => page(4)),
  ]);
}

async function currentPage(client: ControlClient): Promise<number> {
  const res = await client.request(ControlCommand.TestCurrentTab, []);
  return Number(/page=(\d+)/.exec(String(res[1] ?? ""))?.[1] ?? 0);
}

// Right, then how long until the window is on `page` (-1: never)
async function pressRight(client: ControlClient, canvas: number, page: number, timeoutMs: number): Promise<number> {
  const t0 = Date.now();
  postMessage(canvas, WM_KEYDOWN, VK_RIGHT, 0);
  while (Date.now() - t0 < timeoutMs) {
    if ((await currentPage(client)) === page) {
      return Date.now() - t0;
    }
    await sleep(10);
  }
  return -1;
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6362.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    // page 1 and the page 2 the cache loads ahead of it
    await client.waitForRenderIdle(60000);
    const canvas = findCanvas(frame);
    // a key posted this early is now and then dropped; press again
    let ms = -1;
    for (let i = 0; i < 3 && ms < 0; i++) {
      ms = await pressRight(client, canvas, 2, 3000);
    }
    if (ms < 0) {
      throw new Error("issue-6362: Right did not go to page 2");
    }
    // the cache is now loading page 3
    await sleep(300);
    ms = await pressRight(client, canvas, 3, 30000);
    console.log(`issue-6362: Right while page 3 loads took ${ms} ms`);
    if (ms < 0 || ms > MAX_NAV_MS) {
      throw new Error(`issue-6362: going to the next page waited for the render (${ms} ms)`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
