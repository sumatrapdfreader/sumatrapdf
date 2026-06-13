// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5537
//
// A PDF /XYZ destination with a zoom of 0 (or null) must mean "retain the
// current zoom" (PDF spec: "A zoom value of 0 has the same meaning as a null
// value"), NOT force 100%. SumatraPDF resolves such a destination to zoom 0,
// which DisplayModel::ScrollTo treats as "don't change zoom".
//
// This builds a tiny PDF whose outline has three /XYZ destinations and checks
// each via the headless `-test-dest <pdf> <no> <outfile>` flag:
//   1. /XYZ 100 700 0    -> zoom 0    (retain current zoom)   <- the fix
//   2. /XYZ 100 700 1.5  -> zoom 1.5  (explicit 150%)
//   3. /XYZ 100 700 1    -> zoom 1    (explicit 100%, NOT retained)
// Before the fix, dest 1 resolved to zoom 1 (forced 100%) and would fail here.
//
// Run:  bun tests/issue-5537.ts [--no-build]

import { existsSync, writeFileSync, readFileSync, rmSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";

const SCRIPT_DIR = import.meta.dir;
const ROOT = join(SCRIPT_DIR, "..");
const EXE = join(ROOT, "out", "dbg64", "SumatraPDF-dll.exe");
const PDF = join(tmpdir(), "sumatra-issue-5537.pdf");
const OUT = join(tmpdir(), "sumatra-issue-5537-result.txt");

function fail(msg: string): never {
  console.error(`\n❌ ${msg}`);
  process.exit(1);
}

// build a minimal PDF (2 pages) with an outline of 3 /XYZ destinations
function makePdf(): Buffer {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R /Outlines 5 0 R >>`,
    `<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
    `<< /Type /Outlines /First 6 0 R /Last 8 0 R /Count 3 >>`,
    `<< /Title (XYZ zoom 0) /Parent 5 0 R /Next 7 0 R /Dest [4 0 R /XYZ 100 700 0] >>`,
    `<< /Title (XYZ zoom 1.5) /Parent 5 0 R /Prev 6 0 R /Next 8 0 R /Dest [4 0 R /XYZ 100 700 1.5] >>`,
    `<< /Title (XYZ zoom 1) /Parent 5 0 R /Prev 7 0 R /Dest [4 0 R /XYZ 100 700 1] >>`,
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

function buildApp() {
  console.log("• building SumatraPDF-dll.exe (cmd/build.ts) ...");
  const p = Bun.spawnSync({ cmd: ["bun", join(ROOT, "cmd", "build.ts")], cwd: ROOT, stdout: "inherit", stderr: "inherit" });
  if (p.exitCode !== 0) {
    fail("build failed");
  }
}

// returns { page, zoom } for the n-th outline destination
function resolveDest(no: number): { page: number; zoom: number; raw: string } {
  rmSync(OUT, { force: true });
  const p = Bun.spawnSync({ cmd: [EXE, "-test-dest", PDF, String(no), OUT], stdout: "pipe", stderr: "pipe" });
  const raw = existsSync(OUT) ? readFileSync(OUT, "utf-8").trim() : p.stdout.toString().trim();
  const m = raw.match(/page=(-?\d+)\s+zoom=([\d.eE+-]+)/);
  if (!m) {
    return { page: -1, zoom: NaN, raw };
  }
  return { page: parseInt(m[1]), zoom: parseFloat(m[2]), raw };
}

function main() {
  if (!process.argv.includes("--no-build")) {
    buildApp();
  }
  if (!existsSync(EXE)) {
    fail(`app not found: ${EXE} (run without --no-build)`);
  }
  writeFileSync(PDF, makePdf());

  // [destNo, expectedZoom, description]
  const cases: [number, number, string][] = [
    [1, 0, "/XYZ 100 700 0   -> retain current zoom (the #5537 fix)"],
    [2, 1.5, "/XYZ 100 700 1.5 -> explicit 150%"],
    [3, 1, "/XYZ 100 700 1   -> explicit 100% (must NOT be retained)"],
  ];

  let allOk = true;
  for (const [no, expectZoom, desc] of cases) {
    const r = resolveDest(no);
    const ok = r.page === 2 && Math.abs(r.zoom - expectZoom) < 1e-4;
    allOk &&= ok;
    console.log(`  ${ok ? "✅" : "❌"} dest ${no}: ${desc}`);
    console.log(`        -> ${r.raw}  (expected page=2 zoom=${expectZoom})`);
  }

  rmSync(PDF, { force: true });
  rmSync(OUT, { force: true });
  if (allOk) {
    console.log("\n✅ /XYZ zoom 0 retains current zoom; explicit zooms preserved (issue #5537 fixed)");
    process.exit(0);
  }
  fail("destination zoom resolution is wrong");
}

main();
