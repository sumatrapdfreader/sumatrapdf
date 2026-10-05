// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6276
//
// `sumatrapdf-tool grep` printing UTF-8 matches to a console with a DBCS code
// page (936) failed with "cannot fwrite: No space left on device". The failure
// needs a real console, so the tool runs in a new one via `start /wait`.
//
// The access violation was a use-after-free: the rest of a matched line is
// printed at the next match, by which time the search had freed that page.
//
// Run:  bun tests/issue-6276.ts [--no-build]   (or via tests/run-almost-all.ts)

import { existsSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { EXE, ROOT, runStandalone, tmpPath } from "./util";

const FIXTURE = join(ROOT, "tests", "issue-6276.txt");
const DBCS_CODE_PAGE = 936;

function makeFixture(): string {
  const objects: string[] = [];
  const add = (body: string): number => {
    objects.push(body);
    return objects.length;
  };
  const stream = (data: string): string => `<< /Length ${data.length} >>\nstream\n${data}\nendstream`;
  const lines = ["first 2023 rest of line", "", "", "", "\\200 2023"];
  const pageCount = lines.length;
  const fontObj = pageCount * 2 + 3;
  const cmapObj = fontObj + 1;
  const glyphObj = cmapObj + 1;

  add("<< /Type /Catalog /Pages 2 0 R >>");
  const pageIds = Array.from({ length: pageCount }, (_, i) => 3 + i * 2);
  add(`<< /Type /Pages /Count ${pageCount} /Kids [${pageIds.map((id) => `${id} 0 R`).join(" ")}] >>`);
  for (const line of lines) {
    const content = line ? `BT /F1 12 Tf 72 720 Td (${line}) Tj ET` : "";
    const contentId = objects.length + 2;
    add(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 ${fontObj} 0 R >> >> /Contents ${contentId} 0 R >>`,
    );
    add(stream(content));
  }

  // A Type3 font keeps the fixture independent of built-in font resources.
  const glyphNames = ["space", "two", "zero", "three", "f", "i", "r", "s", "t", "e", "o", "l", "n", "uni7535"];
  const charProcs = glyphNames.map((name) => `/${name} ${glyphObj} 0 R`).join(" ");
  const widths = Array.from({ length: 97 }, () => "600").join(" ");
  add(
    `<< /Type /Font /Subtype /Type3 /FontBBox [0 0 600 700] /FontMatrix [.001 0 0 .001 0 0] /CharProcs << ${charProcs} >> /Encoding << /Type /Encoding /BaseEncoding /StandardEncoding /Differences [128 /uni7535] >> /FirstChar 32 /LastChar 128 /Widths [${widths}] /Resources << >> /ToUnicode ${cmapObj} 0 R >>`,
  );
  const cmap =
    "/CIDInit /ProcSet findresource begin\n12 dict begin\nbegincmap\n" +
    "/CMapType 2 def\n1 begincodespacerange\n<00> <FF>\nendcodespacerange\n" +
    "1 beginbfrange\n<20> <7E> <0020>\nendbfrange\n" +
    "1 beginbfchar\n<80> <7535>\nendbfchar\nendcmap\nend\nend";
  add(stream(cmap));
  add(stream("600 0 d0"));

  let pdf = "%PDF-1.4\n";
  const offsets = [0];
  for (let i = 0; i < objects.length; i++) {
    offsets.push(pdf.length);
    pdf += `${i + 1} 0 obj\n${objects[i]}\nendobj\n`;
  }
  const xref = pdf.length;
  pdf += `xref\n0 ${objects.length + 1}\n0000000000 65535 f \n`;
  pdf += offsets
    .slice(1)
    .map((offset) => `${offset.toString().padStart(10, "0")} 00000 n \n`)
    .join("");
  pdf += `trailer\n<< /Size ${objects.length + 1} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return pdf;
}

// a match, many pages without one, then another match
function checkMatchesPagesApart(file: string): void {
  const r = Bun.spawnSync([EXE, "grep", "2023", file]);
  const out = r.stdout.toString();
  if (r.exitCode !== 0 || !out.includes("first 2023 rest of line") || !out.includes("电 2023")) {
    throw new Error(`issue-6276: grep lost the end of a matched line: exit ${r.exitCode}, stdout: ${out}`);
  }
}

export async function testit(): Promise<void> {
  const fixture = tmpPath("issue-6276.pdf");
  writeFileSync(fixture, makeFixture(), "ascii");
  checkMatchesPagesApart(fixture);

  const tool = join(dirname(EXE), "sumatrapdf-tool.exe");
  if (!existsSync(tool)) {
    console.log("  skipping console check: no sumatrapdf-tool.exe from the tested build");
    return;
  }

  const script = tmpPath("issue-6276.cmd");
  const exitFile = tmpPath("issue-6276-exit.txt");
  const errFile = tmpPath("issue-6276-err.txt");
  rmSync(exitFile, { force: true });
  rmSync(errFile, { force: true });

  const lines = [
    "@echo off",
    `chcp ${DBCS_CODE_PAGE} >nul`,
    `"${tool}" grep -n 2023 "${FIXTURE}" 2> "${errFile}"`,
    `echo %errorlevel% > "${exitFile}"`,
  ];
  writeFileSync(script, lines.join("\r\n") + "\r\n");

  Bun.spawnSync(["cmd.exe", "/c", "start", "/wait", "/min", "", "cmd.exe", "/c", script]);
  if (!existsSync(exitFile)) {
    console.log("  skipping: could not run the tool in a new console");
    return;
  }

  const exitCode = readFileSync(exitFile, "utf8").trim();
  const err = readFileSync(errFile, "utf8").trim();
  if (exitCode !== "0" || err !== "") {
    throw new Error(`issue-6276: grep to a cp${DBCS_CODE_PAGE} console failed: exit ${exitCode}, stderr: ${err}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
