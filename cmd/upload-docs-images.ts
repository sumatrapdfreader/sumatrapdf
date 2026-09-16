// One-time: upload docs/md/img and website www/img to r2 as assets/sumatrapdf/{name}
// and verify they are served via https://files.sumatrapdfreader.org/assets/sumatrapdf/{name}
//
// Run from repo root: bun cmd/upload-docs-images.ts

import { createHash } from "node:crypto";
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { extname, join, resolve } from "node:path";
import { filesUrlPrefix, loadR2Env, newR2Client, r2Prefix } from "./r2";

const repoRoot = resolve(import.meta.dir, "..");
const srcDirs = [
  join(repoRoot, "docs", "md", "img"),
  resolve(import.meta.dir, "..", "..", "hack", "webapps", "sumatra-website", "www", "img"),
];

function contentTypeForFile(path: string): string {
  switch (extname(path).toLowerCase()) {
    case ".png":
      return "image/png";
    case ".jpg":
    case ".jpeg":
      return "image/jpeg";
    case ".gif":
      return "image/gif";
    case ".webp":
      return "image/webp";
    case ".svg":
      return "image/svg+xml";
    default:
      return "application/octet-stream";
  }
}

function sha1Hex(buf: Buffer): string {
  return createHash("sha1").update(buf).digest("hex");
}

function assertHashedName(dir: string, name: string): void {
  const buf = readFileSync(join(dir, name));
  const want = sha1Hex(buf).slice(0, 4);
  const m = name.match(/^(.+)-([0-9a-fA-F]{4})(\.[^.]+)$/);
  if (!m) {
    throw new Error(`${join(dir, name)}: basename must end with -XXXX before the extension (sha1[:4]=${want})`);
  }
  const got = m[2].toLowerCase();
  if (got !== want) {
    throw new Error(`${join(dir, name)}: sha1[:4] is ${want}, name has ${got}`);
  }
}

async function main(): Promise<void> {
  const env = loadR2Env();
  const client = newR2Client();

  const files = new Map<string, { path: string; size: number }>();
  for (const dir of srcDirs) {
    if (!existsSync(dir)) {
      throw new Error(`missing image dir ${dir}`);
    }
    for (const name of readdirSync(dir)) {
      const path = join(dir, name);
      if (!statSync(path).isFile()) {
        continue;
      }
      assertHashedName(dir, name);
      const size = statSync(path).size;
      const prev = files.get(name);
      if (prev && prev.size !== size) {
        throw new Error(`name collision with different content: ${prev.path} vs ${path}`);
      }
      if (!prev) {
        files.set(name, { path, size });
      }
    }
  }
  console.log(`${files.size} unique image files`);

  const existing = new Map<string, number>();
  const stream = client.listObjectsV2(env.bucket, r2Prefix, true);
  await new Promise<void>((resolve, reject) => {
    stream.on("data", (obj: { name?: string; size?: number }) => {
      if (obj.name) {
        existing.set(obj.name, obj.size ?? 0);
      }
    });
    stream.on("error", reject);
    stream.on("end", () => resolve());
  });
  console.log(`${existing.size} objects already under ${r2Prefix}`);

  const keys: string[] = [];
  let nUploaded = 0;
  for (const [name, file] of files) {
    const key = r2Prefix + name;
    keys.push(key);
    if (existing.get(key) === file.size) {
      continue;
    }
    const ct = contentTypeForFile(file.path);
    await client.fPutObject(env.bucket, key, file.path, { "Content-Type": ct });
    nUploaded++;
    console.log(`uploaded ${file.path} => ${key} (${ct}, ${file.size} bytes)`);
  }
  console.log(`uploaded ${nUploaded} files, ${keys.length - nUploaded} already existed`);

  let nFailed = 0;
  for (const key of keys) {
    const uri = filesUrlPrefix + key;
    const rsp = await fetch(uri, { method: "HEAD" });
    if (!rsp.ok) {
      console.log(`FAILED: HEAD ${uri}: status ${rsp.status}`);
      nFailed++;
      continue;
    }
    const ct = rsp.headers.get("content-type") ?? "";
    if (!ct.startsWith("image/")) {
      console.log(`WARNING: ${uri} has Content-Type '${ct}'`);
    }
  }
  console.log(`verified ${keys.length - nFailed} of ${keys.length} files served via ${filesUrlPrefix}`);
  if (nFailed > 0) {
    process.exit(1);
  }
}

if (import.meta.main) {
  await main();
}
