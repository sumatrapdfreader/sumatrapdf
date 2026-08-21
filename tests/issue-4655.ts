// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/4655
//
// A non-embedded Type1 font named "YFLGZT+Symbol" (PDF subset tag + Symbol) with
// Encoding Differences for the math-bracket pieces (bracketlefttp/ex/bt) used
// to substitute Helvetica, so matrix square brackets never drew. pdf_clean_font_name
// must strip the ABCDEF+ tag and load the builtin Symbol font.
//
// Fixture: tests/issue-4655.pdf. Regenerate by writing makePdf() there.
//
// Run: bun tests/issue-4655.ts [--no-build]

import { existsSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { captureWindowPixels, captureWindowToPng } from "./winapi.ts";
import { findCanvas, sendCommandSync, waitForFrame } from "./win-automation.ts";
import { cmdId, EXE, runStandalone, tmpPath } from "./util.ts";

const PDF = join(import.meta.dir, "issue-4655.pdf");

export function makePdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  // stacked left-bracket pieces from Adobe Symbol: top, extender, bottom
  const stream = [
    "BT",
    "/Helv 14 Tf",
    "1 0 0 1 140 270 Tm",
    "(OK) Tj",
    "/Sym 72 Tf",
    "1 0 0 1 20 210 Tm",
    "(\\351) Tj",
    "1 0 0 1 20 155 Tm",
    "(\\352) Tj",
    "1 0 0 1 20 100 Tm",
    "(\\353) Tj",
    "ET",
  ].join("\n");
  const body: Record<number, Buffer> = {
    1: enc("<< /Type /Catalog /Pages 2 0 R >>"),
    2: enc("<< /Type /Pages /Kids [3 0 R] /Count 1 >>"),
    3: enc(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 300] ` +
        `/Resources << /Font << /Helv 4 0 R /Sym 5 0 R >> >> /Contents 7 0 R >>`,
    ),
    4: enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>"),
    5: enc(
      "<< /Type /Font /Subtype /Type1 /BaseFont /YFLGZT+Symbol /FirstChar 233 /LastChar 235 " +
        "/Encoding 6 0 R /FontDescriptor 8 0 R /Widths [333 333 333] >>",
    ),
    6: enc("<< /Type /Encoding /Differences [233 /bracketlefttp 234 /bracketleftex 235 /bracketleftbt] >>"),
    7: enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`),
    8: enc(
      "<< /Type /FontDescriptor /FontName /Symbol /Flags 32 /ItalicAngle 0 " +
        "/Ascent 709 /Descent -241 /CapHeight 674 /StemV 91 /FontBBox [-180 -293 1090 1010] >>",
    ),
  };
  const parts: Buffer[] = [enc("%PDF-1.4\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0].length;
  for (let n = 1; n <= 8; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj\n`), body[n]!, enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  let xref = `xref\n0 9\n0000000000 65535 f \n`;
  for (let n = 1; n <= 8; n++) {
    xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size 9 /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

function isDark(b: number, g: number, r: number): boolean {
  return b < 80 && g < 80 && r < 80;
}

export async function testit(): Promise<void> {
  if (!existsSync(PDF)) {
    throw new Error(`issue-4655: fixture not found: ${PDF}`);
  }

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      if (!frame) {
        throw new Error("issue-4655: no frame");
      }
      await client.request(ControlCommand.SetNotificationsEnabled, [0]);
      await client.waitForRenderIdle();
      sendCommandSync(frame, cmdId("CmdZoomFitPage"));
      await client.waitForRenderIdle();

      const canvas = findCanvas(frame);
      if (!canvas) {
        throw new Error("issue-4655: no canvas");
      }
      captureWindowToPng(canvas, tmpPath("issue-4655.png"));
      const shot = captureWindowPixels(canvas);
      if (!shot) {
        throw new Error("issue-4655: capture failed");
      }

      // left half of the page holds the stacked Symbol bracket pieces; Helvetica
      // "OK" is on the right. A failed Symbol substitute leaves that half blank.
      const x1 = Math.floor(shot.w * 0.45);
      let n = 0;
      for (let y = 0; y < shot.h; y++) {
        for (let x = 0; x < x1; x++) {
          const i = (y * shot.w + x) * 4;
          if (isDark(shot.data[i]!, shot.data[i + 1]!, shot.data[i + 2]!)) {
            n++;
          }
        }
      }
      if (n < 80) {
        throw new Error(
          `issue-4655: matrix bracket pieces did not draw (${n} dark pixels in left 45% of ${shot.w}x${shot.h})`,
        );
      }
    },
    [PDF],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
