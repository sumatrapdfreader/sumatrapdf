// Attach File on a selected file attachment annotation: the document reloads
// (file changed on disk) while the file dialog is open. The reload frees the
// annotation; attaching the picked file must not write into it.
//
// orig only: ng's file dialog doesn't pump the auto-reload.
// Run: bun tests/annot-attach-file-reload.ts [--no-build]

import { appendFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { IS_WIN } from "./host.ts";
import { assemblePdf, cmdId, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { clickAt, findCanvas, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";
import {
  enumChildWindows,
  enumWindows,
  findTopWindow,
  getClassName,
  getWindowPid,
  isWindowVisible,
  MK_LBUTTON,
  packCoords,
  postMessage,
  sendText,
  sleep,
  WM_COMMAND,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
} from "./winapi.ts";

const kTag = "annot-attach-file-reload";
const IDOK = 1;
const TOOLBAR_CLASS = "SumatraAnnotEditToolbar";

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    "<< /Type /Annot /Subtype /FileAttachment /P 3 0 R /Rect [100 600 300 700] /Name /PushPin >>",
  ]);
}

async function markupDump(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
}

async function waitForFileDialog(pid: number): Promise<number> {
  return pollUntil(
    () => {
      let dlg = 0;
      enumWindows((hwnd) => {
        if (getWindowPid(hwnd) === pid && isWindowVisible(hwnd) && getClassName(hwnd) === "#32770") {
          dlg = hwnd;
          return false;
        }
        return true;
      });
      return dlg;
    },
    (h) => h !== 0,
    { timeoutMs: 8000, error: `${kTag}: Attach File did not open a file dialog` },
  );
}

function findFileNameEdit(dlg: number): number {
  let edit = 0;
  enumChildWindows(dlg, (hwnd) => {
    if (getClassName(hwnd) !== "Edit") {
      return true;
    }
    edit = hwnd;
    return false;
  });
  return edit;
}

export async function testit(): Promise<void> {
  if (!IS_WIN || USE_NG) {
    console.log(`SKIP ${kTag}: orig Windows file dialog only`);
    return;
  }
  const dir = tmpPath(kTag);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  const attachment = join(dir, "attachment.txt");
  writeFileSync(pdf, makePdf(), "latin1");
  writeFileSync(attachment, "attached");

  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  const pid = proc.pid!;
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));

    // select the attachment, which shows its toolbar
    const m = /type=FileAttachment page=1 .*screen=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(await markupDump(client));
    if (!m) {
      throw new Error(`${kTag}: no file attachment annotation`);
    }
    await clickAt(findCanvas(frame), +m[1]! + Math.floor(+m[3]! / 2), +m[2]! + Math.floor(+m[4]! / 2), 0);
    const tbLine = await pollUntil(
      async () => /annotEditToolbar .*/.exec(await markupDump(client))?.[0] ?? "",
      (s) => /annotEditToolbar visible=1/.test(s) && /[=;]attachFile:-?\d+,-?\d+,[1-9]/.test(s),
      { error: (s) => `${kTag}: no Attach File chip: ${s}` },
    );
    const placed = / placed=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(tbLine)!;
    const chip = /[=;]attachFile:(-?\d+),(-?\d+),(\d+),(\d+)/.exec(tbLine)!;
    const tb = findTopWindow(pid, TOOLBAR_CLASS);
    if (!tb) {
      throw new Error(`${kTag}: annotation toolbar window not found`);
    }
    const x = +chip[1]! - +placed[1]! + Math.floor(+chip[3]! / 2);
    const y = +chip[2]! - +placed[2]! + Math.floor(+chip[4]! / 2);
    // posted: the click handler runs the modal dialog, a sent click would block
    postMessage(tb, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(x, y));
    postMessage(tb, WM_LBUTTONUP, 0, packCoords(x, y));
    const dlg = await waitForFileDialog(pid);

    // change the file so the document auto-reloads under the dialog
    appendFileSync(pdf, "\n");
    await sleep(2500);

    const edit = findFileNameEdit(dlg);
    if (!edit) {
      throw new Error(`${kTag}: no file name edit in the dialog`);
    }
    sendText(edit, attachment);
    postMessage(dlg, WM_COMMAND, IDOK, 0);

    await sleep(1500);
    if (proc.exitCode !== null) {
      throw new Error(`${kTag}: SumatraPDF exited (code ${proc.exitCode}) after picking the file`);
    }
    await client.request(ControlCommand.Ping, []);
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
  console.log(`${kTag}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
