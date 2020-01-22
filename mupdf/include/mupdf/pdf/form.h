#ifndef MUPDF_PDF_FORM_H
#define MUPDF_PDF_FORM_H

/* Types of widget */
enum pdf_widget_type
{
	PDF_WIDGET_TYPE_UNKNOWN,
	PDF_WIDGET_TYPE_BUTTON,
	PDF_WIDGET_TYPE_CHECKBOX,
	PDF_WIDGET_TYPE_COMBOBOX,
	PDF_WIDGET_TYPE_LISTBOX,
	PDF_WIDGET_TYPE_RADIOBUTTON,
	PDF_WIDGET_TYPE_SIGNATURE,
	PDF_WIDGET_TYPE_TEXT,
};

/* Types of text widget content */
enum pdf_widget_tx_format
{
	PDF_WIDGET_TX_FORMAT_NONE,
	PDF_WIDGET_TX_FORMAT_NUMBER,
	PDF_WIDGET_TX_FORMAT_SPECIAL,
	PDF_WIDGET_TX_FORMAT_DATE,
	PDF_WIDGET_TX_FORMAT_TIME
};

pdf_widget *pdf_keep_widget(fz_context *ctx, pdf_widget *widget);
void pdf_drop_widget(fz_context *ctx, pdf_widget *widget);
pdf_widget *pdf_first_widget(fz_context *ctx, pdf_page *page);
pdf_widget *pdf_next_widget(fz_context *ctx, pdf_widget *previous);
int pdf_update_widget(fz_context *ctx, pdf_widget *widget);

enum pdf_widget_type pdf_widget_type(fz_context *ctx, pdf_widget *widget);

fz_rect pdf_bound_widget(fz_context *ctx, pdf_widget *widget);

int pdf_text_widget_max_len(fz_context *ctx, pdf_widget *tw);
int pdf_text_widget_format(fz_context *ctx, pdf_widget *tw);

int pdf_choice_widget_options(fz_context *ctx, pdf_widget *tw, int exportval, const char *opts[]);
int pdf_choice_widget_is_multiselect(fz_context *ctx, pdf_widget *tw);
int pdf_choice_widget_value(fz_context *ctx, pdf_widget *tw, const char *opts[]);
void pdf_choice_widget_set_value(fz_context *ctx, pdf_widget *tw, int n, const char *opts[]);

int pdf_choice_field_option_count(fz_context *ctx, pdf_obj *field);
const char *pdf_choice_field_option(fz_context *ctx, pdf_obj *field, int exportval, int i);

/* Field flags */
enum
{
	/* All fields */
	PDF_FIELD_IS_READ_ONLY = 1,
	PDF_FIELD_IS_REQUIRED = 1 << 1,
	PDF_FIELD_IS_NO_EXPORT = 1 << 2,

	/* Text fields */
	PDF_TX_FIELD_IS_MULTILINE = 1 << 12,
	PDF_TX_FIELD_IS_PASSWORD = 1 << 13,
	PDF_TX_FIELD_IS_FILE_SELECT = 1 << 20,
	PDF_TX_FIELD_IS_DO_NOT_SPELL_CHECK = 1 << 22,
	PDF_TX_FIELD_IS_DO_NOT_SCROLL = 1 << 23,
	PDF_TX_FIELD_IS_COMB = 1 << 24,
	PDF_TX_FIELD_IS_RICH_TEXT = 1 << 25,

	/* Button fields */
	PDF_BTN_FIELD_IS_NO_TOGGLE_TO_OFF = 1 << 14,
	PDF_BTN_FIELD_IS_RADIO = 1 << 15,
	PDF_BTN_FIELD_IS_PUSHBUTTON = 1 << 16,
	PDF_BTN_FIELD_IS_RADIOS_IN_UNISON = 1 << 25,

	/* Choice fields */
	PDF_CH_FIELD_IS_COMBO = 1 << 17,
	PDF_CH_FIELD_IS_EDIT = 1 << 18,
	PDF_CH_FIELD_IS_SORT = 1 << 19,
	PDF_CH_FIELD_IS_MULTI_SELECT = 1 << 21,
	PDF_CH_FIELD_IS_DO_NOT_SPELL_CHECK = 1 << 22,
	PDF_CH_FIELD_IS_COMMIT_ON_SEL_CHANGE = 1 << 26,
};

