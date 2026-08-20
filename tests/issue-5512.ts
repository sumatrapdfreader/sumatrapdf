// #5512: percentage zoom can normalize mixed-size pages to one displayed width.

import { writeFileSync } from "node:fs";
import { ControlClient, withControlledSumatra } from "./control.ts";
import { sendCommandSync, waitForFrame } from "./win-automation.ts";
import { cmdId, EXE, runStandalone, tmpPath } from "./util.ts";

const PAGE_WIDTHS = [612, 1224, 306];

function buildPdf(): Buffer {
  const objs: string[] = [];
  objs[1] = "<< /Type /Catalog /Pages 2 0 R >>";
  const kids: number[] = [];
  let objNum = 3;
  for (const width of PAGE_WIDTHS) {
    kids.push(objNum);
    objs[objNum++] = `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ${width} 792] /Resources << >> >>`;
  }
  objs[2] = `<< /Type /Pages /Kids [${kids.map((n) => `${n} 0 R`).join(" ")}] /Count ${kids.length} >>`;

  let pdf = "%PDF-1.5\n";
  const offsets: number[] = [];
  for (let i = 1; i < objNum; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${i} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefPos = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${objNum}\n0000000000 65535 f \n`;
  for (const offset of offsets) {
    pdf += `${offset.toString().padStart(10, "0")} 00000 n \n`;
  }
  pdf += `trailer\n<< /Size ${objNum} /Root 1 0 R >>\nstartxref\n${xrefPos}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

function parseZoomReal(info: string): number {
  const match = /zoomR=([\d.]+)/.exec(info);
  if (!match) {
    throw new Error(`issue-5512: no zoomR in '${info}'`);
  }
  return parseFloat(match[1]);
}

async function currentZoom(client: ControlClient): Promise<number> {
  return parseZoomReal(await client.waitForRenderIdle());
}

function expectClose(actual: number, expected: number, what: string): void {
  const tolerance = Math.max(0.01, Math.abs(expected) * 0.002);
  if (Math.abs(actual - expected) > tolerance) {
    throw new Error(`issue-5512: ${what}: expected ${expected.toFixed(3)}, got ${actual.toFixed(3)}`);
  }
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-5512.pdf");
  writeFileSync(pdfPath, buildPdf());

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      if (!frame) {
        throw new Error("issue-5512: no frame");
      }
      await client.setNotificationsEnabled(false);

      const normalZoom = await currentZoom(client);
      sendCommandSync(frame, cmdId("CmdToggleUniformPageWidth"));
      const referenceZoom = await currentZoom(client);
      expectClose(referenceZoom, normalZoom, "toggling changed page 1 zoom");

      for (let page = 2; page <= PAGE_WIDTHS.length; page++) {
        sendCommandSync(frame, cmdId("CmdGoToNextPage"));
        const zoom = await currentZoom(client);
        const expected = referenceZoom * (PAGE_WIDTHS[0] / PAGE_WIDTHS[page - 1]);
        expectClose(zoom, expected, `page ${page} zoom`);
        expectClose(zoom * PAGE_WIDTHS[page - 1], referenceZoom * PAGE_WIDTHS[0], `page ${page} width`);
      }

      sendCommandSync(frame, cmdId("CmdToggleUniformPageWidth"));
      expectClose(await currentZoom(client), normalZoom, "turning uniform width off did not restore normal zoom");
      console.log("issue-5512: mixed-size pages keep one width at percentage zoom");
    },
    ["-view", "single page", "-zoom", "100", pdfPath],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
