// ci-build.ts - replaces Go "-ci" flag
// Called from GitHub Actions CI on push and repository_dispatch events
import { existsSync, readFileSync, writeFileSync, statSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
import { createHmac, createHash } from "node:crypto";
import { getGitLinearVersion, extractSumatraVersion, runLogged, getGitSha1, detectVisualStudio2026 } from "./util";

// const { msbuildPath, llvmPdbutilPath } = detectVisualStudio2022();
// const slnPath = join("vs2022", "SumatraPDF.sln");

const { msbuildPath, llvmPdbutilPath } = detectVisualStudio2026();
const slnPath = join("vs2026", "SumatraPDF.slnx");

const pdbFiles = ["libmupdf.pdb", "SumatraPDF-dll.pdb", "SumatraPDF.pdb"];

// === Secrets ===

let r2Access = "";
let r2Secret = "";
let b2Access = "";
let b2Secret = "";

function getSecrets(): void {
  const secretsPath = resolve(join("..", "secrets", "sumatrapdf.env"));
  if (existsSync(secretsPath)) {
    const content = readFileSync(secretsPath, "utf-8");
    const m = new Map<string, string>();
    for (const line of content.split("\n")) {
      const eq = line.indexOf("=");
      if (eq > 0) {
        m.set(line.slice(0, eq).trim(), line.slice(eq + 1).trim());
      }
    }
    const getEnv = (key: string, minLen: number): string => {
      const v = (m.get(key) ?? "").trim();
      if (v.length < minLen) {
        console.log(`Missing ${key}, len: ${v.length}, wanted: ${minLen}`);
        return "";
      }
      console.log(`Got ${key}`);
      return v;
    };
    r2Access = getEnv("R2_ACCESS", 8);
    r2Secret = getEnv("R2_SECRET", 8);
    b2Access = getEnv("BB_ACCESS", 8);
    b2Secret = getEnv("BB_SECRET", 8);
    return;
  }
  console.log(`Failed to read secrets from ${secretsPath}, will try env variables`);
  r2Access = process.env["R2_ACCESS"] ?? "";
  r2Secret = process.env["R2_SECRET"] ?? "";
  b2Access = process.env["BB_ACCESS"] ?? "";
  b2Secret = process.env["BB_SECRET"] ?? "";
}

function ensureAllUploadCreds(): void {
  if (!r2Access) throw new Error("R2_ACCESS not set");
  if (!r2Secret) throw new Error("R2_SECRET not set");
  if (!b2Access) throw new Error("BB_ACCESS not set");
  if (!b2Secret) throw new Error("BB_SECRET not set");
}

// === Version Detection ===

// === Build Config ===

function buildConfigPath(): string {
  return join("src", "utils", "BuildConfig.h");
}

function setBuildConfigPreRelease(sha1: string, preRelVer: string): void {
  const todayDate = new Date().toISOString().slice(0, 10);
  let s = `#define GIT_COMMIT_ID ${sha1}\n`;
  s += `#define BUILT_ON ${todayDate}\n`;
  s += `#define PRE_RELEASE_VER ${preRelVer}\n`;
  writeFileSync(buildConfigPath(), s, "utf-8");
}

async function revertBuildConfig(): Promise<void> {
  const proc = Bun.spawn(["git", "checkout", buildConfigPath()], {
    stdout: "inherit",
    stderr: "inherit",
  });
  await proc.exited;
}

function ensureManualIsBuilt(): void {
  const path = join("docs", "manual.dat");
  let size = 0;
  try {
    size = statSync(path).size;
  } catch {
    // file doesn't exist
  }
  if (size < 2 * 2024) {
    throw new Error(`size of '${path}' is ${size} which indicates we didn't build it`);
  }
}

// === Command Execution ===

async function runCaptureOutput(cmd: string, args: string[], cwd?: string): Promise<Uint8Array> {
  const short = [cmd.split("\\").pop(), ...args].join(" ");
  console.log(`> ${short}`);
  const proc = Bun.spawn([cmd, ...args], {
    stdout: "pipe",
    stderr: "pipe",
    cwd: cwd,
  });
  const out = await new Response(proc.stdout).arrayBuffer();
  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    const stderr = await new Response(proc.stderr).text();
    throw new Error(`${short} failed with exit code ${exitCode}\n${stderr}`);
  }
  return new Uint8Array(out);
}

