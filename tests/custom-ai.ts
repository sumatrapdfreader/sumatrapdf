import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath, writeAppdata } from "./util.ts";

// AIChatBackend::CustomAPI in src/shared/AIChatTypes.h
const kCustomAPIBackend = 4;

export async function testit(): Promise<void> {
  const secret = "local-test-key-not-for-a-real-provider";
  // the server runs in a worker: see tests/custom-ai-server.ts
  const worker = new Worker(new URL("./custom-ai-server.ts", import.meta.url).href);
  const message = () =>
    new Promise<{ port?: number; unexpected?: number }>((resolve) => (worker.onmessage = (e) => resolve(e.data)));
  const started = message();
  worker.postMessage({ secret });
  const server = { port: (await started).port! };
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
    worker.postMessage("stop");
    const { unexpected } = await message();
    worker.terminate();
    if (unexpected !== 0) throw new Error("Authenticated request followed a redirect");
  }
}

if (import.meta.main) await runStandalone(testit);
