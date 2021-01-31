// update after releasing a new version
var gSumVer = "3.2";

// used by download-prev* pages, update after releasing a new version
var gPrevSumatraVersion = [
  "3.1.2",
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
var dlPrefix2 = "/dl2/";
var host = "https://www.sumatrapdfreader.org";

function a(href, txt) {
  return '<a href="' + href + '">' + txt + "</a>";
}

function dlURL(ver, name) {
  if (ver >= "3.2") {
    return dlPrefix2 + name;
  }
  return dlPrefix + name;
}

function installerHref(ver) {
  var name = "SumatraPDF-" + ver + "-install.exe";
  var url = dlURL(ver, name);
  return a(url, name);
}

function installerHref2(ver) {
  var name = "SumatraPDF-" + ver + "-install.exe";
  var url = dlURL(ver, name);
  return host + url;
}

function zipHref(ver) {
  var name = "SumatraPDF-" + ver + ".zip";
  var url = dlURL(ver, name);
  return a(url, name);
}

function zipHref2(ver) {
  var name = "SumatraPDF-" + ver + ".zip";
  var url = dlURL(ver, name);
  return host + url;
}

function installer64Href(ver) {
  var name = "SumatraPDF-" + ver + "-64-install.exe";
  var url = dlURL(ver, name);
  return a(url, name);
}

function installer64Href2(ver) {
  var name = "SumatraPDF-" + ver + "-64-install.exe";
  var url = dlURL(ver, name);
  return host + url;
}

function zip64Href(ver) {
  var name = "SumatraPDF-" + ver + "-64.zip";
  var url = dlURL(ver, name);
  return a(url, name);
}

function zip64Href2(ver) {
  var name = "SumatraPDF-" + ver + "-64.zip";
  var url = dlURL(ver, name);
  return host + url;
}

var gSumExeName = "SumatraPDF-" + gSumVer + "-install.exe";
var gSumZipName = "SumatraPDF-" + gSumVer + ".zip";
var gSumExeUrl = dlPrefix2 + gSumExeName;
var gSumZipUrl = dlPrefix2 + gSumZipName;

var gSumExeName64 = "SumatraPDF-" + gSumVer + "-64-install.exe";
var gSumZipName64 = "SumatraPDF-" + gSumVer + "-64.zip";
var gSumExeUrl64 = dlPrefix2 + gSumExeName64;
var gSumZipUrl64 = dlPrefix2 + gSumZipName64;

// used by downloadafter*.html pages
function dlAfterHtml(s1, s2, s3, s4) {
  return (
    '<a href="' + gSumExeUrl + '">' + s1 + "</a>" + s2 +
    '<a href="' + gSumZipUrl + '">' + s3 + "</a>" + s4
  );
}

function dlAfterHtml64(s1, s2, s3, s4) {
  return (
    '<a href="' + gSumExeUrl64 + '">' + s1 + "</a>" + s2 +
    '<a href="' + gSumZipUrl64 + '">' + s3 + "</a>" + s4
  );
}

// ie 11 doesn't have String.startsWith
function startsWith(s, prefix) {
  return s.slice(0, prefix.length) === prefix;
}

// given /free-pdf-reader.html returns free-pdf-reader
// special case for /docs/*.html => docs
function getBaseUrl() {
  var loc = location.pathname; // '/free-pdf-reader.html etc.
  if (startsWith(loc, "/docs/") || startsWith(loc, "docs/")) {
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
  //["online", "Online"],
  ["backers", "Support SumatraPDF"],
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
        "/docs/SumatraPDF-documentation.html";
    } else if (currUrl == "forum") {
      url = "https://forum.sumatrapdfreader.org";
    } else if (currUrl == "online") {
      url = "https://online.sumatrapdfreader.org/";
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
function prevDownloadsList2() {
  var s = "";
  for (var i = 0; i < gPrevSumatraVersion.length; i++) {
    var ver = gPrevSumatraVersion[i];
    s += "<p>";
    s += "<b>Version: " + ver + "</b><br>\n";
    s += "Installer" + ": " + installerHref2(ver) + "<br>\n";
    s += "Zip file" + ": " + zipHref2(ver);
    if (verNewerOrEqThan31(ver)) {
      s += "<br>" + "Installer" + " 64-bit: " + installer64Href2(ver) + "<br>\n";
      s += "Zip file" + " 64-bit: " + zip64Href2(ver);
    }
    s += "</p>\n";
  }
  return s;
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

var allShots = [
  "img/homepage.png",
  "img/format-pdf.png",
  "img/format-epub.png",
  "img/menu-file.png",
  "img/menu-view.png",
  "img/dialog-langs.png",
];

var shotDescriptions = [
  "Homepage",
  "supports PDF format, tabbed interface",
  "supports eBook (EPUB and MOBI) formats",
  "file menu",
  "multiple types of views",
  "translated to multiple languages",
]
var currImg = "img/homepage.png";

function getEl(id) {
  if (id[0] == "#") {
    id = id.substr(1);
  }
  return document.getElementById(id);
}

function getImgIdx(img) {
  var n = allShots.length;
  for (var i = 0; i < n; i++) {
    if (img == allShots[i]) {
      return i;
    }
  }
  return 0;
}

function changeShot(imgUrl) {
  currImg = imgUrl;

  var el = getEl("main-shot");
  el.setAttribute("src", currImg);

  var n = allShots.length;
  var isFirstImage = false;
  var isLastImage = false;
  for (var i = 0; i < n; i++) {
    // get id from image path i.e. "img/homepage.png" => "homepage.png"
    var uri = allShots[i];
    var id = uri.split("/")[1];
    el = getEl(id);
    if (uri == imgUrl) {
      isFirstImage = i == 0;
      isLastImage = i == n - 1;
      el.classList.add("selected-img");
      el = getEl("shot-description");
      var desc = shotDescriptions[i];
      el.textContent = desc;
    } else {
      el.classList.remove("selected-img");
    }
  }
}

function advanceImage(n) {
  var idx = getImgIdx(currImg) + n;
  if (idx < 0) {
    idx = allShots.length + idx;
  } else {
    idx = idx % allShots.length;
  }
  changeShot(allShots[idx]);
}

function imgNext() {
  advanceImage(1);
}

function imgPrev() {
  advanceImage(-1);
}