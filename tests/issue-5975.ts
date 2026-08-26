// #5975: Home / End / PageUp / PageDown / arrows should move the annotations
// list, as they did with the native ListBox in 3.6.1. The list is a VirtListBox
// now, so those keys are handled on the editor window.
import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand } from "./control";
import { assemblePdf, cmdId, runStandalone, tmpPath, waitForAnnotWindow } from "./util";
import { postMessage, sleep, WM_CLOSE, WM_KEYDOWN } from "./winapi";
import { killAndWait, launchControlled, sendCommand, waitForExit } from "./win-automation";

const VK_END = 0x23;
const VK_HOME = 0x24;
const VK_NEXT = 0x22; // Page Down
const VK_DOWN = 0x28;

const kAnnotCount = 20;

function makePdf(): string {
  const annots: string[] = [];
  for (let i = 0; i < kAnnotCount; i++) {
    const y = 700 - i * 20;
    annots.push(`<< /Type /Annot /Subtype /Text /Rect [50 ${y} 70 ${y + 18}] /T (t${i}) /Contents (note ${i + 1}) >>`);
  }
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Count 1 /Kids [3 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [${annots.join(" ")}] >>`,
  ];
  return assemblePdf(objs);
}

type AnnotList = { sel: number; n: number; raw: string };

async function listState(client: ControlClient): Promise<AnnotList> {
  const deadline = Date.now() + 10_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestAnnotEditorLayout, [0, 0]);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "").trim();
    if (exitCode === 0) {
      const m = /sel=(-?\d+) n=(\d+)/.exec(raw);
      if (!m) {
        throw new Error(`issue-5975: could not parse: ${raw}`);
      }
      return { sel: +m[1]!, n: +m[2]!, raw };
    }
    if (exitCode !== 2) {
      throw new Error(`issue-5975: TestAnnotEditorLayout failed: ${raw}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5975: editor never ready: ${raw}`);
    }
    await sleep(50);
  }
}

async function waitSel(client: ControlClient, want: number, what: string): Promise<AnnotList> {
  const deadline = Date.now() + 5_000;
  for (;;) {
    const st = await listState(client);
    if (st.sel === want) {
      return st;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5975: ${what} (sel=${st.sel}, want=${want}, n=${st.n})`);
    }
    await sleep(40);
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-5975.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdEditAnnotations"));
    const annotWin = await waitForAnnotWindow(proc.pid!, frame);
    let st = await listState(client);
    if (st.n < kAnnotCount) {
      throw new Error(`issue-5975: expected ${kAnnotCount} annotations, got n=${st.n} (${st.raw})`);
    }

    // no click: keys must work as soon as the window is open (3.6.1 ListBox).
    // Opening now auto-selects the first annot on the current page (so Contents
    // is filled). CmdEditAnnotations can also pick the annot under the cursor.
    // Home pins the caret at 0 from either starting point.
    postMessage(annotWin, WM_KEYDOWN, VK_HOME, 0);
    st = await waitSel(client, 0, "Home on open");

    postMessage(annotWin, WM_KEYDOWN, VK_DOWN, 0);
    st = await waitSel(client, 1, "Down");

    postMessage(annotWin, WM_KEYDOWN, VK_END, 0);
    st = await waitSel(client, st.n - 1, "End");

    postMessage(annotWin, WM_KEYDOWN, VK_HOME, 0);
    st = await waitSel(client, 0, "Home");

    postMessage(annotWin, WM_KEYDOWN, VK_NEXT, 0);
    st = await listState(client);
    if (st.sel <= 0) {
      throw new Error(`issue-5975: PageDown did not move down: ${JSON.stringify(st)}`);
    }

    postMessage(frame, WM_CLOSE, 0, 0);
    if (!(await waitForExit(proc))) {
      throw new Error("issue-5975: SumatraPDF didn't exit after WM_CLOSE");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-5975: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
