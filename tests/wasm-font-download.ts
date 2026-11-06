// tests/font-download.ts for the wasm build. Wasm embeds no fonts, so a page
// needs downloads before it can render at all. Under Node, test_engines reads
// them from SUMATRA_FONT_MIRROR (Node has no XHR) into SUMATRA_FONT_CACHE;
// everything after the fetch is the browser's code path.
//
// Not in tests/run-almost-all.ts: it needs emsdk. CI runs it in the Linux
// daily wasm job.
//
// Run: bun tests/wasm-font-download.ts

import { existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { remoteFontName, writeMirror } from "../cmd/upload-fonts.ts";
import { kTangutFont, makeTangutEpub } from "./font-download.ts";
import { tmpPath } from "./util.ts";

const name = "wasm-font-download";
const root = resolve(import.meta.dir, "..");
// the default EPUB font, and the one for the text
const fonts = ["CharisSIL.cff", "CharisSIL-Bold.cff", "CharisSIL-Italic.cff", "CharisSIL-BoldItalic.cff", kTangutFont];

// builds wasm test_engines if needed and renders page 1 under Node
async function render(epub: string, png: string, mirror: string, cache: string): Promise<{ ok: boolean; out: string }> {
  rmSync(png, { force: true });
  const p = Bun.spawn(["bun", "cmd/ng-build.ts", "-wasm", "-dbg", "test_engines", "-run", "--", epub, png, "1"], {
    cwd: root,
    env: { ...process.env, SUMATRA_FONT_MIRROR: mirror, SUMATRA_FONT_CACHE: cache },
    stdout: "pipe",
    stderr: "pipe",
  });
  const [out, err] = await Promise.all([new Response(p.stdout).text(), new Response(p.stderr).text()]);
  const code = await p.exited;
  return { ok: code === 0 && existsSync(png), out: out + err };
}

function downloads(out: string): number {
  return out.split("\n").filter((l) => /^font '.*': downloading/.test(l)).length;
}

export async function testit(): Promise<void> {
  const dir = tmpPath(name);
  rmSync(dir, { recursive: true, force: true });
  // forward slashes: under Node the app's paths go through emscripten's FS
  const mirror = join(dir, "mirror").replaceAll("\\", "/");
  const cache = join(dir, "cache").replaceAll("\\", "/");
  mkdirSync(cache, { recursive: true });
  writeMirror(mirror, fonts);
  const epub = join(dir, "tangut.epub").replaceAll("\\", "/");
  writeFileSync(epub, makeTangutEpub());
  const png = join(dir, "page.png").replaceAll("\\", "/");

  // empty cache: every font it needs comes from the mirror
  let r = await render(epub, png, mirror, cache);
  if (!r.ok) {
    throw new Error(`${name}: rendering with an empty cache failed\n${r.out.slice(-1500)}`);
  }
  for (const f of ["CharisSIL.cff", kTangutFont]) {
    if (!existsSync(join(cache, remoteFontName(f)))) {
      throw new Error(`${name}: ${f} is not in the cache\n${r.out.slice(-1500)}`);
    }
  }
  if (downloads(r.out) === 0) {
    throw new Error(`${name}: nothing was downloaded\n${r.out.slice(-1500)}`);
  }

  // all cached: no downloads
  r = await render(epub, png, mirror, cache);
  if (!r.ok || downloads(r.out) !== 0) {
    throw new Error(`${name}: a cached font was downloaded again\n${r.out.slice(-1500)}`);
  }

  // a mirror file that isn't the font is rejected and not kept
  const tangut = remoteFontName(kTangutFont);
  rmSync(join(cache, tangut));
  writeFileSync(join(mirror, tangut), "<html>not a font</html>");
  r = await render(epub, png, mirror, cache);
  if (!r.out.includes("doesn't match its sha1") || existsSync(join(cache, tangut))) {
    throw new Error(`${name}: a download with the wrong sha1 was kept\n${r.out.slice(-1500)}`);
  }
  console.log(`PASS ${name}: wasm downloads into the cache, reuses it, rejects a bad download`);
}

if (import.meta.main) {
  try {
    await testit();
  } catch (e) {
    console.error(`❌ ${(e as Error)?.message ?? e}`);
    process.exit(1);
  }
}
