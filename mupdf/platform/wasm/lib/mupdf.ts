// Copyright (C) 2004-2025 Artifex Software, Inc.
//
// This file is part of MuPDF WASM Library.
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
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

"use strict"

import { Pointer } from "./mupdf-wasm.js"
import libmupdf_wasm from "./mupdf-wasm.js"

declare global {
	var $libmupdf_wasm_Module: any
}

var node_fs: any = null
if (typeof process !== "undefined" && process.versions && process.versions.node)
	node_fs = await import("node:fs")

const libmupdf = await libmupdf_wasm(globalThis["$libmupdf_wasm_Module"])

libmupdf._wasm_init_context()

function Malloc<T>(size: number) {
	return libmupdf._wasm_malloc(size) as unknown as Pointer<T>
}

function Free(ptr: any) {
	libmupdf._wasm_free(ptr as Pointer<"void">)
}

export const memento = {
	listBlocks() {
		libmupdf._wasm_Memento_listBlocks()
	},
	checkAllMemory() {
		libmupdf._wasm_Memento_checkAllMemory()
	},
}

/*
--------------------------------------------------------------------------------

How to call into WASM and convert values between JS and WASM (libmupdf) worlds:

Passing values into WASM needs to either copy primitive values into WASM memory
or passing around pointer values.

	Wrap and/or copy non-Userdata values into WASM:

		STRING(stringValue)
		STRING2(stringValue) -- if you need to pass more than one string
		MATRIX(matrixArray)
		RECT(rectArray)
		BUFFER(bufferValue)
		etc.

	Look up an enum value by string:

		ENUM<EnumType>(string, listOfValidValues)

	Pass the pointer when the value is a Userdata object:

		userdataObject.pointer

Convert WASM pointer into a JS value (for simple types like strings and matrices).

	fromType(pointer)

Wrap a WASM pointer in a new Userdata object (for complex types):

	new Wrapper(pointer)

PDFObjects are always bound to a PDFDocument, so must be accessed via a document:

	doc._fromPDFObjectNew(new_ptr)
	doc._fromPDFObjectKeep(borrowed_ptr)
	doc._PDFOBJ(value)

Type checking of input arguments at runtime.

	checkType(value, "string")
	checkType(value, Class)
	checkRect(value)
	checkMatrix(value)

	This code needs to work type safely from plain Javascript too,
	so do NOT rely on Typescript to do all the type checking.

--------------------------------------------------------------------------------
*/

export type Matrix = [number, number, number, number, number, number]
export type Rect = [number, number, number, number]
export type Quad = [number, number, number, number, number, number, number, number]
export type Point = [number, number]
export type Color = [number] | [number, number, number] | [number, number, number, number]
export type AnnotColor = [] | Color

export type Rotate = 0 | 90 | 180 | 270

export const Matrix = {
	identity: [ 1, 0, 0, 1, 0, 0 ] as Matrix,
	scale(sx: number, sy: number): Matrix {
		return [ sx, 0, 0, sy, 0, 0 ]
	},
	translate(tx: number, ty: number): Matrix {
		return [ 1, 0, 0, 1, tx, ty ]
	},
	rotate(d: number): Matrix {
		while (d < 0)
			d += 360
		while (d >= 360)
			d -= 360
		let s = Math.sin((d * Math.PI) / 180)
		let c = Math.cos((d * Math.PI) / 180)
		return [ c, s, -s, c, 0, 0 ]
	},
	invert(m: Matrix): Matrix {
		checkMatrix(m)
		let det = m[0] * m[3] - m[1] * m[2]
		if (det > -1e-23 && det < 1e-23)
			return m
		let rdet = 1 / det
		let inva = m[3] * rdet
		let invb = -m[1] * rdet
		let invc = -m[2] * rdet
		let invd = m[0] * rdet
		let inve = -m[4] * inva - m[5] * invc
		let invf = -m[4] * invb - m[5] * invd
		return [ inva, invb, invc, invd, inve, invf ]
	},
	concat(one: Matrix, two: Matrix): Matrix {
		checkMatrix(one)
		checkMatrix(two)
		return [
			one[0] * two[0] + one[1] * two[2],
			one[0] * two[1] + one[1] * two[3],
			one[2] * two[0] + one[3] * two[2],
			one[2] * two[1] + one[3] * two[3],
			one[4] * two[0] + one[5] * two[2] + two[4],
			one[4] * two[1] + one[5] * two[3] + two[5],
		]
	},
}

export const Rect = {
	MIN_INF_RECT: 0x80000000,
	MAX_INF_RECT: 0x7fffff80,
	empty: [ 0x80000000, 0x80000000, 0x7fffff80, 0x7fffff80 ],
	invalid: [ 0, 0, -1, -1 ],
	infinite: [ 0x7fffff80, 0x7fffff80, 0x80000000, 0x80000000 ],
	isEmpty: function (rect: Rect) {
		checkRect(rect)
		return rect[0] >= rect[2] || rect[1] >= rect[3]
	},
	isValid: function (rect: Rect) {
		checkRect(rect)
		return rect[0] <= rect[2] && rect[1] <= rect[3]
	},
	isInfinite: function (rect: Rect) {
		checkRect(rect)
		return (
			rect[0] === Rect.MAX_INF_RECT &&
			rect[1] === Rect.MAX_INF_RECT &&
			rect[2] === Rect.MIN_INF_RECT &&
			rect[3] === Rect.MIN_INF_RECT
		)
	},
	transform: function (rect: Rect, matrix: Matrix): Rect {
		checkRect(rect)
		checkMatrix(matrix)
		var t

		if (Rect.isInfinite(rect))
			return rect
		if (!Rect.isValid(rect))
			return rect

		var ax0 = rect[0] * matrix[0]
		var ax1 = rect[2] * matrix[0]
		if (ax0 > ax1)
			t = ax0, ax0 = ax1, ax1 = t

		var cy0 = rect[1] * matrix[2]
		var cy1 = rect[3] * matrix[2]
		if (cy0 > cy1)
			t = cy0, cy0 = cy1, cy1 = t

		ax0 += cy0 + matrix[4]
		ax1 += cy1 + matrix[4]

		var bx0 = rect[0] * matrix[1]
		var bx1 = rect[2] * matrix[1]
		if (bx0 > bx1)
			t = bx0, bx0 = bx1, bx1 = t

		var dy0 = rect[1] * matrix[3]
		var dy1 = rect[3] * matrix[3]
		if (dy0 > dy1)
			t = dy0, dy0 = dy1, dy1 = t

		bx0 += dy0 + matrix[5]
		bx1 += dy1 + matrix[5]

		return [ ax0, bx0, ax1, bx1 ]
	},
}

export function enableICC() {
	libmupdf._wasm_enable_icc()
}

export function disableICC() {
	libmupdf._wasm_disable_icc()
}

export function setUserCSS(text: string) {
	libmupdf._wasm_set_user_css(STRING(text))
}

export function emptyStore() {
	libmupdf._wasm_empty_store()
}

export function shrinkStore(percent: number): boolean {
	return libmupdf._wasm_shrink_store(percent)
}

export function installLoadFontFunction(f: (name: string, script: string, bold: boolean, italic: boolean) => Font | null) {
	$libmupdf_load_font_file_js = f
}

/* -------------------------------------------------------------------------- */

// To pass Rect and Matrix as pointer arguments
const _wasm_int = Malloc<"int">(4)
const _wasm_point = Malloc<"fz_point">(4 * 6) >> 2
const _wasm_rect = Malloc<"fz_rect">(4 * 8) >> 2
const _wasm_matrix = Malloc<"fz_matrix">(4 * 6) >> 2
const _wasm_color = Malloc<"number">(4 * 4) >> 2
const _wasm_quad = Malloc<"fz_quad">(4 * 8) >> 2
const _wasm_string: [ Pointer<"char">, Pointer<"char"> ] = [ 0 as Pointer<"char">, 0 as Pointer<"char"> ]

function checkType(value: any, type: any) {
	if (typeof type === "string" && typeof value !== type)
		throw new TypeError("expected " + type)
	if (typeof type === "function" && !(value instanceof type))
		throw new TypeError("expected " + type.name)
}

function checkPoint(value: any): asserts value is Point {
	if (!Array.isArray(value) || value.length !== 2)
		throw new TypeError("expected point")
}

function checkRect(value: any): asserts value is Rect {
	if (!Array.isArray(value) || value.length !== 4)
		throw new TypeError("expected rectangle")
}

function checkMatrix(value: any): asserts value is Matrix {
	if (!Array.isArray(value) || value.length !== 6)
		throw new TypeError("expected matrix")
}

function checkQuad(value: any): asserts value is Quad {
	if (!Array.isArray(value) || value.length !== 8)
		throw new TypeError("expected quad")
}

function checkColor(value: any): asserts value is Color {
	if (!Array.isArray(value) || (value.length !== 1 && value.length !== 3 && value.length !== 4))
		throw new TypeError("expected color array")
}

function checkAnnotColor(value: any): asserts value is AnnotColor {
	if (!Array.isArray(value) || (value.length !== 0 && value.length !== 1 && value.length !== 3 && value.length !== 4))
		throw new TypeError("expected color array")
}

/** The types that can be automatically converted into a Buffer object */
type AnyBuffer = Buffer | ArrayBuffer | Uint8Array | string

function BUFFER(input: AnyBuffer) {
	if (input instanceof Buffer)
		return input.pointer
	if (input instanceof ArrayBuffer || input instanceof Uint8Array)
		return new Buffer(input).pointer
	if (typeof input === "string")
		return new Buffer(input).pointer
	throw new TypeError("expected buffer")
}

function ENUM<T>(value: T, list: readonly T[]): number {
	if (typeof value === "number") {
		if (value >= 0 && value < list.length)
			return value
	}
	if (typeof value === "string") {
		let idx = list.indexOf(value)
		if (idx >= 0)
			return idx
	}
	throw new TypeError(`invalid enum value ("${value}"; expected ${list.join(", ")})`)
}

function allocateUTF8(str: string) {
	var size = libmupdf.lengthBytesUTF8(str) + 1
	var pointer = Malloc<"XXX">(size) as unknown as Pointer<"char">
	libmupdf.stringToUTF8(str, pointer, size)
	return pointer
}

function STRING_N(s: string, i: number) {
	if (_wasm_string[i]) {
		Free(_wasm_string[i])
		_wasm_string[i] = 0 as Pointer<"char">
	}
	return _wasm_string[i] = allocateUTF8(s)
}

function STRING(s: string) {
	return STRING_N(s, 0)
}

function STRING2(s: string) {
	return STRING_N(s, 1)
}

function STRING_OPT(s: string | null | undefined) {
	return typeof s === "string" ? STRING_N(s, 0) : 0 as Pointer<"char">
}

function STRING2_OPT(s: string | null | undefined) {
	return typeof s === "string" ? STRING_N(s, 1) : 0 as Pointer<"char">
}

function POINT(p: Point) {
	libmupdf.HEAPF32[_wasm_point + 0] = p[0]
	libmupdf.HEAPF32[_wasm_point + 1] = p[1]
	return _wasm_point << 2 as Pointer<"fz_point">
}

function POINT2(p: Point) {
	libmupdf.HEAPF32[_wasm_point + 2] = p[0]
	libmupdf.HEAPF32[_wasm_point + 3] = p[1]
	return (_wasm_point + 2) << 2 as Pointer<"fz_point">
}

function POINT3(p: Point) {
	libmupdf.HEAPF32[_wasm_point + 4] = p[0]
	libmupdf.HEAPF32[_wasm_point + 5] = p[1]
	return (_wasm_point + 4) << 2 as Pointer<"fz_point">
}

function RECT(r: Rect) {
	libmupdf.HEAPF32[_wasm_rect + 0] = r[0]
	libmupdf.HEAPF32[_wasm_rect + 1] = r[1]
	libmupdf.HEAPF32[_wasm_rect + 2] = r[2]
	libmupdf.HEAPF32[_wasm_rect + 3] = r[3]
	return _wasm_rect << 2 as Pointer<"fz_rect">
}

function RECT2(r: Rect) {
	libmupdf.HEAPF32[_wasm_rect + 4] = r[0]
	libmupdf.HEAPF32[_wasm_rect + 5] = r[1]
	libmupdf.HEAPF32[_wasm_rect + 6] = r[2]
	libmupdf.HEAPF32[_wasm_rect + 7] = r[3]
	return (_wasm_rect + 4) << 2 as Pointer<"fz_rect">
}

function MATRIX(m: Matrix) {
	libmupdf.HEAPF32[_wasm_matrix + 0] = m[0]
	libmupdf.HEAPF32[_wasm_matrix + 1] = m[1]
	libmupdf.HEAPF32[_wasm_matrix + 2] = m[2]
	libmupdf.HEAPF32[_wasm_matrix + 3] = m[3]
	libmupdf.HEAPF32[_wasm_matrix + 4] = m[4]
	libmupdf.HEAPF32[_wasm_matrix + 5] = m[5]
	return _wasm_matrix << 2 as Pointer<"fz_matrix">
}

function QUAD(q: Quad) {
	libmupdf.HEAPF32[_wasm_quad + 0] = q[0]
	libmupdf.HEAPF32[_wasm_quad + 1] = q[1]
	libmupdf.HEAPF32[_wasm_quad + 2] = q[2]
	libmupdf.HEAPF32[_wasm_quad + 3] = q[3]
	libmupdf.HEAPF32[_wasm_quad + 4] = q[4]
	libmupdf.HEAPF32[_wasm_quad + 5] = q[5]
	libmupdf.HEAPF32[_wasm_quad + 6] = q[6]
	libmupdf.HEAPF32[_wasm_quad + 7] = q[7]
	return _wasm_quad << 2 as Pointer<"fz_quad">
}

function COLOR(c?: AnnotColor) {
	if (typeof c !== "undefined") {
		switch (c.length) {
		case 0:
			break
		case 1:
			libmupdf.HEAPF32[_wasm_color + 0] = c[0]
			break
		case 3:
			libmupdf.HEAPF32[_wasm_color + 0] = c[0]
			libmupdf.HEAPF32[_wasm_color + 1] = c[1]
			libmupdf.HEAPF32[_wasm_color + 2] = c[2]
			break
		case 4:
			libmupdf.HEAPF32[_wasm_color + 0] = c[0]
			libmupdf.HEAPF32[_wasm_color + 1] = c[1]
			libmupdf.HEAPF32[_wasm_color + 2] = c[2]
			libmupdf.HEAPF32[_wasm_color + 3] = c[3]
			break
		}
	}
	return _wasm_color << 2 as Pointer<"float">
}

/* -------------------------------------------------------------------------- */

function fromColor(n: number): Color {
	if (n === 1)
		return [
			libmupdf.HEAPF32[_wasm_color] as number
		]
	if (n === 3)
		return [
			libmupdf.HEAPF32[_wasm_color + 0] as number,
			libmupdf.HEAPF32[_wasm_color + 1] as number,
			libmupdf.HEAPF32[_wasm_color + 2] as number,
		]
	if (n === 4)
		return [
			libmupdf.HEAPF32[_wasm_color + 0] as number,
			libmupdf.HEAPF32[_wasm_color + 1] as number,
			libmupdf.HEAPF32[_wasm_color + 2] as number,
			libmupdf.HEAPF32[_wasm_color + 3] as number,
		]
	throw new TypeError("invalid number of components for Color: " + n)
}

function fromAnnotColor(n: number): AnnotColor {
	if (n === 0)
		return []
	return fromColor(n)
}

function fromColorArray(n: number, ptr: Pointer<"float">): number[] {
	let addr = ptr >> 2
	let color: number[] = []
	for (let i = 0; i < n; ++i)
		color.push(libmupdf.HEAPF32[addr + i] as number)
	return color
}

function fromStringOrNull(ptr: Pointer<"char">): string | null {
	if (ptr === 0)
		return null
	return libmupdf.UTF8ToString(ptr)
}

function fromString(ptr: Pointer<"char">): string {
	return libmupdf.UTF8ToString(ptr)
}

function fromStringFree(ptr: Pointer<"char">): string {
	let str = libmupdf.UTF8ToString(ptr)
	Free(ptr)
	return str
}

function fromPoint(ptr: Pointer<"fz_point">): Point {
	let addr = ptr >> 2
	return [
		libmupdf.HEAPF32[addr + 0] as number,
		libmupdf.HEAPF32[addr + 1] as number,
	]
}

function fromRect(ptr: Pointer<"fz_rect">): Rect {
	let addr = ptr >> 2
	return [
		libmupdf.HEAPF32[addr + 0] as number,
		libmupdf.HEAPF32[addr + 1] as number,
		libmupdf.HEAPF32[addr + 2] as number,
		libmupdf.HEAPF32[addr + 3] as number,
	]
}

function fromMatrix(ptr: Pointer<"fz_matrix">): Matrix {
	let addr = ptr >> 2
	return [
		libmupdf.HEAPF32[addr + 0] as number,
		libmupdf.HEAPF32[addr + 1] as number,
		libmupdf.HEAPF32[addr + 2] as number,
		libmupdf.HEAPF32[addr + 3] as number,
		libmupdf.HEAPF32[addr + 4] as number,
		libmupdf.HEAPF32[addr + 5] as number,
	]
}

function fromQuad(ptr: Pointer<"fz_quad">): Quad {
	let addr = ptr >> 2
	return [
		libmupdf.HEAPF32[addr + 0] as number,
		libmupdf.HEAPF32[addr + 1] as number,
		libmupdf.HEAPF32[addr + 2] as number,
		libmupdf.HEAPF32[addr + 3] as number,
		libmupdf.HEAPF32[addr + 4] as number,
		libmupdf.HEAPF32[addr + 5] as number,
		libmupdf.HEAPF32[addr + 6] as number,
		libmupdf.HEAPF32[addr + 7] as number,
	]
}

function fromBuffer(ptr: Pointer<"fz_buffer">): Uint8Array {
	let data = libmupdf._wasm_buffer_get_data(ptr)
	let size = libmupdf._wasm_buffer_get_len(ptr)
	return libmupdf.HEAPU8.slice(data, data + size)
}

function fromLayerConfigUIInfo(ptr: Pointer<"pdf_layer_config_ui">): Object {
	return {
		text: libmupdf._wasm_pdf_layer_config_ui_get_text(ptr),
		depth: libmupdf._wasm_pdf_layer_config_ui_get_depth(ptr),
		type: libmupdf._wasm_pdf_layer_config_ui_get_type(ptr),
		selected: libmupdf._wasm_pdf_layer_config_ui_get_selected(ptr),
		locked: libmupdf._wasm_pdf_layer_config_ui_get_locked(ptr),
	}
}

/* unused for now
function rgbFromColor(c?: Color): [number, number, number] {
	var r = 0, g = 0, b = 0
	if (typeof c !== "undefined") {
		switch (c.length) {
		case 1:
			r = g = b = c[0]
			break
		case 3:
			r = c[0]
			g = c[1]
			b = c[2]
			break
		case 4:
			r = 1 - Math.min(1, c[0] + c[3])
			g = 1 - Math.min(1, c[1] + c[3])
			b = 1 - Math.min(1, c[2] + c[3])
			break
		}
	}
	return [ r, g, b ]
}

function numberFromColor(c?: Color): number {
	var [ r, g, b ] = rgbFromColor(c)
	return (255 << 24) | (r << 16) | (g << 8) | b
}
*/

