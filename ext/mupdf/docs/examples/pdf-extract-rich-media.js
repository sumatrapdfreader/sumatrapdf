// List and extract embedded rich media in a PDF document.

if (scriptArgs.length != 1 && scriptArgs.length != 3) {
	print("usage: mutool run pdf-extract-rich-media.js input.pdf [index filename]");
	print("    List embedded rich media, or extract an embedded rich media file from a PDF document.")
	quit();
}

var doc = Document.openDocument(scriptArgs[0]);

function mapNameTree(N, fn) {
	function mapNameTreeNames(NN) {
		var i, n = NN.length;
		for (i = 0; i < n; i += 2)
			fn(NN[i], NN[i+1]);
	}
	function mapNameTreeKids(NK) {
		var i, n = NK.length;
		for (i = 0; i < n; ++i)
			mapNameTree(NK[i], fn)
	}
	if ("Names" in N)
		mapNameTreeNames(N.Names);
	if ("Kids" in N)
		mapNameTreeKids(N.Kids);
}

function fileNameFromFS(fs) {
	if ("UF" in fs) return fs.UF.asString();
	if ("F" in fs) return fs.F.asString();
	if ("Unix" in fs) return fs.Unix.asString();
	if ("DOS" in fs) return fs.DOS.asString();
	if ("Mac" in fs) return fs.Mac.asString();
	return "Untitled";
}

function mapRichMediaAssets(fn) {
	var pageCount = doc.countPages();
	var page, annots, a;
	for (page = 0; page < pageCount; ++page) {
		annots = doc.findPage(page).Annots;
		if (annots && annots.length > 0) {
			for (a = 0; a < annots.length; ++a) {
				if (annots[a].Subtype == "RichMedia")
					mapNameTree(annots[a].RichMediaContent.Assets, fn);
			}
		}
	}
}

if (scriptArgs.length == 1) {
	var idx = 1;
	mapRichMediaAssets(function (name, fs) {
		print(idx, name.asString());
		print("\tFilename:", fileNameFromFS(fs));
		if ("Desc" in fs)
			print("\tDescription:", fs.Desc.asString());
		++idx;
	});
}

if (scriptArgs.length == 3) {
	var idx = 1;
	mapRichMediaAssets(function (name, fs) {
		if (idx == scriptArgs[1]) {
			print("Saving embedded file", idx, "as:", scriptArgs[2]);
			fs.EF.F.readStream().save(scriptArgs[2]);
		}
		++idx;
	});
}
