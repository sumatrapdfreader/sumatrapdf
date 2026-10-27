// Issue #6406: resizing the Annotations window with the note shown squeezed the
// note into a narrow column. The min-size query (WM_GETMINMAXINFO) re-wrapped
// it to the window's minimum width and nothing wrapped it back.
//
// Run: bun tests/issue-6406.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { assemblePdf, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util.ts";
import { findTopWindow, getWindowRect, MK_LBUTTON, setWindowPos, sleep } from "./winapi.ts";
import { clickAt, killAndWait, launchControlled } from "./win-automation.ts";

const FLOAT_CLASS = "SUMATRA_ANNOT_FILTER_WND";
const NOTE =
  "Replace this sentence with a shorter one. The paragraph goes on to explain why the change is needed, " +
  "and then proposes the new wording in full so the reviewer can paste it in.";

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    `<< /Type /Annot /Subtype /Text /P 3 0 R /Rect [72 700 92 720] /T (kjk) /Contents (${NOTE}) >>`,
  ]);
}

type State = {
  raw: string;
  visible: boolean;
  sel: number;
  note: number;
  wrapDx: number;
  dx: number;
  itemDy: number;
  listY: number;
};

async function state(client: ControlClient, action = "paint"): Promise<State> {
  const res = await client.request(ControlCommand.TestAnnotFilter, [action, 0, 0]);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0) {
    throw new Error(`issue-6406: TestAnnotFilter ${action}: ${raw.trim()}`);
  }
  return {
    raw,
    visible: /floatVisible=1/.test(raw),
    sel: +(/ sel=(-?\d+)/.exec(raw)?.[1] ?? -1),
    note: +(/^note=(\d)/m.exec(raw)?.[1] ?? -1),
    wrapDx: +(/noteWrapDx=(-?\d+)/.exec(raw)?.[1] ?? -1),
    dx: +(/noteDx=(-?\d+)/.exec(raw)?.[1] ?? -1),
    itemDy: +(/itemDy=(\d+)/.exec(raw)?.[1] ?? 0),
    listY: +(/listY=(\d+)/.exec(raw)?.[1] ?? 0),
  };
}

async function waitFor(client: ControlClient, what: string, pred: (s: State) => boolean): Promise<State> {
  const deadline = Date.now() + 6000 * SLOW_BUILD_FACTOR;
  let s = await state(client);
  while (!pred(s)) {
    if (Date.now() > deadline) {
      throw new Error(`issue-6406: ${what}; state ${s.raw}`);
    }
    await sleep(50);
    s = await state(client);
  }
  return s;
}

export async function testit(): Promise<void> {
  if (USE_NG) {
    console.log("issue-6406: orig only (ng's note is a gpui view)");
    return;
  }
  const dir = tmpPath("issue-6406");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "annots.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const { proc, client } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdToggleEditPDF"]);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdFindAnnotation"]);
    let s = await waitFor(client, "the Annotations window never opened", (st) => st.visible && st.itemDy > 0);
    const hwnd = findTopWindow(proc.pid!, FLOAT_CLASS);
    if (!hwnd) {
      throw new Error("issue-6406: no Annotations window");
    }
    await clickAt(hwnd, 24, s.listY + Math.floor(s.itemDy / 2), 200, MK_LBUTTON);
    await waitFor(client, "the first row was not selected", (st) => st.sel === 0);
    if (s.note !== 1) {
      await state(client, "note");
    }
    await waitFor(client, "the note did not appear", (st) => st.note === 1);

    // wider, then the same size again: the second only asks for the min size
    const r = getWindowRect(hwnd);
    const w = r.right - r.left + 200;
    const h = r.bottom - r.top;
    setWindowPos(hwnd, r.left, r.top, w, h);
    setWindowPos(hwnd, r.left, r.top, w, h);
    s = await state(client);
    if (s.dx <= 0 || s.wrapDx !== s.dx) {
      throw new Error(`issue-6406: note wrapped to ${s.wrapDx}px in a ${s.dx}px box`);
    }
    await client.quit();
  } finally {
    await killAndWait(proc);
  }
  console.log("issue-6406: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
