// Test for issue #3585: manually set the part of the pages that is shown.
//
// Trim Pages To Selection shows on every page only what the selection covers;
// Show Whole Pages undoes it. The page is four colour quadrants with a line of
// text in the top-left (red) one: after trimming to the selected text the page
// is laid out as a wide, short strip and only red (and the text) is visible.

import { writeFileSync } from "node:fs";
import { ControlCommand, type ControlClient } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { cmdId, tmpPath, assemblePdf, runStandalone } from "./util";
import { findCanvas, launchControlled, killAndWait } from "./win-automation";
import { captureWindowPixels, sendMessage, WM_COMMAND } from "./winapi";

const PAGE_DX = 612;
const PAGE_DY = 792;

function makeQuadrantPdf(): string {
  const stream =
    [
      "1 0 0 rg 0 396 306 396 re f", // top-left red
      "0 0.6 0 rg 306 396 306 396 re f", // top-right green
      "0 0 1 rg 0 0 306 396 re f", // bottom-left blue
      "1 1 0 rg 306 0 306 396 re f", // bottom-right yellow
    ].join("\n") + `\nBT /F1 16 Tf 0 0 0 rg 40 690 Td (TrimHereTrimHere) Tj ET`;
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Count 1 /Kids [3 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ${PAGE_DX} ${PAGE_DY}] /Contents 4 0 R ` +
      `/Resources << /Font << /F1 5 0 R >> >> >>`,
    `<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`,
    `<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>`,
  ];
  return assemblePdf(objs);
}

// width / height of page 1 as laid out
async function pageAspect(client: ControlClient): Promise<number> {
  const raw = String((await client.request(ControlCommand.TestLayout, []))[1] ?? "");
  const page = /page n=1 shown=1 pos=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
  if (!page) {
    throw new Error(`issue-3585: layout missing page 1: ${raw}`);
  }
  return Number(page[3]) / Number(page[4]);
}

// number of canvas pixels of the quadrants other than red
function nonRedQuadrantPixels(canvas: number): { red: number; other: number } {
  const cap = captureWindowPixels(canvas);
  if (!cap) {
    throw new Error("could not capture the canvas");
  }
  const { data } = cap;
  let red = 0;
  let other = 0;
  for (let i = 0; i < data.length; i += 4) {
    const b = data[i]!;
    const g = data[i + 1]!;
    const r = data[i + 2]!;
    if (r > 150 && g < 100 && b < 100) {
      red++;
    } else if ((g > 100 && r < 100 && b < 100) || (b > 150 && r < 100 && g < 100) || (r > 150 && g > 150 && b < 100)) {
      other++;
    }
  }
  return { red, other };
}

function sendCmd(frame: number, name: string): void {
  sendMessage(frame, WM_COMMAND, BigInt(cmdId(name)), 0n);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-3585.pdf");
  writeFileSync(pdf, makeQuadrantPdf(), "latin1");

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.setNotificationsEnabled(false);
    sendCmd(frame, "CmdZoomFitPage");
    await client.waitForRenderIdle();
    const whole = await pageAspect(client);
    const pageAspectExpected = PAGE_DX / PAGE_DY;
    if (Math.abs(whole - pageAspectExpected) > 0.05) {
      throw new Error(`untrimmed page aspect ${whole}, expected ${pageAspectExpected}`);
    }

    // select the text line: a few words via keyboard selection
    sendCmd(frame, "CmdSelectTextViaKeyboard");
    for (let i = 0; i < 3; i++) {
      sendCmd(frame, "CmdExtendSelectionWordRight");
    }
    sendCmd(frame, "CmdTrimPagesToSelection");
    sendCmd(frame, "CmdZoomFitPage");
    await client.waitForRenderIdle();

    const trimmed = await pageAspect(client);
    if (trimmed < 3) {
      throw new Error(`Trim Pages To Selection should lay out a wide strip, got aspect ${trimmed}`);
    }
    if (!IS_MAC) {
      const canvas = findCanvas(frame);
      if (!canvas) {
        throw new Error("could not find the canvas window");
      }
      const px = nonRedQuadrantPixels(canvas);
      if (px.red < 1000 || px.other > 0) {
        throw new Error(`trimmed page should show only the red quadrant, got ${JSON.stringify(px)}`);
      }
    }

    sendCmd(frame, "CmdShowWholePages");
    sendCmd(frame, "CmdZoomFitPage");
    await client.waitForRenderIdle();
    const restored = await pageAspect(client);
    if (Math.abs(restored - pageAspectExpected) > 0.05) {
      throw new Error(`Show Whole Pages should restore aspect ${pageAspectExpected}, got ${restored}`);
    }
    console.log(`  aspect: whole ${whole.toFixed(2)}, trimmed ${trimmed.toFixed(2)}, restored ${restored.toFixed(2)}`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
