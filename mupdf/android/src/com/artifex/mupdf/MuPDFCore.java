package com.artifex.mupdf;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.PointF;
import android.graphics.RectF;

public class MuPDFCore
{
	/* load our native library */
	static {
		System.loadLibrary("mupdf");
	}

	/* Readable members */
	private int pageNum  = -1;;
	private int numPages = -1;
	public  float pageWidth;
	public  float pageHeight;

	/* The native functions */
	private static native int openFile(String filename);
	private static native int countPagesInternal();
	private static native void markDirtyInternal(int page);
	private static native void gotoPageInternal(int localActionPageNum);
	private static native float getPageWidth();
	private static native float getPageHeight();
	private static native void drawPage(Bitmap bitmap,
			int pageW, int pageH,
			int patchX, int patchY,
			int patchW, int patchH);
	private static native RectF[] searchPage(String text);
	private static native int getPageLink(int page, float x, float y);
	private static native int passClickEventInternal(int page, float x, float y);
	private static native int setFocusedWidgetTextInternal(String text);
	private static native String getFocusedWidgetTextInternal();
	private static native int getFocusedWidgetTypeInternal();
	private static native LinkInfo [] getPageLinksInternal(int page);
	private static native RectF[] getWidgetAreasInternal(int page);
	private static native OutlineItem [] getOutlineInternal();
	private static native boolean hasOutlineInternal();
	private static native boolean needsPasswordInternal();
	private static native boolean authenticatePasswordInternal(String password);
	private static native void destroying();

	public static native boolean javascriptSupported();

	public MuPDFCore(String filename) throws Exception
	{
		if (openFile(filename) <= 0)
		{
			throw new Exception("Failed to open "+filename);
		}
	}

	public  int countPages()
	{
		if (numPages < 0)
			numPages = countPagesSynchronized();

		return numPages;
	}

	private synchronized int countPagesSynchronized() {
		return countPagesInternal();
	}

	/* Shim function */
	private void gotoPage(int page)
	{
		if (page > numPages-1)
			page = numPages-1;
		else if (page < 0)
			page = 0;
		if (this.pageNum == page)
			return;
		gotoPageInternal(page);
		this.pageNum = page;
		this.pageWidth = getPageWidth();
		this.pageHeight = getPageHeight();
	}

	public synchronized PointF getPageSize(int page) {
		gotoPage(page);
		return new PointF(pageWidth, pageHeight);
	}

	public synchronized void onDestroy() {
		destroying();
	}

	public synchronized Bitmap drawPage(int page,
			int pageW, int pageH,
			int patchX, int patchY,
			int patchW, int patchH) {
		gotoPage(page);
		Bitmap bm = Bitmap.createBitmap(patchW, patchH, Config.ARGB_8888);
		drawPage(bm, pageW, pageH, patchX, patchY, patchW, patchH);
		return bm;
	}

	public synchronized PassClickResult passClickEvent(int page, float x, float y) {
		boolean changed = passClickEventInternal(page, x, y) != 0;
		int type = getFocusedWidgetTypeInternal();
		WidgetType wtype = WidgetType.values()[type];
		String text;

		switch (wtype)
		{
		case TEXT:
			text = getFocusedWidgetTextInternal();
			break;
		default:
			text = "";
			break;
		}

		if (changed)
			markDirtyInternal(page);

		return new PassClickResult(changed, wtype, text);
	}

	public synchronized boolean setFocusedWidgetText(int page, String text) {
		boolean success;
		gotoPage(page);
		success = setFocusedWidgetTextInternal(text) != 0 ? true : false;

		if (success)
			markDirtyInternal(page);

		return success;
	}

	public synchronized int hitLinkPage(int page, float x, float y) {
		return getPageLink(page, x, y);
	}

	public synchronized LinkInfo [] getPageLinks(int page) {
		return getPageLinksInternal(page);
	}

	public synchronized RectF [] getWidgetAreas(int page) {
		return getWidgetAreasInternal(page);
	}

	public synchronized RectF [] searchPage(int page, String text) {
		gotoPage(page);
		return searchPage(text);
	}

	public synchronized boolean hasOutline() {
		return hasOutlineInternal();
	}

	public synchronized OutlineItem [] getOutline() {
		return getOutlineInternal();
	}

	public synchronized boolean needsPassword() {
		return needsPasswordInternal();
	}

	public synchronized boolean authenticatePassword(String password) {
		return authenticatePasswordInternal(password);
	}
}
