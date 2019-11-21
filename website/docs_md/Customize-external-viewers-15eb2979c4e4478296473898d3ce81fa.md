# [Documentation](/docs/) : Customize external viewers

You might have other viewers installed alongside Sumatra and you might wish to open current file in alternative viewer.

We make it easy for you. By default we auto-detect several other PDF readers (Adobe Acrobat, Foxit, PDF XChange etc.). If they are present, we add `Open with Adobe Reader` (etc.) items to `File` menu (available when you have .pdf document opened in Sumatra).

You can also add custom readers by using [advanced settings](http://sumatrapdfreader.org/settings.html) .

To configure a new external reader:

- use `Settings / Advanced Settings...` menu to open configuration file
- find `ExternalViewers` section and fill it out

Here are the relevant settings:

	ExternalViewers [
	 [
	 CommandLine =
	 Name =
	 Filter =
	 ]
	]

 `CommandLine` is a full path to program's executable. You need to add `"%1"` at the end (will be substituted for a full path of the document. You can also use `%p` which will be substituted for a page number (assuming that the viewer has a command-line option to open a file at a given page).

 `Name` is the name of the program that will be displayed in `File` menu.

 `Filter` dictates what kind of files can be opened by the reader. For example:

- to only activate it for PDF files, use `*.pdf` 
- to activate for PNG and JPEG files, use `*.png;*.jpg;*.jpeg` 

You can configure multiple external viewers.