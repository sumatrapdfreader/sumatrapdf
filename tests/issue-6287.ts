// Issue #6287: long notes were not readable in the Annotations window. The
// eye button beside the filter shows the selected annotation's note under the
// list (6 lines), remembered in AnnotationsWindowShowNote; the "?" beside it
// opens the search syntax in the manual (the cheat sheet under the buttons is
// gone).
//
// Run: bun tests/issue-6287.ts [--no-build] [-ng]

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { assemblePdf, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util.ts";
import { sleep, findTopWindow, MK_LBUTTON } from "./winapi.ts";
import { clickAt, launchControlled } from "./win-automation.ts";

const FLOAT_CLASS = "SUMATRA_ANNOT_FILTER_WND";
const NOTE =
  "Replace this sentence with a shorter one. The paragraph goes on to explain why the change is needed, " +
  "and then proposes the new wording in full so the reviewer can paste it in.";

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R 5 0 R] >>",
    `<< /Type /Annot /Subtype /Text /P 3 0 R /Rect [72 700 92 720] /T (kjk) /Contents (${NOTE}) >>`,
    "<< /Type /Annot /Subtype /Highlight /P 3 0 R /Rect [72 500 220 530] " +
      "/QuadPoints [72 530 220 530 72 500 220 500] /C [1 1 0] /T (bob) /Contents (beta note) >>",
  ]);
}

type State = { visible: boolean; sel: number; note: number; noteText: string; itemDy: number; listY: number };

async function state(client: ControlClient, action = "paint"): Promise<State> {
  const res = await client.request(ControlCommand.TestAnnotFilter, [action, 0, 0]);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0) {
    throw new Error(`issue-6287: TestAnnotFilter ${action}: ${raw.trim()}`);
  }
  return {
    visible: /floatVisible=1/.test(raw),
    sel: +(/ sel=(-?\d+)/.exec(raw)?.[1] ?? -1),
    note: +(/^note=(\d)/m.exec(raw)?.[1] ?? -1),
    noteText: /noteText=(.*)$/m.exec(raw)?.[1] ?? "",
    itemDy: +(/itemDy=(\d+)/.exec(raw)?.[1] ?? 0),
    listY: +(/listY=(\d+)/.exec(raw)?.[1] ?? 0),
  };
}

async function waitFor(client: ControlClient, what: string, pred: (s: State) => boolean): Promise<State> {
  const deadline = Date.now() + 6000 * SLOW_BUILD_FACTOR;
  let s = await state(client);
  while (!pred(s)) {
    if (Date.now() > deadline) {
      throw new Error(`issue-6287: ${what}; state ${JSON.stringify(s)}`);
    }
    await sleep(50);
    s = await state(client);
  }
  return s;
}

// select the first row: ng has a test action, orig's list takes a click
async function selectFirstRow(client: ControlClient, pid: number): Promise<void> {
  if (USE_NG) {
    await client.request(ControlCommand.TestAnnotFilter, ["click", 0, 0]);
    return;
  }
  const s = await waitFor(client, "the list never laid out", (st) => st.itemDy > 0);
  const floatWnd = findTopWindow(pid, FLOAT_CLASS);
  if (!floatWnd) {
    throw new Error("issue-6287: no Annotations window");
  }
  await clickAt(floatWnd, 24, s.listY + Math.floor(s.itemDy / 2), 200, MK_LBUTTON);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6287");
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  const settingsPath = join(appdata, "SumatraPDF-settings.txt");
  writeFileSync(settingsPath, "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\n");
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
    let s = await waitFor(client, "the Annotations window never opened", (st) => st.visible && st.note === 0);

    await selectFirstRow(client, proc.pid!);
    await waitFor(client, "the first row was not selected", (st) => st.sel === 0);

    // the eye shows the note of the selected row
    await state(client, "note");
    s = await waitFor(client, "the note did not appear", (st) => st.note === 1 && st.noteText.length > 0);
    if (!s.noteText.startsWith("Replace this sentence")) {
      throw new Error(`issue-6287: the preview shows '${s.noteText}', not the selected annotation's note`);
    }

    // and again hides it
    await state(client, "note");
    await waitFor(client, "the note did not go away", (st) => st.note === 0);

    // shown is what gets remembered
    await state(client, "note");
    await waitFor(client, "the note did not come back", (st) => st.note === 1);
    await client.quit();
    await proc.exited;
  } finally {
    client.close();
    proc.kill();
  }

  const saved = readFileSync(settingsPath, "utf8");
  if (!/^AnnotationsWindowShowNote = true$/m.test(saved)) {
    throw new Error("issue-6287: AnnotationsWindowShowNote = true was not saved to the settings file");
  }
  console.log("issue-6287: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
