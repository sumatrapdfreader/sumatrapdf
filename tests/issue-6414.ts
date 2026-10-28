// A PDF's home page thumbnail is page 1 even when an image with the same base
// name sits next to it ("a.pdf" + "a.png" plot); sibling covers are for ebooks
// (issue #6414).
import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { centerPixel, thumbnailPath, waitForFile } from "./issue-6236.ts";
import { makePng, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const SRC_PDF = join(import.meta.dir, "issue-3219.pdf");
const COVER_JPG = join(import.meta.dir, "issue-6236.jpg");

export async function testit(): Promise<void> {
  const root = tmpPath("issue-6414");
  rmSync(root, { recursive: true, force: true });
  const appData = join(root, "appdata");
  mkdirSync(appData, { recursive: true });
  writeFileSync(
    join(appData, "SumatraPDF-settings.txt"),
    ["UiLanguage = en", "CheckForUpdates = false", "RememberOpenedFiles = true", "ShowStartPage = true", ""].join("\n"),
  );

  // red images next to PDFs of the same base name
  const withPng = join(root, "a.pdf");
  copyFileSync(SRC_PDF, withPng);
  writeFileSync(join(root, "a.png"), makePng(200, 300, [220, 30, 30]));
  const withJpg = join(root, "b.pdf");
  copyFileSync(SRC_PDF, withJpg);
  copyFileSync(COVER_JPG, join(root, "b.jpg"));

  const { proc, client } = await launchControlled(["-appdata", appData, withPng, withJpg]);
  try {
    await client.waitForRenderIdle();
    await waitForFile(thumbnailPath(appData, withPng), "a.pdf");
    await waitForFile(thumbnailPath(appData, withJpg), "b.pdf");
  } finally {
    client.close();
    await killAndWait(proc);
  }

  for (const pdf of [withPng, withJpg]) {
    const [r, g, b] = centerPixel(thumbnailPath(appData, pdf));
    if (r < 200 || g < 200 || b < 200) {
      throw new Error(`issue-6414: ${pdf} thumbnail should be page 1, not the sibling image: rgb(${r},${g},${b})`);
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
