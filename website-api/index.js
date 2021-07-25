/*
Respond to /api/updatecheck/* requests.
*/

addEventListener('fetch', event => {
  event.respondWith(handleRequest(event.request))
})

// TODO: cache in kv
async function handleRequest(request) {
  const url = new URL(request.url).pathname;
  let proxyURL = "";
  // TODO: also fetch from s3 and use one that returns first
  if (url === "/api/updatecheck/pre-release.txt") {
    proxyURL = "https://kjkpubsf.sfo2.digitaloceanspaces.com/software/sumatrapdf/sumpdf-prerelease-update.txt";
  } else if (url === "/api/updatecheck/release.txt") {
    proxyURL = "https://www.sumatrapdfreader.org/update-check-rel.txt"
  }

  let rsp = "";
  if (proxyURL !== "") {
    try {
      const s = await (await fetch(proxyURL)).text();
      rsp = s;
    } catch (ex) {
      const err = ex.toString();
      return new Response(err, {
        status: 500,
        headers: { 'content-type': 'text/plain' },
      })
    }
  } else {
    const cf = request.cf;
    rsp = `url: ${url}
City: ${cf.city}
Country: ${cf.country}`;  
  }

  return new Response(rsp, {
    headers: { 'content-type': 'text/plain' },
  })
}
