// #6023 / discussion #6015: underline markup must sit below the baseline.
// Selection boxes use the font's ascender/descender (as in 3.6.1), so MuPDF's
// line at 1/7 of that box from the bottom clears letters without descenders.
//
// Run: bun tests/issue-6023.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { FRAME_CLASS, sendCommandSync } from "./win-automation.ts";
import { WM_CHAR, WM_KEYDOWN, WM_KEYUP, postMessage, sleep, waitForTopWindow } from "./winapi.ts";

const VK_END = 0x23;
const PAGE_H = 792;
const BASELINE_PDF_Y = 700;
const LINE = "AAAA";

function makeTextPdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const body: Record<number, Buffer> = {};
  body[1] = enc("<< /Type /Catalog /Pages 2 0 R >>");
  body[2] = enc("<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
  body[6] = enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>");
  const stream = `BT /F1 18 Tf 72 ${BASELINE_PDF_Y} Td (${LINE}) Tj ET`;
  body[10] = enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`);
  body[11] = enc("<< /Type /Annot /Subtype /Text /Rect [50 650 70 670] /Contents (existing note) >>");
  body[3] = enc(
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 ${PAGE_H}] ` +
      `/Resources << /Font << /F1 6 0 R >> >> /Contents 10 0 R /Annots [11 0 R] >>`,
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

async function annotationCount(client: ControlClient): Promise<{ count: number; raw: string }> {
  const res = await client.request(ControlCommand.TestAnnotEditorLayout, [0, 0]);
  const raw = String(res[1] ?? "").trim();
  const count = / n=(\d+)/.exec(raw);
  if (res[0] !== 0 || !count) {
    throw new Error(`could not read annotation count: ${raw}`);
  }
  return { count: +count[1]!, raw };
}

function parseMarkupDump(
  dump: string,
): { type: string; page: number; rects: { x: number; y: number; dx: number; dy: number }[] }[] {
  const annots: { type: string; page: number; rects: { x: number; y: number; dx: number; dy: number }[] }[] = [];
  let cur: (typeof annots)[0] | undefined;
  for (const line of dump.split("\n")) {
    const t = /^type=(\S+) page=(\d+) quads=(\d+)$/.exec(line);
    if (t) {
      cur = { type: t[1]!, page: +t[2]!, rects: [] };
      annots.push(cur);
      continue;
    }
    const r = /^rect=([^,]+),([^,]+),([^,]+),([^,]+)$/.exec(line);
    if (r && cur) {
      cur.rects.push({ x: +r[1]!, y: +r[2]!, dx: +r[3]!, dy: +r[4]! });
    }
  }
  return annots;
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6023.pdf");
  writeFileSync(pdf, makeTextPdf());

  await withControlledSumatra(
    EXE,
    async (client: ControlClient, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      if (!frame) {
        throw new Error("no frame window");
      }
      await client.waitForRenderIdle();
      await client.setNotificationsEnabled(false);

      const layoutBefore = await annotationCount(client);
      if (layoutBefore.count !== 1) {
        throw new Error(`expected one initial annotation: ${layoutBefore.raw}`);
      }

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
        throw new Error(`keyboard selection did not start\n${dump}`);
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
        throw new Error(`visual mode did not start\n${dump}`);
      }

      pressVKey(frame, VK_END);
      while (Date.now() < deadline) {
        dump = String((await client.request(ControlCommand.TestSelectTextKeyboard, []))[1] ?? "");
        if (dump.includes(`text=${LINE}`)) {
          break;
        }
        await sleep(25);
      }
      if (!dump.includes(`text=${LINE}`)) {
        throw new Error(`did not select "${LINE}"\n${dump}`);
      }

      sendCommandSync(frame, cmdId("CmdCreateAnnotUnderline"));
      const layoutAfter = await annotationCount(client);
      if (layoutAfter.count !== 2) {
        const message = `annotation list did not add underline: before=${layoutBefore.raw} after=${layoutAfter.raw}`;
        sendCommandSync(frame, cmdId("CmdDiscardChanges"));
        await client.waitForRenderIdle();
        throw new Error(message);
      }
      const annotDeadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
      let annotDump = "";
      let annots = parseMarkupDump("");
      while (Date.now() < annotDeadline) {
        annotDump = String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
        annots = parseMarkupDump(annotDump);
        if (annots.some((a) => a.type === "Underline" && a.rects.length > 0)) {
          break;
        }
        await sleep(25);
      }

      const ul = annots.find((a) => a.type === "Underline");
      if (!ul || ul.rects.length === 0) {
        throw new Error(`no underline annotation\n${annotDump}`);
      }
      const rect = ul.rects[0]!;
      // QuadPoints are in fitz page space (origin top-left, y down). MuPDF
      // draws the underline at 1/7 of the box height from the bottom.
      const lineFitz = rect.y + (rect.dy * 6) / 7;
      const linePdf = PAGE_H - lineFitz;
      if (!(linePdf < BASELINE_PDF_Y)) {
        throw new Error(
          `underline line at PDF y=${linePdf.toFixed(2)} is not below baseline ${BASELINE_PDF_Y} ` +
            `(rect=${rect.x},${rect.y},${rect.dx},${rect.dy})\n${annotDump}`,
        );
      }
      console.log(`issue-6023: underline line PDF y=${linePdf.toFixed(2)} (baseline ${BASELINE_PDF_Y})`);
      // drop the unsaved annot so Quit is not blocked by the save dialog
      sendCommandSync(frame, cmdId("CmdDiscardChanges"));
      await client.waitForRenderIdle();
    },
    [pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
