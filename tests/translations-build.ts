import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import * as translations from "../cmd/trans-dl";
import { runStandalone } from "./util.ts";

export async function testit(): Promise<void> {
  const dir = mkdtempSync(join(tmpdir(), "sumatra-translations-"));
  const cache = join(dir, "translations.txt");
  let requests = 0;
  let status = 200;
  const server = Bun.serve({
    hostname: "127.0.0.1",
    port: 0,
    fetch(req) {
      requests++;
      if (req.method !== "GET" || new URL(req.url).pathname !== "/api/dltranshist")
        throw new Error("Expected read-only history download");
      return new Response(
        't,1,test,cn,File,旧翻译\nt,2,test,cn,File,文件\nt,3,test,de,File,Datei\nt,4,test,cn,"Open, file","打开,文件"\n',
        { status },
      );
    },
  });
  try {
    writeFileSync(cache, "");
    await translations.ensureTranslationsForBuild(cache, ["File", "Open, file"], server.url.origin);
    const text = readFileSync(cache, "utf8");
    if (
      !text.includes("cn:文件") ||
      !text.includes("de:Datei") ||
      !text.includes("cn:打开,文件") ||
      text.includes("旧翻译")
    )
      throw new Error("Missing latest public translations");
    await translations.ensureTranslationsForBuild(cache, ["File"], server.url.origin);
    if (requests !== 1) throw new Error("Valid cache was downloaded again");
    writeFileSync(cache, "AppTranslator: SumatraPDF\n" + "0".repeat(40) + "\n:File\n");
    status = 503;
    let failed = false;
    try {
      await translations.ensureTranslationsForBuild(cache, ["File"], server.url.origin);
    } catch {
      failed = true;
    }
    if (!failed) throw new Error("Build silently accepted missing translations");
  } finally {
    server.stop(true);
    rmSync(dir, { recursive: true, force: true });
  }
}

if (import.meta.main) await runStandalone(testit);
