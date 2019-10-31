package com.artifex.mupdf.fitz;

public class NativeDevice extends Device
{
	static {
		Context.init();
	}

	private long nativeInfo;
	private Object nativeResource;

	protected native void finalize();

	public void destroy() {
		super.destroy();
		nativeInfo = 0;
		nativeResource = null;
	}

	protected NativeDevice(long p) {
		super(p);
	}

	public native final void close();

	public native final void fillPath(Path path, boolean evenOdd, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp);
	public native final void strokePath(Path path, StrokeState stroke, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp);
	public native final void clipPath(Path path, boolean evenOdd, Matrix ctm);
	public native final void clipStrokePath(Path path, StrokeState stroke, Matrix ctm);

	public native final void fillText(Text text, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp);
	public native final void strokeText(Text text, StrokeState stroke, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp);
	public native final void clipText(Text text, Matrix ctm);
	public native final void clipStrokeText(Text text, StrokeState stroke, Matrix ctm);
	public native final void ignoreText(Text text, Matrix ctm);

	public native final void fillShade(Shade shd, Matrix ctm, float alpha, int cp);
	public native final void fillImage(Image img, Matrix ctm, float alpha, int cp);
	public native final void fillImageMask(Image img, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp);
	/* FIXME: Why no scissor? */
	public native final void clipImageMask(Image img, Matrix ctm);

	public native final void popClip();

	public native final void beginMask(Rect rect, boolean luminosity, ColorSpace cs, float[] bc, int cp);
	public native final void endMask();
	public native final void beginGroup(Rect rect, ColorSpace cs, boolean isolated, boolean knockout, int blendmode, float alpha);
	public native final void endGroup();

	public native final int beginTile(Rect area, Rect view, float xstep, float ystep, Matrix ctm, int id);
	public native final void endTile();

	public native final void beginLayer(String name);
	public native final void endLayer();
}
