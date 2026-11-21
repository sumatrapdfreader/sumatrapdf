// #6576: SOLIDWORKS writes "פח עובי" + "5" with the space first in the content
// stream but draws it between the word and the number. It led the extracted
// line, so copy gave "פח עובי5".
import { writeFileSync } from "node:fs";
import { assemblePdf, extractPageText, runStandalone, tmpPath } from "./util.ts";

const WORD = "פח עובי";
const WANTED = "פח עובי 5";
const FONT_SIZE = 16;
const RIGHT_X = 260;
const BASELINE_Y = 45;

// Type3 glyphs: letters and the digit are filled boxes, the space is empty.
// Widths are in 1/1000 em.
const kLetterW = 600;
const kSpaceW = 300;
const kDigitW = 550;

// one-byte code for each char, mapped back to Unicode by the ToUnicode CMap
const chars = [" ", "5", ...new Set(WORD.replace(" ", ""))];
const codeOf = (ch: string) => 32 + chars.indexOf(ch);
const widthOf = (ch: string) => (ch === " " ? kSpaceW : ch === "5" ? kDigitW : kLetterW);
const hex = (n: number, digits: number) => n.toString(16).toUpperCase().padStart(digits, "0");

function makePdf(): Buffer {
  // visual layout, right to left: word, space, "5"
  const em = FONT_SIZE / 1000;
  let x = RIGHT_X;
  const word: [string, number][] = [];
  for (const ch of WORD) {
    x -= widthOf(ch) * em;
    word.push([ch, x]);
  }
  const spaceX = x - kSpaceW * em;
  const fiveX = spaceX - kDigitW * em;

  // content stream order: space, word, number
  const placed: [string, number][] = [[" ", spaceX], ...word, ["5", fiveX]];
  const content = placed
    .map(([ch, cx]) => `BT /F1 ${FONT_SIZE} Tf ${cx.toFixed(3)} ${BASELINE_Y} Td <${hex(codeOf(ch), 2)}> Tj ET`)
    .join("\n");

  const lastCode = 32 + chars.length - 1;
  const widths = chars.map(widthOf).join(" ");
  const glyphName = (ch: string) => (ch === " " ? "space" : ch === "5" ? "digit" : "letter");
  const differences = chars.map((ch) => `/${glyphName(ch)}`).join(" ");
  const bfchar = chars.map((ch) => `<${hex(codeOf(ch), 2)}> <${hex(ch.codePointAt(0)!, 4)}>`).join("\n");
  const cmap =
    "/CIDInit /ProcSet findresource begin 12 dict begin begincmap\n" +
    "/CMapName /Issue6576 def /CMapType 2 def\n" +
    "1 begincodespacerange <00> <FF> endcodespacerange\n" +
    `${chars.length} beginbfchar\n${bfchar}\nendbfchar\n` +
    "endcmap CMapName currentdict /CMap defineresource pop end end";
  const stream = (s: string) => `<< /Length ${s.length} >>\nstream\n${s}\nendstream`;
  const box = (w: number) => `${w} 0 0 0 ${w - 50} 700 d1 25 0 ${w - 100} 700 re f`;

  return Buffer.from(
    assemblePdf([
      "<< /Type /Catalog /Pages 2 0 R >>",
      "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 80] /Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>",
      stream(content),
      `<< /Type /Font /Subtype /Type3 /FontBBox [0 -200 1000 800] /FontMatrix [0.001 0 0 0.001 0 0] ` +
        `/CharProcs << /space 6 0 R /digit 7 0 R /letter 8 0 R >> ` +
        `/Encoding << /Type /Encoding /Differences [32 ${differences}] >> ` +
        `/FirstChar 32 /LastChar ${lastCode} /Widths [${widths}] /ToUnicode 9 0 R /Resources << >> >>`,
      stream(`${kSpaceW} 0 d0`),
      stream(box(kDigitW)),
      stream(box(kLetterW)),
      stream(cmap),
    ]),
    "latin1",
  );
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6576.pdf");
  writeFileSync(pdf, makePdf());
  // keep a leading space visible: trimming it would hide the bug
  const text = extractPageText(pdf, 1).replace(/\n+$/, "");
  if (text !== WANTED) {
    throw new Error(`issue-6576: want '${WANTED}', got '${text}'`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
