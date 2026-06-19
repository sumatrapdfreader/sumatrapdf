// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/906
//
// SumatraPDF should accept a document password from the command line via
// `-pwd <password>`. The test creates an encrypted copy of an existing text
// search fixture with the bundled `clean` tool, then checks that the control search command
// can only load/search it when the password is provided.
//
// Run:  bun tests/issue-906.ts [--no-build]   (or via tests/all.ts)

import { existsSync, rmSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { EXE, runStandalone } from "./util.ts";
import { ControlCommand, runControlCommand } from "../cmd/control.ts";

const SRC_PDF = join(import.meta.dir, "issue-5597.pdf");
const PASSWORD = "issue-906-password";

async function runSearch(pdf: string, password: string | null): Promise<string> {
  const extraArgs = password ? ["-pwd", password] : [];
  const [, rawArg] = await runControlCommand(EXE, ControlCommand.TestSearch, [pdf, "Ankara"], extraArgs);
  return String(rawArg).trim();
}

export async function testit(): Promise<void> {
  if (!existsSync(EXE)) {
    throw new Error(`app not found: ${EXE} (build first)`);
  }
  if (!existsSync(SRC_PDF)) {
    throw new Error(`test pdf not found: ${SRC_PDF}`);
  }

  const encryptedPdf = join(tmpdir(), "sumatra-issue-906-encrypted.pdf");
  rmSync(encryptedPdf, { force: true });

  // Command-line tools must run through cmd.exe when invoked from PowerShell
  // with redirected output; see SumatraStartup.cpp's PowerShell pipe guard.
  const enc = Bun.spawnSync({
    cmd: ["cmd.exe", "/c", EXE, "clean", "-E", "aes-256", "-U", PASSWORD, "-O", PASSWORD, SRC_PDF, encryptedPdf],
    stdout: "pipe",
    stderr: "pipe",
  });
  if (enc.exitCode !== 0 || !existsSync(encryptedPdf)) {
    throw new Error(`failed to create encrypted pdf: exit=${enc.exitCode} stdout=${enc.stdout} stderr=${enc.stderr}`);
  }

  const withoutPwd = await runSearch(encryptedPdf, null);
  if (!withoutPwd.startsWith("ERROR engine-create-failed")) {
    throw new Error(`encrypted pdf unexpectedly opened without -pwd: ${withoutPwd}`);
  }

  const withPwd = await runSearch(encryptedPdf, PASSWORD);
  if (!withPwd.match(/^FOUND .*page=1$/)) {
    throw new Error(`encrypted pdf did not open/search with -pwd: ${withPwd}`);
  }

  rmSync(encryptedPdf, { force: true });
}

if (import.meta.main) {
  await runStandalone(testit);
}
