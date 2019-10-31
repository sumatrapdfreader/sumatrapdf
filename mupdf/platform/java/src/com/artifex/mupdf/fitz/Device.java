package com.artifex.mupdf.fitz;

abstract public class Device
{
	static {
		Context.init();
	}

	protected long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
		pointer = 0;
	}

	private native long newNative();

	protected Device() {
		pointer = newNative();
	}

	protected Device(long p) {
		pointer = p;
	}

	/* To implement your own device in Java, you should define your own
	 * class that extends Device, and override as many of the following
	 * functions as is appropriate. For example:
	 *
	 * class ImageTraceDevice extends Device
	 * {
	 *	void fillImage(Image img, Matrix ctx, float alpha) {
	 *		System.out.println("Image!");
	 *	}
	 * };
	 */

	abstract public void close();
	abstract public void fillPath(Path path, boolean evenOdd, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp);
	abstract public void strokePath(Path path, StrokeState stroke, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp);
	abstract public void clipPath(Path path, boolean evenOdd, Matrix ctm);
	abstract public void clipStrokePath(Path path, StrokeState stroke, Matrix ctm);
	abstract public void fillText(Text text, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp);
	abstract public void strokeText(Text text, StrokeState stroke, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp);
	abstract public void clipText(Text text, Matrix ctm);
	abstract public void clipStrokeText(Text text, StrokeState stroke, Matrix ctm);
	abstract public void ignoreText(Text text, Matrix ctm);
	abstract public void fillShade(Shade shd, Matrix ctm, float alpha, int cp);
	abstract public void fillImage(Image img, Matrix ctm, float alpha, int cp);
	abstract public void fillImageMask(Image img, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp);
	abstract public void clipImageMask(Image img, Matrix ctm);
	abstract public void popClip();
	abstract public void beginMask(Rect area, boolean luminosity, ColorSpace cs, float[] bc, int cp);
	abstract public void endMask();
	abstract public void beginGroup(Rect area, ColorSpace cs, boolean isolated, boolean knockout, int blendmode, float alpha);
	abstract public void endGroup();
	abstract public int beginTile(Rect area, Rect view, float xstep, float ystep, Matrix ctm, int id);
	abstract public void endTile();
	abstract public void beginLayer(String name);
	abstract public void endLayer();

	/* PDF 1.4 -- standard separable */
	public static final int BLEND_NORMAL = 0;
	public static final int BLEND_MULTIPLY = 1;
	public static final int BLEND_SCREEN = 2;
	public static final int BLEND_OVERLAY = 3;
	public static final int BLEND_DARKEN = 4;
	public static final int BLEND_LIGHTEN = 5;
	public static final int BLEND_COLOR_DODGE = 6;
	public static final int BLEND_COLOR_BURN = 7;
	public static final int BLEND_HARD_LIGHT = 8;
	public static final int BLEND_SOFT_LIGHT = 9;
	public static final int BLEND_DIFFERENCE = 10;
	public static final int BLEND_EXCLUSION = 11;

	/* PDF 1.4 -- standard non-separable */
	public static final int BLEND_HUE = 12;
	public static final int BLEND_SATURATION = 13;
	public static final int BLEND_COLOR = 14;
	public static final int BLEND_LUMINOSITY = 15;
}
