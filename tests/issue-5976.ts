// #5976: delete many annotations at once. The Annotations list is a multi-select
// VirtListBox (Shift/Ctrl click, Shift+arrows, Ctrl+A); Del removes every
// selected row.
import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand } from "./control";
import { assemblePdf, cmdId, runStandalone, tmpPath, waitForAnnotWindow } from "./util";
import { postMessage, sleep, WM_CLOSE, WM_KEYDOWN } from "./winapi";
import { killAndWait, launchControlled, sendCommand, sendCommandSync, waitForExit } from "./win-automation";

const VK_DELETE = 0x2e;
const kAnnotCount = 8;

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

type AnnotList = { sel: number; n: number; selCount: number; raw: string };

async function listState(client: ControlClient, selectItem = 0, selectLast = 0): Promise<AnnotList> {
  const deadline = Date.now() + 10_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestAnnotEditorLayout, [0, selectItem, selectLast]);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "").trim();
    if (exitCode === 0) {
      const m = /sel=(-?\d+) n=(\d+) selCount=(\d+)/.exec(raw);
      if (!m) {
        throw new Error(`issue-5976: could not parse: ${raw}`);
      }
      return { sel: +m[1]!, n: +m[2]!, selCount: +m[3]!, raw };
    }
    if (exitCode !== 2) {
      throw new Error(`issue-5976: TestAnnotEditorLayout failed: ${raw}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5976: editor never ready: ${raw}`);
    }
    await sleep(50);
  }
}

async function waitN(client: ControlClient, want: number, what: string): Promise<AnnotList> {
  const deadline = Date.now() + 5_000;
  for (;;) {
    const st = await listState(client);
    if (st.n === want) {
      return st;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5976: ${what} (n=${st.n}, want=${want})`);
    }
    await sleep(40);
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-5976.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdEditAnnotations"));
    const annotWin = await waitForAnnotWindow(proc.pid!, frame);
    let st = await listState(client);
    if (st.n < kAnnotCount) {
      throw new Error(`issue-5976: expected ${kAnnotCount} annotations, got n=${st.n} (${st.raw})`);
    }

    st = await listState(client, 1, 3);
    if (st.selCount !== 3) {
      throw new Error(`issue-5976: range select 1-3: selCount=${st.selCount} (${st.raw})`);
    }
    postMessage(annotWin, WM_KEYDOWN, VK_DELETE, 0);
    st = await waitN(client, kAnnotCount - 3, "Del after range select");
    console.log("  Del removes a Shift-range of 3 ✓");

    st = await listState(client, -1);
    if (st.selCount !== st.n || st.n === 0) {
      throw new Error(`issue-5976: Ctrl+A equivalent: selCount=${st.selCount} n=${st.n} (${st.raw})`);
    }
    postMessage(annotWin, WM_KEYDOWN, VK_DELETE, 0);
    st = await waitN(client, 0, "Del after select all");
    console.log("  Del removes every selected annotation ✓");

    // Deleting annotations intentionally makes the PDF dirty. Discard those
    // test changes before closing; otherwise WM_CLOSE waits in the unsaved
    // annotations dialog for user input that CI cannot provide.
    sendCommandSync(frame, cmdId("CmdDiscardChanges"));
    await client.waitForRenderIdle();
    postMessage(frame, WM_CLOSE, 0, 0);
    if (!(await waitForExit(proc))) {
      throw new Error("issue-5976: SumatraPDF didn't exit after WM_CLOSE");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-5976: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