function colorFromNumber(argb: number): Color {
	var r = (argb >> 16) & 255
	var g = (argb >> 8) & 255
	var b = (argb) & 255
	return [ r / 255, g / 255, b / 255 ]
}

/* -------------------------------------------------------------------------- */

type SearchFunction = (
	display_list: any,
	needle: Pointer<"char">,
	marks: Pointer<"int">,
	hits: Pointer<"fz_quad">,
	hit_max: number
) => number

function runSearch(searchFun: SearchFunction, searchThis: number, needle: string, max_hits = 500) {
	checkType(needle, "string")
	let hits = 0 as Pointer<"fz_quad">
	let marks = 0 as Pointer<"int">
	try {
		hits = Malloc<"fz_quad">(32 * max_hits)
		marks = Malloc<"int">(4 * max_hits)
		let n = searchFun(searchThis as any, STRING(needle), marks, hits, max_hits)
		let outer: Quad[][] = []
		if (n > 0) {
			let inner: Quad[] = []
			for (let i = 0; i < n; ++i) {
				let mark = libmupdf.HEAP32[(marks>>2) + i]
				let quad = fromQuad(hits + i * 32 as Pointer<"fz_quad">)
				if (i > 0 && mark) {
					outer.push(inner)
					inner = []
				}
				inner.push(quad)
			}
			outer.push(inner)
		}
		return outer
	} finally {
		Free(marks)
		Free(hits)
	}
}

/* -------------------------------------------------------------------------- */

abstract class Userdata<B> {
	private static _finalizer: FinalizationRegistry<number>

	static readonly _drop: (pointer: any) => void

	pointer: Pointer<B>

	constructor(pointer: Pointer<B>) {
		if (typeof pointer !== "number")
			throw new Error("invalid pointer: " + typeof pointer)
		if (pointer !== 0) {
			let ctor = this.constructor as typeof Userdata
			if (!ctor._finalizer)
				ctor._finalizer = new FinalizationRegistry(ctor._drop)
			ctor._finalizer.register(this, pointer, this)
		}
		this.pointer = pointer
	}

	destroy() {
		if (this.pointer !== 0) {
			let ctor = this.constructor as typeof Userdata
			ctor._finalizer.unregister(this)
			ctor._drop(this.pointer)
		}
		this.pointer = 0 as Pointer<B>
	}

	// Custom "console.log" formatting for Node
	[Symbol.for("nodejs.util.inspect.custom")]() {
		return this.toString()
	}

	toString() {
		return `[${this.constructor.name} ${this.pointer}]`
	}

	valueOf() {
		throw new Error("cannot convert Userdata to Javascript value")
	}
}

export class Buffer extends Userdata<"fz_buffer"> {
	static override readonly _drop = libmupdf._wasm_drop_buffer

	/** New empty Buffer. */
	constructor()

	/** New Buffer initialized with string contents as UTF-8. */
	constructor(data: string)

	/** New Buffer initialized with typed array contents. */
	constructor(data: ArrayBuffer | Uint8Array)

	/** PRIVATE */
	constructor(pointer: Pointer<"fz_buffer">)

	constructor(arg?: Pointer<"fz_buffer"> | string | ArrayBuffer | Uint8Array) {
		if (typeof arg === "undefined")
			super(libmupdf._wasm_new_buffer(1024))

		else if (typeof arg === "number")
			super(arg)

		else if (typeof arg === "string") {
			let data_len = libmupdf.lengthBytesUTF8(arg)
			let data_ptr = Malloc<"char">(data_len + 1)
			libmupdf.stringToUTF8(arg, data_ptr, data_len + 1)
			super(libmupdf._wasm_new_buffer_from_data(data_ptr, data_len))
		}

		else if (arg instanceof ArrayBuffer || arg instanceof Uint8Array) {
			let data_len = arg.byteLength
			let data_ptr = Malloc<"char">(data_len)
			libmupdf.HEAPU8.set(new Uint8Array(arg), data_ptr)
			super(libmupdf._wasm_new_buffer_from_data(data_ptr, data_len))
		}
	}

	get length() {
		return this.getLength()
	}

	set length(_: number) {
		throw new TypeError("buffer length is read-only")
	}

	getLength() {
		return libmupdf._wasm_buffer_get_len(this.pointer)
	}

	readByte(at: number) {
		let data = libmupdf._wasm_buffer_get_data(this.pointer)
		return libmupdf.HEAPU8[data + at] as number
	}

	write(s: string) {
		libmupdf._wasm_append_string(this.pointer, STRING(s))
	}

	writeByte(b: number) {
		libmupdf._wasm_append_byte(this.pointer, b)
	}

	writeLine(s: string) {
		this.write(s)
		this.writeByte(10)
	}

	writeBuffer(other: AnyBuffer) {
		libmupdf._wasm_append_buffer(this.pointer, BUFFER(other))
	}

	asUint8Array() {
		let data = libmupdf._wasm_buffer_get_data(this.pointer)
		let size = libmupdf._wasm_buffer_get_len(this.pointer)
		return libmupdf.HEAPU8.subarray(data, data + size)
	}

	slice(start: number, end?: number) {
		if (typeof end === "undefined")
			end = this.getLength()
		return new Buffer(libmupdf._wasm_slice_buffer(this.pointer, start, end))
	}

	asString() {
		return fromString(libmupdf._wasm_string_from_buffer(this.pointer))
	}

	save(filename: string) {
		if (node_fs)
			node_fs.writeFileSync(filename, this.asUint8Array())
		else
			throw new Error("missing 'fs' module")
	}
}

export type ColorSpaceType =
	"None" |
	"Gray" |
	"RGB" |
	"BGR" |
	"CMYK" |
	"Lab" |
	"Indexed" |
	"Separation"

export class ColorSpace extends Userdata<"fz_colorspace"> {
	static override readonly _drop = libmupdf._wasm_drop_colorspace

	static readonly COLORSPACE_TYPES: ColorSpaceType[] = [
		"None",
		"Gray",
		"RGB",
		"BGR",
		"CMYK",
		"Lab",
		"Indexed",
		"Separation"
	]

	// Create ColorSpace from ICC profile.
	constructor(profile: AnyBuffer, name: string)

	// PRIVATE
	constructor(pointer: Pointer<"fz_colorspace">)

	constructor(from: Pointer<"fz_colorspace"> | AnyBuffer, name?: string) {
		if (typeof from === "number") {
			super(from)
		} else {
			if (typeof from === "string") {
				if (node_fs)
					from = node_fs.readFileSync(from)
				else
					throw new Error("missing 'fs' module")
			}
			super(libmupdf._wasm_new_icc_colorspace(STRING_OPT(name), BUFFER(from as AnyBuffer)))
		}
	}

	getName() {
		return fromString(libmupdf._wasm_colorspace_get_name(this.pointer))
	}

	getType() {
		return ColorSpace.COLORSPACE_TYPES[libmupdf._wasm_colorspace_get_type(this.pointer)] || "None"
	}

	getNumberOfComponents() {
		return libmupdf._wasm_colorspace_get_n(this.pointer)
	}

	isGray(): boolean { return this.getType() === "Gray" }
	isRGB(): boolean { return this.getType() === "RGB" }
	isCMYK(): boolean { return this.getType() === "CMYK" }
	isIndexed(): boolean { return this.getType() === "Indexed" }
	isLab(): boolean { return this.getType() === "Lab" }
	isDeviceN(): boolean { return this.getType() === "Separation" }
	isSubtractive(): boolean { return this.getType() === "CMYK" || this.getType() === "Separation" }

	override toString() {
		return "[ColorSpace " + this.getName() + "]"
	}

	static readonly DeviceGray = new ColorSpace(libmupdf._wasm_device_gray())
	static readonly DeviceRGB = new ColorSpace(libmupdf._wasm_device_rgb())
	static readonly DeviceBGR = new ColorSpace(libmupdf._wasm_device_bgr())
	static readonly DeviceCMYK = new ColorSpace(libmupdf._wasm_device_cmyk())
	static readonly Lab = new ColorSpace(libmupdf._wasm_device_lab())
}

export type FontSimpleEncoding = "Latin" | "Greek" | "Cyrillic"

export type FontCJKOrdering = 0 | 1 | 2 | 3

export type FontCJKLanguage =
	"Adobe-CNS1" |
	"Adobe-GB1" |
	"Adobe-Japan1" |
	"Adobe-Korea1" |
	"zh-Hant" |
	"zh-TW" |
	"zh-HK" |
	"zh-Hans" |
	"zh-CN" |
	"ja" |
	"ko"

export class Font extends Userdata<"fz_font"> {
	static override readonly _drop = libmupdf._wasm_drop_font

	static readonly SIMPLE_ENCODING: FontSimpleEncoding[] = [
		"Latin",
		"Greek",
		"Cyrillic"
	]

	static readonly SIMPLE_ENCODING_LATIN = "Latin"
	static readonly SIMPLE_ENCODING_GREEK = "Greek"
	static readonly SIMPLE_ENCODING_CYRILLIC = "Cyrillic"

	static readonly ADOBE_CNS = 0
	static readonly ADOBE_GB = 1
	static readonly ADOBE_JAPAN = 2
	static readonly ADOBE_KOREA = 3

	static readonly CJK_ORDERING_BY_LANG: Record<FontCJKLanguage,FontCJKOrdering> = {
		"Adobe-CNS1": 0,
		"Adobe-GB1": 1,
		"Adobe-Japan1": 2,
		"Adobe-Korea1": 3,
		"zh-Hant": 0,
		"zh-TW": 0,
		"zh-HK": 0,
		"zh-Hans": 1,
		"zh-CN": 1,
		"ja": 2,
		"ko": 3,
	}

	constructor(name: string)
	constructor(name: string, filename: string, subfont?: number)
	constructor(name: string, data: AnyBuffer, subfont?: number)
	constructor(pointer: Pointer<"fz_font">)

	constructor(name_or_pointer: Pointer<"fz_font"> | string, data?: AnyBuffer, subfont=0) {
		let pointer = 0 as Pointer<"fz_font">
		if (typeof name_or_pointer === "number") {
			pointer = libmupdf._wasm_keep_font(name_or_pointer)
		} else {
			if (typeof data === "string") {
				if (node_fs)
					data = node_fs.readFileSync(data)
				else
					throw new Error("missing 'fs' module")
			}
			if (data)
				pointer = libmupdf._wasm_new_font_from_buffer(STRING(name_or_pointer), BUFFER(data), subfont)
			else if (name_or_pointer === "zh-Hant")
				pointer = libmupdf._wasm_new_cjk_font(Font.ADOBE_CNS)
			else if (name_or_pointer === "zh-Hans")
				pointer = libmupdf._wasm_new_cjk_font(Font.ADOBE_GB)
			else if (name_or_pointer === "ja")
				pointer = libmupdf._wasm_new_cjk_font(Font.ADOBE_JAPAN)
			else if (name_or_pointer === "ko")
				pointer = libmupdf._wasm_new_cjk_font(Font.ADOBE_KOREA)
			else
				pointer = libmupdf._wasm_new_base14_font(STRING(name_or_pointer))
		}
		super(pointer)
	}

	getName() {
		return fromString(libmupdf._wasm_font_get_name(this.pointer))
	}

	encodeCharacter(uni: number | string) {
		if (typeof uni === "string")
			uni = uni.charCodeAt(0)
		return libmupdf._wasm_encode_character(this.pointer, uni)
	}

	advanceGlyph(gid: number, wmode = 0) {
		return libmupdf._wasm_advance_glyph(this.pointer, gid, wmode)
	}

	isMono() {
		return !!libmupdf._wasm_font_is_monospaced(this.pointer)
	}

	isSerif() {
		return !!libmupdf._wasm_font_is_serif(this.pointer)
	}

	isBold() {
		return !!libmupdf._wasm_font_is_bold(this.pointer)
	}

	isItalic() {
		return !!libmupdf._wasm_font_is_italic(this.pointer)
	}
}

export class Image extends Userdata<"fz_image"> {
	static override readonly _drop = libmupdf._wasm_drop_image

	constructor(pointer: Pointer<"fz_image">)
	constructor(filename: string)
	constructor(data: AnyBuffer)
	constructor(pixmap: Pixmap, mask?: Image)

	constructor(data: Pointer<"fz_image"> | Pixmap | AnyBuffer, mask?: Image) {
		let pointer = 0 as Pointer<"fz_image">
		if (typeof data === "number") {
			pointer = libmupdf._wasm_keep_image(data)
		} else if (data instanceof Pixmap) {
			pointer = libmupdf._wasm_new_image_from_pixmap(
				data.pointer,
				mask ? mask.pointer : 0 as Pointer<"fz_image">
			)
		} else {
			if (typeof data === "string") {
				if (node_fs)
					data = node_fs.readFileSync(data)
				else
					throw new Error("missing 'fs' module")
			}
			pointer = libmupdf._wasm_new_image_from_buffer(BUFFER(data as Uint8Array))
		}
		super(pointer)
	}

	getWidth() {
		return libmupdf._wasm_image_get_w(this.pointer)
	}

	getHeight() {
		return libmupdf._wasm_image_get_h(this.pointer)
	}

	getNumberOfComponents() {
		return libmupdf._wasm_image_get_n(this.pointer)
	}

	getBitsPerComponent() {
		return libmupdf._wasm_image_get_bpc(this.pointer)
	}

	getXResolution() {
		return libmupdf._wasm_image_get_xres(this.pointer)
	}

	getYResolution() {
		return libmupdf._wasm_image_get_yres(this.pointer)
	}

	getImageMask() {
		return !!libmupdf._wasm_image_get_imagemask(this.pointer)
	}

	getColorSpace() {
		let cs = libmupdf._wasm_image_get_colorspace(this.pointer)
		if (cs)
			return new ColorSpace(libmupdf._wasm_keep_colorspace(cs))
		return null
	}

	getMask() {
		let mask = libmupdf._wasm_image_get_mask(this.pointer)
		if (mask)
			return new Image(libmupdf._wasm_keep_image(mask))
		return null
	}

	toPixmap() {
		return new Pixmap(libmupdf._wasm_get_pixmap_from_image(this.pointer))
	}
}

export type LineCap = "Butt" | "Round" | "Square" | "Triangle"
export type LineJoin = "Miter" | "Round" | "Bevel" | "MiterXPS"

// TODO: convert StrokeState from plain JS object to match mutool run ffi_pushstroke/ffi_tostroke

export type StrokeStateData  = {
	lineCap: LineCap,
	lineJoin: LineJoin,
	lineWidth: number,
	miterLimit: number,
	dashPhase?: number,
	dashes?: number[],
}

export class StrokeState extends Userdata<"fz_stroke_state"> {
	static override readonly _drop = libmupdf._wasm_drop_stroke_state

	static readonly LINE_CAP: LineCap[] = [
		"Butt",
		"Round",
		"Square",
		"Triangle"
	]

	static readonly LINE_JOIN: LineJoin[] = [
		"Miter",
		"Round",
		"Bevel",
		"MiterXPS"
	]

	constructor(data: Pointer<"fz_stroke_state"> | StrokeStateData) {
		if (typeof data === "number") {
			super(data)
			return this
		}
		super(libmupdf._wasm_new_stroke_state(data?.dashes?.length ?? 0))
		let lineCap = ENUM<LineCap>(data.lineCap, StrokeState.LINE_CAP)
		let lineJoin = ENUM<LineJoin>(data.lineJoin, StrokeState.LINE_JOIN)
		libmupdf._wasm_stroke_state_set_start_cap(this.pointer, lineCap)
		libmupdf._wasm_stroke_state_set_dash_cap(this.pointer, lineCap)
		libmupdf._wasm_stroke_state_set_end_cap(this.pointer, lineCap)
		libmupdf._wasm_stroke_state_set_linejoin(this.pointer, lineJoin)
		libmupdf._wasm_stroke_state_set_linewidth(this.pointer, data.lineWidth)
		libmupdf._wasm_stroke_state_set_miterlimit(this.pointer, data.miterLimit)
		libmupdf._wasm_stroke_state_set_dash_phase(this.pointer, data.dashPhase ?? 0)
		if (data.dashes) {
			for (let i = 0; i < data.dashes.length; ++i)
				libmupdf._wasm_stroke_state_set_dash_item(this.pointer, i, data.dashes[i] ?? 0)
		}
	}

	getLineCap() {
		return libmupdf._wasm_stroke_state_get_start_cap(this.pointer)
	}

	getLineJoin() {
		return libmupdf._wasm_stroke_state_get_linejoin(this.pointer)
	}

	getLineWidth() {
		return libmupdf._wasm_stroke_state_get_linewidth(this.pointer)
	}

	getMiterLimit() {
		return libmupdf._wasm_stroke_state_get_miterlimit(this.pointer)
	}

	getDashPhase() {
		return libmupdf._wasm_stroke_state_get_dash_phase(this.pointer)
	}

	getDashes(): number[] | null {
		var n = libmupdf._wasm_stroke_state_get_dash_len(this.pointer)
		if (n > 0) {
			var out: number[] = []
			for (let i = 0; i < n; ++i)
				out[i] = libmupdf._wasm_stroke_state_get_dash_item(this.pointer, i)
			return out
		}
		return null
	}
}

interface PathWalker {
	moveTo?(x: number, y: number): void
	lineTo?(x: number, y: number): void
	curveTo?(x1: number, y1: number, x2: number, y2: number, x3: number, y3: number): void
	closePath?(): void
}

export class Path extends Userdata<"fz_path"> {
	static override readonly _drop = libmupdf._wasm_drop_path

	constructor(pointer?: Pointer<"fz_path">) {
		if (typeof pointer === "number")
			super(pointer)
		else
			super(libmupdf._wasm_new_path())
	}

	getBounds(strokeState: StrokeState, transform: Matrix) {
		if (strokeState !== null)
			checkType(strokeState, StrokeState)
		checkMatrix(transform)
		return fromRect(libmupdf._wasm_bound_path(this.pointer, strokeState?.pointer, MATRIX(transform)))
	}

	moveTo(x: number, y: number) {
		checkType(x, "number")
		checkType(y, "number")
		libmupdf._wasm_moveto(this.pointer, x, y)
	}

	lineTo(x: number, y: number) {
		checkType(x, "number")
		checkType(y, "number")
		libmupdf._wasm_lineto(this.pointer, x, y)
	}

	curveTo(x1: number, y1: number, x2: number, y2: number, x3: number, y3: number) {
		checkType(x1, "number")
		checkType(y1, "number")
		checkType(x2, "number")
		checkType(y2, "number")
		checkType(x3, "number")
		checkType(y3, "number")
		libmupdf._wasm_curveto(this.pointer, x1, y1, x2, y2, x3, y3)
	}

	curveToV(cx: number, cy: number, ex: number, ey: number) {
		checkType(cx, "number")
		checkType(cy, "number")
		checkType(ex, "number")
		checkType(ey, "number")
		libmupdf._wasm_curvetov(this.pointer, cx, cy, ex, ey)
	}

	curveToY(cx: number, cy: number, ex: number, ey: number) {
		checkType(cx, "number")
		checkType(cy, "number")
		checkType(ex, "number")
		checkType(ey, "number")
		libmupdf._wasm_curvetoy(this.pointer, cx, cy, ex, ey)
	}

	closePath() {
		libmupdf._wasm_closepath(this.pointer)
	}

