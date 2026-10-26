// `SumatraPDF grep` (mupdf's mugrep): a bad -S or -O value (e.g. "-SHin", where
// -S takes "Hin") must be reported, not abort the process with a crash dump
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, runStandalone, tmpPath } from "./util.ts";

// one page with "Hello" in Helvetica
function helloPdf(): string {
  const content = "BT /F1 24 Tf 72 100 Td (Hello) Tj ET\n";
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] /Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>",
    `<< /Length ${content.length} >>\nstream\n${content}endstream`,
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
  ];
  let out = "%PDF-1.4\n";
  const offsets: number[] = [];
  objs.forEach((o, i) => {
    offsets.push(out.length);
    out += `${i + 1} 0 obj\n${o}\nendobj\n`;
  });
  const xref = out.length;
  out += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    out += `${String(off).padStart(10, "0")} 00000 n \n`;
  }
  out += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return out;
}

function run(args: string[]): { code: number; out: string } {
  const p = Bun.spawnSync([EXE, ...args]);
  return { code: p.exitCode ?? -1, out: p.stdout.toString() + p.stderr.toString() };
}

export async function testit(): Promise<void> {
  const dir = tmpPath("tool-grep");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "hello.pdf");
  writeFileSync(pdf, helloPdf());

  const ok = run(["grep", "-i", "hello", pdf]);
  if (ok.code !== 0 || !ok.out.includes("Hello")) {
    throw new Error(`tool-grep: grep -i hello failed (exit ${ok.code}):\n${ok.out}`);
  }

  // -S and -O take an argument, so "-SHin" / "-OHin" pass "Hin" as options
  for (const flag of ["-SHin", "-OHin"]) {
    const bad = run(["grep", flag, "hello", pdf]);
    if (bad.code !== 1 || /uncaught error/.test(bad.out)) {
      throw new Error(`tool-grep: grep ${flag} did not report the bad option (exit ${bad.code}):\n${bad.out}`);
    }
  }

  console.log("tool-grep: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
