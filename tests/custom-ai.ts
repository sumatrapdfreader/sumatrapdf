import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath, writeAppdata } from "./util.ts";

// AIChatBackend::CustomAPI in src/shared/AIChatTypes.h
const kCustomAPIBackend = 4;

export async function testit(): Promise<void> {
  const secret = "local-test-key-not-for-a-real-provider";
  let unexpected = 0;
  const server = Bun.serve({
    hostname: "127.0.0.1",
    port: 0,
    async fetch(request) {
      if (new URL(request.url).pathname !== "/v1/chat/completions") {
        unexpected++;
        return new Response("unexpected redirect", { status: 500 });
      }
      if (request.headers.get("authorization") !== `Bearer ${secret}`) {
        return new Response("missing authentication", { status: 403 });
      }
      const body = (await request.json()) as {
        model: string;
        stream: boolean;
        messages: { role: string; content: string }[];
      };
      if (body.model !== "fixture-model" || body.stream !== true) return new Response("bad payload", { status: 400 });
      const prompt = body.messages.at(-1)?.content;
      if (prompt === "unauthorized")
        return Response.json({ error: { message: `invalid key ${secret}` } }, { status: 401 });
      if (prompt === "redirect") return new Response(null, { status: 302, headers: { location: "/unexpected" } });
      if (prompt === "stream-error")
        return new Response('data: {"error":{"message":"model is unavailable"}}\n\n', {
          headers: { "content-type": "text/event-stream" },
        });
      if (prompt === "malformed") return new Response("data: invalid-json\n\n");
      if (prompt === "empty") return new Response("data: [DONE]\n\n");
      const frames =
        ": keepalive\r\n\r\n" +
        'data: {"choices":[{"index":0,"delta":{"role":"assistant"}}]}\r\n\r\n' +
        'data: {"choices":[{"index":0,"delta":{"content":"你好 world"}}]}\r\n\r\n' +
        'data: {"choices":[{"index":0,"delta":{},"finish_reason":"stop"}]}\n\n' +
        (prompt === "truncated" ? "" : "data: [DONE]\n\n");
      const bytes = new TextEncoder().encode(prompt === "cr-only" ? frames.replace(/\r?\n/g, "\r") : frames);
      let canceled = false;
      return new Response(
        new ReadableStream({
          async start(controller) {
            for (let i = 0; i < bytes.length; i += 5) {
              if (canceled) return;
              controller.enqueue(bytes.slice(i, i + 5));
              await Bun.sleep(1);
            }
            if (!canceled) controller.close();
          },
          cancel() {
            canceled = true;
          },
        }),
        { headers: { "content-type": "text/event-stream" } },
      );
    },
  });
  const appdata = writeAppdata(
    "custom-ai",
    `CustomAI [\n BaseURL = http://127.0.0.1:${server.port}/v1\n ApiKeyEnv = SUMATRA_CUSTOM_AI_TEST_KEY\n Model = fixture-model\n]\n`,
  );
  try {
    await withControlledSumatra(
      EXE,
      async (client) => {
        const ok = await client.request(ControlCommand.TestAIChat, [
          kCustomAPIBackend,
          tmpPath("document.pdf"),
          'hello "quoted"\ntext',
        ]);
        if (ok[0] !== 0 || !String(ok[1]).includes("你好 world")) throw new Error(`stream: ${ok}`);
        const cr = await client.request(ControlCommand.TestAIChat, [
          kCustomAPIBackend,
          tmpPath("document.pdf"),
          "cr-only",
        ]);
        if (cr[0] !== 0 || !String(cr[1]).includes("你好 world")) throw new Error(`CR stream: ${cr}`);
        for (const [prompt, expected] of [
          ["unauthorized", "HTTP 401"],
          ["redirect", "HTTP 302"],
          ["stream-error", "model is unavailable"],
          ["truncated", "incomplete"],
          ["malformed", "invalid streaming event"],
          ["empty", "no assistant text"],
        ]) {
          const result = await client.request(ControlCommand.TestAIChat, [
            kCustomAPIBackend,
            tmpPath("document.pdf"),
            prompt,
          ]);
          if (result[0] === 0 || !String(result[1]).startsWith("FAIL:")) throw new Error(`${prompt}: ${result}`);
          if (!String(result[1]).includes(expected)) throw new Error(`${prompt}: missing error detail: ${result[1]}`);
          if (String(result[1]).includes(secret)) throw new Error("API key leaked into test output");
        }
        if (unexpected !== 0) throw new Error("Authenticated request followed a redirect");
      },
      ["-appdata", appdata],
      { env: { ...process.env, SUMATRA_CUSTOM_AI_TEST_KEY: secret } },
    );
    const directKey = writeAppdata(
      "custom-ai-direct-key",
      `CustomAI [\n BaseURL = http://127.0.0.1:${server.port}/v1\n ApiKeyEnv = SUMATRA_CUSTOM_AI_TEST_KEY\n ApiKey = ${secret}\n Model = fixture-model\n]\n`,
    );
    await withControlledSumatra(
      EXE,
      async (client) => {
        const result = await client.request(ControlCommand.TestAIChat, [
          kCustomAPIBackend,
          tmpPath("document.pdf"),
          "direct key",
        ]);
        if (result[0] !== 0) throw new Error(`Configured API key was not used: ${result[1]}`);
      },
      ["-appdata", directKey],
      { env: { ...process.env, SUMATRA_CUSTOM_AI_TEST_KEY: "" } },
    );
  } finally {
    server.stop(true);
  }
}

if (import.meta.main) await runStandalone(testit);