	rect(x1: number, y1: number, x2: number, y2: number) {
		checkType(x1, "number")
		checkType(y1, "number")
		checkType(x2, "number")
		checkType(y2, "number")
		libmupdf._wasm_rectto(this.pointer, x1, y1, x2, y2)
	}

	transform(matrix: Matrix) {
		checkMatrix(matrix)
		libmupdf._wasm_transform_path(this.pointer, MATRIX(matrix))
	}

	walk(walker: PathWalker) {
		let id = $libmupdf_path_id++
		$libmupdf_path_table.set(id, walker)
		libmupdf._wasm_walk_path(this.pointer, id)
		$libmupdf_path_table.delete(id)
	}
}

interface TextWalker {
	beginSpan?(font: Font, trm: Matrix, wmode: number, bidi: number, markupDirection: number, language: string): void
	showGlyph?(font: Font, trm: Matrix, glyph: number, unicode: number, wmode: number, bidi: number): void
	endSpan?(): void
}

export class Text extends Userdata<"fz_text"> {
	static override readonly _drop = libmupdf._wasm_drop_text

	constructor(pointer?: Pointer<"fz_text">) {
		if (typeof pointer === "number")
			super(pointer)
		else
			super(libmupdf._wasm_new_text())
	}

	getBounds(strokeState: StrokeState, transform: Matrix) {
		if (strokeState !== null)
			checkType(strokeState, StrokeState)
		checkMatrix(transform)
		return fromRect(libmupdf._wasm_bound_text(this.pointer, strokeState?.pointer, MATRIX(transform)))
	}

	showGlyph(font: Font, trm: Matrix, gid: number, uni: number, wmode = 0) {
		checkType(font, Font)
		checkMatrix(trm)
		checkType(gid, "number")
		checkType(uni, "number")
		libmupdf._wasm_show_glyph(
			this.pointer,
			font.pointer,
			MATRIX(trm),
			gid,
			uni,
			wmode
		)
	}

	showString(font: Font, trm: Matrix, str: string, wmode = 0) {
		checkType(font, Font)
		checkMatrix(trm)
		checkType(str, "string")
		return fromMatrix(
			libmupdf._wasm_show_string(
				this.pointer,
				font.pointer,
				MATRIX(trm),
				STRING(str),
				wmode
			)
		)
	}

	walk(walker: TextWalker) {
		let id = $libmupdf_text_id++
		$libmupdf_text_table.set(id, walker)
		libmupdf._wasm_walk_text(this.pointer, id)
		$libmupdf_text_table.delete(id)
	}
}

export class DisplayList extends Userdata<"fz_display_list"> {
	static override readonly _drop = libmupdf._wasm_drop_display_list

	constructor(pointer: Pointer<"fz_display_list">)
	constructor(mediabox: Rect)

	constructor(arg1: Pointer<"fz_display_list"> | Rect) {
		let pointer = 0 as Pointer<"fz_display_list">
		if (typeof arg1 === "number") {
			pointer = arg1
		} else {
			checkRect(arg1)
			pointer = libmupdf._wasm_new_display_list(RECT(arg1))
		}
		super(pointer)
	}

	getBounds() {
		return fromRect(libmupdf._wasm_bound_display_list(this.pointer))
	}

	toPixmap(matrix: Matrix, colorspace: ColorSpace, alpha = false) {
		checkMatrix(matrix)
		checkType(colorspace, ColorSpace)
		return new Pixmap(
			libmupdf._wasm_new_pixmap_from_display_list(
				this.pointer,
				MATRIX(matrix),
				colorspace.pointer,
				alpha
			)
		)
	}

	toStructuredText(options = "") {
		checkType(options, "string")
		return new StructuredText(libmupdf._wasm_new_stext_page_from_display_list(this.pointer, STRING(options)))
	}

	run(device: Device, matrix: Matrix) {
		checkType(device, Device)
		checkMatrix(matrix)
		libmupdf._wasm_run_display_list(this.pointer, device.pointer, MATRIX(matrix))
	}

	search(needle: string, max_hits = 500) {
		return runSearch(libmupdf._wasm_search_display_list, this.pointer, needle, max_hits)
	}
}

export class Pixmap extends Userdata<"fz_pixmap"> {
	static override readonly _drop = libmupdf._wasm_drop_pixmap

	constructor(pointer: Pointer<"fz_pixmap">)
	constructor(colorspace: ColorSpace, bbox: Rect, alpha: boolean)

	constructor(arg1: Pointer<"fz_pixmap"> | ColorSpace, bbox?: Rect, alpha = false) {
		if (typeof arg1 === "number") {
			super(arg1)
		}
		if (arg1 instanceof ColorSpace) {
			checkRect(bbox)
			super(libmupdf._wasm_new_pixmap_with_bbox(arg1.pointer, RECT(bbox), alpha))
		}
		if (arg1 === null) {
			checkRect(bbox)
			super(libmupdf._wasm_new_pixmap_with_bbox(0 as Pointer<"fz_colorspace">, RECT(bbox), alpha))
		}
	}

	getBounds() {
		let x = libmupdf._wasm_pixmap_get_x(this.pointer)
		let y = libmupdf._wasm_pixmap_get_y(this.pointer)
		let w = libmupdf._wasm_pixmap_get_w(this.pointer)
		let h = libmupdf._wasm_pixmap_get_h(this.pointer)
		return [ x, y, x + w, y + h ]
	}

	clear(value?: number) {
		if (typeof value === "undefined")
			libmupdf._wasm_clear_pixmap(this.pointer)
		else
			libmupdf._wasm_clear_pixmap_with_value(this.pointer, value)
	}

	getWidth() {
		return libmupdf._wasm_pixmap_get_w(this.pointer)
	}
	getHeight() {
		return libmupdf._wasm_pixmap_get_h(this.pointer)
	}
	getX() {
		return libmupdf._wasm_pixmap_get_x(this.pointer)
	}
	getY() {
		return libmupdf._wasm_pixmap_get_y(this.pointer)
	}
	getStride() {
		return libmupdf._wasm_pixmap_get_stride(this.pointer)
	}
	getNumberOfComponents() {
		return libmupdf._wasm_pixmap_get_n(this.pointer)
	}
	getAlpha() {
		return libmupdf._wasm_pixmap_get_alpha(this.pointer)
	}
	getXResolution() {
		return libmupdf._wasm_pixmap_get_xres(this.pointer)
	}
	getYResolution() {
		return libmupdf._wasm_pixmap_get_yres(this.pointer)
	}

	setResolution(x: number, y: number) {
		libmupdf._wasm_pixmap_set_xres(this.pointer, x)
		libmupdf._wasm_pixmap_set_yres(this.pointer, y)
	}

	getColorSpace() {
		let cs = libmupdf._wasm_pixmap_get_colorspace(this.pointer)
		if (cs)
			return new ColorSpace(libmupdf._wasm_keep_colorspace(cs))
		return null
	}

	getPixels() {
		let s = libmupdf._wasm_pixmap_get_stride(this.pointer)
		let h = libmupdf._wasm_pixmap_get_h(this.pointer)
		let p = libmupdf._wasm_pixmap_get_samples(this.pointer)
		return new Uint8ClampedArray(libmupdf.HEAPU8.buffer, p, s * h)
	}

	asPNG() {
		let buf = libmupdf._wasm_new_buffer_from_pixmap_as_png(this.pointer)
		try {
			return fromBuffer(buf)
		} finally {
			libmupdf._wasm_drop_buffer(buf)
		}
	}

	asPSD() {
		let buf = libmupdf._wasm_new_buffer_from_pixmap_as_psd(this.pointer)
		try {
			return fromBuffer(buf)
		} finally {
			libmupdf._wasm_drop_buffer(buf)
		}
	}

	asPAM() {
		let buf = libmupdf._wasm_new_buffer_from_pixmap_as_pam(this.pointer)
		try {
			return fromBuffer(buf)
		} finally {
			libmupdf._wasm_drop_buffer(buf)
		}
	}

	asJPEG(quality: number, invert_cmyk=false) {
		let buf = libmupdf._wasm_new_buffer_from_pixmap_as_jpeg(this.pointer, quality, invert_cmyk)
		try {
			return fromBuffer(buf)
		} finally {
			libmupdf._wasm_drop_buffer(buf)
		}
	}

	invert() {
		libmupdf._wasm_invert_pixmap(this.pointer)
	}

	invertLuminance() {
		libmupdf._wasm_invert_pixmap_luminance(this.pointer)
	}

	gamma(p: number) {
		libmupdf._wasm_gamma_pixmap(this.pointer, p)
	}

	tint(black: number | Color, white: number | Color) {
		let black_hex = 0x000000
		let white_hex = 0xffffff
		if (typeof black === "number")
			black_hex = black
		else if (black instanceof Array && black.length === 3)
			black_hex = ( ( (black[0] * 255) << 16 ) | ( (black[1] * 255) << 8 ) | ( (black[2] * 255) ) )
		if (typeof white === "number")
			white_hex = white
		else if (white instanceof Array && white.length === 3)
			white = ( ( (white[0] * 255) << 16 ) | ( (white[1] * 255) << 8 ) | ( (white[2] * 255) ) )
		libmupdf._wasm_tint_pixmap(this.pointer, black_hex, white_hex)
	}

	convertToColorSpace(colorspace: ColorSpace, keepAlpha=false) {
		checkType(colorspace, ColorSpace)
		checkType(keepAlpha, "boolean")
		return new Pixmap(libmupdf._wasm_convert_pixmap(this.pointer, colorspace.pointer, keepAlpha))
	}

	warp(points: Point[], width: number, height: number) {
		let quad = points.flat()
		checkQuad(quad)
		checkType(width, "number")
		checkType(height, "number")
		return new Pixmap(libmupdf._wasm_warp_pixmap(this.pointer, QUAD(quad), width, height))
	}
}

export class Shade extends Userdata<"fz_shade"> {
	static override readonly _drop = libmupdf._wasm_drop_shade
	getBounds() {
		return fromRect(libmupdf._wasm_bound_shade(this.pointer))
	}
}

interface StructuredTextWalker {
	onImageBlock?(bbox: Rect, transform: Matrix, image: Image): void
	beginTextBlock?(bbox: Rect): void
	beginLine?(bbox: Rect, wmode: number, direction: Point): void
	onChar?(c: string, origin: Point, font: Font, size: number, quad: Quad, color: Color): void
	endLine?(): void
	endTextBlock?(): void
	onVector?(bbox: Rect, flags: any, color: Color): void
}

type SelectMode = "chars" | "words" | "lines"

export class StructuredText extends Userdata<"fz_stext_page"> {
	static override readonly _drop = libmupdf._wasm_drop_stext_page

	static readonly SELECT_MODE: SelectMode[] = [
		"chars",
		"words",
		"lines"
	]

	static readonly SELECT_CHARS = "chars"
	static readonly SELECT_WORDS = "words"
	static readonly SELECT_LINES = "lines"

	walk(walker: StructuredTextWalker) {
		let block = libmupdf._wasm_stext_page_get_first_block(this.pointer)
		while (block) {
			let block_type = libmupdf._wasm_stext_block_get_type(block)
			let block_bbox = fromRect(libmupdf._wasm_stext_block_get_bbox(block))

			if (block_type === 0) {
				if (walker.beginTextBlock)
					walker.beginTextBlock(block_bbox)

				let line = libmupdf._wasm_stext_block_get_first_line(block)
				while (line) {
					let line_bbox = fromRect(libmupdf._wasm_stext_line_get_bbox(line))
					let line_wmode = libmupdf._wasm_stext_line_get_wmode(line)
					let line_dir = fromPoint(libmupdf._wasm_stext_line_get_dir(line))

					if (walker.beginLine)
						walker.beginLine(line_bbox, line_wmode, line_dir)

					if (walker.onChar) {
						let ch = libmupdf._wasm_stext_line_get_first_char(line)
						while (ch) {
							let ch_rune = String.fromCharCode(libmupdf._wasm_stext_char_get_c(ch))
							let ch_origin = fromPoint(libmupdf._wasm_stext_char_get_origin(ch))
							let ch_font = new Font(libmupdf._wasm_stext_char_get_font(ch))
							let ch_size = libmupdf._wasm_stext_char_get_size(ch)
							let ch_quad = fromQuad(libmupdf._wasm_stext_char_get_quad(ch))
							let ch_color = colorFromNumber(libmupdf._wasm_stext_char_get_argb(ch))

							walker.onChar(ch_rune, ch_origin, ch_font, ch_size, ch_quad, ch_color)

							ch = libmupdf._wasm_stext_char_get_next(ch)
						}
					}

					if (walker.endLine)
						walker.endLine()

					line = libmupdf._wasm_stext_line_get_next(line)
				}

				if (walker.endTextBlock)
					walker.endTextBlock()
			} else if (block_type === 1) {
				/* image */
				if (walker.onImageBlock) {
					let matrix = fromMatrix(libmupdf._wasm_stext_block_get_transform(block))
					let image = new Image(libmupdf._wasm_stext_block_get_image(block))
					walker.onImageBlock(block_bbox, matrix, image)
				}
			} else if (block_type === 2) {
				/* struct */
			} else if (block_type === 3) {
				/* vector */
				if (walker.onVector) {
					let v_flags_word = libmupdf._wasm_stext_block_get_v_flags(block)
					let v_flags = {
						isStroked: !!(v_flags_word & 1),
						isRectangle: !!(v_flags_word & 2),
					}
					let v_color = colorFromNumber(libmupdf._wasm_stext_block_get_v_argb(block))
					walker.onVector(block_bbox, v_flags, v_color)
				}
			} else if (block_type === 4) {
				/* grid */
			}

			block = libmupdf._wasm_stext_block_get_next(block)
		}
	}

	asJSON(scale = 1) {
		return fromStringFree(libmupdf._wasm_print_stext_page_as_json(this.pointer, scale))
	}

	asHTML(id: number) {
		return fromStringFree(libmupdf._wasm_print_stext_page_as_html(this.pointer, id))
	}

	asText() {
		return fromStringFree(libmupdf._wasm_print_stext_page_as_text(this.pointer))
	}

	snap(p: Point, q: Point, mode: SelectMode): Quad {
		let mm = ENUM<SelectMode>(mode, StructuredText.SELECT_MODE)
		return fromQuad(libmupdf._wasm_snap_selection(this.pointer, POINT(p), POINT2(q), mm))
	}

	copy(p: Point, q: Point): string {
		return fromStringFree(libmupdf._wasm_copy_selection(this.pointer, POINT(p), POINT2(q)))
	}

	highlight(p: Point, q: Point, max_hits = 100): Quad[] {
		let hits = 0 as Pointer<"fz_quad">
		let result: Quad[] = []
		try {
			hits = Malloc<"fz_quad">(32 * max_hits)
			let n = libmupdf._wasm_highlight_selection(this.pointer, POINT(p), POINT2(q), hits, max_hits)
			for (let i = 0; i < n; ++i)
				result.push(fromQuad(hits + i * 32 as Pointer<"fz_quad">))
		} finally {
			Free(hits)
		}
		return result
	}

	search(needle: string, max_hits = 500) {
		return runSearch(libmupdf._wasm_search_stext_page, this.pointer, needle, max_hits)
	}
}

export type BlendMode =
	"Normal" |
	"Multiply" |
	"Screen" |
	"Overlay" |
	"Darken" |
	"Lighten" |
	"ColorDodge" |
	"ColorBurn" |
	"HardLight" |
	"SoftLight" |
	"Difference" |
	"Exclusion" |
	"Hue" |
	"Saturation" |
	"Color" |
	"Luminosity"

export class Device extends Userdata<"fz_device"> {
	static override readonly _drop = libmupdf._wasm_drop_device

	static readonly BLEND_MODES: BlendMode[] = [
		"Normal",
		"Multiply",
		"Screen",
		"Overlay",
		"Darken",
		"Lighten",
		"ColorDodge",
		"ColorBurn",
		"HardLight",
		"SoftLight",
		"Difference",
		"Exclusion",
		"Hue",
		"Saturation",
		"Color",
		"Luminosity",
	]

	static readonly BLEND_NORMAL = "Normal"
	static readonly BLEND_MULTIPLY = "Multiply"
	static readonly BLEND_SCREEN = "Screen"
	static readonly BLEND_OVERLAY = "Overlay"
	static readonly BLEND_DARKEN = "Darken"
	static readonly BLEND_LIGHTEN = "Lighten"
	static readonly BLEND_COLOR_DODGE = "ColorDodge"
	static readonly BLEND_COLOR_BURN = "ColorBurn"
	static readonly BLEND_HARD_LIGHT = "HardLight"
	static readonly BLEND_SOFT_LIGHT = "SoftLight"
	static readonly BLEND_DIFFERENCE = "Difference"
	static readonly BLEND_EXCLUSION = "Exclusion"
	static readonly BLEND_HUE = "Hue"
	static readonly BLEND_SATURATION = "Saturation"
	static readonly BLEND_COLOR = "Color"
	static readonly BLEND_LUMINOSITY = "Luminosity"

	constructor(callbacks: DeviceFunctions)

	// PRIVATE
	constructor(pointer: Pointer<"fz_device">)

	constructor(pointer_or_callbacks: Pointer<"fz_device"> | DeviceFunctions) {
		if (typeof pointer_or_callbacks === "number")
			super(pointer_or_callbacks)
		else {
			let id = $libmupdf_device_id++
			$libmupdf_device_table.set(id, pointer_or_callbacks)
			super(libmupdf._wasm_new_js_device(id))
		}
	}

	fillPath(path: Path, evenOdd: boolean, ctm: Matrix, colorspace: ColorSpace, color: Color, alpha: number) {
		checkType(path, Path)
		checkMatrix(ctm)
		checkType(colorspace, ColorSpace)
		checkColor(color)
		libmupdf._wasm_fill_path(this.pointer, path.pointer, evenOdd, MATRIX(ctm), colorspace.pointer, COLOR(color), alpha)
	}

	strokePath(path: Path, stroke: StrokeState, ctm: Matrix, colorspace: ColorSpace, color: Color, alpha: number) {
		checkType(path, Path)
		checkType(stroke, StrokeState)
		checkMatrix(ctm)
		checkType(colorspace, ColorSpace)
		checkColor(color)
		libmupdf._wasm_stroke_path(
			this.pointer,
			path.pointer,
			stroke.pointer,
			MATRIX(ctm),
			colorspace.pointer,
			COLOR(color),
			alpha
		)
	}

	clipPath(path: Path, evenOdd: boolean, ctm: Matrix) {
		checkType(path, Path)
		checkMatrix(ctm)
		libmupdf._wasm_clip_path(this.pointer, path.pointer, evenOdd, MATRIX(ctm))
	}

	clipStrokePath(path: Path, stroke: StrokeState, ctm: Matrix) {
		checkType(path, Path)
		checkType(stroke, StrokeState)
		checkMatrix(ctm)
		libmupdf._wasm_clip_stroke_path(this.pointer, path.pointer, stroke.pointer, MATRIX(ctm))
	}

	fillText(text: Text, ctm: Matrix, colorspace: ColorSpace, color: Color, alpha: number) {
		checkType(text, Text)
		checkMatrix(ctm)
		checkType(colorspace, ColorSpace)
		checkColor(color)
		libmupdf._wasm_fill_text(this.pointer, text.pointer, MATRIX(ctm), colorspace.pointer, COLOR(color), alpha)
	}

