// Copyright (C) 2004-2022 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

"use strict";

const { assert } = require("chai");
const fs = require("fs/promises");
const { Rect } = require("../lib/mupdf.js");
const mupdf = require("../lib/mupdf.js");

describe("mupdf", function () {
	let input;
	beforeAll(async function () {
		input = await fs.readFile("samples/annotations_galore_II.pdf");
	});

	beforeAll(async function () {
		await mupdf.ready;
	});

	describe.skip("geometry", function () {
		describe("Matrix", function () {
			it("should transform Rect", function () {
				const matrix = mupdf.Matrix.scale(3, 2);
				const rect = new mupdf.Rect(10, 10, 20, 20);

				assert.deepEqual(matrix.transformRect(rect), new mupdf.Rect(30, 20, 31, 40));
			});
		});
	});

	describe("Document", function () {
		describe("openFromJsBuffer()", function () {
			it("should return a valid Document", function () {
				let doc = mupdf.Document.openFromJsBuffer(input, "application/pdf");

				assert.isNotNull(doc);
				assert.equal(doc.countPages(), 3);
				assert.equal(doc.title(), "");
			});
		});

		describe("openFromStream()", function () {
			it("should return a valid Document", function () {
				let stream = mupdf.Stream.fromJsBuffer(input);
				let doc = mupdf.Document.openFromStream(stream, "application/pdf");

				assert.isNotNull(doc);
				assert.equal(doc.countPages(), 3);
				assert.equal(doc.title(), "");
			});
		});

		describe("loadPage()", function () {
			it("should return a valid Page", function () {
				let doc = mupdf.Document.openFromJsBuffer(input, "application/pdf");
				let page = doc.loadPage(0);

				assert.isNotNull(page);
				assert.instanceOf(page, mupdf.PdfPage);
				assert.deepEqual(page.bounds(), new mupdf.Rect(0, 0, 612, 792));
				assert.equal(page.width(), 612);
				assert.equal(page.height(), 792);
			});

			it("should throw on OOB", function () {
				let doc = mupdf.Document.openFromJsBuffer(input, "application/pdf");
				assert.throws(() => doc.loadPage(500), mupdf.MupdfError);
				assert.throws(() => doc.loadPage(-1), mupdf.MupdfError);
			});
		});

		describe("loadOutline()", function () {
			it("should return a null Outline if document doesn't have one", function () {
				let doc = mupdf.Document.openFromJsBuffer(input, "application/pdf");
				let outline = doc.loadOutline();

				assert.isNull(outline);
			});

			// TODO - non-null outline
		});
	});

	describe("Page", function () {
		let doc;
		let page;
		beforeAll(function () {
			doc = mupdf.Document.openFromJsBuffer(input, "application/pdf");
			page = doc.loadPage(0);
		});
		afterAll(function () {
			doc?.free();
			page?.free();
		});

		describe("toPixmap()", function () {
			it("should return a valid Pixmap", function () {
				let pixmap = page.toPixmap(new mupdf.Matrix(1,0,0,1,0,0), mupdf.DeviceRGB, false);

				assert.isNotNull(pixmap);
				assert.equal(pixmap.width(), 612);
				assert.equal(pixmap.height(), 792);
			});
		});

		describe("toSTextPage()", function () {
			it("should return a valid STextPage", function () {
				let stextPage = page.toSTextPage();

				assert.isNotNull(stextPage);

				let buffer = mupdf.Buffer.empty();
				let output = mupdf.Output.withBuffer(buffer);
				stextPage.printAsJson(output, 1);

				let stextObj = JSON.parse(buffer.toJsString());
				expect(stextObj).toMatchSnapshot();
			});
		});

		describe("loadLinks()", function () {
			it("should return list of Links on page", function () {
				let links = page.loadLinks();

				assert.isNotNull(links);
				assert.lengthOf(links.links, 2);
			});
		});

		describe("search()", function () {
			it("should return list of hitboxes of search results", function () {
				let hits = page.search("a");
				assert.isArray(hits);
				expect(hits).toMatchSnapshot();
			});
		});

		describe("PdfPage", function () {
			describe("annotations()", function () {
				it("should return list of annotations on page", function () {
					let annotations = page.annotations();

					assert.isNotNull(annotations);
					assert.lengthOf(annotations.annotations, 8);
				});
			});
		});
	});

	describe("PdfPage", function () {
		let doc;
		let page;
		beforeAll(function () {
			doc = mupdf.Document.openFromJsBuffer(input, "application/pdf");
			page = doc.loadPage(0);
		});
		afterAll(function () {
			doc?.free();
			page?.free();
		});

		describe("annotations()", function () {
			it("should return AnnotationList", function () {
				let annotations = page.annotations();

				assert.instanceOf(annotations, mupdf.AnnotationList);
				assert.lengthOf(annotations.annotations, 8);
				assert.instanceOf(annotations.annotations[0], mupdf.Annotation);
			});
		});

	});

	describe("Link", function () {
		let doc;
		let page;
		let links;
		beforeAll(function () {
			doc = mupdf.Document.openFromJsBuffer(input, "application/pdf");
			page = doc.loadPage(0);
			links = page.loadLinks();
		});
		afterAll(function () {
			doc?.free();
			page?.free();
			// TODO - free links
		});

		describe("rect()", function () {
			it("should return Link hitbox", function () {
				let link = links.links[0];
				let linkRect = link.rect();

				assert.instanceOf(linkRect, mupdf.Rect);
				expect(linkRect).toMatchSnapshot();
			});
		});

		describe("isExternalLink()", function () {
			it("should return true if link has external URL", function () {
				let link = links.links[0];

				assert.isTrue(link.isExternalLink());
			});
		});

		describe("uri()", function () {
			it("should return link URI", function () {
				let link = links.links[0];

				assert.equal(link.uri(), "http://www.adobe.com");
			});
		});

		// TODO - resolve
	});

	// TODO - Outline

	describe("AnnotationList", function () {
		let doc;
		let page;
		let annotations;
		beforeAll(function () {
			doc = mupdf.Document.openFromJsBuffer(input, "application/pdf");
			page = doc.loadPage(0);
			annotations = page.annotations();
		});
		afterAll(function () {
			doc?.free();
			page?.free();
			// TODO - free annotations
		});

		describe("active()", function () {
			it("should return false by default", function () {
				let annotation = annotations.annotations[0];

				assert.isFalse(annotation.active());
			});

			it("should return the value from setActive()", function () {
				let annotation = annotations.annotations[0];

				annotation.setActive(true);
				assert.isTrue(annotation.active());
			});
		});

		describe("hot()", function () {
			it("should return false by default", function () {
				let annotation = annotations.annotations[0];

				assert.isFalse(annotation.hot());
			});

			it("should return the value from setHot()", function () {
				let annotation = annotations.annotations[0];

				annotation.setHot(true);
				assert.isTrue(annotation.hot());
			});
		});

		describe("getTransform()", function () {
			it("should return a Matrix", function () {
				let annotation = annotations.annotations[0];

				let transform = annotation.getTransform();
				assert.instanceOf(transform, mupdf.Matrix);
				expect(transform).toMatchSnapshot();
			});
		});

		// TODO page

		describe("bound()", function () {
			it("should return a Rect", function () {
				let annotation = annotations.annotations[0];

				let bound = annotation.bound();
				assert.instanceOf(bound, mupdf.Rect);
				expect(bound).toMatchSnapshot();
			});
		});

		describe("needsResynthesis()", function () {
			it("should return false by default", function () {
				let annotation = annotations.annotations[0];

				assert.isFalse(annotation.needsResynthesis());
			});

			// TODO
			it.skip("should return true after setResynthesised()", function () {
				let annotation = annotations.annotations[0];

				annotation.setResynthesised();
				assert.isTrue(annotation.needsResynthesis());
			});

			// TODO
			it.skip("should return true after dirty()", function () {
				let annotation = annotations.annotations[0];

				annotation.dirty();
				assert.isTrue(annotation.needsResynthesis());
			});
		});

		describe("popup()", function () {
			it("should return a Rect", function () {
				let annotation = annotations.annotations[0];

				let popup = annotation.popup();
				assert.instanceOf(popup, mupdf.Rect);
				expect(popup).toMatchSnapshot();
			});

			it("should return the value from setPopup()", function () {
				let annotation = annotations.annotations[0];

				let rect = new Rect(10, 10, 20, 20);
				annotation.setPopup(rect);
				assert.deepEqual(annotation.popup(), rect);
			});
		});

		// TODO - delete

		describe("typeString()", function () {
			it("should return the annotation's type", function () {
				assert.equal(annotations.annotations[0].typeString(), "FreeText");
				assert.equal(annotations.annotations[1].typeString(), "FileAttachment");
				assert.equal(annotations.annotations[2].typeString(), "FileAttachment");
			});
		});

		describe("flags()", function () {
			// TODO
			it("should return a number", function () {
				let annotation = annotations.annotations[0];

				assert.isNumber(annotation.flags());
			});
		});

		describe("rect()", function () {
			it("should return a Rect", function () {
				let annotation = annotations.annotations[0];

				let rect = annotation.rect();
				assert.instanceOf(rect, mupdf.Rect);
				expect(rect).toMatchSnapshot();
			});

			it("should return the value from setRect()", function () {
				let annotation = annotations.annotations[0];

				let rect = new Rect(10, 10, 20, 20);
				annotation.setRect(rect);
				assert.deepEqual(annotation.rect(), rect);
			});
		});

		describe("contents()", function () {
			it("should return the annotation's text", function () {
				let annotation = annotations.annotations[0];

				let contents = annotation.contents();
				assert.equal(contents, "just some links on the page here");
			});

			it("should be empty for non-text annotations", function () {
				let annotation = annotations.annotations[1];

				let contents = annotation.contents();
				assert.equal(contents, "");
			});

			it("should return the value from setContents()", function () {
				let annotation = annotations.annotations[0];

				annotation.setContents("hello world");
				assert.equal(annotation.contents(), "hello world");
			});
		});

		describe("open", function () {
			describe("hasOpen()", function () {
				it("should return a bool", function () {
					// TODO - Should return true for Text annots and annots with popup
					let annotation = annotations.annotations[0];

					assert.isBoolean(annotation.hasOpen());
				});
			});

			// TODO - test isOpen, setIsOpen with annot with popup
		});

		describe("iconName", function () {
			describe("hasIconName()", function () {
				it("should return false for FreeText", function () {
					let annotation = annotations.annotations[0];

					assert.isFalse(annotation.hasIconName());
				});

				it("should return true for FileAttachment", function () {
					let annotation = annotations.annotations[1];

					assert.isTrue(annotation.hasIconName());
				});
			});

			describe("iconName()", function () {
				it("should throw for FreeText", function () {
					let annotation = annotations.annotations[0];

					assert.throws(() => annotation.iconName());
				});

				it("should return icon name", function () {
					let annotation = annotations.annotations[1];

					assert.equal(annotation.iconName(), "Graph");
				});

				it("should return the value from setIconName()", function () {
					let annotation = annotations.annotations[1];

					annotation.setIconName("Foobar");
					assert.equal(annotation.iconName(), "Foobar");
				});
			});

			describe("setIconName()", function () {
				it("should throw for FreeText", function () {
					let annotation = annotations.annotations[0];

					assert.throws(() => annotation.setIconName("Foobar"));
				});
			});
		});

		// TODO - line endings

		describe("border()", function () {
			it("should return a number", function () {
				let annotation = annotations.annotations[0];

				let border = annotation.border();
				assert.isNumber(border);
				expect(border).toMatchSnapshot();
			});

			it("should return the value from setBorder()", function () {
				let annotation = annotations.annotations[0];

				annotation.setBorder(4.0);
				assert.equal(annotation.border(), 4.0);
			});
		});

		describe("language", function () {
			describe("language()", function () {
				it("should return a string", function () {
					let annotation = annotations.annotations[0];

					let language = annotation.language();
					assert.isString(language);
				});
			});

			describe("setLanguage()", function () {
				it("should throw for invalid string", function () {
					let annotation = annotations.annotations[0];

					assert.throws(() => annotation.setLanguage("%%%"));
				});

				it("should set the annotation's language", function () {
					let annotation = annotations.annotations[0];

					annotation.setLanguage("zh-Hant");
					assert.equal(annotation.language(), "zh-Hant");
					annotation.setLanguage("Foo");
					assert.equal(annotation.language(), "foo");
				});
			});
		});

		// TODO
		//wasm_pdf_annot_quadding
		//wasm_pdf_set_annot_quadding

		describe("opacity()", function () {
			it("should return a number", function () {
				let annotation = annotations.annotations[0];

				let opacity = annotation.opacity();
				assert.isNumber(opacity);
				expect(opacity).toMatchSnapshot();
			});

			it("should return the value from setOpacity()", function () {
				let annotation = annotations.annotations[0];

				annotation.setOpacity(0.75);
				assert.equal(annotation.opacity(), 0.75);
			});
		});

		// TODO
		// pdf_annot_MK_BG
		// pdf_set_annot_color
		// pdf_annot_interior_color

		// TODO - line
		// TODO - vertices
		// TODO - quad points

		describe.skip("dates", function () {
			// modificationDate
			// creationDate
			// setModificationDate
			// setCreationDate

			describe("modificationDate()", function () {
				it("should return a string", function () {
					let annotation = annotations.annotations[0];

					let modificationDate = annotation.modificationDate();
					assert.instanceOf(modificationDate, Date);
					expect(modificationDate).toMatchSnapshot();
				});

				// TODO - find case where it throws
			});

			describe("setModificationDate()", function () {
				it("should set the annotation's modificationDate", function () {
					let annotation = annotations.annotations[0];

					annotation.setModificationDate(new Date(2020, 3, 20));
					assert.equal(annotation.modificationDate().getTime(), new Date(2020, 3, 20).getTime());
				});

				// TODO - find case where it throws
			});

			describe("creationDate()", function () {
				it("should return a string", function () {
					let annotation = annotations.annotations[0];

					let creationDate = annotation.creationDate();
					assert.instanceOf(creationDate, Date);
					expect(creationDate).toMatchSnapshot();
				});

				// TODO - find case where it throws
			});

			describe("setCreationDate()", function () {
				it("should set the annotation's creationDate", function () {
					let annotation = annotations.annotations[0];

					annotation.setCreationDate(new Date(2020, 3, 20));
					assert.equal(annotation.modificationDate().getTime(), new Date(2020, 3, 20).getTime());
				});

				// TODO - find case where it throws
			});
		});

		describe("author", function () {
			describe("hasAuthor()", function () {
				it("should return true for FreeText", function () {
					let annotation = annotations.annotations[0];

					assert.isTrue(annotation.hasAuthor());
				});

				// TODO - find case where it returns false
			});

			describe("author()", function () {
				it("should return a string", function () {
					let annotation = annotations.annotations[0];

					let author = annotation.author();
					assert.isString(author);
					expect(author).toMatchSnapshot();
				});

				// TODO - find case where it throws
			});

			describe("setAuthor()", function () {
				it("should set the annotation's author", function () {
					let annotation = annotations.annotations[0];

					annotation.setAuthor("Batman");
					assert.equal(annotation.author(), "Batman");
				});

				// TODO - find case where it throws
			});
		});

		// TODO - default appearance

		describe("fieldFlags()", function () {
			// TODO - test actual flags?
			it("should return a number", function () {
				let annotation = annotations.annotations[0];

				let fieldFlags = annotation.fieldFlags();
				assert.strictEqual(fieldFlags, 0);
			});
		});

		// TODO - fieldValue
		// TODO - fieldLabel
	});

	// TODO - Pixmap

	describe("Buffer", function () {
		describe("empty()", function () {
			it("should return a buffer with size = 0", function () {
				let buffer = mupdf.Buffer.empty();

				assert.isNotNull(buffer);
				assert.equal(buffer.size(), 0);
			});

			it("should reserve at least given capacity", function () {
				let buffer = mupdf.Buffer.empty(64);

				assert.isNotNull(buffer);
				assert.equal(buffer.size(), 0);
				assert.isAtLeast(buffer.capacity(), 64);
			});
		});

		describe("fromJsBuffer()", function () {
			it("should return valid buffer", function () {
				let jsArray = Uint8Array.from([1, 2, 3, 4, 5]);
				let buffer = mupdf.Buffer.fromJsBuffer(jsArray);

				assert.isNotNull(buffer);
				assert.equal(buffer.size(), 5);
			});

			it("should preserve data through round-trip", function () {
				let jsArray = Uint8Array.from([1, 2, 3, 4, 5]);
				let buffer = mupdf.Buffer.fromJsBuffer(jsArray);

				assert.deepEqual(buffer.toUint8Array(), jsArray);
			});

			it("should be valid for empty array", function () {
				let jsArray = Uint8Array.from([]);
				let buffer = mupdf.Buffer.fromJsBuffer(jsArray);

				assert.isNotNull(buffer);
				assert.equal(buffer.size(), 0);
				assert.deepEqual(buffer.toUint8Array(), jsArray);
			});

			it("should throw given invalid value", function () {
				assert.throws(() => mupdf.Buffer.fromJsBuffer(42));
				assert.throws(() => mupdf.Buffer.fromJsBuffer([]));
				assert.throws(() => mupdf.Buffer.fromJsBuffer("hello"));
				assert.throws(() => mupdf.Buffer.fromJsBuffer({ a: 42 }));
				assert.throws(() => mupdf.Buffer.fromJsBuffer(null));
			});
		});

		describe("fromJsString()", function () {
			it("should preserve data through round-trip", function () {
				let buffer = mupdf.Buffer.fromJsString("Hello world");

				assert.isNotNull(buffer);
				assert.isAbove(buffer.size(), 0);
				assert.deepEqual(buffer.toJsString(), "Hello world");
			});

			it("should be valid for empty string", function () {
				let buffer = mupdf.Buffer.fromJsString("");

				assert.isNotNull(buffer);
				assert.equal(buffer.size(), 0);
				assert.deepEqual(buffer.toJsString(), "");
			});
		});

		describe("resize()", function () {
			it("should reserve at least given capacity", function () {
				let jsArray = Uint8Array.from([1, 2, 3, 4, 5]);
				let buffer = mupdf.Buffer.fromJsBuffer(jsArray);

				buffer.resize(128);

				assert.equal(buffer.size(), 5);
				assert.isAtLeast(buffer.capacity(), 128);
			});

			it("should shrink array if given smaller size", function () {
				let jsArray = Uint8Array.from([1, 2, 3, 4, 5]);
				let buffer = mupdf.Buffer.fromJsBuffer(jsArray);

				buffer.resize(3);

				assert.equal(buffer.size(), 3);
				assert.isAtLeast(buffer.capacity(), 3);
			});
		});

		describe("grow()", function () {
			it("should increase capacity", function () {
				let jsArray = Uint8Array.from([1, 2, 3, 4, 5]);
				let buffer = mupdf.Buffer.fromJsBuffer(jsArray);
				let oldCapacity = buffer.capacity();

				buffer.grow();

				assert.equal(buffer.size(), 5);
				assert.isAtLeast(buffer.capacity(), oldCapacity);
			});
		});

		describe("trim()", function () {
			it("should set capacity to length", function () {
				let jsArray = Uint8Array.from([1, 2, 3, 4, 5]);
				let buffer = mupdf.Buffer.fromJsBuffer(jsArray);
				buffer.resize(100);

				buffer.trim();

				assert.equal(buffer.size(), 5);
				assert.equal(buffer.capacity(), 5);
			});
		});

		describe("clear()", function () {
			it("should set buffer size to 0", function () {
				let jsArray = Uint8Array.from([1, 2, 3, 4, 5]);
				let buffer = mupdf.Buffer.fromJsBuffer(jsArray);

				buffer.clear();

				assert.equal(buffer.size(), 0);
			});
		});
	});

	describe("Stream", function () {
		describe("fromBuffer()", function () {
			it("should read bytes from buffer", function () {
				let jsArray = Uint8Array.from([1, 2, 3, 4, 5]);
				let buffer = mupdf.Buffer.fromJsBuffer(jsArray);

				let stream = mupdf.Stream.fromBuffer(buffer);

				assert.isTrue(stream.readAll().sameContentAs(buffer));
			});
		});

		describe("fromJsBuffer()", function () {
			it("should read bytes from JS buffer", function () {
				let jsArray = Uint8Array.from([1, 2, 3, 4, 5]);
				let stream = mupdf.Stream.fromJsBuffer(jsArray);

				assert.isTrue(stream.readAll().sameContentAs(mupdf.Buffer.fromJsBuffer(jsArray)));
			});

			it("should be valid for empty array", function () {
				let jsArray = Uint8Array.from([]);
				let stream = mupdf.Stream.fromJsBuffer(jsArray);

				assert.isTrue(stream.readAll().sameContentAs(mupdf.Buffer.empty()));
			});
		});

		describe("fromJsString()", function () {
			it("should read bytes from string", function () {
				let stream = mupdf.Stream.fromJsString("Hello world");

				assert.isTrue(stream.readAll().sameContentAs(mupdf.Buffer.fromJsString("Hello world")));
			});

			it("should be valid for empty string", function () {
				let stream = mupdf.Stream.fromJsString("");

				assert.isTrue(stream.readAll().sameContentAs(mupdf.Buffer.fromJsString("")));
			});
		});
	});

	// TODO - Output

	it.skip("should save a document to PNG", async function () {
		let doc = mupdf.Document.openFromJsBuffer(input, "application/pdf");
		var page = doc.loadPage(0);
		var pix = page.toPixmap(new mupdf.Matrix(1,0,0,1,0,0), mupdf.DeviceRGB, false);
		var png = pix.toPNG();
		await fs.mkdir("samples/", { recursive: true });
		await fs.writeFile("samples/out.png", png);
	});
});

// TODO
// - DeviceGray/RGB/etc
// - Finalizer?
