// Copyright (C) 2004-2021 Artifex Software, Inc.
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

public class PDFWidget extends PDFAnnotation
{
	static {
		Context.init();
	}

	protected PDFWidget(long p) {
		super(p);
	}

	/* IMPORTANT: Keep in sync with mupdf/pdf/widget.h */
	public static final int TYPE_UNKNOWN = 0;
	public static final int TYPE_BUTTON = 1;
	public static final int TYPE_CHECKBOX = 2;
	public static final int TYPE_COMBOBOX = 3;
	public static final int TYPE_LISTBOX = 4;
	public static final int TYPE_RADIOBUTTON = 5;
	public static final int TYPE_SIGNATURE = 6;
	public static final int TYPE_TEXT = 7;

	public static final int TX_FORMAT_NONE = 0;
	public static final int TX_FORMAT_NUMBER = 1;
	public static final int TX_FORMAT_SPECIAL = 2;
	public static final int TX_FORMAT_DATE = 3;
	public static final int TX_FORMAT_TIME = 4;

	/* Field flags */
	public static final int PDF_FIELD_IS_READ_ONLY = 1;
	public static final int PDF_FIELD_IS_REQUIRED = 1 << 1;
	public static final int PDF_FIELD_IS_NO_EXPORT = 1 << 2;

	/* Text fields */
	public static final int PDF_TX_FIELD_IS_MULTILINE = 1 << 12;
	public static final int PDF_TX_FIELD_IS_PASSWORD = 1 << 13;
	public static final int PDF_TX_FIELD_IS_COMB = 1 << 24;

	/* Button fields */
	public static final int PDF_BTN_FIELD_IS_NO_TOGGLE_TO_OFF = 1 << 14;
	public static final int PDF_BTN_FIELD_IS_RADIO = 1 << 15;
	public static final int PDF_BTN_FIELD_IS_PUSHBUTTON = 1 << 16;

	/* Choice fields */
	public static final int PDF_CH_FIELD_IS_COMBO = 1 << 17;
	public static final int PDF_CH_FIELD_IS_EDIT = 1 << 18;
	public static final int PDF_CH_FIELD_IS_SORT = 1 << 19;
	public static final int PDF_CH_FIELD_IS_MULTI_SELECT = 1 << 21;

	/* Signature appearance */
	public static final int PDF_SIGNATURE_SHOW_LABELS = 1;
	public static final int PDF_SIGNATURE_SHOW_DN = 2;
	public static final int PDF_SIGNATURE_SHOW_DATE = 4;
	public static final int PDF_SIGNATURE_SHOW_TEXT_NAME = 8;
	public static final int PDF_SIGNATURE_SHOW_GRAPHIC_NAME = 16;
	public static final int PDF_SIGNATURE_SHOW_LOGO = 32;
	public static final int PDF_SIGNATURE_DEFAULT_APPEARANCE = 63;

	// These don't change after creation, so are cached in java fields.
	private int fieldType;
	private int fieldFlags;
	private int textFormat; /* text field formatting type */
	private int maxLen; /* text field max length */
	private String[] options; /* choice field option list */

	/* All fields */

	public int getFieldType() {
		return fieldType;
	}
	public int getFieldFlags() {
		return fieldFlags;
	}
	public boolean isReadOnly() {
		return (getFieldFlags() & PDF_FIELD_IS_READ_ONLY) != 0;
	}
	public native String getValue();
	public native boolean setValue(String val);
	public native String getLabel();

	/* Button fields */

	public boolean isButton() {
		int ft = getFieldType();
		return ft == TYPE_BUTTON || ft == TYPE_CHECKBOX || ft == TYPE_RADIOBUTTON;
	}
	public boolean isPushButton() {
		return getFieldType() == TYPE_BUTTON;
	}
	public boolean isCheckbox() {
		return getFieldType() == TYPE_CHECKBOX;
	}
	public boolean isRadioButton() {
		return getFieldType() == TYPE_RADIOBUTTON;
	}
	public native boolean toggle();

	/* Text fields */

	public boolean isText() {
		return getFieldType() == TYPE_TEXT;
	}
	public boolean isMultiline() {
		return (getFieldFlags() & PDF_TX_FIELD_IS_MULTILINE) != 0;
	}
	public boolean isPassword() {
		return (getFieldFlags() & PDF_TX_FIELD_IS_PASSWORD) != 0;
	}
	public boolean isComb() {
		return (getFieldFlags() & PDF_TX_FIELD_IS_COMB) != 0;
	}
	public int getMaxLen() {
		return maxLen;
	}
	public int getTextFormat() {
		return textFormat;
	}
	public native boolean setTextValue(String val);

	/* WIP in-line text editing support */
	public native Quad[] textQuads();
	public native void setEditing(boolean state);
	public native boolean isEditing();

