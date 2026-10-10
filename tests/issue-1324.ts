// Landscape pages occupy a full facing/book-view row (issues #1324, #872).
//
// A comic that stores a double-page spread as one landscape image used to pair
// that image with the next portrait. With LandscapeAsSpread (default true) the
// landscape page is its own row; setting it false restores the old pairing.
//
// Fixture: 6 pages, portrait except page 3 (landscape).
// Book view on:  cover | leftover | spread | pair | leftover
//   next-page sequence 1, 2, 3, 4, 6
// Book view off: cover | pair(incl. landscape) | pair | leftover
//   next-page sequence 1, 2, 4, 6
//
// Run: bun tests/issue-1324.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, runStandalone, tmpPath, makePng, makeStoredZip } from "./util.ts";
import { launchControlled, sendCommandSync, killAndWait } from "./win-automation.ts";
import { ControlClient, ControlCommand } from "./control.ts";
import { sleep } from "./winapi.ts";

const PORTRAIT = makePng(80, 100, [40, 80, 180]);
const LANDSCAPE = makePng(160, 100, [200, 40, 40]);

function mixedCbz(): Buffer {
  return makeStoredZip([
    { name: "001.png", data: PORTRAIT },
    { name: "002.png", data: PORTRAIT },
    { name: "003.png", data: LANDSCAPE },
    { name: "004.png", data: PORTRAIT },
    { name: "005.png", data: PORTRAIT },
    { name: "006.png", data: PORTRAIT },
  ]);
}

function portraitsCbz(): Buffer {
  return makeStoredZip([
    { name: "001.png", data: PORTRAIT },
    { name: "002.png", data: PORTRAIT },
    { name: "003.png", data: PORTRAIT },
    { name: "004.png", data: PORTRAIT },
    { name: "005.png", data: PORTRAIT },
    { name: "006.png", data: PORTRAIT },
  ]);
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
      throw new Error(`issue-1324: could not read current page: ${String(res[1] ?? "").trim()}`);
    }
    await sleep(50);
  }
}

async function pageTurnSequence(client: ControlClient, frame: number): Promise<number[]> {
  sendCommandSync(frame, cmdId("CmdGoToFirstPage"));
  await client.waitForRenderIdle();
  const got: number[] = [await currentPage(client)];
  for (;;) {
    sendCommandSync(frame, cmdId("CmdGoToNextPage"));
    await client.waitForRenderIdle();
    const page = await currentPage(client);
    if (page === got[got.length - 1]) {
      break;
    }
    got.push(page);
    if (got.length > 20) {
      throw new Error(`issue-1324: page turns did not stop: ${got.join(",")}`);
    }
  }
  return got;
}

function expectSeq(got: number[], want: number[], label: string): void {
  if (got.join(",") !== want.join(",")) {
    throw new Error(`issue-1324: ${label}: got ${got.join(",")} want ${want.join(",")}`);
  }
  console.log(`issue-1324: ${label}: ${got.join(",")}`);
}

async function runSession(opts: {
  cbz: string;
  spread: boolean;
  viewCmd: string;
  want: number[];
  label: string;
}): Promise<void> {
  const appdata = tmpPath(`issue-1324-${opts.label.replace(/[^a-z0-9]+/gi, "-")}-appdata`);
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    [
      "ReuseInstance = false",
      "RestoreSession = false",
      "ShowStartPage = false",
      "CheckForUpdates = false",
      "ComicBookUI [",
      `    LandscapeAsSpread = ${opts.spread}`,
      "]",
    ].join("\n"),
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, opts.cbz]);
  try {
    await client.waitForRenderIdle();
    sendCommandSync(frame, cmdId(opts.viewCmd));
    await client.waitForRenderIdle();
    const got = await pageTurnSequence(client, frame);
    expectSeq(got, opts.want, opts.label);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const mixed = tmpPath("issue-1324-mixed.cbz");
  const portraits = tmpPath("issue-1324-portraits.cbz");
  writeFileSync(mixed, mixedCbz());
  writeFileSync(portraits, portraitsCbz());

  await runSession({
    cbz: mixed,
    spread: true,
    viewCmd: "CmdBookView",
    want: [1, 2, 3, 4, 6],
    label: "book view landscape-as-spread",
  });
  await runSession({
    cbz: mixed,
    spread: false,
    viewCmd: "CmdBookView",
    want: [1, 2, 4, 6],
    label: "book view LandscapeAsSpread=false",
  });
  await runSession({
    cbz: mixed,
    spread: true,
    viewCmd: "CmdFacingView",
    want: [1, 3, 4, 6],
    label: "facing landscape-as-spread",
  });
  await runSession({
    cbz: portraits,
    spread: true,
    viewCmd: "CmdBookView",
    want: [1, 2, 4, 6],
    label: "book view all portraits",
  });

  console.log("PASS: landscape pages occupy a full facing/book row (issues #1324, #872)");
}

if (import.meta.main) {
  await runStandalone(testit);
}
