// Search in the compact find bar, let it finish, then pop out into the
// floating find window: the results list must fill with the matches already
// found, without pressing Enter again. And back: docking keeps the search.
//
// Run: bun tests/find-float-switch-results.ts [--no-build] [-ng]

import { resolve } from "node:path";
import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import { EXE, runStandalone, SLOW_BUILD_FACTOR } from "./util.ts";
import { sleep } from "./winapi.ts";

const PDF = resolve("ext/a-zlib/zlib.3.pdf");
const TERM = "the";

type UiState = { compact: number; floating: number; matches: number; busy: number };

async function uiState(client: ControlClient, action = "state"): Promise<UiState> {
  const res = await client.request(ControlCommand.TestFindUiState, [action]);
  const raw = String(res[1] ?? "");
  const m = /compact=(\d+) floating=(\d+) firstTextLen=-?\d+ matches=(\d+) hitPage=\d+ busy=(\d)/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`find-float-switch-results: TestFindUiState ${action}: ${raw.trim()}`);
  }
  return { compact: +m[1]!, floating: +m[2]!, matches: +m[3]!, busy: +m[4]! };
}

async function waitUi(client: ControlClient, what: string, pred: (s: UiState) => boolean): Promise<UiState> {
  const deadline = Date.now() + 15000 * SLOW_BUILD_FACTOR;
  let s = await uiState(client);
  while (!pred(s)) {
    if (Date.now() > deadline) {
      throw new Error(`find-float-switch-results: ${what}; ${JSON.stringify(s)}`);
    }
    await sleep(50);
    s = await uiState(client);
  }
  return s;
}

// the floating window's results list: "OK term=.. n=.. snippets=.." once it
// has the matches with their snippets
async function waitResults(client: ControlClient): Promise<string> {
  const deadline = Date.now() + 5000 * SLOW_BUILD_FACTOR;
  let last = "";
  while (Date.now() < deadline) {
    const res = await client.request(ControlCommand.TestFindWindowContents);
    last = String(res[1] ?? "").trim();
    if (res[0] === 0 && /^OK term=the n=\d+ snippets=[1-9]/.test(last)) {
      return last;
    }
    await sleep(100);
  }
  throw new Error(`find-float-switch-results: the floating window never listed the results: ${last}`);
}

export async function testit(): Promise<void> {
  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      await client.setNotificationsEnabled(false);
      // -search runs the term in the compact bar (the default find UI)
      let s = await waitUi(
        client,
        "the compact bar search did not finish",
        (st) => st.compact === 1 && !st.busy && st.matches > 0,
      );
      const nMatches = s.matches;

      // pop out: the list must show what the bar already found
      await uiState(client, "toggle-first");
      s = await waitUi(client, "the floating window did not open", (st) => st.floating === 1 && st.compact === 0);
      const out = await waitResults(client);
      const n = +(/ n=(\d+)/.exec(out)?.[1] ?? 0);
      if (n !== nMatches) {
        throw new Error(`find-float-switch-results: the list has ${n} rows, the bar found ${nMatches}: ${out}`);
      }

      // and back (the bar's highlights are not rebuilt on docking; not checked)
      await uiState(client, "toggle-first");
      s = await waitUi(client, "the compact bar did not come back", (st) => st.compact === 1 && !st.busy);
      console.log(`find-float-switch-results: OK (${nMatches} matches)`);
    },
    ["-search", TERM, PDF],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
