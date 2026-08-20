// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6005
//
// The Trim page-box label sat below the box, so on a single (or last) page at
// Fit Page it was clipped at the canvas edge. It must stay inside the viewport.
//
// Run: bun tests/issue-6005.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { captureWindowPixels, captureWindowToPng } from "./winapi.ts";
import { findCanvas, sendCommandSync, waitForFrame } from "./win-automation.ts";
import { cmdId, EXE, runStandalone, tmpPath } from "./util.ts";

function makePdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const body: Record<number, Buffer> = {
    1: enc("<< /Type /Catalog /Pages 2 0 R >>"),
    2: enc("<< /Type /Pages /Kids [3 0 R] /Count 1 >>"),
    3: enc(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /TrimBox [0 0 595 842] ` +
        `/Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>`,
    ),
    4: enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>"),
    5: enc("<< /Length 47 >>\nstream\nq BT/F1 24 Tf 200 800 TD (Hello World!)Tj ET Q\nendstream"),
  };
  const parts: Buffer[] = [enc("%PDF-1.7\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0].length;
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

function isTrimGreen(b: number, g: number, r: number): boolean {
  return Math.abs(r - 0x10) <= 24 && Math.abs(g - 0x90) <= 24 && Math.abs(b - 0x20) <= 24;
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6005.pdf");
  writeFileSync(pdf, makePdf());

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      if (!frame) {
        throw new Error("issue-6005: no frame");
      }
      await client.request(ControlCommand.SetNotificationsEnabled, [0]);
      await client.waitForRenderIdle();
      sendCommandSync(frame, cmdId("CmdZoomFitPage"));
      sendCommandSync(frame, cmdId("CmdTogglePageBoxes"));
      await client.waitForRenderIdle();

      const canvas = findCanvas(frame);
      if (!canvas) {
        throw new Error("issue-6005: no canvas");
      }
      captureWindowToPng(canvas, tmpPath("issue-6005.png"));
      const shot = captureWindowPixels(canvas);
      if (!shot) {
        throw new Error("issue-6005: capture failed");
      }

      const band = 3;
      const right = 80;
      let n = 0;
      const x0 = Math.max(0, shot.w - right);
      for (let y = shot.h - band; y < shot.h; y++) {
        for (let x = x0; x < shot.w; x++) {
          const i = (y * shot.w + x) * 4;
          if (isTrimGreen(shot.data[i]!, shot.data[i + 1]!, shot.data[i + 2]!)) {
            n++;
          }
        }
      }
      if (n > 8) {
        throw new Error(
          `issue-6005: trim label clipped at canvas bottom (${n} green pixels in last ${band} rows, ${shot.w}x${shot.h})`,
        );
      }
    },
    [pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
