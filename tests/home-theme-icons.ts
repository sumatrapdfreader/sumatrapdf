// A theme change used to free every cached SVG icon pixmap while the home
// page still held raw pointers to them (HomeViewIconCtrl::pixmap and friends
// are set only by HomePageSyncChrome). UpdateAfterThemeChange repaints
// synchronously, so the very next paint read freed memory and crashed in
// PixmapAsPremulBgra (crash 2026-09-07-13-07-95c3).
//
// Run: bun tests/home-theme-icons.ts [--no-build]   (best under -asan)
import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util";
import { postMessage, WM_CLOSE } from "./winapi";
import { killAndWait, launchControlled, sendCommandSync, waitForExit } from "./win-automation";

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = true
HomePageViewMode = list
`;

const kThemeToggles = 4;

// waits for the home page to paint, and proves the UI thread is still serving
async function waitHomePainted(client: ControlClient, what: string): Promise<void> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestHomeListRows, []);
    const exitCode = res[0] as number;
    const out = String(res[1] ?? "");
    if (exitCode === 0) {
      return;
    }
    if (exitCode !== 2) {
      throw new Error(`home-theme-icons: TestHomeListRows failed ${what}: ${out}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`home-theme-icons: home page never painted ${what}: ${out}`);
    }
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("home-theme-icons");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });

  const doc = join(dir, "a.pdf");
  copyFileSync(join(ROOT, "ext", "a-zlib", "zlib.3.pdf"), doc);
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `${SETTINGS}FileStates [\n\t[\n\t\tFilePath = ${doc}\n\t\tOpenCount = 1\n\t]\n]\n`,
  );

  const { proc, client, frame } = await launchControlled(["-appdata", dir]);
  try {
    await client.setNotificationsEnabled(false);
    await waitHomePainted(client, "on startup");

    // each toggle drops the icon cache and repaints the home page synchronously
    for (let i = 0; i < kThemeToggles; i++) {
      sendCommandSync(frame, cmdId("CmdToggleLightDarkTheme"));
      if (proc.exitCode !== null) {
        throw new Error(`home-theme-icons: the app died on theme change ${i + 1} (exit ${proc.exitCode})`);
      }
      await waitHomePainted(client, `after theme change ${i + 1}`);
    }

    postMessage(frame, WM_CLOSE, 0, 0);
    if (!(await waitForExit(proc))) {
      throw new Error("home-theme-icons: SumatraPDF didn't exit after WM_CLOSE");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("home-theme-icons: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
