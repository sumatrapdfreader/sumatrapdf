// update after releasing a new version
var gSumVer = "3.1.2";

// used by download-prev* pages, update after releasing a new version
var gPrevSumatraVersion = [
  "3.1.1",
  "3.1",
  "3.0",
  "2.5.2",
  "2.5.1",
  "2.5",
  "2.4",
  "2.3.2",
  "2.3.1",
  "2.3",
  "2.2.1",
  "2.2",
  "2.1.1",
  "2.1",
  "2.0.1",
  "2.0",
  "1.9",
  "1.8",
  "1.7",
  "1.6",
  "1.5.1",
  "1.5",
  "1.4",
  "1.3",
  "1.2",
  "1.1",
  "1.0.1",
  "1.0",
  "0.9.4",
  "0.9.3",
  "0.9.1",
  "0.9",
  "0.8.1",
  "0.8",
  "0.7",
  "0.6",
  "0.5",
  "0.4",
  "0.3",
  "0.2"
];

var dlPrefix = "/dl/";

function a(href, txt) {
  return '<a href="' + href + '">' + txt + "</a>";
}

function installerHref(ver) {
  var txt = "SumatraPDF-" + ver + "-install.exe";
  var url = dlPrefix + txt;
  return a(url, txt);
}

function zipHref(ver) {
  var txt = "SumatraPDF-" + ver + ".zip";
  var url = dlPrefix + txt;
  return a(url, txt);
}

function installer64Href(ver) {
  var txt = "SumatraPDF-" + ver + "-64-install.exe";
  var url = dlPrefix + txt;
  return a(url, txt);
}

function zip64Href(ver) {
  var txt = "SumatraPDF-" + ver + "-64.zip";
  var url = dlPrefix + txt;
  return a(url, txt);
}

var gSumExeName = "SumatraPDF-" + gSumVer + "-install.exe";
var gSumZipName = "SumatraPDF-" + gSumVer + ".zip";
var gSumExeUrl = dlPrefix + gSumExeName;
var gSumZipUrl = dlPrefix + gSumZipName;

var gSumExeName64 = "SumatraPDF-" + gSumVer + "-64-install.exe";
var gSumZipName64 = "SumatraPDF-" + gSumVer + "-64.zip";
var gSumExeUrl64 = dlPrefix + gSumExeName64;
var gSumZipUrl64 = dlPrefix + gSumZipName64;

// used by download-free-pdf-viewer*.html pages
function dlHtml(s1, s2, s3) {
  if (!s3) {
    s3 = "";
  } else {
    s3 = " <span style='font-size:90%; color:gray'>" + s3 + "</span>";
  }
  return (
    "<table><tr><td>" +
    s1 +
    '&nbsp;&nbsp;</td><td><a href="' +
    gSumExeUrl +
    '" onclick="return SetupRedirect()">' +
    gSumExeName +
    "</a></td></tr><tr><td>" +
    s2 +
    '&nbsp;&nbsp;</td><td><a href="' +
    gSumZipUrl +
    '" onclick="return SetupRedirect()">' +
    gSumZipName +
    "</a>" +
    s3 +
    "</td></tr></table>"
  );
}

function dlHtml64(s1, s2, s3) {
  if (!s3) {
    s3 = "";
  } else {
    s3 = " <span style='font-size:90%; color:gray'>" + s3 + "</span>";
  }
  return (
    "<table><tr><td>" +
    s1 +
    '&nbsp;&nbsp;</td><td><a href="' +
    gSumExeUrl64 +
    '" onclick="return SetupRedirect()">' +
    gSumExeName64 +
    "</a></td></tr><tr><td>" +
    s2 +
    '&nbsp;&nbsp;</td><td><a href="' +
    gSumZipUrl64 +
    '" onclick="return SetupRedirect()">' +
    gSumZipName64 +
    "</a>" +
    s3 +
    "</td></tr></table>"
  );
}

