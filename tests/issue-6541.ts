// #6541: in keyboard selection (F7), a find moves the caret to the match, like
// Emacs isearch: forward to its end, backward to its start. In plain mode the
// match becomes the selection; in visual mode the selection grows to it.
import { writeFileSync } from "node:fs";
import type { ControlClient } from "./control";
import { withControlledSumatra } from "./control";
import { LINE1, makeTextPdf, waitForState } from "./issue-4684";
import { EXE, cmdId, runStandalone, tmpPath } from "./util";
import { FRAME_CLASS, sendCommandSync } from "./win-automation";
import { postChar, waitForTopWindow } from "./winapi";

// LINE1 is "The quick brown fox jumps over the lazy dog": "the" again at 31
const SECOND_THE = LINE1.indexOf("the ");

async function run(client: ControlClient, frame: number): Promise<void> {
  await client.waitForRenderIdle();
  const cmd = (name: string) => sendCommandSync(frame, cmdId(name));

  cmd("CmdSelectTextViaKeyboard");
  await waitForState(client, (s) => s.active && s.page === 1 && s.glyph === 0);

  // select "The" and search for the next one
  await postChar(frame, "v");
  await waitForState(client, (s) => s.visual);
  for (let i = 0; i < 3; i++) cmd("CmdCaretRight");
  await waitForState(client, (s) => s.text === "The");
  await postChar(frame, "v");
  await waitForState(client, (s) => !s.visual);
  cmd("CmdFindNextSel");

  // plain mode: the caret is at the match's end and the match is selected
  const end = SECOND_THE + 3;
  await waitForState(client, (s) => s.page === 1 && s.glyph === end && s.text.toLowerCase() === "the");

  // visual mode, backward: the selection grows from the caret to the match start
  await postChar(frame, "v");
  await waitForState(client, (s) => s.visual);
  cmd("CmdFindPrev");
  await waitForState(client, (s) => s.glyph === 0 && s.text === LINE1.slice(0, end));
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6541.pdf");
  writeFileSync(pdf, makeTextPdf());
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      if (!frame) throw new Error("issue-6541: no frame window");
      await run(client, frame);
    },
    [pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
