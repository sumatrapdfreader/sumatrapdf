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

// If running in Node.js environment
if (typeof require === "function") {
	var libmupdf;
	if (globalThis.SharedArrayBuffer != null) {
		libmupdf = require("../mupdf-wasm.js");
	} else {
		libmupdf = require("../mupdf-wasm-singlethread.js");
	}
}

function assert(pred, message) {
	if (!pred) {
		if (message == null)
			throw new Error("assertion failed");
		else
			throw new Error(message);
	}
}

function allocateUTF8(str) {
	var size = libmupdf.lengthBytesUTF8(str) + 1;
	var pointer = libmupdf._wasm_malloc(size);
	libmupdf.stringToUTF8(str, pointer, size);
	return pointer;
}

class MupdfError extends Error {
	constructor(message) {
		super(message);
		this.name = "MupdfError";
	}
}

class MupdfTryLaterError extends MupdfError {
	constructor(message) {
		super(message);
		this.name = "MupdfTryLaterError";
	}
}

// TODO - Port fitz geometry methods
// See platform/java/src/com/artifex/mupdf/fitz/Point.java
class Point {
	constructor(x, y) {
		assert(typeof x === "number" && !Number.isNaN(x), "invalid x argument");
		assert(typeof y === "number" && !Number.isNaN(y), "invalid y argument");
		this.x = x;
		this.y = y;
	}

	// TODO - See 'Matrix.from' below
	// static from(value)

	static fromPtr(ptr) {
		ptr = ptr >> 2;
		return new Point(
			libmupdf.HEAPF32[ptr],
			libmupdf.HEAPF32[ptr+1],
		);
	}
}

// TODO - Port fitz geometry methods
// See platform/java/src/com/artifex/mupdf/fitz/Rect.java
class Rect {
	constructor(x0, y0, x1, y1) {
		assert(typeof x0 === "number" && !Number.isNaN(x0), "invalid x0 argument");
		assert(typeof y0 === "number" && !Number.isNaN(y0), "invalid y0 argument");
		assert(typeof x1 === "number" && !Number.isNaN(x1), "invalid x1 argument");
		assert(typeof y1 === "number" && !Number.isNaN(y1), "invalid y1 argument");
		this.x0 = x0;
		this.y0 = y0;
		this.x1 = x1;
		this.y1 = y1;
	}

	// TODO - See 'Matrix.from' below
	// static from(value)

	static fromFloatRectPtr(ptr) {
		ptr = ptr >> 2;
		return new Rect(
			libmupdf.HEAPF32[ptr],
			libmupdf.HEAPF32[ptr+1],
			libmupdf.HEAPF32[ptr+2],
			libmupdf.HEAPF32[ptr+3],
		);
	}

	static fromIntRectPtr(ptr) {
		ptr = ptr >> 2;
		return new Rect(
			libmupdf.HEAP32[ptr],
			libmupdf.HEAP32[ptr+1],
			libmupdf.HEAP32[ptr+2],
			libmupdf.HEAP32[ptr+3],
		);
	}

	width() {
		return this.x1 - this.x0;
	}

	height() {
		return this.y1 - this.y0;
	}

	translated(xoff, yoff) {
		return new Rect(this.x0 + xoff, this.y0 + yoff, this.x1 + xoff, this.y1 + yoff);
	}

	transformed(matrix) {
		return matrix.transformRect(this);
	}
}

// TODO - Port fitz geometry methods
// See platform/java/src/com/artifex/mupdf/fitz/Matrix.java
class Matrix {
	constructor(a, b, c, d, e, f) {
		this.a = a;
		this.b = b;
		this.c = c;
		this.d = d;
		this.e = e;
		this.f = f;
	}

	static identity = new Matrix(1, 0, 0, 1, 0, 0);

	static from(value) {
		if (value instanceof Matrix)
			return value;
		if (Array.isArray(value) && value.length === 6)
			return new Matrix(value[0], value[1], value[2], value[3], value[4], value[5]);
		else
			throw new Error(`cannot create matrix from '${value}'`);
	}

	static fromPtr(ptr) {
		ptr = ptr >> 2;
		return new Matrix(
			libmupdf.HEAPF32[ptr],
			libmupdf.HEAPF32[ptr+1],
			libmupdf.HEAPF32[ptr+2],
			libmupdf.HEAPF32[ptr+3],
			libmupdf.HEAPF32[ptr+4],
			libmupdf.HEAPF32[ptr+5],
		);
	}

	static scale(scale_x, scale_y) {
		return Matrix.fromPtr(libmupdf._wasm_scale(scale_x, scale_y));
	}

	transformRect(rect) {
		assert(rect instanceof Rect, "invalid rect argument");
		return Rect.fromFloatRectPtr(libmupdf._wasm_transform_rect(
			rect.x0, rect.y0, rect.x1, rect.y1,
			this.a, this.b, this.c, this.d, this.e, this.f,
		));
	}
}

// TODO - All constructors should take a pointer, plus a private token

const finalizer = new FinalizationRegistry(callback => callback());

