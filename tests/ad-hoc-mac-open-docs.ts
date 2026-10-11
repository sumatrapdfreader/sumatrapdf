import assert from "node:assert/strict";
import { copyFileSync, mkdtempSync, readFileSync, realpathSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import type { Socket } from "node:net";
import { makeMacBundle } from "../cmd/helper/ng-mac-bundle";
import { connect, controlCommands as C, sendCommand } from "../cmd/ng-dbg-control";
import { EXE, ROOT, makePdf, runStandalone } from "./util";

function run(args: string[]): void {
  const result = Bun.spawnSync(args, { stdout: "pipe", stderr: "pipe" });
  assert.equal(result.exitCode, 0, result.stderr.toString());
}

async function currentDoc(socket: Socket, document: string, page = 1): Promise<void> {
  const deadline = Date.now() + 5000;
  let state = "";
  do {
    const reply = await sendCommand(socket, C.TestCurrentTab!, []);
    state = String(reply[1]);
    if (reply[0] === 0 && state === `path=${document} page=${page}`) return;
    await Bun.sleep(25);
  } while (Date.now() < deadline);
  assert.fail(`Apple-event document was not opened: ${state}`);
}

export async function testit(): Promise<void> {
  if (process.platform !== "darwin") return;
  const dir = realpathSync(mkdtempSync(join(tmpdir(), "sumatra-open-docs-")));
  const exe = join(dir, "SumatraPDF");
  copyFileSync(EXE, exe);
  const app = makeMacBundle(ROOT, exe, "1.0");
  const plist = join(app, "Contents", "Info.plist");
  writeFileSync(
    plist,
    readFileSync(plist, "utf8").replace(
      "org.sumatrapdfreader.SumatraPDF",
      `org.sumatrapdfreader.open-docs-test.${process.pid}`,
    ),
  );
  run(["codesign", "--force", "--sign", "-", app]);
  const document = join(dir, "中文 文档 #1.pdf");
  copyFileSync(join(ROOT, "ext", "a-zlib", "zlib.3.pdf"), document);
  try {
    for (const mode of ["running", "startup", "rewrite", "reopen"]) {
      writeFileSync(
        join(dir, "SumatraPDF-settings.txt"),
        `RestoreSession = false\nCheckForUpdates = false\nReloadModifiedDocuments = ${mode !== "reopen"}\n`,
      );
      const pipe = `open-docs-${process.pid}-${mode}`;
      let socket: Socket | undefined;
      try {
        run([
          "open",
          "-n",
          "-a",
          app,
          ...(mode === "startup" ? [document] : []),
          "--args",
          "-for-testing",
          "-appdata",
          dir,
          "-dbg-control",
          pipe,
        ]);
        socket = await connect(pipe, 15000);
        if (mode !== "startup") run(["open", "-a", app, document]);
        await currentDoc(socket, document);
        if (mode === "rewrite" || mode === "reopen") {
          writeFileSync(document, "");
          if (mode === "reopen") await sendCommand(socket, C.TestInvokeCommand!, ["CmdReloadDocument"]);
          await currentDoc(socket, document, 0);
          writeFileSync(document, makePdf(2, 612, 0, 0));
          if (mode === "reopen") run(["open", "-a", app, document]);
          await currentDoc(socket, document);
        }
        console.log(`mac-open-docs: OK (${mode})`);
      } finally {
        if (socket) {
          await sendCommand(socket, C.Quit!, []);
          socket.destroy();
        }
      }
    }
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
