// Right click on an image / Selected Image / Copy To Clipboard must put an
// image another app can paste (issue #6360). The clipboard listed CF_DIB, but
// the bitmap behind it was a DIB section of our process, so a paste in Paint
// got nothing. The test reads CF_DIB from this process, as a pasting app does.
//
// Run: bun tests/issue-6360.ts [--no-build] [-ng]

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath, USE_NG } from "./util.ts";
import {
  clientToScreen,
  emptyClipboard,
  getClientRect,
  getClipboardDibInfo,
  postMessage,
  sleep,
  VK_ESCAPE,
  WM_CHAR,
} from "./winapi.ts";
import {
  findCanvas,
  killAndWait,
  launchControlled,
  ngKey,
  openContextMenu,
  waitForContextMenu,
} from "./win-automation.ts";

const IMG_DX = 4;
const IMG_DY = 4;

// one page that is mostly one 4x4 image of red and blue pixels
function makePdf(): string {
  let hex = "";
  for (let i = 0; i < IMG_DX * IMG_DY; i++) {
    hex += i % 2 ? "ff0000" : "0000ff";
  }
  const content = "q 500 0 0 700 56 46 cm /Im0 Do Q";
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /XObject << /Im0 5 0 R >> >> /Contents 4 0 R >>",
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
    `<< /Type /XObject /Subtype /Image /Width ${IMG_DX} /Height ${IMG_DY} /ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /ASCIIHexDecode /Length ${hex.length + 1} >>\nstream\n${hex}>\nendstream`,
  ]);
}

async function uiState(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
}

// right click the middle of the page, which is the image, and pick Copy
async function copyImageFromMenu(client: ControlClient, frame: number): Promise<void> {
  if (USE_NG) {
    const m = /canvas=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(await uiState(client));
    if (!m) {
      throw new Error("issue-6360: no canvas rect");
    }
    const x = +m[1]! + Math.floor(+m[3]! / 2);
    const y = +m[2]! + Math.floor(+m[4]! / 2);
    await client.request(ControlCommand.TestInput, ["click", x, y, 1, 0]);
    const deadline = Date.now() + 3000;
    while (!/ popup=1/.test(await uiState(client))) {
      if (Date.now() > deadline) {
        throw new Error("issue-6360: context menu did not open");
      }
      await sleep(40);
    }
    // ng has no menu window to post the mnemonics to; this is the command the item runs
    await client.request(ControlCommand.TestUiState, ["ctxcmd", cmdId("CmdCopyImage")]);
    await ngKey(client, VK_ESCAPE);
    return;
  }
  const canvas = findCanvas(frame);
  const cr = getClientRect(canvas);
  const pt = clientToScreen(canvas, Math.floor(cr.right / 2), Math.floor(cr.bottom / 2));
  openContextMenu(canvas, pt.x, pt.y);
  const popup = await waitForContextMenu(3000);
  if (!popup) {
    throw new Error("issue-6360: context menu did not open");
  }
  // "Selected &Image", then "C&opy To Clipboard"
  postMessage(popup, WM_CHAR, "i".charCodeAt(0), 0);
  await sleep(150);
  postMessage(popup, WM_CHAR, "o".charCodeAt(0), 0);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6360.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  emptyClipboard();

  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await copyImageFromMenu(client, frame);

    const deadline = Date.now() + 3000;
    let dib = getClipboardDibInfo();
    while (!dib && Date.now() < deadline) {
      await sleep(100);
      dib = getClipboardDibInfo();
    }
    console.log(`issue-6360: CF_DIB ${JSON.stringify(dib)}`);
    if (!dib) {
      throw new Error("issue-6360: no CF_DIB another app can paste");
    }
    if (dib.width !== IMG_DX || dib.height !== IMG_DY || dib.nonBlack !== IMG_DX * IMG_DY) {
      throw new Error(`issue-6360: pasted image is not the ${IMG_DX}x${IMG_DY} image: ${JSON.stringify(dib)}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
