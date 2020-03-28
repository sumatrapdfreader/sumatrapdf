const fs = require('fs');

const pdf  = require('pdfjs-dist/build/pdf.js');

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

async function doPDF(path) {
  const doc = await pdf.getDocument(path).promise;
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
    console.log(`${path} has ${nPages} pages and ${nAnnots} annotations`);
  } else {
    console.log(`${path} has ${nPages} pages`);
  }
}

(async function main() {
  await doPDF(pdfPath);
})();
