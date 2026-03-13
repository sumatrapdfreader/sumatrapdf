import { existsSync, readdirSync, statSync } from "node:fs";
import { join } from "node:path";

const binDir = join("bin");
const zopflipngPath = join(binDir, "zopflipng.exe");
const imgDir = join("docs", "md", "img");
const downloadUrl = "https://drpleaserespect.github.io/drpleaserespect-webassets/compiled_binaries/google/zopflipng.exe";

async function ensureZopflipng(): Promise<void> {
  if (existsSync(zopflipngPath)) {
    console.log(`using cached ${zopflipngPath}`);
    return;
  }
  console.log(`downloading zopflipng.exe from ${downloadUrl}`);
  const resp = await fetch(downloadUrl);
  if (!resp.ok) {
    throw new Error(`download failed: ${resp.status} ${resp.statusText}`);
  }
  const data = await resp.arrayBuffer();
  await Bun.write(zopflipngPath, data);
  console.log(`saved ${zopflipngPath}`);
}

async function compressImage(pngPath: string): Promise<void> {
  const sizeBefore = statSync(pngPath).size;
  // zopflipng overwrites in-place when input and output are the same
  const proc = Bun.spawn([zopflipngPath, "-y", pngPath, pngPath], {
    stdout: "pipe",
    stderr: "pipe",
  });
  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    const stderr = await new Response(proc.stderr).text();
    console.log(`  warning: zopflipng failed on ${pngPath}: ${stderr.trim()}`);
    return;
  }
  const sizeAfter = statSync(pngPath).size;
  const saved = sizeBefore - sizeAfter;
  if (saved > 0) {
    console.log(`  ${pngPath}: ${sizeBefore} -> ${sizeAfter} (saved ${saved} bytes)`);
  } else {
    console.log(`  ${pngPath}: ${sizeBefore} bytes (already optimal)`);
  }
}

async function main(): Promise<void> {
  await ensureZopflipng();

  const files = readdirSync(imgDir)
    .filter((f) => f.toLowerCase().endsWith(".png"))
    .map((f) => join(imgDir, f));

  console.log(`compressing ${files.length} PNG files in ${imgDir}`);
  for (const file of files) {
    await compressImage(file);
  }
  console.log("done");
}

if (import.meta.main) {
  await main();
}