// === GitHub Event Detection ===

function getGitHubEventType(): string {
  const v = process.env["GITHUB_EVENT_NAME"] ?? "";
  if (v !== "repository_dispatch") return "push";
  const path = process.env["GITHUB_EVENT_PATH"] ?? "";
  const d = JSON.parse(readFileSync(path, "utf-8"));
  if (d.action === "codeql") return "codeql";
  throw new Error(`invalid GitHub event action: '${d.action}'`);
}

// === S3 Upload (AWS Signature V4) ===

interface S3Config {
  bucket: string;
  endpoint: string;
  access: string;
  secret: string;
  region: string;
}

function hmacSha256(key: Buffer | string, data: string): Buffer {
  return createHmac("sha256", key).update(data).digest();
}

function sha256Hex(data: Buffer | Uint8Array | string): string {
  return createHash("sha256").update(data).digest("hex");
}

async function s3PutObject(config: S3Config, key: string, body: Uint8Array): Promise<void> {
  const now = new Date();
  const dateStamp = now.toISOString().slice(0, 10).replace(/-/g, "");
  const amzDate = dateStamp + "T" + now.toISOString().slice(11, 19).replace(/:/g, "") + "Z";

  // path-style: host is the endpoint, URI includes bucket
  const host = config.endpoint;
  const canonicalUri = "/" + config.bucket + "/" + key;
  const payloadHash = sha256Hex(body);

  const headers: Record<string, string> = {
    host: host,
    "content-length": String(body.length),
    "x-amz-content-sha256": payloadHash,
    "x-amz-date": amzDate,
  };

  const signedHeaderKeys = Object.keys(headers).sort();
  const signedHeaders = signedHeaderKeys.join(";");
  const canonicalHeaders = signedHeaderKeys.map((k) => `${k}:${headers[k]}\n`).join("");

  const canonicalRequest = ["PUT", canonicalUri, "", canonicalHeaders, signedHeaders, payloadHash].join("\n");

  const credentialScope = `${dateStamp}/${config.region}/s3/aws4_request`;
  const stringToSign = ["AWS4-HMAC-SHA256", amzDate, credentialScope, sha256Hex(canonicalRequest)].join("\n");

  let signingKey: Buffer = hmacSha256("AWS4" + config.secret, dateStamp);
  signingKey = hmacSha256(signingKey, config.region);
  signingKey = hmacSha256(signingKey, "s3");
  signingKey = hmacSha256(signingKey, "aws4_request");
  const signature = hmacSha256(signingKey, stringToSign).toString("hex");

  const authorization = `AWS4-HMAC-SHA256 Credential=${config.access}/${credentialScope}, SignedHeaders=${signedHeaders}, Signature=${signature}`;

  const url = `https://${host}${canonicalUri}`;
  const response = await fetch(url, {
    method: "PUT",
    headers: {
      ...headers,
      Authorization: authorization,
    },
    // @ts-ignore
    body: body,
  });

  if (!response.ok) {
    const text = await response.text();
    throw new Error(`S3 upload to ${url} failed: ${response.status} ${response.statusText}\n${text}`);
  }
}

function s3UrlForPath(config: S3Config, key: string): string {
  return `https://${config.endpoint}/${config.bucket}/${key}`;
}

function newR2Config(): S3Config {
  return {
    bucket: "files",
    endpoint: "71694ef61795ecbe1bc331d217dbd7a7.r2.cloudflarestorage.com",
    access: r2Access,
    secret: r2Secret,
    region: "auto",
  };
}

function newBackblazeConfig(): S3Config {
  return {
    bucket: "kjk-files",
    endpoint: "s3.us-west-001.backblazeb2.com",
    access: b2Access,
    secret: b2Secret,
    region: "us-west-001",
  };
}