	strokeText(text: Text, stroke: StrokeState, ctm: Matrix, colorspace: ColorSpace, color: Color, alpha: number) {
		checkType(text, Text)
		checkType(stroke, StrokeState)
		checkMatrix(ctm)
		checkType(colorspace, ColorSpace)
		checkColor(color)
		libmupdf._wasm_stroke_text(
			this.pointer,
			text.pointer,
			stroke.pointer,
			MATRIX(ctm),
			colorspace.pointer,
			COLOR(color),
			alpha
		)
	}

	clipText(text: Text, ctm: Matrix) {
		checkType(text, Text)
		checkMatrix(ctm)
		libmupdf._wasm_clip_text(this.pointer, text.pointer, MATRIX(ctm))
	}

	clipStrokeText(text: Text, stroke: StrokeState, ctm: Matrix) {
		checkType(text, Text)
		checkType(stroke, StrokeState)
		checkMatrix(ctm)
		libmupdf._wasm_clip_stroke_text(this.pointer, text.pointer, stroke.pointer, MATRIX(ctm))
	}

	ignoreText(text: Text, ctm: Matrix) {
		checkType(text, Text)
		checkMatrix(ctm)
		libmupdf._wasm_ignore_text(this.pointer, text.pointer, MATRIX(ctm))
	}

	fillShade(shade: Shade, ctm: Matrix, alpha: number) {
		checkType(shade, Shade)
		checkMatrix(ctm)
		libmupdf._wasm_fill_shade(this.pointer, shade.pointer, MATRIX(ctm), alpha)
	}

	fillImage(image: Image, ctm: Matrix, alpha: number) {
		checkType(image, Image)
		checkMatrix(ctm)
		libmupdf._wasm_fill_image(this.pointer, image.pointer, MATRIX(ctm), alpha)
	}

	fillImageMask(image: Image, ctm: Matrix, colorspace: ColorSpace, color: Color, alpha: number) {
		checkType(image, Image)
		checkMatrix(ctm)
		checkType(colorspace, ColorSpace)
		checkColor(color)
		libmupdf._wasm_fill_image_mask(this.pointer, image.pointer, MATRIX(ctm), colorspace.pointer, COLOR(color), alpha)
	}

	clipImageMask(image: Image, ctm: Matrix) {
		checkType(image, Image)
		checkMatrix(ctm)
		libmupdf._wasm_clip_image_mask(this.pointer, image.pointer, MATRIX(ctm))
	}

	popClip() {
		libmupdf._wasm_pop_clip(this.pointer)
	}

	beginMask(area: Rect, luminosity: boolean, colorspace: ColorSpace, color: Color) {
		checkRect(area)
		checkType(colorspace, ColorSpace)
		checkColor(color)
		libmupdf._wasm_begin_mask(this.pointer, RECT(area), luminosity, colorspace.pointer, COLOR(color))
	}

	endMask() {
		libmupdf._wasm_end_mask(this.pointer)
	}

	beginGroup(area: Rect, colorspace: ColorSpace, isolated: boolean, knockout: boolean, blendmode: BlendMode, alpha: number) {
		checkRect(area)
		checkType(colorspace, ColorSpace)
		let blendmode_ix = ENUM<BlendMode>(blendmode, Device.BLEND_MODES)
		libmupdf._wasm_begin_group(this.pointer, RECT(area), colorspace.pointer, isolated, knockout, blendmode_ix, alpha)
	}

	endGroup() {
		libmupdf._wasm_end_group(this.pointer)
	}

	beginTile(area: Rect, view: Rect, xstep: number, ystep: number, ctm: Matrix, id: number, doc_id: number) {
		checkRect(area)
		checkRect(view)
		checkMatrix(ctm)
		return libmupdf._wasm_begin_tile(this.pointer, RECT(area), RECT2(view), xstep, ystep, MATRIX(ctm), id, doc_id)
	}

	endTile() {
		libmupdf._wasm_end_tile(this.pointer)
	}

	beginLayer(name: string) {
		libmupdf._wasm_begin_layer(this.pointer, STRING(name))
	}

	endLayer() {
		libmupdf._wasm_end_layer(this.pointer)
	}

	close() {
		libmupdf._wasm_close_device(this.pointer)
	}
}

export class DrawDevice extends Device {
	constructor(matrix: Matrix, pixmap: Pixmap) {
		checkMatrix(matrix)
		checkType(pixmap, Pixmap)
		super(libmupdf._wasm_new_draw_device(MATRIX(matrix), pixmap.pointer))
	}
}

export class DisplayListDevice extends Device {
	constructor(displayList: DisplayList) {
		checkType(displayList, DisplayList)
		super(libmupdf._wasm_new_display_list_device(displayList.pointer))
	}
}

export class DocumentWriter extends Userdata<"fz_document_writer"> {
	static override readonly _drop = libmupdf._wasm_drop_document_writer

	constructor(buffer: Buffer, format: string, options: string) {
		super(
			libmupdf._wasm_new_document_writer_with_buffer(
				BUFFER(buffer),
				STRING(format),
				STRING2(options)
			)
		)
	}

	beginPage(mediabox: Rect) {
		checkRect(mediabox)
		return new Device(
			libmupdf._wasm_keep_device(
				libmupdf._wasm_begin_page(this.pointer, RECT(mediabox))
			)
		)
	}

	endPage() {
		libmupdf._wasm_end_page(this.pointer)
	}

	close() {
		libmupdf._wasm_close_document_writer(this.pointer)
	}
}

export type DocumentPermission =
	"print" |
	"copy" |
	"edit" |
	"annotate" |
	"form" |
	"accessibility" |
	"assemble" |
	"print-hq"

export class LinkDestination {
	static readonly LINK_DEST: LinkDestType[] = [
		"Fit",
		"FitB",
		"FitH",
		"FitBH",
		"FitV",
		"FitBV",
		"FitR",
		"XYZ",
	]

	static readonly FIT = "Fit"
	static readonly FIT_B = "FitB"
	static readonly FIT_H = "FitH"
	static readonly FIT_BH = "FitBH"
	static readonly FIT_V = "FitV"
	static readonly FIT_BV = "FitBV"
	static readonly FIT_R = "FitR"
	static readonly XYZ = "XYZ"

	type: LinkDestType
	chapter: number
	page: number
	x: number
	y: number
	width: number
	height: number
	zoom: number

	constructor(chapter=0, page=0, type:LinkDestType="Fit", x=NaN, y=NaN, width=NaN, height=NaN, zoom=NaN) {
		this.chapter = chapter
		this.page = page
		this.type = type
		this.x = x
		this.y = y
		this.width = width
		this.height = height
		this.zoom = zoom
	}
}

export class Document extends Userdata<"any_document"> {
	static override readonly _drop = libmupdf._wasm_drop_document

	static readonly META_FORMAT = "format"
	static readonly META_ENCRYPTION = "encryption"
	static readonly META_INFO_AUTHOR = "info:Author"
	static readonly META_INFO_TITLE = "info:Title"
	static readonly META_INFO_SUBJECT = "info:Subject"
	static readonly META_INFO_KEYWORDS = "info:Keywords"
	static readonly META_INFO_CREATOR = "info:Creator"
	static readonly META_INFO_PRODUCER = "info:Producer"
	static readonly META_INFO_CREATIONDATE = "info:CreationDate"
	static readonly META_INFO_MODIFICATIONDATE = "info:ModDate"

	static readonly PERMISSION_PRINT = "print"
	static readonly PERMISSION_COPY = "copy"
	static readonly PERMISSION_EDIT = "eedit"
	static readonly PERMISSION_ANNOTATE = "annotate"
	static readonly PERMISSION_FORM = "form"
	static readonly PERMISSION_ACCESSIBILITY = "accessibility"
	static readonly PERMISSION_ASSEMBLE = "assemble"
	static readonly PERMISSION_PRINT_HQ = "print-hq"

	static readonly PERMISSION: Record<DocumentPermission,number> = {
		"print": "p".charCodeAt(0),
		"copy": "c".charCodeAt(0),
		"edit": "e".charCodeAt(0),
		"annotate": "n".charCodeAt(0),
		"form": "f".charCodeAt(0),
		"accessibility": "y".charCodeAt(0),
		"assemble": "a".charCodeAt(0),
		"print-hq": "h".charCodeAt(0),
	}

	static openDocument(from: Buffer | ArrayBuffer | Uint8Array | Stream | string, magic?: string): Document {
		let pointer = 0 as Pointer<"any_document">
		let free_from = false

		if (typeof from === "string") {
			magic = from
			if (node_fs)
				from = node_fs.readFileSync(from)
			else
				throw new Error("missing 'fs' module")
		} else {
			if (typeof magic === "undefined")
				magic = "application/pdf"
		}

		checkType(magic, "string")

		if (from instanceof ArrayBuffer || from instanceof Uint8Array) {
			from = new Buffer(from)
			free_from = true
		}
		if (from instanceof Buffer)
			pointer = libmupdf._wasm_open_document_with_buffer(STRING(magic), from.pointer)
		else if (from instanceof Stream)
			pointer = libmupdf._wasm_open_document_with_stream(STRING(magic), from.pointer)
		else
			throw new Error("not a Buffer or Stream")

		if (free_from) {
			// Destroy any implicit Buffer instances immediately!
			// This may help the GC and FinalizationRegistry out when
			// processing many documents without a pause.
			from.destroy()
		}

		let pdf = libmupdf._wasm_pdf_document_from_fz_document(pointer)
		if (pdf)
			return new PDFDocument(pdf)

		return new Document(pointer)
	}

	formatLinkURI(dest: LinkDest) {
		return fromStringFree(
			libmupdf._wasm_format_link_uri(this.pointer,
				dest.chapter | 0,
				dest.page | 0,
				ENUM<LinkDestType>(dest.type, LinkDestination.LINK_DEST),
				+dest.x,
				+dest.y,
				+dest.width,
				+dest.height,
				+dest.zoom
			)
		)
	}

	asPDF() {
		if (this instanceof PDFDocument)
			return this
		return null
	}

	isPDF() {
		return this instanceof PDFDocument
	}

	needsPassword() {
		return !!libmupdf._wasm_needs_password(this.pointer)
	}

	authenticatePassword(password: string) {
		return libmupdf._wasm_authenticate_password(this.pointer, STRING(password))
	}

	hasPermission(perm: DocumentPermission) {
		let perm_ix = (typeof perm === "number") ? perm : Document.PERMISSION[perm]
		return !!libmupdf._wasm_has_permission(this.pointer, perm_ix)
	}

	getMetaData(key: string) {
		let value = libmupdf._wasm_lookup_metadata(this.pointer, STRING(key))
		if (value)
			return fromString(value)
		return undefined
	}

	setMetaData(key: string, value: string) {
		libmupdf._wasm_set_metadata(this.pointer, STRING(key), STRING2(value))
	}

	countPages() {
		return libmupdf._wasm_count_pages(this.pointer)
	}

	isReflowable() {
		libmupdf._wasm_is_document_reflowable(this.pointer)
	}

	layout(w: number, h: number, em: number) {
		libmupdf._wasm_layout_document(this.pointer, w, h, em)
	}

	loadPage(index: number) {
		let fz_ptr = libmupdf._wasm_load_page(this.pointer, index)
		if (this instanceof PDFDocument) {
			let pdf_ptr = libmupdf._wasm_pdf_page_from_fz_page(fz_ptr)
			if (pdf_ptr)
				return new PDFPage(this, pdf_ptr)
		}
		return new Page(fz_ptr)
	}

	loadOutline() {
		let doc = this.pointer
		function to_outline(outline: Pointer<"fz_outline">) {
			let result: OutlineItem[] = []
			while (outline) {
				let title = libmupdf._wasm_outline_get_title(outline)
				let uri = libmupdf._wasm_outline_get_uri(outline)
				let open = libmupdf._wasm_outline_get_is_open(outline)

				let item: OutlineItem = {
					title: title ? fromString(title) : undefined,
					uri: uri ? fromString(uri) : undefined,
					open: !!open,
				}

				let page = libmupdf._wasm_outline_get_page(doc, outline)
				if (page >= 0)
					item.page = page

				let down = libmupdf._wasm_outline_get_down(outline)
				if (down)
					item.down = to_outline(down)

				result.push(item)

				outline = libmupdf._wasm_outline_get_next(outline)
			}
			return result
		}
		let root = libmupdf._wasm_load_outline(doc)
		if (root)
			return to_outline(root)
		return null
	}

	resolveLink(link: string | Link) {
		if (link instanceof Link)
			return libmupdf._wasm_resolve_link(this.pointer, libmupdf._wasm_link_get_uri(link.pointer))
		return libmupdf._wasm_resolve_link(this.pointer, STRING(link))
	}

	resolveLinkDestination(link: string | Link): LinkDest {
		let dest: Pointer<"fz_link_dest">
		if (link instanceof Link)
			dest = libmupdf._wasm_resolve_link_dest(this.pointer, libmupdf._wasm_link_get_uri(link.pointer))
		else
			dest = libmupdf._wasm_resolve_link_dest(this.pointer, STRING(link))
		return {
			type: LinkDestination.LINK_DEST[libmupdf._wasm_link_dest_get_type(dest)] as LinkDestType,
			chapter: libmupdf._wasm_link_dest_get_chapter(dest),
			page: libmupdf._wasm_link_dest_get_page(dest),
			x: libmupdf._wasm_link_dest_get_x(dest),
			y: libmupdf._wasm_link_dest_get_y(dest),
			width: libmupdf._wasm_link_dest_get_w(dest),
			height: libmupdf._wasm_link_dest_get_h(dest),
			zoom: libmupdf._wasm_link_dest_get_zoom(dest),
		}
	}

	outlineIterator() {
		return new OutlineIterator(libmupdf._wasm_new_outline_iterator(this.pointer))
	}
}

interface OutlineItem {
	title: string | undefined,
	uri: string | undefined,
	open: boolean,
	down?: OutlineItem[],
	page?: number,
}

export class OutlineIterator extends Userdata<"fz_outline_iterator"> {
	static override readonly _drop = libmupdf._wasm_drop_outline_iterator

	static readonly ITERATOR_DID_NOT_MOVE = -1
	static readonly ITERATOR_AT_ITEM = 0
	static readonly ITERATOR_AT_EMPTY = 1

	static readonly FLAG_BOLD = 1
	static readonly FLAG_ITALIC = 2

	item() {
		let item = libmupdf._wasm_outline_iterator_item(this.pointer)
		if (item) {
			let title_ptr = libmupdf._wasm_outline_item_get_title(item)
			let uri_ptr = libmupdf._wasm_outline_item_get_uri(item)
			let is_open = libmupdf._wasm_outline_item_get_is_open(item)
			return {
				title: title_ptr ? fromString(title_ptr) : undefined,
				uri: uri_ptr ? fromString(uri_ptr) : undefined,
				open: !!is_open,
			} as OutlineItem
		}
		return null
	}

	next() {
		return libmupdf._wasm_outline_iterator_next(this.pointer)
	}

	prev() {
		return libmupdf._wasm_outline_iterator_prev(this.pointer)
	}

	up() {
		return libmupdf._wasm_outline_iterator_up(this.pointer)
	}

	down() {
		return libmupdf._wasm_outline_iterator_down(this.pointer)
	}

	delete() {
		return libmupdf._wasm_outline_iterator_delete(this.pointer)
	}

	insert(item: OutlineItem) {
		return libmupdf._wasm_outline_iterator_insert(this.pointer, STRING_OPT(item.title), STRING2_OPT(item.uri), item.open)
	}

	update(item: OutlineItem) {
		libmupdf._wasm_outline_iterator_update(this.pointer, STRING_OPT(item.title), STRING2_OPT(item.uri), item.open)
	}
}

export type LinkDestType =
	"Fit" |
	"FitB" |
	"FitH" |
	"FitBH" |
	"FitV" |
	"FitBV" |
	"FitR" |
	"XYZ"

interface LinkDest {
	type: LinkDestType,
	chapter: number,
	page: number,
	x: number,
	y: number,
	width: number,
	height: number,
	zoom: number,
}

export class Link extends Userdata<"fz_link"> {
	static override readonly _drop = libmupdf._wasm_drop_link

	getBounds() {
		return fromRect(libmupdf._wasm_link_get_rect(this.pointer))
	}

	setBounds(rect: Rect) {
		checkRect(rect)
		libmupdf._wasm_link_set_rect(this.pointer, RECT(rect))
	}

	getURI() {
		return fromString(libmupdf._wasm_link_get_uri(this.pointer))
	}

	setURI(uri: string) {
		checkType(uri, "string")
		libmupdf._wasm_link_set_uri(this.pointer, STRING(uri))
	}

	isExternal() {
		return /^\w[\w+-.]*:/.test(this.getURI())
	}
}

export type PageBox = "MediaBox" | "CropBox" | "BleedBox" | "TrimBox" | "ArtBox"

export class Page extends Userdata<"any_page"> {
	static override readonly _drop = libmupdf._wasm_drop_page

	static readonly BOXES: PageBox[] = [
		"MediaBox",
		"CropBox",
		"BleedBox",
		"TrimBox",
		"ArtBox"
	]

	static readonly MEDIA_BOX = "MediaBox"
	static readonly CROP_BOX = "CropBox"
	static readonly BLEED_BOX = "BleedBox"
	static readonly TRIM_BOX = "TrimBox"
	static readonly ART_BOX = "ArtBox"

	isPDF() {
		return this instanceof PDFPage
	}

	getBounds(box: PageBox = "CropBox") {
		let box_ix = ENUM<PageBox>(box, Page.BOXES)
		return fromRect(libmupdf._wasm_bound_page(this.pointer, box_ix))
	}

	getLabel() {
		return fromString(libmupdf._wasm_page_label(this.pointer))
	}

	run(device: Device, matrix: Matrix) {
		checkType(device, Device)
		checkMatrix(matrix)
		libmupdf._wasm_run_page(this.pointer, device.pointer, MATRIX(matrix))
	}

	runPageContents(device: Device, matrix: Matrix) {
		checkType(device, Device)
		checkMatrix(matrix)
		libmupdf._wasm_run_page_contents(this.pointer, device.pointer, MATRIX(matrix))
	}

	runPageAnnots(device: Device, matrix: Matrix) {
		checkType(device, Device)
		checkMatrix(matrix)
		libmupdf._wasm_run_page_annots(this.pointer, device.pointer, MATRIX(matrix))
	}

	runPageWidgets(device: Device, matrix: Matrix) {
		checkType(device, Device)
		checkMatrix(matrix)
		libmupdf._wasm_run_page_widgets(this.pointer, device.pointer, MATRIX(matrix))
	}

	toPixmap(matrix: Matrix, colorspace: ColorSpace, alpha = false, showExtras = true) {
		checkType(colorspace, ColorSpace)
		checkMatrix(matrix)
		let result
		if (showExtras)
			result = libmupdf._wasm_new_pixmap_from_page(this.pointer,
				MATRIX(matrix),
				colorspace.pointer,
				alpha)
		else
			result = libmupdf._wasm_new_pixmap_from_page_contents(this.pointer,
				MATRIX(matrix),
				colorspace.pointer,
				alpha)
		return new Pixmap(result)
	}

