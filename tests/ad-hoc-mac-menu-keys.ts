// ng on macOS: menu bar items and shortcuts follow macOS conventions, tested
// with real key presses, which AppKit gives the menu bar first.
//
// Ad-hoc: needs Accessibility for the terminal (see mac-system-events.ts) and
// takes the keyboard focus while it runs.
// Run: bun tests/ad-hoc-mac-menu-keys.ts [--no-build]

import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { kKeyEscape, macActivate, macCanSendKeys, macKeyCode, macKeystroke } from "./mac-system-events.ts";
import { ROOT, runStandalone, USE_NG } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const name = "ad-hoc-mac-menu-keys";
const kSettleMs = 600;

type App = { pid: number; client: ControlClient };

async function uiState(app: App): Promise<string> {
  return String((await app.client.request(ControlCommand.TestUiState, []))[1] ?? "");
}

async function field(app: App, re: RegExp): Promise<string> {
  return re.exec(await uiState(app))?.[1] ?? "";
}

function expect(what: string, ok: boolean, got: string): void {
  if (!ok) {
    throw new Error(`${name}: ${what}: ${got}`);
  }
}

// ⌘A is Select All in the menu bar, which takes it before the palette's
// text field would; the field must still select its text.
async function selectAllInTextField(app: App): Promise<void> {
  macKeystroke(app.pid, "k", { cmd: true });
  await sleep(kSettleMs);
  macKeystroke(app.pid, "abc");
  macKeystroke(app.pid, "a", { cmd: true });
  macKeystroke(app.pid, "x");
  await sleep(kSettleMs);
  const text = await field(app, / editText='([^']*)'/);
  expect("⌘A in the command palette selects its text", text === "x", text);
  macKeyCode(app.pid, kKeyEscape);
  await sleep(kSettleMs);
}

export async function testit(): Promise<void> {
  if (!USE_NG || !IS_MAC) {
    console.log(`SKIP ${name}: ng on macOS only`);
    return;
  }
  if (!macCanSendKeys()) {
    console.log(`SKIP ${name}: allow the terminal in System Settings > Privacy & Security > Accessibility`);
    return;
  }
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const cbz = join(ROOT, "tests", "issue-1201.cbz");
  const { proc, client } = await launchControlled([cbz, pdf]);
  const app: App = { pid: proc.pid, client };
  try {
    await client.waitForRenderIdle();
    macActivate(app.pid);
    await sleep(kSettleMs);
    await selectAllInTextField(app);
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
