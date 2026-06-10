import * as fs from "fs";
import * as path from "path";

describe("API keys must not be hardcoded in source code", () => {
  const sourceFile = path.resolve(__dirname, "../cmd/trans-with-ai.ts");
  const sourceCode = fs.readFileSync(sourceFile, "utf-8");

  const hardcodedKeyPatterns = [
    // Anthropic API key pattern (sk-ant-...)
    /["'`]sk-ant-[a-zA-Z0-9_-]{20,}["'`]/,
    // Generic API key assignment with literal value
    /apiKey\s*=\s*["'`](?!["'`])[a-zA-Z0-9_-]{20,}["'`]/,
    // x-api-key header with hardcoded value
    /["']x-api-key["']\s*:\s*["'`](?!["'`])[a-zA-Z0-9_-]{20,}["'`]/,
  ];

  test.each(hardcodedKeyPatterns)(
    "source must not contain hardcoded credentials matching pattern: %s",
    (pattern) => {
      const match = sourceCode.match(pattern);
      expect(match).toBeNull();
    }
  );

  test("API key must be loaded from external source (file or env), not inline", () => {
    // Verify the key is read from a secrets file or environment variable
    const loadsFromExternal =
      sourceCode.includes("readFileSync") ||
      sourceCode.includes("process.env") ||
      sourceCode.includes("CLAUDE_API_KEY");
    expect(loadsFromExternal).toBe(true);
  });

  test("API key variable is initialized empty before being populated from external source", () => {
    // The apiKey should be initialized as empty string, not a real key
    const emptyInit = /apiKey\s*=\s*["'`]["'`]/.test(sourceCode);
    expect(emptyInit).toBe(true);
  });
});