class Wrapper {
	constructor(pointer, dropFunction) {
		this.pointer = pointer;
		this.dropFunction = dropFunction;

		if (typeof pointer !== "number" || pointer === 0)
			throw new Error(`cannot create ${this.constructor.name}: invalid pointer param value '${pointer}'`);
		if (dropFunction == null)
			throw new Error(`cannot create ${this.constructor.name}: dropFunction is null`);
		if (typeof dropFunction !== "function")
			throw new Error(`cannot create ${this.constructor.name}: dropFunction value '${dropFunction}' is not a function`);

		finalizer.register(this, () => dropFunction(pointer), this);
	}
	free() {
		finalizer.unregister(this);
		this.dropFunction(this.pointer);
		this.pointer = 0;
	}
	toString() {
		return `[${this.constructor.name} ${this.pointer}]`;
	}
}

// platform/java/src/com/artifex/mupdf/fitz/Document.java
class Document extends Wrapper {
	constructor(pointer, buffer = null) {
		super(pointer, libmupdf._wasm_drop_document);
		// If this document is built from a buffer, we keep a
		// reference so it's not garbage-collected before the document
		this.buffer = buffer;
	}

	free() {
		super.free();
		this.buffer?.free();
		//this.stream?.free();
	}

	// recognize
	// needsPassword
	// authenticatePassword

	// resolveLink

	// getMetaData(String key);
	// setMetaData(String key, String value);

	// isReflowable
	// layout

	// isPDF

	static openFromJsBuffer(buffer, magic) {
		let fzBuffer = Buffer.fromJsBuffer(buffer);
		return Document.openFromBuffer(fzBuffer, magic);
	}

	static openFromBuffer(buffer, magic) {
		assert(buffer instanceof Buffer, "invalid buffer argument");
		assert(typeof magic === "string" || magic instanceof String, "invalid magic argument");
		let pointer = libmupdf.ccall(
			"wasm_open_document_with_buffer",
			"number",
			["number", "string"],
			[buffer.pointer, magic]
		);
		return new Document(pointer, buffer);
	}

	static openFromStream(stream, magic) {
		assert(stream instanceof Stream, "invalid stream argument");
		assert(typeof magic === "string" || magic instanceof String, "invalid magic argument");
		let pointer = libmupdf.ccall(
			"wasm_open_document_with_stream",
			"number",
			["number", "string"],
			[stream.pointer, magic]
		);
		// TODO - Add reference to stream.
		return new Document(pointer);
	}

	countPages() {
		return libmupdf._wasm_count_pages(this.pointer);
	}

	loadPage(pageNumber) {
		let page_ptr = libmupdf._wasm_load_page(this.pointer, pageNumber);
		let pdfPage_ptr = libmupdf._wasm_pdf_page_from_fz_page(page_ptr);

		if (pdfPage_ptr !== 0) {
			return new PdfPage(page_ptr, pdfPage_ptr);
		} else {
			return new Page(page_ptr);
		}
	}

	title() {
		// the string returned by this function is static and doesn't need to be freed
		return libmupdf.UTF8ToString(libmupdf._wasm_document_title(this.pointer));
	}

	loadOutline() {
		return new_outline(libmupdf._wasm_load_outline(this.pointer));
	}
}

// platform/java/src/com/artifex/mupdf/fitz/PdfDocument.java
class PdfDocument extends Document {
	// isPDF !

	// hasUnsavedChanges
	// canBeSavedIncrementally

	// save !

	// JS Form Support
	// Undo history

	// canUndo
	// canRedo
	// undo
	// redo

	// beginOperation
	// endOperation

	// getLanguage !
	// setLanguage

	// addEmbeddedFile
	// getEmbeddedFileParams
	// loadEmbeddedFileContents
	// verifyEmbeddedFileChecksum
}

class Page extends Wrapper {
	constructor(pointer) {
		super(pointer, libmupdf._wasm_drop_page);
	}

	bounds() {
		return Rect.fromFloatRectPtr(libmupdf._wasm_bound_page(this.pointer));
	}

	width() {
		return this.bounds().width();
	}

	height() {
		return this.bounds().height();
	}

	run(device, transformMatrix = Matrix.identity, cookie = null) {
		assert(device instanceof Device, "invalid device argument");
		let m = Matrix.from(transformMatrix);
		libmupdf._wasm_run_page(
			this.pointer,
			device.pointer,
			m.a, m.b, m.c, m.d, m.e, m.f,
			cookie?.pointer,
		);
	}

	runPageContents(device, transformMatrix = Matrix.identity, cookie = null) {
		assert(device instanceof Device, "invalid device argument");
		let m = Matrix.from(transformMatrix);
		libmupdf._wasm_run_page_contents(
			this.pointer,
			device.pointer,
			m.a, m.b, m.c, m.d, m.e, m.f,
			cookie?.pointer,
		);
	}

	runPageAnnots(device, transformMatrix = Matrix.identity, cookie = null) {
		assert(device instanceof Device, "invalid device argument");
		let m = Matrix.from(transformMatrix);
		libmupdf._wasm_run_page_annots(
			this.pointer,
			device.pointer,
			m.a, m.b, m.c, m.d, m.e, m.f,
			cookie?.pointer,
		);
	}

	runPageWidgets(device, transformMatrix = Matrix.identity, cookie = null) {
		assert(device instanceof Device, "invalid device argument");
		let m = Matrix.from(transformMatrix);
		libmupdf._wasm_run_page_widgets(
			this.pointer,
			device.pointer,
			m.a, m.b, m.c, m.d, m.e, m.f,
			cookie?.pointer,
		);
	}

