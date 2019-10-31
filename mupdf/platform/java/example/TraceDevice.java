package example;

import com.artifex.mupdf.fitz.*;

public class TraceDevice extends Device implements PathWalker, TextWalker
{
	public String traceColor(ColorSpace cs, float[] color, float alpha) {
		String s = cs + " [";
		int i;
		for (i = 0; i < color.length; ++i) {
			if (i > 0) s += " ";
			s += color[i];
		}
		return s + "] " + alpha;
	}
	public String traceStroke(StrokeState stroke) {
		return "c=" + stroke.getStartCap() + "," + stroke.getDashCap() + "," + stroke.getEndCap() +
			" j=" + stroke.getLineJoin() +
			" m=" + stroke.getMiterLimit() +
			" l=" + stroke.getLineWidth();
	}

	public void moveTo(float x, float y) {
		System.out.println("moveto " + x + " " + y);
	}

	public void lineTo(float x, float y) {
		System.out.println("lineto " + x + " " + y);
	}

	public void curveTo(float cx1, float cy1, float cx2, float cy2, float ex, float ey) {
		System.out.println("curveto " + cx1 + " " + cy1 + " " + cx2 + " " + cy2 + " " + ex + " " + ey);
	}

	public void closePath() {
		System.out.println("closepath");
	}

	public void showGlyph(Font font, Matrix trm, int glyph, int unicode, boolean wmode) {
		System.out.println("glyph '" + (char)unicode + "' " + glyph + "\t" + font + " " + trm);
	}

	public void tracePath(Path path) {
		path.walk(this);
	}

	public void traceText(Text text) {
		text.walk(this);
	}

	public void close() {
	}

	public void fillPath(Path path, boolean evenOdd, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp) {
		System.out.println("fillPath " + evenOdd + " " + ctm + " " + traceColor(cs, color, alpha));
		tracePath(path);
	}

	public void strokePath(Path path, StrokeState stroke, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp) {
		System.out.println("strokePath " + traceStroke(stroke) + " " + ctm + " " + traceColor(cs, color, alpha));
		tracePath(path);
	}

	public void clipPath(Path path, boolean evenOdd, Matrix ctm) {
		System.out.println("clipPath " + evenOdd + " " + ctm);
		tracePath(path);
	}

	public void clipStrokePath(Path path, StrokeState stroke, Matrix ctm) {
		System.out.println("clipStrokePath " + traceStroke(stroke) + " " + ctm);
		tracePath(path);
	}

	public void fillText(Text text, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp) {
		System.out.println("fillText " + ctm + " " + traceColor(cs, color, alpha));
		traceText(text);
	}

	public void strokeText(Text text, StrokeState stroke, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp) {
		System.out.println("strokeText " + ctm + " " + traceStroke(stroke) + " " + traceColor(cs, color, alpha));
		traceText(text);
	}

	public void clipText(Text text, Matrix ctm) {
		System.out.println("clipText " + ctm);
		traceText(text);
	}

	public void clipStrokeText(Text text, StrokeState stroke, Matrix ctm) {
		System.out.println("clipStrokeText " + ctm + " " + traceStroke(stroke));
		traceText(text);
	}

	public void ignoreText(Text text, Matrix ctm) {
		System.out.println("ignoreText " + ctm);
		traceText(text);
	}

	public void fillShade(Shade shd, Matrix ctm, float alpha, int cp) {
		System.out.println("fillShade " + ctm + " " + alpha);
	}

	public void fillImage(Image img, Matrix ctm, float alpha, int cp) {
		System.out.println("fillImage " + ctm + " " + alpha);
	}

	public void fillImageMask(Image img, Matrix ctm, ColorSpace cs, float[] color, float alpha, int cp) {
		System.out.println("fillImageMask " + ctm + " " + traceColor(cs, color, alpha));
	}

	public void clipImageMask(Image img, Matrix ctm) {
		System.out.println("clipImageMask " + ctm);
	}

	public void popClip() {
		System.out.println("popClip");
	}

	public void beginMask(Rect rect, boolean luminosity, ColorSpace cs, float[] bc, int cp) {
		System.out.println("beginMask r=" + rect +
				" l=" + luminosity +
				" " + traceColor(cs, bc, 1));
	}

	public void endMask() {
		System.out.println("endMask");
	}

	public void beginGroup(Rect rect, ColorSpace cs, boolean isolated, boolean knockout, int blendmode, float alpha) {
		System.out.println("beginGroup r=" + rect +
				" i=" + isolated +
				" k=" + knockout +
				" bm=" + blendmode +
				" a=" + alpha);
	}

	public void endGroup() {
		System.out.println("endGroup");
	}

	public int beginTile(Rect area, Rect view, float xstep, float ystep, Matrix ctm, int id) {
		System.out.println("beginTile");
		return 0;
	}

	public void endTile() {
		System.out.println("endTile");
	}

	public void beginLayer(String name) {
		System.out.println("beginLayer");
	}

	public void endLayer() {
		System.out.println("endLayer");
	}

	public static void main(String[] args) {
		Document doc = Document.openDocument("pdfref17.pdf");
		Page page = doc.loadPage(1144);
		TraceDevice dev = new TraceDevice();
		page.run(dev, new Matrix());
	}
}
