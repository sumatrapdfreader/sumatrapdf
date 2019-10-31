var Q = JSON.stringify

var pathPrinter = {
	moveTo: function (x,y) { print("moveTo", x, y) },
	lineTo: function (x,y) { print("lineTo", x, y) },
	curveTo: function (x1,y1,x2,y2,x3,y3) { print("curveTo", x1, y1, x2, y2, x3, y3) },
	closePath: function () { print("closePath") },
}

var textPrinter = {
	showGlyph: function (f,m,g,u,v,b) { print("glyph",f,m,g,u,v,b) },
}

var traceDevice = {
	fillPath: function (path, evenOdd, ctm, colorSpace, color, alpha) {
		print("fillPath", evenOdd, ctm, colorSpace, color, alpha)
		path.walk(pathPrinter)
	},
	clipPath: function (path, evenOdd, ctm) {
		print("clipPath", evenOdd, ctm)
		path.walk(pathPrinter)
	},
	strokePath: function (path, stroke, ctm, colorSpace, color, alpha) {
		print("strokePath", Q(stroke), ctm, colorSpace, color, alpha)
		path.walk(pathPrinter)
	},
	clipStrokePath: function (path, stroke, ctm) {
		print("clipStrokePath", Q(stroke), ctm)
		path.walk(pathPrinter)
	},

	fillText: function (text, ctm, colorSpace, color, alpha) {
		print("fillText", ctm, colorSpace, color, alpha)
		text.walk(textPrinter)
	},
	clipText: function (text, ctm) {
		print("clipText", ctm)
		text.walk(textPrinter)
	},
	strokeText: function (text, stroke, ctm, colorSpace, color, alpha) {
		print("strokeText", Q(stroke), ctm, colorSpace, color, alpha)
		text.walk(textPrinter)
	},
	clipStrokeText: function (text, stroke, ctm) {
		print("clipStrokeText", Q(stroke), ctm)
		text.walk(textPrinter)
	},
	ignoreText: function (text, ctm) {
		print("ignoreText", ctm)
		text.walk(textPrinter)
	},

	fillShade: function (shade, ctm, alpha) {
		print("fillShade", shade, ctm, alpha)
	},
	fillImage: function (image, ctm, alpha) {
		print("fillImage", image, ctm, alpha)
	},
	fillImageMask: function (image, ctm, colorSpace, color, alpha) {
		print("fillImageMask", image, ctm, colorSpace, color, alpha)
	},
	clipImageMask: function (image, ctm) {
		print("clipImageMask", image, ctm)
	},

	beginMask: function (area, luminosity, colorspace, color) {
		print("beginMask", area, luminosity, colorspace, color)
	},
	endMask: function () {
		print("endMask")
	},

	popClip: function () {
		print("popClip")
	},

	beginGroup: function (area, isolated, knockout, blendmode, alpha) {
		print("beginGroup", area, isolated, knockout, blendmode, alpha)
	},
	endGroup: function () {
		print("endGroup")
	},
	beginTile: function (area, view, xstep, ystep, ctm, id) {
		print("beginTile", area, view, xstep, ystep, ctm, id)
		return 0
	},
	endTile: function () {
		print("endTile")
	},

	close: function () {
		print("close")
	},
}

if (scriptArgs.length != 2)
	print("usage: mutool run trace-device.js document.pdf pageNumber")
else {
	var doc = new Document(scriptArgs[0]);
	var page = doc.loadPage(parseInt(scriptArgs[1])-1);
	page.run(traceDevice, Identity);
}