	// showExtras?
	toPixmap(transformMatrix, colorspace, alpha = false, cookie = null) {
		assert(colorspace instanceof ColorSpace, "invalid colorspace argument");

		let bbox = this.bounds().transformed(Matrix.from(transformMatrix));
		let pixmap = Pixmap.withBbox(colorspace, bbox, alpha);
		if (alpha)
			pixmap.clear();
		else
			pixmap.clearWithWhite();

		let device = Device.drawDevice(transformMatrix, pixmap);
		this.run(device, Matrix.identity, cookie);
		device.close();
		device.free();

		return pixmap;
	}

	// toDisplayList(showExtras?)

	// options
	toSTextPage() {
		return new STextPage(
			libmupdf._wasm_new_stext_page_from_page(this.pointer)
		);
	}

	getLinks() {
		let links = [];

		for (let link = libmupdf._wasm_load_links(this.pointer); link !== 0; link = libmupdf._wasm_next_link(link)) {
			links.push(new Link(link));
		}

		// TODO - return plain array
		return new Links(links);
	}

	search(needle) {
		const MAX_HIT_COUNT = 500;
		let needle_ptr = 0;
		let hits_ptr = 0;

		try {
			hits_ptr = libmupdf._wasm_malloc(libmupdf._wasm_size_of_quad() * MAX_HIT_COUNT);

			let needle_ptr = allocateUTF8(needle);
			let hitCount = libmupdf._wasm_search_page(
				this.pointer, needle_ptr, hits_ptr, MAX_HIT_COUNT
			);

			let rects = [];
			for (let i = 0; i < hitCount; ++i) {
				let hit = hits_ptr + i * libmupdf._wasm_size_of_quad();
				let rect = Rect.fromFloatRectPtr(libmupdf._wasm_rect_from_quad(hit));
				rects.push(rect);
			}

			return rects;
		}
		finally {
			libmupdf._wasm_free(needle_ptr);
			libmupdf._wasm_free(hits_ptr);
		}
	}
}

class PdfPage extends Page {
	constructor(pagePointer, pdfPagePointer) {
		super(pagePointer);
		this.pdfPagePointer = pdfPagePointer;
		this.annotationList = null;
	}

	update() {
		libmupdf._wasm_pdf_update_page(this.pdfPagePointer);
	}

	getAnnotations() {
		let annotations = [];

		for (let annot = libmupdf._wasm_pdf_first_annot(this.pdfPagePointer); annot !== 0; annot = libmupdf._wasm_pdf_next_annot(annot)) {
			annotations.push(new Annotation(annot));
		}

		// TODO - find other way to structure this
		this.annotationList = new AnnotationList(annotations);
		return this.annotationList;
	}

	createAnnotation(annotType) {
		// TODO - Validate annot type in separate function
		assert(typeof annotType === "number" && !Number.isNaN(annotType), "invalid annotType argument");

		// TODO - Update annotation list?
		let annotPointer = libmupdf._wasm_pdf_create_annot(this.pdfPagePointer, annotType);
		let annot = new Annotation(annotPointer);

		this.update();
		return annot;
	}

	deleteAnnotation(removedAnnotation) {
		assert(removedAnnotation instanceof Annotation, "invalid annotation argument");
		libmupdf._wasm_pdf_delete_annot(this.pointer, removedAnnotation.pointer);
		if (this.annotationList) {
			this.annotationList.annotations = this.annotationList.annotations.filter(annot => annot.pointer === removedAnnotation.pointer);
		}
	}

	// applyRedactions

	// getWidgetsAt

	createLink(bbox, uri) {
		assert(bbox instanceof Rect, "invalid bbox argument");
		// TODO bbox is rect

		let uri_ptr = allocateUTF8(uri);

		try {
			return new Link(libmupdf._wasm_pdf_create_link(this.pdfPagePointer, bbox.x0, bbox.y0, bbox.x1, bbox.y1, uri_ptr));
		}
		finally {
			libmupdf._wasm_free(uri_ptr);
		}
	}
}

// TODO remove class
class Links {
	constructor(links) {
		this.links = links;
	}

	free() {
		// TODO
	}
}

class Link extends Wrapper {
	constructor(pointer) {
		// TODO
		super(pointer, () => {});
	}

	getBounds() {
		return Rect.fromFloatRectPtr(libmupdf._wasm_link_rect(this.pointer));
	}

	// setBounds

	isExternal() {
		return libmupdf._wasm_is_external_link(this.pointer) !== 0;
	}

	getURI() {
		// the string returned by this function is borrowed and doesn't need to be freed
		return libmupdf.UTF8ToString(libmupdf._wasm_link_uri(this.pointer));
	}

	// setURI

	resolve(doc) {
		assert(doc instanceof Document, "invalid doc argument");
		const uri_string_ptr = libmupdf._wasm_link_uri(this.pointer);
		return new Location(
			libmupdf._wasm_resolve_link_chapter(doc.pointer, uri_string_ptr),
			libmupdf._wasm_resolve_link_page(doc.pointer, uri_string_ptr),
		);
	}
}

class Location {
	constructor(chapter, page) {
		this.chapter = chapter;
		this.page = page;
	}

	pageNumber(doc) {
		assert(doc instanceof Document, "invalid doc argument");
		return libmupdf._wasm_page_number_from_location(doc.pointer, this.chapter, this.page);
	}
}

function new_outline(pointer) {
	if (pointer === 0)
		return null;
	else
		return new Outline(pointer);
}

