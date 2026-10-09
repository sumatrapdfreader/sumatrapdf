// #6280: Ctrl+F, search, Esc, Ctrl+F again leaves the term in the find box.
// Reopening must highlight its matches without moving to one, and a single
// Enter must restart the search (it took two: the first one never finished).
//
// Run:  bun tests/issue-6280.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlCommand, type ControlClient } from "./control.ts";
import { assemblePdf, cmdId, pollUntil, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util.ts";
import { launchControlled, pressKey, sendCommand, typeIntoInput, waitForFocusClass } from "./win-automation.ts";
import { getClassName, getParentWindow, VK_ESCAPE, VK_RETURN } from "./winapi.ts";

const kTerm = "tagged";
const kPageCount = 4;
// pages that contain kTerm
const kFirstHitPage = 1;
const kSecondHitPage = 3;

type FindState = { matches: number; hitPage: number; busy: number; page: number; raw: string };

function makeSearchPdf(): string {
  const pageObj = (i: number) => 4 + i * 2;
  const kids = Array.from({ length: kPageCount }, (_, i) => `${pageObj(i)} 0 R`).join(" ");
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    `<< /Type /Pages /Count ${kPageCount} /Kids [${kids}] >>`,
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
  ];
  for (let i = 0; i < kPageCount; i++) {
    const pageNo = i + 1;
    const hasTerm = pageNo === kFirstHitPage || pageNo === kSecondHitPage;
    const text = hasTerm ? `page ${pageNo} holds ${kTerm} data` : `page ${pageNo} is plain`;
    const content = `BT /F1 24 Tf 72 700 Td (${text}) Tj ET`;
    objs.push(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> ` +
        `/Contents ${pageObj(i) + 1} 0 R >>`,
    );
    objs.push(`<< /Length ${content.length} >>\nstream\n${content}\nendstream`);
  }
  return assemblePdf(objs);
}

async function findState(client: ControlClient): Promise<FindState> {
  const res = await client.request(ControlCommand.TestFindUiState, ["state"]);
  const raw = String(res[1] ?? "").trim();
  const values: Record<string, number> = {};
  for (const m of raw.matchAll(/(\w+)=(\d+)/g)) {
    values[m[1]] = Number(m[2]);
  }
  return {
    matches: values.matches ?? -1,
    hitPage: values.hitPage ?? -1,
    busy: values.busy ?? -1,
    page: values.page ?? -1,
    raw,
  };
}

async function ngFind(client: ControlClient, action: string, arg = ""): Promise<string> {
  const res = await client.request(ControlCommand.TestFindUiState, arg ? [action, arg] : [action]);
  if (res[0] !== 0) {
    throw new Error(`issue-6280: ${action} failed: ${String(res[1] ?? "")}`);
  }
  return String(res[1] ?? "");
}

// the find box is drawn in the frame; CmdFindFirst is posted, so wait until it shows
async function waitFindOpen(client: ControlClient): Promise<void> {
  const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
  let last = "";
  for (;;) {
    last = String((await client.request(ControlCommand.TestFindUiState, ["state"]))[1] ?? "");
    if (/compact=1/.test(last) || /floating=1/.test(last)) {
      return;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6280: find bar did not open (${last})`);
    }
    await new Promise((r) => setTimeout(r, 40));
  }
}

// waits for the search to settle, then checks where it ended up
async function expectSettled(client: ControlClient, what: string, want: Partial<FindState>): Promise<void> {
  const state = await pollUntil(
    () => findState(client),
    (s) => s.busy === 0,
    { timeoutMs: 8000 * SLOW_BUILD_FACTOR, error: (s) => `issue-6280: ${what}: search never finished (${s.raw})` },
  );
  for (const [key, value] of Object.entries(want)) {
    if (state[key as keyof FindState] !== value) {
      throw new Error(`issue-6280: ${what}: expected ${key}=${value}, got ${state.raw}`);
    }
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6280.pdf");
  writeFileSync(pdf, makeSearchPdf(), "latin1");

  const { client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    if (USE_NG) {
      // the find box is a gpui input. "set" and "enter" follow the bar;
      // Escape is the key the frame handles
      sendCommand(frame, cmdId("CmdFindFirst"));
      await waitFindOpen(client);
      await ngFind(client, "set", kTerm);
      await ngFind(client, "enter");
      await expectSettled(client, "first search", { matches: 2, hitPage: kFirstHitPage });

      await client.request(ControlCommand.TestInput, ["key", VK_ESCAPE, 0, 0, 0]);
      await expectSettled(client, "after Esc", { matches: 0, hitPage: 0 });

      sendCommand(frame, cmdId("CmdFindFirst"));
      await waitFindOpen(client);
      await expectSettled(client, "reopened find", { matches: 2, hitPage: 0, page: kFirstHitPage });

      await ngFind(client, "enter");
      await expectSettled(client, "one Enter after reopening", { matches: 2, hitPage: kFirstHitPage });

      await ngFind(client, "enter");
      await expectSettled(client, "second Enter", { hitPage: kSecondHitPage, page: kSecondHitPage });
      return;
    }

    sendCommand(frame, cmdId("CmdFindFirst"));
    const edit = await waitForFocusClass(frame, "Edit");
    const parent = getParentWindow(edit);
    const combo = parent && getClassName(parent) === "ComboBox" ? parent : edit;
    await typeIntoInput(combo, kTerm, false);
    await pressKey(edit, VK_RETURN);
    await expectSettled(client, "first search", { matches: 2, hitPage: kFirstHitPage });

    await pressKey(edit, VK_ESCAPE);
    await expectSettled(client, "after Esc", { matches: 0, hitPage: 0 });

    sendCommand(frame, cmdId("CmdFindFirst"));
    const edit2 = await waitForFocusClass(frame, "Edit");
    await expectSettled(client, "reopened find", { matches: 2, hitPage: 0, page: kFirstHitPage });

    await pressKey(edit2, VK_RETURN);
    await expectSettled(client, "one Enter after reopening", { matches: 2, hitPage: kFirstHitPage });

    await pressKey(edit2, VK_RETURN);
    await expectSettled(client, "second Enter", { hitPage: kSecondHitPage, page: kSecondHitPage });
  } finally {
    await client.quit();
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
