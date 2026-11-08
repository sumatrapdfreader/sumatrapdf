import { homedir } from "node:os";
import { existsSync, mkdirSync, writeFileSync } from "node:fs";
import { delimiter, join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, ROOT, runStandalone, tmpPath, writeAppdata } from "./util.ts";

export async function testit(): Promise<void> {
  if (process.platform !== "win32") return;
  if ([".codex", ".local"].some((root) => existsSync(join(homedir(), root, "bin", "codex.exe")))) {
    console.log("Skipping Codex mock test: a user CLI takes precedence over PATH");
    return;
  }
  const dir = tmpPath("codex-provider");
  mkdirSync(dir, { recursive: true });
  const compile = Bun.spawnSync([
    "cl",
    "/nologo",
    join(ROOT, "tests/codex-provider.c"),
    `/Fe${join(dir, "codex.exe")}`,
    `/Fo${join(dir, "codex.obj")}`,
  ]);
  if (compile.exitCode !== 0) throw new Error(compile.stderr.toString() + compile.stdout.toString());
  const cache = join(dir, "models_cache.json");
  const catalog = (slug: string) =>
    JSON.stringify({
      models: [
        { slug: "hidden-model", visibility: "hide" },
        { visibility: "list", slug },
        { slug, visibility: "list" },
      ],
    });
  writeFileSync(cache, catalog("gpt-6.1-sol"));
  const appdata = writeAppdata("codex-provider-settings", "CodexBuild [\n Model = gpt-6.1-sol\n]\n");
  const failures: string[] = [];
  await withControlledSumatra(
    EXE,
    async (client) => {
      for (const [prompt, expected] of [
        ["model", "gpt-6.1-sol"],
        ["codex-error", "Selected model is unavailable"],
        ["codex-turn-failed", "Selected model requires a newer CLI"],
        ["codex-plain-error", "unexpected argument"],
      ]) {
        const result = await client.request(ControlCommand.TestAIChat, [2, join(dir, "document.pdf"), prompt]);
        const text = String(result[1]);
        // once: a failed turn repeats its error; retries ("Reconnecting...") aren't errors
        const shown = text.split("--- debug log ---")[0];
        const shownOnce = shown.split(expected).length === 2;
        if (
          !text.split("\n").slice(0, 2).join("\n").includes(expected) ||
          !shownOnce ||
          shown.includes("Reconnecting")
        ) {
          failures.push(`${prompt}: ${text}`);
        }
      }
      writeFileSync(cache, catalog("fresh-catalog-model"));
      const refreshed = await client.request(ControlCommand.TestAIChat, [2, join(dir, "document.pdf"), "model"]);
      if (!String(refreshed[1]).includes("fresh-catalog-model")) failures.push(`refresh: ${refreshed[1]}`);
    },
    ["-appdata", appdata],
    { env: { ...process.env, PATH: dir + delimiter + process.env.PATH, CODEX_HOME: dir } },
  );
  if (failures.length) throw new Error(failures.join("\n"));
}

if (import.meta.main) await runStandalone(testit);
