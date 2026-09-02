// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/4839
//
// Selection of rotated PDF text must follow the glyph quads, not an
// axis-aligned box around them.
//
// Run: bun tests/issue-4839.ts [--no-build]

import { copyFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { cmdId, ROOT, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";
import { postMessage, sleep, WM_CHAR, WM_KEYDOWN, WM_KEYUP } from "./winapi.ts";

const CM_PDF = join(ROOT, "tests", "issue-4839-data", "rotated.pdf");

const VK_END = 0x23;

function makeRotatedPdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  // 45-degree Tm: (cos, sin, -sin, cos, x, y)
  const stream = "BT /F1 24 Tf 0.7071 0.7071 -0.7071 0.7071 120 200 Tm (ROTATED) Tj ET";
  const body: Record<number, Buffer> = {
    1: enc("<< /Type /Catalog /Pages 2 0 R >>"),
    2: enc("<< /Type /Pages /Kids [3 0 R] /Count 1 >>"),
    3: enc(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
        `/Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>`,
    ),
    4: enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>"),
    5: enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`),
  };
  const parts: Buffer[] = [enc("%PDF-1.4\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0]!.length;
  for (let n = 1; n <= 5; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj\n`), body[n]!, enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  let xref = `xref\n0 6\n0000000000 65535 f \n`;
  for (let n = 1; n <= 5; n++) {
    xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

function parseQuads(raw: string): { ulx: number; uly: number; urx: number; ury: number }[] {
  const out: { ulx: number; uly: number; urx: number; ury: number }[] = [];
  const re = /^quad=([^,]+),([^ ]+) ([^,]+),([^ ]+) /gm;
  let m: RegExpExecArray | null;
  while ((m = re.exec(raw))) {
    out.push({ ulx: +m[1]!, uly: +m[2]!, urx: +m[3]!, ury: +m[4]! });
  }
  return out;
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-4839.pdf");
  writeFileSync(pdf, makeRotatedPdf());

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
      throw new Error(`issue-4839: keyboard selection did not start\n${dump}`);
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
      throw new Error(`issue-4839: visual mode did not start\n${dump}`);
    }
    postMessage(frame, WM_KEYDOWN, VK_END, 0);
    postMessage(frame, WM_KEYUP, VK_END, 0);

    let raw = "";
    let quads: ReturnType<typeof parseQuads> = [];
    const selDeadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
    while (Date.now() < selDeadline) {
      raw = String((await client.request(ControlCommand.TestSelectionVars, ["${selection}"]))[1] ?? "");
      quads = parseQuads(raw);
      if (quads.some((q) => Math.abs(q.uly - q.ury) > 2)) {
        break;
      }
      await sleep(50);
    }
    if (!/^expanded=.+$/m.test(raw) || /^expanded=\s*$/m.test(raw)) {
      throw new Error(`issue-4839: no text selection:\n${raw}`);
    }
    if (quads.length === 0) {
      throw new Error(`issue-4839: no glyph quads in selection dump:\n${raw}`);
    }
    // 45-degree glyphs: top edge is diagonal, so ul.y != ur.y
    const hasTilt = quads.some((q) => Math.abs(q.uly - q.ury) > 2);
    if (!hasTilt) {
      throw new Error(`issue-4839: selection quads are axis-aligned:\n${raw}`);
    }
  } finally {
    client.close();
    try {
      await killAndWait(proc);
    } catch {
      // already exited
    }
  }

  // click-drag must stay a text selection, not a rubber-band rectangle
  const cmPdf = tmpPath("issue-4839-cm.pdf");
  copyFileSync(CM_PDF, cmPdf);
  const drag = await launchControlled(["-view", "single page", "-zoom", "fit page", cmPdf]);
  try {
    await drag.client.waitForRenderIdle();
    await drag.client.setNotificationsEnabled(false);
    const res = await drag.client.request(ControlCommand.TestRotatedTextMouseDrag, ["Example"]);
    const dragRaw = String(res[1] ?? "");
    if (res[0] !== 0) {
      throw new Error(`issue-4839: mouse drag of rotated text failed\n${dragRaw}`);
    }
  } finally {
    drag.client.close();
    try {
      await killAndWait(drag.proc);
    } catch {
      // already exited
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
