// The control pipe serves at most 8 clients at once. With 8 connected, the
// listener's next CreateNamedPipeW failed with ERROR_PIPE_BUSY and the listener
// thread exited, so once a client disconnected no new client could connect.
// The test fills every slot, frees one and expects a new client to get in.
//
// Windows only: elsewhere the channel is a unix socket.
//
// Run: bun tests/control-pipe-busy.ts [-ng] [--no-build]
import { ControlClient, ControlCommand, uniquePipeName } from "./control.ts";
import { IS_WIN } from "./host.ts";
import { EXE, runStandalone } from "./util.ts";
import { killAndWait } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const kMaxConns = 8;

export async function testit(): Promise<void> {
  if (!IS_WIN) {
    console.log("control-pipe-busy: skipped, Windows only");
    return;
  }
  const pipe = uniquePipeName();
  const proc = Bun.spawn([EXE, "-for-testing", "-dbg-control", pipe], { stdout: "ignore", stderr: "ignore" });
  const clients: ControlClient[] = [];
  try {
    for (let i = 0; i < kMaxConns; i++) {
      const c = await ControlClient.connect(pipe);
      await c.request(ControlCommand.Ping);
      clients.push(c);
    }
    // let the listener try (and fail) to create a 9th instance
    await sleep(300);

    clients.pop()!.close();
    let late: ControlClient;
    try {
      late = await ControlClient.connect(pipe, 5000);
    } catch (e) {
      throw new Error(`no client could connect after one of ${kMaxConns} disconnected: ${e}`);
    }
    clients.push(late);
    await late.request(ControlCommand.Ping);
    await late.quit();
    clients.pop();
    const code = await Promise.race([proc.exited, sleep(10_000).then(() => "timeout")]);
    if (code === "timeout") {
      throw new Error("SumatraPDF did not exit after Quit");
    }
  } finally {
    for (const c of clients) {
      c.close();
    }
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
