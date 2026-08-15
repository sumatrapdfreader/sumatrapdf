// issue #4576: multi-open of password PDFs from Explorer created 2N-1 tabs
// (focused file once, each other file twice). Root cause: same path opened via
// cmdline and/or reuseInstance DDE/COPYDATA while the first load was still in
// the password dialog (tab not registered yet).
//
// This test covers the durable part of the fix without interactive password
// dialogs: opening the same path twice on the cmdline (or via a second process
// with ReuseInstance) must not create a second document tab.

import { mkdirSync, writeFileSync, existsSync } from "node:fs";
import { join } from "node:path";
import { EXE, ROOT, runStandalone, tmpPath } from "./util.ts";
import { spawn } from "node:child_process";
import { killAndWait } from "./winapi.ts";

const mutoolCandidates = [
  join(process.env.USERPROFILE || "", "OneDrive", "bin", "mupdf-1.27.0", "mutool.exe"),
  "mutool",
];

async function whichMutool(): Promise<string | null> {
  for (const c of mutoolCandidates) {
    if (c === "mutool") {
      continue;
    }
    if (existsSync(c)) {
      return c;
    }
  }
  return null;
}

function run(cmd: string, args: string[]): Promise<void> {
  return new Promise((resolve, reject) => {
    const p = spawn(cmd, args, { stdio: "ignore" });
    p.on("error", reject);
    p.on("exit", (code) => (code === 0 ? resolve() : reject(new Error(`${cmd} exit ${code}`))));
  });
}

async function ensurePasswordPdfs(dir: string): Promise<{ a: string; b: string }> {
  mkdirSync(dir, { recursive: true });
  const aPlain = join(dir, "plain-a.pdf");
  const bPlain = join(dir, "plain-b.pdf");
  const aPwd = join(dir, "a-pwd.pdf");
  const bPwd = join(dir, "b-pwd.pdf");
  if (existsSync(aPwd) && existsSync(bPwd)) {
    return { a: aPwd, b: bPwd };
  }
  const mutool = await whichMutool();
  if (!mutool) {
    console.log("skip issue-4576: mutool not found (need it to create password PDFs)");
    return { a: "", b: "" };
  }
  writeFileSync(
    join(dir, "a.txt"),
    "%%MediaBox 0 0 300 200\n%%Font Helv Helvetica\n.point 12\n.font Helv\n.color 0 0 0\n.text 50 100 File A\n",
  );
  writeFileSync(
    join(dir, "b.txt"),
    "%%MediaBox 0 0 300 200\n%%Font Helv Helvetica\n.point 12\n.font Helv\n.color 0 0 0\n.text 50 100 File B\n",
  );
  await run(mutool, ["create", "-o", aPlain, join(dir, "a.txt")]);
  await run(mutool, ["create", "-o", bPlain, join(dir, "b.txt")]);
  await run(mutool, ["clean", "-E", "aes-128", "-U", "test", "-O", "owner", aPlain, aPwd]);
  await run(mutool, ["clean", "-E", "aes-128", "-U", "test", "-O", "owner", bPlain, bPwd]);
  return { a: aPwd, b: bPwd };
}

function sleep(ms: number): Promise<void> {
  return new Promise((r) => setTimeout(r, ms));
}

/** Count LoadDocument success lines for a path basename in a log file. */
async function countLoads(logPath: string, basename: string): Promise<number> {
  if (!existsSync(logPath)) {
    return 0;
  }
  const text = await Bun.file(logPath).text();
  const re = new RegExp(`LoadDocument: .* pages for '.*${basename.replace(".", "\\.")}'`, "g");
  return (text.match(re) || []).length;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("bug-4576");
  const { a, b } = await ensurePasswordPdfs(dir);
  if (!a || !b) {
    return; // skipped
  }

  // --- 1) Same process: open A, B, A again. Third must not re-load A. ---
  const log1 = join(dir, "log-cmdline.txt");
  try {
    await Bun.file(log1).unlink();
  } catch {
    /* ok */
  }
  const p1 = spawn(EXE, ["-for-testing", "-pwd", "test", "-log", "-log-to-file", log1, a, b, a], {
    cwd: ROOT,
    stdio: "ignore",
  });
  const deadline1 = Date.now() + 8000;
  let loadsA1 = 0;
  let loadsB1 = 0;
  while (Date.now() < deadline1) {
    loadsA1 = await countLoads(log1, "a-pwd.pdf");
    loadsB1 = await countLoads(log1, "b-pwd.pdf");
    if (loadsA1 >= 1 && loadsB1 >= 1) {
      break;
    }
    await sleep(40);
  }
  await killAndWait(p1);
  if (loadsA1 !== 1) {
    throw new Error(`cmdline multi-open: expected 1 load of a-pwd.pdf, got ${loadsA1}`);
  }
  if (loadsB1 !== 1) {
    throw new Error(`cmdline multi-open: expected 1 load of b-pwd.pdf, got ${loadsB1}`);
  }

  // --- 2) ReuseInstance: primary A, then secondary B, then secondary A again. ---
  // Cannot use -for-testing (it disables reuseInstance).
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    [
      "ReuseInstance = true",
      "UseTabs = true",
      "RestoreSession = false",
      "RememberOpenedFiles = false",
      "ShowStartPage = false",
      "",
    ].join("\n"),
  );
  const log2 = join(dir, "log-reuse.txt");
  try {
    await Bun.file(log2).unlink();
  } catch {
    /* ok */
  }

  const primary = spawn(EXE, ["-appdata", appdata, "-pwd", "test", "-log", "-log-to-file", log2, a], {
    cwd: ROOT,
    stdio: "ignore",
  });
  const deadlineP = Date.now() + 5000;
  while (Date.now() < deadlineP) {
    if ((await countLoads(log2, "a-pwd.pdf")) >= 1) {
      break;
    }
    await sleep(40);
  }
  const secB = spawn(EXE, ["-appdata", appdata, "-pwd", "test", b], { cwd: ROOT, stdio: "ignore" });
  const deadlineB = Date.now() + 4000;
  while (Date.now() < deadlineB) {
    const text = existsSync(log2) ? await Bun.file(log2).text() : "";
    if ((text.match(/CreateControllerForEngineOrFile: '.*b-pwd\.pdf'/g) || []).length >= 1) {
      break;
    }
    await sleep(40);
  }
  const secA = spawn(EXE, ["-appdata", appdata, "-pwd", "test", a], { cwd: ROOT, stdio: "ignore" });
  await sleep(400);
  await killAndWait(secB);
  await killAndWait(secA);
  await killAndWait(primary);

  // Primary log: one sync load of A. B is async (StartLoadDocument) so it may not
  // emit "LoadDocument: N pages" — but a second A must not produce a second
  // CreateController for A.
  const text2 = existsSync(log2) ? await Bun.file(log2).text() : "";
  const createA = (text2.match(/CreateControllerForEngineOrFile: '.*a-pwd\.pdf'/g) || []).length;
  const createB = (text2.match(/CreateControllerForEngineOrFile: '.*b-pwd\.pdf'/g) || []).length;
  if (createA !== 1) {
    throw new Error(`reuseInstance: expected 1 CreateController for a-pwd.pdf, got ${createA}\n${text2}`);
  }
  if (createB !== 1) {
    throw new Error(`reuseInstance: expected 1 CreateController for b-pwd.pdf, got ${createB}\n${text2}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