	private String originalValue;
	public void startEditing() {
		setEditing(true);
		originalValue = getValue();
	}
	public void cancelEditing() {
		setValue(originalValue);
		setEditing(false);
	}
	public boolean commitEditing(String newValue) {
		setValue(originalValue);
		setEditing(false);
		if (setTextValue(newValue)) {
			return true;
		} else {
			setEditing(true);
			return false;
		}
	}

	/* Choice fields */

	public boolean isChoice() {
		int ft = getFieldType();
		return ft == TYPE_COMBOBOX || ft == TYPE_LISTBOX;
	}
	public boolean isComboBox() {
		return getFieldType() == TYPE_COMBOBOX;
	}
	public boolean isListBox() {
		return getFieldType() == TYPE_LISTBOX;
	}
	public String[] getOptions() {
		return options;
	}
	public native boolean setChoiceValue(String val);

	/* Signature fields */
	private static native Pixmap previewSignatureNative(int width, int height, int lang, PKCS7Signer signer, int flags, Image image, String reason, String location);
	public static Pixmap previewSignature(int width, int height, int lang, PKCS7Signer signer, int flags, Image image, String reason, String location) {
		return previewSignatureNative(width, height, lang, signer, flags, image, reason, location);
	}
	public static Pixmap previewSignature(int width, int height, int lang, PKCS7Signer signer, Image image) {
		return previewSignatureNative(width, height, lang, signer, PDF_SIGNATURE_DEFAULT_APPEARANCE, image, null, null);
	}
	public static Pixmap previewSignature(int width, int height, int lang, PKCS7Signer signer) {
		return previewSignatureNative(width, height, lang, signer, PDF_SIGNATURE_DEFAULT_APPEARANCE, null, null, null);
	}
	public static Pixmap previewSignature(int width, int height, PKCS7Signer signer, Image image) {
		return previewSignatureNative(width, height, PDFDocument.LANGUAGE_UNSET, signer, PDF_SIGNATURE_DEFAULT_APPEARANCE, image, null, null);
	}
	public static Pixmap previewSignature(int width, int height, PKCS7Signer signer) {
		return previewSignatureNative(width, height, PDFDocument.LANGUAGE_UNSET, signer, PDF_SIGNATURE_DEFAULT_APPEARANCE, null, null, null);
	}
	public Pixmap previewSignature(float dpi, PKCS7Signer signer, int flags, Image image, String reason, String location) {
		Rect r = getBounds();
		float scale = dpi / 72.0f;
		int w = Math.round((r.x1 - r.x0) * scale);
		int h = Math.round((r.x1 - r.x0) * scale);
		return previewSignature(w, h, getLanguage(), signer, flags, image, reason, location);
	}
	public Pixmap previewSignature(float dpi, PKCS7Signer signer, Image image) {
		Rect r = getBounds();
		float scale = dpi / 72.0f;
		int w = Math.round((r.x1 - r.x0) * scale);
		int h = Math.round((r.x1 - r.x0) * scale);
		return previewSignature(w, h, getLanguage(), signer, image);
	}
	public Pixmap previewSignature(float dpi, PKCS7Signer signer) {
		Rect r = getBounds();
		float scale = dpi / 72.0f;
		int w = Math.round((r.x1 - r.x0) * scale);
		int h = Math.round((r.x1 - r.x0) * scale);
		return previewSignature(w, h, getLanguage(), signer);
	}
	private native boolean signNative(PKCS7Signer signer, int flags, Image image, String reason, String location);
	public boolean sign(PKCS7Signer signer, int flags, Image image, String reason, String location) {
		return signNative(signer, flags, image, reason, location);
	}
	public boolean sign(PKCS7Signer signer, Image image) {
		return signNative(signer, PDF_SIGNATURE_DEFAULT_APPEARANCE, image, null, null);
	}
	public boolean sign(PKCS7Signer signer) {
		return signNative(signer, PDF_SIGNATURE_DEFAULT_APPEARANCE, null, null, null);
	}
	public native int checkCertificate(PKCS7Verifier verifier);
	public native int checkDigest(PKCS7Verifier verifier);
	public native boolean incrementalChangeAfterSigning();
	public boolean verify(PKCS7Verifier verifier) {
		if (checkDigest(verifier) != PKCS7Verifier.PKCS7VerifierOK)
			return false;
		if (checkCertificate(verifier) != PKCS7Verifier.PKCS7VerifierOK)
			return false;
		return !incrementalChangeAfterSigning();
	}
	public native PKCS7DistinguishedName getDistinguishedName(PKCS7Verifier verifier);

	public native int validateSignature();
	public native void clearSignature();
	public native boolean isSigned();

	public native TextWidgetLayout layoutTextWidget();

	public static class TextWidgetLayout {
		public Matrix matrix;
		public Matrix invMatrix;
		public TextWidgetLineLayout[] lines;
	}

	public static class TextWidgetLineLayout {
		public float x;
		public float y;
		public float fontSize;
		public int index;
		public Rect rect;
		public TextWidgetCharLayout[] chars;
	}

	public static class TextWidgetCharLayout {
		public float x;
		public float advance;
		public int index;
		public Rect rect;
	}
}
