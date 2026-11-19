// #6539: the CmdCaret* commands move the keyboard selection (F7) caret like the
// arrow / Home / End keys, so other keys (Emacs-style Ctrl+n) can be bound to
// them. Outside keyboard selection they run what the matching key does there.
import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { LINE1, makeTextPdf, getState, waitForState } from "./issue-4684";
import { EXE, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util";
import { FRAME_CLASS, sendCommandSync } from "./win-automation";
import { sleep, postChar, waitForTopWindow } from "./winapi";

async function currentPage(client: ControlClient): Promise<number> {
  const res = await client.request(ControlCommand.TestCurrentTab, []);
  const m = / page=(\d+)/.exec(String(res[1]));
  if (!m) throw new Error(`issue-6539: no current page: ${res[1]}`);
  return +m[1]!;
}

async function waitForPage(client: ControlClient, page: number): Promise<void> {
  const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
  let got = 0;
  while (Date.now() < deadline) {
    got = await currentPage(client);
    if (got === page) return;
    await sleep(25);
  }
  throw new Error(`issue-6539: expected page ${page}, at ${got}`);
}

async function run(client: ControlClient, frame: number): Promise<void> {
  await client.waitForRenderIdle();
  const cmd = (name: string) => sendCommandSync(frame, cmdId(name));

  // outside keyboard selection a caret command does what its key does there
  cmd("CmdCaretDocEnd");
  await waitForPage(client, 2);
  cmd("CmdCaretDocStart");
  await waitForPage(client, 1);

  cmd("CmdSelectTextViaKeyboard");
  await waitForState(client, (s) => s.active && s.page === 1 && s.glyph === 0);

  for (let i = 0; i < 3; i++) cmd("CmdCaretRight");
  let { state, dump } = await waitForState(client, (s) => s.glyph === 3);
  if (state.selRects !== 0) throw new Error(`issue-6539: a plain caret move selected text\n${dump}`);

  cmd("CmdCaretLeft");
  await waitForState(client, (s) => s.glyph === 2);
  cmd("CmdCaretLineEnd");
  await waitForState(client, (s) => s.glyph === LINE1.length);
  cmd("CmdCaretLineStart");
  await waitForState(client, (s) => s.glyph === 0);

  // visual mode: caret commands extend the selection, like the keys
  await postChar(frame, "v");
  await waitForState(client, (s) => s.visual);
  cmd("CmdCaretWordRight");
  cmd("CmdCaretWordRight");
  ({ state, dump } = await waitForState(client, (s) => s.glyph > 0 && s.selRects > 0));
  if (!LINE1.startsWith(state.text.trimEnd()) || state.text.trim().split(" ").length !== 2)
    throw new Error(`issue-6539: two words right should select two words, got "${state.text}"\n${dump}`);
  await postChar(frame, "v");
  await waitForState(client, (s) => !s.visual);

  // inside keyboard selection, document end moves the caret, not just the view
  cmd("CmdCaretDocEnd");
  await waitForState(client, (s) => s.page === 2 && s.glyph > 0);
  cmd("CmdCaretDocStart");
  await waitForState(client, (s) => s.page === 1 && s.glyph === 0);

  ({ state, dump } = await getState(client));
  if (!state.active) throw new Error(`issue-6539: caret commands ended keyboard selection\n${dump}`);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6539.pdf");
  writeFileSync(pdf, makeTextPdf());
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      if (!frame) throw new Error("issue-6539: no frame window");
      await run(client, frame);
    },
    [pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
