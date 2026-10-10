// Regression test for issue #5718.
//
// When text is selected and the user opens the context menu over text and picks
// "Copy Selection", the selected text must be copied. The bug was that building
// the context menu called ReadAloudCanReadFromCursor(), which mutated the live
// TextSelection's start glyph, so the copied text ran from the old selection end
// to the cursor instead of the actual selection.
//
// This drives the real app via -dbg-control: it loads a one-line PDF, selects
// word1..word2 on the live document, then runs the same read-only cursor check
// the context menu performs and verifies the selection text is unchanged.

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath, makeOneLinePdf } from "./util.ts";

const LINE = "alpha beta gamma delta epsilon";
const WORD1 = "alpha";
const WORD2 = "epsilon";
const CURSOR_WORD = "gamma"; // distinct from the selection start, so the bug is observable

async function requestWithRetry(client: ControlClient): Promise<string> {
  // the document is loaded asynchronously after the app starts; retry until the
  // window/document is ready (the command returns "NOTREADY ..." until then)
  const deadline = Date.now() + 10_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestContextMenuSelection, [WORD1, WORD2, CURSOR_WORD]);
    const exitCode = res[0] as number;
    const raw = (res[1] as string) ?? "";
    if (!raw.includes("NOTREADY")) {
      if (exitCode !== 0) {
        throw new Error(`issue-5718: context menu corrupted selection: ${raw.trim()}`);
      }
      return raw.trim();
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5718: document never became ready: ${raw.trim()}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-5718.pdf");
  writeFileSync(pdfPath, makeOneLinePdf(LINE));

  const result = await withControlledSumatra(EXE, (client) => requestWithRetry(client), [pdfPath]);
  console.log(`issue-5718: ${result}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
