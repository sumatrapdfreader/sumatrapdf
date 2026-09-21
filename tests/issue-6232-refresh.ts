// Navigate Files in Folder: re-reading the folder (F5, activation) keeps the
// list on screen instead of flashing ".." and jumping (issue #6232, 3rd item).
import { copyFileSync, mkdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const SRC_PDF = join(import.meta.dir, "issue-3219.pdf");

type NavState = { scan: number; sel: number; items: number; name: string };
let lastNavRaw = "";

async function navState(client: ControlClient, action = "", idx = -1): Promise<NavState> {
  const res = await client.request(ControlCommand.TestNavFiles, [action, idx]);
  const raw = String(res[1] ?? "").trim();
  lastNavRaw = raw;
  const m = /^OK scan=(\d) sel=(-?\d+) items=(\d+) back=\d fwd=\d dir="[^"]*" name="([^"]*)"$/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`navigate files: could not parse state: ${raw}`);
  }
  return { scan: Number(m[1]), sel: Number(m[2]), items: Number(m[3]), name: m[4]! };
}

async function waitNav(client: ControlClient, what: string, pred: (state: NavState) => boolean): Promise<NavState> {
  const deadline = Date.now() + 8000;
  let last: NavState | null = null;
  while (Date.now() < deadline) {
    last = await navState(client);
    if (!last.scan && pred(last)) {
      return last;
    }
    await sleep(40);
  }
  throw new Error(`navigate files: ${what}: state did not settle: ${JSON.stringify(last)} raw=${lastNavRaw}`);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6232-refresh");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  for (const name of ["aaa.pdf", "bbb.pdf", "ccc.pdf", "ddd.pdf"]) {
    copyFileSync(SRC_PDF, join(dir, name));
  }

  const { proc, client } = await launchControlled([join(dir, "aaa.pdf")]);
  try {
    await client.request(ControlCommand.TestInvokeCommand, ["CmdNavigateFilesInFolder"]);
    await waitNav(client, "initial", (s) => s.items === 5 && s.name === "aaa.pdf");
    await navState(client, "select", 3);

    // the state right after a refresh, before the re-read lands: the old
    // listing and selection must still be there
    const during = await navState(client, "refresh");
    if (during.items !== 5 || during.name !== "ccc.pdf") {
      throw new Error(`refresh dropped the list while re-reading: ${JSON.stringify(during)}`);
    }
    await waitNav(client, "refresh unchanged", (s) => s.items === 5 && s.sel === 3 && s.name === "ccc.pdf");

    // a real change is picked up, keeping the selection on the same file
    copyFileSync(SRC_PDF, join(dir, "bba.pdf"));
    const during2 = await navState(client, "refresh");
    if (during2.items !== 5 || during2.name !== "ccc.pdf") {
      throw new Error(`refresh dropped the list while re-reading: ${JSON.stringify(during2)}`);
    }
    await waitNav(client, "refresh with new file", (s) => s.items === 6 && s.sel === 4 && s.name === "ccc.pdf");
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
