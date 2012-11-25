package com.artifex.mupdf;

// Version of MuPDFAlert without enums to simplify JNI
public class MuPDFAlertInternal {
	public final String message;
	public final int iconType;
	public final int buttonGroupType;
	public final String title;
	public int buttonPressed;

	MuPDFAlertInternal(String aMessage, int aIconType, int aButtonGroupType, String aTitle, int aButtonPressed) {
		message = aMessage;
		iconType = aIconType;
		buttonGroupType = aButtonGroupType;
		title = aTitle;
		buttonPressed = aButtonPressed;
	}

	MuPDFAlertInternal(MuPDFAlert alert) {
		message = alert.message;
		iconType = alert.iconType.ordinal();
		buttonGroupType = alert.buttonGroupType.ordinal();
		title = alert.message;
		buttonPressed = alert.buttonPressed.ordinal();
	}

	MuPDFAlert toAlert() {
		return new MuPDFAlert(message, MuPDFAlert.IconType.values()[iconType], MuPDFAlert.ButtonGroupType.values()[buttonGroupType], title, MuPDFAlert.ButtonPressed.values()[buttonPressed]);
	}
}
