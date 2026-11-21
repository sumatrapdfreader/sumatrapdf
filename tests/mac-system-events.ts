// Real keyboard input and menu bar reads on macOS through System Events.
// Unlike mac-control.ts (the app's control pipe), keys sent this way go
// through AppKit, so the menu bar sees them first, as it does for a person.
//
// Needs the terminal that runs the test to be allowed in System Settings >
// Privacy & Security > Accessibility (and, once, to control System Events).
// Each call takes the keyboard focus: don't type while a test runs.

import { dlopen, FFIType, type Pointer } from "bun:ffi";

export type MacMods = { cmd?: boolean; shift?: boolean; opt?: boolean; ctrl?: boolean };

// key codes of keys without a character (Carbon's kVK_*)
export const kKeyTab = 48;
export const kKeyEscape = 53;
export const kKeyDelete = 51;

function osa(script: string): { ok: boolean; out: string } {
  const p = Bun.spawnSync(["osascript", "-e", script], { stdout: "pipe", stderr: "pipe" });
  return { ok: p.exitCode === 0, out: (p.stdout.toString() + p.stderr.toString()).trim() };
}

function inProcess(pid: number, body: string): string {
  const r = osa(`tell application "System Events"\nset p to first process whose unix id is ${pid}\n${body}\nend tell`);
  if (!r.ok) {
    throw new Error(`System Events: ${r.out}`);
  }
  return r.out;
}

function usingClause(m: MacMods): string {
  const names: string[] = [];
  if (m.cmd) names.push("command down");
  if (m.shift) names.push("shift down");
  if (m.opt) names.push("option down");
  if (m.ctrl) names.push("control down");
  return names.length > 0 ? ` using {${names.join(", ")}}` : "";
}

// Whether System Events may drive other apps; false explains how to allow it.
export function macCanSendKeys(): boolean {
  return osa(`tell application "System Events" to return name of first process whose frontmost is true`).ok;
}

// Brings the process's front window forward and makes it the key window:
// keys typed afterwards go to it.
export function macActivate(pid: number): void {
  inProcess(pid, 'set frontmost of p to true\nperform action "AXRaise" of front window of p');
}

export function macKeystroke(pid: number, text: string, mods: MacMods = {}): void {
  inProcess(pid, `keystroke "${text.replace(/"/g, '\\"')}"${usingClause(mods)}`);
}

export function macKeyCode(pid: number, code: number, mods: MacMods = {}): void {
  inProcess(pid, `key code ${code}${usingClause(mods)}`);
}

// CoreGraphics' keyboard events, posted as the hardware would: a modifier is
// pressed and released as a key of its own, which System Events' `using
// {control down}` doesn't send (an app that acts when Control is let go, like
// the ⌃Tab switcher, needs it). They go to the frontmost app (macActivate).
type CgApi = {
  createSource: (state: number) => Pointer | null;
  createKey: (src: Pointer | null, code: number, down: boolean) => Pointer | null;
  setFlags: (ev: Pointer, flags: bigint) => void;
  post: (tap: number, ev: Pointer) => void;
  release: (ref: Pointer) => void;
};
let cgApi: CgApi | null = null;

function cg(): CgApi {
  if (cgApi) {
    return cgApi;
  }
  const lib = dlopen("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics", {
    CGEventSourceCreate: { args: [FFIType.i32], returns: FFIType.ptr },
    CGEventCreateKeyboardEvent: { args: [FFIType.ptr, FFIType.u16, FFIType.bool], returns: FFIType.ptr },
    CGEventSetFlags: { args: [FFIType.ptr, FFIType.u64], returns: FFIType.void },
    CGEventPost: { args: [FFIType.u32, FFIType.ptr], returns: FFIType.void },
  });
  const cf = dlopen("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", {
    CFRelease: { args: [FFIType.ptr], returns: FFIType.void },
  });
  cgApi = {
    createSource: (state) => lib.symbols.CGEventSourceCreate(state),
    createKey: (src, code, down) => lib.symbols.CGEventCreateKeyboardEvent(src, code, down),
    setFlags: (ev, flags) => lib.symbols.CGEventSetFlags(ev, flags),
    post: (tap, ev) => lib.symbols.CGEventPost(tap, ev),
    release: (ref) => cf.symbols.CFRelease(ref),
  };
  return cgApi;
}

const kCGEventSourceStateHIDSystemState = 1;
const kCGHIDEventTap = 0;
const kKeyHoldMs = 30;
// kVK_Shift, kVK_Control, kVK_Option, kVK_Command with their CGEventFlags
const kModKeys: { name: keyof MacMods; code: number; flag: bigint }[] = [
  { name: "ctrl", code: 59, flag: 0x40000n },
  { name: "shift", code: 56, flag: 0x20000n },
  { name: "opt", code: 58, flag: 0x80000n },
  { name: "cmd", code: 55, flag: 0x100000n },
];

// Presses the key with code `code` while holding the modifiers.
export async function macPressHolding(code: number, mods: MacMods): Promise<void> {
  const api = cg();
  const src = api.createSource(kCGEventSourceStateHIDSystemState);
  let flags = 0n;
  const send = async (key: number, down: boolean) => {
    const ev = api.createKey(src, key, down);
    if (!ev) {
      throw new Error("CGEventCreateKeyboardEvent failed");
    }
    api.setFlags(ev, flags);
    api.post(kCGHIDEventTap, ev);
    api.release(ev);
    await Bun.sleep(kKeyHoldMs);
  };
  const held = kModKeys.filter((m) => mods[m.name]);
  for (const m of held) {
    flags |= m.flag;
    await send(m.code, true);
  }
  await send(code, true);
  await send(code, false);
  for (const m of held.reverse()) {
    flags &= ~m.flag;
    await send(m.code, false);
  }
  if (src) {
    api.release(src);
  }
}

export function macFrontWindowTitle(pid: number): string {
  return inProcess(pid, "return name of front window of p");
}

export type MacMenuItem = { menu: string; path: string; key: string; mods: number };

// Every item of the menu bar (submenus too) with its key equivalent. mods is
// AXMenuItemCmdModifiers: bit 0 Shift, 1 Option, 2 Control, 3 no Command.
export function macMenuItems(pid: number): MacMenuItem[] {
  const script = `
on menuRows(mi, top, prefix)
  tell application "System Events"
    set nm to name of mi
    if nm is missing value then return ""
    set ch to ""
    set md to 0
    try
      set ch to value of attribute "AXMenuItemCmdChar" of mi
    end try
    try
      set md to value of attribute "AXMenuItemCmdModifiers" of mi
    end try
    if ch is missing value then set ch to ""
    set t to character id 9
    set out to top & t & prefix & nm & t & ch & t & md & linefeed
    try
      repeat with sub in menu items of menu 1 of mi
        set out to out & my menuRows(sub, top, prefix & nm & " > ")
      end repeat
    end try
    return out
  end tell
end menuRows
tell application "System Events"
  set p to first process whose unix id is ${pid}
  set out to ""
  repeat with mb in menu bar items of menu bar 1 of p
    try
      repeat with mi in menu items of menu 1 of mb
        set out to out & my menuRows(mi, name of mb, "")
      end repeat
    end try
  end repeat
  return out
end tell`;
  const r = osa(script);
  if (!r.ok) {
    throw new Error(`System Events: ${r.out}`);
  }
  const items: MacMenuItem[] = [];
  for (const line of r.out.split("\n")) {
    const [menu, path, key, mods] = line.split("\t");
    if (!menu || !path) continue;
    items.push({ menu, path, key: key ?? "", mods: Number(mods ?? 0) });
  }
  return items;
}
