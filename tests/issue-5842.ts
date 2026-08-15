// Regression test for issue #5842. Deferred TOC navigation and the Markdown
// WebView navigation URL must both preserve the named heading fragment.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import {
  ControlClient,
  ControlCommand,
  withControlledSumatra,
} from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const TARGET_DEST_NO = 3;
const MIN_TARGET_SCROLL_Y = 500;

async function requestUntilReady(
  client: ControlClient,
  destNo: number,
  minScrollY: number,
  expected: string,
): Promise<string> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkdownTocNavigate, [
      destNo,
      minScrollY,
    ]);
    const exitCode = res[0] as number;
    const output = String(res[1] ?? "").trim();
    if (exitCode === 0 && output.startsWith(expected)) {
      return output;
    }
    if (exitCode !== 2 || !output.startsWith("NOTREADY")) {
      throw new Error(`#5842 WebView TOC navigation failed: ${output}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`#5842 WebView TOC navigation timed out: ${output}`);
    }
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
}

export async function testit(): Promise<void> {
  const proc = Bun.spawn([EXE, "-unit-tests"], {
    stdout: "pipe",
    stderr: "pipe",
  });
  const [stdout, stderr, exitCode] = await Promise.all([
    new Response(proc.stdout).text(),
    new Response(proc.stderr).text(),
    proc.exited,
  ]);
  const output = (stdout + stderr).trim();
  if (exitCode !== 0) {
    throw new Error(
      `#5842 app unit tests failed (exit ${exitCode}):\n${output}`,
    );
  }

  const fixtureDir = tmpPath("issue-5842-data");
  rmSync(fixtureDir, { recursive: true, force: true });
  mkdirSync(fixtureDir, { recursive: true });
  const markdown = join(fixtureDir, "issue-5842.md");
  const paragraphs = Array.from(
    { length: 120 },
    (_, i) =>
      `Paragraph ${i + 1}: enough content to put the target heading below the viewport.`,
  );
  writeFileSync(
    markdown,
    ["# Start", ...paragraphs, "## Target Heading", "Target content."].join(
      "\n\n",
    ),
  );

  const appdata = tmpPath("issue-5842-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    [
      "MarkdownUI [",
      "\tUseFixedPageUI = false",
      "]",
      "RestoreSession = false",
      "ShowStartPage = false",
      "",
    ].join("\n"),
  );

  const result = await withControlledSumatra(
    EXE,
    async (client) => {
      const started = await requestUntilReady(
        client,
        TARGET_DEST_NO,
        MIN_TARGET_SCROLL_Y,
        "NAVIGATING",
      );
      const landed = await requestUntilReady(
        client,
        0,
        MIN_TARGET_SCROLL_Y,
        "OK",
      );
      return `${started}\n${landed}`;
    },
    ["-appdata", appdata, markdown],
  );
  console.log(`issue-5842: ${result}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
