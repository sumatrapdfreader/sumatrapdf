// Source filenames in the Favorites tree are bold (issue #6043). The PDF and
// its page label deliberately produce "Page MMMM.pdf : Page NNNN.pdf": the bold
// source filename must come before the regular favorite text.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control";
import { IS_MAC } from "./host";
import { assemblePdf, cmdId, runStandalone, tmpPath, USE_NG } from "./util";
import {
  captureWindowPixels,
  captureWindowToPng,
  findVisibleChildWindow,
  getClientRect,
  moveWindow,
  sendMessage,
  sleep,
  treeClearSelection,
  treeGetItemHeight,
  treeGetRoot,
  WM_COMMAND,
} from "./winapi";
import { killAndWait, launchControlled } from "./win-automation";

function makePdf(): string {
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R /PageLabels << /Nums [0 << /P (NNNN.pdf) >>] >> >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
  ];
  return assemblePdf(objs);
}

type InkCount = { left: number; right: number; span: number; bounds: string };

// Returns null while the tree isn't showing the favorite yet - the caller polls,
// so "nothing drawn" is a retry, not a failure.
function countLabelHalves(tree: number): InkCount | null {
  const cap = captureWindowPixels(tree);
  if (!cap) {
    return null;
  }
  const rowHeight = treeGetItemHeight(tree);
  if (rowHeight <= 0 || rowHeight > cap.h) {
    return null;
  }
  const { w, data } = cap;
  const bgX = Math.max(0, w - 20);
  const bgY = Math.min(cap.h - 1, rowHeight + 10);
  const bgIdx = (bgY * w + bgX) * 4;
  const bg = [data[bgIdx]!, data[bgIdx + 1]!, data[bgIdx + 2]!];
  const isInk = (x: number, y: number): boolean => {
    const i = (y * w + x) * 4;
    const distance = Math.abs(data[i]! - bg[0]!) + Math.abs(data[i + 1]! - bg[1]!) + Math.abs(data[i + 2]! - bg[2]!);
    return distance > 90;
  };

  let minX = w;
  let maxX = -1;
  for (let y = 1; y < rowHeight - 1; y++) {
    for (let x = 8; x < w - 20; x++) {
      if (isInk(x, y)) {
        minX = Math.min(minX, x);
        maxX = Math.max(maxX, x);
      }
    }
  }
  if (maxX <= minX) {
    return null;
  }

  // The two strings have similar widths and are separated by " : ", so the
  // center of their combined ink bounds falls inside the separator. Ignore a
  // few center columns so the colon cannot affect either glyph count.
  const midX = Math.floor((minX + maxX) / 2);
  let left = 0;
  let right = 0;
  for (let y = 1; y < rowHeight - 1; y++) {
    for (let x = minX; x < midX - 2; x++) {
      if (isInk(x, y)) {
        left++;
      }
    }
    for (let x = midX + 3; x <= maxX; x++) {
      if (isInk(x, y)) {
        right++;
      }
    }
  }
  return { left, right, span: maxX - minX, bounds: `${minX}-${maxX}x0-${rowHeight}` };
}

// ng has no favorites tree window. favView= is that pane in frame dips; the
// first row is the compact "filename : label" line.
function countFrameLabel(frame: number, raw: string): InkCount | null {
  const view = /favView=(\d+),(\d+),(\d+),(\d+)/.exec(raw);
  const canvas = /canvas=(\d+),(\d+),(\d+),(\d+)/.exec(raw);
  if (!view || !canvas) {
    return null;
  }
  const vx = Number(view[1]);
  const vy = Number(view[2]);
  const vw = Number(view[3]);
  const vh = Number(view[4]);
  if (vw < 40 || vh < 16) {
    return null;
  }
  const cap = captureWindowPixels(frame);
  if (!cap) {
    return null;
  }
  const rc = getClientRect(frame);
  const dipW = Number(canvas[1]) + Number(canvas[3]);
  const dipH = Number(canvas[2]) + Number(canvas[4]);
  const sx = dipW > 0 ? (rc.right - rc.left) / dipW : 1;
  const sy = dipH > 0 ? (rc.bottom - rc.top) / dipH : 1;
  const ox = Math.round(vx * sx);
  const oy = Math.round(vy * sy);
  const w = Math.round(vw * sx);
  const rowH = Math.max(8, Math.round(18 * sy));
  const { data } = cap;
  const fullW = cap.w;
  const at = (x: number, y: number) => ((y + oy) * fullW + (x + ox)) * 4;
  const bgX = Math.max(0, w - 20);
  const bgY = Math.min(rowH - 1, Math.floor(rowH / 2));
  const bgIdx = at(bgX, bgY);
  const bg = [data[bgIdx]!, data[bgIdx + 1]!, data[bgIdx + 2]!];
  const isInk = (x: number, y: number): boolean => {
    const i = at(x, y);
    const distance = Math.abs(data[i]! - bg[0]!) + Math.abs(data[i + 1]! - bg[1]!) + Math.abs(data[i + 2]! - bg[2]!);
    return distance > 90;
  };
  let minX = w;
  let maxX = -1;
  for (let y = 1; y < rowH - 1; y++) {
    for (let x = 8; x < w - 20; x++) {
      if (isInk(x, y)) {
        minX = Math.min(minX, x);
        maxX = Math.max(maxX, x);
      }
    }
  }
  if (maxX <= minX) {
    return null;
  }
  const midX = Math.floor((minX + maxX) / 2);
  let left = 0;
  let right = 0;
  for (let y = 1; y < rowH - 1; y++) {
    for (let x = minX; x < midX - 2; x++) {
      if (isInk(x, y)) {
        left++;
      }
    }
    for (let x = midX + 3; x <= maxX; x++) {
      if (isInk(x, y)) {
        right++;
      }
    }
  }
  return { left, right, span: maxX - minX, bounds: `${minX}-${maxX}x0-${rowH}` };
}