// TODO - implement class Outline as follows:
/*
public class Outline
{
	public String title;
	public String uri;
	public Outline[] down; // children
}
*/

class Outline extends Wrapper {
	constructor(pointer) {
		// TODO
		super(pointer, () => {});
	}

	pageNumber(doc) {
		assert(doc instanceof Document, "invalid doc argument");
		return libmupdf._wasm_outline_page(doc.pointer, this.pointer);
	}

	title() {
		// the string returned by this function is borrowed and doesn't need to be freed
		return libmupdf.UTF8ToString(libmupdf._wasm_outline_title(this.pointer));
	}

	down() {
		return new_outline(libmupdf._wasm_outline_down(this.pointer));
	}

	next() {
		return new_outline(libmupdf._wasm_outline_next(this.pointer));
	}
}

// TODO - remove this class and have getAnnotations() return an Array directly
class AnnotationList {
	constructor(annotations) {
		this.annotations = annotations;
	}

	free() {
		// TODO
	}
}

// TODO extends PdfObj
class Annotation extends Wrapper {
	constructor(pointer) {
		super(pointer, () => {});
	}

	active() {
		return libmupdf._wasm_pdf_annot_active(this.pointer) !== 0;
	}

	setActive(active) {
		libmupdf._wasm_pdf_set_annot_active(this.pointer, active ? 1 : 0);
	}

	hot() {
		return libmupdf._wasm_pdf_annot_hot(this.pointer) !== 0;
	}

	setHot(hot) {
		libmupdf._wasm_pdf_set_annot_hot(this.pointer, hot ? 1 : 0);
	}

	getTransform() {
		return Matrix.fromPtr(libmupdf._wasm_pdf_annot_transform(this.pointer));
	}

	// TODO getObj? Or use extends?

	page() {
		// TODO - store page ref in class
	}

	bound() {
		return Rect.fromFloatRectPtr(libmupdf._wasm_pdf_bound_annot(this.pointer));
	}

	needsResynthesis() {
		return libmupdf._wasm_pdf_annot_needs_resynthesis(this.pointer) !== 0;
	}

	setResynthesised() {
		libmupdf._wasm_pdf_set_annot_resynthesised(this.pointer);
	}

	dirty() {
		libmupdf._wasm_pdf_dirty_annot(this.pointer);
	}

	setPopup(rect) {
		assert(rect instanceof Rect, "invalid rect argument");
		libmupdf._wasm_pdf_set_annot_popup(this.pointer, rect.x0, rect.y0, rect.x1, rect.y1);
	}

	popup() {
		return Rect.fromFloatRectPtr(libmupdf._wasm_pdf_annot_popup(this.pointer));
	}

	typeString() {
		// the string returned by this function is static and doesn't need to be freed
		return libmupdf.UTF8ToString(libmupdf._wasm_pdf_annot_type_string(this.pointer));
	}

	// TODO
	flags() {
		return libmupdf._wasm_pdf_annot_flags(this.pointer);
	}

	setFlags(flags) {
		return libmupdf._wasm_pdf_set_annot_flags(this.pointer, flags);
	}

	hasRect() {
		return libmupdf._wasm_pdf_annot_has_rect(this.pointer) !== 0;
	}

	rect() {
		return Rect.fromFloatRectPtr(libmupdf._wasm_pdf_annot_rect(this.pointer));
	}

	setRect(rect) {
		assert(rect instanceof Rect, "invalid rect argument");
		libmupdf._wasm_pdf_set_annot_rect(this.pointer, rect.x0, rect.y0, rect.x1, rect.y1);
	}

	contents() {
		let string_ptr = libmupdf._wasm_pdf_annot_contents(this.pointer);
		try {
			return libmupdf.UTF8ToString(string_ptr);
		}
		finally {
			libmupdf._wasm_free(string_ptr);
		}
	}

	setContents(text) {
		let text_ptr = allocateUTF8(text);
		try {
			libmupdf._wasm_pdf_set_annot_contents(this.pointer, text_ptr);
		}
		finally {
			libmupdf._wasm_free(text_ptr);
		}
	}

	hasOpen() {
		return libmupdf._wasm_pdf_annot_has_open(this.pointer) !== 0;
	}

	isOpen() {
		return libmupdf._wasm_pdf_annot_is_open(this.pointer) !== 0;
	}

	setIsOpen(isOpen) {
		return libmupdf._wasm_pdf_annot_set_is_open(this.pointer, isOpen ? 1 : 0);
	}

	hasIconName() {
		return libmupdf._wasm_pdf_annot_has_icon_name(this.pointer) !== 0;
	}

	iconName() {
		// the string returned by this function is static and doesn't need to be freed
		return libmupdf.UTF8ToString(libmupdf._wasm_pdf_annot_icon_name(this.pointer));
	}

	setIconName(name) {
		let name_ptr = allocateUTF8(name);
		try {
			libmupdf._wasm_pdf_set_annot_icon_name(this.pointer, name_ptr);
		}
		finally {
			libmupdf._wasm_free(name_ptr);
		}
	}

	// TODO - line endings

	border() {
		return libmupdf._wasm_pdf_annot_border(this.pointer);
	}

	setBorder(width) {
		libmupdf._wasm_pdf_set_annot_border(this.pointer, width);
	}

	// TODO - fz_document_language

