import {
  readdirSync,
  renameSync,
  copyFileSync,
  statSync,
  existsSync,
  unlinkSync,
  openSync,
  readSync,
  closeSync,
} from "fs";
import { join } from "path";

function dot() {
  process.stdout.write(".");
  // @ts-ignore - Bun-specific API for flushing stdout
  if (typeof Bun !== "undefined") Bun.stdout.flush?.();
}

const dirs = ["X:\\sumtest\\bugs\\", "C:\\Users\\kjk\\OneDrive\\!sumatra\\bugs\\"];

// --- Step 1: rename "bug<number><rest>" to "bug-<number><rest>" ---

const reBugNoHyphen = /^bug(\d+)(.*)/i;

function renameBugFiles(dir: string) {
  console.log(`renameBugFiles: ${dir}`);
  if (!existsSync(dir)) {
    console.log(`skipping ${dir} (does not exist)`);
    return;
  }
  const entries = readdirSync(dir);
  let nRenamed = 0;
  let nSeen = 0;
  for (const name of entries) {
    nSeen++;
    if (nSeen % 16 === 0) {
      dot();
    }
    const m = name.match(reBugNoHyphen);
    if (!m) {
      continue;
    }
    const newName = `bug-${m[1]}${m[2]}`;
    if (newName === name) {
      continue;
    }
    const oldPath = join(dir, name);
    const newPath = join(dir, newName);
    if (existsSync(newPath)) {
      console.log(`skipping ${name} -> ${newName} (target already exists)`);
      continue;
    }
    console.log(`${name} -> ${newName}`);
    renameSync(oldPath, newPath);
    nRenamed++;
  }
  if (nSeen > 0) {
    console.log(""); // newline after dots
  }
  if (nRenamed > 0) {
    console.log(`${dir}: renamed ${nRenamed} files`);
  }
}

// --- Step 2: sync directories ---

interface BugFileInfo {
  bugNumber: number;
  fileName: string;
  fileSize: number;
}

const reBugFile = /^bug-(\d+)(.*)/i;

const kPrefixCmpChunk = 1024 * 1024;

// True when prefixPath is a byte-for-byte prefix of fullPath.
function fileIsPrefix(prefixPath: string, prefixSize: number, fullPath: string): boolean {
  if (prefixSize === 0) {
    return true;
  }
  const a = Buffer.alloc(Math.min(kPrefixCmpChunk, prefixSize));
  const b = Buffer.alloc(a.length);
  const fa = openSync(prefixPath, "r");
  const fb = openSync(fullPath, "r");
  try {
    let off = 0;
    while (off < prefixSize) {
      const n = Math.min(a.length, prefixSize - off);
      const na = readSync(fa, a, 0, n, off);
      const nb = readSync(fb, b, 0, n, off);
      if (na !== n || nb !== n || !a.subarray(0, n).equals(b.subarray(0, n))) {
        return false;
      }
      off += n;
    }
    return true;
  } finally {
    closeSync(fa);
    closeSync(fb);
  }
}

function collectBugFiles(dir: string): BugFileInfo[] {
  console.log(`collectBugFiles: ${dir}`);
  if (!existsSync(dir)) {
    console.log(`collectBugFiles: ${dir} does not exist, returning empty`);
    return [];
  }
  const entries = readdirSync(dir);
  const result: BugFileInfo[] = [];
  for (const name of entries) {
    const m = name.match(reBugFile);
    if (!m) {
      continue;
    }
    const fullPath = join(dir, name);
    const st = statSync(fullPath);
    if (!st.isFile()) {
      continue;
    }
    result.push({
      bugNumber: parseInt(m[1], 10),
      fileName: name,
      fileSize: st.size,
    });
    if (result.length % 16 === 0) {
      dot();
    }
  }
  if (result.length > 0) {
    console.log(""); // newline after dots
  }
  console.log(`collectBugFiles: ${dir} done, ${result.length} files`);
  return result;
}

