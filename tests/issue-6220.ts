// A restored session of a comic in continuous + fit width scrolled itself by a
// few pixels on every start (#6220).
//
// Run: bun tests/issue-6220.ts [--no-build]
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { deflateSync } from "node:zlib";
import { join } from "node:path";
import { runStandalone, tmpPath, pngChunk, makeStoredZip } from "./util.ts";
import { killAndWait, killProcessesNamed, launchControlled } from "./win-automation.ts";

function makePng(w: number, h: number): Buffer {
  const raw = Buffer.alloc((w * 3 + 1) * h);
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0);
  ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8;
  ihdr[9] = 2;
  return Buffer.concat([
    Buffer.from("89504e470d0a1a0a", "hex"),
    pngChunk("IHDR", ihdr),
    pngChunk("IDAT", deflateSync(raw)),
    pngChunk("IEND", Buffer.alloc(0)),
  ]);
}

type Variant = {
  // scanned comics: every page a few pixels different in size
  varied?: boolean;
  // real comic page size (fit width zoom < 1)
  big?: boolean;
  scrollY?: number;
};

function makeCbz(nPages: number, v: Variant): Buffer {
  const entries: { name: string; data: Buffer }[] = [];
  const w = v.big ? 1200 : 400;
  const h = v.big ? 1800 : 600;
  for (let i = 1; i <= nPages; i++) {
    const dw = v.varied ? (i * 7) % 11 : 0;
    const dh = v.varied ? (i * 5) % 9 : 0;
    entries.push({ name: `${String(i).padStart(3, "0")}.png`, data: makePng(w + dw, h + dh) });
  }
  return makeStoredZip(entries);
}

const START_PAGE = 10;
const START_SCROLL_Y = 200;
const RESTART_COUNT = 2;

function seedSettings(dir: string, cbz: string, v: Variant): void {
  const START_SCROLL_Y = v.scrollY ?? 200;
  const settings = `UiLanguage = en
CheckForUpdates = false
RestoreSession = true
RememberOpenedFiles = true
RememberStatePerDocument = true
ReuseInstance = false
UseTabs = true
ShowStartPage = false
FileStates [
  [
    FilePath = ${cbz}
    DisplayMode = continuous
    Zoom = fit width
    PageNo = ${START_PAGE}
    ScrollPos = -1 ${START_SCROLL_Y}
    Rotation = 0
    WindowState = 1
  ]
]
SessionData [
  [
    TabStates [
      [
        FilePath = ${cbz}
        DisplayMode = continuous
        PageNo = ${START_PAGE}
        Zoom = fit width
        Rotation = 0
        ScrollPos = -1 ${START_SCROLL_Y}
        ShowToc = false
      ]
    ]
    TabIndex = 1
    WindowState = 1
    WindowPos = 100 100 800 600
  ]
]
`;
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), settings);
}

function readScrollPos(dir: string): { page: string; x: number; y: number } {
  const text = readFileSync(join(dir, "SumatraPDF-settings.txt"), "utf8");
  const m = /TabStates \[\s*\[[^\]]*?PageNo = (\S+)[^\]]*?ScrollPos = (\S+) (\S+)/.exec(text);
  if (!m) {
    throw new Error("issue-6220: no TabStates ScrollPos in saved settings");
  }
  return { page: m[1]!, x: parseFloat(m[2]!), y: parseFloat(m[3]!) };
}

// one start: restore the session, let it settle, quit; returns what got saved
let gRunNo = 0;

async function restartOnce(dir: string, v: Variant): Promise<{ page: string; x: number; y: number }> {
  gRunNo++;
  const logArgs = process.env.ISSUE_6220_LOG ? ["-log-to-file", join(dir, `log-${gRunNo}.txt`)] : [];
  const { proc, client } = await launchControlled(["-appdata", dir, ...logArgs], { saveSettings: true });
  try {
    await client.waitForSessionRestored(30000);
    await client.waitForRenderIdle(30000);
    await client.quit();
  } catch (e) {
    await killAndWait(proc);
    throw e;
  }
  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    throw new Error(`issue-6220: exit code ${exitCode}, want 0`);
  }
  return readScrollPos(dir);
}

async function runVariant(name: string, v: Variant): Promise<void> {
  const dir = tmpPath(`issue-6220-${name}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const cbz = join(dir, "pages.cbz");
  writeFileSync(cbz, makeCbz(30, v));
  seedSettings(dir, cbz, v);

  const runs: { page: string; x: number; y: number }[] = [];
  for (let i = 0; i < RESTART_COUNT; i++) {
    runs.push(await restartOnce(dir, v));
    console.log(
      `issue-6220 ${name} start ${i + 1}: PageNo = ${runs[i]!.page}, ScrollPos = ${runs[i]!.x} ${runs[i]!.y}`,
    );
  }
  // the first start is allowed to land at whatever the seeded position rounds
  // to; from then on every start must save back exactly what it restored
  for (let i = 1; i < runs.length; i++) {
    const a = runs[i - 1]!;
    const b = runs[i]!;
    if (a.page !== b.page || a.x !== b.x || a.y !== b.y) {
      throw new Error(
        `issue-6220 ${name}: scroll position drifted between starts: ` +
          `${a.page} ${a.x} ${a.y} -> ${b.page} ${b.x} ${b.y}`,
      );
    }
  }
}

export async function testit(): Promise<void> {
  await killProcessesNamed("SumatraPDF.exe");
  try {
    const only = process.env.ISSUE_6220_VARIANT;
    // deep into the page, so the next page is the most visible one
    const variants: [string, Variant][] = [
      ["deep", { scrollY: 500 }],
      ["big-varied", { big: true, varied: true, scrollY: 1300 }],
    ];
    for (const [name, v] of variants) {
      if (only && only !== name) {
        continue;
      }
      await runVariant(name, v);
    }
  } finally {
    await killProcessesNamed("SumatraPDF.exe");
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
