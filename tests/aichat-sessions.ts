// The AI chat session combo is filled off the UI thread: opening the Codex
// panel lists a fake $CODEX_HOME's sessions for the document's dir and
// auto-selects the most recent one.
import { copyFileSync, mkdirSync, rmSync, utimesSync, writeFileSync } from "node:fs";
import { delimiter, join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, ROOT, cmdId, pollUntil, runStandalone, tmpPath, writeAppdata } from "./util.ts";
import { FRAME_CLASS, sendCommandSync } from "./win-automation.ts";
import { waitForTopWindow } from "./winapi.ts";

const OLD_ID = "019a0000-0000-7000-8000-000000000001";
const NEW_ID = "019a0000-0000-7000-8000-000000000002";

// a rollout's first line is its session_meta: the session's id and cwd
function writeRollout(codexHome: string, id: string, cwd: string, mtime: Date) {
  const day = join(codexHome, "sessions", "2026", "11", "23");
  mkdirSync(day, { recursive: true });
  const path = join(day, `rollout-2026-11-23T10-00-00-${id}.jsonl`);
  const meta = { type: "session_meta", payload: { id, cwd } };
  writeFileSync(path, JSON.stringify(meta) + "\n");
  utimesSync(path, mtime, mtime);
}

export async function testit(): Promise<void> {
  if (process.platform !== "win32") return;
  const dir = tmpPath("aichat-sessions");
  rmSync(dir, { recursive: true, force: true });
  const docDir = join(dir, "doc");
  mkdirSync(docDir, { recursive: true });
  const pdf = join(docDir, "doc.pdf");
  copyFileSync(join(ROOT, "ext/a-zlib/zlib.3.pdf"), pdf);

  // a mock codex.exe on PATH so the panel opens
  const compile = Bun.spawnSync([
    "cl",
    "/nologo",
    join(ROOT, "tests/codex-provider.c"),
    `/Fe${join(dir, "codex.exe")}`,
    `/Fo${join(dir, "codex.obj")}`,
  ]);
  if (compile.exitCode !== 0) throw new Error(compile.stderr.toString() + compile.stdout.toString());

  writeRollout(dir, OLD_ID, docDir, new Date("2026-01-01T00:00:00Z"));
  writeRollout(dir, NEW_ID, docDir, new Date("2026-02-01T00:00:00Z"));
  writeRollout(dir, "019a0000-0000-7000-8000-000000000003", join(dir, "elsewhere"), new Date());
  const history = [
    { session_id: OLD_ID, ts: 1, text: "older fake session" },
    { session_id: NEW_ID, ts: 2, text: "newer fake session" },
  ];
  writeFileSync(join(dir, "history.jsonl"), history.map((h) => JSON.stringify(h)).join("\n") + "\n");

  const appdata = writeAppdata("aichat-sessions-settings", "");
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      await client.waitForRenderIdle();
      sendCommandSync(frame, cmdId("CmdAIChatWithOpenAICodex"));
      const expected = "pending=0 sel=1\n+ New Session\nnewer fake session\nolder fake session\n";
      await pollUntil(
        async () => String((await client.request(ControlCommand.TestAIChatSessions))[1]),
        (s) => s === expected,
        { timeoutMs: 15000, error: (s) => `session combo:\n${s}\nexpected:\n${expected}` },
      );
      await client.quit();
    },
    ["-appdata", appdata, pdf],
    { env: { ...process.env, PATH: dir + delimiter + process.env.PATH, CODEX_HOME: dir } },
  );
}

if (import.meta.main) await runStandalone(testit);
