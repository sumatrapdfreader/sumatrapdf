// Navigate Files in Folder: Back / Forward / Up / Home buttons (issue #6232).
import { copyFileSync, mkdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const SRC_PDF = join(import.meta.dir, "issue-3219.pdf");

type NavState = { scan: number; sel: number; items: number; back: number; fwd: number; dir: string; name: string };
let lastNavRaw = "";

async function navState(client: ControlClient, action = "", idx = -1): Promise<NavState | null> {
  const res = await client.request(ControlCommand.TestNavFiles, [action, idx]);
  const raw = String(res[1] ?? "").trim();
  lastNavRaw = raw;
  if (res[0] !== 0 || raw === "OK closed") {
    return null;
  }
  const m = /^OK scan=(\d) sel=(-?\d+) items=(\d+) back=(\d) fwd=(\d) dir="([^"]*)" name="([^"]*)"$/.exec(raw);
  if (!m) {
    throw new Error(`navigate files: could not parse state: ${raw}`);
  }
  return {
    scan: Number(m[1]),
    sel: Number(m[2]),
    items: Number(m[3]),
    back: Number(m[4]),
    fwd: Number(m[5]),
    dir: m[6]!,
    name: m[7]!,
  };
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
  const root = tmpPath("issue-6232");
  const sub = join(root, "sub");
  rmSync(root, { recursive: true, force: true });
  mkdirSync(sub, { recursive: true });
  copyFileSync(SRC_PDF, join(sub, "a.pdf"));
  copyFileSync(SRC_PDF, join(root, "b.pdf"));

  const { proc, client } = await launchControlled([join(sub, "a.pdf")]);
  try {
    await client.request(ControlCommand.TestInvokeCommand, ["CmdNavigateFilesInFolder"]);
    // "..", a.pdf
    await waitNav(client, "initial", (s) => sameDir(s.dir, sub) && s.items === 2 && s.name === "a.pdf");
    await waitNav(client, "initial buttons", (s) => s.back === 0 && s.fwd === 0);

    // Up: parent listing with the directory we came from selected
    await navState(client, "up");
    await waitNav(client, "up", (s) => sameDir(s.dir, root) && s.items === 3 && s.name === "sub\\" && s.back === 1);

    await navState(client, "back");
    await waitNav(client, "back", (s) => sameDir(s.dir, sub) && s.back === 0 && s.fwd === 1);

    await navState(client, "forward");
    await waitNav(client, "forward", (s) => sameDir(s.dir, root) && s.back === 1 && s.fwd === 0);

    // Home: no directory, drives listed first (a drive root like "C:\")
    await navState(client, "home");
    await waitNav(client, "home", (s) => s.dir === "" && s.items >= 1 && /^[A-Z]:\\$/.test(s.name) && s.back === 1);

    // a new navigation after Back drops the forward history
    await navState(client, "back");
    await waitNav(client, "back from home", (s) => sameDir(s.dir, root) && s.fwd === 1);
    await navState(client, "up");
    await waitNav(client, "up drops forward", (s) => !sameDir(s.dir, root) && s.fwd === 0 && s.back === 1);

    // a drive root still lists "..": Enter on it, like Up, lands on the home view
    for (let i = 0; i < 32; i++) {
      const s = await waitNav(client, "up to root", () => true);
      if (/^[A-Z]:\\$/.test(s.dir)) {
        break;
      }
      await navState(client, "up");
    }
    await navState(client, "select", 0);
    await waitNav(client, "root lists ..", (s) => s.sel === 0 && s.name === "..");
    await navState(client, "execute");
    await waitNav(client, "up from root", (s) => s.dir === "" && s.back === 1);

    // history outlives the window: a re-opened dialog can go Back to where
    // the previous one was (the home view)
    await navState(client, "close");
    await sleep(200);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdNavigateFilesInFolder"]);
    await waitNav(client, "reopened", (s) => sameDir(s.dir, sub) && s.name === "a.pdf" && s.back === 1);
    await navState(client, "back");
    await waitNav(client, "back across launches", (s) => s.dir === "" && s.fwd === 1);
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(root, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
