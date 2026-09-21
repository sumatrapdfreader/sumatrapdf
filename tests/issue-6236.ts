// A sibling image with the book's base name (Calibre's "Title.epub" +
// "Title.jpg") is the home page thumbnail instead of page 1 (issue #6236).
// issue-6236.jpg is a solid red 200x300 JPEG:
//   python -c "from PIL import Image; Image.new('RGB',(200,300),(220,30,30)).save('tests/issue-6236.jpg',quality=90)"
import { copyFileSync, existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { loadPng, pngPixel, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const SRC_PDF = join(import.meta.dir, "issue-3219.pdf");
const COVER = join(import.meta.dir, "issue-6236.jpg");

// mirrors GetThumbnailPathTemp: md5 of the path, lowercase hex, .png
function thumbnailPath(appData: string, docPath: string): string {
  const hash = new Bun.CryptoHasher("md5").update(docPath).digest("hex");
  return join(appData, "sumatrapdfcache", `${hash}.png`);
}

async function waitForFile(path: string, what: string): Promise<void> {
  const deadline = Date.now() + 15_000;
  while (Date.now() < deadline) {
    if (existsSync(path)) {
      // the app writes the png in one go, but give it a moment to close it
      await sleep(200);
      return;
    }
    await sleep(100);
  }
  throw new Error(`issue-6236: ${what}: thumbnail not written: ${path}`);
}

function centerPixel(path: string): [number, number, number] {
  const img = loadPng(path);
  return pngPixel(img, Math.floor(img.w / 2), Math.floor(img.h / 2));
}

export async function testit(): Promise<void> {
  const root = tmpPath("issue-6236");
  rmSync(root, { recursive: true, force: true });
  const appData = join(root, "appdata");
  mkdirSync(appData, { recursive: true });
  writeFileSync(
    join(appData, "SumatraPDF-settings.txt"),
    ["UiLanguage = en", "CheckForUpdates = false", "RememberOpenedFiles = true", "ShowStartPage = true", ""].join("\n"),
  );
  const withCover = join(root, "book.pdf");
  const plain = join(root, "plain.pdf");
  copyFileSync(SRC_PDF, withCover);
  copyFileSync(COVER, join(root, "book.jpg"));
  copyFileSync(SRC_PDF, plain);

  const { proc, client } = await launchControlled(["-appdata", appData, withCover, plain]);
  try {
    await client.waitForRenderIdle();
    await waitForFile(thumbnailPath(appData, withCover), "book.pdf");
    await waitForFile(thumbnailPath(appData, plain), "plain.pdf");
  } finally {
    client.close();
    await killAndWait(proc);
  }

  const [r, g, b] = centerPixel(thumbnailPath(appData, withCover));
  if (r < 180 || g > 80 || b > 80) {
    throw new Error(`issue-6236: thumbnail should be the red cover, center pixel is rgb(${r},${g},${b})`);
  }
  const [r2, g2, b2] = centerPixel(thumbnailPath(appData, plain));
  if (r2 < 200 || g2 < 200 || b2 < 200) {
    throw new Error(`issue-6236: thumbnail without a cover should be page 1, center pixel is rgb(${r2},${g2},${b2})`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
