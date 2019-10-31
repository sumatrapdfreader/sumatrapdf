function BBoxDevice(bbox) {
	function extend(x,y) {
		if (x < bbox[0]) bbox[0] = x;
		if (x > bbox[2]) bbox[2] = x;
		if (y < bbox[1]) bbox[1] = y;
		if (y > bbox[3]) bbox[3] = y;
	}
	function extendPoint(m, px, py) {
		var x = px * m[0] + py * m[2] + m[4];
		var y = px * m[1] + py * m[3] + m[5];
		extend(x, y);
	}
	function extendRect(m, r) {
		var x0 = r[0], y0 = r[1];
		var x1 = r[2], y1 = r[3];
		extendPoint(m, x0, y0);
		extendPoint(m, x1, y0);
		extendPoint(m, x0, y1);
		extendPoint(m, x1, y1);
	}
	function PathBounder(ctm) {
		return {
			moveTo: function (x,y) { extendPoint(ctm, x, y) },
			lineTo: function (x,y) { extendPoint(ctm, x, y) },
			curveTo: function (x1,y1,x2,y2,x3,y3) {
				extendPoint(ctm, x1, y1);
				extendPoint(ctm, x2, y2);
				extendPoint(ctm, x3, y3);
			},
		};
	}
	function TextBounder(ctm) {
		return {
			showGlyph: function (font,trm,gid,ucs,wmode,bidi) {
				var bbox = [ 0, -0.2, font.advanceGlyph(gid, 0), 0.8 ];
				extendRect(Concat(trm, ctm), bbox);
			},
		};
	}
	return {
		fillPath: function (path, evenOdd, ctm, colorSpace, color, alpha) {
			path.walk(new PathBounder(ctm));
		},
		clipPath: function (path, evenOdd, ctm) {
			path.walk(new PathBounder(ctm));
		},
		strokePath: function (path, stroke, ctm, colorSpace, color, alpha) {
			path.walk(new PathBounder(ctm));
		},
		clipStrokePath: function (path, stroke, ctm) {
			path.walk(new PathBounder(ctm));
		},
		fillText: function (text, ctm, colorSpace, color, alpha) {
			text.walk(new TextBounder(ctm));
		},
		clipText: function (text, ctm) {
			text.walk(new TextBounder(ctm));
		},
		strokeText: function (text, stroke, ctm, colorSpace, color, alpha) {
			text.walk(new TextBounder(ctm));
		},
		clipStrokeText: function (text, stroke, ctm) {
			text.walk(new TextBounder(ctm));
		},
		ignoreText: function (text, ctm) {
			text.walk(new TextBounder(ctm));
		},
		fillShade: function (shade, ctm, alpha) {
			var bbox = shade.bound(ctm);
			extend(bbox[0], bbox[1]);
			extend(bbox[2], bbox[3]);
		},
		fillImage: function (image, ctm, alpha) {
			extendRect(ctm, [0,0,1,1]);
		},
		fillImageMask: function (image, ctm, colorSpace, color, alpha) {
			extendRect(ctm, [0,0,1,1]);
		},
	};
}

if (scriptArgs.length != 2)
	print("usage: mutool run bbox-device.js document.pdf pageNumber")
else {
	var doc = new Document(scriptArgs[0]);
	var page = doc.loadPage(parseInt(scriptArgs[1])-1);
	var bbox = [Infinity, Infinity, -Infinity, -Infinity];
	page.run(new BBoxDevice(bbox), Identity);
	print("original bbox:", page.bound());
	print("computed bbox:", bbox.map(function (x) { return Math.round(x); }));
}
