import { copyFileSync, mkdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const SRC_PDF = join(import.meta.dir, "issue-3219.pdf");

type NavState = { scan: number; sel: number; items: number; name: string };
let lastNavRaw = "";

async function navState(client: ControlClient, action = "", idx = -1): Promise<NavState | null> {
  const res = await client.request(ControlCommand.TestNavFiles, [action, idx]);
  const raw = String(res[1] ?? "").trim();
  lastNavRaw = raw;
  if (res[0] !== 0) {
    return null;
  }
  const m = /^OK scan=(\d) sel=(-?\d+) items=(\d+) name=(.*)$/.exec(raw);
  if (!m) {
    throw new Error(`navigate files: could not parse state: ${raw}`);
  }
  return { scan: Number(m[1]), sel: Number(m[2]), items: Number(m[3]), name: m[4]! };
}

async function waitNav(client: ControlClient, pred: (state: NavState) => boolean): Promise<NavState> {
  const deadline = Date.now() + 8000;
  let last: NavState | null = null;
  while (Date.now() < deadline) {
    last = await navState(client);
    if (last && pred(last)) {
      return last;
    }
    await sleep(40);
  }
  throw new Error(`navigate files: state did not settle: ${JSON.stringify(last)} raw=${lastNavRaw}`);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("navigate-files-delete-selection");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  for (const name of ["aaa.pdf", "bbb.pdf", "ccc.pdf", "ddd.pdf"]) {
    copyFileSync(SRC_PDF, join(dir, name));
  }

  const { proc, client } = await launchControlled([join(dir, "aaa.pdf")]);
  try {
    await client.request(ControlCommand.TestInvokeCommand, ["CmdNavigateFilesInFolder"]);
    await waitNav(client, (s) => !s.scan && s.items === 5 && s.name === "aaa.pdf");

    await navState(client, "select", 2);
    await navState(client, "delete-refresh");
    await waitNav(client, (s) => !s.scan && s.items === 4 && s.sel === 2 && s.name === "ccc.pdf");

    await navState(client, "select", 3);
    await navState(client, "delete-refresh");
    await waitNav(client, (s) => !s.scan && s.items === 3 && s.sel === 2 && s.name === "ccc.pdf");
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