	language() {
		// the string returned by this function is static and doesn't need to be freed
		return libmupdf.UTF8ToString(libmupdf._wasm_pdf_annot_language(this.pointer));
	}

	setLanguage(lang) {
		let lang_ptr = allocateUTF8(lang);
		try {
			libmupdf._wasm_pdf_set_annot_language(this.pointer, lang_ptr);
		}
		finally {
			libmupdf._wasm_free(lang_ptr);
		}
	}

	// TODO
	//wasm_pdf_annot_quadding
	//wasm_pdf_set_annot_quadding

	opacity() {
		return libmupdf._wasm_pdf_annot_opacity(this.pointer);
	}

	setOpacity(opacity) {
		libmupdf._wasm_pdf_set_annot_opacity(this.pointer, opacity);
	}

	// TODO
	// pdf_annot_MK_BG
	// pdf_set_annot_color
	// pdf_annot_interior_color

	hasLine() {
		return libmupdf._wasm_pdf_annot_has_line(this.pointer) !== 0;
	}

	line() {
		let line_ptr = libmupdf._wasm_pdf_annot_line(this.pointer);
		return [
			Point.fromPtr(line_ptr),
			Point.fromPtr(line_ptr + 8),
		];
	}

	setLine(point0, point1) {
		assert(point0 instanceof Point, "invalid point0 argument");
		assert(point1 instanceof Point, "invalid point1 argument");
		libmupdf._wasm_pdf_set_annot_line(this.pointer, point0.x, point0.y, point1.x, point1.y);
	}

	hasVertices() {
		return libmupdf._wasm_pdf_annot_has_vertices(this.pointer) !== 0;
	}

	vertexCount() {
		return libmupdf._wasm_pdf_annot_vertex_count(this.pointer);
	}

	vertex(i) {
		return Point.fromPtr(libmupdf._wasm_pdf_annot_vertex(this.pointer, i));
	}

	// TODO pdf_set_annot_vertices

	clearVertices() {
		libmupdf._wasm_pdf_clear_annot_vertices(this.pointer);
	}

	addVertex(point) {
		assert(point instanceof Point, "invalid point argument");
		libmupdf._wasm_pdf_add_annot_vertex(this.pointer, point.x, point.y);
	}

	setVertex(i, point) {
		assert(point instanceof Point, "invalid point argument");
		libmupdf._wasm_pdf_set_annot_vertex(this.pointer, i, point.x, point.y);
	}

	// TODO - quad points

	modificationDate() {
		// libmupdf uses seconds since epoch, but Date expects milliseconds
		return new Date(libmupdf._wasm_pdf_annot_modification_date(this.pointer) * 1000);
	}

	creationDate() {
		// libmupdf uses seconds since epoch, but Date expects milliseconds
		return new Date(libmupdf._wasm_pdf_annot_creation_date(this.pointer) * 1000);
	}

	setModificationDate(date) {
		assert(date instanceof Date, "invalid date argument");
		// Date stores milliseconds since epoch, but libmupdf expects seconds
		libmupdf._wasm_pdf_set_annot_modification_date(this.pointer, date.getTime() / 1000);
	}

	setCreationDate(date) {
		assert(date instanceof Date, "invalid date argument");
		// Date stores milliseconds since epoch, but libmupdf expects seconds
		libmupdf._wasm_pdf_set_annot_creation_date(this.pointer, date.getTime() / 1000);
	}

	hasAuthor() {
		return libmupdf._wasm_pdf_annot_has_author(this.pointer) !== 0;
	}

	author() {
		let string_ptr = libmupdf._wasm_pdf_annot_author(this.pointer);
		try {
			return libmupdf.UTF8ToString(string_ptr);
		}
		finally {
			libmupdf._wasm_free(string_ptr);
		}
	}

	setAuthor(name) {
		let name_ptr = allocateUTF8(name);
		try {
			libmupdf._wasm_pdf_set_annot_author(this.pointer, name_ptr);
		}
		finally {
			libmupdf._wasm_free(name_ptr);
		}
	}

	// TODO - default appearance

	fieldFlags() {
		return libmupdf._wasm_pdf_annot_field_flags(this.pointer);
	}

	fieldValue() {
		let string_ptr = libmupdf._wasm_pdf_annot_field_value(this.pointer);
		try {
			return libmupdf.UTF8ToString(string_ptr);
		}
		finally {
			libmupdf._wasm_free(string_ptr);
		}
	}

	fieldLabel() {
		let string_ptr = libmupdf._wasm_pdf_annot_field_label(this.pointer);
		try {
			return libmupdf.UTF8ToString(string_ptr);
		}
		finally {
			libmupdf._wasm_free(string_ptr);
		}
	}

	// TODO
	//int pdf_set_annot_field_value(fz_context *ctx, pdf_document *doc, pdf_annot *annot, const char *text, int ignore_trigger_events)
	// void pdf_set_annot_appearance(fz_context *ctx, pdf_annot *annot, const char *appearance, const char *state, fz_matrix ctm, fz_rect bbox, pdf_obj *res, fz_buffer *contents)
	// void pdf_set_annot_appearance_from_display_list(fz_context *ctx, pdf_annot *annot, const char *appearance, const char *state, fz_matrix ctm, fz_display_list *list)

	// TODO filespec

}