// === Build Functions ===

function removeReleaseBuilds(): void {
  const dirs = [join("out", "arm64"), join("out", "rel32"), join("out", "rel64")];
  for (const dir of dirs) {
    rmSync(dir, { recursive: true, force: true });
  }
}

async function createPdbZip(dir: string): Promise<void> {
  // use PowerShell to create zip from PDB files
  const files = pdbFiles.map((f) => `'${f}'`).join(",");
  const cmd = `Compress-Archive -Path ${files} -DestinationPath 'SumatraPDF.pdb.zip' -Force`;
  await runLogged("powershell", ["-Command", cmd], dir);
}

async function createPdbLzsa(dir: string): Promise<void> {
  const makeLzsa = resolve(join("bin", "MakeLZSA.exe"));
  if (!existsSync(makeLzsa)) {
    throw new Error(`'${makeLzsa}' doesn't exist`);
  }
  await runLogged(makeLzsa, ["SumatraPDF.pdb.lzsa", ...pdbFiles], dir);
}

async function buildPreRelease(preRelVer: string, sha1: string, vsplatform: string, outDir: string): Promise<void> {
  ensureManualIsBuilt();
  console.log(`building pre-release version ${preRelVer}`);
  const buildStart = performance.now();

  setBuildConfigPreRelease(sha1, preRelVer);
  try {
    const p = `/p:Configuration=Release;Platform=${vsplatform}`;

    // build and run tests (skip for ARM64)
    await runLogged(msbuildPath, [slnPath, `/t:test_util:Rebuild`, p, `/m`]);
    if (vsplatform !== "ARM64") {
      const testUtil = resolve(join(outDir, "test_util.exe"));
      await runLogged(testUtil, [], outDir);
    }

    // build all targets
    const targets = ["PdfFilter", "PdfPreview", "SumatraPDF", "SumatraPDF-dll"];
    const t = `/t:${targets.map((t) => t + ":Rebuild").join(";")}`;
    await runLogged(msbuildPath, [slnPath, t, p, `/m`]);

    // create PDB archives
    await createPdbZip(outDir);
    await createPdbLzsa(outDir);
  } finally {
    await revertBuildConfig();
  }

  const elapsed = ((performance.now() - buildStart) / 1000).toFixed(1);
  console.log(`building pre-release version ${preRelVer} took ${elapsed}s`);
}

async function buildSmoke(): Promise<void> {
  const buildStart = performance.now();
  console.log("smoke build");

  removeReleaseBuilds();

  const { main: genDocs } = await import("./gen-docs");
  await genDocs();

  const makeLzsa = resolve(join("bin", "MakeLZSA.exe"));
  if (!existsSync(makeLzsa)) {
    throw new Error(`'${makeLzsa}' doesn't exist`);
  }

  const t = `/t:SumatraPDF-dll:Rebuild;test_util:Rebuild`;
  const p = `/p:Configuration=Release;Platform=x64`;
  await runLogged(msbuildPath, [slnPath, t, p, `/m`]);

  const outDir = join("out", "rel64");
  const testUtil = resolve(join(outDir, "test_util.exe"));
  await runLogged(testUtil, [], outDir);

  // create PDB LZSA
  await runLogged(
    makeLzsa,
    ["SumatraPDF.pdb.lzsa", "libmupdf.pdb:libmupdf.pdb", "SumatraPDF-dll.pdb:SumatraPDF-dll.pdb"],
    outDir,
  );

  const elapsed = ((performance.now() - buildStart) / 1000).toFixed(1);
  console.log(`smoke build took ${elapsed}s`);
}

// === PDB Upload ===

