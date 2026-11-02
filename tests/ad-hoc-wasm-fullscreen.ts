// The browser leaving fullscreen by itself (Esc, which the page never gets,
// or its "exit full screen" UI) takes the wasm app out of fullscreen and
// presentation mode too. Leaving from the app still leaves the browser's.
//
// The browser's own exit is done with document.exitFullscreen(): the page
// sees the same fullscreenchange as after the browser's Esc.
//
// bun tests/ad-hoc-wasm-fullscreen.ts [chrome|firefox]

import { spawnSync } from "node:child_process";
import { existsSync, mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { chromium, firefox, type BrowserType, type Page } from "playwright";
import { ROOT } from "./util.ts";

const OUT_DIR = join(ROOT, "out", "wasm", "rel");
const STEP_TIMEOUT_MS = 30_000;

async function waitFor(page: Page, fn: string, what: string): Promise<any> {
  const deadline = Date.now() + STEP_TIMEOUT_MS;
  for (;;) {
    const v = await page.evaluate(fn).catch(() => null);
    if (v) return v;
    if (Date.now() > deadline) {
      const state = await page.evaluate(`[${appMode}, ${browserFs}]`).catch(() => "?");
      throw new Error(`timed out waiting for ${what} (app mode, browser fullscreen: ${state})`);
    }
    await Bun.sleep(100);
  }
}

const appMode = `Module._sumatra_test_fullscreen()`;
const browserFs = `!!document.fullscreenElement`;

async function enter(page: Page, key: string, mode: number, what: string): Promise<void> {
  await page.keyboard.press(key);
  await waitFor(page, `${appMode} === ${mode} && ${browserFs}`, `${what} with ${key}`);
}

async function runBrowser(name: string, type: BrowserType, base: string): Promise<void> {
  const profile = mkdtempSync(join(tmpdir(), `wasm-fs-${name}-`));
  const ctx = await type.launchPersistentContext(profile, name === "chrome" ? { channel: "chrome" } : {});
  try {
    const page = ctx.pages()[0] ?? (await ctx.newPage());
    await page.goto(base + "?test-hooks&file=/docs/zlib.3.pdf");
    await waitFor(
      page,
      `!document.getElementById("gpui-loading") && !!window.Module?._sumatra_test_page?.()`,
      "the document",
    );
    await page.evaluate(`document.getElementById("gpui-canvas").focus()`);
    await Bun.sleep(1000);

    // A refused request (the browser fires fullscreenerror) leaves the app out
    // of fullscreen too. Firefox driven by Playwright refuses for real.
    await page.keyboard.press("F11");
    await waitFor(page, `${appMode} === 0 && !${browserFs} || ${appMode} === 1 && ${browserFs}`, "F11 to settle");
    if (!(await page.evaluate(browserFs))) {
      console.log(`${name}: ok: the browser refused fullscreen and the app left it too`);
      return;
    }
    await page.evaluate(`document.exitFullscreen()`);
    await waitFor(page, `!${browserFs} && ${appMode} === 0`, "the app to leave fullscreen");
    await page.evaluate(`{
      const el = document.documentElement;
      el.__requestFullscreen = el.requestFullscreen;
      el.requestFullscreen = () => {
        document.dispatchEvent(new Event("fullscreenerror"));
        return Promise.reject(new TypeError("refused"));
      };
    }`);
    await page.keyboard.press("F11");
    await Bun.sleep(500);
    await waitFor(page, `${appMode} === 0`, "the app to leave fullscreen the browser refused");
    await page.evaluate(`{ const el = document.documentElement; el.requestFullscreen = el.__requestFullscreen; }`);
    console.log(`${name}: ok: a refused fullscreen request leaves the app out of fullscreen`);

    await enter(page, "F11", 1, "fullscreen");
    await page.evaluate(`document.exitFullscreen()`);
    await waitFor(page, `${appMode} === 0`, "the app to leave fullscreen after the browser did");
    console.log(`${name}: ok: fullscreen ends when the browser leaves it`);

    await enter(page, "Shift+F11", 2, "presentation");
    await page.evaluate(`document.exitFullscreen()`);
    await waitFor(page, `${appMode} === 0`, "the app to leave presentation after the browser did");
    console.log(`${name}: ok: presentation ends when the browser leaves fullscreen`);

    await enter(page, "F11", 1, "fullscreen");
    await page.keyboard.press("Escape");
    await waitFor(page, `${appMode} === 0 && !${browserFs}`, "the app's Esc to leave both");
    await enter(page, "F11", 1, "fullscreen again");
    await page.keyboard.press("F11");
    await waitFor(page, `${appMode} === 0 && !${browserFs}`, "F11 to leave both");
    console.log(`${name}: ok: leaving from the app leaves the browser's fullscreen`);
  } finally {
    await ctx.close();
    rmSync(profile, { recursive: true, force: true });
  }
}

export async function testit(only = ""): Promise<void> {
  const build = spawnSync("bun", ["cmd/ng-build.ts", "-wasm", "-rel", "SumatraPDF"], { cwd: ROOT, stdio: "inherit" });
  if (build.status !== 0) throw new Error("ad-hoc-wasm-fullscreen: wasm build failed");
  const server = Bun.serve({
    port: 0,
    async fetch(req) {
      const path = new URL(req.url).pathname;
      const f = Bun.file(join(OUT_DIR, path === "/" ? "SumatraPDF.html" : path));
      return (await f.exists()) ? new Response(f) : new Response("not found", { status: 404 });
    },
  });
  const base = `http://localhost:${server.port}/`;
  try {
    for (const [name, type] of [
      ["chrome", chromium],
      ["firefox", firefox],
    ] as [string, BrowserType][]) {
      if (only && only !== name) continue;
      if (name !== "chrome" && !existsSync(type.executablePath())) {
        console.log(`ad-hoc-wasm-fullscreen: ${name}: not installed, skipping`);
        continue;
      }
      await runBrowser(name, type, base);
    }
  } finally {
    server.stop(true);
  }
}

if (import.meta.main) {
  await testit(process.argv[2] ?? "");
  console.log("✅ ad-hoc-wasm-fullscreen passed");
  process.exit(0);
}
