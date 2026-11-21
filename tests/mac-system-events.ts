// Real keyboard input and menu bar reads on macOS through System Events.
// Unlike mac-control.ts (the app's control pipe), keys sent this way go
// through AppKit, so the menu bar sees them first, as it does for a person.
//
// Needs the terminal that runs the test to be allowed in System Settings >
// Privacy & Security > Accessibility (and, once, to control System Events).
// Each call takes the keyboard focus: don't type while a test runs.

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

export function macFrontWindowTitle(pid: number): string {
  return inProcess(pid, "return name of front window of p");
}

export type MacMenuItem = { menu: string; path: string; key: string; mods: number };

// Every item of the menu bar (submenus too) with its key equivalent. mods is
// AXMenuItemCmdModifiers: bit 0 Shift, 1 Option, 2 Control, 3 no Command.
export function macMenuItems(pid: number): MacMenuItem[] {
  const script = `
on rows(mi, top, prefix)
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
    set out to top & tab & prefix & nm & tab & ch & tab & md & linefeed
    try
      repeat with sub in menu items of menu 1 of mi
        set out to out & my rows(sub, top, prefix & nm & " > ")
      end repeat
    end try
    return out
  end tell
end rows
tell application "System Events"
  set p to first process whose unix id is ${pid}
  set out to ""
  repeat with mb in menu bar items of menu bar 1 of p
    try
      repeat with mi in menu items of menu 1 of mb
        set out to out & my rows(mi, name of mb, "")
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
