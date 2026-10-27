// Test for https://github.com/sumatrapdfreader/sumatrapdf/discussions/6411
//
// EBookUI.FontName must find a font installed per-user, i.e. in
// %LOCALAPPDATA%\Microsoft\Windows\Fonts, not only in C:\Windows\Fonts.
// The test copies arial.ttf there with its family renamed to a unique name.

import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { withControlledSumatra } from "./control.ts";
import { IS_WIN } from "./host.ts";
import { makeFixtures } from "./ad-hoc-issue-4600.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const SRC_FONT = "C:\\Windows\\Fonts\\arial.ttf";
// same length as "Arial" so the name table is patched in place
const FAMILY = "Zqxyw";

// replace "Arial" (ASCII and UTF-16BE) in the font's 'name' table
function renameFont(data: Buffer): Buffer {
  const out = Buffer.from(data);
  const numTables = out.readUInt16BE(4);
  for (let i = 0; i < numTables; i++) {
    const rec = 12 + i * 16;
    if (out.toString("latin1", rec, rec + 4) !== "name") continue;
    const off = out.readUInt32BE(rec + 8);
    const end = off + out.readUInt32BE(rec + 12);
    const pairs: [Buffer, Buffer][] = [
      [Buffer.from("Arial", "latin1"), Buffer.from(FAMILY, "latin1")],
      [Buffer.from("Arial", "utf16le").swap16(), Buffer.from(FAMILY, "utf16le").swap16()],
    ];
    for (const [from, to] of pairs) {
      for (let p = out.indexOf(from, off); p >= 0 && p < end; p = out.indexOf(from, p + 1)) {
        to.copy(out, p);
      }
    }
    return out;
  }
  throw new Error("issue-6411: no name table in arial.ttf");
}

// open the epub with EBookUI.FontName = fontName; true if the app logged it as not found
async function fontMissing(epub: string, fontName: string): Promise<boolean> {
  const appData = tmpPath("issue-6411-appdata");
  rmSync(appData, { recursive: true, force: true });
  mkdirSync(appData, { recursive: true });
  writeFileSync(
    join(appData, "SumatraPDF-settings.txt"),
    `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
EBookUI [
    FontName = ${fontName}
]
`,
  );
  const logPath = join(appData, "log.txt");
  await withControlledSumatra(EXE, (client) => client.waitForRenderIdle(), [
    "-appdata",
    appData,
    "-log-to-file",
    logPath,
    epub,
  ]);
  return readFileSync(logPath, "utf8").includes(`ebook font '${fontName}' not found`);
}

export async function testit(): Promise<void> {
  if (!IS_WIN || !existsSync(SRC_FONT) || !process.env.LOCALAPPDATA) {
    console.log("issue-6411: skipped, needs Windows with arial.ttf");
    return;
  }
  makeFixtures();
  const epub = join(tmpPath("epub-font"), "none.epub");

  // the log check must be able to fail
  if (!(await fontMissing(epub, "NoSuchFont6411"))) {
    throw new Error("issue-6411: missing font was not reported");
  }

  const userFontDir = join(process.env.LOCALAPPDATA, "Microsoft", "Windows", "Fonts");
  const userFont = join(userFontDir, "sumatra-test-issue-6411.ttf");
  mkdirSync(userFontDir, { recursive: true });
  writeFileSync(userFont, renameFont(readFileSync(SRC_FONT)));
  try {
    if (await fontMissing(epub, FAMILY)) {
      throw new Error(`issue-6411: per-user font ${FAMILY} not found`);
    }
  } finally {
    rmSync(userFont, { force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
