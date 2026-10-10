// Home page list view: rows scrolled into view must show the directory path,
// not just the file name. The name/path split used to be computed during layout
// and only for the rows on screen at that moment, so rows revealed by scrolling
// (which reuses the cached layout, without re-measuring) were drawn with an
// empty path rect.
//
// After one page-down the bottom rows are exactly those that were never
// measured. Counting text pixels in the directory column there: ~850 when they
// render, ~0-120 (just the row separator lines) when they don't.
import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control";
import { IS_MAC } from "./host";
import { ROOT, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util";
import { getWindowRect, postMessage, readWindowDCColumn, sleep, waitForWindowIdle } from "./winapi";
import { findCanvas, launchControlled, launchSumatra, waitForFrame, killAndWait } from "./win-automation";

const WM_VSCROLL = 0x0115;
const SB_PAGEDOWN = 3;
const nFiles = 30;
const kMinTextPixels = 400;

// The checks below sample fixed fractions of the window, and whether 30 rows
// are more than one page depends on how tall it is, so the window can't be
// whatever size the screen (or a CI runner's work area) suggests: at
// 1853x1111 the list is laid out with the directory column outside the sampled
// band and every row fits on one page, and the test failed for both reasons.
const WINDOW_POS = ["-window-pos", "900x700@40x40"];

// Pixels differing from the background in the directory column, over a band of
// rows. x covers only the path text: file names end well to its left, the size
// column sits to its right.
function dirColumnTextPixels(hwnd: number, yFrac: number, h: number): number {
  const r = getWindowRect(hwnd);
  const dx = r.right - r.left;
  const dy = r.bottom - r.top;
  const px: number[] = [];
  for (let x = Math.floor(dx * 0.45); x < Math.floor(dx * 0.72); x += 2) {
    px.push(...readWindowDCColumn(hwnd, x, Math.floor(dy * yFrac), h));
  }
  const counts = new Map<number, number>();
  for (const p of px) {
    counts.set(p, (counts.get(p) ?? 0) + 1);
  }
  let bg = 0;
  let best = -1;
  for (const [color, n] of counts) {
    if (n > best) {
      best = n;
      bg = color;
    }
  }
  return px.filter((p) => p !== bg).length;
}

type ListRow = { path: string; pathRect: [number, number, number, number] };

// pathRect is the directory text measured while that row was painted.
// A row that was never measured stays 0x0.
function parseListRows(dump: string): ListRow[] {
  const rows: ListRow[] = [];
  let path = "";
  for (const line of dump.split("\n")) {
    const row = /^row=\d+ size='[^']*' sizeRect=-?\d+,-?\d+,-?\d+,-?\d+ progress='[^']*' path=(.*)$/.exec(line);
    if (row) {
      path = row[1]!;
      continue;
    }
    const rc = /^pathRect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)$/.exec(line);
    if (rc) {
      rows.push({ path, pathRect: [+rc[1]!, +rc[2]!, +rc[3]!, +rc[4]!] });
    }
  }
  return rows;
}

function measuredDirs(rows: ListRow[], dirPath: string): number {
  return rows.filter((r) => r.path.includes(dirPath) && r.pathRect[2] > 0 && r.pathRect[3] > 0).length;
}

async function homeRows(client: ControlClient): Promise<ListRow[] | null> {
  const res = await client.request(ControlCommand.TestHomeListRows, []);
  if (res[0] !== 0) {
    return null;
  }
  return parseListRows(String(res[1] ?? ""));
}

async function macListDirs(dir: string, dirPath: string): Promise<void> {
  const { proc, client, frame } = await launchControlled([...WINDOW_POS, "-appdata", dir]);
  try {
    const deadline = Date.now() + 8000 * SLOW_BUILD_FACTOR;
    let rows: ListRow[] | null = null;
    let measured = 0;
    while (Date.now() < deadline) {
      rows = await homeRows(client);
      measured = rows ? measuredDirs(rows, dirPath) : 0;
      if (measured > 0) {
        break;
      }
      await sleep(40);
    }
    if (!rows || measured === 0) {
      throw new Error(`no directory text in the initial rows (${measured})`);
    }

    postMessage(frame, WM_VSCROLL, SB_PAGEDOWN, 0);
    const deadline2 = Date.now() + 8000 * SLOW_BUILD_FACTOR;
    let after = measured;
    while (Date.now() < deadline2) {
      rows = await homeRows(client);
      after = rows ? measuredDirs(rows, dirPath) : 0;
      // every row is built, including ones that start below the fold
      if (rows && after === rows.length && rows.length >= nFiles) {
        break;
      }
      await sleep(40);
    }
    if (!rows || after !== rows.length || rows.length < nFiles) {
      throw new Error(`rows scrolled into view show no directory path (${after}/${rows?.length ?? 0})`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-5870-list-dirs");
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
      `HomePageViewMode = list\nFileStates [\n${states.join("\n")}\n]\n`,
  );

  const dirPath = join(dir, "sub");
  if (IS_MAC) {
    await macListDirs(dir, dirPath);
    return;
  }

  const proc = launchSumatra([...WINDOW_POS, "-appdata", dir]);
  try {
    const frame = await waitForFrame(proc.pid!);
    if (!frame) {
      throw new Error("no frame window");
    }
    const canvas = findCanvas(frame) || frame;
    await waitForWindowIdle(canvas, 8000, 150);

    // sanity: rows visible at startup show a directory
    const deadline = Date.now() + 4000;
    let atTop = 0;
    while (Date.now() < deadline) {
      atTop = dirColumnTextPixels(frame, 0.3, 60);
      if (atTop >= kMinTextPixels) {
        break;
      }
      await sleep(40);
    }
    if (atTop < kMinTextPixels) {
      throw new Error(`no directory text in the initial rows (${atTop} text pixels): test setup is wrong`);
    }

    postMessage(canvas, WM_VSCROLL, SB_PAGEDOWN, 0);
    const deadline2 = Date.now() + 4000;
    let atBottom = 0;
    while (Date.now() < deadline2) {
      atBottom = dirColumnTextPixels(frame, 0.86, 60) + dirColumnTextPixels(frame, 0.9, 60);
      if (atBottom >= kMinTextPixels) {
        break;
      }
      await sleep(40);
    }
    if (atBottom < kMinTextPixels) {
      throw new Error(`rows scrolled into view show no directory path (${atBottom} text pixels)`);
    }
  } finally {
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
