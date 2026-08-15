"use strict"

import * as fs from "fs"
import * as mupdf from "mupdf"

// Use synchronous file descriptor API with Node.
class NodeFileStream {
        constructor(path) {
                this.fd = fs.openSync(path)
        }
        fileSize() {
                return fs.fstatSync(this.fd).size
        }
        read(memory, offset, size, position) {
                return fs.readSync(this.fd, memory, offset, size, position)
        }
        close() {
                fs.closeSync(this.fd)
        }
}

// Use FileReaderSync on Blob/File objects in Web Workers.
class WorkerBlobStream {
	constructor(blob) {
		this.reader = new FileReaderSync()
		this.blob = blob
	}
	fileSize() {
		return this.blob.size
	}
	read(memory, offset, size, position) {
		var data = this.reader.readAsArrayBuffer(this.blob.slice(position, position + size))
		memory.set(new Uint8Array(data), offset)
		return data.byteLength
	}
	close() {
		this.reader = null
		this.blob = null
	}
}

/* to test:
var stm = new mupdf.Stream(new NodeFileStream("pdfref17.pdf"))
var doc = mupdf.Document.openDocument(stm, "application/pdf")
*/