// used by downloadafter*.html pages
function dlAfterHtml(s1, s2, s3, s4) {
  return (
    '<a href="' +
    gSumExeUrl +
    '">' +
    s1 +
    "</a>" +
    s2 +
    '<a href="' +
    gSumZipUrl +
    '">' +
    s3 +
    "</a>" +
    s4
  );
}

function dlAfterHtml64(s1, s2, s3, s4) {
  return (
    '<a href="' +
    gSumExeUrl64 +
    '">' +
    s1 +
    "</a>" +
    s2 +
    '<a href="' +
    gSumZipUrl64 +
    '">' +
    s3 +
    "</a>" +
    s4
  );
}

// given /free-pdf-reader.html returns free-pdf-reader
// special case for /docs/*.html => docs
function getBaseUrl() {
  var loc = location.pathname; // '/free-pdf-reader.html etc.
  if (loc.startsWith("/docs/")) {
    return "docs";
  }
  var url = loc.split("/");
  url = url[url.length - 1];
  url = url.split(".html")[0];
  return url;
}

/*
	Construct html as below, filling the apropriate inter-language links.
	<div id="ddcolortabs">
		<ul>
			<li id="current"><a href="free-pdf-reader.html" title="Home"><span>Home</span></a></li>
			<li><a href="news.html" title="News"><span>News</span></a></li>
			<li><a href="manual.html" title="Manual"><span>Manual</span></a></li>
			<li><a href="docs.html" title="Documentation"><span>Documentation</span></a></li>
			<li><a href="download-free-pdf-viewer.html" title="Download"><span>Download</span></a></li>
			<li><a href="forum.html" title="Forums"><span>Forum</span></a></li>
		</ul>
	</div>
	<div id="ddcolortabsline"> </div>
*/
var baseUrls = [
  ["free-pdf-reader", "Home"],
  ["download-free-pdf-viewer", "Download"],
  ["manual", "Manual"],
  ["docs", "Documentation"],
  ["news", "Version History"],
  ["forum", "Discussion Forum"]
];

function navHtml() {
  var baseUrl = getBaseUrl();

  var s = '<div id="ddcolortabs"><ul>';
  for (var i = 0; i < baseUrls.length; i++) {
    var currUrl = baseUrls[i][0];
    if (currUrl == baseUrl) {
      s += '<li id="current">';
    } else {
      s += "<li>";
    }
    var url = "/" + currUrl + ".html";
    var txt = baseUrls[i][1];
    if (currUrl == "docs") {
      url =
        "/docs/SumatraPDF-documentation-fed36a5624d443fe9f7be0e410ecd715.html";
    }
    if (currUrl == "forum") {
      url = "https://forum.sumatrapdfreader.org";
    }
    s +=
      '<a href="' +
      url +
      '" title="' +
      txt +
      '"><span>' +
      txt +
      "</span></a></li>";
  }
  s += '</ul></div><div id="ddcolortabsline"> </div>';
  return s;
}

function verNewerOrEqThan31(ver) {
  var parts = ver.split(".");
  var major = parseInt(parts[0]);
  if (major > 3) {
    return true;
  }
  if (major < 3 || parts.length < 2) {
    return false;
  }
  var minor = parseInt(parts[1]);
  return minor >= 1;
}

// used by download-prev* pages
function prevDownloadsList() {
  var s = "";
  for (var i = 0; i < gPrevSumatraVersion.length; i++) {
    var ver = gPrevSumatraVersion[i];
    s += "<p>";
    s += "Installer" + ": " + installerHref(ver) + "<br>\n";
    s += "Zip file" + ": " + zipHref(ver);
    if (verNewerOrEqThan31(ver)) {
      s += "<br>" + "Installer" + " 64-bit: " + installer64Href(ver) + "<br>\n";
      s += "Zip file" + " 64-bit: " + zip64Href(ver);
    }
    s += "</p>\n";
  }
  return s;
}

function httpsRedirect() {
  if (window.location.protocol !== "http:") {
    return;
  }
  if (window.location.hostname !== "www.sumatrapdfreader.org") {
    return;
  }
  var uri = window.location.toString();
  uri = uri.replace("http://", "https://");
  window.location = uri;
}

httpsRedirect();
