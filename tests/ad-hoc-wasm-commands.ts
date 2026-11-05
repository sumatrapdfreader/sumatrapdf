// Every command of the wasm build, in a real browser (headless Chrome or Edge
// over the DevTools protocol). Each runs in a freshly loaded page with storage
// reset and a PDF restored. Fails on an exception, a main loop that stopped, or
// a command hidden on wasm that became visible. Also checks that the font host
// sends CORS headers, without which PDFs with non-embedded fonts render blank.
// Ad-hoc: it needs a wasm build (emsdk), a browser and the network; it takes
// about 15 minutes. `bun tests/ad-hoc-wasm-commands.ts Zoom` runs only the
// commands whose name contains "Zoom".

import { existsSync, mkdtempSync, readFileSync, rmSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { Cdp, waitFor } from "./ad-hoc-wasm-host.ts";
import { ROOT } from "./util.ts";

const OUT_DIR = join(ROOT, "out", "wasm", "rel");
const BROWSERS = [
  "C:/Program Files/Google/Chrome/Application/chrome.exe",
  "C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe",
];
const STEP_TIMEOUT_MS = 30_000;
const PDF = readFileSync(join(ROOT, "docs", "test", "bookmarks.pdf")).toString("base64");
const DOC_NAME = "commands.pdf";
// one of the base 14 fonts, which PDFs often use without embedding them
const FONT_URL = "https://files.sumatrapdfreader.org/assets/sumatrapdf/fonts/Dingbats-8e76.cff";
const SETTLE_MS = 1200;

// the names in gHiddenOnWasm (src/shared/CommandAvailability.cpp)
const HIDDEN_ON_WASM = [
  "CmdRunPDFScript",
  "CmdNewWindow",
  "CmdDuplicateInNewWindow",
  "CmdCheckUpdate",
  "CmdInstallPrereleaseUpdate",
  "CmdSumatraPDFWeb",
  "CmdSetScreenshotHotkey",
  "CmdSetInverseSearch",
  "CmdInvokeInverseSearch",
  "CmdToggleInverseSearch",
  "CmdExec",
  "CmdViewWithExternalViewer",
  "CmdListPrinters",
  "CmdFixDefaultApp",
  "CmdCreateShortcutToFile",
  "CmdOpenSettingsFile",
  "CmdDebugShowCrashMiniDumps",
  "CmdConfigureRestrictedUse",
  "CmdToggleFilePicker",
  "CmdOpenFileWithSumatraFilePicker",
  "CmdOpenFileWithOSFilePicker",
  "CmdShowInFolder",
  "CmdCopyFilePath",
];
// stop the app or the page by design
const SKIP = new Set(["CmdExit", "CmdDebugCrashMe", "CmdDebugCorruptMemory", "CmdNone"]);

// alert() and friends would block the page; nothing here answers them
const PAGE_SCRIPT = `
  window.__calls = [];
  for (const f of ["alert", "confirm", "prompt", "print"]) {
    window[f] = (...a) => { window.__calls.push(f); return f === "confirm" ? false : null; };
  }
  window.open = (u) => { window.__calls.push("window.open " + u); return null; };
  window.sumatraHost = {
    apiVersion: 1,
    pickFile(done) { setTimeout(() => done(null), 0); return true; },
    download() { return true; },
    openUrl() { return true; },
    exit() { return true; },
  };`;

type Vis = { id: number; name: string; menu: string; palette: string };
type State = "home" | "doc" | "sel";

function visible(v: Vis | undefined): boolean {
  return !!v && (v.menu === "show" || v.palette === "show");
}

async function checkFontCors(): Promise<void> {
  const rsp = await fetch(FONT_URL, { headers: { Origin: "http://localhost" } });
  const allow = rsp.headers.get("access-control-allow-origin");
  if (!rsp.ok || !allow) {
    throw new Error(
      `ad-hoc-wasm-commands: ${FONT_URL} answered ${rsp.status} without Access-Control-Allow-Origin. ` +
        "The CORS rule on the R2 bucket is gone (docs/wasm-host-api.md)",
    );
  }
  console.log(`ad-hoc-wasm-commands: ok: the font host allows ${allow}`);
}

class Page {
  constructor(
    public cdp: Cdp,
    private targetId: string,
  ) {}

  static async open(wsUrl: string): Promise<Page> {
    const cdp = await Cdp.connect(wsUrl);
    const { targetId } = await cdp.send("Target.createTarget", { url: "about:blank" });
    const { sessionId } = await cdp.send("Target.attachToTarget", { targetId, flatten: true });
    cdp.attach(sessionId);
    await cdp.send("Page.enable");
    await cdp.send("Runtime.enable");
    await cdp.send("Emulation.setFocusEmulationEnabled", { enabled: true });
    await cdp.send("Emulation.setDeviceMetricsOverride", {
      width: 1100,
      height: 800,
      deviceScaleFactor: 1,
      mobile: false,
    });
    await cdp.send("Page.addScriptToEvaluateOnNewDocument", { source: PAGE_SCRIPT });
    return new Page(cdp, targetId);
  }

  async close(): Promise<void> {
    await this.cdp.send("Target.closeTarget", { targetId: this.targetId }).catch(() => {});
    this.cdp.close();
  }

  // the stored files go away, the PDF comes back, before the app loads
  async load(base: string, state: State): Promise<void> {
    await this.cdp.send("Page.navigate", { url: `${base}blank` });
    await waitFor(this.cdp, `document.title === "blank"`, "the blank page");
    await this.cdp.eval(`(async () => {
      const r = await navigator.storage.getDirectory();
      for (const n of ["settings", "uploads"]) { try { await r.removeEntry(n, { recursive: true }); } catch {} }
      if (${state !== "home"}) {
        const d = await r.getDirectoryHandle("uploads", { create: true });
        const w = await (await d.getFileHandle("${DOC_NAME}", { create: true })).createWritable();
        await w.write(Uint8Array.from(atob("${PDF}"), (c) => c.charCodeAt(0)));
        await w.close();
      }
    })()`);
    const query = state === "home" ? "?test-hooks" : `?test-hooks&file=/uploads/${DOC_NAME}`;
    await this.cdp.send("Page.navigate", { url: base + query });
    await waitFor(
      this.cdp,
      `window.__sumatraStorage === "ready" && !document.getElementById("gpui-loading") && !!Module._sumatra_test_run`,
      "the app",
    );
    await Bun.sleep(SETTLE_MS);
    if (state === "sel") {
      await this.run("CmdSelectAll");
    }
  }

  async visibility(): Promise<Map<string, Vis>> {
    await this.cdp.eval(`Module._sumatra_test_visibility()`);
    const text: string = await this.cdp.eval(`window.__testVisibility`);
    const res = new Map<string, Vis>();
    for (const line of text.split("\n")) {
      if (!line) continue;
      const [id, name, menu, palette] = line.split("\t");
      res.set(name, { id: Number(id), name, menu, palette });
    }
    return res;
  }

  async run(name: string): Promise<void> {
    const v = (await this.visibility()).get(name);
    if (!v) throw new Error(`ad-hoc-wasm-commands: no command ${name}`);
    await this.cdp.eval(`Module._sumatra_test_run(${v.id})`);
    await Bun.sleep(500);
  }

  async alive(): Promise<boolean> {
    const deadline = Date.now() + 5000;
    try {
      const before = await this.cdp.eval(`Module._sumatra_test_pong()`);
      await this.cdp.eval(`Module._sumatra_test_ping()`);
      while (Date.now() < deadline) {
        if ((await this.cdp.eval(`Module._sumatra_test_pong()`)) > before) return true;
        await Bun.sleep(100);
      }
    } catch {
      // the page itself is stuck
    }
    return false;
  }
}

function checkHidden(state: State, vis: Map<string, Vis>): void {
  const shown = HIDDEN_ON_WASM.filter((name) => visible(vis.get(name)));
  if (shown.length > 0) {
    throw new Error(`ad-hoc-wasm-commands: hidden on wasm but visible (${state}): ${shown.join(", ")}`);
  }
}

export async function testit(filter = ""): Promise<void> {
  await checkFontCors();
  const browser = BROWSERS.find((b) => existsSync(b));
  if (!browser) {
    console.log("ad-hoc-wasm-commands: no Chrome or Edge, skipping");
    return;
  }
  const build = spawnSync("bun", ["cmd/ng-build.ts", "-wasm", "-rel", "SumatraPDF"], { cwd: ROOT, stdio: "inherit" });
  if (build.status !== 0) throw new Error("ad-hoc-wasm-commands: wasm build failed");

  // OPFS needs a secure context, which localhost is
  const server = Bun.serve({
    port: 0,
    async fetch(req) {
      const path = new URL(req.url).pathname;
      if (path === "/blank") {
        return new Response("<title>blank</title>", { headers: { "Content-Type": "text/html" } });
      }
      const f = Bun.file(join(OUT_DIR, path === "/" ? "SumatraPDF.html" : path));
      return (await f.exists()) ? new Response(f) : new Response("not found", { status: 404 });
    },
  });
  const base = `http://localhost:${server.port}/`;
  const profile = mkdtempSync(join(tmpdir(), "wasm-commands-"));
  const proc = Bun.spawn(
    [browser, "--headless=new", "--remote-debugging-port=0", `--user-data-dir=${profile}`, "--no-first-run"],
    { stdout: "ignore", stderr: "pipe" },
  );
  const failures: string[] = [];
  let page: Page | null = null;
  try {
    const portFile = join(profile, "DevToolsActivePort");
    const deadline = Date.now() + STEP_TIMEOUT_MS;
    let lines: string[] = [];
    while (lines.length < 2) {
      if (Date.now() > deadline) throw new Error("ad-hoc-wasm-commands: the browser didn't start");
      try {
        lines = readFileSync(portFile, "utf8").split("\n");
      } catch {
        // not written yet, or still open (EBUSY on Windows)
      }
      await Bun.sleep(100);
    }
    const wsUrl = `ws://127.0.0.1:${lines[0]}${lines[1]}`;
    page = await Page.open(wsUrl);

    const states = new Map<State, Map<string, Vis>>();
    for (const state of ["home", "doc", "sel"] as State[]) {
      await page.load(base, state);
      const vis = await page.visibility();
      checkHidden(state, vis);
      if (state !== "home" && !visible(vis.get("CmdProperties"))) {
        throw new Error("ad-hoc-wasm-commands: the PDF didn't load");
      }
      states.set(state, vis);
    }
    console.log("ad-hoc-wasm-commands: ok: commands hidden on wasm stay hidden");

    // each command once, in the first state that shows it
    const runs: { name: string; state: State }[] = [];
    const seen = new Set<string>();
    for (const state of ["doc", "sel", "home"] as State[]) {
      for (const v of states.get(state)!.values()) {
        if (seen.has(v.name) || !visible(v) || SKIP.has(v.name) || !v.name.includes(filter)) continue;
        seen.add(v.name);
        runs.push({ name: v.name, state });
      }
    }

    let n = 0;
    for (const { name, state } of runs) {
      n++;
      try {
        await page.load(base, state);
        page.cdp.events.length = 0;
        await page.run(name);
        await Bun.sleep(SETTLE_MS);
        const crash = page.cdp.events.find((e) => e.method === "Runtime.exceptionThrown");
        if (crash) {
          const { exception, text } = crash.params.exceptionDetails;
          throw new Error(String(exception?.description ?? text).split("\n")[0]);
        }
        if (!(await page.alive())) throw new Error("the main loop stopped");
        console.log(`${n}/${runs.length} ok ${name} [${state}]`);
      } catch (e) {
        failures.push(`${name} [${state}]: ${(e as Error).message}`);
        console.log(`${n}/${runs.length} FAILED ${name} [${state}]: ${(e as Error).message}`);
        // a crashed page can't be reloaded reliably; start over in a new one
        await page.close();
        page = await Page.open(wsUrl);
      }
    }
  } finally {
    await page?.close();
    proc.kill();
    await proc.exited;
    server.stop(true);
    rmSync(profile, { recursive: true, force: true });
  }
  if (failures.length > 0) {
    throw new Error(`ad-hoc-wasm-commands: ${failures.length} commands failed:\n${failures.join("\n")}`);
  }
}

if (import.meta.main) {
  await testit(process.argv[2] ?? "");
  console.log("✅ ad-hoc-wasm-commands passed");
  process.exit(0);
}
