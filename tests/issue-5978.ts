// #5978: hovering a home-page thumbnail that has scrolled under the search
// field must not draw the selection outline on top of the search field.
import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, runStandalone, tmpPath } from "./util";
import { postMessage, setCursorPos, sleep } from "./winapi";
import { findCanvas, launchControlled, killAndWait } from "./win-automation";
import type { ControlClient, HomeSelection } from "./control.ts";

const WM_MOUSEWHEEL = 0x020a;
const WHEEL_DELTA = 120;
const nFiles = 8;

function makeAppDir(): string {
  const dir = tmpPath("issue-5978");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(join(dir, "sub"), { recursive: true });
  const src = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const states: string[] = [];
  for (let i = 0; i < nFiles; i++) {
    const p = join(dir, "sub", `doc-${String(i).padStart(2, "0")}.pdf`);
    copyFileSync(src, p);
    states.push(`\t[\n\t\tFilePath = ${p}\n\t\tOpenCount = ${nFiles - i}\n\t]`);
  }
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nRememberOpenedFiles = true\n` +
      `HomePageViewMode = thumbnails\nFileStates [\n${states.join("\n")}\n]\n`,
  );
  return dir;
}

function rectsOverlap(a: number[], b: number[]): boolean {
  if (a[2]! <= 0 || a[3]! <= 0 || b[2]! <= 0 || b[3]! <= 0) {
    return false;
  }
  return a[0]! < b[0]! + b[2]! && a[0]! + a[2]! > b[0]! && a[1]! < b[1]! + b[3]! && a[1]! + a[3]! > b[1]!;
}

async function waitForHome(
  client: ControlClient,
  pred: (h: HomeSelection) => boolean,
  what: string,
  timeoutMs = 8000,
): Promise<HomeSelection> {
  const deadline = Date.now() + timeoutMs;
  let last: HomeSelection | null = null;
  for (;;) {
    last = await client.homeSelection();
    if (last.ready && pred(last)) {
      return last;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5978: ${what} (last: ${last.raw})`);
    }
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  // a cursor left sitting over the thumbnails keeps re-selecting whatever is
  // under it as the band scrolls, so the selection never ends up under the
  // search field (same hover-vs-selection problem as issue-1136)
  setCursorPos(0, 0);
  // compact window so a few files overflow the thumbs band and we can scroll
  // a thumbnail under the search field
  const { proc, client, frame } = await launchControlled(["-appdata", makeAppDir(), "-window-pos", "820x500@20x20"]);
  try {
    await waitForHome(
      client,
      (h) => h.entries === nFiles && h.searchBox && h.search[2]! > 0 && h.outlineFull[3]! > 0,
      "home page never listed the files with a search box and a selection outline",
    );

    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("issue-5978: no canvas");
    }

    let sawWouldOverlap = false;
    for (let i = 0; i < 12; i++) {
      postMessage(canvas, WM_MOUSEWHEEL, (-WHEEL_DELTA << 16) >>> 0, 0n);
      const h = await waitForHome(
        client,
        (s) => s.ready && s.entries === nFiles,
        "home page lost its files after scroll",
      );
      if (rectsOverlap(h.outline, h.search)) {
        throw new Error(`issue-5978: painted outline overlaps the search field: ${h.raw}`);
      }
      if (rectsOverlap(h.outlineFull, h.search)) {
        sawWouldOverlap = true;
        break;
      }
    }
    if (!sawWouldOverlap) {
      throw new Error("issue-5978: could not scroll a selected thumbnail under the search field");
    }
    console.log("issue-5978: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
