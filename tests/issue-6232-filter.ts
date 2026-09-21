// Navigate Files in Folder: the search field filters the list (issue #6232, 2nd item).
import { copyFileSync, mkdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const SRC_PDF = join(import.meta.dir, "issue-3219.pdf");

type NavState = { scan: number; sel: number; items: number; dir: string; name: string };
let lastNavRaw = "";

async function navState(client: ControlClient, action = "", idx = -1): Promise<NavState | null> {
  const res = await client.request(ControlCommand.TestNavFiles, [action, idx]);
  const raw = String(res[1] ?? "").trim();
  lastNavRaw = raw;
  if (res[0] !== 0) {
    return null;
  }
  const m = /^OK scan=(\d) sel=(-?\d+) items=(\d+) back=\d fwd=\d dir="([^"]*)" name="([^"]*)"$/.exec(raw);
  if (!m) {
    throw new Error(`navigate files: could not parse state: ${raw}`);
  }
  return { scan: Number(m[1]), sel: Number(m[2]), items: Number(m[3]), dir: m[4]!, name: m[5]! };
}

async function waitNav(client: ControlClient, what: string, pred: (state: NavState) => boolean): Promise<NavState> {
  const deadline = Date.now() + 8000;
  let last: NavState | null = null;
  while (Date.now() < deadline) {
    last = await navState(client);
    if (last && !last.scan && pred(last)) {
      return last;
    }
    await sleep(40);
  }
  throw new Error(`navigate files: ${what}: state did not settle: ${JSON.stringify(last)} raw=${lastNavRaw}`);
}

function sameDir(a: string, b: string): boolean {
  return a.replace(/[\\/]+$/, "").toLowerCase() === b.replace(/[\\/]+$/, "").toLowerCase();
}

export async function testit(): Promise<void> {
  const root = tmpPath("issue-6232-filter");
  rmSync(root, { recursive: true, force: true });
  mkdirSync(join(root, "alpha"), { recursive: true });
  for (const name of ["a.pdf", "ab.pdf", "b.pdf"]) {
    copyFileSync(SRC_PDF, join(root, name));
  }

  const { proc, client } = await launchControlled([join(root, "b.pdf")]);
  try {
    await client.request(ControlCommand.TestInvokeCommand, ["CmdNavigateFilesInFolder"]);
    // "..", alpha\, a.pdf, ab.pdf, b.pdf
    await waitNav(client, "initial", (s) => sameDir(s.dir, root) && s.items === 5 && s.name === "b.pdf");

    // filter hides ".." and keeps dirs and files that match; first match selected
    await navState(client, "filter:a");
    await waitNav(client, "filter a", (s) => s.items === 3 && s.sel === 0 && s.name === "alpha\\");

    // case-insensitive, like the command palette
    await navState(client, "filter:B");
    await waitNav(client, "filter B", (s) => s.items === 2 && s.name === "ab.pdf");

    // several words must all match
    await navState(client, "filter:a b");
    await waitNav(client, "filter a b", (s) => s.items === 1 && s.name === "ab.pdf");

    await navState(client, "filter:zzz");
    await waitNav(client, "filter zzz", (s) => s.items === 0 && s.sel === -1);

    await navState(client, "filter:");
    await waitNav(client, "filter cleared", (s) => s.items === 5);

    // changing folder drops the filter
    await navState(client, "filter:a");
    await waitNav(client, "filter a again", (s) => s.items === 3);
    await navState(client, "up");
    await waitNav(client, "up clears filter", (s) => !sameDir(s.dir, root) && s.name === "issue-6232-filter\\");
    await navState(client, "back");
    await waitNav(client, "back", (s) => sameDir(s.dir, root) && s.items === 5);
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(root, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
