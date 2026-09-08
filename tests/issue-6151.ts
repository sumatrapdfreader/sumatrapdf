// #6151: settled tiles stay sharp. #6152: zoom out escapes Fit Width.

import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { findCanvas, sendCommandSync, waitForFrame } from "./win-automation.ts";
import { captureWindowPixels } from "./winapi.ts";
import { assemblePdf, cmdId, EXE, runStandalone, tmpPath } from "./util.ts";

const PAGE_COUNT = 4;
const WIDE_PAGE = 3;
const ZOOM_OUT_STEPS = 12;

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
    throw new Error(`issue-6151: no zoomR in '${info}'`);
  }
  return parseFloat(m[1]);
}

async function testZoomSteps(): Promise<void> {
  const pdfPath = tmpPath("issue-6151.pdf");
  writeFileSync(pdfPath, buildPdf());

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      if (!frame) {
        throw new Error("issue-6151: no frame");
      }
      await client.setNotificationsEnabled(false);
      const start = parseZoomR(await client.waitForRenderIdle());
      const zooms: number[] = [start];
      for (let i = 0; i < ZOOM_OUT_STEPS; i++) {
        sendCommandSync(frame, cmdId("CmdZoomOut"));
        const zoom = parseZoomR(await client.waitForRenderIdle());
        if (zoom > zooms[zooms.length - 1]!) {
          throw new Error(`issue-6152: zoom out increased scale: ${zooms.join(" -> ")} -> ${zoom}`);
        }
        zooms.push(zoom);
      }
      const end = zooms[zooms.length - 1]!;
      if (!(end < start * 0.5)) {
        throw new Error(
          `issue-6151: zoom out from Fit Width stuck (start ${start} -> ${zooms.join(" -> ")}); expected to reach below half`,
        );
      }
      console.log(`issue-6151: zoomR ${zooms.join(" -> ")}`);
    },
    ["-view", "continuous", "-zoom", "fit width", pdfPath],
  );
}

async function testSharpTiles(): Promise<void> {
  const pdf = tmpPath("issue-6151-sharp.pdf");
  const checkerStep = 12;
  const checkerInset = 12.3;
  let content = "0 g\n";
  for (let y = checkerInset; y < 792 - checkerStep; y += checkerStep) {
    for (let x = checkerInset; x < 612 - checkerStep; x += checkerStep) {
      content += `${x.toFixed(1)} ${y.toFixed(1)} ${checkerStep / 2} ${checkerStep / 2} re f\n`;
    }
  }
  // Keep the bitmap in full color: an indexed black/white palette hides interpolation.
  const colorSteps = 16;
  for (let r = 0; r <= colorSteps; r++) {
    for (let g = 0; g <= colorSteps; g++) {
      content += `${r / colorSteps} ${g / colorSteps} 0 rg ${30 + r} ${30 + g} 1 1 re f\n`;
    }
  }
  writeFileSync(
    pdf,
    assemblePdf([
      "<< /Type /Catalog /Pages 2 0 R >>",
      "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> /Contents 4 0 R >>",
      `<< /Length ${content.length} >>\nstream\n${content}endstream`,
    ]),
  );
  const appdata = tmpPath("issue-6151-settings");
  mkdirSync(appdata, { recursive: true });
  // Without rasterizer AA, intermediate grays can only come from bitmap scaling.
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), "DisableAntiAlias = true\nCustomScreenDPI = 96\n");

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      if (!frame) {
        throw new Error("issue-6151: no frame");
      }
      await client.setNotificationsEnabled(false);
      for (const command of [
        null,
        "CmdZoom800",
        "CmdZoom125",
        "CmdRotateRight",
        "CmdRotateRight",
        "CmdRotateRight",
        "CmdToggleTrimEmptyMargins",
        "CmdZoom800",
        "CmdZoom125",
      ]) {
        if (command) {
          sendCommandSync(frame, cmdId(command));
        }
        const info = await client.waitForRenderIdle();
        if (command === "CmdZoom800" && !/res=[1-9]/.test(info)) {
          throw new Error(`issue-6151: expected tiled rendering at 800%: ${info}`);
        }
        const [code, raw] = await client.request(ControlCommand.TestLayout, ["get"]);
        const page = /page n=1 shown=\d+ pos=[^\n]+? screen=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(String(raw));
        const px = captureWindowPixels(findCanvas(frame));
        if (code !== 0 || !page || !px) {
          throw new Error(`issue-6151: cannot capture page: ${raw}`);
        }
        const [left, top, width, height] = page.slice(1).map(Number);
        const inset = 4;
        let black = 0,
          white = 0,
          gray = 0;
        for (let y = Math.max(inset, top + inset); y < Math.min(px.h - inset, top + height - inset); y++) {
          for (let x = Math.max(inset, left + inset); x < Math.min(px.w - inset, left + width - inset); x++) {
            const offset = (y * px.w + x) * 4;
            const v = px.data[offset];
            if (v !== px.data[offset + 1] || v !== px.data[offset + 2]) continue;
            if (v === 0) black++;
            else if (v === 255) white++;
            else gray++;
          }
        }
        const minColorPixels = 100;
        if (black < minColorPixels || white < minColorPixels || gray !== 0) {
          throw new Error(
            `issue-6151: ${command ?? "initial 125%"}: black=${black} white=${white} gray=${gray}; ${info}`,
          );
        }
      }
      console.log("issue-6151: settled tiles stay sharp through zoom, rotation and margin trimming");
    },
    ["-appdata", appdata, "-view", "single page", "-zoom", "125", pdf],
  );
}

export async function testit(): Promise<void> {
  await testSharpTiles();
  await testZoomSteps();
}

if (import.meta.main) {
  await runStandalone(testit);
}