function syncDirs(dirs: string[]) {
  // collect file info for each directory
  const dirFiles: Map<string, BugFileInfo[]> = new Map();
  for (const dir of dirs) {
    dirFiles.set(dir, collectBugFiles(dir));
  }

  let nCopied = 0;
  let nOverwritten = 0;
  // for each directory, check each file against all other directories
  for (const srcDir of dirs) {
    console.log(`syncDirs: processing srcDir ${srcDir}`);
    const srcFiles = dirFiles.get(srcDir)!;
    for (const srcFile of srcFiles) {
      for (const dstDir of dirs) {
        if (dstDir === srcDir) {
          continue;
        }
        const dstFiles = dirFiles.get(dstDir)!;
        // look for a file with same bug number and file size
        const match = dstFiles.find((f) => f.bugNumber === srcFile.bugNumber && f.fileSize === srcFile.fileSize);
        if (match) {
          if (match.fileName.toLowerCase() !== srcFile.fileName.toLowerCase()) {
            // rename the worse-named file to the better name
            // prefer longer name; if same length, prefer name with file extension (dot)
            let longer: string;
            let shorter: string;
            if (srcFile.fileName.length !== match.fileName.length) {
              longer = srcFile.fileName.length > match.fileName.length ? srcFile.fileName : match.fileName;
              shorter = srcFile.fileName.length > match.fileName.length ? match.fileName : srcFile.fileName;
            } else {
              // same length: prefer the one with a dot-extension
              const srcHasDot = srcFile.fileName.includes(".");
              const matchHasDot = match.fileName.includes(".");
              if (srcHasDot && !matchHasDot) {
                longer = srcFile.fileName;
                shorter = match.fileName;
              } else if (!srcHasDot && matchHasDot) {
                longer = match.fileName;
                shorter = srcFile.fileName;
              } else {
                // both have or both lack extension, pick src
                longer = srcFile.fileName;
                shorter = match.fileName;
              }
            }
            if (longer !== shorter) {
              // figure out which entry has the shorter name and rename it
              const shorterInSrc = srcFile.fileName === shorter;
              const renameDir = shorterInSrc ? srcDir : dstDir;
              const renameInfo = shorterInSrc ? srcFile : match;
              const oldPath = join(renameDir, shorter);
              const newPath = join(renameDir, longer);
              if (!existsSync(newPath)) {
                console.log(`rename: ${shorter} -> ${longer} in ${renameDir}`);
                renameSync(oldPath, newPath);
                renameInfo.fileName = longer;
              } else {
                // better-named file already exists, delete the worse-named duplicate
                console.log(`delete duplicate: ${shorter} in ${renameDir} (keeping ${longer})`);
                unlinkSync(oldPath);
                renameInfo.fileName = longer;
              }
            }
          }
          continue;
        }
        // check if a file with the same name already exists (different size = conflict)
        const srcPath = join(srcDir, srcFile.fileName);
        const dstPath = join(dstDir, srcFile.fileName);
        if (existsSync(dstPath)) {
          const dstSt = statSync(dstPath);
          if (dstSt.isFile() && dstSt.size < srcFile.fileSize && fileIsPrefix(dstPath, dstSt.size, srcPath)) {
            const copyStart = performance.now();
            copyFileSync(srcPath, dstPath);
            const copyMs = (performance.now() - copyStart).toFixed(0);
            console.log(
              `overwrite truncated: ${srcFile.fileName} in ${dstDir} (${dstSt.size} -> ${srcFile.fileSize}, ${copyMs}ms)`,
            );
            const existing = dstFiles.find((f) => f.fileName.toLowerCase() === srcFile.fileName.toLowerCase());
            if (existing) {
              existing.fileSize = srcFile.fileSize;
            } else {
              dstFiles.push({
                bugNumber: srcFile.bugNumber,
                fileName: srcFile.fileName,
                fileSize: srcFile.fileSize,
              });
            }
            nOverwritten++;
            continue;
          }
          console.log(
            `conflict: ${srcFile.fileName} exists in ${dstDir} with different size (${srcFile.fileSize} vs ${dstSt.size}), skipping`,
          );
          continue;
        }
        // copy the file
        const copyStart = performance.now();
        copyFileSync(srcPath, dstPath);
        const copyMs = (performance.now() - copyStart).toFixed(0);
        console.log(`copy: ${srcFile.fileName} -> ${dstDir} (${copyMs}ms)`);
        // add to dstFiles so we don't copy it again from another source
        dstFiles.push({
          bugNumber: srcFile.bugNumber,
          fileName: srcFile.fileName,
          fileSize: srcFile.fileSize,
        });
        nCopied++;
      }
    }
  }
  console.log(`sync: copied ${nCopied} files, overwrote ${nOverwritten} truncated`);
}

export function run() {
  for (const dir of dirs) {
    renameBugFiles(dir);
  }
  syncDirs(dirs);
}

run(); // uncomment or call run() to execute
