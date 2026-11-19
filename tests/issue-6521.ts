// Inverse search must pick the line box under the cursor, not a page-level box
// that also contains it. SyncTeX compares box areas; they overflowed a 32-bit
// `long` on Windows. Box geometry is taken from the bug report's .synctex.
import { gzipSync } from "node:zlib";
import { writeFileSync } from "node:fs";
import { ControlCommand, runControlCommand } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const LINE_TEXT = 2;
const LINE_PAGE_BOX = 9;
// in bp, inside both boxes (synctex scales sp by 65781.76)
const HIT_X = 234;
const HIT_Y = 152;

function buildPdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const objs = [
    "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n",
    "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n",
    "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>\nendobj\n",
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

// a page-sized hbox (output routine) followed by a text line hbox
function buildSynctexGz(): Buffer {
  const synctex =
    `SyncTeX Version:1\n` +
    `Input:1:test.tex\n` +
    `Output:pdf\n` +
    `Magnification:1000\n` +
    `Unit:1\n` +
    `X Offset:0\n` +
    `Y Offset:0\n` +
    `Content:\n` +
    `{1\n` +
    `(1,${LINE_PAGE_BOX}:2949120,33987124:20927064,31562292,153929\n` +
    `g1,${LINE_PAGE_BOX}:2949120,33987124\n` +
    `)\n` +
    `(1,5:13569629,10238041:9965148,465734,140115\n` +
    `x1,${LINE_TEXT}:13860384,10238041\n` +
    `g1,${LINE_TEXT}:14024838,10238041\n` +
    `x1,${LINE_TEXT}:16689656,10238041\n` +
    `)\n` +
    `}1\n` +
    `Postamble:\n` +
    `Count:8\n` +
    `Post scriptum:\n`;
  return gzipSync(Buffer.from(synctex, "latin1"));
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6521.pdf");
  writeFileSync(pdf, buildPdf());
  writeFileSync(pdf.replace(/\.pdf$/, ".synctex.gz"), buildSynctexGz());

  const [, raw] = await runControlCommand(EXE, ControlCommand.TestInverseSearch, [pdf, 1, HIT_X, HIT_Y]);
  const res = String(raw).trim();
  const m = /\bline=(\d+)/.exec(res);
  if (!m || Number(m[1]) !== LINE_TEXT) {
    throw new Error(`expected line=${LINE_TEXT}, got:\n${res}`);
  }
  console.log("issue-6521: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
