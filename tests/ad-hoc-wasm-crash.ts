// A crash in the wasm build reaches sumatraHost.onCrash (docs/wasm-host-api.md)
// with the stack, the build and the log, and the stack's wasm-function[N]
// frames resolve to names with the --emit-symbol-map file. This runs
// CmdDebugCrashMe in headless Chrome or Edge. Ad-hoc: it needs a wasm build
// (emsdk) and a browser.

import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { Cdp, waitFor } from "./ad-hoc-wasm-host.ts";
import { ROOT, tmpPath } from "./util.ts";

const OUT_DIR = join(ROOT, "out", "wasm", "rel");
const SYMBOLS = join(OUT_DIR, "SumatraPDF.html.symbols");
const BROWSERS = [
  "C:/Program Files/Google/Chrome/Application/chrome.exe",
  "C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe",
];
const STEP_TIMEOUT_MS = 60_000;
const WATCHDOG_MS = 5 * 60_000;
// ExecuteCmd's switch runs CrashMe(), which is inlined into it
const CRASH_FRAME = "ExecuteCmd";

const HOST_SCRIPT = `window.sumatraHost = {
  apiVersion: 1,
  onCrash: function (report) {
    window.__crashes = (window.__crashes || []).concat([report]);
    return true;
  },
};`;

// the wasm-function[N] frames of a stack, with N resolved by the symbol map
// (lines of "N:name")
function symbolicate(stack: string, map: string): string[] {
  const names = new Map<number, string>();
  for (const line of map.split("\n")) {
    const i = line.indexOf(":");
    if (i > 0) names.set(Number(line.slice(0, i)), line.slice(i + 1).trim());
  }
  const frames: string[] = [];
  for (const m of stack.matchAll(/wasm-function\[(\d+)\]/g)) {
    frames.push(names.get(Number(m[1])) ?? `wasm-function[${m[1]}]`);
  }
  return frames;
}

async function commandId(cdp: Cdp, name: string): Promise<number> {
  await cdp.eval(`Module._sumatra_test_visibility()`);
  const text: string = await cdp.eval(`window.__testVisibility`);
  for (const line of text.split("\n")) {
    const [id, n] = line.split("\t");
    if (n === name) return Number(id);
  }
  throw new Error(`ad-hoc-wasm-crash: no command ${name}`);
}

export async function testit(): Promise<void> {
  const browser = BROWSERS.find((b) => existsSync(b));
  if (!browser) {
    console.log("ad-hoc-wasm-crash: no Chrome or Edge, skipping");
    return;
  }
  const build = spawnSync("bun", ["cmd/ng-build.ts", "-wasm", "-rel", "SumatraPDF"], { cwd: ROOT, stdio: "inherit" });
  if (build.status !== 0) throw new Error("ad-hoc-wasm-crash: wasm build failed");
  if (!existsSync(SYMBOLS)) throw new Error(`ad-hoc-wasm-crash: the build made no ${SYMBOLS}`);

  const server = Bun.serve({
    port: 0,
    async fetch(req) {
      const path = new URL(req.url).pathname;
      const f = Bun.file(join(OUT_DIR, path === "/" ? "SumatraPDF.html" : path));
      return (await f.exists()) ? new Response(f) : new Response("not found", { status: 404 });
    },
  });
  const profile = mkdtempSync(join(tmpdir(), "wasm-crash-"));
  const proc = Bun.spawn(
    [browser, "--headless=new", "--remote-debugging-port=0", `--user-data-dir=${profile}`, "--no-first-run"],
    { stdout: "ignore", stderr: "pipe" },
  );
  let cdp: Cdp | null = null;
  try {
    const portFile = join(profile, "DevToolsActivePort");
    const deadline = Date.now() + STEP_TIMEOUT_MS;
    let lines: string[] = [];
    while (lines.length < 2) {
      if (Date.now() > deadline) throw new Error("ad-hoc-wasm-crash: the browser didn't start");
      try {
        lines = readFileSync(portFile, "utf8").split("\n");
      } catch {
        // not written yet, or still open (EBUSY on Windows)
      }
      await Bun.sleep(100);
    }
    cdp = await Cdp.connect(`ws://127.0.0.1:${lines[0]}${lines[1]}`);
    const { targetId } = await cdp.send("Target.createTarget", { url: "about:blank" });
    const { sessionId } = await cdp.send("Target.attachToTarget", { targetId, flatten: true });
    cdp.attach(sessionId);
    await cdp.send("Page.enable");
    await cdp.send("Runtime.enable");
    await cdp.send("Page.addScriptToEvaluateOnNewDocument", { source: HOST_SCRIPT });

    await cdp.send("Page.navigate", { url: `http://localhost:${server.port}/?test-hooks` });
    await waitFor(
      cdp,
      `window.__sumatraStorage === "ready" && !document.getElementById("gpui-loading") && !!Module._sumatra_test_run`,
      "the app",
    );

    const id = await commandId(cdp, "CmdDebugCrashMe");
    await cdp.eval(`Module._sumatra_test_run(${id})`);
    await waitFor(cdp, `(window.__crashes || []).length > 0`, "the onCrash hook");
    await Bun.sleep(1000);

    const crashes = await cdp.eval(`window.__crashes`);
    if (crashes.length !== 1) throw new Error(`ad-hoc-wasm-crash: onCrash ran ${crashes.length} times`);
    const r = crashes[0];
    // the report as a host gets it, for checking a host's symbolication
    writeFileSync(tmpPath("ad-hoc-wasm-crash.json"), JSON.stringify(r, null, 2));
    if (r.kind !== "trap") throw new Error(`ad-hoc-wasm-crash: kind is '${r.kind}'`);
    if (!/\S+ \S+/.test(r.build)) throw new Error(`ad-hoc-wasm-crash: build is '${r.build}'`);
    if (!r.log.includes("SumatraPDF")) throw new Error(`ad-hoc-wasm-crash: no log:\n${r.log.slice(-400)}`);

    const frames = symbolicate(r.stack, readFileSync(SYMBOLS, "utf8"));
    if (!frames.some((f) => f.includes(CRASH_FRAME))) {
      throw new Error(`ad-hoc-wasm-crash: no ${CRASH_FRAME} frame in:\n${r.stack}\n=>\n${frames.join("\n")}`);
    }
    const raw = r.stack.split("\n").slice(0, 3).join("\n");
    console.log(`ad-hoc-wasm-crash: ${r.message}\n${raw}\n=>\n  ${frames.slice(0, 8).join("\n  ")}`);
  } finally {
    cdp?.close();
    proc.kill();
    await proc.exited;
    server.stop(true);
    rmSync(profile, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  const watchdog = setTimeout(() => {
    console.log("ad-hoc-wasm-crash: the page stopped responding");
    process.exit(1);
  }, WATCHDOG_MS);
  await testit();
  clearTimeout(watchdog);
  console.log("✅ ad-hoc-wasm-crash passed");
  process.exit(0);
}
