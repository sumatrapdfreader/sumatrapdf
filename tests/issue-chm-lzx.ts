// Test for libchm LZX make_decode_table PRETREE overflow (CC-0010 class advisory).
//
// Builds SumatraPDF.exe with ASan (cmd/build-asan.ts), generates a minimal
// malicious CHM (issue-chm-lzx-make.ts), and runs the control pipe CHM test command.
// With the fix, the isolated pretree check rejects malformed input and the
// process exits 0. Without the fix, ASan aborts on the isolated heap buffer test.
//
// Run:  bun tests/issue-chm-lzx.ts [--no-build]

import { existsSync } from "node:fs";
import { join } from "node:path";
import { ROOT, runStandalone } from "./util.ts";
import { ControlCommand, withControlledSumatra } from "../cmd/control.ts";

const ASAN_EXE = join(ROOT, "out", "dbg64_asan", "SumatraPDF.exe");
const CHM = join(import.meta.dir, "issue-chm-lzx.chm");
const MAKE = join(import.meta.dir, "issue-chm-lzx-make.ts");

function buildAsanApp(): void {
  console.log("• building SumatraPDF.exe with ASan (cmd/build-asan.ts) ...");
  const p = Bun.spawnSync({ cmd: ["bun", join(ROOT, "cmd", "build-asan.ts")], cwd: ROOT, stdout: "inherit", stderr: "inherit" });
  if (p.exitCode !== 0) {
    throw new Error("ASan build failed");
  }
}

async function runTestChm(chmPath: string): Promise<{ code: number; out: string }> {
  return await withControlledSumatra(
    ASAN_EXE,
    async (client) => {
      const [code, out] = await client.request(ControlCommand.TestChm, [chmPath]);
      return { code: Number(code), out: String(out) };
    },
    [],
    {
      cwd: ROOT,
      env: { ...process.env, ASAN_OPTIONS: "abort_on_error=1:halt_on_error=1:detect_leaks=0" },
      connectTimeoutMs: 30000,
    },
  );
}

export async function testit(): Promise<void> {
  if (!existsSync(ASAN_EXE)) {
    buildAsanApp();
  }
  if (!existsSync(ASAN_EXE)) {
    throw new Error(`ASan exe not found: ${ASAN_EXE}`);
  }

  const p = Bun.spawnSync({ cmd: ["bun", MAKE], cwd: ROOT, stdout: "inherit", stderr: "inherit" });
  if (p.exitCode !== 0) {
    throw new Error("failed to generate issue-chm-lzx.chm");
  }
  if (!existsSync(CHM)) {
    throw new Error(`missing fixture: ${CHM}`);
  }

  const res = await runTestChm(CHM);
  console.log(res.out.trim());

  if (res.code !== 0) {
    throw new Error(`control TestChm exited with ${res.code} (expected 0; ASan abort means unfixed lzx.c)`);
  }
  if (!res.out.includes("pretree_isolated=REJECTED")) {
    throw new Error("expected pretree_isolated=REJECTED in output");
  }
  if (!res.out.includes("result=OK")) {
    throw new Error("expected result=OK in output");
  }
  if (!res.out.includes("chm_open=OK")) {
    throw new Error("expected chm_open=OK - evil CHM fixture did not open");
  }

  console.log("✅ CHM LZX pretree overflow rejected safely under ASan");
}

if (import.meta.main) {
  await runStandalone(async () => {
    if (!process.argv.includes("--no-build")) {
      buildAsanApp();
    }
    await testit();
  });
}