const PDF_ANNOT_TEXT = 0;
const PDF_ANNOT_LINK = 1;
const PDF_ANNOT_FREE_TEXT = 2;
const PDF_ANNOT_LINE = 3;
const PDF_ANNOT_SQUARE = 4;
const PDF_ANNOT_CIRCLE = 5;
const PDF_ANNOT_POLYGON = 6;
const PDF_ANNOT_POLY_LINE = 7;
const PDF_ANNOT_HIGHLIGHT = 8;
const PDF_ANNOT_UNDERLINE = 9;
const PDF_ANNOT_SQUIGGLY = 10;
const PDF_ANNOT_STRIKE_OUT = 11;
const PDF_ANNOT_REDACT = 12;
const PDF_ANNOT_STAMP = 13;
const PDF_ANNOT_CARET = 14;
const PDF_ANNOT_INK = 15;
const PDF_ANNOT_POPUP = 16;
const PDF_ANNOT_FILE_ATTACHMENT = 17;
const PDF_ANNOT_SOUND = 18;
const PDF_ANNOT_MOVIE = 19;
const PDF_ANNOT_RICH_MEDIA = 20;
const PDF_ANNOT_WIDGET = 21;
const PDF_ANNOT_SCREEN = 22;
const PDF_ANNOT_PRINTER_MARK = 23;
const PDF_ANNOT_TRAP_NET = 24;
const PDF_ANNOT_WATERMARK = 25;
const PDF_ANNOT_3D = 26;
const PDF_ANNOT_PROJECTION = 27;
const PDF_ANNOT_UNKNOWN = -1;


class ColorSpace extends Wrapper {
	constructor(pointer) {
		super(pointer, libmupdf._wasm_drop_colorspace);
	}
}

class Pixmap extends Wrapper {
	constructor(pointer) {
		super(pointer, libmupdf._wasm_drop_pixmap);
		this.bbox = Rect.fromIntRectPtr(libmupdf._wasm_pixmap_bbox(this.pointer));
	}

	static withBbox(colorspace, bbox, alpha) {
		// Note that the wasm function expects integers, but we pass JS `Number`
		// values. Floating-point numbers are truncated to integers automatically.
		return new Pixmap(libmupdf._wasm_new_pixmap_with_bbox(
			colorspace.pointer,
			bbox.x0, bbox.y0, bbox.x1, bbox.y1,
			0,
			alpha,
		));
	}

	clear() {
		libmupdf._wasm_clear_pixmap(this.pointer);
	}

	clearWithWhite() {
		libmupdf._wasm_clear_pixmap_with_value(this.pointer, 0xff);
	}

	// getWidth
	width() {
		return this.bbox.width();
	}

	// getHeight
	height() {
		return this.bbox.height();
	}

	// getStride
	// getAlpha
	// getColorSpace
	// getSamples

	// invert
	// invertLuminance
	// gamma
	// tint

	// TODO
	drawGrabHandle(x, y) {
		// TODO
		const VALUE = 0;
		libmupdf._wasm_clear_pixmap_rect_with_value(this.pointer, VALUE, x - 5, y - 5, x + 5, y + 5);
	}

	samples() {
		let stride = libmupdf._wasm_pixmap_stride(this.pointer);
		let n = stride * this.height;
		let p = libmupdf._wasm_pixmap_samples(this.pointer);
		return libmupdf.HEAPU8.subarray(p, p + n);
	}

	toPNG() {
		let buf = libmupdf._wasm_new_buffer_from_pixmap_as_png(this.pointer);
		try {
			let data = libmupdf._wasm_buffer_data(buf);
			let size = libmupdf._wasm_buffer_size(buf);
			return libmupdf.HEAPU8.slice(data, data + size);
		} finally {
			libmupdf._wasm_drop_buffer(buf);
		}
	}

	toUint8ClampedArray() {
		let n = libmupdf._wasm_pixmap_samples_size(this.pointer);
		let p = libmupdf._wasm_pixmap_samples(this.pointer);
		return new Uint8ClampedArray(libmupdf.HEAPU8.buffer, p, n).slice();
	}
}

class Device extends Wrapper {
	constructor(pointer) {
		super(pointer, libmupdf._wasm_drop_device);
	}

	static drawDevice(transformMatrix, pixmap) {
		assert(pixmap instanceof Pixmap, "invalid pixmap argument");
		let m = Matrix.from(transformMatrix);
		return new Device(libmupdf._wasm_new_draw_device(
			m.a, m.b, m.c, m.d, m.e, m.f,
			pixmap.pointer
		));
	}

	// displayListDevice

	close() {
		libmupdf._wasm_close_device(this.pointer);
	}
}

// class DisplayList

class JobCookie extends Wrapper {
	constructor(pointer) {
		super(pointer, libmupdf._wasm_free_cookie);
	}

	static create() {
		return new JobCookie(libmupdf._wasm_new_cookie());
	}

	aborted() {
		return libmupdf._wasm_cookie_aborted(this.pointer);
	}
}

function createCookie() {
	return libmupdf._wasm_new_cookie();
}

function cookieAborted(cookiePointer) {
	return libmupdf._wasm_cookie_aborted(cookiePointer);
}

function deleteCookie(cookiePointer) {
	libmupdf._wasm_free_cookie(cookiePointer);
}

class Buffer extends Wrapper {
	constructor(pointer) {
		super(pointer, libmupdf._wasm_drop_buffer);
	}

	static empty(capacity = 0) {
		let pointer = libmupdf._wasm_new_buffer(capacity);
		return new Buffer(pointer);
	}

