// #5970: "Remove Deleted Files from History" left deleted files on the home
// page. The command skipped every path that was not on a fixed drive (USB,
// mapped share, portable install) and only redrew gWindows[0].
import { copyFileSync, mkdirSync, rmSync, unlinkSync, writeFileSync } from "node:fs";
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

type Row = { path: string; size: string };

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
          rows.push({ size: m[1]!, path: m[6]! });
        }
      }
      return rows;
    }
    if (exitCode !== 2) {
      throw new Error(`issue-5970: TestHomeListRows failed: ${out}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5970: home page never painted its list: ${out}`);
    }
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
}

function hasPath(rows: Row[], path: string): boolean {
  const want = path.toLowerCase();
  return rows.some((r) => r.path.toLowerCase() === want);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-5970");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });

  const src = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const a = join(dir, "gone-a.pdf");
  const b = join(dir, "gone-b.pdf");
  const keep = join(dir, "keep.pdf");
  copyFileSync(src, a);
  copyFileSync(src, b);
  copyFileSync(src, keep);

  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `${SETTINGS}FileStates [\n` +
      `\t[\n\t\tFilePath = ${a}\n\t\tOpenCount = 3\n\t]\n` +
      `\t[\n\t\tFilePath = ${b}\n\t\tOpenCount = 2\n\t]\n` +
      `\t[\n\t\tFilePath = ${keep}\n\t\tOpenCount = 1\n\t]\n` +
      `]\n`,
  );

  const { proc, client, frame } = await launchControlled(["-appdata", dir]);
  try {
    await client.setNotificationsEnabled(false);
    let rows = await homeListRows(client);
    if (!hasPath(rows, a) || !hasPath(rows, b) || !hasPath(rows, keep)) {
      throw new Error(`issue-5970: home list missing seeded files: ${JSON.stringify(rows)}`);
    }

    unlinkSync(a);
    unlinkSync(b);

    sendCommandSync(frame, cmdId("CmdRemoveDeletedFilesFromHistory"));

    const deadline = Date.now() + 10_000;
    for (;;) {
      rows = await homeListRows(client);
      if (!hasPath(rows, a) && !hasPath(rows, b)) {
        break;
      }
      if (Date.now() > deadline) {
        throw new Error(`issue-5970: deleted files still in history after the command: ${JSON.stringify(rows)}`);
      }
      await new Promise((resolve) => setTimeout(resolve, 100));
    }
    if (!hasPath(rows, keep)) {
      throw new Error(`issue-5970: still-existing file was removed: ${JSON.stringify(rows)}`);
    }

    postMessage(frame, WM_CLOSE, 0, 0);
    if (!(await waitForExit(proc))) {
      throw new Error("issue-5970: SumatraPDF didn't exit after WM_CLOSE");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-5970: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
