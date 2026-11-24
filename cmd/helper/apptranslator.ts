// Talking to apptranslator, where SumatraPDF's translations live: the upload
// secret and editing one translation. Used by trans-dl.ts and check-access-keys.ts.

import { readFileSync } from "node:fs";
import { join } from "node:path";

export const DEFAULT_SERVER = "https://www.apptranslator.org";
export const LOCAL_SERVER = "http://127.0.0.1:9311";
export const APP_NAME = "SumatraPDF";

// a secret from ../hack/secrets/sumatrapdf.env, else from the environment
export function readSecret(name: string, required: boolean): string {
  const secretsPath = join("..", "hack", "secrets", "sumatrapdf.env");
  try {
    const content = readFileSync(secretsPath, "utf-8");
    for (const line of content.split("\n")) {
      const trimmed = line.trim();
      if (trimmed.startsWith(name)) {
        const eqIdx = trimmed.indexOf("=");
        if (eqIdx !== -1) {
          const val = trimmed.substring(eqIdx + 1).trim();
          if (val.length >= 4) {
            return val;
          }
        }
      }
    }
  } catch {
    // fall through
  }
  const val = process.env[name] ?? "";
  if (val.length >= 4) {
    return val;
  }
  if (required) {
    throw new Error(`must set ${name} env variable or in secrets file`);
  }
  return "";
}

export function tryGetTransSecret(): string {
  const s = readSecret("TRANS_UPLOAD_SECRET", false);
  if (s) {
    console.log("Got TRANS_UPLOAD_SECRET");
  }
  return s;
}

// sets one translation; `user` is what the server's history shows as its author
export async function submitTranslation(
  server: string,
  secret: string,
  user: string,
  lang: string,
  english: string,
  translation: string,
): Promise<void> {
  const params = new URLSearchParams({
    app: APP_NAME,
    lang,
    string: english,
    translation,
    secret,
    user,
  });
  const uri = `${server}/api/edittranslation?${params.toString()}`;
  const resp = await fetch(uri, { method: "GET" });
  if (!resp.ok) {
    const body = await resp.text();
    throw new Error(`edittranslation HTTP ${resp.status}: ${body.slice(0, 300)}`);
  }
  const text = await resp.text();
  if (!text.includes("OK")) {
    throw new Error(`edittranslation unexpected response: ${text.slice(0, 200)}`);
  }
}
