// Regression test for GHSA-jf4v-rw66-j4w2: SyncTeX inverse-search argument
// injection. The source file name in a .synctex file travels with the PDF and
// is attacker-controlled; it ends up on the command line of the configured
// inverse-search editor. A name like
//   foo.tex" --install-extension evil.vsix --
// breaks out of the editor template's quotes ("%f:%l") and injects arguments.
//
// The fix rejects a synctex source name containing characters that can't be in
// a real Windows path ('"' above all). This drives the app's headless
// inverse-search (-dbg-control TestInverseSearch) at a point that maps to the
// malicious node and asserts the resolved source path is NOT handed back with
// the injection intact.
//
// The synctex below uses Unit:1 with no offset, so the inverse-search query
// coordinates are raw synctex units. The hit box spans h in [8799518,31409438]
// and v in [5784862,46220574]; a point inside it maps to the malicious node.
import { gzipSync } from "node:zlib";
import { writeFileSync } from "node:fs";
import { ControlCommand, runControlCommand } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

// the injection payload used as the synctex source file name. The leading real
// name plus the quote breakout and an editor flag mirror the advisory's PoC.
const PAYLOAD = String.raw`C:\Users\Public\evil.tex" --install-extension C:\Users\Public\payload.vsix --`;
const TARGET_LINE = 1;
// a point (synctex units) inside the box the malicious node covers
const HIT_X = 20000000;
const HIT_Y = 25000000;

// minimal single-page PDF (matches the advisory's PoC geometry so the synctex
// box below maps onto it)
function buildPdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const objs = [
    "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n",
    "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n",
    "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R" +
      " /Resources << /Font << /F1 5 0 R >> >> >>\nendobj\n",
    (() => {
      const stream = "BT /F1 24 Tf 100 700 Td (Double-click here) Tj ET";
      return `4 0 obj\n<< /Length ${stream.length} >>\nstream\n${stream}\nendstream\nendobj\n`;
    })(),
    "5 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n",
  ];
  const header = "%PDF-1.4\n";
  const offsets: number[] = [];
  let pos = header.length;
  for (const o of objs) {
    offsets.push(pos);
    pos += o.length;
  }
  let xref = `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    xref += `${String(off).padStart(10, "0")} 00000 n \n`;
  }
  const trailer = `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF`;
  return Buffer.concat([enc(header), ...objs.map(enc), enc(xref), enc(trailer)]);
}

// valid synctex whose single Input carries the injection payload (advisory PoC)
function buildSynctexGz(): Buffer {
  const synctex =
    `SyncTeX Version:1\n` +
    `Input:1:${PAYLOAD}\n` +
    `Input:2:/usr/share/texlive/texmf-dist/tex/latex/base/article.cls\n` +
    `Output:pdf\n` +
    `Magnification:1000\n` +
    `Unit:1\n` +
    `X Offset:0\n` +
    `Y Offset:0\n` +
    `Content:\n` +
    `!335\n` +
    `{{1\n` +
    `[1,${TARGET_LINE}:4736286,46220574:26673152,41484288,0\n` +
    `[1,${TARGET_LINE}:8799518,46220574:22609920,40435712,0\n` +
    `h1,${TARGET_LINE}:8799518,6571294:22609920,0,0\n` +
    `]\n` +
    `]\n` +
    `}}1\n` +
    `Postamble:\n` +
    `count:1\n` +
    `Post scriptum:\n`;
  return gzipSync(Buffer.from(synctex, "latin1"));
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("ghsa-jf4v.pdf");
  writeFileSync(pdf, buildPdf());
  // the app looks for <pdf-basename>.synctex.gz next to the pdf
  writeFileSync(pdf.replace(/\.pdf$/, ".synctex.gz"), buildSynctexGz());

  // inverse search at a point that maps to the malicious node
  const [, invRaw] = await runControlCommand(EXE, ControlCommand.TestInverseSearch, [pdf, 1, HIT_X, HIT_Y]);
  const inv = String(invRaw).trim();

  // A vulnerable build resolves srcfile to the payload verbatim; the fixed build
  // fails DocToSource with PDFSYNCERR_UNKNOWN_SOURCEFILE (5) and returns no
  // srcfile. Either way, the injection must never come back as a source path we
  // would put on an editor command line.
  if (inv.includes("--install-extension") || inv.includes("payload.vsix") || inv.includes('"')) {
    throw new Error(`inverse search returned the injection payload as a source path:\n${inv}`);
  }
  // (a plain "no sync at this location" would also lack the payload but wouldn't
  // prove the node was reached; require the specific rejection code)
  if (!/doctosource-failed err=5\b/.test(inv)) {
    throw new Error(`expected DocToSource to reject the malicious name (err=5), got:\n${inv}`);
  }

  console.log("security-ghsa-jf4v-rw66-j4w2: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
