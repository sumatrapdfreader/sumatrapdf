// Cached Home thumbnails keep their aspect ratio when loaded after layout.
import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { waitForHome } from "./control.ts";
import { ROOT, makePng, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { findCanvas, killAndWait, launchControlled } from "./win-automation.ts";
import { repaintWindow, setCursorPos } from "./winapi.ts";

const FRAME = { width: 150, height: 212 };
const SIZES = [
  { width: 150, height: 50 },
  { width: 50, height: 212 },
  { width: 150, height: 12 },
];

export async function testit(): Promise<void> {
  if (USE_NG) {
    console.log("SKIP issue-6352: Windows loads cached thumbnails during paint");
    return;
  }
  setCursorPos(0, 0);
  for (const size of SIZES) {
    const appdata = tmpPath(`issue-6352-${size.width}x${size.height}`);
    const cache = join(appdata, "sumatrapdfcache");
    rmSync(appdata, { recursive: true, force: true });
    mkdirSync(cache, { recursive: true });
    const pdf = join(appdata, "document.pdf");
    copyFileSync(join(ROOT, "ext", "a-zlib", "zlib.3.pdf"), pdf);
    const hash = new Bun.CryptoHasher("md5").update(pdf).digest("hex");
    const png = join(cache, `${hash}.png`);
    writeFileSync(
      join(appdata, "SumatraPDF-settings.txt"),
      `UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nShowStartPage = true\n` +
        `RememberOpenedFiles = true\nShowTips = false\nThumbnailsType = full\nHomePageViewMode = thumbnails\n` +
        `FileStates [\n [\n FilePath = ${pdf}\n OpenCount = 1\n ]\n]\n`,
    );
    const { proc, client, frame } = await launchControlled(["-appdata", appdata, "-window-pos", "820x600@20x20"]);
    let expected: number[];
    try {
      const blank = await waitForHome(client, (h) => h.entries === 1 && h.outlineFull[3]! > 0, "home not ready");
      const [x, y, width, height] = blank.outlineFull as [number, number, number, number];
      const dx = FRAME.width - size.width;
      const dy = FRAME.height - size.height;
      expected = [x + Math.floor(dx / 2), y + dy, width - dx, height - dy];
      // Make the cached image available after the placeholder has been laid out.
      writeFileSync(png, makePng(size.width, size.height, [220, 30, 30]));
      repaintWindow(findCanvas(frame));
      await waitForHome(
        client,
        (h) => h.outlineFull.every((v, i) => v === expected[i]),
        `issue-6352: ${size.width}x${size.height} thumbnail geometry was not updated; expected ${expected}`,
      );
    } finally {
      client.close();
      await killAndWait(proc);
    }
    const restarted = await launchControlled(["-appdata", appdata, "-window-pos", "820x600@20x20"]);
    try {
      await waitForHome(
        restarted.client,
        (h) => h.entries === 1 && h.outlineFull.every((v, i) => v === expected[i]),
        "issue-6352: cached thumbnail stretched after restart",
      );
    } finally {
      restarted.client.close();
      await killAndWait(restarted.proc);
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
