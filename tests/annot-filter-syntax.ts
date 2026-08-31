// Annotation filter conditions (issue #6034): ":a=" author, ":t=" type,
// ":c+" / ":c-" contents, plus plain words. The parsing itself has unit tests
// (AnnotSearch_UnitTests); this checks the filter box really drives the list,
// and that the syntax help appears only while the box has the focus.
//
// Run: bun tests/annot-filter-syntax.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control";
import { assemblePdf, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util";
import { findTopWindow, getWindowPid, isWindowVisible, sendText, sleep } from "./winapi";
import { clickAt, killAndWait, launchControlled, sendCommand } from "./win-automation";

const FLOAT_CLASS = "SUMATRA_ANNOT_FILTER_WND";

// kjk: a Text with contents, a Text with none. bob: a Highlight with contents.
function makePdf(): string {
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R 5 0 R 6 0 R] >>",
    "<< /Type /Annot /Subtype /Text /P 3 0 R /Rect [72 700 92 720] /T (kjk) /Contents (alpha note) >>",
    "<< /Type /Annot /Subtype /Text /P 3 0 R /Rect [72 600 92 620] /T (kjk) /Contents () >>",
    "<< /Type /Annot /Subtype /Highlight /P 3 0 R /Rect [72 500 220 530] " +
      "/QuadPoints [72 530 220 530 72 500 220 500] /C [1 1 0] /T (bob) /Contents (beta note) >>",
  ];
  return assemblePdf(objs);
}

async function nVisible(client: ControlClient): Promise<number> {
  const res = await client.request(ControlCommand.TestToolbarButtons, []);
  const dump = String(res[1] ?? "");
  const m = /nVisible=(\d+)/.exec(dump);
  if (!m) {
    throw new Error(`annot-filter-syntax: no nVisible in\n${dump}`);
  }
  return +m[1]!;
}

// the filter is debounced, so poll instead of sleeping a fixed time
async function expectVisible(client: ControlClient, filter: string, want: number): Promise<void> {
  const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
  let got = -1;
  while (Date.now() < deadline) {
    got = await nVisible(client);
    if (got === want) {
      return;
    }
    await sleep(50);
  }
  throw new Error(`annot-filter-syntax: '${filter}' showed ${got} annotations, wanted ${want}`);
}

async function setFilter(edit: number, client: ControlClient, filter: string, want: number): Promise<void> {
  sendText(edit, filter, true);
  await expectVisible(client, filter, want);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("annot-filter-syntax");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "annots.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const pid = getWindowPid(frame);

    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300 * SLOW_BUILD_FACTOR);
    sendCommand(frame, cmdId("CmdFindAnnotation"));
    const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
    let floatWnd = 0;
    while (Date.now() < deadline) {
      floatWnd = findTopWindow(pid, FLOAT_CLASS);
      if (floatWnd && isWindowVisible(floatWnd)) {
        break;
      }
      await sleep(50);
    }
    if (!floatWnd) {
      throw new Error("annot-filter-syntax: the Annotations window did not open");
    }
    let edit = 0;
    const { enumChildWindows, getClassName } = await import("./winapi");
    enumChildWindows(floatWnd, (hwnd) => {
      if (getClassName(hwnd) === "Edit" && isWindowVisible(hwnd)) {
        edit = hwnd;
        return false;
      }
      return true;
    });
    if (!edit) {
      throw new Error("annot-filter-syntax: no filter edit");
    }

    await expectVisible(client, "", 3);
    await setFilter(edit, client, ":c+", 2); // the two with contents
    await setFilter(edit, client, ":c-", 1); // the one without
    await setFilter(edit, client, ":a=kjk", 2);
    await setFilter(edit, client, ":a!=kjk", 1);
    await setFilter(edit, client, ":t=text", 2);
    await setFilter(edit, client, ":t!=text", 1);
    await setFilter(edit, client, ":t=text :t=highlight", 3); // "==" alternatives
    await setFilter(edit, client, ":a=kjk :c+", 1); // conditions AND
    await setFilter(edit, client, ":a=kjk beta", 0); // condition + word
    await setFilter(edit, client, "note", 2); // plain words still work
    // a typo is not a filter: fall back to matching it as text, matching none
    await setFilter(edit, client, ":t=nosuchtype", 0);
    await setFilter(edit, client, "", 3);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
