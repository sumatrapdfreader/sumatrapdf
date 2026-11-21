// A finger held still on a word selects it and puts a drag handle under each
// end; dragging the end handle extends the selection (issue #538). ng gets
// the raw contact from gpui's TouchEvent, orig from WM_POINTER.
//
// Run: bun tests/touch-long-press-select.ts [-ng] [--no-build]

import { writeFileSync } from "node:fs";
import { ControlCommand, type ControlClient } from "./control.ts";
import { IS_WIN } from "./host.ts";
import { makeTextPdf, pollUntil, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import {
  injectTouch,
  POINTER_FLAG_DOWN,
  POINTER_FLAG_UP,
  POINTER_FLAG_UPDATE,
  setProcessDpiAware,
  sleep,
} from "./winapi.ts";

const LINE = "alpha bravo charlie delta echo";
// longer than the app's 500ms long press
const HOLD_MS = 900;
const SAMPLE_MS = 40;

type Pt = { x: number; y: number };

async function touchSel(client: ControlClient, action: string, word = ""): Promise<string> {
  const res = await client.request(ControlCommand.TestTouchSel, [action, word]);
  return String(res[1] ?? "").trim();
}

async function wordPos(client: ControlClient, word: string): Promise<Pt> {
  const raw = await touchSel(client, "word", word);
  const m = /^OK (-?\d+) (-?\d+)/.exec(raw);
  if (!m) {
    throw new Error(`touch-long-press-select: no position for ${word}: ${raw}`);
  }
  return { x: Number(m[1]), y: Number(m[2]) };
}

async function selectedText(client: ControlClient): Promise<string> {
  await client.request(ControlCommand.TestInvokeCommand, ["CmdShowSelectedText"]);
  await sleep(100);
  const [, raw] = await client.request(ControlCommand.TestSelectedText, []);
  return String(raw ?? "")
    .replace(/^OK ?/, "")
    .trim();
}

// a contact that stays put long enough to be a long press; updates keep the
// injected contact alive
async function holdAt(p: Pt): Promise<boolean> {
  if (!injectTouch(p.x, p.y, POINTER_FLAG_DOWN)) {
    return false;
  }
  for (let t = 0; t < HOLD_MS; t += SAMPLE_MS) {
    await sleep(SAMPLE_MS);
    injectTouch(p.x, p.y, POINTER_FLAG_UPDATE);
  }
  return injectTouch(p.x, p.y, POINTER_FLAG_UP);
}

async function dragFromTo(from: Pt, to: Pt, steps = 15): Promise<void> {
  injectTouch(from.x, from.y, POINTER_FLAG_DOWN);
  await sleep(SAMPLE_MS);
  for (let i = 1; i <= steps; i++) {
    const x = Math.round(from.x + ((to.x - from.x) * i) / steps);
    const y = Math.round(from.y + ((to.y - from.y) * i) / steps);
    injectTouch(x, y, POINTER_FLAG_UPDATE);
    await sleep(SAMPLE_MS);
  }
  injectTouch(to.x, to.y, POINTER_FLAG_UP);
}

export async function testit(): Promise<void> {
  if (!IS_WIN) {
    console.log("SKIP touch-long-press-select: touch injection is Windows only");
    return;
  }
  setProcessDpiAware();
  const pdf = tmpPath("touch-long-press-select.pdf");
  writeFileSync(pdf, makeTextPdf(LINE));
  const { proc, client } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(true);
    const bravo = await wordPos(client, "bravo");
    if (!(await holdAt(bravo))) {
      console.log("touch-long-press-select: touch injection not available here, skipping");
      return;
    }
    const state = await pollUntil(
      () => touchSel(client, "state"),
      (s) => s.includes("handles=1"),
      { timeoutMs: 3000, error: (s) => `touch-long-press-select: no handles after a long press: ${s}` },
    );
    const word = await pollUntil(
      () => selectedText(client),
      (s) => s.length > 0,
      { timeoutMs: 3000, error: "touch-long-press-select: long press selected nothing" },
    );
    if (word !== "bravo") {
      throw new Error(`touch-long-press-select: long press selected ${JSON.stringify(word)}, not "bravo"`);
    }

    // drag the end handle along the line to "delta"
    const m = /end=(-?\d+),(-?\d+)/.exec(state);
    if (!m) {
      throw new Error(`touch-long-press-select: no end handle in: ${state}`);
    }
    const endHandle = { x: Number(m[1]), y: Number(m[2]) };
    const delta = await wordPos(client, "delta");
    await dragFromTo(endHandle, { x: delta.x, y: endHandle.y });
    const extended = await pollUntil(
      () => selectedText(client),
      (s) => s.startsWith("bravo charlie d"),
      { timeoutMs: 3000, error: (s) => `touch-long-press-select: handle drag selected ${JSON.stringify(s)}` },
    );
    console.log(`touch-long-press-select: selected "${word}", then "${extended}"`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
