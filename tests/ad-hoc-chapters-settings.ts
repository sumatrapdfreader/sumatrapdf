// Ad-hoc regression test for the PagePosition settings round trip (see
// docs/location-chapters-plan.md, Phase 5): FileState.PageNo is a plain
// integer for single-chapter documents (PDF) and a "bm:..." chapter bookmark
// for chaptered documents (MOBI), never the other way around.
//
// No GitHub issue backs this, so it's an ad-hoc test, not tests/issue-N.ts.
// Not registered in run-almost-all/run-all: the MOBI half needs a large file
// that lives in OneDrive, not the repo.
//
// Uses a scratch -appdata dir (never the user's real %LOCALAPPDATA%\SumatraPDF)
// and -dbg-control without -for-testing so settings actually get saved.
//
// Run: bun tests/ad-hoc-chapters-settings.ts [--no-build]

import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, killProcessesNamed, launchControlled, sendCommandSync } from "./win-automation.ts";

const MOBI = String.raw`C:\Users\kjk\OneDrive\!sumatra\1000.mobi`;

const APPDATA = tmpPath("ad-hoc-chapters-settings-appdata");
const SETTINGS_PATH = join(APPDATA, "SumatraPDF-settings.txt");

function resetAppData(): void {
  rmSync(APPDATA, { recursive: true, force: true });
  mkdirSync(APPDATA, { recursive: true });
}

function readSettings(): string {
  return readFileSync(SETTINGS_PATH, "utf8");
}

// PageNo of the FileState block whose FilePath matches path
function pageNoFor(settingsTxt: string, path: string): string | null {
  const idx = settingsTxt.indexOf(`FilePath = ${path}`);
  if (idx < 0) {
    return null;
  }
  const m = /PageNo = (.*)/.exec(settingsTxt.slice(idx));
  return m ? m[1].trim() : null;
}

// a small self-contained multi-page PDF, so this test doesn't depend on any
// checked-in fixture's page count
function makePdf(nPages: number): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const kidsRefs: string[] = [];
  for (let i = 0; i < nPages; i++) {
    kidsRefs.push(`${3 + i} 0 R`);
  }
  const objs: Buffer[] = [];
  objs.push(enc(`<< /Type /Catalog /Pages 2 0 R >>`));
  objs.push(enc(`<< /Type /Pages /Kids [${kidsRefs.join(" ")}] /Count ${nPages} >>`));
  for (let i = 0; i < nPages; i++) {
    objs.push(enc(`<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] /Resources << >> >>`));
  }
  const parts: Buffer[] = [enc(`%PDF-1.7\n%\xe2\xe3\xcf\xd3\n`)];
  let pos = parts[0].length;
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets[i] = pos;
    const obj = Buffer.concat([enc(`${i + 1} 0 obj\n`), objs[i], enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  const xrefAt = pos;
  const n = objs.length + 1;
  let xref = `xref\n0 ${n}\n0000000000 65535 f \n`;
  for (let i = 0; i < objs.length; i++) {
    xref += `${String(offsets[i]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size ${n} /Root 1 0 R >>\nstartxref\n${xrefAt}\n%%EOF\n`));
  return Buffer.concat(parts);
}

async function flatPageNo(client: ControlClient): Promise<number> {
  const res = await client.request(ControlCommand.TestFavoriteNav, ["page"]);
  const exitCode = res[0] as number;
  const raw = String(res[1] ?? "");
  if (exitCode !== 0) {
    throw new Error(`TestFavoriteNav page failed: ${raw}`);
  }
  const m = /page=(-?\d+)/.exec(raw);
  if (!m) {
    throw new Error(`no page= in: ${raw}`);
  }
  return Number(m[1]);
}

function assertEq(label: string, got: unknown, want: unknown): void {
  if (got !== want) {
    throw new Error(`${label}: got ${got}, want ${want}`);
  }
}

