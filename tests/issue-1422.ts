// Regression test for issue #1422: zooming in a mixed-size document
// (portrait pages plus a much wider landscape page) must not shrink the
// page you are looking at.
//
// Fit Width gives each page its own scale. The first "+" used to step from
// the *smallest* of those (the landscape page), so a portrait page jumped
// down to 66.67% (or similar) before later steps zoomed in normally.

import { writeFileSync } from "node:fs";
import { ControlClient, withControlledSumatra } from "./control.ts";
import { sendCommandSync, waitForFrame } from "./win-automation.ts";
import { cmdId, EXE, runStandalone, tmpPath } from "./util.ts";

const PAGE_COUNT = 4;
const WIDE_PAGE = 3;

function buildPdf(): Buffer {
  const objs: string[] = [];
  objs[1] = "<< /Type /Catalog /Pages 2 0 R >>";
  objs[3] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";
  const kids: number[] = [];
  let objNum = 4;
  for (let page = 1; page <= PAGE_COUNT; page++) {
    const pageNum = objNum++;
    const contentNum = objNum++;
    kids.push(pageNum);
    const wide = page === WIDE_PAGE;
    const box = wide ? "[0 0 1224 792]" : "[0 0 612 792]";
    const content = `BT /F1 24 Tf 72 720 Td (page ${page}) Tj ET`;
    objs[pageNum] =
      `<< /Type /Page /Parent 2 0 R /MediaBox ${box} ` +
      `/Resources << /Font << /F1 3 0 R >> >> /Contents ${contentNum} 0 R >>`;
    objs[contentNum] = `<< /Length ${content.length} >>\nstream\n${content}\nendstream`;
  }
  objs[2] = `<< /Type /Pages /Kids [${kids.map((k) => `${k} 0 R`).join(" ")}] /Count ${PAGE_COUNT} >>`;
  const maxN = objNum - 1;

  let pdf = "%PDF-1.5\n";
  const offsets: number[] = [];
  for (let i = 1; i <= maxN; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${i} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefPos = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${maxN + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    pdf += off.toString().padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${xrefPos}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

function parseZoomR(info: string): number {
  const m = /zoomR=([\d.]+)/.exec(info);
  if (!m) {
    throw new Error(`issue-1422: no zoomR in '${info}'`);
  }
  return parseFloat(m[1]);
}

async function pageZoom(client: ControlClient): Promise<number> {
  return parseZoomR(await client.waitForRenderIdle());
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-1422.pdf");
  writeFileSync(pdfPath, buildPdf());

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      if (!frame) {
        throw new Error("issue-1422: no frame");
      }
      await client.setNotificationsEnabled(false);
      const before = await pageZoom(client);
      sendCommandSync(frame, cmdId("CmdZoomIn"));
      const after = await pageZoom(client);
      if (!(after > before)) {
        throw new Error(`issue-1422: first Zoom In shrank the current page (fit-width zoomR ${before} -> ${after})`);
      }
      sendCommandSync(frame, cmdId("CmdZoomIn"));
      const after2 = await pageZoom(client);
      if (!(after2 > after)) {
        throw new Error(`issue-1422: second Zoom In did not increase zoom (zoomR ${after} -> ${after2})`);
      }
      console.log(`issue-1422: zoomR ${before} -> ${after} -> ${after2}`);
    },
    ["-view", "continuous", "-zoom", "fit width", pdfPath],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
