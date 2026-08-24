// #6046: Extract Pages can restrict a page range to pages with annotations.
//
// Run: bun tests/issue-6046.ts [--no-build]

import { existsSync, readFileSync, rmSync, statSync, writeFileSync } from "node:fs";
import { pdfPageCount } from "./print-util.ts";
import { cmdId, extractPageText, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, pressEnter, sendCommand } from "./win-automation.ts";
import {
  enumChildWindows,
  enumWindows,
  getClassName,
  getControlText,
  getWindowPid,
  getWindowText,
  packCoords,
  sendMessage,
  sendText,
  sleep,
  WM_COMMAND,
} from "./winapi.ts";

const BM_CLICK = 0x00f5;
const annotatedPages = [2, 4];
const expectedPages = [1, ...annotatedPages];

function buildPdf(): Buffer {
  const objects: string[] = [];
  objects[1] = "<< /Type /Catalog /Pages 2 0 R >>";
  objects[3] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";

  const pageObjects: number[] = [];
  let nextObject = 4;
  for (let pageNo = 1; pageNo <= 4; pageNo++) {
    const pageObject = nextObject++;
    const contentObject = nextObject++;
    const hasAnnotation = annotatedPages.includes(pageNo);
    const annotationObject = hasAnnotation ? nextObject++ : 0;
    pageObjects.push(pageObject);

    const text = `Page ${pageNo}`;
    const content = `BT /F1 24 Tf 72 720 Td (${text}) Tj ET`;
    const annots = hasAnnotation ? ` /Annots [${annotationObject} 0 R]` : "";
    objects[pageObject] =
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792]` +
      ` /Resources << /Font << /F1 3 0 R >> >> /Contents ${contentObject} 0 R${annots} >>`;
    objects[contentObject] = `<< /Length ${content.length} >>\nstream\n${content}\nendstream`;
    if (hasAnnotation) {
      objects[annotationObject] =
        `<< /Type /Annot /Subtype /Text /Rect [72 640 96 664] /P ${pageObject} 0 R` +
        ` /Contents (Annotation on page ${pageNo}) >>`;
    }
  }
  objects[2] =
    `<< /Type /Pages /Kids [${pageObjects.map((pageObject) => `${pageObject} 0 R`).join(" ")}]` +
    ` /Count ${pageObjects.length} >>`;

  let pdf = "%PDF-1.5\n";
  const offsets: number[] = [];
  for (let objectNumber = 1; objectNumber < nextObject; objectNumber++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${objectNumber} 0 obj\n${objects[objectNumber]}\nendobj\n`;
  }
  const xrefOffset = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${nextObject}\n0000000000 65535 f \n`;
  for (const offset of offsets) {
    pdf += `${offset.toString().padStart(10, "0")} 00000 n \n`;
  }
  pdf += `trailer\n<< /Size ${nextObject} /Root 1 0 R >>\nstartxref\n${xrefOffset}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

function findExtractDialog(pid: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) === pid && getWindowText(hwnd) === "Extract Pages From PDF") {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

async function waitForExtractDialog(pid: number, timeoutMs = 5000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const dialog = findExtractDialog(pid);
    if (dialog) {
      return dialog;
    }
    await sleep(30);
  }
  throw new Error("issue-6046: Extract Pages dialog did not appear");
}

function childWindowsByClass(parent: number, className: string): number[] {
  const children: number[] = [];
  enumChildWindows(parent, (hwnd) => {
    if (getClassName(hwnd) === className) {
      children.push(hwnd);
    }
    return true;
  });
  return children;
}

async function waitForOutput(path: string, timeoutMs = 10000): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  let previousSize = -1;
  while (Date.now() < deadline) {
    if (existsSync(path)) {
      const size = statSync(path).size;
      if (size > 0 && size === previousSize) {
        return;
      }
      previousSize = size;
    }
    await sleep(50);
  }
  throw new Error("issue-6046: extracted PDF was not created");
}

export async function testit(): Promise<void> {
  const source = tmpPath("issue-6046.pdf");
  const output = tmpPath("issue-6046-extracted.pdf");
  writeFileSync(source, buildPdf());
  rmSync(output, { force: true });

  const { proc, client, frame } = await launchControlled([source]);
  try {
    await client.waitForRenderIdle();
    // Add an unsaved annotation on page 1. Filtering must see it, and the
    // extracted PDF must retain it instead of reopening only the disk copy.
    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotStamp"), packCoords(80, 80));
    await client.waitForRenderIdle();

    sendCommand(frame, cmdId("CmdPdfExtractPages"));
    const dialog = await waitForExtractDialog(proc.pid!);

    const edits = childWindowsByClass(dialog, "Edit");
    const destEdit = edits.find((hwnd) => /\.pdf$/i.test(getControlText(hwnd)));
    const pagesEdit = edits.find((hwnd) => hwnd !== destEdit);
    if (!destEdit || !pagesEdit) {
      throw new Error(`issue-6046: expected destination and pages edits, got ${edits.length}`);
    }
    sendText(destEdit, output);
    sendText(pagesEdit, "1-N");

    const checkbox = childWindowsByClass(dialog, "Button").find(
      (hwnd) => getControlText(hwnd).replace(/&/g, "") === "Only with annotations",
    );
    if (!checkbox) {
      throw new Error("issue-6046: Only with annotations checkbox not found");
    }
    sendMessage(checkbox, BM_CLICK, 0, 0);
    await pressEnter(dialog);
    await waitForOutput(output);

    const count = pdfPageCount(output);
    if (count !== expectedPages.length) {
      throw new Error(`issue-6046: extracted ${count} pages, expected ${expectedPages.length}`);
    }
    const text = extractPageText(output);
    if (!text.includes("Page 1") || !text.includes("Page 2") || !text.includes("Page 4") || text.includes("Page 3")) {
      throw new Error(`issue-6046: wrong pages extracted: ${JSON.stringify(text)}`);
    }
    if (!/\/Subtype\s*\/Stamp/.test(readFileSync(output, "latin1"))) {
      throw new Error("issue-6046: unsaved annotation was not retained in extracted PDF");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
