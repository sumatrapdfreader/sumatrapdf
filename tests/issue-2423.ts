// Tests for https://github.com/sumatrapdfreader/sumatrapdf/issues/2423
//
// Copying a text selection dropped leading spaces and blank lines, so a code
// listing lost its indentation. Indentation is rebuilt from glyph positions and
// blank lines from the vertical gap between selected lines.
//
// Fixture: tests/issue-2423.pdf (Courier listing with a 4-space indented line
// and a blank line; content stream puts real space glyphs there).
// Run:  bun tests/issue-2423.ts [--no-build]

import { existsSync } from "node:fs";
import { join } from "node:path";
import { cmdId, EXE, runStandalone } from "./util.ts";
import { launchControlled, sendCommandSync, killAndWait } from "./win-automation.ts";
import { getClipboardText } from "./winapi.ts";

const PDF = join(import.meta.dir, "issue-2423.pdf");

const EXPECTED = ["def hello_world():", '    print("Hello World!")', "", "hello_world()"].join("\r\n");

export async function testit(): Promise<void> {
  if (!existsSync(PDF)) {
    throw new Error(`fixture missing: ${PDF}`);
  }
  if (!existsSync(EXE)) {
    throw new Error(`exe missing: ${EXE}`);
  }

  const { proc, client, frame } = await launchControlled([PDF]);
  try {
    await client.waitForRenderIdle();
    await client.seedTextSelection(1);
    sendCommandSync(frame, cmdId("CmdCopySelection"));
    const got = (await getClipboardText(2000)).replace(/[\r\n]+$/, "");
    if (got !== EXPECTED) {
      throw new Error(`#2423: clipboard\n${JSON.stringify(got)}\nexpected\n${JSON.stringify(EXPECTED)}`);
    }
    console.log(`#2423: clipboard ok: ${JSON.stringify(got)}`);
  } finally {
    client.close();
    try {
      await killAndWait(proc);
    } catch {
      // already exited
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
