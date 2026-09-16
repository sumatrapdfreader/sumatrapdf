// #6199: show reading progress on the home page (list column and thumbnail
// badge). Seed FileStates with PageNo / PageCount and check the list dump.
import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { EXE, ROOT, runStandalone, tmpPath } from "./util";

type Row = { size: string; progress: string; path: string };

async function homeListRows(client: ControlClient): Promise<Row[]> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestHomeListRows, []);
    const exitCode = res[0] as number;
    const out = String(res[1] ?? "");
    if (exitCode === 0) {
      const rows: Row[] = [];
      for (const line of out.split("\n")) {
        const m = /^row=\d+ size='([^']*)' sizeRect=(-?\d+),(-?\d+),(-?\d+),(-?\d+) progress='([^']*)' path=(.*)$/.exec(
          line,
        );
        if (m) {
          rows.push({ size: m[1]!, progress: m[6]!, path: m[7]! });
        }
      }
      return rows;
    }
    if (exitCode !== 2) {
      throw new Error(`issue-6199: TestHomeListRows failed: ${out}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6199: home page never painted its list: ${out}`);
    }
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
}

function settingsFor(
  dir: string,
  showProgress: boolean,
  files: { path: string; pageNo: string; pageCount: number }[],
): string {
  const states = files
    .map(
      (f) =>
        `\t[\n\t\tFilePath = ${f.path}\n\t\tPageNo = ${f.pageNo}\n\t\tPageCount = ${f.pageCount}\n\t\tOpenCount = 3\n\t]`,
    )
    .join("\n");
  return `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = true
HomePageViewMode = list
ShowHomePageReadingProgress = ${showProgress ? "true" : "false"}
FileStates [
${states}
]
`;
}

async function rowsFor(
  dir: string,
  showProgress: boolean,
  files: { path: string; pageNo: string; pageCount: number }[],
): Promise<Row[]> {
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), settingsFor(dir, showProgress, files));
  return withControlledSumatra(EXE, (client) => homeListRows(client), ["-appdata", dir]);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6199");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });

  const src = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const pdf = join(dir, "book.pdf");
  const ebook = join(dir, "ebook.pdf");
  copyFileSync(src, pdf);
  copyFileSync(src, ebook);

  const files = [
    { path: pdf, pageNo: "12", pageCount: 62 },
    { path: ebook, pageNo: "bm:3:5:20", pageCount: 0 },
  ];

  const on = await rowsFor(dir, true, files);
  const pdfRow = on.find((r) => r.path.toLowerCase() === pdf.toLowerCase());
  const ebookRow = on.find((r) => r.path.toLowerCase() === ebook.toLowerCase());
  if (!pdfRow || pdfRow.progress !== "12/62") {
    throw new Error(`issue-6199: want progress 12/62 for ${pdf}, got ${JSON.stringify(on)}`);
  }
  if (!ebookRow || ebookRow.progress !== "3:5/20") {
    throw new Error(`issue-6199: want progress 3:5/20 for ${ebook}, got ${JSON.stringify(on)}`);
  }

  const off = await rowsFor(dir, false, files);
  if (off.some((r) => r.progress !== "")) {
    throw new Error(`issue-6199: progress still shown when setting is off: ${JSON.stringify(off)}`);
  }
  console.log("issue-6199: list progress 12/62 and 3:5/20, hidden when off");
}

if (import.meta.main) {
  await runStandalone(testit);
}
