// HttpGetWithHeaders / HttpPostUrlNoRedirect / HttpDownloadWithHeaders
// (WinHTTP on Windows, libcurl on Linux/macOS) against a local server. The C++
// side is HttpTransferTest() in src/shared/tests/SumatraCommon_ut.cpp; it runs
// inside ng's test_util when SUMATRA_TEST_HTTP_SERVER is set.
//
// Run: bun tests/ad-hoc-http-transfer.ts [-linux]
// -linux on Windows re-runs this script inside WSL (bun in ~/.bun/bin), so the
// server and test_util share one localhost.

import { IS_WIN } from "./host.ts";
import { ROOT } from "./util.ts";

const kPort = 9332;
const kFileSize = 100000;
const kBigSize = 60 << 20;

function startServer() {
  return Bun.serve({
    port: kPort,
    hostname: "127.0.0.1",
    async fetch(req) {
      const url = new URL(req.url);
      const auth = req.headers.get("authorization") ?? "";
      if (url.pathname === "/redirect") {
        return new Response("moved", { status: 302, headers: { location: "/api/files" } });
      }
      if (url.pathname === "/big") {
        // declared size over the 50 MiB cap; the client must give up
        const chunk = new Uint8Array(1 << 20);
        let sent = 0;
        const body = new ReadableStream({
          pull(ctl) {
            if (sent >= kBigSize) {
              ctl.close();
              return;
            }
            ctl.enqueue(chunk);
            sent += chunk.length;
          },
        });
        return new Response(body, { headers: { "content-length": String(kBigSize) } });
      }
      if (url.pathname === "/truncated") {
        // promises more than it sends, then drops the connection
        const body = new ReadableStream({
          start(ctl) {
            ctl.enqueue(new Uint8Array(1000).fill(66));
          },
          async pull(ctl) {
            // let the status line and first chunk go out first
            await Bun.sleep(300);
            ctl.error(new Error("truncated"));
          },
        });
        return new Response(body, { headers: { "content-length": "100000" } });
      }
      if (url.pathname === "/public") {
        // no auth: what HttpGet / HttpGetToFile fetch
        return new Response(new Uint8Array(kFileSize).fill(67));
      }
      if (url.pathname === "/public-redirect") {
        return new Response("moved", { status: 302, headers: { location: "/public" } });
      }
      if (url.pathname === "/stream" && req.method === "POST") {
        // a streamed reply in three pieces (HttpPostStream)
        const enc = new TextEncoder();
        const parts = ["a", "b", "c"];
        const body = new ReadableStream({
          async pull(ctl) {
            const part = parts.shift();
            if (part === undefined) {
              ctl.close();
              return;
            }
            await Bun.sleep(50);
            ctl.enqueue(enc.encode(part));
          },
        });
        return new Response(body);
      }
      if (url.pathname === "/slowstream" && req.method === "POST") {
        // one piece, then nothing for a long time: only a cancel ends it early
        let sentFirst = false;
        const body = new ReadableStream({
          async pull(ctl) {
            if (!sentFirst) {
              sentFirst = true;
              ctl.enqueue(new TextEncoder().encode("x"));
              return;
            }
            await Bun.sleep(20000);
            ctl.close();
          },
        });
        return new Response(body);
      }
      if (auth !== "Bearer tk-123") {
        return new Response("unauthorized", { status: 401 });
      }
      if (url.pathname === "/api/files") {
        return Response.json({ files: [{ name: "a.pdf", size: 1 }] });
      }
      if (url.pathname === "/file") {
        return new Response(new Uint8Array(kFileSize).fill(65));
      }
      if (url.pathname === "/upload" && req.method === "POST") {
        const body = await req.arrayBuffer();
        return new Response(
          JSON.stringify({
            len: body.byteLength,
            ct: req.headers.get("content-type"),
            auth,
          }),
          { headers: { "content-type": "application/json" } },
        );
      }
      return new Response("not found", { status: 404 });
    },
  });
}

async function runInWsl(): Promise<void> {
  const wslRoot = ROOT.replace(/^([A-Za-z]):/, (_, d) => `/mnt/${d.toLowerCase()}`).replaceAll("\\", "/");
  const cmd = `cd '${wslRoot}' && ~/.bun/bin/bun tests/ad-hoc-http-transfer.ts`;
  const proc = Bun.spawn(["wsl", "bash", "-lc", cmd], { stdout: "inherit", stderr: "inherit" });
  if ((await proc.exited) !== 0) {
    throw new Error("ad-hoc-http-transfer: WSL run failed");
  }
}

export async function testit(): Promise<void> {
  if (process.argv.includes("-linux") && IS_WIN) {
    await runInWsl();
    return;
  }
  const server = startServer();
  try {
    const args = [process.execPath, "cmd/ng-build.ts"];
    args.push("-dbg", "test_util", "-run", "--", "-for-ai");
    const proc = Bun.spawn(args, {
      cwd: ROOT,
      env: { ...process.env, SUMATRA_TEST_HTTP_SERVER: "1" },
      stdout: "inherit",
      stderr: "inherit",
    });
    const code = await proc.exited;
    if (code !== 0) {
      throw new Error(`ad-hoc-http-transfer: test_util exited with ${code}`);
    }
  } finally {
    server.stop(true);
  }
}

if (import.meta.main) {
  await testit();
}
