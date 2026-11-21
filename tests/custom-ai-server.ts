// The OpenAI-style server tests/custom-ai.ts talks to. It runs in a worker:
// on macOS a control request blocks the test's thread until the app replies,
// and TestAIChat replies only after this server has answered.

declare const self: Worker;

let secret = "";
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

self.onmessage = (e: MessageEvent) => {
  if (e.data === "stop") {
    server.stop(true);
    postMessage({ unexpected });
    return;
  }
  secret = e.data.secret;
  postMessage({ port: server.port });
};
