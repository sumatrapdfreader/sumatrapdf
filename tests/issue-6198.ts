// Issue #6198: free text annotations take any installed font, bold, italic and
// underline. A font other than the base 14 is embedded in the PDF.
// A free text in Georgia Bold (via /DS) gets Italic toggled on from its
// property row; the saved file has the style in /DS and embeds a Georgia font.

import { existsSync, mkdirSync, readFileSync, rmSync, statSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { findTopWindow, sleep } from "./winapi.ts";
import { clickAt, findCanvas, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

type Rect = { x: number; y: number; dx: number; dy: number };

function makePdf(): string {
  const style = "font-family:'Georgia';font-size:14pt;color:#000000;text-align:left;font-weight:bold";
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    `<< /Type /Annot /Subtype /FreeText /P 3 0 R /Rect [72 600 312 680] /Contents (Fontastic) /DA (/Helv 14 Tf 0 g) /DS (${style}) >>`,
  ]);
}

function parseRect(m: RegExpExecArray | null): Rect {
  if (!m) {
    return { x: 0, y: 0, dx: 0, dy: 0 };
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

async function markupDump(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0) {
    throw new Error(`issue-6198: could not read markup state\n${raw}`);
  }
  return raw;
}

// waits for the property row to be in the wanted state
async function waitToolbar(client: ControlClient, want: RegExp, what: string): Promise<string> {
  const deadline = Date.now() + 5_000;
  let line = "";
  for (;;) {
    line = /annotEditToolbar .*/.exec(await markupDump(client))?.[0] ?? "";
    if (want.test(line)) {
      return line;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6198: ${what}\n${line}`);
    }
    await sleep(50);
  }
}

async function clickChip(pid: number, line: string, name: string): Promise<void> {
  const placed = parseRect(/ placed=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(line));
  const chip = parseRect(new RegExp(`[=;]${name}:(-?\\d+),(-?\\d+),(\\d+),(\\d+)`).exec(line));
  if (chip.dx === 0) {
    throw new Error(`issue-6198: no "${name}" chip in ${line}`);
  }
  const tbHwnd = findTopWindow(pid, "SumatraAnnotEditToolbar");
  if (!tbHwnd) {
    throw new Error("issue-6198: property row window not found");
  }
  await clickAt(tbHwnd, chip.x - placed.x + Math.floor(chip.dx / 2), chip.y - placed.y + Math.floor(chip.dy / 2));
}

export async function testit(): Promise<void> {
  if (!existsSync("C:\\Windows\\Fonts\\georgiaz.ttf")) {
    console.log("issue-6198: skipped, needs the Georgia fonts that come with Windows");
    return;
  }
  const dir = tmpPath("issue-6198");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "free-text-font.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdf, makePdf(), "latin1");
  const origSize = statSync(pdf).size;
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n",
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
    const canvas = findCanvas(frame);
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300);

    const m = /type=FreeText[^\n]*screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(await markupDump(client));
    const rect = parseRect(m);
    if (rect.dx <= 0) {
      throw new Error("issue-6198: free text annotation not found");
    }
    await clickAt(canvas, rect.x + Math.floor(rect.dx / 2), rect.y + Math.floor(rect.dy / 2));
    const line = await waitToolbar(
      client,
      /items=[^ ]*font,bold,italic,underline,.* fontStyle=1 font=Georgia /,
      "annotation not selected as Georgia Bold",
    );
    await clickChip(proc.pid!, line, "italic");
    await waitToolbar(client, / fontStyle=3 font=Georgia /, "Italic did not toggle on");

    sendCommand(frame, cmdId("CmdSaveAnnotations"));
    const deadline = Date.now() + 10_000;
    let saved = "";
    for (;;) {
      saved = readFileSync(pdf, "latin1");
      if (saved.includes("font-style:italic") && saved.includes("/FontFile2")) {
        break;
      }
      if (Date.now() > deadline) {
        throw new Error("issue-6198: saved file lacks the italic style or an embedded font");
      }
      await sleep(100);
    }
    await sleep(300);
    saved = readFileSync(pdf, "latin1");
    if (!saved.includes("font-weight:bold")) {
      throw new Error("issue-6198: saved /DS lost font-weight:bold");
    }
    if (!/\/BaseFont\s*\/Georgia/.test(saved)) {
      throw new Error("issue-6198: saved file has no Georgia font");
    }
    const grown = saved.length - origSize;
    if (grown < 20_000) {
      throw new Error(`issue-6198: file grew by ${grown} bytes, too little for an embedded font`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-6198: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
