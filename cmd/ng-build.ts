// The single build entry point. Compiles and links targets from
// cmd/helper/ng-targets.ts by invoking the compiler directly (see
// cmd/helper/ng-compile.ts); no generated project files.
//
//   bun cmd/ng-build.ts -dbg                  # SumatraPDF and its libraries
//   bun cmd/ng-build.ts -rel -run -- file.pdf # release, then run with args
//   bun cmd/ng-build.ts -dbg test_util -run   # one target
//   bun cmd/ng-build.ts -linux -dbg           # Linux; from Windows, inside WSL
//   bun cmd/ng-build.ts -wasm -rel            # emscripten (any host)
//   bun cmd/ng-build.ts -dbg -clean -v        # wipe output dir first, echo commands
//   bun cmd/ng-build.ts -all -rel             # every target for this platform

import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { cpus } from "node:os";
import { basename, dirname, join, relative } from "node:path";
import { emsdkRoots, findToolchain, hostPlatform, type Platform } from "./helper/ng-toolchain";
import {
  buildTarget,
  cleanOutDir,
  depsOf,
  mergeEnv,
  outDir,
  outDirName,
  root,
  stageShared,
  type BuildFlags,
} from "./helper/ng-compile";
import { defaultTarget, findTarget, targetsFor } from "./helper/ng-targets";
import { runBuildInWsl } from "./helper/ng-wsl";
import { genDocsForBuild } from "./gen-docs";
import { packEmbedded } from "./helper/embedded";

const usage = `Usage: bun cmd/ng-build.ts <-dbg|-rel> [options] [target...] [-- <run args>]

Configuration (one required):
  -dbg              debug build (out/<platform>/dbg)
  -rel              release build (out/<platform>/rel)

Options:
  -linux            build for Linux; from Windows this reruns in WSL Ubuntu
  -mac              build for macOS (macOS host only)
  -wasm             build for the browser with emscripten (any host)
  -clang            Windows: clang-cl instead of cl.exe
  -asan             address sanitizer
  -profile          Windows, cl.exe: orig's Profile build (PerfLog, /callcap);
                    run with -start-perf-log -log-perf-file <path>
  -clean            delete the output directory first
  -all              build every target for this platform
  -run              run the (single) built target afterwards; args after --
  -v                echo every compiler and linker command
  -h | -help        this text

Targets (default: ${defaultTarget}):`;

class CliError extends Error {}

type Options = {
  flags: BuildFlags;
  plat: Platform;
  targets: string[];
  all: boolean;
  run: boolean;
  runArgs: string[];
};

function parseArgs(args: string[]): Options {
  const flags: BuildFlags = { debug: false, asan: false, clang: false, clean: false, verbose: false };
  let config: "dbg" | "rel" | null = null;
  let plat: Platform = hostPlatform();
  const targets: string[] = [];
  let all = false;
  let run = false;
  let runArgs: string[] = [];
  for (let i = 0; i < args.length; i++) {
    const a = args[i]!;
    if (a === "--") {
      runArgs = args.slice(i + 1);
      break;
    }
    if (a === "-dbg" || a === "-rel") {
      if (config && config !== a.slice(1)) throw new CliError("-dbg and -rel cannot be used together");
      config = a.slice(1) as "dbg" | "rel";
    } else if (a === "-wasm") plat = "wasm";
    else if (a === "-linux") plat = "linux";
    else if (a === "-mac") plat = "mac";
    else if (a === "-clang") flags.clang = true;
    else if (a === "-asan") flags.asan = true;
    else if (a === "-profile") flags.profile = true;
    else if (a === "-clean") flags.clean = true;
    else if (a === "-all") all = true;
    else if (a === "-run") run = true;
    else if (a === "-v") flags.verbose = true;
    else if (a === "-h" || a === "-help" || a === "--help") throw new CliError("");
    else if (a.startsWith("-")) throw new CliError(`unknown option: ${a}`);
    else targets.push(a);
  }
  if (!config) throw new CliError("missing -dbg or -rel");
  flags.debug = config === "dbg";
  if (run && (all || targets.length > 1)) throw new CliError("-run needs exactly one target");
  return { flags, plat, targets, all, run, runArgs };
}

function printUsage(plat: Platform): void {
  console.log(usage);
  for (const t of targetsFor(plat)) {
    console.log(`  ${t.name.padEnd(16)} ${t.kind}`);
  }
}

function fail(msg: string): never {
  console.error(msg);
  process.exit(1);
}

async function runExe(plat: Platform, exe: string, args: string[], env: Record<string, string>): Promise<number> {
  if (plat === "wasm" && exe.endsWith(".html")) return serveWasm(exe);
  if (plat === "wasm") {
    const node = nodeExe(env);
    console.log(`running ${node} ${relative(root, exe)} ${args.join(" ")}`);
    const p = Bun.spawn([node, exe, ...args], {
      cwd: root,
      env: mergeEnv(process.env, env),
      stdout: "inherit",
      stderr: "inherit",
    });
    return p.exited;
  }
  console.log(`running ${relative(root, exe)} ${args.join(" ")}`);
  if (basename(exe) === "SumatraPDF.exe" || basename(exe) === "SumatraPDF") args = ["-for-testing", ...args];
  const p = Bun.spawn([exe, ...args], { cwd: root, stdout: "inherit", stderr: "inherit" });
  return p.exited;
}

