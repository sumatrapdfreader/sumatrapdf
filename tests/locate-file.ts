// Locate File (CmdLocateFile): a home page entry whose file moved offers
// Locate File...; picking where it is now moves its history entry there (the
// page it was read to comes along) and opens it. The Open dialog is answered
// through -dbg-control.
//
// Run: bun tests/locate-file.ts [--no-build] [-ng]

import { copyFileSync, mkdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { cmdId, ROOT, pollUntil, runStandalone, tmpPath, writeAppdata } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { getWindowText } from "./winapi.ts";

const name = "locate-file";

// the selected home entry's context menu, as "cmd\ttitle" rows
async function homeMenu(client: ControlClient): Promise<string[]> {
  const res = await client.request(ControlCommand.TestContextMenuAt, [0, 0]);
  return String(res[1] ?? "")
    .split("\n")
    .filter((l) => l)
    .map((l) => {
      const f = l.split("\t");
      return `${f[2]}\t${f[5]}`;
    });
}

async function currentPage(client: ControlClient): Promise<number> {
  const out = String((await client.request(ControlCommand.TestScrollState, []))[1] ?? "");
  return Number(/page=(\d+)/.exec(out)?.[1] ?? 0);
}

export async function testit(): Promise<void> {
  const dir = tmpPath(`${name}-docs`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(join(dir, "new-place"), { recursive: true });
  // it was read to page 2 where it used to be. Pinned: orig hides a missing
  // file that isn't (RemoveNonExistentFilesAsync), ng shows them all
  const oldPath = join(dir, "old-place", "moved.pdf");
  const newPath = join(dir, "new-place", "moved.pdf");
  copyFileSync(join(ROOT, "ext", "a-zlib", "zlib.3.pdf"), newPath);

  const appdata = writeAppdata(
    name,
    [
      "UiLanguage = en",
      "RestoreSession = false",
      "CheckForUpdates = false",
      "RememberOpenedFiles = true",
      "HomePageViewMode = list",
      `FileStates [\n\t[\n\t\tFilePath = ${oldPath}\n\t\tOpenCount = 5\n\t\tPageNo = 2\n\t\tIsPinned = true\n\t]\n]`,
    ].join("\n"),
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appdata]);
  try {
    const menu = await pollUntil(
      () => homeMenu(client),
      (m) => m.length > 0,
      { error: () => `${name}: no home entry menu` },
    );
    if (!menu.includes("CmdLocateFile\tLocate File...")) {
      throw new Error(`${name}: no Locate File... for a moved file:\n${menu.join("\n")}`);
    }

    await client.request(ControlCommand.TestOpenFileNames, [newPath]);
    sendCommand(frame, cmdId("CmdLocateFile"));
    await pollUntil(
      () => getWindowText(frame).includes("moved.pdf"),
      (ok) => ok,
      { error: () => `${name}: not opened: '${getWindowText(frame)}'` },
    );
    await pollUntil(
      () => currentPage(client),
      (p) => p === 2,
      { error: (p) => `${name}: opened on page ${p}, want the remembered 2` },
    );
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
