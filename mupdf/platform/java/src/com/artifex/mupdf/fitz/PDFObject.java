// Copyright (C) 2004-2023 Artifex Software, Inc.
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
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

package com.artifex.mupdf.fitz;

import java.util.Date;
import java.util.Iterator;
import java.util.NoSuchElementException;

public class PDFObject implements Iterable<PDFObject>
{
	static {
		Context.init();
	}

	private long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	private PDFObject(long p) {
		pointer = p;
	}

	public native boolean isIndirect();
	public boolean isNull() {
		return this == PDFObject.Null;
	}
	public native boolean isBoolean();
	public native boolean isInteger();
	public native boolean isReal();
	public native boolean isNumber();
	public native boolean isString();
	public native boolean isName();
	public native boolean isArray();
	public native boolean isDictionary();
	public native boolean isStream();

	public native boolean asBoolean();
	public native int asInteger();
	public native float asFloat();
	public native int asIndirect();
	public native String asName();
	public native String asString();
	public native byte[] asByteString();

	public native String toString(boolean tight, boolean ascii);

	public String toString(boolean tight) {
		return toString(tight, false);
	}

	public String toString() {
		return toString(false, false);
	}

	public native PDFObject resolve();
	public native boolean equals(PDFObject other);

	public native byte[] readStream();
	public native byte[] readRawStream();

	public native void writeObject(PDFObject obj);
	private native void writeStreamBuffer(Buffer buf);
	private native void writeStreamString(String str);
	private native void writeRawStreamBuffer(Buffer buf);
	private native void writeRawStreamString(String str);

	public void writeStream(Buffer buf) {
		writeStreamBuffer(buf);
	}

	public void writeStream(String str) {
		writeStreamString(str);
	}

	public void writeRawStream(Buffer buf) {
		writeRawStreamBuffer(buf);
	}

	public void writeRawStream(String str) {
		writeRawStreamString(str);
	}

	private native PDFObject getArray(int index);
	private native PDFObject getDictionary(String name, boolean inheritable);
	private native PDFObject getDictionaryKey(int index);

	public PDFObject get(int index) {
		return getArray(index);
	}

	public PDFObject get(String name) {
		return getDictionary(name, false);
	}

	public PDFObject get(PDFObject name) {
		return getDictionary(name != null ? name.asName() : null, false);
	}

	public PDFObject getInheritable(String name) {
		return getDictionary(name, true);
	}

	public PDFObject getInheritable(PDFObject name) {
		return getDictionary(name != null ? name.asName() : null, true);
	}

	private native void putArrayBoolean(int index, boolean b);
	private native void putArrayInteger(int index, int i);
	private native void putArrayFloat(int index, float f);
	private native void putArrayString(int index, String str);
	private native void putArrayPDFObject(int index, PDFObject obj);

	private native void putDictionaryStringBoolean(String name, boolean b);
	private native void putDictionaryStringInteger(String name, int i);
	private native void putDictionaryStringFloat(String name, float f);
	private native void putDictionaryStringString(String name, String str);
	private native void putDictionaryStringPDFObject(String name, PDFObject obj);

	private native void putDictionaryPDFObjectBoolean(PDFObject name, boolean b);
	private native void putDictionaryPDFObjectInteger(PDFObject name, int i);
	private native void putDictionaryPDFObjectFloat(PDFObject name, float f);
	private native void putDictionaryPDFObjectString(PDFObject name, String str);
	private native void putDictionaryPDFObjectPDFObject(PDFObject name, PDFObject obj);
	private native void putDictionaryPDFObjectRect(PDFObject name, Rect r);
	private native void putDictionaryPDFObjectMatrix(PDFObject name, Matrix m);
	private native void putDictionaryPDFObjectDate(PDFObject name, long secs);

	public void put(int index, boolean b) {
		putArrayBoolean(index, b);
	}

	public void put(int index, int i) {
		putArrayInteger(index, i);
	}

	public void put(int index, float f) {
		putArrayFloat(index, f);
	}

	public void put(int index, String s) {
		putArrayString(index, s);
	}

	public void put(int index, PDFObject obj) {
		putArrayPDFObject(index, obj);
	}

	public void put(String name, boolean b) {
		putDictionaryStringBoolean(name, b);
	}

	public void put(String name, int i) {
		putDictionaryStringInteger(name, i);
	}

	public void put(String name, float f) {
		putDictionaryStringFloat(name, f);
	}

	public void put(String name, String str) {
		putDictionaryStringString(name, str);
	}

	public void put(String name, PDFObject obj) {
		putDictionaryStringPDFObject(name, obj);
	}

	public void put(PDFObject name, boolean b) {
		putDictionaryPDFObjectBoolean(name, b);
	}

	public void put(PDFObject name, int i) {
		putDictionaryPDFObjectInteger(name, i);
	}

	public void put(PDFObject name, float f) {
		putDictionaryPDFObjectFloat(name, f);
	}

	public void put(PDFObject name, String str) {
		putDictionaryPDFObjectString(name, str);
	}

	public void put(PDFObject name, PDFObject obj) {
		putDictionaryPDFObjectPDFObject(name, obj);
	}

	public void put(PDFObject name, Rect r) {
		putDictionaryPDFObjectRect(name, r);
	}

	public void put(PDFObject name, Matrix m) {
		putDictionaryPDFObjectMatrix(name, m);
	}

	public void put(PDFObject name, Date time) {
		putDictionaryPDFObjectDate(name, time.getTime());
	}

	private native void deleteArray(int index);
	private native void deleteDictionaryString(String name);
	private native void deleteDictionaryPDFObject(PDFObject name);

	public void delete(int index) {
		deleteArray(index);
	}

	public void delete(String name) {
		deleteDictionaryString(name);
	}

	public void delete(PDFObject name) {
		deleteDictionaryPDFObject(name);
	}

	public native int size();

	private native void pushBoolean(boolean b);
	private native void pushInteger(int i);
	private native void pushFloat(float f);
	private native void pushString(String s);
	private native void pushPDFObject(PDFObject item);

	public void push(boolean b) {
		pushBoolean(b);
	}

	public void push(int i) {
		pushInteger(i);
	}

	public void push(float f) {
		pushFloat(f);
	}

	public void push(String s) {
		pushString(s);
	}

	public void push(PDFObject obj) {
		pushPDFObject(obj);
	}

	public static final PDFObject Null = new PDFObject(0);

	public Iterator<PDFObject> iterator() {
		return new PDFObjectIterator(this);
	}

	protected class PDFObjectIterator implements Iterator<PDFObject> {
		private PDFObject object;
		private boolean isarray;
		private int position;

		public PDFObjectIterator(PDFObject object) {
			this.object = object;
			isarray = object != null ? object.isArray() : false;
			position = -1;
		}

		public boolean hasNext() {
			return object != null && (position + 1) < object.size();
		}

		public PDFObject next() {
			if (object == null || position >= object.size())
				throw new NoSuchElementException("Object has no more elements");

			position++;
			if (isarray)
				return object.get(position);
			else
				return object.getDictionaryKey(position);
		}

		public void remove() {
			throw new UnsupportedOperationException();
		}
	}
}
