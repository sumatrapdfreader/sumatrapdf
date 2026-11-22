// `SumatraPDF cli sign` and `cli verify` (src/shared/Cli.cpp,
// EngineMupdfSignDocument, EngineMupdfVerifySignatures): a PDF signed with a
// self-signed certificate verifies as intact but untrusted (exit 1), on the
// page it was put on; a byte changed in the signed range makes it invalid; an
// unsigned PDF has no signatures (exit 1); a wrong key password or a missing
// key file is refused.
//
// Needs openssl to make the certificate.
// Run: bun tests/cli-sign.ts [--no-build]

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, ROOT, runStandalone, tmpPath } from "./util.ts";

const name = "cli-sign";

type Res = { code: number; out: string; err: string };

function cli(args: string[], env: Record<string, string> = {}): Res {
  const p = Bun.spawnSync([EXE, "cli", ...args], {
    stdout: "pipe",
    stderr: "pipe",
    env: { ...process.env, ...env },
  });
  return { code: p.exitCode ?? -1, out: p.stdout.toString(), err: p.stderr.toString() };
}

function expect(what: string, ok: boolean, res: Res | string): void {
  if (!ok) {
    const s = typeof res === "string" ? res : `exit ${res.code}\nstdout: ${res.out}\nstderr: ${res.err}`;
    throw new Error(`${name}: ${what}:\n${s}`);
  }
}

// a self-signed certificate and its key in a .pfx with password "pw"
function makePfx(dir: string): string {
  const run = (args: string[]) => {
    const p = Bun.spawnSync(["openssl", ...args], { cwd: dir, stdout: "pipe", stderr: "pipe" });
    expect(`openssl ${args[0]}`, p.exitCode === 0, p.stderr.toString());
  };
  run([
    "req",
    "-x509",
    "-newkey",
    "rsa:2048",
    "-keyout",
    "key.pem",
    "-out",
    "cert.pem",
    "-days",
    "3650",
    "-nodes",
    "-subj",
    "/CN=SumatraPDF Test Signer",
  ]);
  run(["pkcs12", "-export", "-out", "me.pfx", "-inkey", "key.pem", "-in", "cert.pem", "-passout", "pass:pw"]);
  return join(dir, "me.pfx");
}

export async function testit(): Promise<void> {
  if (!Bun.which("openssl")) {
    console.log(`SKIP ${name}: needs openssl to make a certificate`);
    return;
  }
  const dir = tmpPath(name);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pfx = makePfx(dir);
  const src = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const signed = join(dir, "signed.pdf");

  let r = cli(
    [
      "sign",
      src,
      "--key",
      pfx,
      "--key-password-env",
      "SIGN_PW",
      "--pages",
      "2",
      "--rect",
      "72,600,200,60",
      "-o",
      signed,
    ],
    { SIGN_PW: "pw" },
  );
  expect("sign", r.code === 0, r);

  r = cli(["verify", signed, "--json"]);
  const v = JSON.parse(r.out);
  const sig = v.signatures[0];
  expect(`verify ${r.out}`, v.count === 1 && sig.page === 2 && sig.signer.includes("SumatraPDF Test Signer"), r);
  expect("the signed bytes are intact", sig.digestOk === true && sig.verdict === "untrusted", r);
  expect("a self-signed signer isn't trusted: exit 1", r.code === 1 && v.valid === false, r);

  // a byte of the header's binary comment: the file still opens, the digest doesn't hold
  const bytes = readFileSync(signed);
  const at = bytes.indexOf("%", 1);
  bytes[at + 1] = bytes[at + 1]! ^ 1;
  const tampered = join(dir, "tampered.pdf");
  writeFileSync(tampered, bytes);
  r = cli(["verify", tampered, "--json"]);
  expect(`tampered ${r.out}`, JSON.parse(r.out).signatures[0]?.verdict === "invalid", r);

  r = cli(["verify", src]);
  expect("an unsigned PDF: no signatures, exit 1", r.code === 1 && r.out.includes("no signatures"), r);
  r = cli(["sign", src, "--key", pfx, "--key-password", "wrong", "-o", join(dir, "x.pdf")]);
  expect("a wrong key password fails", r.code === 1, r);
  r = cli(["sign", src, "--key", join(dir, "nope.pfx"), "-o", join(dir, "x.pdf")]);
  expect("a missing key is 3", r.code === 3, r);
  r = cli(["sign", src, "-o", join(dir, "x.pdf")]);
  expect("no --key is 2", r.code === 2, r);
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