	toDisplayList(showExtras = true) {
		let result
		if (showExtras)
			result = libmupdf._wasm_new_display_list_from_page(this.pointer)
		else
			result = libmupdf._wasm_new_display_list_from_page_contents(this.pointer)
		return new DisplayList(result)
	}

	toStructuredText(options = "") {
		checkType(options, "string")
		return new StructuredText(libmupdf._wasm_new_stext_page_from_page(this.pointer, STRING(options)))
	}

	getLinks() {
		let links: Link[] = []
		let link = libmupdf._wasm_load_links(this.pointer)
		while (link) {
			links.push(new Link(libmupdf._wasm_keep_link(link)))
			link = libmupdf._wasm_link_get_next(link)
		}
		return links
	}

	createLink(bbox: Rect, uri: string) {
		checkRect(bbox)
		return new Link(libmupdf._wasm_create_link(this.pointer, RECT(bbox), STRING(uri)))
	}

	deleteLink(link: Link) {
		checkType(link, Link)
		libmupdf._wasm_delete_link(this.pointer, link.pointer)
	}

	search(needle: string, max_hits = 500) {
		return runSearch(libmupdf._wasm_search_page, this.pointer, needle, max_hits)
	}

}

/* -------------------------------------------------------------------------- */

export class PDFDocument extends Document {
	// Create a new empty document
	constructor()

	// Open an existing document
	constructor(filename: string)
	constructor(data: Buffer | ArrayBuffer | Uint8Array | Stream)

	// PRIVATE
	constructor(clone: PDFDocument)
	constructor(pointer: Pointer<"any_document">)

	constructor(arg1?: Pointer<"any_document"> | Buffer | ArrayBuffer | Uint8Array | Stream | PDFDocument | string) {
		if (typeof arg1 === "undefined")
			super(libmupdf._wasm_pdf_create_document())
		else if (typeof arg1 === "number")
			super(arg1)
		else if (arg1 instanceof PDFDocument) {
			super(arg1.pointer)
			libmupdf._wasm_keep_document(this.pointer)
		} else {
			let doc = Document.openDocument(arg1, "application/pdf")
			if (doc instanceof PDFDocument)
				return doc
			throw new Error("not a PDF document")
		}
	}

	override loadPage(index: number) {
		return super.loadPage(index) as PDFPage
	}

	// PDFObject instances are always bound to a document, so the WASM/JS value interface lives here.

	// Wrap a pdf_obj in a Userdata object. The pointer must be newly created or we already own it.
	_fromPDFObjectNew(ptr: Pointer<"pdf_obj">) {
		if (ptr === 0)
			return PDFObject.Null
		return new PDFObject(this, ptr)
	}

	// Wrap a pdf_obj in a Userdata object. The pointer must be a borrowed pointer, so we have to take ownership.
	_fromPDFObjectKeep(ptr: Pointer<"pdf_obj">) {
		if (ptr === 0)
			return PDFObject.Null
		return new PDFObject(this, libmupdf._wasm_pdf_keep_obj(ptr))
	}

	_toPDFObject(obj: any) {
		if (obj instanceof PDFObject)
			return obj
		if (obj === null || obj === undefined)
			return this.newNull()
		if (typeof obj === "string") {
			// if a JS string is surrounded by parens, convert it to a PDF string
			if (obj.startsWith("(") && obj.endsWith(")"))
				return this.newString(obj.slice(1, -1))
			// otherwise treat it as a name
			return this.newName(obj)
		}
		if (typeof obj === "number") {
			if (obj === (obj | 0))
				return this.newInteger(obj)
			return this.newReal(obj)
		}
		if (typeof obj === "boolean")
			return this.newBoolean(obj)
		if (obj instanceof Array) {
			let result = this.newArray()
			for (let item of obj)
				result.push(item)
			return result
		}
		if (obj instanceof Object) {
			let result = this.newDictionary()
			for (let key in obj)
				result.put(key, obj[key])
			return result
		}
		throw new TypeError("cannot convert value to PDFObject")
	}

	_PDFOBJ(obj: any) {
		// Note: We have to create a PDFObject instance for garbage collection.
		return this._toPDFObject(obj).pointer
	}

	getVersion() {
		return libmupdf._wasm_pdf_version(this.pointer)
	}

	getLanguage() {
		return fromStringOrNull(libmupdf._wasm_pdf_document_language(this.pointer))
	}

	setLanguage(lang: string) {
		libmupdf._wasm_pdf_set_document_language(this.pointer, STRING(lang))
	}

	countObjects() {
		return libmupdf._wasm_pdf_xref_len(this.pointer)
	}

	getTrailer() {
		return new PDFObject(this, libmupdf._wasm_pdf_trailer(this.pointer))
	}

	createObject() {
		let num = libmupdf._wasm_pdf_create_object(this.pointer)
		return this._fromPDFObjectNew(libmupdf._wasm_pdf_new_indirect(this.pointer, num))
	}

	newNull() { return PDFObject.Null }
	newBoolean(v: boolean) { return this._fromPDFObjectNew(libmupdf._wasm_pdf_new_bool(v)) }
	newInteger(v: number) { return this._fromPDFObjectNew(libmupdf._wasm_pdf_new_int(v)) }
	newReal(v: number) { return this._fromPDFObjectNew(libmupdf._wasm_pdf_new_real(v)) }
	newName(v: string) { return this._fromPDFObjectNew(libmupdf._wasm_pdf_new_name(STRING(v))) }
	newString(v: string) { return this._fromPDFObjectNew(libmupdf._wasm_pdf_new_text_string(STRING(v))) }

	newByteString(v: Uint8Array | number[]) {
		if (v instanceof Array)
			v = Uint8Array.from(v)
		checkType(v, Uint8Array)
		let len = v.byteLength
		let ptr = Malloc<"char">(len)
		libmupdf.HEAPU8.set(v, ptr)
		try {
			return this._fromPDFObjectNew(libmupdf._wasm_pdf_new_string(ptr, len))
		} finally {
			Free(ptr)
		}
	}

	newIndirect(v: number) { return this._fromPDFObjectNew(libmupdf._wasm_pdf_new_indirect(this.pointer, v)) }
	newArray() { return this._fromPDFObjectNew(libmupdf._wasm_pdf_new_array(this.pointer)) }
	newDictionary() { return this._fromPDFObjectNew(libmupdf._wasm_pdf_new_dict(this.pointer)) }

	deleteObject(num: number | PDFObject) {
		if (num instanceof PDFObject)
			num = num.asIndirect()
		else
			checkType(num, "number")
		libmupdf._wasm_pdf_delete_object(this.pointer, num)
	}

	addObject(obj: any) {
		return this._fromPDFObjectNew(libmupdf._wasm_pdf_add_object(this.pointer, this._PDFOBJ(obj)))
	}

	addStream(buf: AnyBuffer, obj: any) {
		return this._fromPDFObjectNew(libmupdf._wasm_pdf_add_stream(this.pointer, BUFFER(buf), this._PDFOBJ(obj), 0))
	}

	addRawStream(buf: AnyBuffer, obj: any) {
		return this._fromPDFObjectNew(libmupdf._wasm_pdf_add_stream(this.pointer, BUFFER(buf), this._PDFOBJ(obj), 1))
	}

	newGraftMap() {
		return new PDFGraftMap(this, libmupdf._wasm_pdf_new_graft_map(this.pointer))
	}

	graftObject(obj: PDFObject) {
		checkType(obj, PDFObject)
		return this._fromPDFObjectNew(libmupdf._wasm_pdf_graft_object(this.pointer, obj.pointer))
	}

	graftPage(to: number, srcDoc: PDFDocument, srcPage: number) {
		checkType(to, "number")
		checkType(srcDoc, PDFDocument)
		checkType(srcPage, "number")
		libmupdf._wasm_pdf_graft_page(this.pointer, to, srcDoc.pointer, srcPage)
	}

	addSimpleFont(font: Font, encoding: FontSimpleEncoding = "Latin") {
		checkType(font, Font)
		var encoding_ix = ENUM<FontSimpleEncoding>(encoding, Font.SIMPLE_ENCODING)
		return this._fromPDFObjectNew(libmupdf._wasm_pdf_add_simple_font(this.pointer, font.pointer, encoding_ix))
	}

	addCJKFont(font: Font, lang: FontCJKOrdering | FontCJKLanguage, wmode = 0, serif = true) {
		checkType(font, Font)
		if (typeof lang === "string")
			lang = Font.CJK_ORDERING_BY_LANG[lang]
		return this._fromPDFObjectNew(libmupdf._wasm_pdf_add_cjk_font(this.pointer, font.pointer, lang, wmode, serif))
	}

	addFont(font: Font) {
		checkType(font, Font)
		return this._fromPDFObjectNew(libmupdf._wasm_pdf_add_cid_font(this.pointer, font.pointer))
	}

	addImage(image: Image) {
		checkType(image, Image)
		return this._fromPDFObjectNew(libmupdf._wasm_pdf_add_image(this.pointer, image.pointer))
	}

	loadImage(ref: PDFObject) {
		checkType(ref, PDFObject)
		return new Image(libmupdf._wasm_pdf_load_image(this.pointer, ref.pointer))
	}

	findPage(index: number) {
		checkType(index, "number")
		return this._fromPDFObjectKeep(libmupdf._wasm_pdf_lookup_page_obj(this.pointer, index))
	}

	setPageTreeCache(enabled: boolean) {
		checkType(enabled, "boolean")
		libmupdf._wasm_pdf_set_page_tree_cache(this.pointer, enabled)
	}

	addPage(mediabox: Rect, rotate: Rotate, resources: any, contents: AnyBuffer) {
		checkRect(mediabox)
		checkType(rotate, "number")
		return this._fromPDFObjectNew(
			libmupdf._wasm_pdf_add_page(
				this.pointer,
				RECT(mediabox),
				rotate,
				this._PDFOBJ(resources),
				BUFFER(contents)
			)
		)
	}

	insertPage(at: number, obj: PDFObject) {
		checkType(at, "number")
		libmupdf._wasm_pdf_insert_page(this.pointer, at, this._PDFOBJ(obj))
	}

	deletePage(at: number) {
		checkType(at, "number")
		libmupdf._wasm_pdf_delete_page(this.pointer, at)
	}

	isEmbeddedFile(ref: PDFObject) {
		checkType(ref, PDFObject)
		return !!libmupdf._wasm_pdf_is_embedded_file(ref.pointer)
	}

	addEmbeddedFile(filename: string, mimetype: string, contents: AnyBuffer, created: Date, modified: Date, checksum = false) {
		checkType(filename, "string")
		checkType(mimetype, "string")
		checkType(created, Date)
		checkType(modified, Date)
		checkType(checksum, "boolean")
		return this._fromPDFObjectNew(
			libmupdf._wasm_pdf_add_embedded_file(
				this.pointer,
				STRING(filename),
				STRING2(mimetype),
				BUFFER(contents),
				created.getTime() / 1000 | 0,
				modified.getTime() / 1000 | 0,
				checksum
			)
		)
	}

	getFilespecParams(ref: PDFObject) {
		checkType(ref, PDFObject)
		let ptr = libmupdf._wasm_pdf_get_filespec_params(ref.pointer)
		return {
			filename:
				fromString(libmupdf._wasm_pdf_filespec_params_get_filename(ptr)),
			mimetype:
				fromString(libmupdf._wasm_pdf_filespec_params_get_mimetype(ptr)),
			size:
				libmupdf._wasm_pdf_filespec_params_get_filename(ptr),
			creationDate:
				new Date(libmupdf._wasm_pdf_filespec_params_get_created(ptr) * 1000),
			modificationDate:
				new Date(libmupdf._wasm_pdf_filespec_params_get_modified(ptr) * 1000),
		}
	}

	getEmbeddedFileContents(ref: PDFObject) {
		checkType(ref, PDFObject)
		let contents = libmupdf._wasm_pdf_load_embedded_file_contents(ref.pointer)
		if (contents)
			return new Buffer(contents)
		return null
	}

	getEmbeddedFiles(): Record<string,PDFObject> {
		function _getEmbeddedFilesRec(result: Record<string,PDFObject>, N: PDFObject) {
			var i, n
			if (N.isDictionary()) {
				var NN = N.get("Names")
				if (NN)
					for (i = 0, n = NN.length; i < n; i += 2)
						result[NN.get(i+0).asString()] = NN.get(i+1)
				var NK = N.get("Kids")
				if (NK)
					for (i = 0, n = NK.length; i < n; i += 1)
						_getEmbeddedFilesRec(result, NK.get(i))
			}
			return result
		}
		return _getEmbeddedFilesRec({}, this.getTrailer().get("Root", "Names", "EmbeddedFiles"))
	}

	loadNameTree(treeName: string): Record<string,PDFObject> {
		function _loadNameTreeRec(dict: Record<string,PDFObject>, node: PDFObject) {
			var kids = node.get("Kids")
			if (kids && kids.isArray())
				for (var i = 0; i < kids.length; i += 1)
					_loadNameTreeRec(dict, kids.get(i))
			var names = node.get("Names")
			if (names && names.isArray())
				for (var i = 0; i < names.length; i += 2)
					dict[names.get(i).asString()] = names.get(i+1)
		}
		var node = this.getTrailer().get("Root").get("Names").get(treeName)
		var dict = {}
		if (node.isDictionary())
			_loadNameTreeRec(dict, node)
		return dict
	}

	insertEmbeddedFile(filename: string, filespec: PDFObject) {
		var efs = this.getEmbeddedFiles()
		efs[filename] = filespec
		this._rewriteEmbeddedFiles(efs)
	}

	deleteEmbeddedFile(filename: string) {
		var efs = this.getEmbeddedFiles()
		delete efs[filename]
		this._rewriteEmbeddedFiles(efs)
	}

	_rewriteEmbeddedFiles(efs: Record<string,PDFObject>) {
		var efs_keys = Object.keys(efs)
		efs_keys.sort()
		var root = this.getTrailer().get("Root")
		var root_names = root.get("Names")
		if (!root_names.isDictionary())
			root_names = root.put("Names", this.newDictionary())
		var root_names_efs = root_names.put("EmbeddedFiles", this.newDictionary())
		var root_names_efs_names = root_names_efs.put("Names", this.newArray())
		for (var key of efs_keys) {
			root_names_efs_names.push(this.newString(key))
			root_names_efs_names.push(efs[key])
		}
	}

	saveToBuffer(options: string | Record<string,any> = "") {
		var options_string
		if (typeof options === "object") {
			options_string = Object.entries(options).map(kv => {
				var k: string = kv[0]
				var v: any = kv[1]
				if (v === true)
					return k + "=" + "yes"
				else if (v === false)
					return k + "=" + "no"
				else
					return k + "=" + String(v).replaceAll(",", ":")
			}).join(",")
		} else {
			options_string = options
		}
		return new Buffer(libmupdf._wasm_pdf_write_document_buffer(this.pointer, STRING(options_string)))
	}

	save(filename: string, options: string | Record<string,any> = "") {
		if (node_fs)
			node_fs.writeFileSync(filename, this.saveToBuffer(options).asUint8Array())
		else
			throw new Error("missing 'fs' module")
	}

	static readonly PAGE_LABEL_NONE = "\0"
	static readonly PAGE_LABEL_DECIMAL = "D"
	static readonly PAGE_LABEL_ROMAN_UC = "R"
	static readonly PAGE_LABEL_ROMAN_LC = "r"
	static readonly PAGE_LABEL_ALPHA_UC = "A"
	static readonly PAGE_LABEL_ALPHA_LC = "a"

	setPageLabels(index: number, style = "D", prefix = "", start = 1) {
		libmupdf._wasm_pdf_set_page_labels(this.pointer, index, style.charCodeAt(0), STRING(prefix), start)
	}

	deletePageLabels(index: number) {
		libmupdf._wasm_pdf_delete_page_labels(this.pointer, index)
	}

	wasRepaired() {
		return !!libmupdf._wasm_pdf_was_repaired(this.pointer)
	}

	hasUnsavedChanges() {
		return !!libmupdf._wasm_pdf_has_unsaved_changes(this.pointer)
	}

	countVersions() {
		return libmupdf._wasm_pdf_count_versions(this.pointer)
	}

	countUnsavedVersions() {
		return libmupdf._wasm_pdf_count_unsaved_versions(this.pointer)
	}

	validateChangeHistory() {
		return libmupdf._wasm_pdf_validate_change_history(this.pointer)
	}

	canBeSavedIncrementally() {
		return !!libmupdf._wasm_pdf_can_be_saved_incrementally(this.pointer)
	}

	enableJournal() {
		libmupdf._wasm_pdf_enable_journal(this.pointer)
	}

	getJournal() {
		let position = libmupdf._wasm_pdf_undoredo_state_position(this.pointer)
		let n = libmupdf._wasm_pdf_undoredo_state_count(this.pointer)
		let steps: string[] = []
		for (let i = 0; i < n; ++i)
			steps.push(
				fromString(
					libmupdf._wasm_pdf_undoredo_step(this.pointer, i),
				)
			)
		return { position, steps }
	}

	beginOperation(op: string) {
		libmupdf._wasm_pdf_begin_operation(this.pointer, STRING(op))
	}

	beginImplicitOperation() {
		libmupdf._wasm_pdf_begin_implicit_operation(this.pointer)
	}

	endOperation() {
		libmupdf._wasm_pdf_end_operation(this.pointer)
	}

	abandonOperation() {
		libmupdf._wasm_pdf_abandon_operation(this.pointer)
	}

	canUndo() {
		return !!libmupdf._wasm_pdf_can_undo(this.pointer)
	}

	canRedo() {
		return !!libmupdf._wasm_pdf_can_redo(this.pointer)
	}

	undo() {
		libmupdf._wasm_pdf_undo(this.pointer)
	}

	redo() {
		libmupdf._wasm_pdf_redo(this.pointer)
	}

	isJSSupported() {
		return !!libmupdf._wasm_pdf_js_supported(this.pointer)
	}

	enableJS() {
		libmupdf._wasm_pdf_enable_js(this.pointer)
	}

	disableJS() {
		libmupdf._wasm_pdf_disable_js(this.pointer)
	}

	setJSEventListener(_listener: any) {
		throw "TODO"
	}

	rearrangePages(pages: number[]) {
		let n = pages.length
		let ptr = Malloc<"int">(n << 2)
		for (let i = 0; i < n; ++i)
			libmupdf.HEAPU32[(ptr >> 2) + i] = pages[i] || 0
		try {
			libmupdf._wasm_pdf_rearrange_pages(this.pointer, n, ptr)
		} finally {
			Free(ptr)
		}
	}

	subsetFonts() {
		libmupdf._wasm_pdf_subset_fonts(this.pointer)
	}

	bake(bakeAnnots = true, bakeWidgets = true) {
		libmupdf._wasm_pdf_bake_document(this.pointer, bakeAnnots, bakeWidgets)
	}

	countLayerConfigs(): number {
		return libmupdf._wasm_pdf_count_layer_configs(this.pointer)
	}

	getLayerConfigCreator(config: number): string {
		return fromString(libmupdf._wasm_pdf_layer_config_creator(this.pointer, config))
	}

	getLayerConfigName(config: number): string {
		return fromString(libmupdf._wasm_pdf_layer_config_name(this.pointer, config))
	}

	selectLayerConfig(config: number) {
		libmupdf._wasm_pdf_select_layer_config(this.pointer, config)
	}

