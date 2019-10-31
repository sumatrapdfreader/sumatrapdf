BEGIN		{ last_char = "x"; }
/^$/		{ if (last_char == "{") print(FILENAME, FNR); last_char = " "; next; }
/^\s*{$/	{ last_char = "{"; next; }
/ {$/		{ last_char = "{"; next; }
/^\s*}$/	{ if (last_char == " ") print(FILENAME, FNR); last_char = "}"; next; }
		{ last_char = "x"; }
