// #5978: hovering a home-page thumbnail that has scrolled under the search
// field must not draw the selection outline on top of the search field.
import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { IS_MAC } from "./host";
import { ROOT, runStandalone, tmpPath } from "./util";
import { postMessage, setCursorPos, sleep } from "./winapi";
import { findCanvas, launchControlled, killAndWait, ensureModifierKeysUp } from "./win-automation";
import { ControlCommand, type HomeSelection, waitForHome } from "./control.ts";

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

function sameRect(a: number[], b: number[]): boolean {
  return a.length === b.length && a.every((v, i) => v === b[i]);
}

function rectsOverlap(a: number[], b: number[]): boolean {
  if (a[2]! <= 0 || a[3]! <= 0 || b[2]! <= 0 || b[3]! <= 0) {
    return false;
  }
  return a[0]! < b[0]! + b[2]! && a[0]! + a[2]! > b[0]! && a[1]! < b[1]! + b[3]! && a[1]! + a[3]! > b[1]!;
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

    // Windows posts WM_MOUSEWHEEL at the canvas. macOS delivers a wheel
    // through the control channel, over the thumbnail band.
    const canvas = IS_MAC ? 0 : findCanvas(frame);
    if (!IS_MAC && !canvas) {
      throw new Error("issue-5978: no canvas");
    }

    // A wheel notch is applied asynchronously, so the state right after posting
    // it is still the pre-scroll one - and the predicate below is already true
    // then, which is what used to make this test read stale rects and give up
    // before the band had moved at all. Wait for the band to settle instead.
    const settled = async (): Promise<HomeSelection> => {
      const deadline = Date.now() + 4000;
      let prev = await waitForHome(client, (s) => s.ready && s.entries === nFiles, "home page lost its files");
      for (;;) {
        await sleep(60);
        const cur = await waitForHome(client, (s) => s.ready && s.entries === nFiles, "home page lost its files");
        if (sameRect(cur.outlineFull, prev.outlineFull) || Date.now() >= deadline) {
          return cur;
        }
        prev = cur;
      }
    };

    let sawWouldOverlap = false;
    let last = await settled();
    await ensureModifierKeysUp();
    for (let i = 0; i < 12; i++) {
      if (IS_MAC) {
        const area = last.thumbsArea;
        const x = area.length >= 4 && area[2]! > 0 ? Math.round(area[0]! + area[2]! / 2) : 200;
        const y = area.length >= 4 && area[3]! > 0 ? Math.round(area[1]! + Math.min(40, area[3]! / 2)) : 240;
        const res = await client.request(ControlCommand.TestInput, ["wheel", x, y, -WHEEL_DELTA, 0]);
        const raw = String(res[1] ?? "");
        if (res[0] !== 0 || raw.startsWith("ERR")) {
          throw new Error(`issue-5978: wheel failed: ${raw || res[0]}`);
        }
      } else {
        postMessage(canvas, WM_MOUSEWHEEL, (-WHEEL_DELTA << 16) >>> 0, 0n);
      }
      const h = await settled();
      if (rectsOverlap(h.outline, h.search)) {
        throw new Error(`issue-5978: painted outline overlaps the search field: ${h.raw}`);
      }
      if (rectsOverlap(h.outlineFull, h.search)) {
        sawWouldOverlap = true;
        break;
      }
      if (sameRect(h.outlineFull, last.outlineFull)) {
        throw new Error(`issue-5978: the thumbnail band stopped scrolling after ${i + 1} notches: ${h.raw}`);
      }
      last = h;
    }
    if (!sawWouldOverlap) {
      throw new Error(`issue-5978: could not scroll a selected thumbnail under the search field: ${last.raw}`);
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
