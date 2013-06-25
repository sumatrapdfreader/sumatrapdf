#ifndef MUPDF_PDF_EVENT_H
#define MUPDF_PDF_EVENT_H

enum
{
	HOTSPOT_POINTER_DOWN = 0x1,
	HOTSPOT_POINTER_OVER = 0x2
};

/* Types of UI event */
enum
{
	PDF_EVENT_TYPE_POINTER,
};

/* Types of pointer event */
enum
{
	PDF_POINTER_DOWN,
	PDF_POINTER_UP,
};

/*
	UI events that can be passed to an interactive document.
*/
typedef struct pdf_ui_event_s
{
	int etype;
	union
	{
		struct
		{
			int ptype;
			fz_point pt;
		} pointer;
	} event;
} pdf_ui_event;

/*
	pdf_init_ui_pointer_event: Set up a pointer event
*/
void pdf_init_ui_pointer_event(pdf_ui_event *event, int type, float x, float y);

/*
	Document events: the objects via which MuPDF informs the calling app
	of occurrences emanating from the document, possibly from user interaction
	or javascript execution. MuPDF informs the app of document events via a
	callback.
*/

/*
	pdf_pass_event: Pass a UI event to an interactive
	document.

	Returns a boolean indication of whether the ui_event was
	handled. Example of use for the return value: when considering
	passing the events that make up a drag, if the down event isn't
	accepted then don't send the move events or the up event.
*/
int pdf_pass_event(pdf_document *doc, pdf_page *page, pdf_ui_event *ui_event);

struct pdf_doc_event_s
{
	int type;
};

enum
{
	PDF_DOCUMENT_EVENT_ALERT,
	PDF_DOCUMENT_EVENT_PRINT,
	PDF_DOCUMENT_EVENT_LAUNCH_URL,
	PDF_DOCUMENT_EVENT_MAIL_DOC,
	PDF_DOCUMENT_EVENT_SUBMIT,
	PDF_DOCUMENT_EVENT_EXEC_MENU_ITEM,
	PDF_DOCUMENT_EVENT_EXEC_DIALOG
};

/*
	pdf_set_doc_event_callback: set the function via which to receive
	document events.
*/
void pdf_set_doc_event_callback(pdf_document *doc, pdf_doc_event_cb *event_cb, void *data);

/*
	The various types of document events
*/

/*
	pdf_alert_event: details of an alert event. In response the app should
	display an alert dialog with the bittons specified by "button_type_group".
	If "check_box_message" is non-NULL, a checkbox should be displayed in
	the lower-left corned along with the messsage.

	"finally_checked" and "button_pressed" should be set by the app
	before returning from the callback. "finally_checked" need be set
	only if "check_box_message" is non-NULL.
*/
typedef struct
{
	char *message;
	int icon_type;
	int button_group_type;
	char *title;
	char *check_box_message;
	int initially_checked;
	int finally_checked;
	int button_pressed;
} pdf_alert_event;

/* Possible values of icon_type */
enum
{
	PDF_ALERT_ICON_ERROR,
	PDF_ALERT_ICON_WARNING,
	PDF_ALERT_ICON_QUESTION,
	PDF_ALERT_ICON_STATUS
};

/* Possible values of button_group_type */
enum
{
	PDF_ALERT_BUTTON_GROUP_OK,
	PDF_ALERT_BUTTON_GROUP_OK_CANCEL,
	PDF_ALERT_BUTTON_GROUP_YES_NO,
	PDF_ALERT_BUTTON_GROUP_YES_NO_CANCEL
};

/* Possible values of button_pressed */
enum
{
	PDF_ALERT_BUTTON_NONE,
	PDF_ALERT_BUTTON_OK,
	PDF_ALERT_BUTTON_CANCEL,
	PDF_ALERT_BUTTON_NO,
	PDF_ALERT_BUTTON_YES
};

/*
	pdf_access_alert_event: access the details of an alert event
	The returned pointer and all the data referred to by the
	structire are owned by mupdf and need not be freed by the
	caller.
*/
pdf_alert_event *pdf_access_alert_event(pdf_doc_event *event);

/*
	pdf_access_exec_menu_item_event: access the details of am execMenuItem
	event, which consists of just the name of the menu item
*/
char *pdf_access_exec_menu_item_event(pdf_doc_event *event);

/*
	pdf_submit_event: details of a submit event. The app should submit
	the specified data to the specified url. "get" determines whether
	to use the GET or POST method.
*/
typedef struct
{
	char *url;
	char *data;
	int data_len;
	int get;
} pdf_submit_event;

/*
	pdf_access_submit_event: access the details of a submit event
	The returned pointer and all data referred to by the structure are
	owned by mupdf and need not be freed by the caller.
*/
pdf_submit_event *pdf_access_submit_event(pdf_doc_event *event);

/*
	pdf_launch_url_event: details of a launch-url event. The app should
	open the url, either in a new frame or in the current window.
*/
typedef struct
{
	char *url;
	int new_frame;
} pdf_launch_url_event;

/*
	pdf_access_launch_url_event: access the details of a launch-url
	event. The returned pointer and all data referred to by the structure
	are owned by mupdf and need not be freed by the caller.
*/
pdf_launch_url_event *pdf_access_launch_url_event(pdf_doc_event *event);

/*
	pdf_mail_doc_event: details of a mail_doc event. The app should save
	the current state of the document and email it using the specified
	parameters.
*/
typedef struct
{
	int ask_user;
	char *to;
	char *cc;
	char *bcc;
	char *subject;
	char *message;
} pdf_mail_doc_event;

/*
	pdf_acccess_mail_doc_event: access the details of a mail-doc event.
*/
pdf_mail_doc_event *pdf_access_mail_doc_event(pdf_doc_event *event);

void pdf_event_issue_alert(pdf_document *doc, pdf_alert_event *event);
void pdf_event_issue_print(pdf_document *doc);
void pdf_event_issue_exec_menu_item(pdf_document *doc, char *item);
void pdf_event_issue_exec_dialog(pdf_document *doc);
void pdf_event_issue_launch_url(pdf_document *doc, char *url, int new_frame);
void pdf_event_issue_mail_doc(pdf_document *doc, pdf_mail_doc_event *event);

#endif
