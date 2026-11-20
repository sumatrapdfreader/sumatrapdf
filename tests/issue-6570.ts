// #6570: CmdToggleTextSelecting does what `v` does in keyboard selection (F7):
// starts / stops selecting at the caret. Takes the toggle `state` argument.
import { writeFileSync } from "node:fs";
import type { ControlClient } from "./control";
import { withControlledSumatra } from "./control";
import { getState, makeTextPdf, waitForState } from "./issue-4684";
import { EXE, runStandalone, tmpPath } from "./util";
import { FRAME_CLASS } from "./win-automation";
import { sendCopyDataW, sleep, waitForTopWindow } from "./winapi";

const kCopyDataDdeW = 0x44646557;

async function dde(client: ControlClient, frame: number, cmd: string): Promise<void> {
  if (!sendCopyDataW(frame, kCopyDataDdeW, cmd)) throw new Error(`issue-6570: DDE ${cmd} not handled`);
  await client.waitForRenderIdle();
}

function anchorGlyph(dump: string): number {
  const m = /anchorGlyph=(-?\d+)/.exec(dump);
  if (!m) throw new Error(`issue-6570: no anchorGlyph\n${dump}`);
  return +m[1]!;
}

// state must hold across a settle period, not just flicker into it
async function expectVisual(client: ControlClient, visual: boolean, what: string) {
  await waitForState(client, (s) => s.visual === visual);
  await sleep(200);
  const got = await getState(client);
  if (got.state.visual !== visual || !got.state.active) {
    throw new Error(`issue-6570: ${what}: visual=${got.state.visual} active=${got.state.active}\n${got.dump}`);
  }
  return got;
}

async function run(client: ControlClient, frame: number): Promise<void> {
  await client.waitForRenderIdle();

  // F7 off: nothing happens
  await dde(client, frame, "[CmdToggleTextSelecting]");
  await sleep(200);
  let { state, dump } = await getState(client);
  if (state.active || state.visual) throw new Error(`issue-6570: ran with F7 off\n${dump}`);

  await dde(client, frame, "[CmdSelectTextViaKeyboard on]");
  await waitForState(client, (s) => s.active);

  await dde(client, frame, "[CmdToggleTextSelecting on]");
  ({ state, dump } = await expectVisual(client, true, "first 'on'"));
  const anchor = anchorGlyph(dump);

  await dde(client, frame, "[CmdCaretRight]");
  await dde(client, frame, "[CmdCaretRight]");
  ({ state, dump } = await waitForState(client, (s) => s.selRects > 0));
  const caret = state.glyph;

  await dde(client, frame, "[CmdToggleTextSelecting on]");
  ({ state, dump } = await expectVisual(client, true, "'on' when already on"));
  if (anchorGlyph(dump) !== anchor) throw new Error(`issue-6570: 'on' moved the anchor\n${dump}`);

  await dde(client, frame, "[CmdToggleTextSelecting off]");
  ({ state, dump } = await expectVisual(client, false, "first 'off'"));
  if (state.glyph !== caret) throw new Error(`issue-6570: 'off' moved the caret\n${dump}`);
  await dde(client, frame, "[CmdToggleTextSelecting off]");
  await expectVisual(client, false, "'off' when already off");

  // no state: a toggle, selecting from the caret
  await dde(client, frame, "[CmdToggleTextSelecting]");
  ({ state, dump } = await expectVisual(client, true, "plain toggle"));
  if (anchorGlyph(dump) !== caret) throw new Error(`issue-6570: toggle didn't anchor at caret\n${dump}`);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6570.pdf");
  writeFileSync(pdf, makeTextPdf());
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      if (!frame) throw new Error("issue-6570: no frame window");
      await run(client, frame);
    },
    [pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