	countLayerConfigUIs(): number {
		return libmupdf._wasm_pdf_count_layer_config_uis(this.pointer)
	}

	getLayerConfigUIInfo(configui: number) {
		return fromLayerConfigUIInfo(libmupdf._wasm_pdf_layer_config_ui_info(this.pointer, configui));
	}

	countLayers(): number {
		return libmupdf._wasm_pdf_count_layers(this.pointer)
	}

	isLayerVisible(layer: number): boolean {
		return !!libmupdf._wasm_pdf_layer_is_enabled(this.pointer, layer)
	}

	setLayerVisible(layer: number, visible: boolean): void {
		libmupdf._wasm_pdf_enable_layer(this.pointer, layer, Number(visible))
	}

	getLayerName(layer: number): string {
		return fromString(libmupdf._wasm_pdf_layer_name(this.pointer, layer))
	}

	resetForm(fields: PDFObject, exclude: boolean) {
		libmupdf._wasm_pdf_reset_form(this.pointer, this._PDFOBJ(fields), Number(exclude))
	}
}

export class PDFPage extends Page {
	_doc: PDFDocument
	_annots: PDFAnnotation[] | null
	_widgets: PDFWidget[] | null

	// PRIVATE
	constructor(doc: PDFDocument, pointer: Pointer<"any_page">) {
		super(pointer)
		this._doc = doc
		this._annots = null
		this._widgets = null
	}

	getObject() {
		return this._doc._fromPDFObjectKeep(libmupdf._wasm_pdf_page_get_obj(this.pointer))
	}

	getTransform() {
		return fromMatrix(libmupdf._wasm_pdf_page_transform(this.pointer))
	}

	setPageBox(box: PageBox, rect: Rect) {
		let box_ix = ENUM<PageBox>(box, Page.BOXES)
		checkRect(rect)
		libmupdf._wasm_pdf_set_page_box(this.pointer, box_ix, RECT(rect))
	}

	override toPixmap(matrix: Matrix, colorspace: ColorSpace, alpha = false, showExtras = true, usage = "View", box: PageBox = "CropBox") {
		checkMatrix(matrix)
		checkType(colorspace, ColorSpace)
		let box_ix = ENUM<PageBox>(box, Page.BOXES)
		let result
		if (showExtras)
			result = libmupdf._wasm_pdf_new_pixmap_from_page_with_usage(this.pointer,
				MATRIX(matrix),
				colorspace.pointer,
				alpha,
				STRING(usage),
				box_ix)
		else
			result = libmupdf._wasm_pdf_new_pixmap_from_page_contents_with_usage(this.pointer,
				MATRIX(matrix),
				colorspace.pointer,
				alpha,
				STRING(usage),
				box_ix)
		return new Pixmap(result)
	}

	getWidgets() {
		if (!this._widgets) {
			this._widgets = []
			let widget = libmupdf._wasm_pdf_first_widget(this.pointer)
			while (widget) {
				this._widgets.push(new PDFWidget(this._doc, libmupdf._wasm_pdf_keep_annot(widget)))
				widget = libmupdf._wasm_pdf_next_widget(widget)
			}
		}
		return this._widgets
	}

	getAnnotations() {
		if (!this._annots) {
			this._annots = []
			let annot = libmupdf._wasm_pdf_first_annot(this.pointer)
			while (annot) {
				this._annots.push(new PDFAnnotation(this._doc, libmupdf._wasm_pdf_keep_annot(annot)))
				annot = libmupdf._wasm_pdf_next_annot(annot)
			}
		}
		return this._annots
	}

	createAnnotation(type: PDFAnnotationType) {
		let type_ix = ENUM<PDFAnnotationType>(type, PDFAnnotation.ANNOT_TYPES)
		let annot = new PDFAnnotation(this._doc, libmupdf._wasm_pdf_create_annot(this.pointer, type_ix))
		if (this._annots)
			this._annots.push(annot)
		return annot
	}

	deleteAnnotation(annot: PDFAnnotation) {
		checkType(annot, PDFAnnotation)
		libmupdf._wasm_pdf_delete_annot(this.pointer, annot.pointer)
		if (this._annots) {
			let ix = this._annots.indexOf(annot)
			if (ix >= 0)
				this._annots.splice(ix, 1)
		}
	}

	static readonly REDACT_IMAGE_NONE = 0
	static readonly REDACT_IMAGE_REMOVE = 1
	static readonly REDACT_IMAGE_PIXELS = 2
	static readonly REDACT_IMAGE_UNLESS_INVISIBLE = 3

	static readonly REDACT_LINE_ART_NONE = 0
	static readonly REDACT_LINE_ART_REMOVE_IF_COVERED = 1
	static readonly REDACT_LINE_ART_REMOVE_IF_TOUCHED = 2

	static readonly REDACT_TEXT_REMOVE = 0
	static readonly REDACT_TEXT_NONE = 1

	applyRedactions(black_boxes = true, image_method = 2, line_art_method = 1, text_method = 0) {
		libmupdf._wasm_pdf_redact_page(this.pointer, Number(black_boxes), image_method, line_art_method, text_method)
	}

	update() {
		return !!libmupdf._wasm_pdf_update_page(this.pointer)
	}
}

type PDFObjectPath = Array<number | string | PDFObject>

export class PDFObject extends Userdata<"pdf_obj"> {
	static override readonly _drop = libmupdf._wasm_pdf_drop_obj

	static readonly Null = new PDFObject(null as unknown as PDFDocument, 0 as Pointer<"pdf_obj">)

	_doc: PDFDocument

	// PRIVATE
	constructor(doc: PDFDocument, pointer: Pointer<"pdf_obj">) {
		super(libmupdf._wasm_pdf_keep_obj(pointer))
		this._doc = doc
	}

	isNull() { return this === PDFObject.Null }
	isIndirect() { return !!libmupdf._wasm_pdf_is_indirect(this.pointer) }
	isBoolean() { return !!libmupdf._wasm_pdf_is_bool(this.pointer) }
	isInteger() { return !!libmupdf._wasm_pdf_is_int(this.pointer) }
	isReal() { return !!libmupdf._wasm_pdf_is_real(this.pointer) }
	isNumber() { return !!libmupdf._wasm_pdf_is_number(this.pointer) }
	isName() { return !!libmupdf._wasm_pdf_is_name(this.pointer) }
	isString() { return !!libmupdf._wasm_pdf_is_string(this.pointer) }
	isArray() { return !!libmupdf._wasm_pdf_is_array(this.pointer) }
	isDictionary() { return !!libmupdf._wasm_pdf_is_dict(this.pointer) }
	isStream() { return !!libmupdf._wasm_pdf_is_stream(this.pointer) }

	asIndirect(): number { return libmupdf._wasm_pdf_to_num(this.pointer) }
	asBoolean() { return !!libmupdf._wasm_pdf_to_bool(this.pointer) }
	asNumber(): number { return libmupdf._wasm_pdf_to_real(this.pointer) }
	asName() { return fromString(libmupdf._wasm_pdf_to_name(this.pointer)) }
	asString() { return fromString(libmupdf._wasm_pdf_to_text_string(this.pointer)) }

	asByteString() {
		let ptr = libmupdf._wasm_pdf_to_string(this.pointer, _wasm_int)
		let len = libmupdf.HEAPU32[_wasm_int >> 2] as number
		return libmupdf.HEAPU8.slice(ptr, ptr + len)
	}

	readStream() { return new Buffer(libmupdf._wasm_pdf_load_stream(this.pointer)) }
	readRawStream() { return new Buffer(libmupdf._wasm_pdf_load_raw_stream(this.pointer)) }

	writeObject(obj: any) {
		if (!this.isIndirect())
			throw new TypeError("can only call PDFObject.writeObject on an indirect reference")
		libmupdf._wasm_pdf_update_object(this._doc.pointer, this.asIndirect(), this._doc._PDFOBJ(obj))
	}

	writeStream(buf: AnyBuffer) {
		if (!this.isIndirect())
			throw new TypeError("can only call PDFObject.writeStream on an indirect reference")
		libmupdf._wasm_pdf_update_stream(this._doc.pointer, this.pointer, BUFFER(buf), 0)
	}

	writeRawStream(buf: AnyBuffer) {
		if (!this.isIndirect())
			throw new TypeError("can only call PDFObject.writeRawStream on an indirect reference")
		libmupdf._wasm_pdf_update_stream(this._doc.pointer, this.pointer, BUFFER(buf), 1)
	}

	resolve() {
		return this._doc._fromPDFObjectKeep(libmupdf._wasm_pdf_resolve_indirect(this.pointer))
	}

	get length() {
		return libmupdf._wasm_pdf_array_len(this.pointer)
	}

	set length(_: number) {
		throw new TypeError("object length is read-only")
	}

	_get(path: PDFObjectPath) {
		let obj = this.pointer
		for (let key of path) {
			if (typeof key === "number")
				obj = libmupdf._wasm_pdf_array_get(obj, key)
			else if (key instanceof PDFObject)
				obj = libmupdf._wasm_pdf_dict_get(obj, key.pointer)
			else
				obj = libmupdf._wasm_pdf_dict_gets(obj, STRING(key))
			if (obj === 0)
				break
		}
		return obj
	}

	get(...path: PDFObjectPath): PDFObject {
		return this._doc._fromPDFObjectKeep(this._get(path))
	}

	getInheritable(key: string | PDFObject) {
		if (key instanceof PDFObject)
			return this._doc._fromPDFObjectKeep(libmupdf._wasm_pdf_dict_get_inheritable(this.pointer, key.pointer))
		return this._doc._fromPDFObjectKeep(libmupdf._wasm_pdf_dict_gets_inheritable(this.pointer, STRING(key)))
	}

	put(key: number | string | PDFObject, value: any) {
		value = this._doc._toPDFObject(value)
		if (typeof key === "number")
			libmupdf._wasm_pdf_array_put(this.pointer, key, value.pointer)
		else if (key instanceof PDFObject)
			libmupdf._wasm_pdf_dict_put(this.pointer, key.pointer, value.pointer)
		else
			libmupdf._wasm_pdf_dict_puts(this.pointer, STRING(key), value.pointer)
		return value
	}

	push(value: any) {
		value = this._doc._toPDFObject(value)
		libmupdf._wasm_pdf_array_push(this.pointer, value.pointer)
		return value
	}

	delete(key: number | string | PDFObject) {
		if (typeof key === "number")
			libmupdf._wasm_pdf_array_delete(this.pointer, key)
		else if (key instanceof PDFObject)
			libmupdf._wasm_pdf_dict_del(this.pointer, key.pointer)
		else
			libmupdf._wasm_pdf_dict_dels(this.pointer, STRING(key))
	}

	override valueOf() {
		if (this.isNull()) return null
		if (this.isBoolean()) return this.asBoolean()
		if (this.isNumber()) return this.asNumber()
		if (this.isName()) return this.asName()
		if (this.isString()) return this.asString()
		if (this.isIndirect()) return `${this.asIndirect()} 0 R`
		return this
	}

	override toString(tight = true, ascii = true) {
		return fromStringFree(libmupdf._wasm_pdf_sprint_obj(this.pointer, tight, ascii))
	}

	forEach(fn: (val: PDFObject, key: number | string, self: PDFObject) => void) {
		if (this.isArray()) {
			let n = this.length
			for (let i = 0; i < n; ++i)
				fn(this.get(i), i, this)
		} else if (this.isDictionary()) {
			let n = libmupdf._wasm_pdf_dict_len(this.pointer)
			for (let i = 0; i < n; ++i) {
				let key = this._doc._fromPDFObjectKeep(libmupdf._wasm_pdf_dict_get_key(this.pointer, i))
				let val = this._doc._fromPDFObjectKeep(libmupdf._wasm_pdf_dict_get_val(this.pointer, i))
				fn(val, key.asName(), this)
			}
		}
	}

	// Convert to plain Javascript values, objects, and arrays.
	// If you want to resolve indirect references, pass an empty object or array as the first argument.
	// On exit, this object will contain all indirect objects encountered indexed by object number.
	// Note: This function will omit cyclic references.
	asJS(seen?: Record<number,PDFObject>): any {
		if (this.isIndirect()) {
			let ref = this.asIndirect()
			if (!seen)
				return `${ref} 0 R`
			if (ref in seen)
				return seen[ref]
			seen[ref] = PDFObject.Null // stop recursion!
			return seen[ref] = this.resolve().asJS(seen)
		}

		if (this.isArray()) {
			let result: any[] = []
			this.forEach(val => {
				result.push(val.asJS(seen))
			})
			return result
		}

		if (this.isDictionary()) {
			let result: Record<string,any> = {}
			this.forEach((val, key) => {
				result[key] = val.asJS(seen)
			})
			return result
		}

		return this.valueOf()
	}
}

export class PDFGraftMap extends Userdata<"pdf_graft_map"> {
	static override readonly _drop = libmupdf._wasm_pdf_drop_graft_map

	_doc: PDFDocument

	// PRIVATE
	constructor(doc: PDFDocument, pointer: Pointer<"pdf_graft_map">) {
		super(pointer)
		this._doc = doc
	}

	graftObject(obj: PDFObject) {
		checkType(obj, PDFObject)
		return this._doc._fromPDFObjectNew(libmupdf._wasm_pdf_graft_mapped_object(this.pointer, obj.pointer))
	}

	graftPage(to: number, srcDoc: PDFDocument, srcPage: number) {
		checkType(to, "number")
		checkType(srcDoc, PDFDocument)
		checkType(srcPage, "number")
		libmupdf._wasm_pdf_graft_mapped_page(this.pointer, to, srcDoc.pointer, srcPage)
	}
}

export type PDFAnnotationType =
	"Text" |
	"Link" |
	"FreeText" |
	"Line" |
	"Square" |
	"Circle" |
	"Polygon" |
	"PolyLine" |
	"Highlight" |
	"Underline" |
	"Squiggly" |
	"StrikeOut" |
	"Redact" |
	"Stamp" |
	"Caret" |
	"Ink" |
	"Popup" |
	"FileAttachment" |
	"Sound" |
	"Movie" |
	"RichMedia" |
	"Widget" |
	"Screen" |
	"PrinterMark" |
	"TrapNet" |
	"Watermark" |
	"3D" |
	"Projection"

export type PDFAnnotationLineEndingStyle =
	"None" |
	"Square" |
	"Circle" |
	"Diamond" |
	"OpenArrow" |
	"ClosedArrow" |
	"Butt" |
	"ROpenArrow" |
	"RClosedArrow" |
	"Slash"

export type PDFAnnotationBorderStyle = "Solid" | "Dashed" | "Beveled" | "Inset" | "Underline"

export type PDFAnnotationBorderEffect = "None" | "Cloudy"

export type PDFAnnotationIntent =
	null |
	"FreeTextCallout" |
	"FreeTextTypeWriter" |
	"LineArrow" |
	"LineDimension" |
	"PloyLine" |
	"PolygonCloud" |
	"PolygonDimension" |
	"StampImage" |
	"StampSnapshot"

export class PDFAnnotation extends Userdata<"pdf_annot"> {
	static override readonly _drop = libmupdf._wasm_pdf_drop_annot

	_doc: PDFDocument

	/* IMPORTANT: Keep in sync with mupdf/pdf/annot.h and PDFAnnotation.java */
	static readonly ANNOT_TYPES: PDFAnnotationType[] = [
		"Text",
		"Link",
		"FreeText",
		"Line",
		"Square",
		"Circle",
		"Polygon",
		"PolyLine",
		"Highlight",
		"Underline",
		"Squiggly",
		"StrikeOut",
		"Redact",
		"Stamp",
		"Caret",
		"Ink",
		"Popup",
		"FileAttachment",
		"Sound",
		"Movie",
		"RichMedia",
		"Widget",
		"Screen",
		"PrinterMark",
		"TrapNet",
		"Watermark",
		"3D",
		"Projection",
	]

	static readonly LINE_ENDING: PDFAnnotationLineEndingStyle[] = [
		"None",
		"Square",
		"Circle",
		"Diamond",
		"OpenArrow",
		"ClosedArrow",
		"Butt",
		"ROpenArrow",
		"RClosedArrow",
		"Slash",
	]

	static readonly BORDER_STYLE: PDFAnnotationBorderStyle[] = [ "Solid", "Dashed", "Beveled", "Inset", "Underline" ]

	static readonly BORDER_EFFECT: PDFAnnotationBorderEffect[] = [ "None", "Cloudy" ]

	static readonly INTENT: PDFAnnotationIntent[] = [
		null,
		"FreeTextCallout",
		"FreeTextTypeWriter",
		"LineArrow",
		"LineDimension",
		"PloyLine",
		"PolygonCloud",
		"PolygonDimension",
		"StampImage",
		"StampSnapshot"
	]

	// Bit masks for getFlags and setFlags
	static readonly IS_INVISIBLE = 1 << (1 - 1)
	static readonly IS_HIDDEN = 1 << (2 - 1)
	static readonly IS_PRINT = 1 << (3 - 1)
	static readonly IS_NO_ZOOM = 1 << (4 - 1)
	static readonly IS_NO_ROTATE = 1 << (5 - 1)
	static readonly IS_NO_VIEW = 1 << (6 - 1)
	static readonly IS_READ_ONLY = 1 << (7 - 1)
	static readonly IS_LOCKED = 1 << (8 - 1)
	static readonly IS_TOGGLE_NO_VIEW = 1 << (9 - 1)
	static readonly IS_LOCKED_CONTENTS = 1 << (10 - 1)

	// PRIVATE
	constructor(doc: PDFDocument, pointer: Pointer<"pdf_annot">) {
		super(pointer)
		this._doc = doc
	}

	getObject() {
		return this._doc._fromPDFObjectKeep(libmupdf._wasm_pdf_annot_obj(this.pointer))
	}

	getBounds() {
		return fromRect(libmupdf._wasm_pdf_bound_annot(this.pointer))
	}

	run(device: Device, matrix: Matrix) {
		checkType(device, Device)
		checkMatrix(matrix)
		libmupdf._wasm_pdf_run_annot(this.pointer, device.pointer, MATRIX(matrix))
	}

	toPixmap(matrix: Matrix, colorspace: ColorSpace, alpha = false) {
		checkMatrix(matrix)
		checkType(colorspace, ColorSpace)
		return new Pixmap(
			libmupdf._wasm_pdf_new_pixmap_from_annot(
				this.pointer,
				MATRIX(matrix),
				colorspace.pointer,
				alpha)
		)
	}

	toDisplayList() {
		return new DisplayList(libmupdf._wasm_pdf_new_display_list_from_annot(this.pointer))
	}

	update() {
		return !!libmupdf._wasm_pdf_update_annot(this.pointer)
	}

	getType() {
		let type = libmupdf._wasm_pdf_annot_type(this.pointer)
		return PDFAnnotation.ANNOT_TYPES[type] || "Text"
	}

	getLanguage() {
		return fromStringOrNull(libmupdf._wasm_pdf_annot_language(this.pointer))
	}

	setLanguage(lang: string) {
		libmupdf._wasm_pdf_set_annot_language(this.pointer, STRING(lang))
	}

	getFlags() {
		return libmupdf._wasm_pdf_annot_flags(this.pointer)
	}

	setFlags(flags: number) {
		return libmupdf._wasm_pdf_set_annot_flags(this.pointer, flags)
	}

