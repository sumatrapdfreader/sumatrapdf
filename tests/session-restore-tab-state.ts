// A session restored without lazy loading loads its tabs asynchronously, and
// that path never applied the TabState: the restored tab showed whatever the
// file history said (or the defaults), not the page, mode and zoom it was
// closed with.
//
// Run: bun tests/session-restore-tab-state.ts [--no-build]
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { assemblePdf, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, killProcessesNamed, launchControlled } from "./win-automation.ts";

const N_PAGES = 12;
const WANT_PAGE = 7;

function makePdf(): string {
  const objs = ["<< /Type /Catalog /Pages 2 0 R >>"];
  const kids: string[] = [];
  for (let i = 0; i < N_PAGES; i++) {
    kids.push(`${3 + i} 0 R`);
  }
  objs.push(`<< /Type /Pages /Kids [${kids.join(" ")}] /Count ${N_PAGES} >>`);
  for (let i = 0; i < N_PAGES; i++) {
    objs.push("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>");
  }
  return assemblePdf(objs);
}

// the TabState is the only record of how the tab was viewed: no FileState
function seedSettings(dir: string, pdf: string): void {
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `UiLanguage = en
CheckForUpdates = false
RestoreSession = true
RememberOpenedFiles = true
RememberStatePerDocument = false
LazyLoading = false
ReuseInstance = false
UseTabs = true
ShowStartPage = false
SessionData [
  [
    TabStates [
      [
        FilePath = ${pdf}
        DisplayMode = continuous
        PageNo = ${WANT_PAGE}
        Zoom = fit width
        Rotation = 0
        ScrollPos = -1 -1
        ShowToc = false
      ]
    ]
    TabIndex = 1
    WindowState = 1
    WindowPos = 100 100 800 600
  ]
]
`,
  );
}

export async function testit(): Promise<void> {
  await killProcessesNamed("SumatraPDF.exe");
  const dir = tmpPath("session-restore-tab-state");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "pages.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  seedSettings(dir, pdf);

  const { proc, client } = await launchControlled(["-appdata", dir], { saveSettings: true });
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
    throw new Error(`session-restore-tab-state: exit code ${exitCode}, want 0`);
  }

  const text = readFileSync(join(dir, "SumatraPDF-settings.txt"), "utf8");
  const m = /TabStates \[\s*\[[^\]]*?DisplayMode = ([^\n]+)\n[^\]]*?PageNo = (\S+)\n[^\]]*?Zoom = ([^\n]+)\n/.exec(
    text,
  );
  if (!m) {
    throw new Error("session-restore-tab-state: no TabState in saved settings");
  }
  const got = `${m[1]!.trim()} / page ${m[2]} / ${m[3]!.trim()}`;
  const want = `continuous / page ${WANT_PAGE} / fit width`;
  if (got !== want) {
    throw new Error(`session-restore-tab-state: restored tab is '${got}', want '${want}'`);
  }
  rmSync(dir, { recursive: true, force: true });
}

if (import.meta.main) {
  await runStandalone(testit);
}
