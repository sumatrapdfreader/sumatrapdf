package com.artifex.mupdf.fitz;

public class Page
{
	static {
		Context.init();
	}

	private long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
		pointer = 0;
	}

	protected Page(long p) {
		pointer = p;
	}

	public native Rect getBounds();

	public native void run(Device dev, Matrix ctm, Cookie cookie);
	public native void runPageContents(Device dev, Matrix ctm, Cookie cookie);
	public native void runPageAnnots(Device dev, Matrix ctm, Cookie cookie);
	public native void runPageWidgets(Device dev, Matrix ctm, Cookie cookie);

	public void run(Device dev, Matrix ctm) {
		run(dev, ctm, null);
	}

	public native Link[] getLinks();

	public native Pixmap toPixmap(Matrix ctm, ColorSpace cs, boolean alpha, boolean showExtras);
	public Pixmap toPixmap(Matrix ctm, ColorSpace cs, boolean alpha) {
		return toPixmap(ctm, cs, alpha, true);
	}

	public native DisplayList toDisplayList(boolean showExtras);
	public DisplayList toDisplayList() {
		return toDisplayList(true);
	}

	public native StructuredText toStructuredText(String options);
	public StructuredText toStructuredText() {
		return toStructuredText(null);
	}

	public native Quad[] search(String needle);

	public native byte[] textAsHtml();

	public native Separations getSeparations();
}