	getContents() {
		return fromString(libmupdf._wasm_pdf_annot_contents(this.pointer))
	}

	setContents(text: string) {
		libmupdf._wasm_pdf_set_annot_contents(this.pointer, STRING(text))
	}

	getAuthor() {
		return fromString(libmupdf._wasm_pdf_annot_author(this.pointer))
	}

	setAuthor(text: string) {
		libmupdf._wasm_pdf_set_annot_author(this.pointer, STRING(text))
	}

	getCreationDate() {
		return new Date(libmupdf._wasm_pdf_annot_creation_date(this.pointer) * 1000)
	}

	setCreationDate(date: Date) {
		checkType(date, Date)
		libmupdf._wasm_pdf_set_annot_creation_date(this.pointer, date.getTime() / 1000)
	}

	getModificationDate() {
		return new Date(libmupdf._wasm_pdf_annot_modification_date(this.pointer) * 1000)
	}

	setModificationDate(date: Date) {
		checkType(date, Date)
		libmupdf._wasm_pdf_set_annot_modification_date(this.pointer, date.getTime() / 1000)
	}

	hasRect() {
		return !!libmupdf._wasm_pdf_annot_has_rect(this.pointer)
	}
	hasInkList() {
		return !!libmupdf._wasm_pdf_annot_has_ink_list(this.pointer)
	}
	hasQuadPoints() {
		return !!libmupdf._wasm_pdf_annot_has_quad_points(this.pointer)
	}
	hasVertices() {
		return !!libmupdf._wasm_pdf_annot_has_vertices(this.pointer)
	}
	hasLine() {
		return !!libmupdf._wasm_pdf_annot_has_line(this.pointer)
	}
	hasInteriorColor() {
		return !!libmupdf._wasm_pdf_annot_has_interior_color(this.pointer)
	}
	hasLineEndingStyles() {
		return !!libmupdf._wasm_pdf_annot_has_line_ending_styles(this.pointer)
	}
	hasBorder() {
		return !!libmupdf._wasm_pdf_annot_has_border(this.pointer)
	}
	hasBorderEffect() {
		return !!libmupdf._wasm_pdf_annot_has_border_effect(this.pointer)
	}
	hasIcon() {
		return !!libmupdf._wasm_pdf_annot_has_icon_name(this.pointer)
	}
	hasOpen() {
		return !!libmupdf._wasm_pdf_annot_has_open(this.pointer)
	}
	hasAuthor() {
		return !!libmupdf._wasm_pdf_annot_has_author(this.pointer)
	}
	hasFilespec() {
		return !!libmupdf._wasm_pdf_annot_has_filespec(this.pointer)
	}
	hasCallout() {
		return !!libmupdf._wasm_pdf_annot_has_callout(this.pointer)
	}
	hasRichContents() {
		return !!libmupdf._wasm_pdf_annot_has_rich_contents(this.pointer)
	}

	getRect() {
		return fromRect(libmupdf._wasm_pdf_annot_rect(this.pointer))
	}

	setRect(rect: Rect) {
		checkRect(rect)
		libmupdf._wasm_pdf_set_annot_rect(this.pointer, RECT(rect))
	}

	getPopup() {
		return fromRect(libmupdf._wasm_pdf_annot_popup(this.pointer))
	}

	setPopup(rect: Rect) {
		checkRect(rect)
		libmupdf._wasm_pdf_set_annot_popup(this.pointer, RECT(rect))
	}

	getIsOpen() {
		return !!libmupdf._wasm_pdf_annot_is_open(this.pointer)
	}

	setIsOpen(isOpen: boolean) {
		checkType(isOpen, "boolean")
		libmupdf._wasm_pdf_set_annot_is_open(this.pointer, isOpen)
	}

	getHiddenForEditing() {
		return !!libmupdf._wasm_pdf_annot_hidden_for_editing(this.pointer)
	}

	setHiddenForEditing(isHidden: boolean) {
		checkType(isHidden, "boolean")
		libmupdf._wasm_pdf_set_annot_hidden_for_editing(this.pointer, isHidden)
	}

	getIcon() {
		return fromString(libmupdf._wasm_pdf_annot_icon_name(this.pointer))
	}

	setIcon(text: string) {
		checkType(text, "string")
		libmupdf._wasm_pdf_set_annot_icon_name(this.pointer, STRING(text))
	}

	getOpacity() {
		return libmupdf._wasm_pdf_annot_opacity(this.pointer)
	}

	setOpacity(opacity: number) {
		checkType(opacity, "number")
		libmupdf._wasm_pdf_set_annot_opacity(this.pointer, opacity)
	}

	getQuadding() {
		return libmupdf._wasm_pdf_annot_quadding(this.pointer)
	}

	setQuadding(quadding: number) {
		checkType(quadding, "number")
		libmupdf._wasm_pdf_set_annot_quadding(this.pointer, quadding)
	}

	getLine() {
		let a = fromPoint(libmupdf._wasm_pdf_annot_line_1(this.pointer))
		let b = fromPoint(libmupdf._wasm_pdf_annot_line_2(this.pointer))
		return [ a, b ]
	}

	setLine(a: Point, b: Point) {
		checkPoint(a)
		checkPoint(b)
		libmupdf._wasm_pdf_set_annot_line(this.pointer, POINT(a), POINT2(b))
	}

	getLineEndingStyles() {
		let a = libmupdf._wasm_pdf_annot_line_ending_styles_start(this.pointer)
		let b = libmupdf._wasm_pdf_annot_line_ending_styles_end(this.pointer)
		return {
			start: PDFAnnotation.LINE_ENDING[a] || "None",
			end: PDFAnnotation.LINE_ENDING[b] || "None",
		}
	}

	setLineEndingStyles(start: PDFAnnotationLineEndingStyle, end: PDFAnnotationLineEndingStyle) {
		let start_ix = ENUM<PDFAnnotationLineEndingStyle>(start, PDFAnnotation.LINE_ENDING)
		let end_ix = ENUM<PDFAnnotationLineEndingStyle>(end, PDFAnnotation.LINE_ENDING)
		libmupdf._wasm_pdf_set_annot_line_ending_styles(this.pointer, start_ix, end_ix)
	}

	getLineCaption() {
		return libmupdf._wasm_pdf_annot_line_caption(this.pointer)
	}

	setLineCaption(on: boolean) {
		return libmupdf._wasm_pdf_set_annot_line_caption(this.pointer, on)
	}

	getLineCaptionOffset() {
		return fromPoint(libmupdf._wasm_pdf_annot_line_caption_offset(this.pointer))
	}

	setLineCaptionOffset(p: Point) {
		return libmupdf._wasm_pdf_set_annot_line_caption_offset(this.pointer, POINT(p))
	}

	getLineLeader() {
		return libmupdf._wasm_pdf_annot_line_leader(this.pointer)
	}

	getLineLeaderExtension() {
		return libmupdf._wasm_pdf_annot_line_leader_extension(this.pointer)
	}

	getLineLeaderOffset() {
		return libmupdf._wasm_pdf_annot_line_leader_offset(this.pointer)
	}

	setLineLeader(v: number) {
		return libmupdf._wasm_pdf_set_annot_line_leader(this.pointer, v)
	}

	setLineLeaderExtension(v: number) {
		return libmupdf._wasm_pdf_set_annot_line_leader_extension(this.pointer, v)
	}

	setLineLeaderOffset(v: number) {
		return libmupdf._wasm_pdf_set_annot_line_leader_offset(this.pointer, v)
	}

	getCalloutStyle() {
		let style = libmupdf._wasm_pdf_annot_callout_style(this.pointer)
		return PDFAnnotation.LINE_ENDING[style] || "None"
	}

	setCalloutStyle(style: PDFAnnotationLineEndingStyle) {
		let style_ix = ENUM<PDFAnnotationLineEndingStyle>(style, PDFAnnotation.LINE_ENDING)
		libmupdf._wasm_pdf_set_annot_callout_style(this.pointer, style_ix)
	}

	getCalloutLine() {
		let n = libmupdf._wasm_pdf_annot_callout_line(this.pointer,
			(_wasm_point << 2) as Pointer<"fz_point">)
		if (n == 3)
			return [
				fromPoint((_wasm_point+0) << 2 as Pointer<"fz_point">),
				fromPoint((_wasm_point+1) << 2 as Pointer<"fz_point">),
				fromPoint((_wasm_point+2) << 2 as Pointer<"fz_point">)
			]
		if (n == 2)
			return [
				fromPoint((_wasm_point+0) << 2 as Pointer<"fz_point">),
				fromPoint((_wasm_point+1) << 2 as Pointer<"fz_point">)
			]
		return null
	}

	setCalloutLine(line: Point[]) {
		let a = line[0] || [0, 0]
		let b = line[1] || [0, 0]
		let c = line[2] || [0, 0]
		libmupdf._wasm_pdf_set_annot_callout_line(this.pointer, line.length, POINT(a), POINT2(b), POINT3(c))
	}

	getCalloutPoint() {
		let line = this.getCalloutLine()
		if (line)
			return line[0]
		return undefined
	}

	setCalloutPoint(p: Point) {
		libmupdf._wasm_pdf_set_annot_callout_point(this.pointer, POINT(p))
	}

	getColor() {
		return fromAnnotColor(libmupdf._wasm_pdf_annot_color(this.pointer, COLOR()))
	}

	getInteriorColor() {
		return fromAnnotColor(libmupdf._wasm_pdf_annot_interior_color(this.pointer, COLOR()))
	}

	setColor(color: AnnotColor) {
		checkAnnotColor(color)
		libmupdf._wasm_pdf_set_annot_color(this.pointer, color.length, COLOR(color))
	}

	setInteriorColor(color: AnnotColor) {
		checkAnnotColor(color)
		libmupdf._wasm_pdf_set_annot_interior_color(this.pointer, color.length, COLOR(color))
	}

	getBorderWidth() {
		return libmupdf._wasm_pdf_annot_border_width(this.pointer)
	}

	setBorderWidth(value: number) {
		checkType(value, "number")
		return libmupdf._wasm_pdf_set_annot_border_width(this.pointer, value)
	}

	getBorderStyle() {
		return PDFAnnotation.BORDER_STYLE[libmupdf._wasm_pdf_annot_border_style(this.pointer)] || "Solid"
	}

	setBorderStyle(value: PDFAnnotationBorderStyle) {
		let value_ix = ENUM<PDFAnnotationBorderStyle>(value, PDFAnnotation.BORDER_STYLE)
		return libmupdf._wasm_pdf_set_annot_border_style(this.pointer, value_ix)
	}

	getBorderEffect() {
		return PDFAnnotation.BORDER_EFFECT[libmupdf._wasm_pdf_annot_border_effect(this.pointer)] || "None"
	}

	setBorderEffect(value: PDFAnnotationBorderEffect) {
		let value_ix = ENUM<PDFAnnotationBorderEffect>(value, PDFAnnotation.BORDER_EFFECT)
		return libmupdf._wasm_pdf_set_annot_border_effect(this.pointer, value_ix)
	}

	getBorderEffectIntensity() {
		return libmupdf._wasm_pdf_annot_border_effect_intensity(this.pointer)
	}

	setBorderEffectIntensity(value: number) {
		checkType(value, "number")
		return libmupdf._wasm_pdf_set_annot_border_effect_intensity(this.pointer, value)
	}

	getBorderDashCount() {
		return libmupdf._wasm_pdf_annot_border_dash_count(this.pointer)
	}

	getBorderDashItem(idx: number) {
		return libmupdf._wasm_pdf_annot_border_dash_item(this.pointer, idx)
	}

	clearBorderDash() {
		libmupdf._wasm_pdf_clear_annot_border_dash(this.pointer)
	}

	addBorderDashItem(v: number) {
		checkType(v, "number")
		return libmupdf._wasm_pdf_add_annot_border_dash_item(this.pointer, v)
	}

	getBorderDashPattern() {
		let n = this.getBorderDashCount()
		let result = new Array(n)
		for (let i = 0; i < n; ++i)
			result[i] = this.getBorderDashItem(i)
		return result
	}

	setBorderDashPattern(list: number[]) {
		this.clearBorderDash()
		for (let v of list)
			this.addBorderDashItem(v)
	}

	getIntent(): PDFAnnotationIntent {
		return PDFAnnotation.INTENT[libmupdf._wasm_pdf_annot_intent(this.pointer)] || null
	}

	setIntent(value: PDFAnnotationIntent) {
		let value_ix = ENUM<PDFAnnotationIntent>(value, PDFAnnotation.INTENT)
		return libmupdf._wasm_pdf_set_annot_intent(this.pointer, value_ix)
	}

	setDefaultAppearance(fontName: string, size: number, color: AnnotColor) {
		checkType(fontName, "string")
		checkType(size, "number")
		checkAnnotColor(color)
		libmupdf._wasm_pdf_set_annot_default_appearance(this.pointer, STRING(fontName), size, color.length, COLOR(color))
	}

	getDefaultAppearance() {
		let font = fromString(libmupdf._wasm_pdf_annot_default_appearance_font(this.pointer))
		let size = libmupdf._wasm_pdf_annot_default_appearance_size(this.pointer)
		let color = fromAnnotColor(libmupdf._wasm_pdf_annot_default_appearance_color(this.pointer, COLOR()))
		return { font, size, color }
	}

	getFileSpec() {
		return this._doc._fromPDFObjectKeep(libmupdf._wasm_pdf_annot_filespec(this.pointer))
	}

	setFileSpec(fs: PDFObject) {
		return libmupdf._wasm_pdf_set_annot_filespec(this.pointer, this._doc._PDFOBJ(fs))
	}

	getQuadPoints() {
		let n = libmupdf._wasm_pdf_annot_quad_point_count(this.pointer)
		let result: Quad[] = []
		for (let i = 0; i < n; ++i)
			result.push(fromQuad(libmupdf._wasm_pdf_annot_quad_point(this.pointer, i)))
		return result
	}

	clearQuadPoints() {
		libmupdf._wasm_pdf_clear_annot_quad_points(this.pointer)
	}

	addQuadPoint(quad: Quad) {
		checkQuad(quad)
		libmupdf._wasm_pdf_add_annot_quad_point(this.pointer, QUAD(quad))
	}

	setQuadPoints(quadlist: Quad[]) {
		this.clearQuadPoints()
		for (let quad of quadlist)
			this.addQuadPoint(quad)
	}

	getVertices() {
		let n = libmupdf._wasm_pdf_annot_vertex_count(this.pointer)
		let result: Point[] = new Array(n)
		for (let i = 0; i < n; ++i)
			result[i] = fromPoint(libmupdf._wasm_pdf_annot_vertex(this.pointer, i))
		return result
	}

	clearVertices() {
		libmupdf._wasm_pdf_clear_annot_vertices(this.pointer)
	}

	addVertex(vertex: Point) {
		checkPoint(vertex)
		libmupdf._wasm_pdf_add_annot_vertex(this.pointer, POINT(vertex))
	}

	setVertices(vertexlist: Point[]) {
		this.clearVertices()
		for (let vertex of vertexlist)
			this.addVertex(vertex)
	}

	getInkList() {
		let n = libmupdf._wasm_pdf_annot_ink_list_count(this.pointer)
		let outer: Point[][] = []
		for (let i = 0; i < n; ++i) {
			let m = libmupdf._wasm_pdf_annot_ink_list_stroke_count(this.pointer, i)
			let inner: Point[] = new Array(m)
			for (let k = 0; k < m; ++k)
				inner[k] = fromPoint(libmupdf._wasm_pdf_annot_ink_list_stroke_vertex(this.pointer, i, k))
			outer.push(inner)
		}
		return outer
	}

	clearInkList() {
		libmupdf._wasm_pdf_clear_annot_ink_list(this.pointer)
	}

	addInkListStroke() {
		libmupdf._wasm_pdf_add_annot_ink_list_stroke(this.pointer)
	}

	addInkListStrokeVertex(v: Point) {
		checkPoint(v)
		libmupdf._wasm_pdf_add_annot_ink_list_stroke_vertex(this.pointer, POINT(v))
	}

	setInkList(inklist: Point[][]) {
		this.clearInkList()
		for (let stroke of inklist) {
			this.addInkListStroke()
			for (let vertex of stroke)
				this.addInkListStrokeVertex(vertex)
		}
	}

	getRichContents() {
		return fromString(libmupdf._wasm_pdf_annot_rich_contents(this.pointer))
	}

	setRichContents(plain: string, html: string) {
		checkType(plain, "string")
		checkType(html, "string")
		libmupdf._wasm_pdf_set_annot_rich_contents(this.pointer, STRING(plain), STRING2(html))
	}

	getRichDefaults() {
		return fromString(libmupdf._wasm_pdf_annot_rich_defaults(this.pointer))
	}

	setRichDefaults(style: string) {
		checkType(style, "string")
		libmupdf._wasm_pdf_set_annot_rich_defaults(this.pointer, STRING(style))
	}

	setStampImage(image: Image) {
		libmupdf._wasm_pdf_set_annot_stamp_image(this.pointer, image.pointer)
	}

	setAppearanceFromDisplayList(appearance: string | null, state: string | null, transform: Matrix, list: DisplayList) {
		checkMatrix(transform)
		checkType(list, DisplayList)
		libmupdf._wasm_pdf_set_annot_appearance_from_display_list(
			this.pointer,
			STRING_OPT(appearance),
			STRING2_OPT(state),
			MATRIX(transform),
			list.pointer
		)
	}

	setAppearance(appearance: string | null, state: string | null, transform: Matrix, bbox: Rect, resources: any, contents: AnyBuffer) {
		checkMatrix(transform)
		checkRect(bbox)
		libmupdf._wasm_pdf_set_annot_appearance(
			this.pointer,
			STRING_OPT(appearance),
			STRING2_OPT(state),
			MATRIX(transform),
			RECT(bbox),
			this._doc._PDFOBJ(resources),
			BUFFER(contents)
		)
	}

	applyRedaction(black_boxes = 1, image_method = 2, line_art_method = 1, text_method = 0) {
		libmupdf._wasm_pdf_apply_redaction(this.pointer, black_boxes, image_method, line_art_method, text_method)
	}
}

export class PDFWidget extends PDFAnnotation {
	/* IMPORTANT: Keep in sync with mupdf/pdf/widget.h and PDFWidget.java */
	static readonly WIDGET_TYPES = [
		"widget", // unknown
		"button",
		"checkbox",
		"combobox",
		"listbox",
		"radiobutton",
		"signature",
		"text",
	]

	/* Field flags */
	static readonly FIELD_IS_READ_ONLY = 1
	static readonly FIELD_IS_REQUIRED = 1 << 1
	static readonly FIELD_IS_NO_EXPORT = 1 << 2

	/* Text fields */
	static readonly TX_FIELD_IS_MULTILINE = 1 << 12
	static readonly TX_FIELD_IS_PASSWORD = 1 << 13
	static readonly TX_FIELD_IS_COMB = 1 << 24

	/* Button fields */
	static readonly BTN_FIELD_IS_NO_TOGGLE_TO_OFF = 1 << 14
	static readonly BTN_FIELD_IS_RADIO = 1 << 15
	static readonly BTN_FIELD_IS_PUSHBUTTON = 1 << 16

	/* Choice fields */
	static readonly CH_FIELD_IS_COMBO = 1 << 17
	static readonly CH_FIELD_IS_EDIT = 1 << 18
	static readonly CH_FIELD_IS_SORT = 1 << 19
	static readonly CH_FIELD_IS_MULTI_SELECT = 1 << 21

