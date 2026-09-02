// #6123: in dark themes the Find Annotation window's disabled action buttons
// used DisabledTextColor on a slightly lifted button background, so the label
// (especially "Save changes to a new PDF") vanished into the fill.
//
// Run: bun tests/issue-6123.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { captureWindowToPng, findTopWindow, getWindowPid, sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const FLOAT_CLASS = "SUMATRA_ANNOT_FILTER_WND";
// GetLightness units: muted text must stay this far from its fill (issue #6123)
const MIN_DELTA = 80;

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    "<< /Type /Annot /Subtype /Highlight /P 3 0 R /Rect [72 680 220 710] " +
      "/QuadPoints [72 710 220 710 72 680 220 680] /C [1 1 0] /CA 0.5 " +
      "/Contents (alpha unique comment) >>",
  ]);
}

function parseSaveNew(dump: string): { x: number; y: number; dx: number; dy: number; text: string; bg: string } {
  const m = /saveNewRect=(-?\d+),(-?\d+),(-?\d+),(-?\d+) saveNewText=(#[0-9a-fA-F]+) saveNewBg=(#[0-9a-fA-F]+)/.exec(
    dump,
  );
  if (!m) {
    throw new Error(`issue-6123: no saveNew colors in dump\n${dump}`);
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]!, text: m[5]!, bg: m[6]! };
}

function parseRgb(hex: string): [number, number, number] {
  const h = hex.replace(/^#/, "");
  const body = h.length === 8 ? h.slice(2) : h;
  return [parseInt(body.slice(0, 2), 16), parseInt(body.slice(2, 4), 16), parseInt(body.slice(4, 6), 16)];
}

async function annotDump(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestToolbarButtons, []);
  return String(res[1] ?? "");
}

async function waitFloat(client: ControlClient, pid: number): Promise<{ dump: string; hwnd: number }> {
  const deadline = Date.now() + 8000 * SLOW_BUILD_FACTOR;
  let dump = "";
  for (;;) {
    dump = await annotDump(client);
    const hwnd = findTopWindow(pid, FLOAT_CLASS);
    if (/annotFilter floatVisible=1/.test(dump) && hwnd && parseSaveNew(dump).dy > 0) {
      return { dump, hwnd };
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6123: annotation list did not open\n${dump}`);
    }
    await sleep(40);
  }
}

function lightness(r: number, g: number, b: number): number {
  return (Math.max(r, g, b) + Math.min(r, g, b)) / 2;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6123");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "annots.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdf, makePdf(), "latin1");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\nTheme = One Dark\n",
  );

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-view",
    "single page",
    "-zoom",
    "fit page",
    pdf,
  ]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300);
    sendCommand(frame, cmdId("CmdFindAnnotation"));
    const pid = getWindowPid(frame) || proc.pid!;
    const { dump, hwnd } = await waitFloat(client, pid);
    if (/saveEnabled=1/.test(dump)) {
      throw new Error(`issue-6123: save buttons should be disabled with no edits\n${dump}`);
    }

    const rc = parseSaveNew(dump);
    captureWindowToPng(hwnd, join(dir, "annot-list.png"));
    const [tr, tg, tb] = parseRgb(rc.text);
    const [br, bg, bb] = parseRgb(rc.bg);
    const delta = Math.abs(lightness(tr, tg, tb) - lightness(br, bg, bb));
    if (delta < MIN_DELTA) {
      throw new Error(
        `issue-6123: disabled Save to new PDF text ${rc.text} is too close to fill ${rc.bg} (lightness delta ${delta.toFixed(1)}, want >= ${MIN_DELTA})`,
      );
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-6123: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
