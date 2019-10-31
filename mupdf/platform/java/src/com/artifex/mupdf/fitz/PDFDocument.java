package com.artifex.mupdf.fitz;

public class PDFDocument extends Document
{
	static {
		Context.init();
	}

	private static native long newNative();

	protected PDFDocument(long p) {
		super(p);
	}

	public PDFDocument() {
		super(newNative());
	}

	public boolean isPDF() {
		return true;
	}

	public native PDFObject findPage(int at);

	public native PDFObject getTrailer();
	public native int countObjects();

	public native PDFObject newNull();
	public native PDFObject newBoolean(boolean b);
	public native PDFObject newInteger(int i);
	public native PDFObject newReal(float f);
	public native PDFObject newString(String s);
	public native PDFObject newByteString(byte[] bs);
	public native PDFObject newName(String name);
	public native PDFObject newIndirect(int num, int gen);
	public native PDFObject newArray();
	public native PDFObject newDictionary();

	public native PDFObject addObject(PDFObject obj);
	public native PDFObject createObject();
	public native void deleteObject(int i);

	public void deleteObject(PDFObject obj) {
		deleteObject(obj.asIndirect());
	}

	public native PDFGraftMap newPDFGraftMap();
	public native PDFObject graftObject(PDFObject obj);

	private native PDFObject addStreamBuffer(Buffer buf, Object obj, boolean compressed);
	private native PDFObject addStreamString(String str, Object obj, boolean compressed);

	public PDFObject addRawStream(Buffer buf, Object obj) {
		return addStreamBuffer(buf, obj, true);
	}

	public PDFObject addStream(Buffer buf, Object obj) {
		return addStreamBuffer(buf, obj, false);
	}

	public PDFObject addRawStream(String str, Object obj) {
		return addStreamString(str, obj, true);
	}

	public PDFObject addStream(String str, Object obj) {
		return addStreamString(str, obj, false);
	}

	public PDFObject addRawStream(Buffer buf) {
		return addStreamBuffer(buf, null, true);
	}

	public PDFObject addStream(Buffer buf) {
		return addStreamBuffer(buf, null, false);
	}

	public PDFObject addRawStream(String str) {
		return addStreamString(str, null, true);
	}

	public PDFObject addStream(String str) {
		return addStreamString(str, null, false);
	}

	private native PDFObject addPageBuffer(Rect mediabox, int rotate, PDFObject resources, Buffer contents);
	private native PDFObject addPageString(Rect mediabox, int rotate, PDFObject resources, String contents);

	public PDFObject addPage(Rect mediabox, int rotate, PDFObject resources, Buffer contents) {
		return addPageBuffer(mediabox, rotate, resources, contents);
	}

	public PDFObject addPage(Rect mediabox, int rotate, PDFObject resources, String contents) {
		return addPageString(mediabox, rotate, resources, contents);
	}

	public native void insertPage(int at, PDFObject page);
	public native void deletePage(int at);
	public native PDFObject addImage(Image image);
	public native PDFObject addSimpleFont(Font font, int encoding);
	public native PDFObject addCJKFont(Font font, int ordering, int wmode, boolean serif);
	public native PDFObject addFont(Font font);
	public native boolean hasUnsavedChanges();
	public native boolean canBeSavedIncrementally();

	public native void save(String filename, String options);

	protected native void nativeSaveWithStream(SeekableOutputStream stream, String options);
	public void save(SeekableOutputStream stream, String options) {
		nativeSaveWithStream(stream, options);
	}

	public interface JsEventListener {
		void onAlert(String message);
	}
	public native void enableJs();
	public native void disableJs();
	public native boolean isJsSupported();
	public native void setJsEventListener(JsEventListener listener);
	public native void calculate(); /* Recalculate form fields. Not needed if using page.update(). */

	public boolean hasAcroForm() {
		return this.getTrailer().get("Root").get("AcroForm").get("Fields").size() > 0;
	}

	public boolean hasXFAForm() {
		return !this.getTrailer().get("Root").get("AcroForm").get("XFA").isNull();
	}
}
