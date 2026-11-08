import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, ROOT, USE_NG, cmdId, pollUntil, runStandalone, writeAppdata } from "./util.ts";
import { FRAME_CLASS, sendCommandSync } from "./win-automation.ts";
import {
  BM_SETCHECK,
  BST_CHECKED,
  CB_SETCURSEL,
  CBN_SELCHANGE,
  ES_MULTILINE,
  GWL_STYLE,
  MK_LBUTTON,
  VK_RETURN,
  WM_COMMAND,
  WM_KEYDOWN,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
  enumChildWindows,
  getClassName,
  getParentWindow,
  getWindowLong,
  getWindowText,
  isWindowEnabled,
  isWindowVisible,
  sendMessage,
  sendText,
  waitForTopWindow,
} from "./winapi.ts";

type ChatBody = { model: string; messages: { role: string; content: string }[] };

export async function testit(): Promise<void> {
  if (process.platform !== "win32" || USE_NG) return;
  const requests: ChatBody[] = [];
  let pending: ReadableStreamDefaultController<Uint8Array> | undefined;
  const server = Bun.serve({
    hostname: "127.0.0.1",
    port: 0,
    async fetch(request) {
      const body = (await request.json()) as ChatBody;
      requests.push(body);
      if (body.model !== "Fixture-Model") throw new Error("Model ID casing was changed");
      if (body.messages.at(-1)?.content === "stop me") {
        return new Response(
          new ReadableStream<Uint8Array>({
            start(controller) {
              pending = controller;
              controller.enqueue(new TextEncoder().encode(": waiting\n\n"));
            },
            cancel() {
              pending = undefined;
            },
          }),
          { headers: { "content-type": "text/event-stream" } },
        );
      }
      return new Response('data: {"choices":[{"index":0,"delta":{"content":"mock answer"}}]}\n\ndata: [DONE]\n\n', {
        headers: { "content-type": "text/event-stream" },
      });
    },
  });
  const appdata = writeAppdata(
    "custom-ai-ui",
    `CustomAI [\n BaseURL = http://127.0.0.1:${server.port}/v1\n Model = Fixture-Model\n ApiKeyEnv = SUMATRA_CUSTOM_AI_UI_TEST_KEY\n]\n`,
  );
  try {
    await withControlledSumatra(
      EXE,
      async (client, proc) => {
        const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
        await client.waitForRenderIdle();
        sendCommandSync(frame, cmdId("CmdAIChatWithCustomAPI"));
        let input = 0;
        await pollUntil(
          async () => {
            enumChildWindows(frame, (hwnd) => {
              if (
                isWindowVisible(hwnd) &&
                /edit/i.test(getClassName(hwnd)) &&
                getWindowLong(hwnd, GWL_STYLE) & ES_MULTILINE
              )
                input = hwnd;
              return true;
            });
            return input;
          },
          (hwnd) => hwnd !== 0,
          { timeoutMs: 10000, error: "Custom AI input did not appear" },
        );
        const send = async (message: string, count: number) => {
          sendText(input, message);
          sendMessage(input, WM_KEYDOWN, VK_RETURN, 0);
          await pollUntil(
            async () => requests.length,
            (n) => n === count,
          );
        };
        const idle = () =>
          pollUntil(
            async () => isWindowEnabled(input),
            (enabled) => enabled,
          );
        await send("first message", 1);
        await idle();
        await send("second message", 2);
        await idle();
        if (
          requests[0]!.messages.length !== 1 ||
          requests[1]!.messages.length !== 3 ||
          requests[1]!.messages[1]!.content !== "mock answer"
        ) {
          throw new Error("Custom AI did not preserve multi-turn history");
        }
        const panel = getParentWindow(input);
        let session = 0;
        enumChildWindows(panel, (hwnd) => {
          if (!session && getClassName(hwnd) === "ComboBox") session = hwnd;
          return true;
        });
        if (!session) throw new Error("Missing Custom AI session/Stop controls");
        sendMessage(session, CB_SETCURSEL, 0, 0); // New Session
        sendMessage(panel, WM_COMMAND, CBN_SELCHANGE << 16, session);
        await send("new session", 3);
        await idle();
        if (requests[2]!.messages.length !== 1) throw new Error("New Session retained the previous conversation");
        await send("stop me", 4);
        const layout = String((await client.request(ControlCommand.TestLayout, ["get"]))[1]);
        const stop = /layout path=aiChat\S* kind=virtCtrlButton visibility=0 rect=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(
          layout,
        );
        if (!stop) throw new Error("Stop button was not shown while an API request was running");
        const x = +stop[1]! + Math.floor(+stop[3]! / 2);
        const y = +stop[2]! + Math.floor(+stop[4]! / 2);
        const point = (y << 16) | (x & 0xffff);
        sendMessage(panel, WM_MOUSEMOVE, 0, point);
        sendMessage(panel, WM_LBUTTONDOWN, MK_LBUTTON, point);
        sendMessage(panel, WM_LBUTTONUP, 0, point);
        await idle();
        await pollUntil(
          async () => pending === undefined,
          (disconnected) => disconnected,
          {
            error: "Stop did not disconnect the HTTP stream",
          },
        );
        await send("after stop", 5);
        await idle();
        if (requests[4]!.messages.length !== 3 || requests[4]!.messages.some((m) => m.content === "stop me")) {
          throw new Error("Stopped request was committed to conversation history");
        }
        let includePage = 0;
        enumChildWindows(panel, (hwnd) => {
          if (getClassName(hwnd) === "Button" && getWindowText(hwnd) === "Include current page text")
            includePage = hwnd;
          return true;
        });
        if (!includePage) throw new Error("Missing page text checkbox");
        sendMessage(includePage, BM_SETCHECK, BST_CHECKED, 0);
        await send("read this page", 6);
        await idle();
        if (!/zlib/i.test(requests[5]!.messages.at(-1)!.content)) throw new Error("Missing opted-in page text");
      },
      ["-appdata", appdata, join(ROOT, "ext/a-zlib/zlib.3.pdf")],
    );
  } finally {
    pending?.close();
    server.stop(true);
  }
}

if (import.meta.main) await runStandalone(testit);