// emsdk brings its own node; fall back to one on PATH
function nodeExe(env: Record<string, string>): string {
  const fromEmsdk = env["EMSDK_NODE"] || process.env["EMSDK_NODE"];
  if (fromEmsdk && existsSync(fromEmsdk)) return fromEmsdk;
  for (const r of emsdkRoots(root)) {
    const nodeDir = join(r, "node");
    if (!existsSync(nodeDir)) continue;
    for (const v of readdirSync(nodeDir)) {
      const exe = join(nodeDir, v, "bin", "node");
      if (existsSync(exe)) return exe;
      if (existsSync(`${exe}.exe`)) return `${exe}.exe`;
      const win = join(nodeDir, v, "node.exe");
      if (existsSync(win)) return win;
    }
  }
  return Bun.which("node") ?? "node";
}

// The build has no pthreads, so no cross-origin isolation is needed; the page
// only has to come back uncached with the right content types.
const wasmHeaders = { "Cache-Control": "no-store" };

const wasmMimeTypes: Record<string, string> = {
  html: "text/html; charset=utf-8",
  js: "text/javascript; charset=utf-8",
  wasm: "application/wasm",
  data: "application/octet-stream",
};

// .wasm is tens of megabytes uncompressed and compresses about 4:1, which is
// the difference between a usable and an unusable page over anything but
// loopback. Keyed by mtime so a rebuild is not served from the old bytes.
const gzipCache = new Map<string, { mtime: number; body: Uint8Array }>();

function gzipOf(file: string, mtime: number): Uint8Array {
  const hit = gzipCache.get(file);
  if (hit && hit.mtime === mtime) return hit.body;
  const body = Bun.gzipSync(readFileSync(file));
  gzipCache.set(file, { mtime, body });
  return body;
}

async function serveWasm(page: string): Promise<number> {
  const dir = dirname(page);
  const index = basename(page);
  console.log(`serving out/${relative(join(root, "out"), dir)}/${index}`);
  const server = Bun.serve({
    port: 8085,
    async fetch(req) {
      const url = new URL(req.url);
      const path = url.pathname === "/" ? `/${index}` : url.pathname;
      const file = join(dir, path);
      const f = Bun.file(file);
      if (!(await f.exists())) return new Response("not found", { status: 404, headers: wasmHeaders });
      const ext = path.slice(path.lastIndexOf(".") + 1);
      const headers: Record<string, string> = { ...wasmHeaders };
      const type = wasmMimeTypes[ext];
      if (type) headers["Content-Type"] = type;
      if ((req.headers.get("accept-encoding") ?? "").includes("gzip")) {
        headers["Content-Encoding"] = "gzip";
        return new Response(gzipOf(file, statSync(file).mtimeMs), { headers });
      }
      return new Response(f, { headers });
    },
  });
  console.log(`http://localhost:${server.port}/`);
  await new Promise(() => {});
  return 0;
}

async function main(): Promise<void> {
  if (process.argv.length === 2) {
    printUsage(hostPlatform());
    return;
  }
  let opts: Options;
  try {
    opts = parseArgs(process.argv.slice(2));
  } catch (e) {
    if (!(e instanceof CliError)) throw e;
    if (e.message) console.error(`${e.message}\n`);
    printUsage(hostPlatform());
    process.exit(e.message ? 1 : 0);
  }
  const { flags, plat } = opts;
  // -linux on a Windows host: the same build, run inside WSL on the same tree
  if (plat === "linux" && hostPlatform() === "win") {
    process.exit(
      await runBuildInWsl(
        root,
        process.argv.slice(2).filter((a) => a !== "-linux"),
      ),
    );
  }
  if (plat !== hostPlatform() && plat !== "wasm") {
    fail(`-${plat} needs a ${plat} host (this is ${hostPlatform()})`);
  }
  const names = opts.all
    ? targetsFor(plat).map((t) => t.name)
    : opts.targets.length > 0
      ? opts.targets
      : [defaultTarget];
  const targets = names.map((n) => findTarget(n) ?? fail(`unknown target: ${n} (bun cmd/ng-build.ts for the list)`));
  for (const t of targets) {
    if (t.platforms && !t.platforms.includes(plat)) fail(`target ${t.name} does not support ${plat}`);
  }
  const tc = findToolchain(root, plat, flags.clang, fail);
  console.log(`${tc.label} -> out/${outDirName(plat, flags)} (${cpus().length} jobs)`);
  if (flags.clean) cleanOutDir(plat, flags);
  stageShared(outDir(plat, flags));

  if (targets.some((t) => t.embedded || depsOf(t, fail).some((d) => d.embedded))) {
    await genDocsForBuild();
    const host = hostPlatform();
    const packFlags: BuildFlags = { debug: false, asan: false, clang: false, clean: false, verbose: flags.verbose };
    const packTc = findToolchain(root, host, false, fail);
    stageShared(outDir(host, packFlags));
    const packer = await buildTarget(packTc, findTarget("MakeLZSA")!, packFlags, fail);
    await packEmbedded(packTc, packer, outDir(plat, flags));
  }

  const started = performance.now();
  let last = "";
  for (const t of targets) {
    last = await buildTarget(tc, t, flags, fail);
  }
  console.log(`done in ${((performance.now() - started) / 1000).toFixed(1)} s`);

  if (opts.run && targets[0]!.kind !== "staticlib") {
    process.exit(await runExe(plat, last, opts.runArgs, tc.env));
  }
}

await main();
