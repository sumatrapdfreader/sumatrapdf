// Tab strip groups (Chrome-style tab groups, from PR #6560): the group
// commands, collapsing, Ctrl+Tab skipping collapsed tabs, drag & drop into a
// group, Chrome's keyboard move rules, and groups in a saved session.
//
// Run: bun tests/issue-6560.ts [--no-build]

import { copyFileSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { ROOT, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const SRC_PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
const RED = "#d93025";

type Tab = { name: string; group: number; hidden: boolean; current: boolean };
type Group = { name: string; color: string; collapsed: boolean };
type State = { groups: Group[]; tabs: Tab[] };

function parseState(raw: string): State {
  const st: State = { groups: [], tabs: [] };
  for (const line of raw.split("\n")) {
    let m = /^group \d+ name=(.*) color=(\S+) collapsed=(\d)$/.exec(line.trim());
    if (m) {
      st.groups.push({ name: m[1]!, color: m[2]!.toLowerCase(), collapsed: m[3] === "1" });
      continue;
    }
    m = /^tab \d+ name=(.+) group=(\d+) hidden=(\d) current=(\d)$/.exec(line.trim());
    if (m) {
      st.tabs.push({ name: m[1]!, group: Number(m[2]), hidden: m[3] === "1", current: m[4] === "1" });
    }
  }
  return st;
}

// "Home a.pdf:1 [c.pdf:1] b.pdf" - group after ':', the current tab in [], hidden in ()
function describe(st: State): string {
  return st.tabs
    .map((t) => {
      let s = t.name + (t.group ? `:${t.group}` : "");
      if (t.hidden) {
        s = `(${s})`;
      }
      return t.current ? `[${s}]` : s;
    })
    .join(" ");
}

async function request(client: ControlClient, args: (string | number)[]): Promise<State> {
  const res = await client.request(ControlCommand.TestStripGroups, args);
  if (res[0] !== 0) {
    throw new Error(`issue-6560: TestStripGroups: ${String(res[1] ?? "")}`);
  }
  return parseState(String(res[1] ?? ""));
}

// commands are posted: poll until the strip looks like `want`
async function waitFor(client: ControlClient, want: string, what: string): Promise<State> {
  const deadline = Date.now() + 8000;
  let st = await request(client, [""]);
  while (describe(st) !== want && Date.now() < deadline) {
    await sleep(50);
    st = await request(client, [""]);
  }
  if (describe(st) !== want) {
    throw new Error(`issue-6560: ${what}: want '${want}', got '${describe(st)}'`);
  }
  return st;
}

async function invoke(client: ControlClient, cmd: string): Promise<void> {
  const res = await client.request(ControlCommand.TestInvokeCommand, [cmd]);
  if (res[0] !== 0) {
    throw new Error(`issue-6560: ${cmd}: ${String(res[1] ?? "")}`);
  }
}

async function selectTab(client: ControlClient, idx: number): Promise<void> {
  await request(client, ["select", idx]);
}

function makeDocs(dir: string): string[] {
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  return ["a", "b", "c", "d"].map((n) => {
    const p = join(dir, `${n}.pdf`);
    copyFileSync(SRC_PDF, p);
    return p;
  });
}

async function testCommands(): Promise<void> {
  const docs = makeDocs(tmpPath("issue-6560-cmds"));
  const { proc, client } = await launchControlled(docs);
  try {
    await client.waitForRenderIdle();
    await waitFor(client, "Home a.pdf b.pdf c.pdf [d.pdf]", "opened");

    await selectTab(client, 1);
    await invoke(client, "CmdStripGroupNew");
    await waitFor(client, "Home [a.pdf:1] b.pdf c.pdf d.pdf", "new group");

    // joins after the group's last tab
    await selectTab(client, 3);
    await invoke(client, "CmdStripGroupAddTab 1");
    let st = await waitFor(client, "Home a.pdf:1 [c.pdf:1] b.pdf d.pdf", "add to group");

    await invoke(client, `CmdStripGroupColor ${RED}`);
    const deadline = Date.now() + 8000;
    while (st.groups[0]?.color !== RED && Date.now() < deadline) {
      await sleep(50);
      st = await request(client, [""]);
    }
    if (st.groups[0]?.color !== RED) {
      throw new Error(`issue-6560: color: got ${st.groups[0]?.color}`);
    }

    // collapsing the current tab's group selects the nearest visible tab
    await invoke(client, "CmdStripGroupToggle");
    await waitFor(client, "Home (a.pdf:1) (c.pdf:1) [b.pdf] d.pdf", "collapse");

    // Ctrl+Tab backwards skips the collapsed tabs
    await invoke(client, "CmdPrevTab");
    await waitFor(client, "[Home] (a.pdf:1) (c.pdf:1) b.pdf d.pdf", "prev tab");

    // selecting a hidden tab opens its group
    await selectTab(client, 1);
    await waitFor(client, "Home [a.pdf:1] c.pdf:1 b.pdf d.pdf", "select hidden");

    // dropped on the group's chip: joins at its front
    st = await request(client, ["drop", 4, 1, 1]);
    await waitFor(client, "Home [d.pdf:1] a.pdf:1 c.pdf:1 b.pdf", "drop on chip");

    // keyboard: within the group a swap; at its edge the tab leaves first
    st = await request(client, ["move", 1]);
    await waitFor(client, "Home a.pdf:1 [d.pdf:1] c.pdf:1 b.pdf", "move in group");
    await selectTab(client, 3);
    st = await request(client, ["move", 1]);
    await waitFor(client, "Home a.pdf:1 d.pdf:1 [c.pdf] b.pdf", "move out of group");
    st = await request(client, ["move", -1]);
    await waitFor(client, "Home a.pdf:1 d.pdf:1 [c.pdf:1] b.pdf", "move into group");

    await invoke(client, "CmdStripGroupUngroup");
    st = await waitFor(client, "Home a.pdf d.pdf [c.pdf] b.pdf", "ungroup");
    if (st.groups.length !== 0) {
      throw new Error(`issue-6560: ungroup left ${st.groups.length} group(s)`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

// a session with a collapsed group comes back, and is saved again on exit
async function testSession(): Promise<void> {
  const dir = tmpPath("issue-6560-session");
  const docs = makeDocs(join(dir, "docs"));
  const tabState = (p: string, extra: string) => `\t\t\t[\n\t\t\t\tFilePath = ${p}\n${extra}\t\t\t]`;
  const grouped = (collapsed: boolean) =>
    `\t\t\t\tGroup = 1\n\t\t\t\tGroupName = Work\n\t\t\t\tGroupColor = ${RED}\n` +
    `\t\t\t\tGroupCollapsed = ${collapsed}\n`;
  const settingsPath = join(dir, "SumatraPDF-settings.txt");
  writeFileSync(
    settingsPath,
    `UiLanguage = en\nCheckForUpdates = false\nRestoreSession = true\nRememberOpenedFiles = true\n` +
      `LazyLoading = false\nSessionData [\n\t[\n\t\tTabStates [\n` +
      [tabState(docs[0]!, grouped(true)), tabState(docs[1]!, grouped(true)), tabState(docs[2]!, "")].join("\n") +
      `\n\t\t]\n\t\tTabIndex = 3\n\t]\n]\n`,
  );
  const { proc, client } = await launchControlled(["-appdata", dir], { saveSettings: true });
  try {
    await client.waitForSessionRestored();
    const st = await waitFor(client, "Home (a.pdf:1) (b.pdf:1) [c.pdf]", "restored");
    const g = st.groups[0];
    if (!g || g.name !== "Work" || g.color !== RED || !g.collapsed) {
      throw new Error(`issue-6560: restored group: ${JSON.stringify(g)}`);
    }
    await client.quit();
  } finally {
    client.close();
    await killAndWait(proc);
  }
  const saved = readFileSync(settingsPath, "utf8");
  for (const want of ["Group = 1", "GroupName = Work", `GroupColor = ${RED}`, "GroupCollapsed = true"]) {
    if (!saved.includes(want)) {
      throw new Error(`issue-6560: saved session lacks '${want}'`);
    }
  }
}

export async function testit(): Promise<void> {
  await testCommands();
  await testSession();
}

if (import.meta.main) {
  await runStandalone(testit);
}
