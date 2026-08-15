BEGIN			{ try=0; tryline=0; }
/fz_try\(ctx\)/		{ try++; if (try == 1) tryline=FNR }
/fz_catch\(ctx\)/	{ try--; if (try == 0) tryline=0 }
/^[\t ]*return/		{ if (try > 0) { print(FILENAME, FNR, tryline); print($0); } }
