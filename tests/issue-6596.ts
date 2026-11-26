// Home list reading progress must scroll with its file row.
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient, withControlledSumatra } from "./control.ts";
import { IS_WIN } from "./host.ts";
import { EXE, makeBlankPdf, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { findCanvas, waitForFrame } from "./win-automation.ts";
import { captureWindowDCRegionPixels, postMessage, raiseProcessWindows, waitForWindowIdle } from "./winapi.ts";

const WM_VSCROLL = 0x0115;
const SB_PAGEDOWN = 3;
const nFiles = 30;
const progressRow = 7;
const pageNo = 8;
const pageCount = 137;

function seedHistory(dir: string) {
  const pdf = makeBlankPdf();
  const states = [];
  for (let i = 0; i < nFiles; i++) {
    const path = join(dir, `doc-${i}.pdf`);
    writeFileSync(path, pdf, "latin1");
    const progress = i === progressRow ? `\nPageNo = ${pageNo}\nPageCount = ${pageCount}` : "";
    states.push(`[\nFilePath = ${path}\nOpenCount = ${nFiles - i}${progress}\n]`);
  }
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nRestoreSession = false\nRememberOpenedFiles = true\n" +
      "HomePageViewMode = list\nShowHomePageReadingProgress = true\nTheme = Light\n" +
      `FileStates [\n${states.join("\n")}\n]\n`,
  );
}

type SizeRect = { x: number; y: number; w: number; h: number };

async function progressRowSize(client: ControlClient): Promise<SizeRect | undefined> {
  const [, dump] = await client.request(ControlCommand.TestHomeListRows, []);
  const rows = String(dump).matchAll(/sizeRect=(-?\d+),(-?\d+),(\d+),(\d+) progress='([^']*)'/g);
  for (const [, x, y, w, h, progress] of rows) {
    if (progress === `${pageNo}/${pageCount}`) return { x: Number(x), y: Number(y), w: Number(w), h: Number(h) };
  }
}

function progressInk(canvas: number, size: SizeRect): number {
  // The equally wide progress column is just left of the size column.
  const pixels = captureWindowDCRegionPixels(
    canvas,
    size.x - size.w,
    size.y + Math.floor(size.h / 4),
    size.w - 8,
    Math.floor(size.h / 2),
  );
  if (!pixels) throw new Error("issue-6596: could not capture the progress column");
  let dark = 0;
  for (let i = 0; i < pixels.length; i += 4) {
    if (pixels[i]! < 180 && pixels[i + 1]! < 180 && pixels[i + 2]! < 180) dark++;
  }
  return dark;
}

export async function testit(): Promise<void> {
  if (!IS_WIN || USE_NG) return;

  const dir = mkdtempSync(tmpPath("issue-6596-"));
  try {
    seedHistory(dir);
    await withControlledSumatra(
      EXE,
      async (client, proc) => {
        await client.setNotificationsEnabled(false);
        const canvas = findCanvas(await waitForFrame(proc.pid));
        if (!canvas) throw new Error("issue-6596: Home canvas did not appear");
        raiseProcessWindows(proc.pid);
        const before = (await pollUntil(() => progressRowSize(client), Boolean, {
          error: "issue-6596: the seeded progress row did not appear",
        }))!;
        await waitForWindowIdle(canvas);
        if (progressInk(canvas, before) === 0) throw new Error("issue-6596: progress label was blank before scrolling");

        postMessage(canvas, WM_VSCROLL, SB_PAGEDOWN, 0);
        const after = (await pollUntil(
          () => progressRowSize(client),
          (rect) => !!rect && rect.y < before.y,
          {
            error: "issue-6596: the file row did not scroll up",
          },
        ))!;
        await waitForWindowIdle(canvas);
        const oldInk = progressInk(canvas, before);
        const newInk = progressInk(canvas, after);
        if (oldInk > 0 || newInk === 0) {
          throw new Error(
            `issue-6596: progress did not move from y=${before.y} to y=${after.y} (${oldInk}/${newInk} pixels)`,
          );
        }
      },
      ["-window-pos", "900x700@40x40", "-appdata", dir],
    );
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
