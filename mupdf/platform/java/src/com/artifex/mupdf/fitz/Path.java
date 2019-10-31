package com.artifex.mupdf.fitz;

public class Path implements PathWalker
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

	private native long newNative();
	private native long cloneNative();

	public Path() {
		pointer = newNative();
	}

	private Path(long p) {
		pointer = p;
	}

	public Path(Path old) {
		pointer = old.cloneNative();
	}

	public native Point currentPoint();

	public native void moveTo(float x, float y);
	public native void lineTo(float x, float y);
	public native void curveTo(float cx1, float cy1, float cx2, float cy2, float ex, float ey);
	public native void curveToV(float cx, float cy, float ex, float ey);
	public native void curveToY(float cx, float cy, float ex, float ey);
	public native void rect(int x1, int y1, int x2, int y2);
	public native void closePath();

	public void moveTo(Point xy) {
		moveTo(xy.x, xy.y);
	}

	public void lineTo(Point xy) {
		lineTo(xy.x, xy.y);
	}

	public void curveTo(Point c1, Point c2, Point e) {
		curveTo(c1.x, c1.y, c2.x, c2.y, e.x, e.y);
	}

	public void curveToV(Point c, Point e) {
		curveToV(c.x, c.y, e.x, e.y);
	}

	public void curveToY(Point c, Point e) {
		curveToY(c.x, c.y, e.x, e.y);
	}

	public native void transform(Matrix mat);

	public native Rect getBounds(StrokeState stroke, Matrix ctm);

	public native void walk(PathWalker walker);
}
