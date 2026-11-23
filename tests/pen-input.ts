// Pen input as orig handles WM_POINTER from a pen: every contact is a left
// press (the barrel button included), and the eraser end erases ink while the
// ink tool is on. ng gets the contacts from gpui's TouchEvent with pen set.
//
// Run: bun tests/pen-input.ts [-ng] [--no-build]

import { writeFileSync } from "node:fs";
import { ControlCommand, type ControlClient } from "./control.ts";
import { IS_WIN } from "./host.ts";
import { cmdId, makeTextPdf, pollUntil, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, prepareTouchTarget, sendCommand } from "./win-automation.ts";
import {
  injectPen,
  PEN_FLAG_BARREL,
  PEN_FLAG_INVERTED,
  POINTER_FLAG_DOWN,
  POINTER_FLAG_UP,
  POINTER_FLAG_UPDATE,
  postMessage,
  setProcessDpiAware,
  sleep,
  VK_ESCAPE,
  WM_KEYDOWN,
} from "./winapi.ts";

const LINE = "alpha bravo charlie delta echo";
const STEP_MS = 30;

type Pt = { x: number; y: number };

async function wordPos(client: ControlClient, word: string): Promise<Pt> {
  const res = await client.request(ControlCommand.TestTouchSel, ["word", word]);
  const raw = String(res[1] ?? "").trim();
  const m = /^OK (-?\d+) (-?\d+)/.exec(raw);
  if (!m) {
    throw new Error(`pen-input: no position for ${word}: ${raw}`);
  }
  return { x: Number(m[1]), y: Number(m[2]) };
}

// the selection as CmdShowSelectedText's notification shows it
async function selectedText(client: ControlClient): Promise<string> {
  await client.request(ControlCommand.TestInvokeCommand, ["CmdShowSelectedText"]);
  await sleep(100);
  const [, raw] = await client.request(ControlCommand.TestSelectedText, []);
  return String(raw ?? "")
    .replace(/^OK ?/, "")
    .trim();
}

async function inkAnnotCount(client: ControlClient): Promise<number> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const m = /annotations=(\d+)/.exec(String(res[1] ?? ""));
  return m ? Number(m[1]) : -1;
}

// hover in, touch down, drag through the points, lift, leave
async function penStroke(from: Pt, to: Pt, penFlags = 0, steps = 12): Promise<boolean> {
  if (!injectPen(from.x, from.y, POINTER_FLAG_UPDATE, { contact: false, penFlags })) {
    return false;
  }
  await sleep(STEP_MS);
  injectPen(from.x, from.y, POINTER_FLAG_DOWN, { penFlags });
  await sleep(STEP_MS);
  for (let i = 1; i <= steps; i++) {
    const x = Math.round(from.x + ((to.x - from.x) * i) / steps);
    const y = Math.round(from.y + ((to.y - from.y) * i) / steps);
    injectPen(x, y, POINTER_FLAG_UPDATE, { penFlags });
    await sleep(STEP_MS);
  }
  injectPen(to.x, to.y, POINTER_FLAG_UP, { penFlags });
  await sleep(STEP_MS);
  injectPen(to.x, to.y, POINTER_FLAG_UPDATE, { contact: false, penFlags });
  await sleep(STEP_MS);
  injectPen(to.x, to.y, POINTER_FLAG_UPDATE, { contact: false, inRange: false, penFlags });
  await sleep(STEP_MS * 3);
  return true;
}

// drags run between word centers, so only the words in between are whole
async function expectSelection(client: ControlClient, what: string, word: string): Promise<void> {
  await pollUntil(
    () => selectedText(client),
    (s) => s.includes(word),
    { timeoutMs: 3000, error: (s) => `pen-input: ${what} selected ${JSON.stringify(s)}, want "${word}" in it` },
  );
}

export async function testit(): Promise<void> {
  if (!IS_WIN) {
    console.log("SKIP pen-input: pen injection is Windows only");
    return;
  }
  setProcessDpiAware();
  const pdf = tmpPath("pen-input.pdf");
  writeFileSync(pdf, makeTextPdf(LINE));
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(true);
    await prepareTouchTarget(proc.pid, frame);
    const alpha = await wordPos(client, "alpha");
    const charlie = await wordPos(client, "charlie");
    const echo = await wordPos(client, "echo");

    // a pen drag selects text like a left-button drag
    if (!(await penStroke(alpha, charlie))) {
      console.log("pen-input: pen injection not available here, skipping");
      return;
    }
    await expectSelection(client, "pen drag", "bravo");

    // orig maps every pen contact to the left button: a barrel drag selects
    // too, instead of being a right-button drag that opens the context menu
    await penStroke(charlie, echo, PEN_FLAG_BARREL);
    await expectSelection(client, "barrel drag", "delta");
    postMessage(frame, WM_KEYDOWN, VK_ESCAPE, 0);

    // with the ink tool on, the pen draws a stroke and the eraser end erases it
    await client.setNotificationsEnabled(false);
    sendCommand(frame, cmdId("CmdCreateAnnotInk"));
    await sleep(300);
    const below = (p: Pt) => ({ x: p.x, y: p.y + 60 });
    await penStroke(below(alpha), below(echo));
    await pollUntil(
      () => inkAnnotCount(client),
      (n) => n === 1,
      { timeoutMs: 3000, error: (n) => `pen-input: pen stroke made ${n} ink annotations, want 1` },
    );
    await penStroke(below(alpha), below(echo), PEN_FLAG_INVERTED);
    await pollUntil(
      () => inkAnnotCount(client),
      (n) => n === 0,
      { timeoutMs: 3000, error: (n) => `pen-input: after erasing there are ${n} ink annotations, want 0` },
    );
    console.log("pen-input: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
