// Auto-reload debounce: while another program is still writing the file we
// watch (a LaTeX run rewriting its .pdf), SumatraPDF must not reload a
// half-written document. It used to reload twice per rewrite -- once on the
// truncated file ("cannot find startxref" / "document has no pages"), once on
// the finished one. AUTO_RELOAD_TIMER now re-arms itself while size/mtime keep
// changing, so a rewrite produces exactly one reload of the final file.
//
// Uses -appdata so it never touches the user's real settings.
import { copyFileSync, mkdirSync, openSync, readFileSync, rmSync, writeFileSync, writeSync, closeSync } from "node:fs";
import { join } from "node:path";
import { IS_MAC } from "./host.ts";
import { EXE, ROOT, runStandalone, tmpPath } from "./util";
import { windowPosArgs } from "./win-automation";
import { sleep, killAndWait } from "./winapi";

function reloadCount(s: string): number {
  return (s.match(/ReloadDocument: .*auto refresh: 1/g) || []).length;
}

function brokenCount(s: string): number {
  return (s.match(/document has no pages|cannot find startxref/g) || []).length;
}

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
ReloadModifiedDocuments = true
`;

export async function testit(): Promise<void> {
  const src = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const dir = tmpPath("reload-debounce");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const doc = join(dir, "paper.pdf");
  copyFileSync(src, doc);
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), SETTINGS);

  const logPath = join(dir, "reload.log");
  const proc = Bun.spawn(
    [EXE, "-for-testing", ...windowPosArgs(), "-appdata", dir, ...(IS_MAC ? ["-log-to-file", logPath] : []), doc],
    {
      stdout: "pipe",
      // ASan writes its report to stderr. "ignore" closes that pipe and the
      // next write kills the process.
      stderr: IS_MAC ? "pipe" : "ignore",
    },
  );
  let out = "";
  const pump = (async () => {
    for await (const chunk of proc.stdout as ReadableStream) {
      out += new TextDecoder().decode(chunk);
    }
  })();
  if (IS_MAC && proc.stderr) {
    void (async () => {
      for await (const _chunk of proc.stderr as ReadableStream) {
        // drain
      }
    })();
  }
  const appLog = (): string => {
    if (!IS_MAC) {
      return out;
    }
    try {
      return readFileSync(logPath, "utf8");
    } catch {
      return "";
    }
  };
  try {
    const loadedUntil = Date.now() + 15000;
    while (!/LoadDocument: .* pages for /.test(appLog())) {
      if (Date.now() > loadedUntil) {
        throw new Error("reload-debounce: document never finished the initial load");
      }
      await sleep(40);
    }
    let seen = appLog().length; // don't count the initial load
    const fresh = (): string => appLog().slice(seen);

    // rewrite the way pdflatex does: truncate, then keep the size changing
    // so AUTO_RELOAD_TIMER re-arms instead of loading a half-written file
    const data = readFileSync(src);
    const fd = openSync(doc, "w");
    const nChunks = 8;
    const chunkSize = Math.ceil(data.length / nChunks);
    for (let i = 0; i < nChunks; i++) {
      writeSync(fd, data.subarray(i * chunkSize, (i + 1) * chunkSize));
      await sleep(80);
    }
    closeSync(fd);

    const reloadUntil = Date.now() + 4000;
    while (reloadCount(fresh()) < 1) {
      if (brokenCount(fresh()) > 0) {
        throw new Error(`reloaded a half-written file (reloads: ${reloadCount(fresh())})`);
      }
      if (Date.now() > reloadUntil) {
        throw new Error(`expected exactly 1 reload of the finished file, got ${reloadCount(fresh())}`);
      }
      await sleep(40);
    }
    // one more debounce window so a second reload would still show up
    const quietUntil = Date.now() + 700;
    while (Date.now() < quietUntil) {
      if (brokenCount(fresh()) > 0) {
        throw new Error(`reloaded a half-written file (reloads: ${reloadCount(fresh())})`);
      }
      if (reloadCount(fresh()) !== 1) {
        throw new Error(`expected exactly 1 reload of the finished file, got ${reloadCount(fresh())}`);
      }
      await sleep(40);
    }
  } finally {
    await killAndWait(proc);
    await pump;
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
