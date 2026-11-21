// The startup window comes on screen with page 1 drawn in it, not a blank
// page that fills in a moment later (MaybeRevealStartupWindow in
// src/ng/gui/AppShell.cpp): it is shown only after a page tile was painted.
// Without a page to wait for (the home page, a file that doesn't load) it is
// shown at its first frame; with a password prompt, for the prompt.
//
// macOS only: ng there paints a window that isn't on screen yet.
// Run: bun tests/startup-window-shown.ts [--no-build]

import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { EXE, ROOT, runStandalone, tmpPath, USE_NG } from "./util.ts";

const name = "startup-window-shown";

type Shown = { firstPaintMs: number; shownMs: number; why: string };

async function startup(file: string | null): Promise<Shown> {
  const pipe = `startup-shown-${process.pid}-${Math.floor(Math.random() * 1e6)}`;
  const args = [EXE, "-for-testing", "-dbg-control", pipe];
  if (file) {
    args.push(file);
  }
  const proc = Bun.spawn(args, { stdout: "ignore", stderr: "ignore" });
  try {
    const client = await ControlClient.connect(pipe, 10000);
    let raw = "";
    const deadline = Date.now() + 10000;
    while (Date.now() < deadline) {
      raw = String((await client.request(ControlCommand.TestPerfStats, []))[1] ?? "");
      const shown = Number(/windowShownMs=(-?\d+)/.exec(raw)?.[1] ?? -1);
      if (shown >= 0) {
        return {
          firstPaintMs: Number(/firstPaintMs=(-?\d+)/.exec(raw)?.[1] ?? -1),
          shownMs: shown,
          why: /windowShownWhy=(\S*)/.exec(raw)?.[1] ?? "",
        };
      }
      await Bun.sleep(20);
    }
    throw new Error(`${name}: the window never came on screen: ${raw}`);
  } finally {
    proc.kill(9);
    await proc.exited;
  }
}

function expect(what: string, ok: boolean, s: Shown): void {
  if (!ok) {
    throw new Error(`${name}: ${what}: ${JSON.stringify(s)}`);
  }
}

export async function testit(): Promise<void> {
  if (!USE_NG || !IS_MAC) {
    console.log(`SKIP ${name}: ng on macOS only`);
    return;
  }
  let s = await startup(join(ROOT, "ext", "a-zlib", "zlib.3.pdf"));
  expect("a PDF: shown for its page", s.why === "page", s);
  expect("a PDF: shown after the page was painted", s.firstPaintMs >= 0 && s.shownMs >= s.firstPaintMs, s);

  s = await startup(join(ROOT, "tests", "issue-1201.cbz"));
  expect("a comic: shown for its page", s.why === "page", s);

  s = await startup(null);
  expect("the home page: shown at its first frame", s.why === "no-page", s);

  s = await startup(tmpPath("startup-window-shown-missing.pdf"));
  expect("a file that doesn't load: shown at its first frame", s.why === "no-page", s);

  const locked = tmpPath("startup-window-shown-locked.pdf");
  Bun.spawnSync([
    EXE,
    "cli",
    "encrypt",
    join(ROOT, "ext", "a-zlib", "zlib.3.pdf"),
    "--new-password",
    "pw",
    "-o",
    locked,
  ]);
  s = await startup(locked);
  expect("an encrypted PDF: shown for its password prompt", s.why === "dialog", s);
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