async function testTheme(theme: "Light" | "Dark"): Promise<void> {
  const dir = tmpPath(`issue-6043-${theme.toLowerCase()}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "Page MMMM.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdf, makePdf(), "latin1");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    `UiLanguage = en\nTheme = ${theme}\nRestoreSession = false\nShowStartPage = false\nShowFavorites = false\nSidebarDx = 500\n`,
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    moveWindow(frame, 40, 20, 1000, 800);
    await client.waitForRenderIdle();
    const add = await client.request(ControlCommand.TestFavoriteNav, ["add", 1]);
    if (add[0] !== 0) {
      throw new Error(`issue-6043: could not add favorite: ${String(add[1] ?? "")}`);
    }
    if (USE_NG) {
      await client.request(ControlCommand.TestInvokeCommand, ["CmdFavoriteToggle"]);
    } else {
      sendMessage(frame, WM_COMMAND, cmdId("CmdFavoriteToggle"), 0);
    }

    const deadline = Date.now() + 5_000;
    let tree = 0;
    let ink: InkCount = { left: 0, right: 0, span: 0, bounds: "" };
    let prev: InkCount | null = null;
    let minSpan = 0;
    while (Date.now() < deadline) {
      let cur: InkCount | null = null;
      if (USE_NG) {
        const raw = String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
        if ((/top=favorites/.test(raw) || /bottom=favorites/.test(raw)) && /favRows=[1-9]/.test(raw)) {
          tree = frame;
          cur = countFrameLabel(frame, raw);
          minSpan = 18 * 6;
        }
      } else {
        tree = findVisibleChildWindow(frame, "SysTreeView32");
        if (tree && treeGetRoot(tree)) {
          treeClearSelection(tree);
          await sleep(20);
          cur = countLabelHalves(tree);
          minSpan = treeGetItemHeight(tree) * 6;
        }
      }
      if (cur) {
        ink = cur;
        // The row can be captured mid-repaint, with the icon drawn but not
        // yet the text. Only trust a reading that spans more than the icon
        // and that repeats, meaning the row is done painting.
        const wideEnough = cur.span > minSpan;
        const stable = prev !== null && prev.left === cur.left && prev.right === cur.right;
        if (wideEnough && stable && cur.left > 20 && cur.right > 20) {
          break;
        }
      }
      prev = cur;
      await sleep(40);
    }
    if (!tree) {
      throw new Error("issue-6043: Favorites tree did not appear");
    }
    captureWindowToPng(tree, join(dir, "favorites.png"));
    if (ink.left <= 20 || ink.right <= 20) {
      throw new Error(`issue-6043: expected two copies of the filename (${JSON.stringify(ink)})`);
    }
    if (ink.span <= minSpan) {
      throw new Error(`issue-6043: favorite label never finished painting (${JSON.stringify(ink)})`);
    }
    if (ink.left < ink.right * 1.08) {
      throw new Error(
        `issue-6043: source filename is not first and visibly bold ` +
          `(source=${ink.left} px, regular=${ink.right} px, bounds=${ink.bounds})`,
      );
    }
    console.log(`  ${theme}: bold source filename comes first: ${ink.left} bold px, ${ink.right} regular px ✓`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  if (IS_MAC) {
    console.log("SKIP issue-6043: bold favorite text is counted from a window DC");
    return;
  }
  await testTheme("Light");
  await testTheme("Dark");
}

if (import.meta.main) {
  await runStandalone(testit);
}
