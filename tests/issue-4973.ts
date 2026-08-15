// #4973: reloading a document at a remembered non-first page must restore
// that page (and still render it). pauseRendering is used during the swap so
// page 1 is not requested first; this catches leaving it stuck or losing the
// scroll restore.

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, cmdId, runStandalone, tmpPath } from "./util.ts";
import { captureWindowPixels, captureWindowToPng, getClientRect } from "./winapi.ts";
import { findCanvas, sendCommand, waitForFrame } from "./win-automation.ts";

const kPages = 8;
const kRememberedPage = 7;

function buildPdf(pageCount: number): Buffer {
  const fontNum = 3;
  const objs: string[] = [];
  objs[1] = "<< /Type /Catalog /Pages 2 0 R >>";
  objs[fontNum] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";

  const kids: number[] = [];
  let objNum = 4;
  for (let page = 1; page <= pageCount; page++) {
    const pageNum = objNum++;
    const contentNum = objNum++;
    kids.push(pageNum);
    const content = `BT /F1 24 Tf 72 720 Td (Page ${page}) Tj ET`;
    objs[pageNum] =
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
      `/Resources << /Font << /F1 ${fontNum} 0 R >> >> /Contents ${contentNum} 0 R >>`;
    objs[contentNum] = `<< /Length ${content.length} >>\nstream\n${content}\nendstream`;
  }
  objs[2] = `<< /Type /Pages /Kids [${kids.map((k) => `${k} 0 R`).join(" ")}] /Count ${pageCount} >>`;
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

async function currentPage(client: ControlClient): Promise<number> {
  const deadline = Date.now() + 10_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestFavoriteNav, ["page", 0]);
    const raw = String(res[1] ?? "");
    const m = /OK page=(\d+)/.exec(raw);
    if (m) {
      return +m[1]!;
    }
    if (!raw.includes("NOTREADY")) {
      throw new Error(`issue-4973: can't read the current page: ${raw.trim()}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-4973: app never became ready: ${raw.trim()}`);
    }
    await new Promise((r) => setTimeout(r, 50));
  }
}

function countDarkPixels(hwnd: number): number {
  const pix = captureWindowPixels(hwnd);
  if (!pix) {
    return 0;
  }
  let n = 0;
  for (let i = 0; i + 2 < pix.data.length; i += 4) {
    if (pix.data[i] + pix.data[i + 1] + pix.data[i + 2] < 500) {
      n++;
    }
  }
  return n;
}

async function waitForPage(client: ControlClient, want: number, timeoutMs = 10_000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  let page = 0;
  while (Date.now() < deadline) {
    page = await currentPage(client);
    if (page === want) {
      return page;
    }
    await new Promise((r) => setTimeout(r, 50));
  }
  return page;
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-4973.pdf");
  writeFileSync(pdf, buildPdf(kPages));
  const afterPng = tmpPath("issue-4973-after.png");

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const page = await waitForPage(client, kRememberedPage);
      if (page !== kRememberedPage) {
        throw new Error(`issue-4973: opened at page ${page}, want ${kRememberedPage}`);
      }

      const frame = await waitForFrame(proc.pid!);
      sendCommand(frame, cmdId("CmdReloadDocument"));

      const after = await waitForPage(client, kRememberedPage);
      if (after !== kRememberedPage) {
        throw new Error(`issue-4973: after reload page=${after}, want ${kRememberedPage}`);
      }

      await client.waitForRenderIdle();
      const canvas = findCanvas(frame);
      if (!canvas) {
        throw new Error("issue-4973: no canvas after reload");
      }
      captureWindowToPng(canvas, afterPng);
      const ink = countDarkPixels(canvas);
      // the fixture is one line of text, so "not blank" is a small number of
      // dark pixels - and how small depends on how big the page is drawn, so
      // scale with the canvas rather than hard-coding a count for one window size
      const cr = getClientRect(canvas);
      const canvasPx = (cr.right - cr.left) * (cr.bottom - cr.top);
      const minInk = Math.max(40, Math.floor(canvasPx / 10000));
      if (ink < minInk) {
        throw new Error(`issue-4973: canvas looks empty after reload (dark pixels=${ink}, want >= ${minInk})`);
      }
    },
    ["-page", String(kRememberedPage), pdf],
  );

  console.log(`issue-4973: reload kept page ${kRememberedPage} and the canvas painted`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
