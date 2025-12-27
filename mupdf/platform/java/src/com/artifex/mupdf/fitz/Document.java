// Copyright (C) 2004-2025 Artifex Software, Inc.
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

public class Document
{
	static {
		Context.init();
	}

	public static final String META_FORMAT = "format";
	public static final String META_ENCRYPTION = "encryption";
	public static final String META_INFO_AUTHOR = "info:Author";
	public static final String META_INFO_TITLE = "info:Title";
	public static final String META_INFO_SUBJECT = "info:Subject";
	public static final String META_INFO_KEYWORDS = "info:Keywords";
	public static final String META_INFO_CREATOR = "info:Creator";
	public static final String META_INFO_PRODUCER = "info:Producer";
	public static final String META_INFO_CREATIONDATE = "info:CreationDate";
	public static final String META_INFO_MODIFICATIONDATE = "info:ModDate";

	protected long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	protected Document(long p) {
		pointer = p;
	}

	protected native static Document openNativeWithPath(String filename, String accelerator, Archive dir);
	protected native static Document openNativeWithBuffer(byte[] buffer, String magic, byte[] accelerator, Archive dir);
	protected native static Document openNativeWithStream(SeekableInputStream stream, String magic, SeekableInputStream accelerator, Archive dir);
	protected native static Document openNativeWithPathAndStream(String filename, SeekableInputStream accelerator, Archive dir);
	public static Document openDocument(String filename) {
		return openNativeWithPath(filename, null, null);
	}
	public static Document openDocument(String filename, Archive dir) {
		return openNativeWithPath(filename, null, dir);
	}
	public static Document openDocument(String filename, String accelerator) {
		return openNativeWithPath(filename, accelerator, null);
	}
	public static Document openDocument(String filename, String accelerator, Archive dir) {
		return openNativeWithPath(filename, accelerator, dir);
	}
	public static Document openDocument(byte[] buffer, String magic) {
		return openNativeWithBuffer(buffer, magic, null, null);
	}
	public static Document openDocument(byte[] buffer, String magic, Archive dir) {
		return openNativeWithBuffer(buffer, magic, null, dir);
	}
	public static Document openDocument(byte[] buffer, String magic, byte[] accelerator) {
		return openNativeWithBuffer(buffer, magic, accelerator, null);
	}
	public static Document openDocument(byte[] buffer, String magic, byte[] accelerator, Archive dir) {
		return openNativeWithBuffer(buffer, magic, accelerator, dir);
	}
	public static Document openDocument(SeekableInputStream stream, String magic) {
		return openNativeWithStream(stream, magic, null, null);
	}
	public static Document openDocument(SeekableInputStream stream, String magic, Archive dir) {
		return openNativeWithStream(stream, magic, null, dir);
	}
	public static Document openDocument(SeekableInputStream stream, String magic, SeekableInputStream accelerator) {
		return openNativeWithStream(stream, magic, accelerator, null);
	}
	public static Document openDocument(SeekableInputStream stream, String magic, SeekableInputStream accelerator, Archive dir) {
		return openNativeWithStream(stream, magic, accelerator, dir);
	}
	public static Document openDocument(String filename, SeekableInputStream accelerator) {
		return openNativeWithPathAndStream(filename, accelerator, null);
	}
	public static Document openDocument(String filename, SeekableInputStream accelerator, Archive dir) {
		return openNativeWithPathAndStream(filename, accelerator, dir);
	}

	public static native boolean recognize(String magic);
	protected native static boolean recognizeContentWithPath(String filename);
	protected native static boolean recognizeContentWithStream(SeekableInputStream stream, String magic, Archive dir);
	public static boolean recognizeContent(String filename) {
		return recognizeContentWithPath(filename);
	}
	public static boolean recognizeContent(SeekableInputStream stream, String magic) {
		return recognizeContentWithStream(stream, magic, null);
	}
	public static boolean recognizeContent(SeekableInputStream stream, String magic, Archive dir) {
		return recognizeContentWithStream(stream, magic, dir);
	}

	public native boolean supportsAccelerator();
	public native void saveAccelerator(String filename);
	public native void outputAccelerator(SeekableOutputStream stream);

	public native boolean needsPassword();
	public native boolean authenticatePassword(String password);

	public native int countChapters();
	public native int countPages(int chapter);
	public native Page loadPage(int chapter, int number);

