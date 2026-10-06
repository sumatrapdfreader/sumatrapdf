// `SumatraPDF merge` (mupdf's pdfmerge):
// - a missing or non-PDF input is skipped, but the exit code must say it failed
// - a backwards range ("3-1") must renumber the bookmarks with the pages
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const kTitles = ["One", "Two", "Three"];

// a 3-page PDF with one bookmark per page
function outlinedPdf(): string {
  const n = kTitles.length;
  const pageObj = (i: number) => 3 + i;
  const itemObj = (i: number) => 3 + n + 1 + i;
  const outlinesObj = 3 + n;
  const objs: string[] = [];
  objs.push(`<< /Type /Catalog /Pages 2 0 R /Outlines ${outlinesObj} 0 R >>`);
  const kids = kTitles.map((_, i) => `${pageObj(i)} 0 R`).join(" ");
  objs.push(`<< /Type /Pages /Kids [${kids}] /Count ${n} >>`);
  for (let i = 0; i < n; i++) {
    objs.push(`<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] >>`);
  }
  objs.push(`<< /Type /Outlines /First ${itemObj(0)} 0 R /Last ${itemObj(n - 1)} 0 R /Count ${n} >>`);
  for (let i = 0; i < n; i++) {
    const prev = i > 0 ? ` /Prev ${itemObj(i - 1)} 0 R` : "";
    const next = i < n - 1 ? ` /Next ${itemObj(i + 1)} 0 R` : "";
    objs.push(`<< /Title (${kTitles[i]}) /Parent ${outlinesObj} 0 R${prev}${next} /Dest [${pageObj(i)} 0 R /Fit] >>`);
  }

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

// one page with an uncompressed content stream, so compress= rewrites it
function streamedPdf(): string {
  const content = "BT /F1 24 Tf 72 100 Td (Hello) Tj ET\n";
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] /Contents 4 0 R >>",
    `<< /Length ${content.length} >>\nstream\n${content}endstream`,
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

// title -> page number, from `show <pdf> outline` ("One\t#page=3&...")
function bookmarkPages(pdf: string): Map<string, number> {
  const r = run(["show", pdf, "outline"]);
  const res = new Map<string, number>();
  for (const line of r.out.split(/\r?\n/)) {
    const m = /^\s*[-+|]?\s*"?([A-Za-z]+)"?\t#page=(\d+)/.exec(line);
    if (m) {
      res.set(m[1]!, +m[2]!);
    }
  }
  return res;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("tool-merge");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const src = join(dir, "in.pdf");
  writeFileSync(src, outlinedPdf());

  // a missing input
  const outMissing = join(dir, "missing-out.pdf");
  const missing = run(["merge", "-o", outMissing, src, join(dir, "does-not-exist.pdf")]);
  if (missing.code === 0) {
    throw new Error(`tool-merge: merging a missing input exited 0:\n${missing.out}`);
  }

  // backwards range: page 3 comes first, so "Three" is page 1 and "One" page 3
  const outRev = join(dir, "reversed.pdf");
  const rev = run(["merge", "-o", outRev, src, "3-1"]);
  if (rev.code !== 0 || !existsSync(outRev)) {
    throw new Error(`tool-merge: merge 3-1 failed (exit ${rev.code}):\n${rev.out}`);
  }
  const got = bookmarkPages(outRev);
  const want = new Map([
    ["Three", 1],
    ["Two", 2],
    ["One", 3],
  ]);
  for (const [title, page] of want) {
    if (got.get(title) !== page) {
      throw new Error(
        `tool-merge: after 3-1 '${title}' points to page ${got.get(title)}, want ${page}; ` +
          `outline: ${JSON.stringify([...got])}`,
      );
    }
  }
  const streamSrc = join(dir, "stream.pdf");
  writeFileSync(streamSrc, streamedPdf());
  const outBrotli = join(dir, "brotli.pdf");
  const brotli = run(["merge", "-o", outBrotli, "-O", "compress=brotli", streamSrc]);
  if (brotli.code !== 0 || !existsSync(outBrotli)) {
    throw new Error(`tool-merge: compress=brotli failed (exit ${brotli.code}):\n${brotli.out}`);
  }
  const brotliPdf = readFileSync(outBrotli).toString("latin1");
  if (!brotliPdf.includes("/BrotliDecode")) {
    throw new Error("tool-merge: compress=brotli did not write a BrotliDecode stream");
  }

  const outFlate = join(dir, "flate.pdf");
  const flate = run(["merge", "-o", outFlate, "-O", "garbage=deduplicate,objstms,compress=flate", streamSrc]);
  if (flate.code !== 0 || !existsSync(outFlate)) {
    throw new Error(`tool-merge: compress=flate failed (exit ${flate.code}):\n${flate.out}`);
  }
  if (!readFileSync(outFlate).toString("latin1").includes("/FlateDecode")) {
    throw new Error("tool-merge: compress=flate did not write a FlateDecode stream");
  }

  const bogus = run(["merge", "-o", join(dir, "bogus.pdf"), "-O", "compress=nope", streamSrc]);
  if (bogus.code === 0) {
    throw new Error(`tool-merge: compress=nope exited 0:\n${bogus.out}`);
  }
  if (!/unknown compression|invalid pdf option/i.test(bogus.out) || /uncaught error/.test(bogus.out)) {
    throw new Error(
      `tool-merge: compress=nope crashed instead of reporting the option (exit ${bogus.code}):\n${bogus.out}`,
    );
  }

  console.log("tool-merge: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
