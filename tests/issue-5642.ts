// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5642
//
// Clicking a link in one PDF that targets a NAMED destination in another PDF
// (a GoToR action) should open the target at that destination -- not page 1.
//
// MuPDF gives the remote link's destination name as "nameddest=<name>" (the URI
// fragment), but EngineBase::GetNamedDest prepends "#nameddest=" itself, so the
// name must be stripped first (CleanRemoteDestName). Without the strip the lookup
// becomes "#nameddest=nameddest=<name>" and fails, leaving the target on page 1.
//
// Builds a PDF with a named destination "ch2" on page 2 and resolves it via the
// control pipe named-destination command (which runs the same
// CleanRemoteDestName + GetNamedDest path LinkHandler::LaunchFile uses):
//   - "nameddest=ch2" (the form a remote link provides) -> page 2   <- the fix
//   - "ch2"           (already-clean name)               -> page 2
//   - "nope"          (unknown)                          -> NOTFOUND
// Before the fix, "nameddest=ch2" resolved to NOTFOUND and would fail here.
//
// Run:  bun tests/issue-5642.ts [--no-build]   (or via tests/all.ts)

import { existsSync, writeFileSync, rmSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { EXE, runStandalone } from "./util.ts";
import { ControlCommand, runControlCommand } from "../cmd/control.ts";

const PDF = join(tmpdir(), "sumatra-issue-5642.pdf");

// minimal 2-page PDF with a named destination "ch2" -> page 2 (in /Names/Dests)
function makePdf(): Buffer {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R /Names 5 0 R >>`,
    `<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
    `<< /Dests 6 0 R >>`,
    `<< /Names [(ch2) [4 0 R /XYZ 100 700 0]] >>`,
  ];
  let pdf = "%PDF-1.7\n%\xe2\xe3\xcf\xd3\n";
  const off: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    off[i] = Buffer.byteLength(pdf, "latin1");
    pdf += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xref = Buffer.byteLength(pdf, "latin1");
  const n = objs.length + 1;
  pdf += `xref\n0 ${n}\n0000000000 65535 f \n`;
  for (let i = 0; i < objs.length; i++) {
    pdf += off[i].toString().padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${n} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

// resolves a named destination, returns the 1-based page or 0 if not found
async function resolve(name: string): Promise<{ page: number; raw: string }> {
  const [, rawArg] = await runControlCommand(EXE, ControlCommand.TestNamedDest, [PDF, name]);
  const raw = String(rawArg).trim();
  const m = raw.match(/page=(-?\d+)/);
  return { page: m ? parseInt(m[1]) : 0, raw };
}

export async function testit(): Promise<void> {
  if (!existsSync(EXE)) {
    throw new Error(`app not found: ${EXE} (build first)`);
  }
  writeFileSync(PDF, makePdf());

  // [name, expectedPage (0 = NOTFOUND), description]
  const cases: [string, number, string][] = [
    ["nameddest=ch2", 2, "remote-link form (the #5642 fix strips the prefix)"],
    ["ch2", 2, "already-clean name"],
    ["nope", 0, "unknown name -> NOTFOUND"],
  ];

  let allOk = true;
  for (const [name, expectPage, desc] of cases) {
    const r = await resolve(name);
    const ok = r.page === expectPage;
    allOk &&= ok;
    console.log(`  ${ok ? "✅" : "❌"} "${name}": ${desc}`);
    console.log(`        -> ${r.raw}  (expected ${expectPage ? `page ${expectPage}` : "NOTFOUND"})`);
  }

  rmSync(PDF, { force: true });
  if (!allOk) {
    throw new Error("remote named-destination resolution is wrong");
  }
  console.log("✅ remote named destinations resolve to the right page (issue #5642 fixed)");
}

if (import.meta.main) {
  await runStandalone(testit);
}
