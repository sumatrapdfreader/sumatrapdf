// PR #6434: text search keeps a byte cursor per page instead of rescanning the
// page text for every match, and skips start positions whose folded first
// character can't match. Both must leave the matches unchanged.
//
// Every line mixes non-ASCII letters (so byte and codepoint offsets drift
// apart along the page) with several case variants of the needle and a ß.
// A full-document find (FindFirst + FindNext, case-insensitive) must report
// exactly the matches the text holds, on the right pages. Searching backward
// must find the same matches in reverse: there the cursor has to move back.
//
// Run: bun tests/issue-6434.ts [--no-build]
import { writeFileSync } from "node:fs";
import { ControlCommand, withControlledSumatra } from "./control";
import { EXE, runStandalone, tmpPath } from "./util";

const PAGES = 3;
const LINES = 40;
// WinAnsi octal escapes: \304 Ä, \351 é, \326 Ö, \334 Ü, \337 ß
const LINE = "\\304\\351 the \\326\\334 THE \\337 The x";

function buildPdf(): Buffer {
  const content = "BT /F1 10 Tf 14 TL 40 760 Td " + Array(LINES).fill(`(${LINE}) '`).join(" ") + " ET";
  const kids = Array.from({ length: PAGES }, (_, i) => `${4 + i} 0 R`).join(" ");
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    `<< /Type /Pages /Kids [${kids}] /Count ${PAGES} >>`,
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>",
  ];
  const contentObj = 4 + PAGES;
  for (let i = 0; i < PAGES; i++) {
    objs.push(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> /Contents ${contentObj} 0 R >>`,
    );
  }
  objs.push(`<< /Length ${content.length} >>\nstream\n${content}\nendstream`);

  let pdf = "%PDF-1.5\n";
  const offsets: number[] = [];
  objs.forEach((o, i) => {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${i + 1} 0 obj\n${o}\nendobj\n`;
  });
  const xref = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    pdf += `${off.toString().padStart(10, "0")} 00000 n \n`;
  }
  pdf += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

// match pages in the order FindNext reported them
function matchPages(raw: string): number[] {
  return [...raw.matchAll(/^page=(\d+)$/gm)].map((m) => +m[1]!);
}

function countPerPage(pages: number[]): number[] {
  const counts = Array(PAGES).fill(0);
  for (const page of pages) {
    counts[page - 1]++;
  }
  return counts;
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6434.pdf");
  writeFileSync(pdf, buildPdf());

  const cases: { needle: string; perPage: number[] }[] = [
    // the, THE, The on every line
    { needle: "the", perPage: Array(PAGES).fill(3 * LINES) },
    // ß folds to "ss"
    { needle: "ss", perPage: Array(PAGES).fill(LINES) },
    // non-ASCII first character
    { needle: "äé", perPage: Array(PAGES).fill(LINES) },
    // across each line break; the last line of a page continues on the next
    { needle: "x äé", perPage: [...Array(PAGES - 1).fill(LINES), LINES - 1] },
  ];

  await withControlledSumatra(EXE, async (client) => {
    for (const c of cases) {
      const [exitCode, raw] = await client.request(ControlCommand.TestFindPageRange, [pdf, c.needle, 0, 0]);
      const fwd = matchPages(String(raw ?? ""));
      const got = countPerPage(fwd);
      console.log(`  "${c.needle}": ${got.join(",")}`);
      if (exitCode !== 0 || got.some((n, i) => n !== c.perPage[i])) {
        throw new Error(`#6434: "${c.needle}": want ${c.perPage.join(",")} per page, got ${got.join(",")}`);
      }
      if (fwd.some((page, i) => i > 0 && page < fwd[i - 1]!)) {
        throw new Error(`#6434: "${c.needle}": forward matches out of page order`);
      }

      const [bwdCode, bwdRaw] = await client.request(ControlCommand.TestFindPageRange, [pdf, c.needle, 0, 0, "", 1]);
      const bwd = matchPages(String(bwdRaw ?? ""));
      const want = [...fwd].reverse();
      console.log(`  "${c.needle}" backward: ${countPerPage(bwd).join(",")}`);
      if (bwdCode !== 0 || bwd.length !== want.length || bwd.some((page, i) => page !== want[i])) {
        throw new Error(
          `#6434: "${c.needle}" backward: want ${countPerPage(want).join(",")} per page in reverse, ` +
            `got ${countPerPage(bwd).join(",")} (${bwd.length} matches)`,
        );
      }
    }
  });
  console.log("#6434: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
