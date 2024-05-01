//import pdf from 'pdfjs-dist';
//import { join } from "path";
//import { stream } from 'fast-glob';

const fs = require("fs");
const fg = require("fast-glob");
const path = require("path");

import * as pdf from "pdfjs-dist/build/pdf";

console.log(pdf);
//console.log(pdf.Util);
//console.log(new pdf.Util());
//console.log(Object.keys(pdf.Util));
//const pdfUtil = pdf.Util();
//console.log(pdfUtil);
//pdfUtil.setVerbosityLevel(0);
//pdf.util.setVerbosityLevel(0);

let startDir = "x:\\books\\books";
// if destDir is "", then don't copy
let destDir = "c:\\Users\\kjk\\sumtest\\files-with-annots";

//startDir = "c:\\Users\\kjk\\sumtest\\files-with-annots";
//destDir = "";

let fileNo = 0;
const maxFiles = 512;

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

let withAnnots = [];
function dumpWithAnnots() {
  console.log("\n\n");
  console.log("dumpWithAnnots:");
  let n = 1;
  for (let o of withAnnots) {
    const { filePath, nAnnots, nPages } = o;
    console.log(
      `${n}: ${filePath} has ${nPages} pages and ${nAnnots} annotations`
    );
    n++;
  }
}

let annotsByType = {};

function recordAnnotation(filePath, pageNo, page, annot) {
  const atp = annot.annotationType;
  if (!atp) {
    console.log("bad annotation:", annot);
    return;
  }
  const el = [filePath, pageNo, page, annot];
  let byType = annotsByType[atp];
  if (byType) {
    byType.push(el);
  } else {
    annotsByType[atp] = [el];
  }
}

function recordAnnotations(filePath, pageNo, page, annots) {
  for (let annot of annots) {
    recordAnnotation(filePath, pageNo, page, annot);
  }
}

const annotationTypes = [
  "TEXT",
  "LINK",
  "FREETEXT",
  "LINE",
  "SQUARE",
  "CIRCLE",
  "POLYGON",
  "POLYLINE",
  "HIGHLIGHT",
  "UNDERLINE",
  "SQUIGGLY",
  "STRIKEOUT",
  "STAMP",
  "CARET",
  "INK",
  "POPUP",
  "FILEATTACHMENT",
  "SOUND",
  "MOVIE",
  "WIDGET",
  "SCREEN",
  "PRINTERMARK",
  "TRAPNET",
  "WATERMARK",
  "THREED",
  "REDACT",
];

function getAnnotName(annotType) {
  const idx = annotType - 1;
  if (idx < 0 || idx >= len(annotationTypes)) {
    return "unkown annotation type";
  }
  return annotationTypes[idx];
}

function dumpAnnotsByTypeAndMaybeCopy(dstDir) {
  console.log("\n");
  //console.log(annotsByType);
  const keys = Object.keys(annotsByType);
  let maxFiles = 8;
  for (let annotType of keys) {
    const els = annotsByType[annotType];
    let n = len(els);
    const annotName = getAnnotName(annotType);
    console.log(`\nAnnotation ${annotType} (${annotName}), ${n} occurences`);
    let prevFilePath = "";
    let prevPageNo = -1;
    let nPrintedFiles = 0;
    for (let i = 0; i < n; i++) {
      const el = els[i];
      const filePath = el[0];
      const pageNo = el[1];
      // only print filePath once
      if (filePath != prevFilePath) {
        nPrintedFiles++;
        if (nPrintedFiles >= maxFiles) {
          break;
        }
        if (dstDir != "") {
          const fileName = path.basename(filePath);
          const dstName = `${annotType}-${fileName}`;
          const dstPath = path.join(dstDir, dstName);
          fs.copyFileSync(filePath, dstPath);
        }
        prevPageNo = -1;
        console.log(`  ${filePath}, page: ${pageNo}`);
      } else {
        if (prevPageNo != pageNo) {
          process.stdout.write(`, ${pageNo}`);
          // console.log(`      page: ${pageNo}`)
        }
      }
      prevPageNo = pageNo;
      prevFilePath = filePath;
    }
  }
}

async function doPDF(filePath) {
  var ab = fs.readFileSync(filePath, null).buffer;
  const params = {
    data: ab,
    verbosity: 0,
  };
  const doc = await pdf.getDocument(params).promise;
  const nPages = doc.numPages;
  let nAnnots = 0;
  for (let i = 1; i <= nPages; i++) {
    const page = await doc.getPage(i);
    let annots = await page.getAnnotations();
    annots = filterAnnots(annots);
    if (len(annots) == 0) {
      continue;
    }
    //console.log("page:", i);
    //console.log("annotations:", annots);
    nAnnots += annots.length;
    recordAnnotations(filePath, i, page, annots);
  }
  if (nAnnots > 0) {
    console.log(
      `${fileNo}: ${filePath} has ${nPages} pages and ${nAnnots} annotations`
    );
    const o = {
      filePath: filePath,
      nAnnots: nAnnots,
      nPages: nPages,
    };
    withAnnots.push(o);
  } else {
    console.log(`  ${fileNo}: ${filePath} has ${nPages} pages`);
  }
}

(async function main() {
  if (false) {
    const pdfPath = "v:\\sumtest\\annots\\1-rooms2001.pdf";
    await doPDF(pdfPath);
    return;
  }
  const opts = {
    cwd: startDir,
  };
  fileNo = 1;
  const stream = fg.stream("**/*.pdf", opts);
  for await (const name of stream) {
    const filePath = path.join(startDir, name);
    try {
      await doPDF(filePath);
    } catch (err) {
      console.log(`${filePath} failed with ${err}`);
      //throw err;
    }
    fileNo++;
    if (fileNo > maxFiles) {
      break;
    }
  }

  dumpWithAnnots();
  dumpAnnotsByTypeAndMaybeCopy(destDir);
})();
