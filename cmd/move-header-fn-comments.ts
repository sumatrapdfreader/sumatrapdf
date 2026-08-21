// Move prose function comments from .h to matching .cpp definitions under src/.
// Per Agents.md: keep headers terse; leave comments that have no .cpp counterpart
// (struct/enum/macro/constexpr/inline/pure virtual) in the header.

import { existsSync, readdirSync, readFileSync, writeFileSync, statSync } from "node:fs";
import { join, dirname, basename, relative, extname } from "node:path";

const ROOT = process.cwd();
const SRC = join(ROOT, "src");
const DRY = process.argv.includes("--dry");
const VERBOSE = process.argv.includes("-v");

const SKIP_HEADERS = new Set([
  "Settings.h", // generated
  "Commands.h", // generated
  "BuildConfig.h",
  "BuildConfig_default.h",
  "CaptionGlyphs.h", // generated
  "Dia2Subset.h", // third-party COM headers under tools/efi
  "nsWindowsDllInterceptor.h", // vendored
  "parg.h",
]);

function walk(dir: string, acc: string[] = []): string[] {
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    const p = join(dir, e.name);
    if (e.isDirectory()) {
      walk(p, acc);
    } else if (e.name.endsWith(".h") || e.name.endsWith(".hpp")) {
      acc.push(p);
    }
  }
  return acc;
}

function isCopyrightOrLicense(comment: string): boolean {
  return /Copyright|License:|SPDX-|COPYING/i.test(comment);
}

/** Header section banners like //--- timing — keep in .h, not function docs. */
function isSectionBannerComment(comment: string): boolean {
  const lines = comment
    .split(/\r?\n/)
    .map((l) => l.trim())
    .filter(Boolean);
  if (!lines.length) {
    return true;
  }
  return lines.every((l) => /^\/\/\s*---/.test(l) || /^\/\/\s*===/.test(l) || /^\/\*\s*---/.test(l));
}

function stripLineCommentNoise(s: string): string {
  return s.replace(/\/\/.*$/, "").trim();
}

/** Extract a C++ identifier that looks like the function name from a declaration. */
function extractFuncName(decl: string): string | null {
  // remove trailing ; =0 =default =delete override final noexcept specs
  let d = decl
    .replace(/\/\*[\s\S]*?\*\//g, " ")
    .replace(/\/\/.*$/gm, "")
    .replace(/\s+/g, " ")
    .trim();
  // cut at first '(' of parameter list — but skip operator() and template <
  // find matching paren for params: last approach — find ) before ; then back to (
  const semi = d.lastIndexOf(";");
  if (semi >= 0) {
    d = d.slice(0, semi).trim();
  }
  // strip trailing = 0 / = default / = delete / override / final / const / noexcept(...)
  d = d
    .replace(/\s*(override|final)\s*$/g, "")
    .replace(/\s*noexcept(\s*\([^)]*\))?\s*$/g, "")
    .replace(/\s*const\s*$/g, "")
    .replace(/\s*=\s*(0|default|delete)\s*$/g, "")
    .trim();

  const open = d.lastIndexOf("(");
  if (open < 0) {
    return null;
  }
  // name token immediately before (
  const before = d.slice(0, open).trim();
  // operator overloads
  const op = before.match(/\boperator\s*(?:\(\)|\[\]|[^\s]+)\s*$/);
  if (op) {
    return op[0].replace(/\s+/g, " ");
  }
  // last identifier
  const m = before.match(/([A-Za-z_~][A-Za-z0-9_]*)\s*$/);
  return m ? m[1] : null;
}

function isPureVirtual(decl: string): boolean {
  return /=\s*0\s*;/.test(decl);
}

function isDefaultOrDelete(decl: string): boolean {
  return /=\s*(default|delete)\s*;/.test(decl);
}