	getFieldType() {
		return PDFWidget.WIDGET_TYPES[libmupdf._wasm_pdf_annot_field_type(this.pointer)] || "button"
	}

	isButton() {
		let type = this.getFieldType()
		return type === "button" || type === "checkbox" || type === "radiobutton"
	}

	isPushButton() {
		return this.getFieldType() === "button"
	}

	isCheckbox() {
		return this.getFieldType() === "checkbox"
	}

	isRadioButton() {
		return this.getFieldType() === "radiobutton"
	}

	isText() {
		return this.getFieldType() === "text"
	}

	isChoice() {
		let type = this.getFieldType()
		return type === "combobox" || type === "listbox"
	}

	isListBox() {
		return this.getFieldType() === "listbox"
	}

	isComboBox() {
		return this.getFieldType() === "combobox"
	}

	getFieldFlags() {
		return libmupdf._wasm_pdf_annot_field_flags(this.pointer)
	}

	isMultiline() {
		return (this.getFieldFlags() & PDFWidget.TX_FIELD_IS_MULTILINE) !== 0
	}

	isPassword() {
		return (this.getFieldFlags() & PDFWidget.TX_FIELD_IS_PASSWORD) !== 0
	}

	isComb() {
		return (this.getFieldFlags() & PDFWidget.TX_FIELD_IS_COMB) !== 0
	}

	isReadOnly() {
		return (this.getFieldFlags() & PDFWidget.FIELD_IS_READ_ONLY) !== 0
	}

	getLabel() {
		return fromString(libmupdf._wasm_pdf_annot_field_label(this.pointer))
	}

	getName() {
		return fromStringFree(libmupdf._wasm_pdf_load_field_name(this.pointer))
	}

	getValue() {
		return fromString(libmupdf._wasm_pdf_annot_field_value(this.pointer))
	}

	setTextValue(value: string) {
		return libmupdf._wasm_pdf_set_annot_text_field_value(this.pointer, STRING(value))
	}

	getMaxLen() {
		return libmupdf._wasm_pdf_annot_text_widget_max_len(this.pointer)
	}

	setChoiceValue(value: string) {
		return libmupdf._wasm_pdf_set_annot_choice_field_value(this.pointer, STRING(value))
	}

	getOptions(isExport=false) {
		let result: string[] = []
		let n = libmupdf._wasm_pdf_annot_choice_field_option_count(this.pointer)
		for (let i = 0; i < n; ++i) {
			result.push(
				fromString(
					libmupdf._wasm_pdf_annot_choice_field_option(this.pointer, isExport, i)
				)
			)
		}
		return result
	}

	toggle() {
		return libmupdf._wasm_pdf_toggle_widget(this.pointer)
	}

	// Interactive Text Widget editing in a GUI.
	// TODO: getEditingState()
	// TODO: setEditingState()
	// TODO: clearEditingState()
	// TODO: layoutTextWidget()

	// Interactive form validation Javascript triggers.
	// NOTE: No embedded PDF Javascript engine in WASM build.
	// TODO: eventEnter()
	// TODO: eventExit()
	// TODO: eventDown()
	// TODO: eventUp()
	// TODO: eventFocus()
	// TODO: eventBlur()

	// NOTE: No OpenSSL support in WASM build.
	// TODO: isSigned()
	// TODO: validateSignature()
	// TODO: checkCertificate()
	// TODO: checkDigest()
	// TODO: getSignature()
	// TODO: previewSignature()
	// TODO: clearSignature()
	// TODO: sign()
}

/* -------------------------------------------------------------------------- */

/* We need a certain level of ugliness to allow callbacks from C to JS */

declare global {
	function $libmupdf_stm_close(ptr: number): void
	function $libmupdf_stm_seek(ptr: number, pos: number, offset: number, whence: number): number
	function $libmupdf_stm_read(ptr: number, pos: number, addr: number, size: number): number
}

interface StreamHandle {
	fileSize(): number,
	read(memory: Uint8Array, offset: number, length: number, position: number): number,
	close(): void,
}

var $libmupdf_stm_id = 0
var $libmupdf_stm_table: Map<number,StreamHandle> = new Map()

globalThis.$libmupdf_stm_close = function (id: number) {
	let handle = $libmupdf_stm_table.get(id)
	if (handle) {
		handle.close()
		$libmupdf_stm_table.delete(id)
		return
	}
	throw new Error("invalid file handle")
}

globalThis.$libmupdf_stm_seek = function (id: number, pos: number, offset: number, whence: number) {
	let handle = $libmupdf_stm_table.get(id)
	if (handle) {
		if (whence === 0)
			return offset
		if (whence === 1)
			return pos + offset
		if (whence === 2) {
			let size = handle.fileSize()
			if (size < 0)
				return -1
			return size + offset
		}
		throw new Error("invalid whence argument")
	}
	throw new Error("invalid file handle")
}

globalThis.$libmupdf_stm_read = function (id: number, pos: number, addr: number, size: number) {
	let handle = $libmupdf_stm_table.get(id)
	if (handle) {
		return handle.read(libmupdf.HEAPU8, addr, size, pos)
	}
	throw new Error("invalid file handle")
}

export class Stream extends Userdata<"fz_stream"> {
	static override readonly _drop = libmupdf._wasm_drop_stream
	constructor(handle: StreamHandle) {
		let id = $libmupdf_stm_id++
		$libmupdf_stm_table.set(id, handle)
		super(libmupdf._wasm_new_stream(id))
	}
}

/* -------------------------------------------------------------------------- */

var $libmupdf_load_font_file_js: (name: string, script: string, bold: boolean, italic: boolean) => Font | null

declare global {
	function $libmupdf_load_font_file(name: Pointer<"char">, script: Pointer<"char">, bold: boolean, italic: boolean): Pointer<"fz_font">
}

globalThis.$libmupdf_load_font_file = function (name, script, bold, italic) {
	if ($libmupdf_load_font_file_js) {
		var font = $libmupdf_load_font_file_js(fromString(name), fromString(script), bold, italic)
		if (font) {
			checkType(font, Font)
			return font.pointer
		}
	}
	return 0 as Pointer<"fz_font">
}

interface DeviceFunctions {
	drop?(): void,
	close?(): void,

	fillPath?(path: Path, evenOdd: boolean, ctm: Matrix, colorspace: ColorSpace, color: number[], alpha: number): void,
	strokePath?(path: Path, stroke: StrokeState, ctm: Matrix, colorspace: ColorSpace, color: number[], alpha: number): void,
	clipPath?(path: Path, evenOdd: boolean, ctm: Matrix): void,
	clipStrokePath?(path: Path, stroke: StrokeState, ctm: Matrix): void,

	fillText?(text: Text, ctm: Matrix, colorspace: ColorSpace, color: number[], alpha: number): void,
	strokeText?(text: Text, stroke: StrokeState, ctm: Matrix, colorspace: ColorSpace, color: number[], alpha: number): void,
	clipText?(text: Text, ctm: Matrix): void,
	clipStrokeText?(text: Text, stroke: StrokeState, ctm: Matrix): void,
	ignoreText?(text: Text, ctm: Matrix): void,

	fillShade?(shade: Shade, ctm: Matrix, alpha: number): void,

	fillImage?(image: Image, ctm: Matrix, alpha: number): void,
	fillImageMask?(image: Image, ctm: Matrix, colorspace: ColorSpace, color: number[], alpha: number): void,
	clipImageMask?(image: Image, ctm: Matrix): void,

	popClip?(): void,

	beginMask?(bbox: Rect, luminosity: boolean, colorspace: ColorSpace, color: number[]): void,
	endMask?(): void,

	beginGroup?(bbox: Rect, colorspace: ColorSpace, isolated: boolean, knockout: boolean, blendmode: BlendMode, alpha: number): void,
	endGroup?(): void,

	beginTile?(area: Rect, view: Rect, xstep: number, ystep: number, ctm: Matrix, id: number, doc_id: number): number,
	endTile?(): void,

	beginLayer?(name: string): void,
	endLayer?(): void,
}

var $libmupdf_device_id = 0
var $libmupdf_device_table: Map<number,DeviceFunctions> = new Map()

var $libmupdf_path_id = 0
var $libmupdf_path_table: Map<number,PathWalker> = new Map()

var $libmupdf_text_id = 0
var $libmupdf_text_table: Map<number,TextWalker> = new Map()

declare global {
	var $libmupdf_path_walk: any
	var $libmupdf_text_walk: any
	var $libmupdf_device: any
}

globalThis.$libmupdf_path_walk = {
	moveto(id: number, x: number, y: number): void {
		$libmupdf_path_table.get(id)?.moveTo?.(x, y)
	},
	lineto(id: number, x: number, y: number): void {
		$libmupdf_path_table.get(id)?.lineTo?.(x, y)
	},
	curveto(id: number, x1: number, y1: number, x2: number, y2: number, x3: number, y3: number): void {
		$libmupdf_path_table.get(id)?.curveTo?.(x1, y1, x2, y2, x3, y3)
	},
	closepath(id: number): void {
		$libmupdf_path_table.get(id)?.closePath?.()
	},
}

var $libmupdf_text_font: Font | null = null

globalThis.$libmupdf_text_walk = {
	begin_span(
		id: number,
		font: Pointer<"fz_font">,
		trm: Pointer<"fz_matrix">,
		wmode: number,
		bidi: number,
		dir: number,
		lang: Pointer<"char">
	): void {
		if (font !== $libmupdf_text_font?.pointer)
			$libmupdf_text_font = new Font(font)
		$libmupdf_text_table.get(id)?.beginSpan?.(
			$libmupdf_text_font,
			fromMatrix(trm),
			wmode,
			bidi,
			dir,
			fromString(lang)
		)
	},
	end_span(id: number): void {
		$libmupdf_text_table.get(id)?.endSpan?.()
	},
	show_glyph(
		id: number,
		font: Pointer<"fz_font">,
		trm: Pointer<"fz_matrix">,
		glyph: number,
		unicode: number,
		wmode: number,
		bidi: number
	): void {
		if (font !== $libmupdf_text_font?.pointer)
			$libmupdf_text_font = new Font(font)
		$libmupdf_text_table.get(id)?.showGlyph?.(
			$libmupdf_text_font,
			fromMatrix(trm),
			glyph,
			unicode,
			wmode,
			bidi
		)
	},
}

globalThis.$libmupdf_device = {
	drop_device(id: number): void {
		$libmupdf_device_table.get(id)?.drop?.()
		$libmupdf_device_table.delete(id)
	},

	close_device(id: number): void {
		$libmupdf_device_table.get(id)?.close?.()
	},

	fill_path(
		id: number,
		path: Pointer<"fz_path">,
		even_odd: number,
		ctm: Pointer<"fz_matrix">,
		colorspace: Pointer<"fz_colorspace">,
		color_n: number,
		color_arr: Pointer<"float">,
		alpha: number
	): void {
		$libmupdf_device_table.get(id)?.fillPath?.(
			new Path(libmupdf._wasm_keep_path(path)),
			!!even_odd,
			fromMatrix(ctm),
			new ColorSpace(libmupdf._wasm_keep_colorspace(colorspace)),
			fromColorArray(color_n, color_arr),
			alpha
		)
	},

	clip_path(
		id: number,
		path: Pointer<"fz_path">,
		even_odd: number,
		ctm: Pointer<"fz_matrix">
	): void {
		$libmupdf_device_table.get(id)?.clipPath?.(
				new Path(libmupdf._wasm_keep_path(path)),
				!!even_odd,
				fromMatrix(ctm)
			)
	},

	stroke_path(
		id: number,
		path: Pointer<"fz_path">,
		stroke: Pointer<"fz_stroke_state">,
		ctm: Pointer<"fz_matrix">,
		colorspace: Pointer<"fz_colorspace">,
		color_n: number,
		color_arr: Pointer<"float">,
		alpha: number
	): void {
		$libmupdf_device_table.get(id)?.strokePath?.(
			new Path(libmupdf._wasm_keep_path(path)),
			new StrokeState(libmupdf._wasm_keep_stroke_state(stroke)),
			fromMatrix(ctm),
			new ColorSpace(libmupdf._wasm_keep_colorspace(colorspace)),
			fromColorArray(color_n, color_arr),
			alpha
		)
	},

	clip_stroke_path(
		id: number,
		path: Pointer<"fz_path">,
		stroke: Pointer<"fz_stroke_state">,
		ctm: Pointer<"fz_matrix">
	): void {
		$libmupdf_device_table.get(id)?.clipStrokePath?.(
			new Path(libmupdf._wasm_keep_path(path)),
			new StrokeState(libmupdf._wasm_keep_stroke_state(stroke)),
			fromMatrix(ctm)
		)
	},

	fill_text(
		id: number,
		text: Pointer<"fz_text">,
		ctm: Pointer<"fz_matrix">,
		colorspace: Pointer<"fz_colorspace">,
		color_n: number,
		color_arr: Pointer<"float">,
		alpha: number
	): void {
		$libmupdf_device_table.get(id)?.fillText?.(
				new Text(libmupdf._wasm_keep_text(text)),
				fromMatrix(ctm),
				new ColorSpace(libmupdf._wasm_keep_colorspace(colorspace)),
				fromColorArray(color_n, color_arr),
				alpha
			)
	},

	stroke_text(
		id: number,
		text: Pointer<"fz_text">,
		stroke: Pointer<"fz_stroke_state">,
		ctm: Pointer<"fz_matrix">,
		colorspace: Pointer<"fz_colorspace">,
		color_n: number,
		color_arr: Pointer<"float">,
		alpha: number
	): void {
		$libmupdf_device_table.get(id)?.strokeText?.(
				new Text(libmupdf._wasm_keep_text(text)),
				new StrokeState(libmupdf._wasm_keep_stroke_state(stroke)),
				fromMatrix(ctm),
				new ColorSpace(libmupdf._wasm_keep_colorspace(colorspace)),
				fromColorArray(color_n, color_arr),
				alpha
			)
	},

	clip_text(
		id: number,
		text: Pointer<"fz_text">,
		ctm: Pointer<"fz_matrix">
	): void {
		$libmupdf_device_table.get(id)?.clipText?.(
				new Text(libmupdf._wasm_keep_text(text)),
				fromMatrix(ctm)
			)
	},

	clip_stroke_text(
		id: number,
		text: Pointer<"fz_text">,
		stroke: Pointer<"fz_stroke_state">,
		ctm: Pointer<"fz_matrix">,
	): void {
		$libmupdf_device_table.get(id)?.clipStrokeText?.(
				new Text(libmupdf._wasm_keep_text(text)),
				new StrokeState(libmupdf._wasm_keep_stroke_state(stroke)),
				fromMatrix(ctm)
			)
	},

	ignore_text(
		id: number,
		text: Pointer<"fz_text">,
		ctm: Pointer<"fz_matrix">
	): void {
		$libmupdf_device_table.get(id)?.ignoreText?.(
				new Text(libmupdf._wasm_keep_text(text)),
				fromMatrix(ctm)
			)
	},

	fill_shade(
		id: number,
		shade: Pointer<"fz_shade">,
		ctm: Pointer<"fz_matrix">,
		alpha: number
	): void {
		$libmupdf_device_table.get(id)?.fillShade?.(
				new Shade(shade),
				fromMatrix(ctm),
				alpha
			)
	},

	fill_image(
		id: number,
		image: Pointer<"fz_image">,
		ctm: Pointer<"fz_matrix">,
		alpha: number
	): void {
		$libmupdf_device_table.get(id)?.fillImage?.(
			new Image(image),
			fromMatrix(ctm),
			alpha
		)
	},

	fill_image_mask(
		id: number,
		image: Pointer<"fz_image">,
		ctm: Pointer<"fz_matrix">,
		colorspace: Pointer<"fz_colorspace">,
		color_n: number,
		color_arr: Pointer<"float">,
		alpha: number
	): void {
		$libmupdf_device_table.get(id)?.fillImageMask?.(
			new Image(image),
			fromMatrix(ctm),
			new ColorSpace(libmupdf._wasm_keep_colorspace(colorspace)),
			fromColorArray(color_n, color_arr),
			alpha
		)
	},

	clip_image_mask(
		id: number,
		image: Pointer<"fz_image">,
		ctm: Pointer<"fz_matrix">
	): void {
		$libmupdf_device_table.get(id)?.clipImageMask?.(
			new Image(image),
			fromMatrix(ctm)
		)
	},

	pop_clip(id: number): void {
		$libmupdf_device_table.get(id)?.popClip?.()
	},

	begin_mask(
		id: number,
		bbox: Pointer<"fz_rect">,
		luminosity: number,
		colorspace: Pointer<"fz_colorspace">,
		color_n: number,
		color_arr: Pointer<"float">
	): void {
		$libmupdf_device_table.get(id)?.beginMask?.(
			fromRect(bbox),
			!!luminosity,
			new ColorSpace(libmupdf._wasm_keep_colorspace(colorspace)),
			fromColorArray(color_n, color_arr)
		)
	},

	begin_group(
		id: number,
		bbox: Pointer<"fz_rect">,
		colorspace: Pointer<"fz_colorspace">,
		isolated: number,
		knockout: number,
		blendmode: number,
		alpha: number
	): void {
		$libmupdf_device_table.get(id)?.beginGroup?.(
			fromRect(bbox),
			new ColorSpace(libmupdf._wasm_keep_colorspace(colorspace)),
			!!isolated,
			!!knockout,
			Device.BLEND_MODES[blendmode] as BlendMode,
			alpha
		)
	},

	begin_tile(
		id: number,
		area: Pointer<"fz_rect">,
		view: Pointer<"fz_rect">,
		xstep: number,
		ystep: number,
		ctm: Pointer<"fz_matrix">,
		tile_id: number,
		doc_id: number
	): number {
		return $libmupdf_device_table.get(id)?.beginTile?.(
			fromRect(area),
			fromRect(view),
			xstep,
			ystep,
			fromMatrix(ctm),
			tile_id,
			doc_id
		) || 0
	},

	begin_layer(id: number, name: Pointer<"char">): void {
		$libmupdf_device_table.get(id)?.beginLayer?.(
			fromString(name)
		)
	},

	end_mask(id: number): void {
		$libmupdf_device_table.get(id)?.endMask?.()
	},

	end_group(id: number): void {
		$libmupdf_device_table.get(id)?.endGroup?.()
	},

	end_tile(id: number): void {
		$libmupdf_device_table.get(id)?.endTile?.()
	},

	end_layer(id: number): void {
		$libmupdf_device_table.get(id)?.endLayer?.()
	},

}

/* default exports */
export default {
	// const
	Matrix,
	Rect,

	// function
	enableICC,
	disableICC,
	setUserCSS,
	installLoadFontFunction,

	// class
	Buffer,
	ColorSpace,
	Font,
	Image,
	StrokeState,
	Path,
	Text,
	DisplayList,
	Pixmap,
	Shade,
	StructuredText,
	Device,
	DrawDevice,
	DisplayListDevice,
	DocumentWriter,
	Document,
	OutlineIterator,
	Link,
	Page,
	PDFDocument,
	PDFPage,
	PDFObject,
	PDFGraftMap,
	PDFAnnotation,
	PDFWidget,
	Stream,

	// debugging
	memento
}
