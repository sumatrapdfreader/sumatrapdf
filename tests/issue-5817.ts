// ThumbnailsType = full renders the whole first page into a portrait frame;
// legacy keeps the old landscape crop of the page's top. Changing the setting
// while the app runs empties the cache and a background thread re-creates
// the thumbnails of the files that still exist (issue #5817).
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { loadPng, makeOnePagePdf, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

// FileThumbnails.h: legacy frame 212x150, full frame 150x212
const LEGACY = { dx: 212, dy: 150 };
const FULL = { dx: 150, dy: 212 };

// mirrors GetThumbnailPathTemp: md5 of the path, lowercase hex, .png
function thumbnailPath(appData: string, docPath: string): string {
  const hash = new Bun.CryptoHasher("md5").update(docPath).digest("hex");
  return join(appData, "sumatrapdfcache", `${hash}.png`);
}

function writeSettings(appData: string, thumbnailsType: string): void {
  const lines = [
    "UiLanguage = en",
    "CheckForUpdates = false",
    "RememberOpenedFiles = true",
    "ShowStartPage = true",
    `ThumbnailsType = ${thumbnailsType}`,
    "",
  ];
  writeFileSync(join(appData, "SumatraPDF-settings.txt"), lines.join("\n"));
}

// the app saves its settings (which the test then edits) a moment after
// opening the files
async function waitForSavedHistory(appData: string, names: string[]): Promise<void> {
  const path = join(appData, "SumatraPDF-settings.txt");
  const deadline = Date.now() + 15_000;
  while (Date.now() < deadline) {
    const s = existsSync(path) ? readFileSync(path, "utf8") : "";
    if (s.includes("FileStates [") && names.every((n) => s.includes(n))) {
      return;
    }
    await sleep(100);
  }
  throw new Error(`issue-5817: file history not saved to ${path}`);
}

// edits the saved settings in place, as a user would, so the file history stays
function setThumbnailsType(appData: string, thumbnailsType: string): void {
  const path = join(appData, "SumatraPDF-settings.txt");
  const s = readFileSync(path, "utf8");
  const line = `ThumbnailsType = ${thumbnailsType}`;
  const re = /^ThumbnailsType = .*$/m;
  writeFileSync(path, re.test(s) ? s.replace(re, line) : `${s}\n${line}\n`);
}

function pngSize(path: string): { w: number; h: number } | null {
  if (!existsSync(path)) {
    return null;
  }
  try {
    const img = loadPng(path);
    return { w: img.w, h: img.h };
  } catch {
    // being written
    return null;
  }
}

// waits until the thumbnail exists and accept() likes its size
async function waitForThumb(
  path: string,
  what: string,
  accept: (w: number, h: number) => boolean,
  timeoutMs: number,
): Promise<{ w: number; h: number }> {
  const deadline = Date.now() + timeoutMs;
  let last: { w: number; h: number } | null = null;
  while (Date.now() < deadline) {
    last = pngSize(path);
    if (last && accept(last.w, last.h)) {
      return last;
    }
    await sleep(100);
  }
  const got = last ? `${last.w}x${last.h}` : "missing";
  throw new Error(`issue-5817: ${what}: thumbnail is ${got}: ${path}`);
}

const isLegacy = (w: number, h: number) => w === LEGACY.dx && h === LEGACY.dy;
// the whole page fits the portrait frame: one side touches it, nothing exceeds it
const isFull = (w: number, h: number) => w <= FULL.dx && h <= FULL.dy && (w === FULL.dx || h === FULL.dy);

export async function testit(): Promise<void> {
  const root = tmpPath("issue-5817");
  rmSync(root, { recursive: true, force: true });
  const appData = join(root, "appdata");
  mkdirSync(appData, { recursive: true });
  writeSettings(appData, "full");

  const portrait = join(root, "portrait.pdf");
  const landscape = join(root, "landscape.pdf");
  writeFileSync(portrait, makeOnePagePdf({ mediaBox: "[0 0 612 792]" }));
  writeFileSync(landscape, makeOnePagePdf({ mediaBox: "[0 0 792 612]" }));
  const thumbPortrait = thumbnailPath(appData, portrait);
  const thumbLandscape = thumbnailPath(appData, landscape);

  // not -for-testing: the test edits the saved settings, which -for-testing
  // never writes
  const { proc, client } = await launchControlled(["-appdata", appData, portrait, landscape], { saveSettings: true });
  try {
    await client.waitForRenderIdle();
    // full: a letter page is width-bound (150x194), a landscape one too (150x116)
    const p = await waitForThumb(thumbPortrait, "full portrait", isFull, 15_000);
    if (p.h <= p.w) {
      throw new Error(`issue-5817: full portrait thumbnail should be taller than wide, is ${p.w}x${p.h}`);
    }
    const l = await waitForThumb(thumbLandscape, "full landscape", isFull, 15_000);
    if (l.h >= l.w) {
      throw new Error(`issue-5817: full landscape thumbnail should be wider than tall, is ${l.w}x${l.h}`);
    }

    // editing the settings file reloads it; the change re-creates the cache
    // on a background thread
    await waitForSavedHistory(appData, ["portrait.pdf", "landscape.pdf"]);
    setThumbnailsType(appData, "legacy");
    await waitForThumb(thumbPortrait, "regenerated legacy portrait", isLegacy, 30_000);
    await waitForThumb(thumbLandscape, "regenerated legacy landscape", isLegacy, 30_000);

    setThumbnailsType(appData, "full");
    await waitForThumb(thumbPortrait, "regenerated full portrait", isFull, 30_000);
    await waitForThumb(thumbLandscape, "regenerated full landscape", isFull, 30_000);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
