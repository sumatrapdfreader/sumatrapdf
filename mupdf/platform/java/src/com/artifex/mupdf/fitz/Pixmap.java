package com.artifex.mupdf.fitz;

public class Pixmap
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

	private native long newNative(ColorSpace cs, int x, int y, int w, int h, boolean alpha);

	private Pixmap(long p) {
		pointer = p;
	}

	public Pixmap(ColorSpace cs, int x, int y, int w, int h, boolean alpha) {
		pointer = newNative(cs, x, y, w, h, alpha);
	}

	public Pixmap(ColorSpace cs, int x, int y, int w, int h) {
		this(cs, x, y, w, h, false);
	}

	public Pixmap(ColorSpace cs, int w, int h, boolean alpha) {
		this(cs, 0, 0, w, h, alpha);
	}

	public Pixmap(ColorSpace cs, int w, int h) {
		this(cs, 0, 0, w, h, false);
	}

	public Pixmap(ColorSpace cs, Rect rect, boolean alpha) {
		this(cs, (int)rect.x0, (int)rect.y0, (int)(rect.x1 - rect.x0), (int)(rect.y1 - rect.y0), alpha);
	}

	public Pixmap(ColorSpace cs, Rect rect) {
		this(cs, rect, false);
	}

	public native void clear();
	private native void clearWithValue(int value);
	public void clear(int value) {
		clearWithValue(value);
	}

	public native void saveAsPNG(String filename);

	public native int getX();
	public native int getY();
	public native int getWidth();
	public native int getHeight();
	public native int getStride();
	public native int getNumberOfComponents();
	public native boolean getAlpha();
	public native ColorSpace getColorSpace();
	public native byte[] getSamples();
	public native byte getSample(int x, int y, int n);
	public native int[] getPixels(); /* only valid for RGBA or BGRA pixmaps */
	public native int getXResolution();
	public native int getYResolution();
	public native void invert();
	public native void invertLuminance();
	public native void gamma(float gamma);

	public Rect getBounds() {
		int x = getX();
		int y = getY();
		return new Rect(x, y, x + getWidth(), y+ getHeight());
	}

	public String toString() {
		return "Pixmap(w=" + getWidth() +
			" h=" + getHeight() +
			" x=" + getX() +
			" y=" + getY() +
			" n=" + getNumberOfComponents() +
			" alpha=" + getAlpha() +
			" cs=" + getColorSpace() +
			")";
	}
}
