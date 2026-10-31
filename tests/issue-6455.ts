// Opening a CAJ file downloads caj2pdf, converts to a cached PDF and later updates
// the converter. A local server publishes a small fake converter
// (issue-6455.fake-caj2pdf.c), so no network or real caj2pdf is involved.
//
// Needs a debug build (the url and dir overrides are debug-only) and cl.exe.
//
// Run: bun tests/issue-6455.ts [--no-build]

import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, rmSync, utimesSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE_FROM_ARGV, ROOT, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const kDayMs = 24 * 60 * 60 * 1000;
const kPollTimeoutMs = 20000;
const kPollIntervalMs = 50;

interface Published {
  ver: string;
  exe: Buffer;
  sha1?: string;
}

// EXE is a copy under .work, so go by what was asked for. The default is the debug build.
function isReleaseExe(): boolean {
  const source = EXE_FROM_ARGV || process.env.SUMATRA_TEST_EXE || "";
  return /[\\/]rel[^\\/]*[\\/]/i.test(source);
}

function hasCl(): boolean {
  try {
    return Bun.spawnSync(["where.exe", "cl.exe"], { stdout: "ignore", stderr: "ignore" }).exitCode === 0;
  } catch {
    return false;
  }
}

function buildFakeConverter(dir: string): Buffer {
  const src = join(ROOT, "tests", "issue-6455.fake-caj2pdf.c");
  const r = Bun.spawnSync(["cl.exe", "/nologo", "/MT", "/Fe:fake.exe", src], {
    cwd: dir,
    stdout: "pipe",
    stderr: "pipe",
  });
  if (r.exitCode !== 0) {
    throw new Error(`issue-6455: compiling the fake converter failed: ${r.stdout.toString()}`);
  }
  return readFileSync(join(dir, "fake.exe"));
}

async function pollUntil(what: string, cond: () => boolean): Promise<void> {
  const deadline = Date.now() + kPollTimeoutMs;
  while (!cond()) {
    if (Date.now() > deadline) {
      throw new Error(`issue-6455: timed out waiting for ${what}`);
    }
    await Bun.sleep(kPollIntervalMs);
  }
}

function age(path: string, ms: number): void {
  const t = new Date(Date.now() - ms);
  utimesSync(path, t, t);
}

export async function testit(): Promise<void> {
  if (process.platform !== "win32" || isReleaseExe() || !hasCl()) {
    console.log("issue-6455: skipped, needs a Windows debug build and cl.exe");
    return;
  }

  const dir = tmpPath("issue-6455");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const cajDir = join(dir, "caj2pdf");
  const latestPath = join(cajDir, "latestver.json");
  const logPath = join(dir, "sumatra.log");
  const doc = join(dir, "paper.caj");
  const pdf = readFileSync(join(ROOT, "ext", "a-zlib", "zlib.3.pdf"));
  writeFileSync(doc, Buffer.concat([Buffer.from("CAJ\0", "latin1"), pdf]));

  const fake = buildFakeConverter(dir);
  let published: Published = { ver: "1.0.0", exe: fake, sha1: "0".repeat(40) };
  const requests: string[] = [];
  const server = Bun.serve({
    port: 0,
    fetch(req) {
      const path = new URL(req.url).pathname;
      requests.push(path);
      if (path === "/latestver.json") {
        const sha1 = published.sha1 ?? createHash("sha1").update(published.exe).digest("hex");
        return Response.json({ Ver: published.ver, Sha1: sha1, Size: published.exe.length });
      }
      if (path === `/${published.ver}/caj2pdf.exe`) {
        return new Response(published.exe);
      }
      return new Response("not found", { status: 404 });
    },
  });
  const env = { SUMATRAPDF_CAJ2PDF_URL: `http://127.0.0.1:${server.port}`, SUMATRAPDF_CAJ2PDF_DIR: cajDir };

  const readLog = () => (existsSync(logPath) ? readFileSync(logPath, "latin1") : "");

  // after(): checks made while the document is open, for what runs in the background
  const open = async (after?: () => Promise<void>): Promise<string> => {
    rmSync(logPath, { force: true });
    requests.length = 0;
    const { proc, client } = await launchControlled(["-log-to-file", logPath, doc], { env });
    try {
      await client.waitForRenderIdle(kPollTimeoutMs);
      const info = await client.chapterInfo();
      if (info.pageCount < 1) {
        throw new Error(`issue-6455: pageCount=${info.pageCount}`);
      }
      await after?.();
      await client.quit();
    } finally {
      await killAndWait(proc);
    }
    return readLog();
  };

  const exeOf = (ver: string) => join(cajDir, ver, "caj2pdf.exe");

  try {
    // a download that doesn't match latestver.json is not installed
    const bad = await launchControlled(["-log-to-file", logPath, doc], { env });
    try {
      await pollUntil("the rejected download", () => readLog().includes("doesn't match latestver.json"));
      if (existsSync(exeOf("1.0.0")) || existsSync(latestPath)) {
        throw new Error("issue-6455: installed a converter with a wrong sha1");
      }
      await bad.client.quit();
    } finally {
      await killAndWait(bad.proc);
    }

    // first open installs the converter
    published = { ver: "1.0.0", exe: fake };
    let log = await open();
    if (!log.includes("caj2pdf: converted") || !existsSync(exeOf("1.0.0"))) {
      throw new Error("issue-6455: first open didn't install and convert");
    }
    if (!requests.includes("/latestver.json") || !requests.includes("/1.0.0/caj2pdf.exe")) {
      throw new Error(`issue-6455: first open requested ${requests.join(" ")}`);
    }

    // second open uses the cached PDF and stays off the network
    log = await open();
    if (!log.includes("caj2pdf: cache hit") || requests.length !== 0) {
      throw new Error(`issue-6455: second open: requests ${requests.join(" ")}`);
    }

    // an edited file converts again, without checking for updates while latestver.json is fresh
    age(doc, kDayMs);
    log = await open();
    if (!log.includes("caj2pdf: converted") || requests.length !== 0) {
      throw new Error(`issue-6455: edited file: requests ${requests.join(" ")}`);
    }

    // a day later the check finds a new version; the document doesn't wait for it
    published = { ver: "2.0.0", exe: Buffer.concat([fake, Buffer.from("v2")]) };
    age(latestPath, 2 * kDayMs);
    age(doc, 2 * kDayMs);
    log = await open(() => pollUntil("the update", () => JSON.parse(readFileSync(latestPath, "utf8")).Ver === "2.0.0"));
    if (!log.includes(join("1.0.0", "caj2pdf.exe")) || !existsSync(exeOf("2.0.0"))) {
      throw new Error("issue-6455: update should apply from the next conversion on");
    }

    // the next conversion uses it, and a later check removes what the first one couldn't
    age(latestPath, 2 * kDayMs);
    age(doc, 3 * kDayMs);
    log = await open(() => pollUntil("removing the old version", () => !existsSync(join(cajDir, "1.0.0"))));
    if (!log.includes(join("2.0.0", "caj2pdf.exe"))) {
      throw new Error("issue-6455: conversion didn't use the updated converter");
    }
  } finally {
    server.stop(true);
    rmSync(dir, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
