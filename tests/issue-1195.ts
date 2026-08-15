// Test for issue #1195: "maximum zoom level still too low for big pdf maps".
//
// The largest zoom was 6400%, which is plenty for reading but not for looking
// closely at a large map. Levels above 6400 can now be listed in the ZoomLevels
// advanced setting, and the largest one is the new limit, up to 1000000%. How
// far a given document can go is also limited by its own size, since the canvas
// all of its pages are laid out on is measured in int pixels.
//
// The test drives the zoom from the command line (-zoom) and reads back the
// zoom SumatraPDF saved for the file, and it measures a small black square on
// the page to check that the zoom is real and not just a remembered number.

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, tmpPath } from "./util";
import { captureWindowPixels, sendMessage, waitForWindowIdle, WM_COMMAND } from "./winapi";
import { findCanvas, launchControlled, sendCommand, waitForExit, killAndWait } from "./win-automation";

// white pages, with a small black square in the top-left corner of page 1
function makePdf(nPages: number, square: number): string {
  const first = `1 1 1 rg 0 0 612 792 re f 0 0 0 rg 0 ${792 - square} ${square} ${square} re f`;
  const rest = `1 1 1 rg 0 0 612 792 re f`;
  const kids: string[] = [];
  for (let i = 0; i < nPages; i++) {
    kids.push(`${3 + i * 2} 0 R`);
  }
  const objs = [`<< /Type /Catalog /Pages 2 0 R >>`, `<< /Type /Pages /Count ${nPages} /Kids [${kids.join(" ")}] >>`];
  for (let i = 0; i < nPages; i++) {
    const stream = i === 0 ? first : rest;
    objs.push(`<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents ${4 + i * 2} 0 R >>`);
    objs.push(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`);
  }
  let body = "%PDF-1.4\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets.push(body.length);
    body += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefStart = body.length;
  const size = objs.length + 1;
  body += `xref\n0 ${size}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    body += off.toString().padStart(10, "0") + " 00000 n \n";
  }
  body += `trailer\n<< /Size ${size} /Root 1 0 R >>\nstartxref\n${xrefStart}\n%%EOF\n`;
  return body;
}

type Result = { zoom: number; dark: number };

// the built-in zoom levels written out, which docs/md/Scrolling-and-zooming.md
// gives users as the ZoomLevels line to copy and edit
const DEFAULT_LEVELS =
  "ZoomLevels = 8.33 12.5 18 25 33.33 50 66.67 75 100 125 150 200 300 400 600 800 1000 " +
  "1200 1600 2000 2400 3200 4800 6400";

// ZoomLevels replaces the built-in levels, so a custom list has to have the
// ordinary ones too; the largest is what raises the limit
const LEVELS_50000 = `${DEFAULT_LEVELS} 12800 25600 50000`;

// open the document at the given zoom, optionally zoom in some more, then quit
// and read back the zoom that was saved for the file
async function openAtZoom(
  pdf: string,
  zoomLevels: string,
  zoom: string,
  nZoomIn = 0,
  needPixels = false,
): Promise<Result> {
  const appdata = tmpPath("issue-1195-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  const settingsPath = join(appdata, "SumatraPDF-settings.txt");
  const settings = ["RestoreSession = false", "ReuseInstance = false", "CheckForUpdates = false", zoomLevels, ""];
  writeFileSync(settingsPath, settings.join("\n"));

  // no -for-testing: this test reads the zoom the app writes into settings.
  // -dbg-control so we can wait for the real tiles, not the blurry preview.
  const { proc, client, frame } = await launchControlled(["-appdata", appdata, "-zoom", zoom, "-scroll", "0,0", pdf], {
    saveSettings: true,
  });
  try {
    await client.waitForRenderIdle(30000);
    const canvas = findCanvas(frame);
    for (let i = 0; i < nZoomIn; i++) {
      // SendMessage so the zoom is applied before we ask whether tiles are ready
      sendMessage(frame, WM_COMMAND, cmdId("CmdZoomIn"), 0);
      await client.waitForRenderIdle(30000);
    }
    // only the dark-pixel comparison needs the second, sharper tile pass.
    // at high zoom that pass arrives ~2s after the first cached tiles
    if (needPixels && canvas) {
      await waitForWindowIdle(canvas, 12000, 2200);
    }

    const px = needPixels ? captureWindowPixels(canvas) : null;
    let dark = 0;
    if (px) {
      for (let i = 0; i < px.data.length; i += 4) {
        if (px.data[i] < 128 && px.data[i + 1] < 128 && px.data[i + 2] < 128) {
          dark++;
        }
      }
    }

    sendCommand(frame, cmdId("CmdExit"));
    // the settings file is complete once the process is gone
    if (!(await waitForExit(proc))) {
      throw new Error("SumatraPDF didn't exit after CmdExit");
    }
    const txt = readFileSync(settingsPath, "utf8");
    const m = txt.split("FileStates")[1]?.match(/\bZoom = ([^\r\n]*)/);
    if (!m) {
      throw new Error("SumatraPDF didn't save a zoom level for the file");
    }
    return { zoom: parseFloat(m[1]), dark };
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const onePage = tmpPath("issue-1195.pdf");
  writeFileSync(onePage, makePdf(1, 0.5), "latin1");
  const longDoc = tmpPath("issue-1195-long.pdf");
  writeFileSync(longDoc, makePdf(400, 0.5), "latin1");

  // without custom zoom levels, 6400% is still the limit
  const std = await openAtZoom(onePage, "", "25600", 0, true);
  if (Math.round(std.zoom) !== 6400) {
    throw new Error(`expected the zoom to be capped at 6400% by default, got ${std.zoom}%`);
  }

  // the levels documented as "the built-in ones, written out" must really be them:
  // zooming in from 3200% has to land on the same level either way
  const builtinStep = await openAtZoom(onePage, "", "3200", 1);
  const documentedStep = await openAtZoom(onePage, DEFAULT_LEVELS, "3200", 1);
  if (Math.round(builtinStep.zoom) !== 4800 || documentedStep.zoom !== builtinStep.zoom) {
    throw new Error(
      `the documented default ZoomLevels don't reproduce the built-in ones: zooming in from ` +
        `3200% goes to ${builtinStep.zoom}% by default but to ${documentedStep.zoom}% with them`,
    );
  }

  // listing higher levels raises it - and the page really is four times as large
  const raised = await openAtZoom(onePage, LEVELS_50000, "25600", 0, true);
  if (Math.round(raised.zoom) !== 25600) {
    throw new Error(`expected ZoomLevels up to 50000 to allow 25600%, got ${raised.zoom}%`);
  }
  const ratio = raised.dark / Math.max(1, std.dark);
  if (std.dark < 500 || ratio < 10 || ratio > 24) {
    throw new Error(
      `zooming from 6400% to 25600% should make the square ~16x bigger, ` +
        `but it went from ${std.dark} to ${raised.dark} pixels`,
    );
  }

  // zooming in steps through the levels above 6400% like any other
  const steps = await openAtZoom(onePage, LEVELS_50000, "6400", 2);
  if (Math.round(steps.zoom) !== 25600) {
    throw new Error(`expected two zoom-in steps from 6400% to reach 25600%, got ${steps.zoom}%`);
  }

  // levels beyond what a page can be laid out at are dropped, so 9000000 in the
  // list means the maximum is the 1000000 before it, not 9000000
  const ceiling = await openAtZoom(onePage, `${LEVELS_50000} 1000000 9000000`, "9000000");
  if (Math.round(ceiling.zoom) !== 1000000) {
    throw new Error(`expected zoom levels above 1000000% to be ignored, got ${ceiling.zoom}%`);
  }

  // a 400 page document can't be laid out at 1000000%: its canvas wouldn't fit
  // in an int, so the zoom stops at what the document can hold
  const long = await openAtZoom(longDoc, `${LEVELS_50000} 1000000`, "1000000");
  if (!(long.zoom > 6400 && long.zoom < 1000000)) {
    throw new Error(`expected a 400 page document to cap below 1000000%, got ${long.zoom}%`);
  }

  console.log(
    `  default limit ${std.zoom}%, with custom ZoomLevels ${raised.zoom}% (${ratio.toFixed(1)}x the pixels) ✓`,
  );
  console.log(`  the documented default ZoomLevels step like the built-in ones (3200% -> ${documentedStep.zoom}%) ✓`);
  console.log(`  zoom in steps to ${steps.zoom}%, the ceiling holds at ${ceiling.zoom}% ✓`);
  console.log(`  a 400 page document stops at ${long.zoom}%, as much as its canvas can hold ✓`);
}

if (import.meta.main) {
  const { runStandalone } = await import("./util");
  await runStandalone(testit);
}
