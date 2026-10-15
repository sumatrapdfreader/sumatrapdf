// Issue #6337: with ContinuousScrollFollowsRotation, Rotate Right stacks the
// next page to the left and Page Down scrolls that way.
//
// Run: bun tests/issue-6337.ts [--no-build] [-ng]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

function makePdf(): string {
  // Tall pages, so after a quarter turn two of them are wider than the test window.
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 2 /Kids [3 0 R 4 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 1200 2400] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 1200 2400] >>",
  ]);
}

type Page = { n: number; x: number; y: number; dx: number; dy: number; sx: number };

function parsePages(raw: string): Page[] {
  const pages: Page[] = [];
  const re = /page n=(\d+) shown=\d+ pos=(-?\d+),(-?\d+),(-?\d+),(-?\d+) screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/g;
  for (const m of raw.matchAll(re)) {
    pages.push({
      n: +m[1]!,
      x: +m[2]!,
      y: +m[3]!,
      dx: +m[4]!,
      dy: +m[5]!,
      sx: +m[6]!,
    });
  }
  return pages;
}

function page(pages: Page[], n: number): Page {
  const found = pages.find((p) => p.n === n);
  if (!found) {
    throw new Error(`issue-6337: page ${n} missing`);
  }
  return found;
}

async function openRotated(dir: string, pdf: string, follow: boolean) {
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    [
      "ReuseInstance = false",
      "RestoreSession = false",
      "ShowStartPage = false",
      "CheckForUpdates = false",
      "RememberOpenedFiles = false",
      "DefaultDisplayMode = continuous",
      "DefaultZoom = 100%",
      "SmoothScroll = false",
      `ContinuousScrollFollowsRotation = ${follow}`,
      "",
    ].join("\n"),
  );
  const launched = await launchControlled(["-appdata", dir, pdf]);
  await launched.client.waitForRenderIdle();
  await launched.client.setNotificationsEnabled(false);
  sendCommandSync(launched.frame, cmdId("CmdZoom100"));
  await launched.client.waitForRenderIdle();
  sendCommandSync(launched.frame, cmdId("CmdRotateRight"));
  await launched.client.waitForRenderIdle();
  return launched;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6337");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "pages.pdf");
  writeFileSync(pdf, makePdf());

  const followed = await openRotated(dir, pdf, true);
  try {
    const before = parsePages((await followed.client.layout()).raw);
    const p1 = page(before, 1);
    const p2 = page(before, 2);
    if (!(p1.dx > p1.dy)) {
      throw new Error(`issue-6337: rotate right did not swap the page, dx=${p1.dx} dy=${p1.dy}`);
    }
    if (p2.x + p2.dx > p1.x) {
      throw new Error(`issue-6337: page 2 is not left of page 1 (${p2.x}+${p2.dx} vs ${p1.x})`);
    }
    if (p1.y !== p2.y) {
      throw new Error(`issue-6337: pages are not on one row (${p1.y} vs ${p2.y})`);
    }

    sendCommandSync(followed.frame, cmdId("CmdScrollDownPage"));
    await followed.client.waitForRenderIdle();
    const after = parsePages((await followed.client.layout()).raw);
    const moved = page(after, 2).sx;
    if (moved <= p2.sx) {
      throw new Error(`issue-6337: page down did not move toward page 2 (screen.x ${p2.sx} -> ${moved})`);
    }
  } finally {
    followed.client.close();
    await killAndWait(followed.proc);
  }

  const stacked = await openRotated(dir, pdf, false);
  try {
    const pages = parsePages((await stacked.client.layout()).raw);
    const p1 = page(pages, 1);
    const p2 = page(pages, 2);
    if (p2.y < p1.y + p1.dy) {
      throw new Error(`issue-6337: with the setting off, page 2 is not below page 1 (${p1.y}+${p1.dy} vs ${p2.y})`);
    }
  } finally {
    stacked.client.close();
    await killAndWait(stacked.proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
