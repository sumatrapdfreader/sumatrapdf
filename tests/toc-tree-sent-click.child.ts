// helper for toc-tree-sent-click.ts: a synchronous SendMessage click on a
// window, run in its own process so the parent can time it out
import { MK_LBUTTON, packCoords, sendMessage, WM_LBUTTONDOWN, WM_LBUTTONUP } from "./winapi.ts";

const hwnd = Number(process.argv[2]);
const x = Number(process.argv[3]);
const y = Number(process.argv[4]);
const lp = packCoords(x, y);
sendMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
sendMessage(hwnd, WM_LBUTTONUP, 0, lp);
