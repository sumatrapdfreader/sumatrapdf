import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join, resolve } from "node:path";
import * as Minio from "minio";

export const filesHost = "files.sumatrapdfreader.org";
export const filesUrlPrefix = `https://${filesHost}/`;
export const r2Prefix = "assets/sumatrapdf/";
export const docsImgCdnPrefix = filesUrlPrefix + r2Prefix;

export function docsImgToCdnUrl(src: string): string {
  let s = src.replace(/%20/g, " ").replace(/\\/g, "/");
  if (s.startsWith("https://") || s.startsWith("http://")) {
    return s;
  }
  if (s.startsWith("./")) {
    s = s.slice(2);
  }
  if (s.startsWith("/img/")) {
    s = s.slice(1);
  }
  if (s.startsWith("img/")) {
    return docsImgCdnPrefix + s.slice("img/".length);
  }
  return s;
}

export function secretsEnvPath(): string {
  return resolve(import.meta.dir, "..", "..", "hack", "webapps", "sumatra-website", "secrets.env");
}

export function parseEnvFile(path: string): Record<string, string> {
  const out: Record<string, string> = {};
  for (const line of readFileSync(path, "utf-8").split(/\r?\n/)) {
    const t = line.trim();
    if (!t || t.startsWith("#")) {
      continue;
    }
    const i = t.indexOf("=");
    if (i < 0) {
      throw new Error(`invalid line in ${path}: ${line}`);
    }
    out[t.slice(0, i).trim()] = t.slice(i + 1).trim();
  }
  return out;
}

export function loadR2Env(): { endpoint: string; access: string; secret: string; bucket: string } {
  const env = parseEnvFile(secretsEnvPath());
  for (const k of ["ENDPOINT", "ACCESS", "SECRET", "BUCKET"] as const) {
    if (!env[k]) {
      throw new Error(`${k} missing in ${secretsEnvPath()}`);
    }
  }
  return { endpoint: env.ENDPOINT, access: env.ACCESS, secret: env.SECRET, bucket: env.BUCKET };
}

export function newR2Client(): Minio.Client {
  const env = loadR2Env();
  return new Minio.Client({
    endPoint: env.endpoint,
    accessKey: env.access,
    secretKey: env.secret,
    useSSL: true,
    region: "auto",
  });
}

export async function listR2Keys(prefix: string): Promise<Set<string>> {
  const env = loadR2Env();
  const client = newR2Client();
  const stream = client.listObjectsV2(env.bucket, prefix, true);
  const keys = new Set<string>();
  await new Promise<void>((resolve, reject) => {
    stream.on("data", (obj: { name?: string }) => {
      if (obj.name) {
        keys.add(obj.name);
      }
    });
    stream.on("error", reject);
    stream.on("end", () => resolve());
  });
  return keys;
}

const rxFilesUrl = /https?:\/\/files\.sumatrapdfreader\.org\/[^\s"'<>()\[\]]+/g;
const rxMdImg = /\]\(\s*(?:\.\.?\/)?(\/?img\/[^)\s]+)/g;

export function extractFilesUrls(text: string): string[] {
  const urls: string[] = [];
  let inCode = false;
  for (const line of text.split("\n")) {
    if (line.trim().startsWith("```")) {
      inCode = !inCode;
      continue;
    }
    if (inCode) {
      continue;
    }
    for (const m of line.matchAll(rxFilesUrl)) {
      let uri = m[0].replace(/[.,;:!?]+$/, "");
      if (uri.includes("{{") || uri.endsWith("/")) {
        continue;
      }
      urls.push(uri);
    }
    for (const m of line.matchAll(rxMdImg)) {
      urls.push(docsImgToCdnUrl(m[1]));
    }
  }
  return urls;
}

export function filesUrlToKey(uri: string): string {
  let key = uri.replace(/^https?:\/\//, "");
  key = key.replace(/^files\.sumatrapdfreader\.org\//, "");
  const q = key.search(/[?#]/);
  if (q >= 0) {
    key = key.slice(0, q);
  }
  try {
    key = decodeURIComponent(key);
  } catch {
    // keep encoded key
  }
  return key;
}

function walkMdFiles(dir: string, out: string[]): void {
  if (!existsSync(dir)) {
    return;
  }
  for (const name of readdirSync(dir, { withFileTypes: true })) {
    const p = join(dir, name.name);
    if (name.isDirectory()) {
      walkMdFiles(p, out);
      continue;
    }
    if (name.name.endsWith(".md")) {
      out.push(p);
    }
  }
}

export function collectMdFilesUrls(mdDirs: string[]): Map<string, string> {
  const res = new Map<string, string>();
  for (const dir of mdDirs) {
    const files: string[] = [];
    walkMdFiles(dir, files);
    for (const p of files) {
      for (const uri of extractFilesUrls(readFileSync(p, "utf-8"))) {
        if (!res.has(uri)) {
          res.set(uri, p);
        }
      }
    }
  }
  return res;
}

export async function checkCdnImages(mdDirs: string[], opts?: { required?: boolean }): Promise<void> {
  const secrets = secretsEnvPath();
  if (!existsSync(secrets)) {
    if (opts?.required) {
      throw new Error(`checkCdnImages: secrets.env not found at ${secrets}`);
    }
    console.log(`checkCdnImages: skip, no secrets at ${secrets}`);
    return;
  }

  const uris = collectMdFilesUrls(mdDirs);
  const keys = await listR2Keys(r2Prefix);
  const missing: string[] = [];
  for (const [uri, path] of uris) {
    if (!keys.has(filesUrlToKey(uri))) {
      missing.push(`  ${uri} (in ${path})`);
    }
  }
  if (missing.length > 0) {
    missing.sort();
    throw new Error(`checkCdnImages: ${missing.length} urls missing in r2:\n${missing.join("\n")}`);
  }
  console.log(
    `checkCdnImages: all ${uris.size} files.sumatrapdfreader.org urls exist in r2 (${keys.size} objects under ${r2Prefix})`,
  );
}
