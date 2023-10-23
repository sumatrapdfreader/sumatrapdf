// Copyright (C) 2004-2023 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

package com.artifex.mupdf.fitz;

import java.io.InputStream;
import java.util.Date;

public class PDFDocument extends Document
{
	/* Languages, keep in sync with FZ_LANG_* */
	public static final int LANGUAGE_UNSET = 0;
	public static final int LANGUAGE_ur = 507;
	public static final int LANGUAGE_urd = 3423;
	public static final int LANGUAGE_ko = 416;
	public static final int LANGUAGE_ja = 37;
	public static final int LANGUAGE_zh = 242;
	public static final int LANGUAGE_zh_Hans = 14093;
	public static final int LANGUAGE_zh_Hant = 14822;

	/* Page Label styles */
	public static final int PAGE_LABEL_NONE = 0;
	public static final int PAGE_LABEL_DECIMAL = 'D';
	public static final int PAGE_LABEL_ROMAN_UC = 'R';
	public static final int PAGE_LABEL_ROMAN_LC = 'r';
	public static final int PAGE_LABEL_ALPHA_UC = 'A';
	public static final int PAGE_LABEL_ALPHA_LC = 'a';

	static {
		Context.init();
	}

	private static native long newNative();
	protected native void finalize();

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
	public native void graftPage(int pageTo, PDFDocument src, int pageFrom);

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
	public native boolean wasRepaired();
	public native boolean canBeSavedIncrementally();
	public native boolean isRedacted();

	public native void save(String filename, String options);

	protected native void nativeSaveWithStream(SeekableInputOutputStream stream, String options);
	public void save(SeekableInputOutputStream stream, String options) {
		nativeSaveWithStream(stream, options);
	}

	public interface JsEventListener {
		public static final int BUTTON_GROUP_OK = 0;
		public static final int BUTTON_GROUP_OK_CANCEL = 1;
		public static final int BUTTON_GROUP_YES_NO = 2;
		public static final int BUTTON_GROUP_YES_NO_CANCEL = 3;

		public static final int BUTTON_NONE = 0;
		public static final int BUTTON_OK = 1;
		public static final int BUTTON_CANCEL = 2;
		public static final int BUTTON_NO = 3;
		public static final int BUTTON_YES = 4;

		public static class AlertResult {
			public int buttonPressed;
			public boolean checkboxChecked;
		}

		public AlertResult onAlert(PDFDocument doc, String title, String message, int iconType, int buttonGroupType, boolean hasCheckbox, String checkboxMessage, boolean checkboxState);
	}
	public native void enableJs();
	public native void disableJs();
	public native boolean isJsSupported();
	public native void setJsEventListener(JsEventListener listener);
	public native void calculate(); /* Recalculate form fields. Not needed if using page.update(). */
	public native int getVersion();

	public boolean hasAcroForm() {
		return this.getTrailer().get("Root").get("AcroForm").get("Fields").size() > 0;
	}

	public boolean hasXFAForm() {
		return !this.getTrailer().get("Root").get("AcroForm").get("XFA").isNull();
	}

	public native int countVersions();
	public native int countUnsavedVersions();
	public native int validateChangeHistory();
	public native boolean wasPureXFA();
	public native boolean wasLinearized();

	public native void enableJournal();
	public native void saveJournal(String filename);
	public native void saveJournalWithStream(SeekableOutputStream stream);
	public native void loadJournal(String filename);
	public native void loadJournalWithStream(SeekableInputStream stream);

	public native int undoRedoPosition();
	public native int undoRedoSteps();
	public native String undoRedoStep(int step);

	public native boolean canUndo();
	public native boolean canRedo();
	public native void undo();
	public native void redo();

	public native void beginOperation(String operation);
	public native void beginImplicitOperation();
	public native void endOperation();
	public native void abandonOperation();

	public native int getLanguage();
	public native void setLanguage(int lang);

	public native void setPageLabels(int index, int style, String prefix, int start);
	public native void deletePageLabels(int index);
	protected static native String formatURIFromPathAndNamedDest(String path, String name);
	protected static native String formatURIFromPathAndExplicitDest(String path, LinkDestination dest);
	protected static native String appendNamedDestToURI(String uri, String name);
	protected static native String appendExplicitDestToURI(String uri, LinkDestination dest);

	public static String formatURIFromPathAndDest(String path, String name) {
		return formatURIFromPathAndNamedDest(path, name);
	}
	public static String formatURIFromPathAndDest(String path, LinkDestination dest) {
		return formatURIFromPathAndExplicitDest(path, dest);
	}

	public static String appendDestToURI(String uri, String name) {
		return appendNamedDestToURI(uri, name);
	}
	public static String appendDestToURI(String uri, LinkDestination dest) {
		return appendExplicitDestToURI(uri, dest);
	}

	public native int countSignatures();

	public native PDFObject addEmbeddedFile(String filename, String mimetype, Buffer contents, long created, long modified, boolean addChecksum);
	public native PDFEmbeddedFileParams getEmbeddedFileParams(PDFObject fs);
	public native Buffer loadEmbeddedFileContents(PDFObject fs);
	public native boolean verifyEmbeddedFileChecksum(PDFObject fs);

	public PDFObject addEmbeddedFile(String filename, String mimetype, InputStream stream, Date created, Date modified, boolean addChecksum) {
		Buffer contents = new Buffer();
		contents.writeFromStream(stream);
		long createdTime = created != null ? created.getTime() : -1;
		long modifiedTime = modified != null ? modified.getTime() : -1;
		return addEmbeddedFile(filename, mimetype, contents, createdTime, modifiedTime, addChecksum);
	}

	public static class PDFEmbeddedFileParams {
		public final String filename;
		public final String mimetype;
		public final int size;
		public final Date creationDate;
		public final Date modificationDate;

		protected PDFEmbeddedFileParams(String filename, String mimetype, int size, long created, long modified) {
			this.filename = filename;
			this.mimetype = mimetype;
			this.size = size;
			this.creationDate = new Date(created);
			this.modificationDate = new Date(modified);
		}
	}

}
