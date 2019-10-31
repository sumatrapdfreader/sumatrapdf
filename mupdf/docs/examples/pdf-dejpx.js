// Find all JPEG-2000 images and turn them into regular images.

var doc = new PDFDocument(scriptArgs[0]);

function isJPXImage(ref) {
	if ("Filter" in ref) {
		var filter = ref.Filter;
		if (filter == "JPXDecode")
			return true;
		if (filter.isArray())
			for (var i = 0; i < filter.length; ++i)
				if (filter[i] == "JPXDecode")
					return true;
	}
	return false;
}

var i, n, ref;

var jpxList = {};
var smaskList = {};

// Preload and destroy all JPX images.
n = doc.countObjects();
for (i=1; i < n; ++i) {
	ref = doc.newIndirect(i, 0);
	if (isJPXImage(ref)) {
		print("Loading JPX image:", i)
		jpxList[i] = doc.loadImage(ref);
		if ("SMask" in ref)
			smaskList[i] = ref.SMask;
		ref.writeObject(null); // make sure we don't reuse the JPX image resource
	}
}

for (i in jpxList) {
	ref = doc.newIndirect(i, 0);
	var jpx = jpxList[i];
	var pix = jpx.toPixmap();
	var raw = new Image(pix);

	// Create a new image, then copy the data to the old object, then delete it.
	print("Decompressed image:", i);
	var img = doc.addImage(raw);
	if (i in smaskList)
		img.SMask = smaskList[i];
	ref.writeObject(img.resolve());
	ref.writeRawStream(img.readRawStream());
	doc.deleteObject(img);

	// Invoke the GC to free intermediate pixmaps and images.
	gc();
}

doc.save(scriptArgs[1], "compress,garbage=compact");