	static fromJsBuffer(buffer) {
		assert(ArrayBuffer.isView(buffer) || buffer instanceof ArrayBuffer, "invalid buffer argument");
		let pointer = libmupdf._wasm_malloc(buffer.byteLength);
		libmupdf.HEAPU8.set(new Uint8Array(buffer), pointer);
		// Note: fz_new_buffer_drom_data takes ownership of the given pointer,
		// so we don't need to call free
		return new Buffer(libmupdf._wasm_new_buffer_from_data(pointer, buffer.byteLength));
	}

	static fromJsString(string) {
		let string_size = libmupdf.lengthBytesUTF8(string);
		let string_ptr = libmupdf._wasm_malloc(string_size) + 1;
		libmupdf.stringToUTF8(string, string_ptr, string_size + 1);
		// Note: fz_new_buffer_drom_data takes ownership of the given pointer,
		// so we don't need to call free
		return new Buffer(libmupdf._wasm_new_buffer_from_data(string_ptr, string_size));
	}

	getLength() {
		return libmupdf._wasm_buffer_size(this.pointer);
	}

	capacity() {
		return libmupdf._wasm_buffer_capacity(this.pointer);
	}

	resize(capacity) {
		libmupdf._wasm_resize_buffer(this.pointer, capacity);
	}

	grow() {
		libmupdf._wasm_grow_buffer(this.pointer);
	}

	trim() {
		libmupdf._wasm_trim_buffer(this.pointer);
	}

	clear() {
		libmupdf._wasm_clear_buffer(this.pointer);
	}

	toUint8Array() {
		let data = libmupdf._wasm_buffer_data(this.pointer);
		let size = libmupdf._wasm_buffer_size(this.pointer);
		return libmupdf.HEAPU8.slice(data, data + size);
	}

	toJsString() {
		let data = libmupdf._wasm_buffer_data(this.pointer);
		let size = libmupdf._wasm_buffer_size(this.pointer);

		return libmupdf.UTF8ToString(data, size);
	}

	sameContentAs(otherBuffer) {
		return libmupdf._wasm_buffers_eq(this.pointer, otherBuffer.pointer) !== 0;
	}
}

class Stream extends Wrapper {
	constructor(pointer, internalBuffer = null) {
		super(pointer, libmupdf._wasm_drop_stream);
		// We keep a reference so the internal buffer isn't dropped before the stream is.
		this.internalBuffer = internalBuffer;
	}

	static fromUrl(url, contentLength, block_size, prefetch) {
		let url_ptr = allocateUTF8(url);

		try {
			let pointer = libmupdf._wasm_open_stream_from_url(url_ptr, contentLength, block_size, prefetch);
			return new Stream(pointer);
		}
		finally {
			libmupdf._wasm_free(url_ptr);
		}
	}

	// This takes a reference to the buffer, not a clone.
	// Modifying the buffer after calling this function will change the returned stream's output.
	static fromBuffer(buffer) {
		assert(buffer instanceof Buffer, "invalid buffer argument");
		return new Stream(libmupdf._wasm_new_stream_from_buffer(buffer.pointer), buffer);
	}

	static fromJsBuffer(buffer) {
		return Stream.fromBuffer(Buffer.fromJsBuffer(buffer));
	}

	static fromJsString(string) {
		return Stream.fromBuffer(Buffer.fromJsString(string));
	}

	readAll(suggestedCapacity = 0) {
		return new Buffer(libmupdf._wasm_read_all(this.pointer, suggestedCapacity));
	}
}

class Output extends Wrapper {
	constructor(pointer) {
		super(pointer, libmupdf._wasm_drop_output);
	}

	static withBuffer(buffer) {
		assert(buffer instanceof Buffer, "invalid buffer argument");
		return new Output(libmupdf._wasm_new_output_with_buffer(buffer.pointer));
	}

	close() {
		libmupdf._wasm_close_output(this.pointer);
	}
}

class STextPage extends Wrapper {
	constructor(pointer) {
		super(pointer, libmupdf._wasm_drop_page);
	}

	printAsJson(output, scale) {
		assert(output instanceof Output, "invalid output argument");
		libmupdf._wasm_print_stext_page_as_json(output.pointer, this.pointer, scale);
	}
}


// Background progressive fetch

// TODO - move in Stream
function onFetchData(id, block, data) {
	let n = data.byteLength;
	let p = libmupdf._wasm_malloc(n);
	libmupdf.HEAPU8.set(new Uint8Array(data), p);
	libmupdf._wasm_on_data_fetched(id, block, p, n);
	libmupdf._wasm_free(p);
}

// TODO - replace with map
let fetchStates = {};

function fetchOpen(id, url, contentLength, blockShift, prefetch) {
	console.log("OPEN", url, "PROGRESSIVELY");
	fetchStates[id] = {
		url: url,
		blockShift: blockShift,
		blockSize: 1 << blockShift,
		prefetch: prefetch,
		contentLength: contentLength,
		map: new Array((contentLength >>> blockShift) + 1).fill(0),
		closed: false,
	};
}

