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

	protected long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
		pointer = 0;
	}

	protected Document(long p) {
		pointer = p;
	}

	protected native static Document openNativeWithPath(String filename, String accelerator);
	protected native static Document openNativeWithBuffer(String magic, byte[] buffer, byte[] accelerator);
	protected native static Document openNativeWithStream(String magic, SeekableInputStream stream, SeekableInputStream accelerator);
	protected native static Document openNativeWithPathAndStream(String filename, SeekableInputStream accelerator);

	public static Document openDocument(String filename) {
		return openNativeWithPath(filename, null);
	}

	public static Document openDocument(String filename, String accelerator) {
		return openNativeWithPath(filename, accelerator);
	}

	public static Document openDocument(String filename, SeekableInputStream accelerator) {
		return openNativeWithPathAndStream(filename, accelerator);
	}

	public static Document openDocument(byte[] buffer, String magic) {
		return openNativeWithBuffer(magic, buffer, null);
	}

	public static Document openDocument(byte[] buffer, String magic, byte[] accelerator) {
		return openNativeWithBuffer(magic, buffer, accelerator);
	}

	public static Document openDocument(SeekableInputStream stream, String magic) {
		return openNativeWithStream(magic, stream, null);
	}

	public static Document openDocument(SeekableInputStream stream, String magic, SeekableInputStream accelerator) {
		return openNativeWithStream(magic, stream, accelerator);
	}

	public static native boolean recognize(String magic);

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
		return resolveLink(link.uri);
	}

	public native Outline[] loadOutline();
	public native String getMetaData(String key);
	public native boolean isReflowable();
	public native void layout(float width, float height, float em);

	public native Location findBookmark(long mark);
	public native long makeBookmark(int chapter, int page);
	public long makeBookmark(Location loc) {
		return makeBookmark(loc.chapter, loc.page);
	}

	public native boolean isUnencryptedPDF();

	public boolean isPDF() {
		return false;
	}
}
