package com.artifex.mupdf.fitz;

public class StrokeState
{
	static {
		Context.init();
	}

	public static final int LINE_CAP_BUTT = 0;
	public static final int LINE_CAP_ROUND = 1;
	public static final int LINE_CAP_SQUARE = 2;
	public static final int LINE_CAP_TRIANGLE = 3;

	public static final int LINE_JOIN_MITER = 0;
	public static final int LINE_JOIN_ROUND = 1;
	public static final int LINE_JOIN_BEVEL = 2;
	public static final int LINE_JOIN_MITER_XPS = 3;

	private long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
		pointer = 0;
	}

	private native long newNative(int startCap, int dashCap, int endCap, int lineJoin, float lineWidth, float miterLimit,
			float dashPhase, float[] dash);

	// Private constructor for the C to use. Any objects created by the
	// C are done for purposes of calling back to a java device, and
	// should therefore be considered const. This is fine as we don't
	// currently provide mechanisms for changing individual elements
	// of the StrokeState.
	private StrokeState(long p) {
		pointer = p;
	}

	public StrokeState(int startCap, int endCap, int lineJoin, float lineWidth, float miterLimit) {
		pointer = newNative(startCap, 0, endCap, lineJoin, lineWidth, miterLimit, 0, null);
	}

	public StrokeState(int startCap, int dashCap, int endCap, int lineJoin, float lineWidth, float miterLimit,
			float dashPhase, float[] dash) {
		pointer = newNative(startCap, dashCap, endCap, lineJoin, lineWidth, miterLimit, dashPhase, dash);
	}

	public native int getStartCap();
	public native int getDashCap();
	public native int getEndCap();
	public native int getLineJoin();
	public native float getLineWidth();
	public native float getMiterLimit();
	public native float getDashPhase();
	public native float[] getDashes();
}
