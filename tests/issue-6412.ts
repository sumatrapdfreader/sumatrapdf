// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6412
//
// A DDE [Search("<file>","<term>")] sent to a running instance must be handled.
// The parser expected "]" right after the term's closing quote instead of ")]",
// so every search was dropped. Covers an ASCII term, a non-ASCII one and one
// with a quote escaped as "".

import { writeFileSync } from "node:fs";
import { ControlCommand, type ControlClient } from "./control.ts";
import { EXE, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sendCopyDataW, sleep } from "./winapi.ts";

const kCopyDataDdeW = 0x44646557;

// one line of WinAnsi text: \351 is "é"
function buildPdf(): Buffer {
  const content = `BT /F1 24 Tf 72 720 Td (needle caf\\351 say "hi" end) Tj ET`;
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

async function waitForMatch(client: ControlClient, term: string): Promise<void> {
  const deadline = Date.now() + 15000 * SLOW_BUILD_FACTOR;
  let last = "";
  while (Date.now() < deadline) {
    last = String((await client.request(ControlCommand.TestFindWindowContents))[1] ?? "");
    if (last.includes(term)) {
      return;
    }
    if (last.includes("ERROR")) {
      break;
    }
    await sleep(40);
  }
  throw new Error(`issue-6412: no match for '${term}': ${last.trim()}`);
}

// a second process forwards -search to the running instance, building [Search] itself
async function searchFromSecondProcess(pdf: string, term: string): Promise<void> {
  // its exit code is 1 even when the forwarding worked
  await Bun.spawn([EXE, "-reuse-instance", "-search", term, pdf]).exited;
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6412.pdf");
  writeFileSync(pdf, buildPdf());
  const terms = ["needle", "café", `"hi"`];
  for (const viaCmdLine of [false, true]) {
    for (const term of terms) {
      await searchOnce(pdf, term, viaCmdLine);
    }
  }
}

async function searchOnce(pdf: string, term: string, viaCmdLine: boolean): Promise<void> {
  {
    const { proc, client, frame } = await launchControlled([pdf]);
    try {
      await client.waitForRenderIdle();
      if (viaCmdLine) {
        await searchFromSecondProcess(pdf, term);
      } else {
        const escaped = term.replaceAll('"', '""');
        const res = sendCopyDataW(frame, kCopyDataDdeW, `[Search("${pdf}","${escaped}")]`);
        if (res !== 1n) {
          throw new Error(`issue-6412: [Search] with '${term}' not handled (result ${res})`);
        }
      }
      await waitForMatch(client, term);
    } finally {
      try {
        await client.quit();
      } catch {
        await killAndWait(proc);
      }
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