void pdf_calculate_form(fz_context *ctx, pdf_document *doc);
void pdf_reset_form(fz_context *ctx, pdf_document *doc, pdf_obj *fields, int exclude);

int pdf_field_type(fz_context *ctx, pdf_obj *field);
int pdf_field_flags(fz_context *ctx, pdf_obj *field);
char *pdf_field_name(fz_context *ctx, pdf_obj *field);
const char *pdf_field_value(fz_context *ctx, pdf_obj *field);

char *pdf_field_border_style(fz_context *ctx, pdf_obj *field);
void pdf_field_set_border_style(fz_context *ctx, pdf_obj *field, const char *text);
void pdf_field_set_button_caption(fz_context *ctx, pdf_obj *field, const char *text);
void pdf_field_set_fill_color(fz_context *ctx, pdf_obj *field, pdf_obj *col);
void pdf_field_set_text_color(fz_context *ctx, pdf_obj *field, pdf_obj *col);
int pdf_field_display(fz_context *ctx, pdf_obj *field);
void pdf_field_set_display(fz_context *ctx, pdf_obj *field, int d);
const char *pdf_field_label(fz_context *ctx, pdf_obj *field);
pdf_obj *pdf_button_field_on_state(fz_context *ctx, pdf_obj *field);

int pdf_set_field_value(fz_context *ctx, pdf_document *doc, pdf_obj *field, const char *text, int ignore_trigger_events);
int pdf_set_text_field_value(fz_context *ctx, pdf_widget *widget, const char *value);
int pdf_set_choice_field_value(fz_context *ctx, pdf_widget *widget, const char *value);

int pdf_signature_is_signed(fz_context *ctx, pdf_document *doc, pdf_obj *field);
void pdf_signature_set_value(fz_context *ctx, pdf_document *doc, pdf_obj *field, pdf_pkcs7_signer *signer, int64_t stime);

int pdf_count_signatures(fz_context *ctx, pdf_document *doc);

void pdf_field_reset(fz_context *ctx, pdf_document *doc, pdf_obj *field);

pdf_obj *pdf_lookup_field(fz_context *ctx, pdf_obj *form, const char *name);

/* Form text field editing events: */

typedef struct pdf_keystroke_event_s
{
	const char *value;
	const char *change;
	int selStart, selEnd;
	int willCommit;
	char *newChange;
} pdf_keystroke_event;

int pdf_field_event_keystroke(fz_context *ctx, pdf_document *doc, pdf_obj *field, pdf_keystroke_event *evt);
int pdf_field_event_validate(fz_context *ctx, pdf_document *doc, pdf_obj *field, const char *value);
void pdf_field_event_calculate(fz_context *ctx, pdf_document *doc, pdf_obj *field);
char *pdf_field_event_format(fz_context *ctx, pdf_document *doc, pdf_obj *field);

/* Call these to trigger actions from various UI events: */

void pdf_document_event_will_close(fz_context *ctx, pdf_document *doc);
void pdf_document_event_will_save(fz_context *ctx, pdf_document *doc);
void pdf_document_event_did_save(fz_context *ctx, pdf_document *doc);
void pdf_document_event_will_print(fz_context *ctx, pdf_document *doc);
void pdf_document_event_did_print(fz_context *ctx, pdf_document *doc);

void pdf_page_event_open(fz_context *ctx, pdf_page *page);
void pdf_page_event_close(fz_context *ctx, pdf_page *page);

void pdf_annot_event_enter(fz_context *ctx, pdf_annot *annot);
void pdf_annot_event_exit(fz_context *ctx, pdf_annot *annot);
void pdf_annot_event_down(fz_context *ctx, pdf_annot *annot);
void pdf_annot_event_up(fz_context *ctx, pdf_annot *annot);
void pdf_annot_event_focus(fz_context *ctx, pdf_annot *annot);
void pdf_annot_event_blur(fz_context *ctx, pdf_annot *annot);
void pdf_annot_event_page_open(fz_context *ctx, pdf_annot *annot);
void pdf_annot_event_page_close(fz_context *ctx, pdf_annot *annot);
void pdf_annot_event_page_visible(fz_context *ctx, pdf_annot *annot);
void pdf_annot_event_page_invisible(fz_context *ctx, pdf_annot *annot);

#endif
