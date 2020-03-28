//const fs = require('fs');
const fg = require('fast-glob');
const path = require("path");

const pdf = require('pdfjs-dist/build/pdf.js');

const pdfPath = "x:\\books\\books\\StartSmallStaySmall.pdf";

function annotHasAppearance(annot) {
  return annot.hasAppearance;
}

function filterAnnots(annots) {
  if (!annots) {
    return null;
  }
  return annots.filter(annotHasAppearance);
}

function len(a) {
  if (!a) {
    return 0;
  }
  return a.length;
}

async function doPDF(filePath) {
  const doc = await pdf.getDocument(filePath).promise;
  const nPages = doc.numPages;
  let nAnnots = 0;
  for (let i = 1; i <= nPages; i++) {
    const page = await doc.getPage(i);
    let annots = await page.getAnnotations();
    annots = filterAnnots(annots);
    if (len(annots) == 0) {
      continue;
    }
    console.log("page:", i);
    console.log("annotations:", annots);
    nAnnots += annots.length;
  }
  if (nAnnots > 0) {
    console.log(`${filePath} has ${nPages} pages and ${nAnnots} annotations`);
  } else {
    console.log(`${filePath} has ${nPages} pages`);
  }
}

(async function main() {
  //await doPDF(pdfPath);
  const startDir = "x:\\books\\books";
  const opts = {
    cwd: startDir,
  }
  let nFiles = 0;
  const maxFiles = 1032;
  const stream = fg.stream("**/*.pdf", opts);
  for await (const name of stream) {
    const filePath = path.join(startDir, name);
    await doPDF(filePath);
    nFiles++;
    if (nFiles > maxFiles) {
      return;
    }
  }
})();
