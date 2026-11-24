// Issue #6587: with the toolbar in overlay mode, the Edit PDF row was clipped
// on both sides (the floating toolbar was only as wide as the main row), and
// toggling the caption left the toolbar much taller than its rows.
//
// Run: bun tests/issue-6587.ts [--no-build]

import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { ROOT, runStandalone, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");

type Host = { dx: number; dy: number; overlay: boolean; contentDy: number };
type Btn = { x1: number; x2: number };

async function dump(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestToolbarButtons, []);
  return String(res[1] ?? "");
}

function parseHost(raw: string): Host | null {
  const m = /^host dx=(\d+) dy=(\d+) overlay=(\d) contentDy=(\d+)/m.exec(raw);
  if (!m) {
    return null;
  }
  return { dx: +m[1]!, dy: +m[2]!, overlay: m[3] === "1", contentDy: +m[4]! };
}

// visible Edit PDF row buttons, x extents in toolbar window coordinates
function annotButtons(raw: string): Btn[] {
  const out: Btn[] = [];
  for (const line of raw.split("\n")) {
    const m = /^annotation-idx=\d+ cmd=\d+ hidden=0 enabled=\d rect=(-?\d+),-?\d+,(-?\d+),-?\d+/.exec(line);
    if (m) {
      out.push({ x1: +m[1]!, x2: +m[2]! });
    }
  }
  return out;
}

async function invoke(client: ControlClient, cmd: string): Promise<void> {
  await client.request(ControlCommand.TestInvokeCommand, [cmd]);
}

// polls until check() returns "" (ok) or the deadline passes
async function waitFor(client: ControlClient, what: string, check: (raw: string) => string): Promise<string> {
  const deadline = Date.now() + 5000;
  let raw = "";
  let why = "";
  for (;;) {
    raw = await dump(client);
    why = check(raw);
    if (!why) {
      return raw;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6587: ${what}: ${why}\n${raw}`);
    }
    await sleep(100);
  }
}

function checkFits(raw: string): string {
  const h = parseHost(raw);
  if (!h) {
    return "no host line";
  }
  const btns = annotButtons(raw);
  if (btns.length === 0) {
    return "Edit PDF row not shown";
  }
  const bad = btns.filter((b) => b.x1 < 0 || b.x2 > h.dx);
  return bad.length ? `${bad.length} Edit PDF buttons outside 0..${h.dx}` : "";
}

function checkHeight(raw: string): string {
  const h = parseHost(raw);
  if (!h) {
    return "no host line";
  }
  return h.dy === h.contentDy ? "" : `toolbar dy=${h.dy}, its rows need ${h.contentDy}`;
}

export async function testit(): Promise<void> {
  if (USE_NG) {
    // ng's overlay toolbar is sized by its content
    console.log("SKIP issue-6587: orig only");
    return;
  }
  const { proc, client } = await launchControlled([PDF]);
  try {
    // Toggle Toolbar cycles show -> overlay -> hide
    for (let i = 0; i < 3 && !parseHost(await dump(client))?.overlay; i++) {
      await invoke(client, "CmdToggleToolbar");
      await sleep(200);
    }
    await waitFor(client, "overlay mode", (raw) => (parseHost(raw)?.overlay ? "" : "not overlay"));
    await invoke(client, "CmdToggleEditPDF");
    await waitFor(client, "Edit PDF row fits", checkFits);
    await waitFor(client, "height with Edit PDF row", checkHeight);

    // hiding the caption must not grow the toolbar
    await invoke(client, "CmdToggleCaption");
    await sleep(300);
    await waitFor(client, "height after hiding the caption", checkHeight);
    await waitFor(client, "Edit PDF row fits after hiding the caption", checkFits);
    await invoke(client, "CmdToggleCaption");
    await sleep(300);
    await waitFor(client, "height after showing the caption", checkHeight);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