async function testPdf(): Promise<void> {
  const pdfPath = tmpPath("ad-hoc-chapters-settings.pdf");
  writeFileSync(pdfPath, makePdf(20));

  resetAppData();
  const seed = `RestoreSession = false
FileStates [
  [
    FilePath = ${pdfPath}
    UseDefaultState = false
    PageNo = 5
  ]
]
`;
  writeFileSync(SETTINGS_PATH, seed, "utf8");

  const { proc, client, frame } = await launchControlled(["-appdata", APPDATA, pdfPath], { saveSettings: true });
  try {
    const openedAt = await flatPageNo(client);
    assertEq("pdf: opened at legacy PageNo=5", openedAt, 5);

    sendCommandSync(frame, cmdId("CmdGoToNextPage"));
    await client.waitForRenderIdle();
    sendCommandSync(frame, cmdId("CmdGoToNextPage"));
    await client.waitForRenderIdle();
    const navigatedTo = await flatPageNo(client);
    assertEq("pdf: page after 2x next", navigatedTo, 7);

    await client.quit();
  } catch (e) {
    await killAndWait(proc);
    throw e;
  }
  const exitCode = await proc.exited;
  assertEq("pdf: process exit code", exitCode, 0);

  const settingsTxt = readSettings();
  const pageNo = pageNoFor(settingsTxt, pdfPath);
  if (!pageNo || !/^\d+$/.test(pageNo)) {
    throw new Error(`pdf: expected plain-integer PageNo, got '${pageNo}'`);
  }
  assertEq("pdf: saved PageNo", pageNo, "7");
  console.log(`ad-hoc-chapters-settings: pdf OK (saved PageNo=${pageNo})`);
}

async function testMobi(): Promise<void> {
  if (!existsSync(MOBI)) {
    console.log(`ad-hoc-chapters-settings: skip MOBI section, missing ${MOBI}`);
    return;
  }

  resetAppData();
  {
    const { proc, client, frame } = await launchControlled(["-appdata", APPDATA, MOBI], { saveSettings: true });
    try {
      // enough next-page presses to force a chapter change (chapter 1 is 2
      // pages long, see ad-hoc-chapters.ts)
      for (let i = 0; i < 5; i++) {
        sendCommandSync(frame, cmdId("CmdGoToNextPage"));
        await client.waitForRenderIdle();
      }
      const info = await client.chapterInfo();
      if (info.chapter <= 1) {
        throw new Error(`mobi: expected to have crossed into chapter > 1, got chapter ${info.chapter}`);
      }
      await client.quit();
    } catch (e) {
      await killAndWait(proc);
      throw e;
    }
    const exitCode = await proc.exited;
    assertEq("mobi: process exit code", exitCode, 0);
  }

  const settingsTxt = readSettings();
  const pageNo = pageNoFor(settingsTxt, MOBI);
  if (!pageNo || !pageNo.startsWith("bm:")) {
    throw new Error(`mobi: expected a 'bm:' chapter bookmark, got '${pageNo}'`);
  }
  const chapterMatch = /^bm:(\d+):/.exec(pageNo);
  if (!chapterMatch) {
    throw new Error(`mobi: could not parse chapter out of '${pageNo}'`);
  }
  const chapter = chapterMatch[1];
  console.log(`ad-hoc-chapters-settings: mobi saved bookmark ${pageNo}`);

  // relaunch: the saved chapter must get laid out again on open
  const relaunchLog = tmpPath("ad-hoc-chapters-settings-mobi-relaunch.log");
  rmSync(relaunchLog, { force: true });
  const { proc, client } = await launchControlled(["-appdata", APPDATA, "-log-to-file", relaunchLog, MOBI], {
    saveSettings: true,
  });
  await client.waitForRenderIdle();
  await client.quit();
  const exitCode = await proc.exited;
  assertEq("mobi relaunch: process exit code", exitCode, 0);

  const logTxt = existsSync(relaunchLog) ? readFileSync(relaunchLog, "utf8") : "";
  const re = new RegExp(`LayOutChapter: chapter ${chapter} ->`);
  if (!re.test(logTxt)) {
    throw new Error(`mobi relaunch: log has no LayOutChapter for chapter ${chapter}:\n${logTxt}`);
  }
  console.log(`ad-hoc-chapters-settings: mobi relaunch laid out chapter ${chapter} OK`);
}

export async function testit(): Promise<void> {
  await killProcessesNamed("SumatraPDF.exe");
  try {
    await testPdf();
    await testMobi();
  } finally {
    await killProcessesNamed("SumatraPDF.exe");
    rmSync(APPDATA, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
