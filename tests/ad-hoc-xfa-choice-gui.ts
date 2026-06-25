// Ad-hoc: manual GUI check for XFA dropDownList (choice) field editing.
//
// Run:  bun tests/ad-hoc-xfa-choice-gui.ts [--no-build]

import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { ControlCommand, withControlledSumatra } from "../cmd/control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";
import {
  launchSumatra,
  waitForFrame,
  findCanvas,
  clickAt,
  captureWindowToPng,
  findChildByClass,
  CANVAS_CLASS,
} from "./win-automation.ts";
import {
  getClientRect,
  sendMessage,
  sleep,
  enumChildWindows,
  getClassName,
  waitForChildWindow,
} from "./winapi.ts";

const here = dirname(fileURLToPath(import.meta.url));
const PDF = join(here, "ad-hoc-xfa-data", "ad-hoc-xfa-choice.pdf");
const FIELD = "filingStatus";
const NEW_VALUE = "Married filing jointly";
const CHOICE_INDEX = 1;

// PDF-space center of filingStatus (from TestXfaFieldRects).
const FIELD_PDF_X = "180";
const FIELD_PDF_Y = "693";

const LB_SETCURSEL = 0x0186;
const LB_GETCURSEL = 0x0188;
const LB_GETCOUNT = 0x018b;

// Default-fit screen center for filingStatus on ad-hoc-xfa-choice.pdf (see TestXfaGuiFieldInteract).
const FIELD_SCREEN_FALLBACK = { x: 276, y: 937 };

async function probeFieldScreenCenter(): Promise<{ x: number; y: number }> {
  try {
    const res = await withControlledSumatra(
      EXE,
      (client) => client.request(ControlCommand.TestXfaGuiFieldInteract, [FIELD_PDF_X, FIELD_PDF_Y, "0"]),
      [PDF],
    );
    const exitCode = res[0] as number;
    const line = String(res[1] ?? "").trim();
    if (exitCode !== 0) {
      throw new Error(`TestXfaGuiFieldInteract failed: ${line}`);
    }
    const m = line.match(/screen=(\d+),(\d+)/);
    if (!m) {
      throw new Error(`could not parse screen coords from: ${line}`);
    }
    if (!line.includes("kind=4") || !line.includes(`name=${FIELD}`)) {
      throw new Error(`expected choice field ${FIELD}, got: ${line}`);
    }
    return { x: Number(m[1]), y: Number(m[2]) };
  } catch (e) {
    console.log(`probe skipped (${e}), using fallback ${FIELD_SCREEN_FALLBACK.x},${FIELD_SCREEN_FALLBACK.y}`);
    return FIELD_SCREEN_FALLBACK;
  }
}

function findListBox(canvas: number): number {
  return findChildByClass(canvas, "ListBox");
}

async function waitForListBox(canvas: number, timeoutMs = 2000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const lb = findListBox(canvas);
    if (lb) return lb;
    await sleep(80);
  }
  return 0;
}

function listBoxItemCount(lb: number): number {
  return Number(sendMessage(lb, LB_GETCOUNT, 0, 0));
}

async function openChoiceAt(canvas: number, x: number, y: number): Promise<number> {
  for (const [dx, dy] of [
    [0, 0],
    [0, -12],
    [0, 12],
    [-20, 0],
    [20, 0],
  ]) {
    await clickAt(canvas, x + dx, y + dy, 300);
    const lb = await waitForListBox(canvas, 800);
    if (lb) return lb;
  }
  return 0;
}

async function commitChoiceSelection(lb: number, index: number): Promise<void> {
  sendMessage(lb, LB_SETCURSEL, index, 0);
  await sleep(120);
  const cr = getClientRect(lb);
  const itemY = 8 + index * 20;
  const itemX = Math.max(8, Math.floor(cr.right / 2));
  await clickAt(lb, itemX, itemY, 350);
}

export async function testit(): Promise<void> {
  const center = process.argv.includes("--probe")
    ? await probeFieldScreenCenter()
    : FIELD_SCREEN_FALLBACK;
  console.log(`field screen center: ${center.x},${center.y}`);

  Bun.spawnSync(["taskkill", "/F", "/IM", "SumatraPDF-dll.exe"]);
  await sleep(400);

  const proc = launchSumatra([PDF]);
  try {
    const frame = await waitForFrame(proc.pid!);
    if (!frame) throw new Error("no frame hwnd");
    const canvas = await waitForChildWindow(frame, CANVAS_CLASS, 15000);
    if (!canvas) throw new Error("no canvas hwnd");
    await sleep(2500);

    const beforePng = tmpPath("ad-hoc-xfa-choice-gui-before.png");
    captureWindowToPng(canvas, beforePng);
    console.log(`before: ${beforePng}`);

    const lb = await openChoiceAt(canvas, center.x, center.y);
    if (!lb) {
      throw new Error("ListBox did not appear after clicking choice field");
    }
    const n = listBoxItemCount(lb);
    if (n < 3) {
      throw new Error(`ListBox has ${n} items, expected >= 3`);
    }

    const openPng = tmpPath("ad-hoc-xfa-choice-gui-open.png");
    captureWindowToPng(canvas, openPng);
    console.log(`dropdown open: ${openPng} (items=${n})`);

    await commitChoiceSelection(lb, CHOICE_INDEX);
    await sleep(400);
    if (findListBox(canvas)) {
      throw new Error("ListBox still visible after commit");
    }

    const afterPng = tmpPath("ad-hoc-xfa-choice-gui-after.png");
    captureWindowToPng(canvas, afterPng);
    console.log(`after commit: ${afterPng}`);

    const lb2 = await openChoiceAt(canvas, center.x, center.y);
    if (!lb2) {
      throw new Error("ListBox did not re-appear for selection verify");
    }
    const sel = Number(sendMessage(lb2, LB_GETCURSEL, 0, 0));
    await commitChoiceSelection(lb2, sel);
    await sleep(300);
    if (sel !== CHOICE_INDEX) {
      throw new Error(`expected LB_GETCURSEL=${CHOICE_INDEX} after commit, got ${sel}`);
    }

    console.log(`ad-hoc-xfa-choice-gui: OK opened ListBox, selected "${NEW_VALUE}", re-open sel=${sel}`);
  } finally {
    proc.kill();
    await sleep(300);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}