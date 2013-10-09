--oo/edit: standard edit control.
setfenv(1, require'winapi')
require'winapi.controlclass'
require'winapi.edit'
require'winapi.wingdi'

Edit = subclass({
	__style_bitmask = bitmask{
		tabstop = WS_TABSTOP,
		border = WS_BORDER,
		readonly = ES_READONLY,
		multiline = ES_MULTILINE,
		password = ES_PASSWORD,
		autovscroll = ES_AUTOVSCROLL,
		autohscroll = ES_AUTOHSCROLL,
		number = ES_NUMBER,
		dont_hide_selection = ES_NOHIDESEL,
		want_return = ES_WANTRETURN,
		align = {
			left = ES_LEFT,
			right = ES_RIGHT,
			center = ES_CENTER,
		},
		case = {
			upper = ES_UPPERCASE,
			lower = ES_LOWERCASE,
		},
	},
	__style_ex_bitmask = bitmask{
		client_edge = WS_EX_CLIENTEDGE,
	},
	__default_style = {
		tabstop = true,
		client_edge = true,
		align = 'left',
		case = 'normal',
	},
	__defaults = {
		text = '',
		w = 100, h = 21,
		readonly = false,
		client_edge = true,
	},
	__init_properties = {
		'limit', 'password_char', 'tabstops', 'margins', 'cue',
	},
	__wm_command_handler_names = index{
		on_setfocus = EN_SETFOCUS,
		on_killfocus = EN_KILLFOCUS,
		on_change = EN_CHANGE,
		on_update = EN_UPDATE,
		on_errspace = EN_ERRSPACE,
		on_maxtext = EN_MAXTEXT,
		on_hscroll = EN_HSCROLL,
		on_vscroll = EN_VSCROLL,
		on_align_ltr_ec = EN_ALIGN_LTR_EC,
		on_align_rtl_ec = EN_ALIGN_RTL_EC,
	},
}, Control)

function Edit:__before_create(info, args)
	Edit.__index.__before_create(self, info, args)
	args.class = WC_EDIT
end

function Edit:get_limit() return Edit_GetLimitText(self.hwnd) end
function Edit:set_limit(limit) Edit_SetLimitText(self.hwnd, limit) end

function Edit:get_selection_indices() return {Edit_GetSel(self.hwnd)} end
function Edit:set_selection_indices(t) Edit_SetSel(self.hwnd, unpack(t,1,2)) end
function Edit:select(i,j) Edit_SetSel(self.hwnd, i, j) end

function Edit:get_selection_text(s) return ':(' end --TODO: get_selection_text
function Edit:set_selection_text(s) Edit_ReplaceSel(self.hwnd, s) end

function Edit:get_modified() return Edit_GetModify(self.hwnd) end
function Edit:set_modified(yes) Edit_SetModify(self.hwnd, yes) end

function Edit:scroll_caret() Edit_ScrollCaret(self.hwnd) end

function Edit:get_first_visible_line() return Edit_GetFirstVisibleLine(self.hwnd) end
function Edit:line_from_char(charindex) return Edit_LineFromChar(self.hwnd, charindex) end
function Edit:line_index(lineindex) return Edit_LineIndex(self.hwnd, lineindex) end
function Edit:line_length(lineindex) return Edit_LineLength(self.hwnd, lineindex) end
function Edit:scroll(dv, dh) return Edit_Scroll(self.hwnd, dv, dh) end

function Edit:get_can_undo() return Edit_CanUndo(self.hwnd) end
function Edit:undo() return Edit_Undo(self.hwnd) end
function Edit:clear_undo() return Edit_EmptyUndoBuffer(self.hwnd) end

function Edit:set_password_char(s) Edit_SetPasswordChar(self.hwnd, wcs(s)[0]) end
function Edit:get_password_char() return mbs(ffi.new('WCHAR[1]', Edit_GetPasswordChar(self.hwnd))) end

function Edit:set_tabstops(tabs) Edit_SetTabStops(self.hwnd, tabs) end --stateful

function Edit:get_margins() return {Edit_GetMargins(self.hwnd)} end
function Edit:set_margins(t) Edit_SetMargins(self.hwnd, unpack(t)) end

function Edit:get_cue() return '' or Edit_GetCueBannerText(self.hwnd) end --TODO: returns FALSE
function Edit:set_cue(s, show_when_focused) Edit_SetCueBannerText(self.hwnd, s, show_when_focused) end

function Edit:show_balloon(title, text, TTI) Edit_ShowBalloonTip(self.hwnd, title, text, TTI) end
function Edit:hide_balloon() Edit_HideBalloonTip(self.hwnd) end


--showcase

if not ... then
require'winapi.showcase'
local window = ShowcaseWindow{w=300,h=200}
local e1 = Edit{parent = window, x = 10, y = 10, case = 'upper', limit = 8}
function e1:on_change() print('changed', self.text) end
e1.text = 'hello'

local e2 = Edit{parent = window, x = 10, y = 40, align = 'right'}
e2.text = 'hola'

local e3 = Edit{x = 10, y = 70, visible = false}
e3.parent = window
e3.visible = true
window.visible = false
print('visible', e2.visible, e2.is_visible)
for e in window:children() do print(e.text) end
window.visible = true

e1:focus()
e1:select(2,4)
print(unpack(e1.selection_indices))
e1.selection_text = 'xx'
e1.margins = {15, 15}
print(unpack(e1.margins))

e3:set_cue('Search', true)
require'winapi.tooltip'
e3:show_balloon('Duude', 'This is Cool!', TTI_INFO)

e4 = Edit{x = 10, y = 100, parent = window, readonly = true}
e4.text = "Can't touch this"

MessageLoop()
end

