#ifndef __WDL_EEL_EDITOR_H_
#define __WDL_EEL_EDITOR_H_

#define STATE_BEFORE_CODE -1

#include "curses_editor.h"
#include "../assocarray.h"

// add EEL syntax highlighting and paren matching, hooks for watch/etc
class EEL_Editor : public WDL_CursesEditor
{
public:
  EEL_Editor(void *cursesCtx);
  virtual ~EEL_Editor();

  virtual void draw_line_highlight(int y, const char *p, int *c_comment_state);
  virtual int do_draw_line(const char *p, int *c_comment_state, int last_attr);
  virtual int GetCommentStateForLineStart(int line); 
  virtual bool LineCanAffectOtherLines(const char *txt, int spos, int slen); // if multiline comment etc
  virtual void doWatchInfo(int c);
  virtual void doParenMatching();

  virtual int onChar(int c);
  virtual void onRightClick(HWND hwnd);
  virtual void draw_bottom_line();

#ifdef WDL_IS_FAKE_CURSES
  virtual LRESULT onMouseMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif

  virtual int overrideSyntaxDrawingForLine(int *skipcnt, const char **p, int *c_comment_state, int *last_attr) { return 0; }
  virtual int namedTokenHighlight(const char *tokStart, int len, int state);

  virtual int is_code_start_line(const char *p) { return 0; } // pass NULL to see if code-start-lines are even used

  virtual void draw_string_internal(int *skipcnt, const char *str, int amt, int *attr, int newAttr);
  virtual void draw_string_urlchk(int *skipcnt, const char *str, int amt, int *attr, int newAttr);
  virtual void draw_string(int *skipcnt, const char *str, int amt, int *attr, int newAttr, int comment_string_state=0);

  virtual bool sh_draw_parenttokenstack_pop(char c);
  virtual bool sh_draw_parentokenstack_update(const char *tok, int toklen);
  virtual const char *sh_tokenize(const char **ptr, const char *endptr, int *lenOut, int *state);

  virtual bool peek_want_VM_funcs() { return false; } // implement if syntax highlighting should (and can safely) call peek_get_VM()
  virtual void *peek_get_VM() { return NULL; } // returns NSEEL_VMCTX (if supported)
  virtual int peek_get_named_string_value(const char *name, char *sstr, size_t sstr_sz) { return -1; } // returns >=0 (index) if found
  virtual bool peek_get_numbered_string_value(double idx, char *sstr, size_t sstr_sz) { return false; }
  virtual bool peek_get_variable_info(const char *name, char *sstr, size_t sstr_sz);

  virtual void peek_lock() { }
  virtual void peek_unlock() { }
  virtual void on_help(const char *str, int curChar) { } // curChar is current character if str is NULL

  virtual bool line_has_openable_file(const char *line, int cursor_bytepos, char *fnout, size_t fnout_sz) { return false; }
  virtual int peek_get_function_info(const char *name, char *sstr, size_t sstr_sz, int chkmask, int ignoreline); // mask: 1=builtin, 2=m_added_funclist, 4=user functions. ignoreline= line to ignore function defs on.

  virtual void draw_top_line();

  // static helpers
  static WDL_TypedBuf<char> s_draw_parentokenstack;
  static int parse_format_specifier(const char *fmt_in, int *var_offs, int *var_len);

  WDL_StringKeyedArray<char *> *m_added_funclist; // caller can use this

  WDL_FastString m_suggestion;
  int m_suggestion_x,m_suggestion_y;

  bool m_case_sensitive; // for function detection, and maybe other places
  const char *m_function_prefix; // defaults to "function "
  const char *m_comment_str; // defaults to "//"
};

#endif