function hasBodyInHeader(decl: string): boolean {
  // definition with { ... } — not pure virtual
  if (isPureVirtual(decl) || isDefaultOrDelete(decl)) {
    return false;
  }
  return /\{/.test(decl);
}

function looksLikeFunctionDecl(decl: string): boolean {
  const d = decl.trim();
  if (!d) {
    return false;
  }
  if (/^\s*#/.test(d)) {
    return false;
  }
  if (/^\s*(struct|class|enum|namespace|typedef|using|template)\b/.test(d)) {
    return false;
  }
  // field-like: Type name; without (
  if (!/\(/.test(d)) {
    return false;
  }
  // macro call style without return type is hard; require ending with ;
  if (!/;\s*$/.test(stripLineCommentNoise(d.split("\n").pop() || ""))) {
    return false;
  }
  // skip constructor initializer lists with : — rare in headers as decls
  // function pointer typedefs: using Foo = void(*)();
  if (/^\s*using\b/.test(d)) {
    return false;
  }
  return extractFuncName(d) !== null;
}

type Candidate = {
  headerPath: string;
  commentStart: number; // line index
  commentEnd: number; // exclusive
  declStart: number;
  declEnd: number; // exclusive
  comment: string;
  decl: string;
  funcName: string;
  className: string | null; // enclosing class/struct if any
  namespaceStack: string[];
};

function parseHeader(headerPath: string): Candidate[] {
  const text = readFileSync(headerPath, "utf8");
  const lines = text.split(/\r?\n/);
  const candidates: Candidate[] = [];

  // track class/struct/namespace with a simple brace stack
  type Frame = { kind: "ns" | "class" | "other"; name: string | null };
  const stack: Frame[] = [];
  let i = 0;

  const currentClass = (): string | null => {
    for (let k = stack.length - 1; k >= 0; k--) {
      if (stack[k].kind === "class") {
        return stack[k].name;
      }
    }
    return null;
  };
  const currentNamespaces = (): string[] => stack.filter((f) => f.kind === "ns" && f.name).map((f) => f.name!);

  while (i < lines.length) {
    const line = lines[i];
    const trimmed = line.trim();

    // preprocessor: ignore brace tracking inside? still track for #if roughly by ignoring
    if (trimmed.startsWith("#")) {
      i++;
      continue;
    }

    // comment block candidate
    if (trimmed.startsWith("//") || trimmed.startsWith("/*")) {
      const commentStart = i;
      if (trimmed.startsWith("/*")) {
        while (i < lines.length && !lines[i].includes("*/")) {
          i++;
        }
        i++; // line with */
      } else {
        while (i < lines.length && lines[i].trim().startsWith("//")) {
          i++;
        }
      }
      const commentEnd = i;
      const comment = lines.slice(commentStart, commentEnd).join("\n");
      if (isCopyrightOrLicense(comment)) {
        continue;
      }

      // skip blank lines between comment and decl
      let j = i;
      while (j < lines.length && lines[j].trim() === "") {
        j++;
      }
      if (j >= lines.length) {
        continue;
      }

      // collect declaration (possibly multi-line) until ; or { ... };
      let k = j;
      let declLines: string[] = [];
      let brace = 0;
      let seenSemiOrBody = false;
      while (k < lines.length) {
        const L = lines[k];
        declLines.push(L);
        for (const ch of L) {
          if (ch === "{") {
            brace++;
          } else if (ch === "}") {
            brace--;
          }
        }
        const stripped = stripLineCommentNoise(L);
        if (brace === 0 && /;/.test(stripped)) {
          seenSemiOrBody = true;
          k++;
          break;
        }
        if (brace > 0 && brace === 0) {
          // closed body
        }
        // inline body: once braces return to 0 after being positive and we see }
        if (declLines.join("\n").includes("{") && brace === 0 && /}/.test(L)) {
          seenSemiOrBody = true;
          k++;
          break;
        }
        k++;
        if (k - j > 40) {
          break;
        }
      }
      if (!seenSemiOrBody) {
        // not a finished decl; fall through to normal line processing from commentStart+1
        // Actually we must still process braces from comment lines? comments don't have braces.
        // Re-process from commentStart without treating as candidate — advance one line only
        i = commentStart + 1;
        // still need brace tracking for that line if not comment... comments skip
        continue;
      }

      const decl = declLines.join("\n");
      const declStart = j;
      const declEnd = k;

      // What follows the comment?
      if (!looksLikeFunctionDecl(decl) || hasBodyInHeader(decl) || isPureVirtual(decl) || isDefaultOrDelete(decl)) {
        // still need to advance i past comment and process decl for braces
        // fall through: set i to declStart and let normal path handle brace tracking
        i = declStart;
        continue;
      }

      // also skip if comment attaches to non-function (access specifier etc.)
      const firstDeclLine = lines[j].trim();
      if (/^(public|private|protected)\s*:/.test(firstDeclLine)) {
        i = declStart;
        continue;
      }
      if (
        /^(struct|class|enum|namespace|typedef|using|template|constexpr|const\s|static\s+constexpr)\b/.test(
          firstDeclLine,
        )
      ) {
        // comment documents type/constant — keep
        // but `static Type foo();` is a function — constexpr function rare as decl
        if (
          !/^\s*(static\s+)?(inline\s+)?(constexpr\s+)?[\w:<>\*&]+\s+[\w:]+\s*\(/.test(firstDeclLine) &&
          !/^\s*(static\s+)?(inline\s+)?[\w:<>\*&]+\s*[\*&]?\s*[\w:]+\s*\(/.test(firstDeclLine) &&
          !/^\s*[\w:<>\*&]+\s+[\w:]+\s*\(/.test(firstDeclLine) &&
          !/^\s*virtual\b/.test(firstDeclLine) &&
          !/^\s*explicit\b/.test(firstDeclLine)
        ) {
          // check more carefully with extractFuncName after full decl
          if (/^\s*(struct|class|enum|namespace|typedef|using|template)\b/.test(firstDeclLine)) {
            i = declStart;
            continue;
          }
        }
        if (/^\s*(struct|class|enum|namespace|typedef|using|template)\b/.test(firstDeclLine)) {
          i = declStart;
          continue;
        }
        // constexpr variable: constexpr int x = 1;
        if (/^\s*(static\s+)?(inline\s+)?constexpr\b/.test(firstDeclLine) && !/\(.*\)/.test(decl.split("=")[0])) {
          i = declStart;
          continue;
        }
      }

      const funcName = extractFuncName(decl);
      if (!funcName) {
        i = declStart;
        continue;
      }

      // skip if it's clearly a data member with function type? rare
      candidates.push({
        headerPath,
        commentStart,
        commentEnd,
        declStart,
        declEnd,
        comment,
        decl,
        funcName,
        className: currentClass(),
        namespaceStack: currentNamespaces(),
      });

      // process decl lines for brace/class tracking then continue after
      i = declStart;
      // don't double-add: process braces for these lines then jump
      // Fall through by not continuing — re-read same lines for stack
      // To avoid re-detecting same comment, skip past comment: we'll re-enter on decl lines
      // Actually next loop iteration if i=declStart will not re-match comment.
      continue;
    }

    // brace / class / namespace tracking for non-comment lines
    // strip strings roughly
    let code = line.replace(/\/\/.*$/, "");
    // class/struct/namespace open
    {
      const ns = code.match(/\bnamespace\s+([A-Za-z_][A-Za-z0-9_]*)?\s*\{/);
      if (ns) {
        stack.push({ kind: "ns", name: ns[1] || null });
        // remove matched part for further braces on same line
        code = code.replace(/\bnamespace\s+([A-Za-z_][A-Za-z0-9_]*)?\s*\{/, "");
      }
      const cls = code.match(/\b(class|struct)\s+([A-Za-z_][A-Za-z0-9_]*)\b[^{;]*\{/);
      if (cls) {
        stack.push({ kind: "class", name: cls[2] });
        code = code.replace(/\b(class|struct)\s+([A-Za-z_][A-Za-z0-9_]*)\b[^{;]*\{/, "");
      } else if (/\b(class|struct)\b[^{;]*\{/.test(code)) {
        // anonymous
        stack.push({ kind: "class", name: null });
        code = code.replace(/\b(class|struct)\b[^{;]*\{/, "");
      }
    }
    for (const ch of code) {
      if (ch === "{") {
        // if we didn't just push class/ns for this brace, push other
        // heuristic: only push other when brace not accounted — simpler: count only closes
        stack.push({ kind: "other", name: null });
      } else if (ch === "}") {
        stack.pop();
      }
    }
    // Fix over-push: class/ns push already included { so we double-pushed.
    // Redo tracking more carefully:
    i++;
  }

  // Re-parse with better brace tracking for class names (second pass only for candidates we already have from broken stack)
  // Actually the stack above double-counts. Re-implement parse with correct tracking.
  return parseHeaderFixed(headerPath);
}

function parseHeaderFixed(headerPath: string): Candidate[] {
  const text = readFileSync(headerPath, "utf8");
  const lines = text.split(/\r?\n/);
  const candidates: Candidate[] = [];

  type Frame = { kind: "ns" | "class" | "other"; name: string | null };
  const stack: Frame[] = [];

  const currentClass = (): string | null => {
    for (let k = stack.length - 1; k >= 0; k--) {
      if (stack[k].kind === "class" && stack[k].name) {
        return stack[k].name;
      }
    }
    return null;
  };
  const currentNamespaces = (): string[] => stack.filter((f) => f.kind === "ns" && f.name).map((f) => f.name as string);

  function trackLine(line: string) {
    // remove comments and strings (rough)
    let s = line.replace(/\/\/.*$/, "");
    s = s.replace(/"([^"\\]|\\.)*"/g, '""');
    s = s.replace(/'([^'\\]|\\.)*'/g, "''");

    // process tokens left to right for class/namespace keywords with following {
    // We'll scan character by character, detecting keywords before {
    let i = 0;
    while (i < s.length) {
      // try namespace
      const rest = s.slice(i);
      const nsM = rest.match(/^\bnamespace\b(\s+([A-Za-z_][A-Za-z0-9_]*))?/);
      if (nsM) {
        const after = rest.slice(nsM[0].length);
        const braceIdx = after.search(/\S/);
        if (braceIdx >= 0 && after[braceIdx] === "{") {
          stack.push({ kind: "ns", name: nsM[2] || null });
          i += nsM[0].length + braceIdx + 1;
          continue;
        }
        i += nsM[0].length;
        continue;
      }
      const clsM = rest.match(/^\b(class|struct)\b(\s+([A-Za-z_][A-Za-z0-9_]*))?/);
      if (clsM) {
        // find { or ; before next
        let j = i + clsM[0].length;
        while (j < s.length && s[j] !== "{" && s[j] !== ";") {
          j++;
        }
        if (j < s.length && s[j] === "{") {
          stack.push({ kind: "class", name: clsM[3] || null });
          i = j + 1;
          continue;
        }
        i += clsM[0].length;
        continue;
      }
      if (s[i] === "{") {
        stack.push({ kind: "other", name: null });
        i++;
        continue;
      }
      if (s[i] === "}") {
        stack.pop();
        i++;
        continue;
      }
      i++;
    }
  }

  let i = 0;
  while (i < lines.length) {
    const line = lines[i];
    const trimmed = line.trim();

    if (trimmed.startsWith("#")) {
      i++;
      continue;
    }

    if (trimmed.startsWith("//") || trimmed.startsWith("/*")) {
      const commentStart = i;
      if (trimmed.startsWith("/*")) {
        while (i < lines.length && !lines[i].includes("*/")) {
          i++;
        }
        i++;
      } else {
        while (i < lines.length && lines[i].trim().startsWith("//")) {
          i++;
        }
      }
      const commentEnd = i;
      const comment = lines.slice(commentStart, commentEnd).join("\n");
      if (isCopyrightOrLicense(comment)) {
        continue;
      }
      // section banners stay in the header (//--- foo, //=== bar)
      if (isSectionBannerComment(comment)) {
        continue;
      }

      let j = i;
      while (j < lines.length && lines[j].trim() === "") {
        j++;
      }
      if (j >= lines.length) {
        continue;
      }

      // don't attach comment across #if etc.
      if (lines[j].trim().startsWith("#")) {
        continue;
      }

      let k = j;
      let brace = 0;
      let startedBody = false;
      const declLines: string[] = [];
      while (k < lines.length) {
        const L = lines[k];
        declLines.push(L);
        const codeOnly = L.replace(/\/\/.*$/, "").replace(/"([^"\\]|\\.)*"/g, '""');
        for (const ch of codeOnly) {
          if (ch === "{") {
            brace++;
            startedBody = true;
          } else if (ch === "}") {
            brace--;
          }
        }
        const stripped = stripLineCommentNoise(L);
        if (!startedBody && /;/.test(stripped)) {
          k++;
          break;
        }
        if (startedBody && brace === 0) {
          k++;
          break;
        }
        k++;
        if (k - j > 40) {
          break;
        }
      }

      const decl = declLines.join("\n");
      const declStart = j;

      if (looksLikeFunctionDecl(decl) && !hasBodyInHeader(decl) && !isPureVirtual(decl) && !isDefaultOrDelete(decl)) {
        const firstDeclLine = lines[j].trim();
        const isTypeIntro = /^(struct|class|enum|namespace|typedef|using|template)\b/.test(firstDeclLine);
        // field / var with initializer call: Type name = Foo(...);
        const eqBeforeParen = (() => {
          const idxEq = decl.indexOf("=");
          const idxPar = decl.indexOf("(");
          return idxEq >= 0 && idxPar >= 0 && idxEq < idxPar;
        })();
        if (!isTypeIntro && !eqBeforeParen) {
          // skip constexpr data
          const beforeParen = decl.split("(")[0];
          if (/^\s*(static\s+)?(inline\s+)?constexpr\b/.test(firstDeclLine) && !/\(/.test(beforeParen)) {
            // not a function
          } else {
            const funcName = extractFuncName(decl);
            const keywords = new Set([
              "return",
              "if",
              "while",
              "for",
              "switch",
              "sizeof",
              "typeof",
              "decltype",
              "new",
              "delete",
              "case",
              "catch",
              "throw",
              "else",
              "const",
              "static",
              "virtual",
              "inline",
              "explicit",
              "operator",
            ]);
            if (funcName && !keywords.has(funcName)) {
              // skip obvious macros: ALL_CAPS name and no type keywords in decl
              const isMacroish =
                /^[A-Z][A-Z0-9_]*$/.test(funcName) &&
                !/\b(void|bool|int|unsigned|long|short|char|float|double|auto|const|static|virtual|explicit|inline|constexpr|noexcept|override|TempStr|TempWStr|Str|WStr|Size|Rect|Color|u8|u16|u32|u64|i8|i16|i32|i64|size_t|HWND|HDC|HMENU|HFONT|HBITMAP|COLORREF|BYTE|DWORD|LONG|UINT|LRESULT|WCHAR|FILETIME|Point|PointF|RectF|Vec|ByteSlice|Pixmap|RenderedBitmap|LPTSTR|LPWSTR|HRESULT|BOOL|HANDLE|HINSTANCE|HMODULE|INT_PTR|UINT_PTR|WPARAM|LPARAM|ATOM)\b/.test(
                  decl,
                );
              if (!isMacroish) {
                candidates.push({
                  headerPath,
                  commentStart,
                  commentEnd,
                  declStart,
                  declEnd: k,
                  comment,
                  decl,
                  funcName,
                  className: currentClass(),
                  namespaceStack: currentNamespaces(),
                });
              }
            }
          }
        }
      }

      // track lines from commentEnd to k-1... actually from i (after comment) we skipped blanks
      // Track all lines from j to k-1, then continue at k
      for (let t = j; t < k; t++) {
        trackLine(lines[t]);
      }
      i = k;
      continue;
    }

    trackLine(line);
    i++;
  }

  return candidates;
}

function siblingCppFiles(headerPath: string): string[] {
  const dir = dirname(headerPath);
  const base = basename(headerPath).replace(/\.(h|hpp)$/i, "");
  const suffixes = ["", "_win", "_posix", "_mac", "_linux"];
  const out: string[] = [];
  for (const s of suffixes) {
    for (const ext of [".cpp", ".cc", ".cxx", ".c", ".mm"]) {
      const p = join(dir, base + s + ext);
      if (existsSync(p)) {
        out.push(p);
      }
    }
  }
  // also scan dir for *base*.cpp that might host methods (EngineMupdf etc. is different)
  return out;
}

/** Find all .cpp/.c/.mm under src for broader method search when sibling fails. */
let allCppCache: string[] | null = null;
function allCppFiles(): string[] {
  if (allCppCache) {
    return allCppCache;
  }
  const acc: string[] = [];
  function w(d: string) {
    for (const e of readdirSync(d, { withFileTypes: true })) {
      const p = join(d, e.name);
      if (e.isDirectory()) {
        w(p);
      } else if (/\.(cpp|cc|cxx|c|mm)$/i.test(e.name)) {
        acc.push(p);
      }
    }
  }
  w(SRC);
  allCppCache = acc;
  return acc;
}

function normalizeComment(c: string): string {
  return c
    .split(/\r?\n/)
    .map((l) => l.trim())
    .filter(Boolean)
    .join("\n");
}

function lineHasCommentAbove(lines: string[], defLine: number): boolean {
  let i = defLine - 1;
  while (i >= 0 && lines[i].trim() === "") {
    i--;
  }
  if (i < 0) {
    return false;
  }
  const t = lines[i].trim();
  return t.startsWith("//") || t.endsWith("*/") || t.startsWith("/*") || t.startsWith("*");
}

/** True if text starting at openParenIdx of `(` is a function *definition* body opener. */
function isFunctionDefinitionAt(lines: string[], lineIdx: number, openParenCol: number): boolean {
  // From the '(' of the candidate name, walk until parens balance, then require '{' next
  // (after const/noexcept/override/final/&/&&/try). A call like if (!Foo(x)) { fails because
  // after Foo's ')' the next char is ')' not '{'.
  let paren = 0;
  let started = false;
  let pastParams = false;
  let trail = "";

  for (let k = lineIdx; k < Math.min(lines.length, lineIdx + 40); k++) {
    const L = lines[k].replace(/\/\/.*$/, "").replace(/\/\*[\s\S]*?\*\//g, " ");
    const startCol = k === lineIdx ? openParenCol : 0;
    for (let c = startCol; c < L.length; c++) {
      const ch = L[c];
      if (!pastParams) {
        if (ch === "(") {
          paren++;
          started = true;
        } else if (ch === ")") {
          paren--;
          if (started && paren === 0) {
            pastParams = true;
          }
        }
        continue;
      }
      // past parameter list
      trail += ch;
      if (/\s/.test(ch)) {
        continue;
      }
      // allow trailing qualifiers
      if (/^[A-Za-z_]/.test(ch)) {
        // collect identifier
        let id = ch;
        c++;
        while (c < L.length && /[A-Za-z0-9_]/.test(L[c])) {
          id += L[c];
          c++;
        }
        c--; // outer loop will ++
        trail += id.slice(1);
        if (/^(const|volatile|noexcept|override|final|try|throw|requires)$/.test(id)) {
          continue;
        }
        // unexpected identifier — not a plain definition header
        return false;
      }
      if (ch === "&" || ch === "*") {
        continue; // ref-qualifiers
      }
      if (ch === "(") {
        // noexcept(
        let depth = 1;
        c++;
        while (c < L.length && depth > 0) {
          if (L[c] === "(") {
            depth++;
          } else if (L[c] === ")") {
            depth--;
          }
          c++;
        }
        c--;
        continue;
      }
      if (ch === "{") {
        return true;
      }
      if (ch === ";") {
        return false; // pure declaration
      }
      if (ch === ":") {
        // ctor initializer list — still a definition, body follows
        // scan forward for {
        for (let k2 = k; k2 < Math.min(lines.length, k + 40); k2++) {
          const L2 = (k2 === k ? L.slice(c) : lines[k2]).replace(/\/\/.*$/, "");
          if (L2.includes("{")) {
            return true;
          }
          if (L2.includes(";")) {
            return false;
          }
        }
        return false;
      }
      // anything else (e.g. ')' of surrounding if) — not a definition
      return false;
    }
  }
  return false;
}

function findDefinitions(cand: Candidate, searchFiles: string[]): { file: string; line: number }[] {
  const hits: { file: string; line: number }[] = [];
  const name = cand.funcName;
  const esc = name.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");

  // Patterns for free function and Class::method — capture start of name for column
  type Pat = { re: RegExp; isMethod: boolean };
  const patterns: Pat[] = [];
  if (cand.className) {
    const cls = cand.className.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
    patterns.push({ re: new RegExp(`\\b${cls}\\s*::\\s*${esc}\\s*\\(`), isMethod: true });
  } else {
    patterns.push({ re: new RegExp(`(?<![:\\w])\\b${esc}\\s*\\(`), isMethod: false });
    for (const ns of cand.namespaceStack) {
      const nse = ns.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
      patterns.push({ re: new RegExp(`\\b${nse}\\s*::\\s*${esc}\\s*\\(`), isMethod: true });
    }
  }

  for (const file of searchFiles) {
    const text = readFileSync(file, "utf8");
    const lines = text.split(/\r?\n/);
    for (let li = 0; li < lines.length; li++) {
      const line = lines[li];
      if (line.trim().startsWith("//") || line.trim().startsWith("*") || line.trim().startsWith("/*")) {
        continue;
      }
      // reject control-flow / return / assignment call sites on this line
      if (/\b(if|while|for|switch|return|sizeof|typeof|decltype)\s*\(/.test(line) && !/::/.test(line)) {
        // might still be `Type Foo(` after if on same line — rare; allow methods with ::
        if (!cand.className) {
          // if the only Name( is inside if(...), skip
          // e.g. if (!GetCachedAttributesEx(...))
          const strippedCtrl = line.replace(/\b(if|while|for|switch|return)\s*\([^;{]*/, "");
          if (!new RegExp(`\\b${esc}\\s*\\(`).test(strippedCtrl)) {
            continue;
          }
        }
      }

      for (const { re, isMethod } of patterns) {
        const m = re.exec(line);
        if (!m) {
          continue;
        }
        const openParenCol = m.index + m[0].length - 1; // position of '('

        // Free function: require a type/qualifier before the name, or Class::, or prev-line type.
        if (!isMethod) {
          const before = line.slice(0, m.index);
          const hasTypeOnLine =
            /\b(static|inline|constexpr|virtual|explicit|extern|const|unsigned|signed|struct|class|enum|void|bool|int|char|long|short|float|double|auto|size_t|u8|u16|u32|u64|i8|i16|i32|i64|Str|WStr|TempStr|TempWStr|Size|Rect|Point|Color|HWND|HDC|BOOL|DWORD|LRESULT|HRESULT|UINT|BYTE|HANDLE|HFONT|HBITMAP|COLORREF|Vec|ByteSlice|Pixmap|RenderedBitmap|FILETIME|PointF|RectF|INT_PTR|UINT_PTR|WPARAM|LPARAM|ATOM|WCHAR)\b/.test(
              before,
            ) ||
            /[>*&]\s*$/.test(before.trim()) ||
            /::\s*$/.test(before);
          if (!hasTypeOnLine) {
            let p = li - 1;
            while (p >= 0 && lines[p].trim() === "") {
              p--;
            }
            if (p < 0) {
              continue;
            }
            const prev = lines[p].trim();
            if (prev.startsWith("//") || prev.startsWith("*") || /[;{}]$/.test(prev)) {
              continue;
            }
            // previous line should look like a return type
            if (!/^[A-Za-z_~]/.test(prev) && !prev.endsWith(">") && !prev.endsWith("*") && !prev.endsWith("&")) {
              continue;
            }
          }
          // reject if name is right after `=` (assignment/lambda) or `!` without type
          if (/[=!]\s*$/.test(before) || /\b(if|while|for|switch|return)\s+!?$/.test(before)) {
            continue;
          }
        }

        if (!isFunctionDefinitionAt(lines, li, openParenCol)) {
          continue;
        }

        hits.push({ file, line: li });
        break;
      }
    }
  }
  return hits;
}

type Edit = {
  file: string;
  // remove inclusive line range, or insert comment before line
  kind: "remove" | "insert";
  startLine: number;
  endLine?: number; // exclusive for remove
  text?: string; // for insert
};

function main() {
  const headers = walk(SRC).filter((h) => !SKIP_HEADERS.has(basename(h)));
  let moved = 0;
  let skippedNoDef = 0;
  let skippedAlready = 0;
  let skippedAmbiguous = 0;

  // Group edits per file
  const headerRemoves = new Map<string, { start: number; end: number }[]>();
  const cppInserts = new Map<string, { line: number; comment: string }[]>();

  for (const h of headers) {
    const cands = parseHeaderFixed(h);
    if (!cands.length) {
      continue;
    }
    const siblings = siblingCppFiles(h);
    for (const c of cands) {
      let files = siblings;
      let hits = findDefinitions(c, files);
      if (!hits.length && c.className) {
        // broader search for methods (impl may live in another cpp)
        hits = findDefinitions(c, allCppFiles());
      }
      if (!hits.length && !c.className) {
        // try broader for free functions once
        hits = findDefinitions(c, allCppFiles());
      }

      // Deduplicate hits by file+line
      const uniq = new Map<string, { file: string; line: number }>();
      for (const hit of hits) {
        uniq.set(`${hit.file}:${hit.line}`, hit);
      }
      hits = [...uniq.values()];

      if (!hits.length) {
        skippedNoDef++;
        if (VERBOSE) {
          console.log("no-def", relative(ROOT, h), c.funcName, c.className || "");
        }
        continue;
      }

      // If too many hits for free function without class, be careful — take only siblings if any hit was sibling
      if (!c.className && hits.length > 4) {
        const sibSet = new Set(siblings);
        const sibHits = hits.filter((x) => sibSet.has(x.file));
        if (sibHits.length) {
          hits = sibHits;
        } else {
          skippedAmbiguous++;
          if (VERBOSE) {
            console.log("ambiguous", relative(ROOT, h), c.funcName, hits.length);
          }
          continue;
        }
      }

      // Check each hit for existing comment
      const toInsert: { file: string; line: number }[] = [];
      let identicalOnSomeDef = false;
      for (const hit of hits) {
        const lines = readFileSync(hit.file, "utf8").split(/\r?\n/);
        if (lineHasCommentAbove(lines, hit.line)) {
          const existing = (() => {
            let x = hit.line - 1;
            while (x >= 0 && lines[x].trim() === "") {
              x--;
            }
            let end = x;
            let start = x;
            if (lines[x].includes("*/")) {
              while (start >= 0 && !lines[start].includes("/*")) {
                start--;
              }
            } else if (lines[x].trim().startsWith("//")) {
              while (start >= 0 && lines[start].trim().startsWith("//")) {
                start--;
              }
              start++;
            }
            return lines.slice(start, end + 1).join("\n");
          })();
          if (normalizeComment(existing) === normalizeComment(c.comment)) {
            // already present on this def
            identicalOnSomeDef = true;
          } else {
            // different comment already on def: still insert header prose above it
            // so we don't drop documentation, then keep both.
            toInsert.push(hit);
            skippedAlready++;
            if (VERBOSE) {
              console.log("prepend-above-existing", relative(ROOT, hit.file), c.funcName);
            }
          }
        } else {
          toInsert.push(hit);
        }
      }

      // Only strip the header comment if it lands on at least one definition
      // (or is already identical there). Never delete prose with nowhere to go.
      if (!toInsert.length && !identicalOnSomeDef) {
        skippedNoDef++;
        if (VERBOSE) {
          console.log("no-place", relative(ROOT, h), c.funcName);
        }
        continue;
      }

      const rems = headerRemoves.get(c.headerPath) || [];
      rems.push({ start: c.commentStart, end: c.commentEnd });
      headerRemoves.set(c.headerPath, rems);

      for (const hit of toInsert) {
        const arr = cppInserts.get(hit.file) || [];
        arr.push({ line: hit.line, comment: c.comment });
        cppInserts.set(hit.file, arr);
        moved++;
      }
    }
  }

  // Apply header removes (from bottom to top)
  let headersChanged = 0;
  for (const [file, rems] of headerRemoves) {
    rems.sort((a, b) => b.start - a.start);
    // merge overlapping
    const lines = readFileSync(file, "utf8").split(/\r?\n/);
    const remove = new Set<number>();
    for (const r of rems) {
      for (let i = r.start; i < r.end; i++) {
        remove.add(i);
      }
      // also remove a single blank line that would be left between prev and decl?
      // If line after commentEnd is blank and was only separator, keep one blank as before.
      // Removing only comment lines is fine.
    }
    const newLines = lines.filter((_, idx) => !remove.has(idx));
    // collapse triple blank lines that may result
    const cleaned: string[] = [];
    for (const L of newLines) {
      if (L.trim() === "" && cleaned.length && cleaned[cleaned.length - 1].trim() === "") {
        // allow single blank; skip extra
        continue;
      }
      cleaned.push(L);
    }
    if (!DRY) {
      writeFileSync(file, cleaned.join("\n"), "utf8");
    }
    headersChanged++;
    console.log("header", relative(ROOT, file), "removed", rems.length, "comment-block(s)");
  }

  // Apply cpp inserts (from bottom to top so line numbers stay valid)
  let cppsChanged = 0;
  for (const [file, inserts] of cppInserts) {
    inserts.sort((a, b) => b.line - a.line);
    const lines = readFileSync(file, "utf8").split(/\r?\n/);
    for (const ins of inserts) {
      // detect indentation of definition line
      const defLine = lines[ins.line] || "";
      const indent = (defLine.match(/^(\s*)/) || ["", ""])[1];
      const commentLines = ins.comment.split(/\r?\n/).map((l) => {
        // re-indent // comments to match definition
        if (l.trim().startsWith("//")) {
          return indent + l.trim();
        }
        if (l.trim().startsWith("/*") || l.trim().startsWith("*") || l.trim().endsWith("*/")) {
          return indent + l.trim();
        }
        return indent + l;
      });
      lines.splice(ins.line, 0, ...commentLines);
    }
    if (!DRY) {
      writeFileSync(file, lines.join("\n"), "utf8");
    }
    cppsChanged++;
    console.log("cpp", relative(ROOT, file), "inserted", inserts.length, "comment-block(s)");
  }

  console.log(
    `\n${DRY ? "DRY RUN " : ""}done. headers=${headersChanged} cpps=${cppsChanged} inserts=${moved} no-def=${skippedNoDef} already-commented-hits=${skippedAlready} ambiguous=${skippedAmbiguous}`,
  );
}

main();
