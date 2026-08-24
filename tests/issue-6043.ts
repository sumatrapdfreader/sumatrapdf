// Source filenames in the Favorites tree are bold (issue #6043). The PDF and
// its page label deliberately produce "Page MMMM.pdf : Page NNNN.pdf": the bold
// source filename must come before the regular favorite text.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control";
import { cmdId, runStandalone, tmpPath } from "./util";
import {
  captureWindowPixels,
  captureWindowToPng,
  findVisibleChildWindow,
  moveWindow,
  sendMessage,
  sleep,
  treeClearSelection,
  treeGetItemHeight,
  WM_COMMAND,
} from "./winapi";
import { killAndWait, launchControlled } from "./win-automation";

function makePdf(): string {
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R /PageLabels << /Nums [0 << /P (NNNN.pdf) >>] >> >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
  ];
  let body = "%PDF-1.4\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets.push(body.length);
    body += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefStart = body.length;
  body += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    body += `${off.toString().padStart(10, "0")} 00000 n \n`;
  }
  body += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xrefStart}\n%%EOF\n`;
  return body;
}

type InkCount = { left: number; right: number; bounds: string };

function countLabelHalves(tree: number): InkCount {
  const cap = captureWindowPixels(tree);
  if (!cap) {
    throw new Error("issue-6043: could not capture the Favorites tree");
  }
  const rowHeight = treeGetItemHeight(tree);
  if (rowHeight <= 0 || rowHeight > cap.h) {
    throw new Error(`issue-6043: invalid tree row height ${rowHeight}`);
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
    throw new Error("issue-6043: no favorite label pixels found");
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
  return { left, right, bounds: `${minX}-${maxX}x0-${rowHeight}` };
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

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf], { defaultWindowPos: true });
  try {
    moveWindow(frame, 40, 20, 1000, 800);
    await client.waitForRenderIdle();
    const add = await client.request(ControlCommand.TestFavoriteNav, ["add", 1]);
    if (add[0] !== 0) {
      throw new Error(`issue-6043: could not add favorite: ${String(add[1] ?? "")}`);
    }
    sendMessage(frame, WM_COMMAND, cmdId("CmdFavoriteToggle"), 0);

    const deadline = Date.now() + 5_000;
    let tree = 0;
    let ink: InkCount = { left: 0, right: 0, bounds: "" };
    while (Date.now() < deadline) {
      tree = findVisibleChildWindow(frame, "SysTreeView32");
      if (tree) {
        treeClearSelection(tree);
        await sleep(20);
        ink = countLabelHalves(tree);
        if (ink.left > 20 && ink.right > 20) {
          break;
        }
      }
      await sleep(40);
    }
    if (!tree) {
      throw new Error("issue-6043: Favorites tree did not appear");
    }
    captureWindowToPng(tree, join(dir, "favorites.png"));
    if (ink.left <= 20 || ink.right <= 20) {
      throw new Error(`issue-6043: expected two copies of the filename (${JSON.stringify(ink)})`);
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
  await testTheme("Light");
  await testTheme("Dark");
}

if (import.meta.main) {
  await runStandalone(testit);
}