function formatSize(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / 1024 / 1024).toFixed(1)} MB`;
}

function fileSize(path: string): number {
  try {
    return statSync(path).size;
  } catch {
    return 0;
  }
}

async function runLlvmPdbutilGzipped(exePath: string, pdbPath: string, outPath: string, ...args: string[]) {
  const cmdArgs = ["pretty", ...args, pdbPath];
  const output = await runCaptureOutput(exePath, cmdArgs);

  if (true) {
    // @ts-ignore
    const gzipped = Bun.gzipSync(output);
    writeFileSync(outPath, gzipped);
    console.log(`wrote ${outPath} (${formatSize(output.length)}) (${formatSize(fileSize(outPath))})`);
  } else {
    outPath += ".txt";
    writeFileSync(outPath, output);
    console.log(`wrote ${outPath} (${formatSize(output.length)}) (${formatSize(fileSize(outPath))})`);
  }
}

const globalsPath = "SumatraPDF-globals.txt.gz";
const classesPath = "SumatraPDF-classes.txt.gz";

async function extractClassesAndGlobalsFromPDB(): Promise<void> {
  if (!llvmPdbutilPath) {
    console.log("extractClassesAndGlobalsFromPDB: skipping because llvmPdbutilPath is not set");
    return;
  }

  const pdbPath = join("out", "rel64", "SumatraPDF.pdb");
  if (!existsSync(pdbPath)) {
    console.log(`uploadPdbBuildArtifacts: '${pdbPath}' doesn't exist, skipping`);
    return;
  }

  await runLlvmPdbutilGzipped(llvmPdbutilPath, pdbPath, globalsPath, "-globals", "-symbol-order=size");
  await runLlvmPdbutilGzipped(llvmPdbutilPath, pdbPath, classesPath, "-classes");
}

async function uploadPdbBuildArtifacts(preRelVer: string, sha1: string): Promise<void> {
  if (!llvmPdbutilPath) {
    console.log("uploadPdbBuildArtifacts: skipping because llvmPdbutilPath is not set");
    return;
  }
  const shortSha1 = sha1.slice(0, 8);
  const dateStr = new Date().toISOString().slice(0, 10);
  const prefix = `software/sumatrapdf-build-artifacts/${dateStr}-${preRelVer}-${shortSha1}`;
  const remoteGlobals = prefix + ".SumatraPDF-globals.txt.gz";
  const remoteClasses = prefix + ".SumatraPDF-classes.txt.gz";

  await extractClassesAndGlobalsFromPDB();

  const globalsData = new Uint8Array(readFileSync(globalsPath));
  const classesData = new Uint8Array(readFileSync(classesPath));

  // upload to R2 and Backblaze in parallel
  const uploadToProvider = async (config: S3Config, name: string) => {
    await s3PutObject(config, remoteGlobals, globalsData);
    console.log(`uploaded ${s3UrlForPath(config, remoteGlobals)}`);
    await s3PutObject(config, remoteClasses, classesData);
    console.log(`uploaded ${s3UrlForPath(config, remoteClasses)}`);
  };

  await Promise.all([uploadToProvider(newR2Config(), "R2"), uploadToProvider(newBackblazeConfig(), "Backblaze")]);
}

// === Main ===

async function main() {
  const timeStart = performance.now();
  console.log(`Current directory: ${resolve(".")}`);

  getSecrets();

  const preRelVer = String(await getGitLinearVersion());
  const sha1 = await getGitSha1();
  const sumatraVer = extractSumatraVersion();
  console.log(`preReleaseVer: '${preRelVer}'`);
  console.log(`gitSha1: '${sha1}'`);
  console.log(`sumatraVersion: '${sumatraVer}'`);

  const eventType = getGitHubEventType();
  console.log(`GitHub event type: ${eventType}`);

  switch (eventType) {
    case "push":
      removeReleaseBuilds();
      // generate HTML docs
      {
        const { main: genDocs } = await import("./gen-docs");
        await genDocs();
      }
      await buildPreRelease(preRelVer, sha1, "x64", join("out", "rel64"));
      break;
    case "codeql":
      await buildSmoke();
      break;
    default:
      throw new Error(`unknown GitHub event type: '${eventType}'`);
  }

  //ensureAllUploadCreds();
  //await uploadPdbBuildArtifacts(preRelVer, sha1);

  const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
  console.log(`Finished in ${elapsed}s`);
}

if (import.meta.main) {
  await main();
}
