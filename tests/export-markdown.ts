// Export as Markdown... (CmdExportMarkdown): the dialog writes the document as
// Markdown next to it, the same as `SumatraPDF cli markdown`.
//
// ng only: orig has no Markdown export.
// Run: bun tests/export-markdown.ts [--no-build]

import { copyFileSync, existsSync, mkdirSync, readFileSync, rmSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { cmdId, EXE, pollUntil, ROOT, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const name = "export-markdown";
const kTool = "pdfexportmarkdown";

async function click(client: ControlClient, label: string): Promise<void> {
  const line = await pollUntil(
    async () => {
      const res = await client.request(ControlCommand.TestToolWindow, ["layout", kTool, "", 0, 0, 0, 0]);
      return String(res[1] ?? "")
        .split("\n")
        .find((l) => l.startsWith("node ") && l.includes(`label='${label}'`));
    },
    (l) => !!l,
    { timeoutMs: 10000 * SLOW_BUILD_FACTOR, error: () => `${name}: no '${label}' in the dialog` },
  );
  const m = /rect=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(line!)!;
  const x = +m[1]! + Math.floor(+m[3]! / 2);
  const y = +m[2]! + Math.floor(+m[4]! / 2);
  await client.request(ControlCommand.TestToolWindow, ["input", kTool, "click", x, y, 0, 0]);
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log(`SKIP ${name}: ng only (run with -ng)`);
    return;
  }
  const dir = tmpPath(name);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "zlib.pdf");
  copyFileSync(join(ROOT, "ext", "a-zlib", "zlib.3.pdf"), pdf);
  const md = join(dir, "zlib.md");

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdExportMarkdown"));
    await click(client, "Export");
    await pollUntil(
      async () => existsSync(md),
      (ok) => ok,
      { timeoutMs: 15000 * SLOW_BUILD_FACTOR, error: () => `${name}: ${md} wasn't written` },
    );
  } finally {
    client.close();
    await killAndWait(proc);
  }
  const want = join(dir, "cli.md");
  const p = Bun.spawnSync([EXE, "cli", "markdown", pdf, "-o", want]);
  if (p.exitCode !== 0) {
    throw new Error(`${name}: cli markdown failed: ${p.stderr.toString()}`);
  }
  const got = readFileSync(md, "utf8");
  if (got !== readFileSync(want, "utf8") || !got.includes("# ")) {
    throw new Error(`${name}: the export differs from cli markdown:\n${got.slice(0, 400)}`);
  }
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
