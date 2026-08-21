// Exercises AI chat end-to-end through -dbg-control, using the same provider
// code the app's chat panel does. Pick a backend, a document and a message; the
// app builds the provider command line, runs the CLI, parses the streamed
// response and returns the assistant text (or "FAIL: <reason>" plus a dump of
// the remembered sent/received traffic).
//
// Requires the chosen backend's CLI installed and authenticated, and network
// access, so it isn't part of the default CI suite - run it directly:
//   bun tests/ai-chat.ts --no-build grok "C:\\path\\doc.pdf" "summarize page 1"
import { ControlCommand, withControlledSumatra } from "./control";
import { EXE, runStandalone } from "./util";

// AIChatBackend enum order in src/AIChatCommon.h
const BACKENDS: Record<string, number> = { claude: 0, grok: 1, codex: 2, antigravity: 3 };

function parseArgs(): { backend: number; backendName: string; file: string; message: string } {
  const args = process.argv.slice(2).filter((a) => a !== "--no-build");
  const backendName = (args[0] ?? "grok").toLowerCase();
  const file = args[1] ?? "C:\\Users\\kjk\\Downloads\\a_players_guide_to_talislanta.1.pdf";
  const message = args[2] ?? "summarize page 1";
  const backend = BACKENDS[backendName];
  if (backend === undefined) {
    throw new Error(`unknown backend "${backendName}", expected one of: ${Object.keys(BACKENDS).join(", ")}`);
  }
  return { backend, backendName, file, message };
}

export async function testit(): Promise<void> {
  const { backend, backendName, file, message } = parseArgs();
  console.log(`ai-chat: backend=${backendName} file=${file}\n  message: ${message}`);

  await withControlledSumatra(EXE, async (client) => {
    const res = await client.request(ControlCommand.TestAIChat, [backend, file, message]);
    const exitCode = Number(res[0] ?? -1);
    const text = String(res[1] ?? "");
    console.log(`\n--- result (exitCode=${exitCode}) ---\n${text}`);
    if (exitCode !== 0) {
      throw new Error(`ai-chat ${backendName} failed`);
    }
    if (text.length < 10) {
      throw new Error(`ai-chat ${backendName} returned suspiciously short response`);
    }
    console.log(`\nai-chat: OK (${backendName})`);
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
