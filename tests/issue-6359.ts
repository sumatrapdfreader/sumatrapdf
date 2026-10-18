// Navigate Files in Folder (the SumatraPDF file picker): a touch swipe on the
// list scrolls it, not only dragging its scrollbar (issue #6359).
//
// Run: bun tests/issue-6359.ts [--no-build]

import { copyFileSync, mkdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, touchSwipe } from "./win-automation.ts";
import { setProcessDpiAware, sleep } from "./winapi.ts";

const SRC_PDF = join(import.meta.dir, "issue-3219.pdf");
// enough files that the list scrolls
const N_FILES = 80;

async function nav(client: ControlClient, action = ""): Promise<string> {
  const res = await client.request(ControlCommand.TestNavFiles, [action, -1]);
  return String(res[1] ?? "").trim();
}

function scrollY(state: string): number {
  const m = / scrollY=(-?\d+)/.exec(state);
  if (!m) {
    throw new Error(`issue-6359: no scrollY in: ${state}`);
  }
  return Number(m[1]);
}

async function waitListed(client: ControlClient): Promise<void> {
  const deadline = Date.now() + 8000;
  let state = "";
  while (Date.now() < deadline) {
    state = await nav(client);
    if (state.includes("scan=0") && state.includes(`items=${N_FILES + 1}`)) {
      return;
    }
    await sleep(40);
  }
  throw new Error(`issue-6359: list did not fill: ${state}`);
}

export async function testit(): Promise<void> {
  setProcessDpiAware();
  const dir = tmpPath("issue-6359");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  for (let i = 0; i < N_FILES; i++) {
    copyFileSync(SRC_PDF, join(dir, `f${String(i).padStart(3, "0")}.pdf`));
  }

  const { proc, client } = await launchControlled([join(dir, "f000.pdf")]);
  try {
    await client.request(ControlCommand.TestInvokeCommand, ["CmdNavigateFilesInFolder"]);
    await waitListed(client);
    const before = scrollY(await nav(client));

    const rect = await nav(client, "list-screen-rect");
    const m = /^OK (-?\d+) (-?\d+) (\d+) (\d+)/.exec(rect);
    if (!m) {
      throw new Error(`issue-6359: no list rect: ${rect}`);
    }
    const [x, y, dx, dy] = m.slice(1, 5).map(Number) as [number, number, number, number];
    // finger up the middle of the list (away from the scrollbar on the right)
    const cx = x + Math.round(dx / 3);
    if (!(await touchSwipe(cx, y + Math.round(dy * 0.8), y + Math.round(dy * 0.2)))) {
      console.log("issue-6359: touch injection not available here, skipping");
      return;
    }
    await sleep(500);
    const after = scrollY(await nav(client));
    console.log(`issue-6359: scrollY ${before} -> ${after}`);
    if (after <= before) {
      throw new Error(`issue-6359: swiping up did not scroll the list (scrollY ${before} -> ${after})`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
