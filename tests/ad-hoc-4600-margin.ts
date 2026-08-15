// Ad-hoc: EBookUI.Margin (#4600).
//
// mupdf's html_default_css starts with "@page{margin:3em 2em}", and
// fz_layout_html turns the root box's margins (matched from @page rules by
// fz_match_css_at_page) into html->page_margin[], which it subtracts from the
// page it lays the text into. So the setting is applied as a user-stylesheet
// @page rule, in points like LayoutDx.
//
// Checks the three forms it takes -- one value for all four sides, two for
// top/bottom and left/right, four in CSS order -- plus 0 (a real value: no
// margin at all) and a per-document override.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { tmpPath } from "./util.ts";
import { makeFixtures } from "./ad-hoc-issue-4600.ts";
import { findCanvas, launchSumatra, waitForFrame, killAndWait, killProcessesNamed } from "./win-automation.ts";
import { captureWindowPixels, moveWindow, setForegroundWindow, showWindow, sleep, SW_RESTORE } from "./winapi.ts";

type Cap = { w: number; h: number; data: Uint8Array };
type Box = { x0: number; y0: number; x1: number; y1: number };

async function render(settings: string[]): Promise<Cap> {
  const appdata = tmpPath("issue-4600-margin-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), [...settings, `RestoreSession = false`, ``].join("\n"));
  const epub = join(tmpPath("epub-font"), "none.epub");
  const proc = launchSumatra(["-appdata", appdata, "-view", "single page", "-zoom", "fit page", epub]);
  try {
    const frame = await waitForFrame(proc.pid!);
    showWindow(frame, SW_RESTORE);
    moveWindow(frame, 40, 40, 1000, 800);
    setForegroundWindow(frame);
    await sleep(2500);
    const cap = captureWindowPixels(findCanvas(frame));
    if (!cap) {
      throw new Error("no capture");
    }
    return cap;
  } finally {
    await killAndWait(proc);
  }
}

// bounding box of the text (dark pixels) on the canvas
function inkBox(cap: Cap): Box {
  let x0 = cap.w;
  let y0 = cap.h;
  let x1 = -1;
  let y1 = -1;
  for (let y = 0; y < cap.h; y++) {
    for (let x = 0; x < cap.w; x++) {
      const o = (y * cap.w + x) * 4;
      if (cap.data[o]! + cap.data[o + 1]! + cap.data[o + 2]! < 400) {
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (y < y0) y0 = y;
        if (y > y1) y1 = y;
      }
    }
  }
  return { x0, y0, x1, y1 };
}

function ebookUI(lines: string[]): string[] {
  return [`EBookUI [`, ...lines.map((l) => `\t${l}`), `]`];
}

async function box(margin: string | null): Promise<Box> {
  return inkBox(await render(ebookUI(margin === null ? [] : [`Margin = ${margin}`])));
}

function show(name: string, b: Box): void {
  console.log(`  ${name.padEnd(22)} x ${b.x0}..${b.x1}  y ${b.y0}..${b.y1}`);
}

export async function testit(): Promise<void> {
  makeFixtures();
  await killProcessesNamed("SumatraPDF.exe");
  const epub = join(tmpPath("epub-font"), "none.epub");

  const base = await box(null);
  const one = await box("60");
  const none = await box("0");
  show("default (3em/2em)", base);
  show("Margin = 60", one);
  show("Margin = 0", none);
  if (one.x0 <= base.x0 || one.y0 <= base.y0) {
    throw new Error("one value didn't push the text inwards on all sides");
  }
  if (none.x0 >= base.x0 || none.y0 >= base.y0) {
    throw new Error("Margin = 0 didn't remove the default margin");
  }

  // two values are vertical then horizontal, four are top right bottom left,
  // so both of these have to land on exactly the same box as "60"
  const two = await box("60 60");
  const four = await box("60 60 60 60");
  show("Margin = 60 60", two);
  show("Margin = 60 60 60 60", four);
  if (two.x0 !== one.x0 || two.y0 !== one.y0 || four.x0 !== one.x0 || four.y0 !== one.y0) {
    throw new Error("the 2- and 4-value forms don't match the 1-value form");
  }

  // and the two forms have to be able to differ per axis, in CSS order
  const vh = await box("20 80"); // 20 above and below, 80 left and right
  show("Margin = 20 80", vh);
  if (!(vh.y0 < one.y0 && vh.x0 > one.x0)) {
    throw new Error("two values aren't vertical-then-horizontal");
  }
  const trbl = await box("20 80 20 80");
  show("Margin = 20 80 20 80", trbl);
  if (trbl.x0 !== vh.x0 || trbl.y0 !== vh.y0) {
    throw new Error("the 4-value form isn't top right bottom left");
  }

  // a per-document margin overrides the global one
  const perFile = inkBox(
    await render([
      ...ebookUI([`Margin = 60`]),
      `FileStates [`,
      `\t[`,
      `\t\tFilePath = ${epub}`,
      `\t\tEBookUI [`,
      `\t\t\tMargin = 0`,
      `\t\t]`,
      `\t]`,
      `]`,
    ]),
  );
  if (perFile.x0 !== none.x0 || perFile.y0 !== none.y0) {
    throw new Error(`the per-document margin didn't win: x0=${perFile.x0} y0=${perFile.y0}`);
  }
  console.log("  1 / 2 / 4 values behave like CSS, and a per-document Margin wins ✓");
}

if (import.meta.main) {
  const { runStandalone } = await import("./util.ts");
  await runStandalone(testit);
}
