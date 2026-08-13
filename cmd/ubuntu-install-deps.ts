/**
 * Install Ubuntu/Debian packages needed to build SumatraPDF on Linux
 * (`bun cmd/build-linux.ts`).
 *
 * Usage:
 *   sudo bun cmd/ubuntu-install-deps.ts
 *
 * Runs `apt-get update` then a non-interactive `apt-get install`.
 */

const requiredPackages = [
  "build-essential", // gcc, g++, make, binutils (ar, objcopy)
  "clang", // preferred compiler for build-linux.ts
  "libssl-dev", // openssl headers + libcrypto.so (mupdf pkcs7)
  "xxd", // font embedding on Linux arm64
];

function requireLinux(): void {
  if (process.platform !== "linux") {
    console.error(`This script must be run on Ubuntu/Debian (got ${process.platform})`);
    process.exit(1);
  }
}

function requireRoot(): void {
  if (typeof process.getuid !== "function" || process.getuid() !== 0) {
    console.error("This script must be run as root, e.g.:");
    console.error("  sudo bun cmd/ubuntu-install-deps.ts");
    process.exit(1);
  }
}

function requireAptGet(): void {
  if (!Bun.which("apt-get")) {
    console.error("apt-get not found. This script only supports Ubuntu/Debian.");
    process.exit(1);
  }
}

async function runApt(args: string[]): Promise<void> {
  const cmd = ["apt-get", ...args];
  console.log(`> ${cmd.join(" ")}`);
  const proc = Bun.spawn(cmd, {
    stdout: "inherit",
    stderr: "inherit",
    stdin: "inherit",
    env: {
      ...process.env,
      DEBIAN_FRONTEND: "noninteractive",
    },
  });
  const code = await proc.exited;
  if (code !== 0) {
    throw new Error(`apt-get ${args[0]} failed with exit code ${code}`);
  }
}

function aptShow(packageName: string): string | null {
  const p = Bun.spawnSync(["apt-cache", "show", packageName], {
    stdout: "pipe",
    stderr: "pipe",
  });
  if (p.exitCode !== 0) {
    return null;
  }
  return p.stdout.toString();
}

function packageExists(packageName: string): boolean {
  return aptShow(packageName) !== null;
}

// Ubuntu's clang package does not include compiler-rt; match the default clang-N.
function clangRtPackage(): string | null {
  const show = aptShow("clang");
  if (show) {
    const m = show.match(/^Depends:.*\bclang-(\d+)\b/m);
    if (m) {
      const pkg = `libclang-rt-${m[1]}-dev`;
      if (packageExists(pkg)) {
        return pkg;
      }
    }
  }
  if (packageExists("libclang-rt-dev")) {
    return "libclang-rt-dev";
  }
  return null;
}

async function main(): Promise<void> {
  requireLinux();
  requireRoot();
  requireAptGet();

  await runApt(["update"]);

  const packages = [...requiredPackages];
  if (packageExists("libasan8")) {
    packages.push("libasan8");
  }
  const clangRt = clangRtPackage();
  if (clangRt) {
    packages.push(clangRt);
  }

  await runApt(["install", "-y", "--no-install-recommends", ...packages]);
  console.log("\nInstalled build dependencies:");
  for (const pkg of packages) {
    console.log(`  ${pkg}`);
  }
}

if (import.meta.main) {
  main().catch((e: unknown) => {
    const msg = e instanceof Error ? e.message : String(e);
    console.error(`\nFailed to install build dependencies: ${msg}`);
    process.exit(1);
  });
}