async function fetchRead(id, block) {
	let state = fetchStates[id];

	if (state.map[block] > 0)
		return;

	state.map[block] = 1;
	let contentLength = state.contentLength;
	let url = state.url;
	let start = block << state.blockShift;
	let end = start + state.blockSize;
	if (end > contentLength)
		end = contentLength;

	try {
		let response = await fetch(url, { headers: { Range: `bytes=${start}-${end-1}` } });
		if (state.closed)
			return;

		// TODO - use ReadableStream instead?
		let buffer = await response.arrayBuffer();
		if (state.closed)
			return;

		console.log("READ", url, block+1, "/", state.map.length);
		state.map[block] = 2;

		onFetchData(id, block, buffer);

		onFetchCompleted(id);

		// TODO - Does this create a risk of stack overflow?
		if (state.prefetch)
			fetchReadNext(id, block + 1);
	} catch (error) {
		state.map[block] = 0;
		console.log("FETCH ERROR", url, block, error.toString);
	}
}

function fetchReadNext(id, next) {
	let state = fetchStates[id];
	if (!state)
		return;

	// Don't prefetch if we're already waiting for any blocks.
	for (let block = 0; block < state.map.length; ++block)
		if (state.map[block] === 1)
			return;

	// Find next block to prefetch (starting with the last fetched block)
	for (let block = next; block < state.map.length; ++block)
		if (state.map[block] === 0)
			return fetchRead(id, block);

	// Find next block to prefetch (starting from the beginning)
	for (let block = 0; block < state.map.length; ++block)
		if (state.map[block] === 0)
			return fetchRead(id, block);

	console.log("ALL BLOCKS READ");
}

function fetchClose(id) {
	fetchStates[id].closed = true;
	delete fetchStates[id];
}

// --- EXPORTS ---

const mupdf = {
	MupdfError,
	MupdfTryLaterError,
	Point,
	Rect,
	Matrix,
	Document,
	PdfDocument,
	Page,
	Links,
	Link,
	Location,
	Outline,
	PdfPage,
	AnnotationList,
	Annotation,
	ColorSpace,
	Pixmap,
	Device,
	JobCookie,
	createCookie,
	cookieAborted,
	deleteCookie,
	Buffer,
	Stream,
	Output,
	STextPage,
	PDF_ANNOT_TEXT,
	PDF_ANNOT_LINK,
	PDF_ANNOT_FREE_TEXT,
	PDF_ANNOT_LINE,
	PDF_ANNOT_SQUARE,
	PDF_ANNOT_CIRCLE,
	PDF_ANNOT_POLYGON,
	PDF_ANNOT_POLY_LINE,
	PDF_ANNOT_HIGHLIGHT,
	PDF_ANNOT_UNDERLINE,
	PDF_ANNOT_SQUIGGLY,
	PDF_ANNOT_STRIKE_OUT,
	PDF_ANNOT_REDACT,
	PDF_ANNOT_STAMP,
	PDF_ANNOT_CARET,
	PDF_ANNOT_INK,
	PDF_ANNOT_POPUP,
	PDF_ANNOT_FILE_ATTACHMENT,
	PDF_ANNOT_SOUND,
	PDF_ANNOT_MOVIE,
	PDF_ANNOT_RICH_MEDIA,
	PDF_ANNOT_WIDGET,
	PDF_ANNOT_SCREEN,
	PDF_ANNOT_PRINTER_MARK,
	PDF_ANNOT_TRAP_NET,
	PDF_ANNOT_WATERMARK,
	PDF_ANNOT_3D,
	PDF_ANNOT_PROJECTION,
	PDF_ANNOT_UNKNOWN,
	onFetchCompleted: () => {},
};

// TODO - Figure out better naming scheme for fetch methods
function onFetchCompleted(id) {
	mupdf.onFetchCompleted(id);
}

const libmupdf_injections = {
	fetchOpen,
	fetchRead,
	fetchClose,
	MupdfError,
	MupdfTryLaterError,
};

mupdf.ready = libmupdf(libmupdf_injections).then(m => {
	libmupdf = m;
	libmupdf._wasm_init_context();

	mupdf.DeviceGray = new ColorSpace(libmupdf._wasm_device_gray());
	mupdf.DeviceRGB = new ColorSpace(libmupdf._wasm_device_rgb());
	mupdf.DeviceBGR = new ColorSpace(libmupdf._wasm_device_bgr());
	mupdf.DeviceCMYK = new ColorSpace(libmupdf._wasm_device_cmyk());

	if (!globalThis.crossOriginIsolated) {
		console.warn("MuPDF: The current page is running in a non-isolated context. This means SharedArrayBuffer is not available. See https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer for details.");
		return { sharedBuffer: null };
	}
	if (globalThis.SharedArrayBuffer == null) {
		console.warn("MuPDF: You browser does not implement SharedArrayBuffer.");
		return { sharedBuffer: null };
	}
	if (libmupdf.wasmMemory == null) {
		console.error("MuPDF internal error: emscripten does not export wasmMemory");
		return { sharedBuffer: null };
	}
	if (!(libmupdf.wasmMemory instanceof WebAssembly.Memory) || !(libmupdf.wasmMemory.buffer instanceof SharedArrayBuffer)) {
		console.error("MuPDF internal error: wasmMemory exported by emscripten is not a valid instance of WebAssembly.Memory");
		return { sharedBuffer: null };
	}

	console.log("MuPDF: WASM module running in cross-origin isolated context")
	return { sharedBuffer: libmupdf.wasmMemory.buffer }
});

// If running in Node.js environment
if (typeof require === "function") {
	module.exports = mupdf;
}
