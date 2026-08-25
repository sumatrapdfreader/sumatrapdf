// Text selection boxes use the font's ascender/descender (3.6.1), not tight
// glyph ink. A lowercase word such as "compass" (no d/h/l) still has padding
// above the letters. The highlight outline is 1px (not part of this dump).
//
// Run: bun tests/selection-font-bbox.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";
import { WM_CHAR, WM_KEYDOWN, WM_KEYUP, postMessage, sleep } from "./winapi.ts";

const VK_END = 0x23;
const PAGE_H = 792;
const BASELINE_PDF_Y = 700;
const FONT_SIZE = 18;
const LINE = "compass";

function makeTextPdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const body: Record<number, Buffer> = {};
  body[1] = enc("<< /Type /Catalog /Pages 2 0 R >>");
  body[2] = enc("<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
  body[6] = enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>");
  const stream = `BT /F1 ${FONT_SIZE} Tf 72 ${BASELINE_PDF_Y} Td (${LINE}) Tj ET`;
  body[10] = enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`);
  body[3] = enc(
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 ${PAGE_H}] ` +
      `/Resources << /Font << /F1 6 0 R >> >> /Contents 10 0 R >>`,
  );
  const maxN = 12;
  const parts: Buffer[] = [enc("%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0]!.length;
  for (let n = 1; n <= maxN; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj\n`), body[n] ?? enc("null"), enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  let xref = `xref\n0 ${maxN + 1}\n0000000000 65535 f \n`;
  for (let n = 1; n <= maxN; n++) {
    xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

function pressVKey(hwnd: number, vk: number): void {
  postMessage(hwnd, WM_KEYDOWN, vk, 0);
  postMessage(hwnd, WM_KEYUP, vk, 0);
}

function parseExpanded(raw: string): string {
  const m = /^expanded=(.*)$/m.exec(raw);
  if (!m) {
    throw new Error(`selection-font-bbox: no expanded= in:\n${raw}`);
  }
  return m[1]!.trim();
}

function parsePageRect(raw: string): { x: number; y: number; dx: number; dy: number } {
  const m = /^rect=([^,]+),([^,]+),([^,]+),([^,]+) page=/m.exec(raw);
  if (!m) {
    throw new Error(`selection-font-bbox: no page-space rect in:\n${raw}`);
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

async function waitForSelection(client: ControlClient, want: string): Promise<string> {
  const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
  let raw = "";
  while (Date.now() < deadline) {
    raw = String((await client.request(ControlCommand.TestSelectionVars, ["${selection}"]))[1] ?? "");
    if (parseExpanded(raw) === want) {
      return raw;
    }
    await sleep(25);
  }
  throw new Error(`selection-font-bbox: selected '${parseExpanded(raw)}', want '${want}'\n${raw}`);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("selection-font-bbox.pdf");
  writeFileSync(pdf, makeTextPdf());

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    sendCommandSync(frame, cmdId("CmdSelectTextViaKeyboard"));
    const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
    let dump = "";
    while (Date.now() < deadline) {
      dump = String((await client.request(ControlCommand.TestSelectTextKeyboard, []))[1] ?? "");
      if (/active=1/.test(dump)) {
        break;
      }
      await sleep(25);
    }
    if (!/active=1/.test(dump)) {
      throw new Error(`selection-font-bbox: keyboard selection did not start\n${dump}`);
    }

    postMessage(frame, WM_CHAR, "v".charCodeAt(0), 0);
    while (Date.now() < deadline) {
      dump = String((await client.request(ControlCommand.TestSelectTextKeyboard, []))[1] ?? "");
      if (/visual=1/.test(dump)) {
        break;
      }
      await sleep(25);
    }
    if (!/visual=1/.test(dump)) {
      throw new Error(`selection-font-bbox: visual mode did not start\n${dump}`);
    }

    pressVKey(frame, VK_END);
    const raw = await waitForSelection(client, LINE);
    const rect = parsePageRect(raw);
    // Fitz page space: origin top-left, y down. PDF baseline is BASELINE_PDF_Y
    // from the bottom. Tight glyph ink for "compass" only reaches ~x-height
    // above the baseline (~0.5em); font-height boxes reach the ascender (~0.7em).
    const baselineFitz = PAGE_H - BASELINE_PDF_Y;
    const topPad = baselineFitz - rect.y;
    const minTop = FONT_SIZE * 0.65;
    if (!(topPad >= minTop)) {
      throw new Error(
        `selection-font-bbox: top padding ${topPad.toFixed(2)} is tight glyph ink, ` +
          `want >= ${minTop} (font-height). rect=${rect.x},${rect.y},${rect.dx},${rect.dy}\n${raw}`,
      );
    }
    console.log(`selection-font-bbox: topPad=${topPad.toFixed(2)} dy=${rect.dy} (font ${FONT_SIZE})`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
