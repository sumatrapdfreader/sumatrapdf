// With an overlay scrollbar (orig's smart / overlay modes, every ng bar), the
// vertical bar marks where the search matches are (#5692):
// - continuous: the whole document, each match at its place in it
// - single page: the current page only, updated when the page changes
// - single page with ScrollbarInSinglePage: one mark per match's page
// and the marks go away with the find UI. (A smart bar shows while it has marks,
// but only in the active window, which a test window isn't reliably.)
//
// Run: bun tests/find-scrollbar-markers.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled, pressEscape, sendCommandSync } from "./win-automation.ts";

const WORD = "needle";
const MATCH_PAGES = [2, 5, 8];
const PAGE_COUNT = 10;
// match text near the top of its page: its mark sits just past the page start
const TEXT_Y = 720;

function makePdf(): string {
  const objs = ["<< /Type /Catalog /Pages 2 0 R >>", "", "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>"];
  const kids: string[] = [];
  for (let page = 1; page <= PAGE_COUNT; page++) {
    const line = MATCH_PAGES.includes(page) ? `page ${page} has a ${WORD} on it` : `page ${page} filler`;
    const content = `BT /F1 24 Tf 72 ${TEXT_Y} Td (${line}) Tj ET`;
    const pageObj = objs.length + 1;
    objs.push(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> /Contents ${pageObj + 1} 0 R >>`,
    );
    objs.push(`<< /Length ${content.length} >>\nstream\n${content}\nendstream`);
    kids.push(`${pageObj} 0 R`);
  }
  objs[1] = `<< /Type /Pages /Count ${PAGE_COUNT} /Kids [${kids.join(" ")}] >>`;
  return assemblePdf(objs);
}

interface Markers {
  n: number;
  ticks: number;
  current: number;
  visible: boolean;
  at: number[];
  raw: string;
}

async function markers(client: ControlClient): Promise<Markers> {
  const res = await client.request(ControlCommand.TestLayout, ["get"]);
  const raw = String(res[1] ?? "");
  const m = /findMarkers n=(\d+) ticks=(\d+) current=(-?[\d.]+) visible=(\d) at=([\d.,]*)/.exec(raw);
  if (!m) {
    throw new Error(`find-scrollbar-markers: no findMarkers line\n${raw}`);
  }
  return {
    n: +m[1]!,
    ticks: +m[2]!,
    current: +m[3]!,
    visible: m[4] === "1",
    at: m[5]!.length > 0 ? m[5]!.split(",").map(Number) : [],
    raw: m[0],
  };
}

async function waitMarkers(client: ControlClient, what: string, ok: (m: Markers) => boolean): Promise<Markers> {
  const deadline = Date.now() + 10_000 * SLOW_BUILD_FACTOR;
  let m = await markers(client);
  while (!ok(m)) {
    if (Date.now() > deadline) {
      throw new Error(`find-scrollbar-markers: ${what}: ${m.raw}`);
    }
    await sleep(100);
    m = await markers(client);
  }
  return m;
}

// runs the search through the find UI, as typing it does
async function search(client: ControlClient, startPage: number): Promise<void> {
  const deadline = Date.now() + 20_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const res = await client.request(ControlCommand.TestFindResultsOrder, [WORD, startPage]);
    const raw = String(res[1] ?? "");
    if (!raw.includes("NOTREADY")) {
      return;
    }
    if (Date.now() > deadline) {
      throw new Error(`find-scrollbar-markers: the search never finished: ${raw}`);
    }
    await sleep(100);
  }
}

function near(got: number, want: number, tol: number): boolean {
  return Math.abs(got - want) <= tol;
}

async function run(
  dir: string,
  name: string,
  settings: string,
  args: string[],
  fn: (c: ControlClient, frame: number) => Promise<void>,
) {
  const appdata = join(dir, `appdata-${name}`);
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    `UiLanguage = en\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n${settings}`,
  );
  const { proc, client, frame } = await launchControlled(["-appdata", appdata, ...args]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    await fn(client, frame);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("find-scrollbar-markers");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "pages.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  // continuous: a mark per match, at its page's share of the document
  await run(
    dir,
    "continuous",
    "Scrollbars = overlay\n",
    ["-view", "continuous", "-zoom", "fit width", pdf],
    async (c, frame) => {
      await search(c, 1);
      const m = await waitMarkers(c, "continuous: 3 marks", (m) => m.n === MATCH_PAGES.length);
      MATCH_PAGES.forEach((page, i) => {
        if (!near(m.at[i]!, (page - 1) / PAGE_COUNT, 0.05)) {
          throw new Error(`find-scrollbar-markers: continuous: mark ${i} at ${m.at[i]}, page ${page}: ${m.raw}`);
        }
      });
      if (!m.visible || m.ticks !== MATCH_PAGES.length || m.current < 0) {
        throw new Error(`find-scrollbar-markers: continuous: bar not shown with its marks: ${m.raw}`);
      }
      // closing the find UI takes them away
      await pressEscape(frame);
      await waitMarkers(c, "continuous: marks after closing find", (m) => m.n === 0);
    },
  );

  // single page: the bar is the current page, so only its match is marked
  await run(
    dir,
    "single",
    "Scrollbars = overlay\n",
    ["-view", "single page", "-zoom", "200", pdf],
    async (c, frame) => {
      await search(c, 5);
      const onMatch = await waitMarkers(c, "single page: the current page's mark", (m) => m.n === 1);
      if (onMatch.at[0]! > 0.3) {
        throw new Error(`find-scrollbar-markers: single page: the match is at the page top: ${onMatch.raw}`);
      }
      sendCommandSync(frame, cmdId("CmdGoToNextPage"));
      await waitMarkers(c, "single page: no mark on a page without a match", (m) => m.n === 0);
      sendCommandSync(frame, cmdId("CmdGoToPrevPage"));
      await waitMarkers(c, "single page: the mark back with its page", (m) => m.n === 1);
    },
  );

  // ScrollbarInSinglePage: the bar's range is the pages
  await run(
    dir,
    "bypage",
    "Scrollbars = overlay\nScrollbarInSinglePage = true\n",
    ["-view", "single page", "-zoom", "fit page", pdf],
    async (c) => {
      await search(c, 1);
      const m = await waitMarkers(c, "by page: 3 marks", (m) => m.n === MATCH_PAGES.length);
      MATCH_PAGES.forEach((page, i) => {
        if (!near(m.at[i]!, (page - 1) / PAGE_COUNT, 0.03)) {
          throw new Error(`find-scrollbar-markers: by page: mark ${i} at ${m.at[i]}, page ${page}: ${m.raw}`);
        }
      });
    },
  );

  console.log("find-scrollbar-markers: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
