// Issue #6305: reading a comic rotated 90° with Fit Page, a double-page spread
// (an image wider than it is tall) turns tall and narrow and Fit Page shrinks
// it; the reader had to rotate back and forth around every spread.
//
// ComicBookUI.AutoRotateWidePages turns such a page on its own, in single-page
// layouts only, so its long side follows the window's: with the document
// rotated 90° the spread shows upright, and on a portrait window an unrotated
// spread turns 90°. Regular pages keep the document's rotation.
//
// The CBZ is generated: page 1 is portrait, page 2 is a spread. TestPageInfo
// reports each page's display rotation and the current page's size on screen.
//
// Run: bun tests/issue-6305.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { cmdId, EXE, makePng, makeZip, runStandalone, tmpPath, writeAppdata } from "./util.ts";
import { sendCommandSync, waitForFrame } from "./win-automation.ts";

// comics measure page sizes lazily, so the spread sits well past the pages
// measured at load
const PAGE_COUNT = 40;
const SPREAD_PAGE = 20;

type PageInfo = { rotations: number[]; screenDx: number; screenDy: number };

async function pageInfo(client: { request: Function }): Promise<PageInfo> {
  const res = await client.request(ControlCommand.TestPageInfo, []);
  const raw = String(res[1] ?? "");
  const rot = /rotations=([\d,]+)/.exec(raw);
  const scr = /screen=(\d+),(\d+)/.exec(raw);
  if (!rot || !scr) {
    throw new Error(`issue-6305: TestPageInfo has no rotations / screen fields: ${raw.trim()}`);
  }
  return { rotations: rot[1]!.split(",").map(Number), screenDx: +scr[1]!, screenDy: +scr[2]! };
}

function settings(autoRotate: boolean, view: string): string {
  return [
    "UiLanguage = en",
    "RestoreSession = false",
    "ShowStartPage = false",
    "CheckForUpdates = false",
    `DefaultDisplayMode = ${view}`,
    "ComicBookUI [",
    `\tAutoRotateWidePages = ${autoRotate}`,
    "\tDefaultZoom = fit page",
    "]",
  ].join("\n");
}

type Case = {
  name: string;
  autoRotate: boolean;
  view: string;
  windowPos: string;
  rotateRight: boolean;
  // the display rotation of regular pages and of the spread
  want: { base: number; spread: number };
  // null: the current page is not the spread in this layout, skip the size check
  spreadWide: boolean | null;
};

const CASES: Case[] = [
  // the report: document rotated right on a landscape window, Fit Page
  {
    name: "rotated, on",
    autoRotate: true,
    view: "single page",
    windowPos: "1000x700@40x40",
    rotateRight: true,
    want: { base: 90, spread: 0 },
    spreadWide: true,
  },
  {
    name: "rotated, off",
    autoRotate: false,
    view: "single page",
    windowPos: "1000x700@40x40",
    rotateRight: true,
    want: { base: 90, spread: 90 },
    spreadWide: false,
  },
  // unrotated on a portrait window (a tablet): the spread turns, LTR so clockwise
  {
    name: "portrait window, on",
    autoRotate: true,
    view: "single page",
    windowPos: "600x1000@40x40",
    rotateRight: false,
    want: { base: 0, spread: 90 },
    spreadWide: false,
  },
  // a landscape window already fits a spread: nothing to do
  {
    name: "landscape window, on",
    autoRotate: true,
    view: "single page",
    windowPos: "1000x700@40x40",
    rotateRight: false,
    want: { base: 0, spread: 0 },
    spreadWide: true,
  },
  // facing layouts are left alone
  {
    name: "facing, on",
    autoRotate: true,
    view: "facing",
    windowPos: "600x1000@40x40",
    rotateRight: false,
    want: { base: 0, spread: 0 },
    spreadWide: null,
  },
];

async function runCase(c: Case, cbz: string): Promise<void> {
  const appdata = writeAppdata("issue-6305", settings(c.autoRotate, c.view));
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      await client.waitForRenderIdle(30000);
      if (c.rotateRight) {
        sendCommandSync(frame, cmdId("CmdRotateRight"));
      }
      for (let i = 1; i < SPREAD_PAGE; i++) {
        sendCommandSync(frame, cmdId("CmdGoToNextPage"));
      }
      await client.waitForRenderIdle(30000);
      const info = await pageInfo(client);
      const want = Array.from({ length: PAGE_COUNT }, (_, i) => (i + 1 === SPREAD_PAGE ? c.want.spread : c.want.base));
      if (info.rotations.join(",") !== want.join(",")) {
        throw new Error(`issue-6305: ${c.name}: page rotations ${info.rotations.join(",")}, want ${want.join(",")}`);
      }
      const wide = info.screenDx > info.screenDy;
      if (c.spreadWide !== null && wide !== c.spreadWide) {
        throw new Error(
          `issue-6305: ${c.name}: spread is ${info.screenDx}x${info.screenDy} on screen, want it ${c.spreadWide ? "wider" : "taller"} than tall`,
        );
      }
      console.log(
        `issue-6305: ${c.name}: rotations ${info.rotations.join(",")}, spread ${info.screenDx}x${info.screenDy}`,
      );
    },
    ["-appdata", appdata, "-window-pos", c.windowPos, cbz],
  );
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6305-data");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const cbz = join(dir, "spread.cbz");
  const enc = new TextEncoder();
  writeFileSync(
    cbz,
    makeZip([
      ...Array.from({ length: PAGE_COUNT }, (_, i) => {
        const n = i + 1;
        const spread = n === SPREAD_PAGE;
        return {
          name: `${String(n).padStart(3, "0")}.png`,
          data: makePng(spread ? 1800 : 600, 900, spread ? [200, 60, 40] : [40, 60, 200]),
        };
      }),
      { name: "ComicInfo.xml", data: enc.encode("<ComicInfo/>") },
    ]),
  );
  for (const c of CASES) {
    await runCase(c, cbz);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
