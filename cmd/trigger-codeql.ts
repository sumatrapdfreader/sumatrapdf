const ghtoken = process.env["GITHUB_TOKEN"] ?? "";
if (!ghtoken) {
  console.error("need GITHUB_TOKEN env variable");
  process.exit(1);
}

const uri = "https://api.github.com/repos/sumatrapdfreader/sumatrapdf/dispatches";
const resp = await fetch(uri, {
  method: "POST",
  headers: {
    Accept: "application/vnd.github.everest-preview+json",
    Authorization: `token ${ghtoken}`,
  },
  body: JSON.stringify({ event_type: "codeql" }),
});

if (resp.status >= 400) {
  console.error(`Failed: HTTP ${resp.status} ${resp.statusText}`);
  process.exit(1);
}

console.log("Triggered codeql build");
