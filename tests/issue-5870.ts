// #5870: the home page list view showed "0.00 KB" for every file. The cached
// size on ThumbnailLayout used -2 as "not fetched yet", but those layouts are
// created with Vec::AppendBlanks(), which zero-fills -- default member
// initializers never run, so the field read back as 0, the sentinel never
// matched and file::GetSize() was never called.
//
// Renders the home page list for a file and asks the app what it drew in the
// size column: the text must be there and must match the file's real size.
//
// This used to sample the row's pixels at fixed coordinates and require two
// files of different sizes to render differently. That was wrong: the size
// column's x depends on the window width, the DPI and the theme, so on another
// machine the sample band landed on the path -- identical for both renders --
// and the test failed with "file size not shown" while the app was fine.
import { mkdirSync, rmSync, statSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { EXE, ROOT, runStandalone, tmpPath } from "./util";

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = true
HomePageViewMode = list
`;

type Row = { size: string; sizeRect: number[]; path: string };

// wait for the home page to have painted, then return its list rows
async function homeListRows(client: ControlClient): Promise<Row[]> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestHomeListRows, []);
    const exitCode = res[0] as number;
    const out = String(res[1] ?? "");
    if (exitCode === 0) {
      const rows: Row[] = [];
      for (const line of out.split("\n")) {
        const m = /^row=\d+ size='([^']*)' sizeRect=(-?\d+),(-?\d+),(-?\d+),(-?\d+) path=(.*)$/.exec(line);
        if (m) {
          rows.push({ size: m[1]!, sizeRect: [+m[2]!, +m[3]!, +m[4]!, +m[5]!], path: m[6]! });
        }
      }
      return rows;
    }
    if (exitCode !== 2) {
      throw new Error(`issue-5870: TestHomeListRows failed: ${out}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5870: home page never painted its list: ${out}`);
    }
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
}

// "26.9 KB" / "1.23 MB" / "512 B" -> bytes
function parseSize(s: string): number {
  const m = /^([\d.,]+)\s*(B|KB|MB|GB)$/i.exec(s.trim());
  if (!m) {
    return NaN;
  }
  const units: Record<string, number> = { b: 1, kb: 1024, mb: 1024 * 1024, gb: 1024 * 1024 * 1024 };
  return parseFloat(m[1]!.replace(",", ".")) * units[m[2]!.toLowerCase()]!;
}

async function checkFile(docPath: string): Promise<void> {
  const dir = tmpPath("issue-5870");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `${SETTINGS}FileStates [\n\t[\n\t\tFilePath = ${docPath}\n\t\tOpenCount = 3\n\t]\n]\n`,
  );

  const rows = await withControlledSumatra(EXE, (client) => homeListRows(client), ["-appdata", dir]);
  const row = rows.find((r) => r.path.toLowerCase() === docPath.toLowerCase());
  if (!row) {
    throw new Error(`issue-5870: no home page list row for ${docPath}, got ${JSON.stringify(rows)}`);
  }
  if (row.sizeRect[2]! <= 0 || row.sizeRect[3]! <= 0) {
    throw new Error(`issue-5870: the row has no size column: ${JSON.stringify(row)}`);
  }
  if (!row.size) {
    throw new Error(`issue-5870: no file size drawn for ${docPath}`);
  }

  const want = statSync(docPath).size;
  const got = parseSize(row.size);
  if (!isFinite(got)) {
    throw new Error(`issue-5870: can't parse the drawn size '${row.size}' for ${docPath}`);
  }
  // the text is rounded (3 significant digits), so allow 1%
  if (Math.abs(got - want) > want * 0.01) {
    throw new Error(`issue-5870: drew '${row.size}' (${Math.round(got)} bytes) for a ${want} byte file`);
  }
  console.log(`issue-5870: ${want} bytes drawn as '${row.size}'`);
}

export async function testit(): Promise<void> {
  // two sizes, so a stuck constant (the bug drew "0.00 KB" for everything)
  // can't pass for both
  await checkFile(join(ROOT, "ext", "a-zlib", "zlib.3.pdf")); // ~27 KB
  await checkFile(join(ROOT, "ext", "brotli", "docs", "brotli-comparison-study-2015-09-22.pdf")); // ~210 KB
}

if (import.meta.main) {
  await runStandalone(testit);
}
