// Ad-hoc: per-document ebook settings (FileStates -> EBookUI).
//
// Three things to show:
//   1. a per-file FontName overrides the global one for that document only
//   2. an unset per-file field inherits the global value (here: LineSpacing)
//   3. the block round-trips through a settings save and isn't written for
//      documents that don't have one

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, tmpPath } from "./util.ts";
import { makeFixtures } from "./ad-hoc-issue-4600.ts";
import { findCanvas, killAndWait, killProcessesNamed, launchSumatra, waitForFrame } from "./win-automation.ts";
import { captureWindowPixels, moveWindow, setForegroundWindow, showWindow, sleep, SW_RESTORE } from "./winapi.ts";

const EPUB_DIR = tmpPath("epub-font");

async function render(epub: string, settings: string): Promise<Uint8Array> {
  const appdata = tmpPath("issue-4600-perfile-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), settings);
  const proc = launchSumatra(["-appdata", appdata, "-view", "single page", "-zoom", "fit page", epub]);
  try {
    const frame = await waitForFrame(proc.pid!);
    if (!frame) {
      throw new Error(`no frame for ${epub}`);
    }
    showWindow(frame, SW_RESTORE);
    moveWindow(frame, 40, 40, 1000, 800);
    setForegroundWindow(frame);
    await sleep(2500);
    const cap = captureWindowPixels(findCanvas(frame));
    if (!cap) {
      throw new Error("no capture");
    }
    return cap.data;
  } finally {
    await killAndWait(proc);
  }
}

function diffPct(a: Uint8Array, b: Uint8Array): number {
  const n = Math.min(a.length, b.length) / 4;
  let d = 0;
  for (let i = 0; i < n; i++) {
    const o = i * 4;
    if (Math.abs(a[o]! + a[o + 1]! + a[o + 2]! - (b[o]! + b[o + 1]! + b[o + 2]!)) > 40) {
      d++;
    }
  }
  return (d * 100) / n;
}

function prefs(fileBlock: string, epub: string): string {
  return [
    `EBookUI [`,
    `\tFontName = Georgia`,
    `]`,
    `RestoreSession = false`,
    `ShowStartPage = false`,
    `FileStates [`,
    `\t[`,
    `\t\tFilePath = ${epub}`,
    fileBlock,
    `\t]`,
    `]`,
    ``,
  ].join("\n");
}

export async function testit(): Promise<void> {
  makeFixtures();
  await killProcessesNamed("SumatraPDF.exe");
  const epub = join(EPUB_DIR, "none.epub");

  // baseline: global FontName only
  const base = await render(epub, prefs("", epub));
  // same, but this document asks for a very different font
  const perFile = await render(epub, prefs(`\t\tEBookUI [\n\t\t\tFontName = Courier New\n\t\t]`, epub));
  // a per-file block that only sets LineSpacing must still use the global font:
  // it has to render exactly like setting that same LineSpacing globally
  const spacing = await render(epub, prefs(`\t\tEBookUI [\n\t\t\tLineSpacing = 2.0\n\t\t]`, epub));
  const globalSpacing = await render(
    epub,
    prefs("", epub).replace("\tFontName = Georgia", "\tFontName = Georgia\n\tLineSpacing = 2.0"),
  );

  const dFont = diffPct(base, perFile);
  const dSpacing = diffPct(base, spacing);
  const dInherit = diffPct(globalSpacing, spacing);
  console.log(`  per-file FontName: ${dFont.toFixed(2)}% changed (expect > 5)`);
  console.log(`  per-file LineSpacing: ${dSpacing.toFixed(2)}% changed (expect > 5)`);
  console.log(`  unset fields inherit: ${dInherit.toFixed(2)}% vs the same values set globally (expect 0)`);
  if (dFont < 5) {
    throw new Error("per-file FontName did not reach the layout");
  }
  if (dSpacing < 5) {
    throw new Error("per-file LineSpacing did not reach the layout");
  }
  if (dInherit > 0) {
    throw new Error("a per-file block changed something it left unset");
  }

  // round-trip: run without -for-testing so settings are saved, then check the
  // block is still there and that documents without one stay clean
  const appdata = tmpPath("issue-4600-perfile-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  const settingsPath = join(appdata, "SumatraPDF-settings.txt");
  writeFileSync(settingsPath, prefs(`\t\tEBookUI [\n\t\t\tFontName = Courier New\n\t\t]`, epub));
  const other = join(EPUB_DIR, "cls.epub");
  // no -for-testing: that mode never writes the settings file back
  const proc = Bun.spawn([EXE, "-appdata", appdata, other], {
    stdout: "ignore",
    stderr: "ignore",
  });
  const frame = await waitForFrame(proc.pid!);
  if (!frame) {
    throw new Error("no frame for the round-trip run");
  }
  await sleep(2000);
  await killAndWait(proc);

  const saved = readFileSync(settingsPath, "utf8");
  const forEpub = saved.split("FilePath").find((b) => b.includes("none.epub"));
  if (!forEpub?.includes("Courier New")) {
    throw new Error("the per-file EBookUI block was lost on save");
  }
  // exactly two EBookUI blocks in the file: the global section and this one.
  // cls.epub has a FileState now too and must not have grown an empty block
  const nBlocks = (saved.match(/EBookUI \[/g) ?? []).length;
  if (nBlocks !== 2) {
    throw new Error(`expected 2 EBookUI blocks (global + none.epub), found ${nBlocks}`);
  }
  console.log(`  round-trip: kept for none.epub, no block written for cls.epub ✓`);
}

if (import.meta.main) {
  const { runStandalone } = await import("./util.ts");
  await runStandalone(testit);
}
