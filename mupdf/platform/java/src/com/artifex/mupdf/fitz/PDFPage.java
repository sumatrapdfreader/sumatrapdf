package com.artifex.mupdf.fitz;

public class PDFPage extends Page
{
	static {
		Context.init();
	}

	private PDFPage(long p) { super(p); }

	public native PDFAnnotation[] getAnnotations();
	public native PDFAnnotation createAnnotation(int subtype);
	public native void deleteAnnotation(PDFAnnotation annot);

	public static final int REDACT_IMAGE_NONE = 0;
	public static final int REDACT_IMAGE_REMOVE = 1;
	public static final int REDACT_IMAGE_PIXELS = 2;

	public native boolean applyRedactions(boolean blackBoxes, int imageMethod);

	public boolean applyRedactions() {
		return applyRedactions(true, REDACT_IMAGE_PIXELS);
	}

	public native boolean update();

	private PDFWidget[] widgets;
	private native PDFWidget[] getWidgetsNative();

	public PDFWidget[] getWidgets() {
		if (widgets == null)
			widgets = getWidgetsNative();
		return widgets;
	}

	public PDFWidget activateWidgetAt(float pageX, float pageY) {
		for (PDFWidget widget : getWidgets()) {
			if (widget.getBounds().contains(pageX, pageY)) {
				widget.eventEnter();
				widget.eventDown();
				widget.eventFocus();
				widget.eventUp();
				widget.eventExit();
				widget.eventBlur();
				return widget;
			}
		}
		return null;
	}
}
