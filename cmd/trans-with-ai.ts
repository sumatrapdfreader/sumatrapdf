import { join } from "path";

const scriptDir = import.meta.dir;
const rootDir = join(scriptDir, "..");

// extract lang codes and names by parsing trans-gen.ts gLangs array
function parseLangs(text: string): { code: string; name: string }[] {
  const langs: { code: string; name: string }[] = [];
  // match lines like: ["af", "Afrikaans", ...]
  const re = /\["([^"]+)",\s*"([^"]+)"/g;
  let m: RegExpExecArray | null;
  while ((m = re.exec(text)) !== null) {
    const name = m[2].replace(/\s*\(.*\)$/, "");
    langs.push({ code: m[1], name });
  }
  return langs;
}

async function main() {
  // read language codes from trans-gen.ts
  const transGenPath = join(scriptDir, "trans-gen.ts");
  const transGenText = await Bun.file(transGenPath).text();
  // extract only within the gLangs array
  const startIdx = transGenText.indexOf("const gLangs");
  const endIdx = transGenText.indexOf("];", startIdx);
  const gLangsText = transGenText.substring(startIdx, endIdx);
  const allLangs = parseLangs(gLangsText);

  // exclude "en" since it's the source language
  const langs = allLangs.filter((l) => l.code !== "en");
  const langCodes = langs.map((l) => l.code);
  const langCodesSet = new Set(langCodes);
  const langNameByCode = new Map(langs.map((l) => [l.code, l.name]));

  // read translations.txt
  const transPath = join(rootDir, "translations", "translations.txt");
  const transText = await Bun.file(transPath).text();
  const lines = transText.split("\n");

  // parse translations
  // format: lines starting with ":" are English phrases
  // followed by lines like "lang-code:translation"
  let currentEnglish = "";
  const missing: Map<string, string[]> = new Map();

  for (const line of lines) {
    if (line.startsWith(":")) {
      // new English phrase
      currentEnglish = line; // includes the leading ":"
      if (!missing.has(currentEnglish)) {
        missing.set(currentEnglish, [...langCodes]);
      }
    } else if (currentEnglish !== "") {
      const colonIdx = line.indexOf(":");
      if (colonIdx > 0) {
        const code = line.substring(0, colonIdx);
        if (langCodesSet.has(code)) {
          const arr = missing.get(currentEnglish)!;
          const idx = arr.indexOf(code);
          if (idx !== -1) {
            arr.splice(idx, 1);
          }
        }
      }
    }
  }

  // group missing translations by language
  const missingByLang = new Map<string, string[]>();
  for (const code of langCodes) {
    missingByLang.set(code, []);
  }
  for (const [english, codes] of missing) {
    for (const code of codes) {
      missingByLang.get(code)!.push(english);
    }
  }

  // read Claude API key from secrets file
  const secretsPath = join(rootDir, "..", "hack", "secrets", "sumatrapdf.env");
  const secretsText = await Bun.file(secretsPath).text();
  let apiKey = "";
  for (const sline of secretsText.split("\n")) {
    const eqIdx = sline.indexOf("=");
    if (eqIdx > 0 && sline.substring(0, eqIdx).trim() === "CLAUDE_API_KEY") {
      apiKey = sline.substring(eqIdx + 1).trim();
      break;
    }
  }
  if (!apiKey) {
    console.error("CLAUDE_API_KEY not found in " + secretsPath);
    process.exit(1);
  }

  const outDir = join(rootDir, "translations");

  for (const code of langCodes) {
    const rawStrings = missingByLang.get(code)!;
    if (rawStrings.length === 0) continue;

    const langName = langNameByCode.get(code)!;
    // strip leading ":" and remove "&" characters
    const phrases = rawStrings.map((s) =>
      s.replace(/^:/, "").replaceAll("&", "")
    );

    console.log(`Translating ${phrases.length} phrases to ${langName} (${code})...`);

    const prompt = `Translate the following English phrases to ${langName} (language code: ${code}).
Return a JSON object where keys are the original English phrases and values are the translations.
Return ONLY the JSON object, no other text.

Phrases to translate:
${JSON.stringify(phrases, null, 2)}`;

    const resp = await fetch("https://api.anthropic.com/v1/messages", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        "x-api-key": apiKey,
        "anthropic-version": "2023-06-01",
      },
      body: JSON.stringify({
        model: "claude-sonnet-4-5-20250929",
        max_tokens: 4096,
        messages: [{ role: "user", content: prompt }],
      }),
    });

    if (!resp.ok) {
      const errText = await resp.text();
      console.error(`API error for ${code}: ${resp.status} ${errText}`);
      continue;
    }

    const data = await resp.json();
    const text = data.content[0].text;

    // extract JSON from response (handle possible markdown code blocks)
    let jsonStr = text;
    const jsonMatch = text.match(/```(?:json)?\s*([\s\S]*?)```/);
    if (jsonMatch) {
      jsonStr = jsonMatch[1].trim();
    }

    const outPath = join(outDir, `${code}.trans.json`);
    await Bun.write(outPath, jsonStr);
    console.log(`Saved ${outPath}`);
  }
}

main();
