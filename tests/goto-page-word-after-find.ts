// Crash 2026-10-03-07-24-4189: after a find thread finishes, dm->textSearch
// still holds a progress callback bound to the freed FindThreadData. The next
// DDE [GotoPageWord] search calls it and reads the freed window.
//
// A debug report under -for-testing exits with code 105. ASan aborts on the
// same read. The find-match list only proves the find thread has finished
// (and therefore deleted its FindThreadData) before GotoPageWord runs.

import { writeFileSync } from "node:fs";
import { ControlCommand, type ControlClient } from "./control.ts";
import { runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sendCopyDataW, sleep } from "./winapi.ts";

const kCopyDataDdeW = 0x44646557;
const kWord = "needle";

// one page, one line, so GotoPageWord has text to search after the find ends
function buildPdf(): Buffer {
  const content = `BT /F1 24 Tf 72 720 Td (${kWord}) Tj ET`;
  const objs = [
    "",
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [4 0 R] /Count 1 >>",
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> /Contents 5 0 R >>",
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
  ];
  let pdf = "%PDF-1.4\n";
  const offsets: number[] = [];
  for (let i = 1; i < objs.length; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${i} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefPos = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${objs.length}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    pdf += off.toString().padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${objs.length} /Root 1 0 R >>\nstartxref\n${xrefPos}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

async function findWindow(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestFindWindowContents);
  return String(res[1] ?? "");
}

async function waitForMatch(client: ControlClient): Promise<void> {
  const deadline = Date.now() + 15000 * SLOW_BUILD_FACTOR;
  let last = "";
  while (Date.now() < deadline) {
    last = await findWindow(client);
    if (last.includes(kWord)) {
      return;
    }
    if (last.includes("ERROR")) {
      throw new Error(`goto-page-word-after-find: find produced no match: ${last.trim()}`);
    }
    await sleep(40);
  }
  throw new Error(`goto-page-word-after-find: find did not finish: ${last.trim()}`);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("goto-page-word-after-find.pdf");
  writeFileSync(pdf, buildPdf());
  const { proc, client, frame } = await launchControlled(["-search", kWord, pdf]);
  try {
    await client.waitForRenderIdle();
    await waitForMatch(client);

    const goto = sendCopyDataW(frame, kCopyDataDdeW, `[GotoPageWord("${pdf}",1,"${kWord}")]`);
    if (goto !== 1n || proc.exitCode !== null) {
      throw new Error(`goto-page-word-after-find: GotoPageWord failed (result ${goto}, exit ${proc.exitCode})`);
    }
    // one more round-trip: the process is still answering after the search
    await client.chapterInfo();
  } finally {
    if (proc.exitCode === null) {
      try {
        await client.quit();
      } catch {
        await killAndWait(proc);
      }
    }
  }
  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    throw new Error(`goto-page-word-after-find: exit code ${exitCode}, want 0 (105 = debug report)`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