	public int countPages() {
		int np = 0;
		int nc = countChapters();
		for (int i = 0; i < nc; ++i)
			np += countPages(i);
		return np;
	}

	public Page loadPage(Location loc) {
		return loadPage(loc.chapter, loc.page);
	}

	public Page loadPage(int number) {
		int start = 0;
		int nc = countChapters();
		for (int i = 0; i < nc; ++i) {
			int np = countPages(i);
			if (number < start + np)
				return loadPage(i, number - start);
			start += np;
		}
		throw new IllegalArgumentException("page number out of range");
	}

	public Location lastPage() {
		int nc = countChapters();
		int np = countPages(nc-1);
		return new Location(nc-1, np-1);
	}

	public Location nextPage(Location loc) {
		int np = countPages(loc.chapter);
		if (loc.page + 1 == np) {
			int nc = countChapters();
			if (loc.chapter + 1 < nc)
				return new Location(loc.chapter + 1, 0);
		} else {
			return new Location(loc.chapter, loc.page + 1);
		}
		return loc;
	}

	public Location previousPage(Location loc) {
		if (loc.page == 0) {
			if (loc.chapter > 0) {
				int np = countPages(loc.chapter - 1);
				return new Location(loc.chapter - 1, np - 1);
			}
		} else {
			return new Location(loc.chapter, loc.page - 1);
		}
		return loc;
	}

	public Location clampLocation(Location input) {
		int c = input.chapter;
		int p = input.page;
		int nc = countChapters();
		if (c < 0) c = 0;
		if (c >= nc) c = nc - 1;
		int np = countPages(c);
		if (p < 0) p = 0;
		if (p >= np) p = np - 1;
		if (input.chapter == c && input.page == p)
			return input;
		return new Location(c, p);
	}

	public Location locationFromPageNumber(int number) {
		int i, start = 0, np = 0, nc = countChapters();
		if (number < 0)
			number = 0;
		for (i = 0; i < nc; ++i)
		{
			np = countPages(i);
			if (number < start + np)
				return new Location(i, number - start);
			start += np;
		}
		return new Location(i - 1, np - 1);
	}

	public int pageNumberFromLocation(Location loc) {
		int nc = countChapters();
		int start = 0;
		if (loc == null)
			return -1;
		for (int i = 0; i < nc; ++i) {
			if (i == loc.chapter)
				return start + loc.page;
			start += countPages(i);
		}
		return -1;
	}

	public native Location resolveLink(String uri);
	public Location resolveLink(Outline link) {
		return resolveLink(link.uri);
	}
	public Location resolveLink(Link link) {
		return resolveLink(link.getURI());
	}

	public native LinkDestination resolveLinkDestination(String uri);
	public LinkDestination resolveLinkDestination(OutlineIterator.OutlineItem item) {
		return resolveLinkDestination(item.uri);
	}
	public LinkDestination resolveLinkDestination(Outline link) {
		return resolveLinkDestination(link.uri);
	}
	public LinkDestination resolveLinkDestination(Link link) {
		return resolveLinkDestination(link.getURI());
	}

	public native Outline[] loadOutline();
	public native OutlineIterator outlineIterator();
	public native String getMetaData(String key);
	public native void setMetaData(String key, String value);
	public native boolean isReflowable();
	public native void layout(float width, float height, float em);

	public native Location findBookmark(long mark);
	public native long makeBookmark(int chapter, int page);
	public long makeBookmark(Location loc) {
		return makeBookmark(loc.chapter, loc.page);
	}

	public static final int PERMISSION_PRINT = (int) 'p';
	public static final int PERMISSION_COPY = (int) 'c';
	public static final int PERMISSION_EDIT = (int) 'e';
	public static final int PERMISSION_ANNOTATE = (int) 'n';
	public static final int PERMISSION_FORM = (int) 'f';
	public static final int PERMISSION_ACCESSIBILITY = (int) 'y';
	public static final int PERMISSION_ASSEMBLE = (int) 'a';
	public static final int PERMISSION_PRINT_HQ = (int) 'h';

	public native boolean hasPermission(int permission);

	public native boolean isUnencryptedPDF();

	public native String formatLinkURI(LinkDestination dest);

	public boolean isPDF() {
		return false;
	}

	public native PDFDocument asPDF();
}
