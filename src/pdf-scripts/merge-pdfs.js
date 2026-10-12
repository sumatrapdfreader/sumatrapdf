// Adapted from MuPDF documentation; arguments: input, output, optional extras.
// A re-implementation of "sumatrapdf-tool merge" in JavaScript.

function copyPage(dstDoc, srcDoc, pageNumber, dstFromSrc) {
  var srcPage, dstPage;
  srcPage = srcDoc.findPage(pageNumber);
  dstPage = dstDoc.newDictionary();
  dstPage.Type = dstDoc.newName("Page");
  if (srcPage.MediaBox) dstPage.MediaBox = dstFromSrc.graftObject(srcPage.MediaBox);
  if (srcPage.Rotate) dstPage.Rotate = dstFromSrc.graftObject(srcPage.Rotate);
  if (srcPage.Resources) dstPage.Resources = dstFromSrc.graftObject(srcPage.Resources);
  if (srcPage.Contents) dstPage.Contents = dstFromSrc.graftObject(srcPage.Contents);
  dstDoc.insertPage(-1, dstDoc.addObject(dstPage));
}

function copyAllPages(dstDoc, srcDoc) {
  var dstFromSrc = dstDoc.newGraftMap();
  var k,
    n = srcDoc.countPages();
  for (k = 0; k < n; ++k) copyPage(dstDoc, srcDoc, k, dstFromSrc);
}

function pdfmerge() {
  var srcDoc, dstDoc, i;

  dstDoc = new PDFDocument();
  for (i = 0; i < scriptArgs.length; ++i) {
    if (i == 1) continue;
    srcDoc = Document.openDocument(scriptArgs[i]);
    copyAllPages(dstDoc, srcDoc);
  }
  dstDoc.save(scriptArgs[1], "compress");
}

if (scriptArgs.length < 3) print("arguments: input1.pdf output.pdf input2.pdf ...");
else pdfmerge();
