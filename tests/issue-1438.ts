// Regression test for issue #1438: the saved scroll position is lost when the
// document is zoomed in.
//
// The document that shows it has one page much wider than the rest (the
// "iceberg" shape of #1422). The canvas is then as wide as that page and the
// narrow pages sit centered in it, so at scroll x = 0 the viewport overlaps no
// page at all. "Which page am I on" used to answer "the last one" in that case
// (anything below page 1), and restoring a saved position went through it:
// the view jumped to the end of the document and the position came back as the
// top of page 1. The next exit then saved *that*, so the position was lost for
// good - exactly what the reporter kept seeing.
//
// The test drives the real app twice with its own -appdata directory: session 1
// zooms in, goes to a page and exits (which saves the state); session 2 reopens
// and exits without touching anything. The state session 2 saves must be the
// one session 1 saved.

import { writeFileSync, mkdirSync, rmSync, readFileSync, existsSync } from "node:fs";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { launchControlled, sendCommandSync, waitForExit, killAndWait } from "./win-automation.ts";

const PAGE_COUNT = 12;
const WIDE_PAGE = 3; // 10x wider than the others
const GO_TO_PAGE = 9;

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
    const box = wide ? "[0 0 6120 792]" : "[0 0 612 792]";
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

// PageNo / ScrollPos of the one file in the settings file
function savedState(settingsPath: string): string {
  if (!existsSync(settingsPath)) {
    throw new Error("issue-1438: no settings file was written");
  }
  const s = readFileSync(settingsPath, "latin1");
  const i = s.indexOf("FileStates [");
  if (i < 0) {
    throw new Error("issue-1438: settings file has no FileStates");
  }
  const lines = s
    .slice(i)
    .split("\n")
    .map((l) => l.trim())
    .filter((l) => /^(PageNo|ScrollPos) =/.test(l));
  return lines.slice(0, 2).sort().join(" ");
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-1438.pdf");
  writeFileSync(pdfPath, buildPdf());
  const appDataDir = tmpPath("issue-1438-appdata");
  rmSync(appDataDir, { recursive: true, force: true });
  mkdirSync(appDataDir, { recursive: true });
  const settingsPath = `${appDataDir}/SumatraPDF-settings.txt`;

  // no -for-testing: this test reads the position the app writes into settings
  async function session(act: (frame: number) => void): Promise<void> {
    const { proc, client, frame } = await launchControlled(["-appdata", appDataDir, pdfPath], { saveSettings: true });
    try {
      await client.waitForRenderIdle();
      act(frame);
      sendCommandSync(frame, cmdId("CmdExit"));
      if (!(await waitForExit(proc))) {
        throw new Error("issue-1438: SumatraPDF didn't exit after CmdExit");
      }
    } finally {
      client.close();
      await killAndWait(proc);
    }
  }

  // session 1: zoom in, go to a page well into the document, exit
  await session((frame) => {
    sendCommandSync(frame, cmdId("CmdZoom200"));
    for (let i = 1; i < GO_TO_PAGE; i++) {
      sendCommandSync(frame, cmdId("CmdGoToNextPage"));
    }
    // scroll inside the page too: the position to restore is then a point
    // within page 9, not just "the top of page 9"
    for (let i = 0; i < 5; i++) {
      sendCommandSync(frame, cmdId("CmdScrollDown"));
    }
  });
  const want = savedState(settingsPath);
  if (!/PageNo = 9/.test(want)) {
    throw new Error(`issue-1438: session 1 did not end up on page ${GO_TO_PAGE}: ${want}`);
  }

  // session 2: reopen (restoring that position) and exit without touching it
  await session(() => {});
  const got = savedState(settingsPath);
  if (got !== want) {
    throw new Error(`issue-1438: reopening lost the position: saved '${want}', got '${got}'`);
  }
  console.log(`issue-1438: ${got}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
