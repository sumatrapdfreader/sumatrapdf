package com.artifex.mupdf;

public class MuPDFAlert {
	public enum IconType {Error,Warning,Question,Status};
	public enum ButtonPressed {None,Ok,Cancel,No,Yes};
	public enum ButtonGroupType {Ok,OkCancel,YesNo,YesNoCancel};

	public final String message;
	public final IconType iconType;
	public final ButtonGroupType buttonGroupType;
	public final String title;
	public ButtonPressed buttonPressed;

	MuPDFAlert(String aMessage, IconType aIconType, ButtonGroupType aButtonGroupType, String aTitle, ButtonPressed aButtonPressed) {
		message = aMessage;
		iconType = aIconType;
		buttonGroupType = aButtonGroupType;
		title = aTitle;
		buttonPressed = aButtonPressed;
	}
}
