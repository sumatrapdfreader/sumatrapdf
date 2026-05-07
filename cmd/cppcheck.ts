import { $ } from "bun";
import { existsSync } from "node:fs";
import { unlink } from "node:fs/promises";

const cppcheckLogFile = "cppcheck.out.txt";

function detectCppcheckExe(): string {
  const path = String.raw`c:\Program Files\Cppcheck\cppcheck.exe`;
  if (existsSync(path)) {
    return path;
  }
  return "cppcheck.exe";
}

async function runCppCheck(all: boolean): Promise<void> {
  const args: string[] = [
    "--platform=win64",
    "-DWIN32",
    "-D_WIN32",
    "-D_MSC_VER=1800",
    "-D_M_X64",
    "-DIFACEMETHODIMP_(x)=x",
    "-DSTDMETHODIMP_(x)=x",
    "-DSTDAPI_(x)=x",
    "-DPRE_RELEASE_VER=3.4",
    "-v",
  ];

  if (all) {
    args.push(
      "--enable=style",
      "--suppress=constParameter",
      "--suppress=cstyleCast",
      "--suppress=useStlAlgorithm",
      "--suppress=noExplicitConstructor",
      "--suppress=variableScope",
      "--suppress=memsetClassFloat",
      "--suppress=ignoredReturnValue",
      "--suppress=invalidPointerCast",
      "--suppress=AssignmentIntegerToAddress",
      "--suppress=knownConditionTrueFalse",
      "--suppress=constParameterPointer",
      "--suppress=constVariablePointer",
      "--suppress=constVariableReference",
      "--suppress=constParameterReference",
      "--suppress=useInitializationList",
      "--suppress=duplInheritedMember",
      "--suppress=unusedStructMember",
      "--suppress=CastIntegerToAddressAtReturn",
      "--suppress=dangerousTypeCast",
      "--suppress=uselessOverride",
    );
  }

  args.push("--check-level=exhaustive", "--inline-suppr", "-I", "src", "-I", "src/utils", "src");

  const cppcheckExe = detectCppcheckExe();
  console.log(`using '${cppcheckExe}'`);

  await unlink(cppcheckLogFile).catch(() => {});

  const proc = Bun.spawn([cppcheckExe, ...args], {
    stdout: "pipe",
    stderr: "pipe",
  });

  const logFile = Bun.file(cppcheckLogFile);
  const writer = logFile.writer();

  async function pipeStream(stream: ReadableStream<Uint8Array>, out: { write(chunk: Uint8Array): void }) {
    const reader = stream.getReader();
    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      out.write(value);
      writer.write(value);
    }
  }

  await Promise.all([
    pipeStream(proc.stdout, process.stdout),
    pipeStream(proc.stderr, process.stderr),
  ]);

  writer.end();
  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    console.error(`cppcheck exited with code ${exitCode}`);
    process.exit(exitCode);
  }

  console.log(`\nLogged output to '${cppcheckLogFile}'`);
}

const args = process.argv.slice(2);
const all = args.includes("-all");
await runCppCheck(all);
