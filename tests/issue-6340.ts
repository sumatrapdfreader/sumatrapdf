// Issue #6340: showing a long note made the Find Annotation window's minimum
// width the note laid out on one line, so a resize or a reopen stretched it
// and it could not be made smaller until the note was hidden.
//
// Run: bun tests/issue-6340.ts [--no-build] [-ng]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { assemblePdf, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util.ts";
import {
  findTopWindow,
  getWindowRect,
  MK_LBUTTON,
  setWindowPos,
  sleep,
  SWP_NOACTIVATE,
  SWP_NOZORDER,
} from "./winapi.ts";
import { clickAt, launchControlled } from "./win-automation.ts";

const FLOAT_CLASS = "SUMATRA_ANNOT_FILTER_WND";
const NOTE = "The reviewer asked for a shorter sentence here. ".repeat(20);

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    `<< /Type /Annot /Subtype /Text /P 3 0 R /Rect [72 700 92 720] /T (kjk) /Contents (${NOTE}) >>`,
  ]);
}

type State = { visible: boolean; sel: number; note: number; dx: number; itemDy: number; listY: number };

async function state(client: ControlClient): Promise<State> {
  const res = await client.request(ControlCommand.TestAnnotFilter, ["paint", 0, 0]);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0) {
    throw new Error(`issue-6340: TestAnnotFilter: ${raw.trim()}`);
  }
  const rc = /floatRect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
  return {
    visible: /floatVisible=1/.test(raw),
    sel: +(/ sel=(-?\d+)/.exec(raw)?.[1] ?? -1),
    note: +(/^note=(\d)/m.exec(raw)?.[1] ?? -1),
    dx: rc ? Number(rc[3]) : 0,
    itemDy: +(/itemDy=(\d+)/.exec(raw)?.[1] ?? 0),
    listY: +(/listY=(\d+)/.exec(raw)?.[1] ?? 0),
  };
}

async function waitFor(client: ControlClient, what: string, pred: (s: State) => boolean): Promise<State> {
  const deadline = Date.now() + 6000 * SLOW_BUILD_FACTOR;
  let s = await state(client);
  while (!pred(s)) {
    if (Date.now() > deadline) {
      throw new Error(`issue-6340: ${what}; state ${JSON.stringify(s)}`);
    }
    await sleep(50);
    s = await state(client);
  }
  return s;
}

function assertWidth(where: string, before: number, after: number): void {
  // a one-line note is thousands of pixels; the window's own minimum is not
  if (after > before + 48) {
    throw new Error(`issue-6340: ${where} widened the window from ${before} to ${after}`);
  }
}

async function selectFirstRow(client: ControlClient, pid: number): Promise<void> {
  if (USE_NG) {
    await client.request(ControlCommand.TestAnnotFilter, ["click", 0, 0]);
    return;
  }
  const s = await waitFor(client, "the list never laid out", (st) => st.itemDy > 0);
  const hwnd = findTopWindow(pid, FLOAT_CLASS);
  if (!hwnd) {
    throw new Error("issue-6340: no Annotations window");
  }
  await clickAt(hwnd, 24, s.listY + Math.floor(s.itemDy / 2), 200, MK_LBUTTON);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6340");
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\n",
  );
  const pdf = join(dir, "annots.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const { proc, client } = await launchControlled(
    ["-appdata", appdata, "-view", "single page", "-zoom", "fit page", pdf],
    { saveSettings: true },
  );
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdToggleEditPDF"]);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdFindAnnotation"]);
    const opened = await waitFor(client, "the Annotations window never opened", (s) => s.visible && s.dx > 0);
    const before = opened.dx;

    await selectFirstRow(client, proc.pid!);
    await waitFor(client, "the first row was not selected", (s) => s.sel === 0);
    await client.request(ControlCommand.TestAnnotFilter, ["note", 0, 0]);
    await waitFor(client, "the note did not appear", (s) => s.note === 1);

    if (!USE_NG) {
      const hwnd = findTopWindow(proc.pid!, FLOAT_CLASS);
      const rc = getWindowRect(hwnd);
      const dx = rc.right - rc.left;
      const dy = rc.bottom - rc.top;
      setWindowPos(hwnd, rc.left, rc.top, Math.max(dx - 80, 200), dy, SWP_NOZORDER | SWP_NOACTIVATE);
      await sleep(100);
      const shrunk = getWindowRect(hwnd);
      assertWidth("shrinking", before, shrunk.right - shrunk.left);
    }

    await client.request(ControlCommand.TestInvokeCommand, ["CmdFindAnnotation"]);
    await waitFor(client, "the window did not close", (s) => !s.visible);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdFindAnnotation"]);
    const again = await waitFor(client, "the window did not reopen", (s) => s.visible && s.dx > 0);
    assertWidth("reopening", before, again.dx);
    console.log("issue-6340: OK");
  } finally {
    client.close();
    proc.kill();
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
