// ng on macOS: menu bar items and shortcuts follow macOS conventions, tested
// with real key presses, which AppKit gives the menu bar first.
//
// Ad-hoc: needs Accessibility for the terminal (see mac-system-events.ts) and
// takes the keyboard focus while it runs.
// Run: bun tests/ad-hoc-mac-menu-keys.ts [--no-build]

import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import {
  kKeyEscape,
  kKeyTab,
  macActivate,
  macCanSendKeys,
  macFrontWindowTitle,
  macKeyCode,
  macKeystroke,
  macMenuItems,
  macPressHolding,
  type MacMenuItem,
} from "./mac-system-events.ts";
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

// AXMenuItemCmdModifiers of a ⌘ and a ⇧⌘ shortcut
const kModsCmd = 0;
const kModsShiftCmd = 1;

function findItem(items: MacMenuItem[], menu: string, path: string): MacMenuItem | undefined {
  return items.find((it) => it.menu === menu && it.path === path);
}

// The application menu has Services and Hide, Edit has the edit rows, Window
// Minimize; Windows idioms (File > Exit, About in Help, Show Menu) and
// AppKit's tab and full screen rows are gone.
function menuLayout(app: App): void {
  const items = macMenuItems(app.pid);
  const appMenu = items.find((it) => it.path === "Settings...")?.menu ?? "";
  const shortcut = (menu: string, path: string, key: string) => {
    const it = findItem(items, menu, path);
    expect(`${menu} > ${path} is ⌘${key}`, it?.key === key && it?.mods === kModsCmd, JSON.stringify(it));
  };
  const absent = (menu: string, path: string) => {
    expect(`no ${menu} > ${path}`, !findItem(items, menu, path), menu);
  };
  shortcut(appMenu, `Hide ${appMenu}`, "H");
  expect("the application menu has Services", !!findItem(items, appMenu, "Services"), appMenu);
  shortcut(appMenu, "Quit SumatraPDF", "Q");
  expect("the application menu has About SumatraPDF", !!findItem(items, appMenu, "About SumatraPDF"), appMenu);
  shortcut("Edit", "Undo", "Z");
  shortcut("Edit", "Copy", "C");
  shortcut("Edit", "Paste", "V");
  shortcut("Edit", "Select All", "A");
  shortcut("Window", "Minimize", "M");
  shortcut("Go To", "Find Next", "G");
  const closeWindow = findItem(items, "File", "Close Window");
  expect(
    "File > Close Window is ⇧⌘W",
    closeWindow?.key === "W" && closeWindow?.mods === kModsShiftCmd,
    JSON.stringify(closeWindow),
  );
  shortcut("Go To", "Find Next Selection", "E");
  absent("File", "Exit");
  absent("Help", "About");
  absent("View", "Show Menu");
  absent("View", "Show Tab Bar");
  absent("View", "Enter Full Screen");
  absent("Window", "Show Next Tab");
}

// ⌃Tab switches the tab when Control is let go. AppKit took ⌃Tab for its own
// tabs and focus moves, and never reported Control going up.
async function tabKeys(app: App): Promise<void> {
  const before = macFrontWindowTitle(app.pid);
  await macPressHolding(kKeyTab, { ctrl: true });
  // the switcher closes after the switch
  await sleep(2 * kSettleMs);
  const other = macFrontWindowTitle(app.pid);
  expect("⌃Tab switches the tab", other !== before, other);
  await macPressHolding(kKeyTab, { ctrl: true });
  // the switcher closes after the switch
  await sleep(2 * kSettleMs);
  const again = macFrontWindowTitle(app.pid);
  expect("⌃Tab switches again", again !== other, again);
}

// ⇧⌘= is how ⌘+ is typed: zoom in, not rotate (the Windows Ctrl+Shift+=)
async function zoomKeys(app: App): Promise<void> {
  const zoom = async () => Number(await field(app, / zoom=([\d.]+)/));
  const before = await zoom();
  macKeystroke(app.pid, "=", { cmd: true, shift: true });
  await sleep(kSettleMs);
  const bigger = await zoom();
  expect("⇧⌘= zooms in", bigger > before, `${before} -> ${bigger}`);
  macKeystroke(app.pid, "-", { cmd: true, shift: true });
  await sleep(kSettleMs);
  const back = await zoom();
  expect("⇧⌘- zooms out", back < bigger, `${bigger} -> ${back}`);
}

async function minimizeKey(app: App): Promise<void> {
  macKeystroke(app.pid, "m", { cmd: true });
  await sleep(2 * kSettleMs);
  const minimized = await field(app, / minimized=(\d)/);
  expect("⌘M minimizes the window", minimized === "1", minimized);
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
    menuLayout(app);
    // reading the menus moves the focus: each check takes it back first
    const checks = [zoomKeys, selectAllInTextField, tabKeys, minimizeKey]; // minimize last: the window is gone after it
    for (const check of checks) {
      macActivate(app.pid);
      await sleep(kSettleMs);
      await check(app);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
