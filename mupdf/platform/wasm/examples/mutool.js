// Copyright (C) 2004-2023 Artifex Software, Inc.
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

const fs = require("fs")
const mupdf = require("mupdf")

var mutool = {}

function parseDocument(path) {
	return mupdf.Document.openDocument(fs.readFileSync(path), path)
}

function parseColorSpace(colorspace) {
	if (colorspace === "Gray")
		return mupdf.ColorSpace.DeviceGray
	if (colorspace === "RGB")
		return mupdf.ColorSpace.DeviceRGB
	if (colorspace === "CMYK")
		return mupdf.ColorSpace.DeviceCMYK
	return new mupdf.ColorSpace(fs.readFileSync(colorspace), colorspace)
}

function parse_options(argv, defs) {
	let result = {}

	for (let i = 0; i < argv.length; ) {
		let str = argv[i]
		if (str[0] !== "-")
			throw Error("expected option, got '" + str + "'")
		str = str.substring(1)
		let o = i
		for (let [ opt, type, val ] of defs) {
			if (str === opt) {
				if (type === Boolean) {
					result[opt] = true
					i += 1
				} else {
					result[opt] = type(argv[i + 1])
					i += 2
				}
			}
		}
		if (o === i)
			throw Error("unknown option: " + str)
	}

	for (let [ opt, type, val ] of defs) {
		if (result[opt] === undefined) {
			if (val !== undefined)
				result[opt] = type(val)
			else
				throw Error("missing argument: " + opt)
		}
	}

	return result
}

function for_each_page(doc, range_list, f) {
	let N = doc.countPages()
	for (let range of range_list.split(",")) {
		let a, b
		if (range.indexOf("-") >= 0) {
			[a, b] = range.split("-")
			a = (a === "N") ? N : parseInt(a)
			b = (b === "N") ? N : parseInt(b)
		} else {
			a = b = (range === "N") ? N : parseInt(range)
		}
		if (a <= b) {
			while (a <= b) {
				f(doc, doc.loadPage(a - 1), a)
				a += 1
			}
		} else {
			while (a >= b) {
				f(doc, doc.loadPage(a - 1), a)
				a -= 1
			}
		}
	}
}

mutool.draw = {
	options: [
		[ "input", parseDocument ],
		[ "output", String, "out%d.png" ],
		[ "resolution", Number, 72 ],
		[ "colorspace", parseColorSpace, "RGB" ],
		[ "format", String, "png" ],
		[ "pages", String, "1-N" ],
	],
	run(options) {
		for_each_page(options.input, options.pages, function (doc, page, number) {
			let pixmap = page.toPixmap(
				mupdf.Matrix.scale(options.resolution/72, options.resolution/72),
				options.colorspace,
				false
			)
			let data
			if (options.format === "png")
				data = pixmap.asPNG()
			else if (options.format === "pam")
				data = pixmap.asPAM()
			else
				throw Error("unknown output format: " + options.format)
			fs.writeFileSync(options.output.replace("%d", number), data)
		})
	}
}

mutool.text = {
	options: [
		[ "input", parseDocument ],
		[ "output", String, "/dev/stdout" ],
		[ "pages", String, "1-N" ],
	],
	run(options) {
		let result = []
		for_each_page(options.input, options.pages, function (doc, page, number) {
			result.push(JSON.parse(page.toStructuredText().asJSON()))
		})
		fs.writeFileSync(options.output, JSON.stringify(result, 0, 4))
	},
}

mutool.clean = {
	options: [
		[ "input", parseDocument ],
		[ "output", String ],
		[ "pages", String, "1-N" ],
		[ "garbage", Number, 0 ],
		[ "clean", Boolean, false ],
		[ "sanitize", Boolean, false ],
		[ "decompress", Boolean, false ],
		[ "ascii", Boolean, false ],
	],
	run(options) {
		console.log("CLEAN options", options)
	},
}

function main(argv) {
	let cmd = argv.shift()
	if (cmd in mutool)
		mutool[cmd].run(parse_options(argv, mutool[cmd].options))
	else
		throw Error("unknown command: " + cmd + " (valid commands: " + Object.keys(mutool).join(", ") + ")")
}

main(process.argv.slice(2))
