// Enter on a setting in the wasm build, in a real browser. The dialogs are inside
// the frame there, so Enter takes other paths than in native ng: it closed Advanced
// Settings through the default button, and ran the command palette's row twice,
// which toggled a bool back. Ad-hoc: it needs a wasm build (emsdk) and a browser.
//
// Nothing here can read the app's state, so screen areas are compared: a setting's
// value changes and the title of what should stay open doesn't.
//
// Run: bun tests/ad-hoc-wasm-enter.ts

import { spawnSync } from "node:child_process";
import { existsSync, mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { Cdp, clickAt, runFromPalette } from "./ad-hoc-wasm-host.ts";
import { ROOT, runStandalone } from "./util.ts";

const OUT_DIR = join(ROOT, "out", "wasm", "rel");
const BROWSERS = [
  "C:/Program Files/Google/Chrome/Application/chrome.exe",
  "C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe",
];
const START_TIMEOUT_MS = 30_000;
const LOAD_MS = 9000;
const SETTLE_MS = 1000;
const VK_DOWN = 40;
const VK_ENTER = 13;
const VK_ESCAPE = 27;
const VK_EQUAL = 187;

type Clip = { x: number; y: number; width: number; height: number; scale: number };
const clip = (x: number, y: number, width: number, height: number): Clip => ({ x, y, width, height, scale: 1 });

// in the default headless viewport
const NEUTRAL_POINT = { x: 100, y: 400 };
const PALETTE_QUERY = clip(75, 50, 400, 28);
const PALETTE_BOOL_VALUE = clip(620, 120, 70, 16); // AIChat.Bubbles
const PALETTE_STRING_ROW = 1; // AIChat.FontName: asks for a value
const ADV_TITLE = clip(30, 35, 150, 18);
const ADV_BOOL_VALUE = clip(660, 150, 75, 16); // DisableJavaScript
const ADV_BOOL_ROW = 2;

async function area(cdp: Cdp, c: Clip): Promise<string> {
  await Bun.sleep(SETTLE_MS);
  return (await cdp.send("Page.captureScreenshot", { clip: c })).data;
}

async function key(cdp: Cdp, name: string, vk: number): Promise<void> {
  const base = { key: name, code: name, windowsVirtualKeyCode: vk, nativeVirtualKeyCode: vk };
  await cdp.send("Input.dispatchKeyEvent", { type: "rawKeyDown", ...base });
  await cdp.send("Input.dispatchKeyEvent", { type: "keyUp", ...base });
  await Bun.sleep(300);
}

async function testPalette(cdp: Cdp): Promise<void> {
  await clickAt(cdp, NEUTRAL_POINT.x, NEUTRAL_POINT.y);
  await Bun.sleep(300);
  const eq = { key: "=", text: "=", code: "Equal", windowsVirtualKeyCode: VK_EQUAL };
  await cdp.send("Input.dispatchKeyEvent", { type: "keyDown", ...eq });
  await cdp.send("Input.dispatchKeyEvent", { type: "keyUp", ...eq });

  const value = await area(cdp, PALETTE_BOOL_VALUE);
  const query = await area(cdp, PALETTE_QUERY);
  await key(cdp, "Enter", VK_ENTER);
  const toggled = await area(cdp, PALETTE_BOOL_VALUE);
  if (toggled === value) {
    throw new Error("ad-hoc-wasm-enter: Enter in the palette didn't toggle the bool");
  }
  await key(cdp, "Enter", VK_ENTER);
  if ((await area(cdp, PALETTE_BOOL_VALUE)) !== value) {
    throw new Error("ad-hoc-wasm-enter: a second Enter in the palette didn't toggle the bool back");
  }

  // the query turns into "=<name> = <value>"; it used to be answered at once
  for (let i = 0; i < PALETTE_STRING_ROW; i++) {
    await key(cdp, "ArrowDown", VK_DOWN);
  }
  await key(cdp, "Enter", VK_ENTER);
  if ((await area(cdp, PALETTE_QUERY)) === query) {
    throw new Error("ad-hoc-wasm-enter: Enter on a string setting didn't ask for a value");
  }
  await key(cdp, "Escape", VK_ESCAPE);
  await key(cdp, "Escape", VK_ESCAPE);
}

async function testAdvancedSettings(cdp: Cdp): Promise<void> {
  await runFromPalette(cdp, "advanced settings");
  const title = await area(cdp, ADV_TITLE);
  for (let i = 0; i < ADV_BOOL_ROW; i++) {
    await key(cdp, "ArrowDown", VK_DOWN);
  }
  const value = await area(cdp, ADV_BOOL_VALUE);
  await key(cdp, "Enter", VK_ENTER);
  if ((await area(cdp, ADV_TITLE)) !== title) {
    throw new Error("ad-hoc-wasm-enter: Enter closed Advanced Settings");
  }
  if ((await area(cdp, ADV_BOOL_VALUE)) === value) {
    throw new Error("ad-hoc-wasm-enter: Enter didn't toggle the bool in Advanced Settings");
  }
}

export async function testit(): Promise<void> {
  const browser = BROWSERS.find((b) => existsSync(b));
  if (!browser) {
    console.log("ad-hoc-wasm-enter: no Chrome or Edge, skipping");
    return;
  }
  const build = spawnSync("bun", ["cmd/ng-build.ts", "-wasm", "-rel", "SumatraPDF"], { cwd: ROOT, stdio: "inherit" });
  if (build.status !== 0) {
    throw new Error("ad-hoc-wasm-enter: wasm build failed");
  }

  // OPFS needs a secure context, which localhost is
  const server = Bun.serve({
    port: 0,
    async fetch(req) {
      const path = new URL(req.url).pathname;
      const f = Bun.file(join(OUT_DIR, path === "/" ? "SumatraPDF.html" : path));
      return (await f.exists()) ? new Response(f) : new Response("not found", { status: 404 });
    },
  });
  const profile = mkdtempSync(join(tmpdir(), "wasm-enter-"));
  const proc = Bun.spawn(
    [
      browser,
      "--headless=new",
      "--remote-debugging-port=0",
      `--user-data-dir=${profile}`,
      "--no-first-run",
      "about:blank",
    ],
    { stdout: "ignore", stderr: "ignore" },
  );
  let cdp: Cdp | null = null;
  try {
    const portFile = join(profile, "DevToolsActivePort");
    const deadline = Date.now() + START_TIMEOUT_MS;
    while (!existsSync(portFile) || readFileSync(portFile, "utf8").split("\n").length < 2) {
      if (Date.now() > deadline) {
        throw new Error("ad-hoc-wasm-enter: the browser didn't start");
      }
      await Bun.sleep(100);
    }
    const [port, wsPath] = readFileSync(portFile, "utf8").split("\n");
    cdp = await Cdp.connect(`ws://127.0.0.1:${port}${wsPath}`);
    const { targetId } = await cdp.send("Target.createTarget", { url: "about:blank" });
    const { sessionId } = await cdp.send("Target.attachToTarget", { targetId, flatten: true });
    cdp.attach(sessionId);
    await cdp.send("Page.enable");
    await cdp.send("Runtime.enable");
    // headless: without this the page never counts as focused, and keys are dropped
    await cdp.send("Emulation.setFocusEmulationEnabled", { enabled: true });
    await cdp.send("Page.navigate", { url: `http://localhost:${server.port}/` });
    await Bun.sleep(LOAD_MS);

    await testPalette(cdp);
    await testAdvancedSettings(cdp);
    console.log("ad-hoc-wasm-enter: OK");
  } finally {
    cdp?.close();
    proc.kill();
    await proc.exited;
    server.stop(true);
    rmSync(profile, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
