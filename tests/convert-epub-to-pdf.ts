// Convert EPUB to PDF (CmdConvertEpubToPdf): offered for EPUB only, lays the
// book out at the chosen page and font size, keeps the text, and the dialog
// writes the PDF and opens it.
//
// Run: bun tests/convert-epub-to-pdf.ts [--no-build] [-ng]

import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { makeEpub } from "./epub-relayout-stale-page.ts";
import { cmdId, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { enumWindows, getWindowPid, getWindowText, isWindowVisible, sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const kTitle = "Convert EPUB to PDF";
const kAction = "Convert to PDF";
const kA4 = 1;
const kNgWindow = "convertepubtopdf";

async function visibility(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestCommandVisibility, ["CmdConvertEpubToPdf", "menu"]);
  return String(res[1] ?? "");
}

async function convert(client: ControlClient, dest: string, pageSizeIdx: number, fontSize: number): Promise<number> {
  const res = await client.request(ControlCommand.TestConvertEpubToPdf, [dest, pageSizeIdx, fontSize]);
  const m = /^OK pages=(\d+)/.exec(String(res[1] ?? ""));
  if (res[0] !== 0 || !m) {
    throw new Error(`convert-epub-to-pdf: conversion failed: ${String(res[1] ?? "")}`);
  }
  return +m[1]!;
}

async function waitFor(what: string, pred: () => Promise<boolean>): Promise<void> {
  const deadline = Date.now() + 10000;
  while (!(await pred())) {
    if (Date.now() > deadline) {
      throw new Error(`convert-epub-to-pdf: ${what}`);
    }
    await sleep(50);
  }
}

// opens the dialog and clicks its action button, as a user does
async function convertWithDialog(client: ControlClient, frame: number, pid: number): Promise<void> {
  if (USE_NG) {
    await client.request(ControlCommand.TestInvokeCommand, ["CmdConvertEpubToPdf"]);
    let line = "";
    await waitFor("the dialog did not open", async () => {
      const res = await client.request(ControlCommand.TestToolWindow, ["layout", kNgWindow, "", 0, 0, 0, 0]);
      line = String(res[1] ?? "")
        .split("\n")
        .find((l) => l.startsWith("node ") && l.includes(`label='${kAction}'`) && !l.includes("disabled=1"))!;
      return !!line;
    });
    const m = /rect=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(line)!;
    const x = +m[1]! + Math.floor(+m[3]! / 2);
    const y = +m[2]! + Math.floor(+m[4]! / 2);
    await client.request(ControlCommand.TestToolWindow, ["input", kNgWindow, "click", x, y, 0, 0]);
    return;
  }
  sendCommand(frame, cmdId("CmdConvertEpubToPdf"));
  let dlg = 0;
  await waitFor("the dialog did not open", async () => {
    enumWindows((h) => {
      if (getWindowPid(h) === pid && isWindowVisible(h) && getWindowText(h) === kTitle) {
        dlg = h;
        return false;
      }
      return true;
    });
    return dlg !== 0;
  });
  const { getClassName, packCoords, postMessage, MK_LBUTTON, WM_LBUTTONDOWN, WM_LBUTTONUP, WM_MOUSEMOVE } =
    await import("./winapi.ts");
  let btn: { x: number; y: number; dx: number; dy: number } | undefined;
  await waitFor("no Convert to PDF button", async () => {
    btn = (await client.virtTree(getClassName(dlg))).find((r) => r.text === kAction)?.rect;
    return !!btn;
  });
  const lp = packCoords(btn!.x + Math.floor(btn!.dx / 2), btn!.y + Math.floor(btn!.dy / 2));
  postMessage(dlg, WM_MOUSEMOVE, 0, lp);
  postMessage(dlg, WM_LBUTTONDOWN, MK_LBUTTON, lp);
  postMessage(dlg, WM_LBUTTONUP, 0, lp);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("convert-epub-to-pdf");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const epub = join(dir, "book.epub");
  writeFileSync(epub, makeEpub({ chapterCount: 2, parasPerChapter: 6 }));

  const { proc, client, frame } = await launchControlled([epub]);
  try {
    await client.waitForRenderIdle();
    const vis = await visibility(client);
    if (!/vis=show/.test(vis)) {
      throw new Error(`convert-epub-to-pdf: the command should show for an EPUB: ${vis}`);
    }

    // A4 pages, text kept; a bigger font takes more pages
    const a4 = join(dir, "a4.pdf");
    const nSmall = await convert(client, a4, kA4, 12);
    const pdf = readFileSync(a4, "latin1");
    if (!/\/MediaBox\s*\[\s*0 0 595 842\s*\]/.test(pdf)) {
      throw new Error("convert-epub-to-pdf: pages should be A4");
    }
    const found = String((await client.request(ControlCommand.TestSearch, [a4, "chapterword"]))[1] ?? "");
    if (!found.startsWith("FOUND")) {
      throw new Error(`convert-epub-to-pdf: the PDF should keep the text: ${found}`);
    }
    const nBig = await convert(client, join(dir, "a4-big.pdf"), kA4, 24);
    if (nBig <= nSmall) {
      throw new Error(`convert-epub-to-pdf: 24 pt took ${nBig} pages, 12 pt ${nSmall}`);
    }

    // the dialog writes book.pdf and opens it, where the command is hidden
    const bookPdf = join(dir, "book.pdf");
    await convertWithDialog(client, frame, proc.pid);
    await waitFor("the dialog should write book.pdf", async () => existsSync(bookPdf));
    await waitFor("book.pdf should open", async () => /vis=hide/.test(await visibility(client)));
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("convert-epub-to-pdf: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
