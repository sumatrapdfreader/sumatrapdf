// #6437: files opened from a temporary folder (%TEMP%).
//  - a notification offers to copy the file; closing it turns off
//    TemporaryFileShowNotification
//  - CmdCopyTemporaryFileToNonTemporary copies it to TemporaryFileDestDir,
//    adding ".<N>" before the extension on a name conflict, and re-opens it
//  - TemporaryFileAutoMove copies it there on open, and reuses a file with the
//    same content instead of making another copy
//
// Run: bun tests/issue-6437.ts [--no-build]
import { existsSync, mkdirSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { basename, dirname, join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { EXE, makeMinimalPdf, runStandalone, tmpPath } from "./util";
import { sleep } from "./winapi";

type State = { temp: boolean; notif: boolean; show: boolean; path: string };

async function state(client: ControlClient, action = ""): Promise<State | null> {
  const res = await client.request(ControlCommand.TestTemporaryFile, [action]);
  const m = /^OK temp=(\d) notif=(\d) show=(\d) path=(.*)$/.exec(String(res[1] ?? "").trim());
  if (res[0] !== 0 || !m) {
    return null;
  }
  return { temp: m[1] === "1", notif: m[2] === "1", show: m[3] === "1", path: m[4]! };
}

async function waitState(client: ControlClient, what: string, pred: (s: State) => boolean): Promise<State> {
  const deadline = Date.now() + 10000;
  let s: State | null = null;
  for (;;) {
    s = await state(client);
    if (s && pred(s)) {
      return s;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6437: ${what}; state: ${JSON.stringify(s)}`);
    }
    await sleep(50);
  }
}

function writeSettings(dir: string, lines: string[]): string {
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const all = ["UiLanguage = en", "CheckForUpdates = false", "RestoreSession = false", ...lines, ""];
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), all.join("\n"));
  return dir;
}

export async function testit(): Promise<void> {
  const tempDir = join(tmpdir(), `sumatra-issue-6437-${process.pid}`);
  rmSync(tempDir, { recursive: true, force: true });
  mkdirSync(tempDir, { recursive: true });
  const tempPdf = join(tempDir, "doc.pdf");
  const content = makeMinimalPdf("temporary");
  writeFileSync(tempPdf, content);
  const destDir = tmpPath("issue-6437-dest");

  try {
    // 1. the notification, and closing it
    let appdata = writeSettings(tmpPath("issue-6437-app1"), []);
    await withControlledSumatra(
      EXE,
      async (client) => {
        const s = await waitState(client, "a %TEMP% file should show the notification", (s) => s.notif);
        if (!s.temp || !s.show) {
          throw new Error(`issue-6437: the file should count as temporary: ${JSON.stringify(s)}`);
        }
        await state(client, "dismiss");
        await waitState(client, "closing the notification should turn it off", (s) => !s.notif && !s.show);
      },
      ["-appdata", appdata, tempPdf],
    );

    // 2. the command: a different doc.pdf is already there, so doc.1.pdf
    rmSync(destDir, { recursive: true, force: true });
    mkdirSync(destDir, { recursive: true });
    writeFileSync(join(destDir, "doc.pdf"), makeMinimalPdf("another document"));
    appdata = writeSettings(tmpPath("issue-6437-app2"), [
      `TemporaryFileDestDir = ${destDir}`,
      "TemporaryFileAutoMove = false",
    ]);
    await withControlledSumatra(
      EXE,
      async (client) => {
        await waitState(client, "the file should open from %TEMP%", (s) => s.temp);
        await client.request(ControlCommand.TestInvokeCommand, ["CmdCopyTemporaryFileToNonTemporary"]);
        const s = await waitState(client, "the command should re-open the copy", (s) => !s.temp);
        if (dirname(s.path).toLowerCase() !== destDir.toLowerCase() || basename(s.path) !== "doc.1.pdf") {
          throw new Error(`issue-6437: want ${join(destDir, "doc.1.pdf")}, got ${s.path}`);
        }
        if (s.notif) {
          throw new Error("issue-6437: the notification should be gone after copying");
        }
      },
      ["-appdata", appdata, tempPdf],
    );
    if (!existsSync(tempPdf)) {
      throw new Error("issue-6437: the original temporary file should stay");
    }

    // 3. auto copy: doc.1.pdf now has the same content, so it is reused
    appdata = writeSettings(tmpPath("issue-6437-app3"), [`TemporaryFileDestDir = ${destDir}`]);
    await withControlledSumatra(
      EXE,
      async (client) => {
        const s = await waitState(client, "the file should open from its copy", (s) => s.path.length > 0);
        if (s.temp || basename(s.path) !== "doc.1.pdf" || s.notif) {
          throw new Error(`issue-6437: want the existing doc.1.pdf, no notification; got ${JSON.stringify(s)}`);
        }
      },
      ["-appdata", appdata, tempPdf],
    );
    const names = readdirSync(destDir).sort();
    if (names.join(",") !== "doc.1.pdf,doc.pdf") {
      throw new Error(`issue-6437: the same content shouldn't be copied again; folder has ${names.join(",")}`);
    }
  } finally {
    rmSync(tempDir, { recursive: true, force: true });
  }
  console.log("issue-6437: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
