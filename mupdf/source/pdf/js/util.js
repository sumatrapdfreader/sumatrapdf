Error.prototype.toString = function() {
	if (this.stackTrace) return this.name + ': ' + this.message + this.stackTrace;
	return this.name + ': ' + this.message;
};

// display must be kept in sync with an enum in pdf_form.c
var display = {
	visible: 0,
	hidden: 1,
	noPrint: 2,
	noView: 3,
};

var border = {
	b: 'beveled',
	d: 'dashed',
	i: 'inset',
	s: 'solid',
	u: 'underline',
};

var color = {
	transparent: [ 'T' ],
	black: [ 'G', 0 ],
	white: [ 'G', 1 ],
	gray: [ 'G', 0.5 ],
	ltGray: [ 'G', 0.75 ],
	dkGray: [ 'G', 0.25 ],
	red: [ 'RGB', 1, 0, 0 ],
	green: [ 'RGB', 0, 1, 0 ],
	blue: [ 'RGB', 0, 0, 1 ],
	cyan: [ 'CMYK', 1, 0, 0, 0 ],
	magenta: [ 'CMYK', 0, 1, 0, 0 ],
	yellow: [ 'CMYK', 0, 0, 1, 0 ],
};

color.convert = function (c, colorspace) {
	switch (colorspace) {
	case 'G':
		if (c[0] === 'RGB')
			return [ 'G', c[1] * 0.3 + c[2] * 0.59 + c[3] * 0.11 ];
		if (c[0] === 'CMYK')
			return [ 'CMYK', 1 - Math.min(1, c[1] * 0.3 + c[2] * 0.59 + c[3] * 0.11 + c[4])];
		break;
	case 'RGB':
		if (c[0] === 'G')
			return [ 'RGB', c[1], c[1], c[1] ];
		if (c[0] === 'CMYK')
			return [ 'RGB',
				1 - Math.min(1, c[1] + c[4]),
				1 - Math.min(1, c[2] + c[4]),
				1 - Math.min(1, c[3] + c[4]) ];
		break;
	case 'CMYK':
		if (c[0] === 'G')
			return [ 'CMYK', 0, 0, 0, 1 - c[1] ];
		if (c[0] === 'RGB')
			return [ 'CMYK', 1 - c[1], 1 - c[2], 1 - c[3], 0 ];
		break;
	}
	return c;
}

color.equal = function (a, b) {
	var i, n
	if (a[0] === 'G')
		a = color.convert(a, b[0]);
	else
		b = color.convert(b, a[0]);
	if (a[0] !== b[0])
		return false;
	switch (a[0]) {
	case 'G': n = 1; break;
	case 'RGB': n = 3; break;
	case 'CMYK': n = 4; break;
	default: n = 0; break;
	}
	for (i = 1; i <= n; ++i)
		if (a[i] !== b[i])
			return false;
	return true;
}

var font = {
	Cour: 'Courier',
	CourB: 'Courier-Bold',
	CourBI: 'Courier-BoldOblique',
	CourI: 'Courier-Oblique',
	Helv: 'Helvetica',
	HelvB: 'Helvetica-Bold',
	HelvBI: 'Helvetica-BoldOblique',
	HelvI: 'Helvetica-Oblique',
	Symbol: 'Symbol',
	Times: 'Times-Roman',
	TimesB: 'Times-Bold',
	TimesBI: 'Times-BoldItalic',
	TimesI: 'Times-Italic',
	ZapfD: 'ZapfDingbats',
};

var highlight = {
	i: 'invert',
	n: 'none',
	o: 'outline',
	p: 'push',
};

var position = {
	textOnly: 0,
	iconOnly: 1,
	iconTextV: 2,
	textIconV: 3,
	iconTextH: 4,
	textIconH: 5,
	overlay: 6,
};

var scaleHow = {
	proportional: 0,
	anamorphic: 1,
};

var scaleWhen = {
	always: 0,
	never: 1,
	tooBig: 2,
	tooSmall: 3,
};

var style = {
	ch: 'check',
	ci: 'circle',
	cr: 'cross',
	di: 'diamond',
	sq: 'square',
	st: 'star',
};

var zoomtype = {
	fitH: 'FitHeight',
	fitP: 'FitPage',
	fitV: 'FitVisibleWidth',
	fitW: 'FitWidth',
	none: 'NoVary',
	pref: 'Preferred',
	refW: 'ReflowWidth',
};

util.scand = function (fmt, input) {
	// This seems to match Acrobat's parsing behavior
	return AFParseDateEx(input, fmt);
}

util.printd = function (fmt, date) {
	var monthName = [
		'January',
		'February',
		'March',
		'April',
		'May',
		'June',
		'July',
		'August',
		'September',
		'October',
		'November',
		'December'
	];
	var dayName = [
		'Sunday',
		'Monday',
		'Tuesday',
		'Wednesday',
		'Thursday',
		'Friday',
		'Saturday'
	];
	if (fmt === 0)
		fmt = 'D:yyyymmddHHMMss';
	else if (fmt === 1)
		fmt = 'yyyy.mm.dd HH:MM:ss';
	else if (fmt === 2)
		fmt = 'm/d/yy h:MM:ss tt';
	if (!date)
		date = new Date();
	else if (!(date instanceof Date))
		date = new Date(date);
	var tokens = fmt.match(/(\\.|m+|d+|y+|H+|h+|M+|s+|t+|[^\\mdyHhMst]*)/g);
	var out = '';
	for (var i = 0; i < tokens.length; ++i) {
		var token = tokens[i];
		switch (token) {
		case 'mmmm': out += monthName[date.getMonth()]; break;
		case 'mmm': out += monthName[date.getMonth()].substring(0, 3); break;
		case 'mm': out += util.printf('%02d', date.getMonth()+1); break;
		case 'm': out += date.getMonth()+1; break;
		case 'dddd': out += dayName[date.getDay()]; break;
		case 'ddd': out += dayName[date.getDay()].substring(0, 3); break;
		case 'dd': out += util.printf('%02d', date.getDate()); break;
		case 'd': out += date.getDate(); break;
		case 'yyyy': out += date.getFullYear(); break;
		case 'yy': out += date.getFullYear() % 100; break;
		case 'HH': out += util.printf('%02d', date.getHours()); break;
		case 'H': out += date.getHours(); break;
		case 'hh': out += util.printf('%02d', (date.getHours()+11)%12+1); break;
		case 'h': out += (date.getHours() + 11) % 12 + 1; break;
		case 'MM': out += util.printf('%02d', date.getMinutes()); break;
		case 'M': out += date.getMinutes(); break;
		case 'ss': out += util.printf('%02d', date.getSeconds()); break;
		case 's': out += date.getSeconds(); break;
		case 'tt': out += date.getHours() < 12 ? 'am' : 'pm'; break;
		case 't': out += date.getHours() < 12 ? 'a' : 'p'; break;
		default: out += (token[0] == '\\') ? token[1] : token; break;
		}
	}
	return out;
}

util.printx = function (fmt, val) {
	function toUpper(str) { return str.toUpperCase(); }
	function toLower(str) { return str.toLowerCase(); }
	function toSame(str) { return str; }
	var convertCase = toSame;
	var res = '';
	var i, m;
	var n = fmt ? fmt.length : 0;
	for (i = 0; i < n; ++i) {
		switch (fmt.charAt(i)) {
		case '\\':
			if (++i < n)
				res += fmt.charAt(i);
			break;
		case 'X':
			m = val.match(/\w/);
			if (m) {
				res += convertCase(m[0]);
				val = val.replace(/^\W*\w/, '');
			}
			break;
		case 'A':
			m = val.match(/[A-Za-z]/);
			if (m) {
				res += convertCase(m[0]);
				val = val.replace(/^[^A-Za-z]*[A-Za-z]/, '');
			}
			break;
		case '9':
			m = val.match(/\d/);
			if (m) {
				res += m[0];
				val = val.replace(/^\D*\d/, '');
			}
			break;
		case '*':
			res += convertCase(val);
			val = '';
			break;
		case '?':
			if (val !== '') {
				res += convertCase(val.charAt(0));
				val = val.substring(1);
			}
			break;
		case '=':
			convertCase = toSame;
			break;
		case '>':
			convertCase = toUpper;
			break;
		case '<':
			convertCase = toLower;
			break;
		default:
			res += convertCase(fmt.charAt(i));
			break;
		}
	}
	return res;
}

function AFMergeChange(event) {
	var prefix, postfix;
	var value = event.value;
	if (event.willCommit)
		return value;
	if (event.selStart >= 0)
		prefix = value.substring(0, event.selStart);
	else
		prefix = '';
	if (event.selEnd >= 0 && event.selEnd <= value.length)
		postfix = value.substring(event.selEnd, value.length);
	else
		postfix = '';
	return prefix + event.change + postfix;
}

function AFExtractNums(string) {
	if (string.charAt(0) == '.' || string.charAt(0) == ',')
		string = '0' + string;
	return string.match(/\d+/g);
}

function AFMakeNumber(string) {
	if (typeof string == 'number')
		return string;
	if (typeof string != 'string')
		return null;
	var nums = AFExtractNums(string);
	if (!nums)
		return null;
	var result = nums.join('.');
	if (string.indexOf('-.') >= 0)
		result = '0.' + result;
	if (string.indexOf('-') >= 0)
		return -result;
	return +result;
}

function AFExtractTime(string) {
	var pattern = /\d\d?:\d\d?(:\d\d?)?\s*(am|pm)?/i;
	var match = pattern.exec(string);
	if (match) {
		var prefix = string.substring(0, match.index);
		var suffix = string.substring(match.index + match[0].length);
		return [ prefix + suffix, match[0] ];
	}
	return null;
}

function AFParseDateOrder(fmt) {
	var order = '';
	fmt += 'mdy'; // Default order if any parts are missing.
	for (var i = 0; i < fmt.length; i++) {
		var c = fmt.charAt(i);
		if ((c == 'y' || c == 'm' || c == 'd') && order.indexOf(c) < 0)
			order += c;
	}
	return order;
}

function AFMatchMonth(date) {
	var names = ['jan','feb','mar','apr','may','jun','jul','aug','sep','oct','nov','dec'];
	var month = date.match(/Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec/i);
	if (month)
		return names.indexOf(month[0].toLowerCase()) + 1;
	return null;
}

function AFParseTime(string, date) {
	if (!date)
		date = new Date();
	if (!string)
		return date;
	var nums = AFExtractNums(string);
	if (!nums || nums.length < 2 || nums.length > 3)
		return null;
	var hour = nums[0];
	var min = nums[1];
	var sec = (nums.length == 3) ? nums[2] : 0;
	if (hour < 12 && (/pm/i).test(string))
		hour += 12;
	if (hour >= 12 && (/am/i).test(string))
		hour -= 12;
	date.setHours(hour, min, sec);
	if (date.getHours() != hour || date.getMinutes() != min || date.getSeconds() != sec)
		return null;
	return date;
}

function AFMakeDate(out, year, month, date, time)
{
	if (year < 50)
		year += 2000;
	if (year < 100)
		year += 1900;
	out.setFullYear(year, month, date);
	if (out.getFullYear() != year || out.getMonth() != month || out.getDate() != date)
		return null;
	if (time)
		out = AFParseTime(time, out);
	else
		out.setHours(0, 0, 0);
	return out;
}

function AFParseDateEx(string, fmt) {
	var out = new Date();
	var year = out.getFullYear();
	var month;
	var date;
	var i;

	out.setHours(12, 0, 0);

	var order = AFParseDateOrder(fmt);

	var time = AFExtractTime(string);
	if (time) {
		string = time[0];
		time = time[1];
	}

	var nums = AFExtractNums(string);
	if (!nums)
		return null;

	if (nums.length == 3) {
		year = nums[order.indexOf('y')];
		month = nums[order.indexOf('m')];
		date = nums[order.indexOf('d')];
		return AFMakeDate(out, year, month-1, date, time);
	}

	month = AFMatchMonth(string);

	if (nums.length == 2) {
		// We have a textual month.
		if (month) {
			if (order.indexOf('y') < order.indexOf('d')) {
				year = nums[0];
				date = nums[1];
			} else {
				year = nums[1];
				date = nums[0];
			}
		}

		// Year before date: set year and month.
		else if (order.indexOf('y') < order.indexOf('d')) {
			if (order.indexOf('y') < order.indexOf('m')) {
				year = nums[0];
				month = nums[1];
			} else {
				year = nums[1];
				month = nums[0];
			}
		}

		// Date before year: set date and month.
		else {
			if (order.indexOf('d') < order.indexOf('m')) {
				date = nums[0];
				month = nums[1];
			} else {
				date = nums[1];
				month = nums[0];
			}
		}

		return AFMakeDate(out, year, month-1, date, time);
	}

	if (nums.length == 1) {
		if (month) {
			if (order.indexOf('y') < order.indexOf('d')) {
				year = nums[0];
				date = 1;
			} else {
				date = nums[0];
			}
			return AFMakeDate(out, year, month-1, date, time);
		}

		// Only one number: must match format exactly!
		if (string.length == fmt.length) {
			year = month = date = '';
			for (i = 0; i < fmt.length; ++i) {
				switch (fmt.charAt(i)) {
				case '\\': ++i; break;
				case 'y': year += string.charAt(i); break;
				case 'm': month += string.charAt(i); break;
				case 'd': date += string.charAt(i); break;
				}
			}
			return AFMakeDate(out, year, month-1, date, time);
		}
	}

	return null;
}

var AFDate_oldFormats = [
	'm/d',
	'm/d/yy',
	'mm/dd/yy',
	'mm/yy',
	'd-mmm',
	'd-mmm-yy',
	'dd-mm-yy',
	'yy-mm-dd',
	'mmm-yy',
	'mmmm-yy',
	'mmm d, yyyy',
	'mmmm d, yyyy',
	'm/d/yy h:MM tt',
	'm/d/yy HH:MM'
];

function AFDate_KeystrokeEx(fmt) {
	if (event.willCommit && !AFParseDateEx(event.value, fmt)) {
		app.alert('The date/time entered ('+event.value+') does not match the format ('+fmt+') of the field [ '+event.target.name+' ]');
		event.rc = false;
	}
}

function AFDate_Keystroke(index) {
	AFDate_KeystrokeEx(AFDate_oldFormats[index]);
}

function AFDate_FormatEx(fmt) {
	var d = AFParseDateEx(event.value, fmt);
	event.value = d ? util.printd(fmt, d) : '';
}

function AFDate_Format(index) {
	AFDate_FormatEx(AFDate_oldFormats[index]);
}

function AFTime_Keystroke(index) {
	if (event.willCommit && !AFParseTime(event.value, null)) {
		app.alert('The value entered ('+event.value+') does not match the format of the field [ '+event.target.name+' ]');
		event.rc = false;
	}
}

function AFTime_FormatEx(fmt) {
	var d = AFParseTime(event.value, null);
	event.value = d ? util.printd(fmt, d) : '';
}

function AFTime_Format(index) {
	var formats = [ 'HH:MM', 'h:MM tt', 'HH:MM:ss', 'h:MM:ss tt' ];
	AFTime_FormatEx(formats[index]);
}

function AFSpecial_KeystrokeEx(fmt) {
	function toUpper(str) { return str.toUpperCase(); }
	function toLower(str) { return str.toLowerCase(); }
	function toSame(str) { return str; }
	var convertCase = toSame;
	var val = event.value;
	var res = '';
	var i = 0;
	var m;
	var length = fmt ? fmt.length : 0;
	while (i < length) {
		switch (fmt.charAt(i)) {
		case '\\':
			i++;
			if (i >= length)
				break;
			res += fmt.charAt(i);
			if (val && val.charAt(0) === fmt.charAt(i))
				val = val.substring(1);
			break;

		case 'X':
			m = val.match(/^\w/);
			if (!m) {
				event.rc = false;
				break;
			}
			res += convertCase(m[0]);
			val = val.substring(1);
			break;

		case 'A':
			m = val.match(/^[A-Za-z]/);
			if (!m) {
				event.rc = false;
				break;
			}
			res += convertCase(m[0]);
			val = val.substring(1);
			break;

		case '9':
			m = val.match(/^\d/);
			if (!m) {
				event.rc = false;
				break;
			}
			res += m[0];
			val = val.substring(1);
			break;

		case '*':
			res += convertCase(val);
			val = '';
			break;

		case '?':
			if (val === '') {
				event.rc = false;
				break;
			}
			res += convertCase(val.charAt(0));
			val = val.substring(1);
			break;

		case '=':
			convertCase = toSame;
			break;
		case '>':
			convertCase = toUpper;
			break;
		case '<':
			convertCase = toLower;
			break;

		default:
			res += fmt.charAt(i);
			if (val && val.charAt(0) === fmt.charAt(i))
				val = val.substring(1);
			break;
		}

		i++;
	}

	//  If there are characters left over in the value, it's not a match.
	if (val.length > 0)
		event.rc = false;

	if (event.rc)
		event.value = res;
	else if (event.willCommit)
		app.alert('The value entered ('+event.value+') does not match the format of the field [ '+event.target.name+' ] should be '+fmt);
}

function AFSpecial_Keystroke(index) {
	if (event.willCommit) {
		switch (index) {
		case 0:
			if (!event.value.match(/^\d{5}$/))
				event.rc = false;
			break;
		case 1:
			if (!event.value.match(/^\d{5}[-. ]?\d{4}$/))
				event.rc = false;
			break;
		case 2:
			if (!event.value.match(/^((\(\d{3}\)|\d{3})[-. ]?)?\d{3}[-. ]?\d{4}$/))
				event.rc = false;
			break;
		case 3:
			if (!event.value.match(/^\d{3}[-. ]?\d{2}[-. ]?\d{4}$/))
				event.rc = false;
			break;
		}
		if (!event.rc)
			app.alert('The value entered ('+event.value+') does not match the format of the field [ '+event.target.name+' ]');
	}
}

function AFSpecial_Format(index) {
	var res;
	switch (index) {
	case 0:
		res = util.printx('99999', event.value);
		break;
	case 1:
		res = util.printx('99999-9999', event.value);
		break;
	case 2:
		res = util.printx('9999999999', event.value);
		res = util.printx(res.length >= 10 ? '(999) 999-9999' : '999-9999', event.value);
		break;
	case 3:
		res = util.printx('999-99-9999', event.value);
		break;
	}
	event.value = res ? res : '';
}

function AFNumber_Keystroke(nDec, sepStyle, negStyle, currStyle, strCurrency, bCurrencyPrepend) {
	if (sepStyle & 2) {
		if (!event.value.match(/^[+-]?\d*[,.]?\d*$/))
			event.rc = false;
	} else {
		if (!event.value.match(/^[+-]?\d*\.?\d*$/))
			event.rc = false;
	}
	if (event.willCommit) {
		if (!event.value.match(/\d/))
			event.rc = false;
		if (!event.rc)
			app.alert('The value entered ('+event.value+') does not match the format of the field [ '+event.target.name+' ]');
	}
}

function AFNumber_Format(nDec, sepStyle, negStyle, currStyle, strCurrency, bCurrencyPrepend) {
	var value = AFMakeNumber(event.value);
	var fmt = '%,' + sepStyle + '.' + nDec + 'f';
	if (value == null) {
		event.value = '';
		return;
	}
	if (bCurrencyPrepend)
		fmt = strCurrency + fmt;
	else
		fmt = fmt + strCurrency;
	if (value < 0) {
		/* negStyle: 0=MinusBlack, 1=Red, 2=ParensBlack, 3=ParensRed */
		value = Math.abs(value);
		if (negStyle == 2 || negStyle == 3)
			fmt = '(' + fmt + ')';
		else if (negStyle == 0)
			fmt = '-' + fmt;
		if (negStyle == 1 || negStyle == 3)
			event.target.textColor = color.red;
		else
			event.target.textColor = color.black;
	} else {
		event.target.textColor = color.black;
	}
	event.value = util.printf(fmt, value);
}

function AFPercent_Keystroke(nDec, sepStyle) {
	AFNumber_Keystroke(nDec, sepStyle, 0, 0, '', true);
}

function AFPercent_Format(nDec, sepStyle) {
	var val = AFMakeNumber(event.value);
	if (val == null) {
		event.value = '';
		return;
	}
	event.value = (val * 100) + '';
	AFNumber_Format(nDec, sepStyle, 0, 0, '%', false);
}

function AFSimple_Calculate(op, list) {
	var i, res;

	switch (op) {
	case 'SUM': res = 0; break;
	case 'PRD': res = 1; break;
	case 'AVG': res = 0; break;
	}

	if (typeof list === 'string')
		list = list.split(/ *, */);

	for (i = 0; i < list.length; i++) {
		var field = this.getField(list[i]);
		var value = Number(field.value);
		switch (op) {
		case 'SUM': res += value; break;
		case 'PRD': res *= value; break;
		case 'AVG': res += value; break;
		case 'MIN': if (i === 0 || value < res) res = value; break;
		case 'MAX': if (i === 0 || value > res) res = value; break;
		}
	}

	if (op === 'AVG')
		res /= list.length;

	event.value = res;
}

function AFRange_Validate(lowerCheck, lowerLimit, upperCheck, upperLimit) {
	if (upperCheck && event.value > upperLimit)
		event.rc = false;
	if (lowerCheck && event.value < lowerLimit)
		event.rc = false;
	if (!event.rc) {
		if (lowerCheck && upperCheck)
			app.alert(util.printf('The entered value ('+event.value+') must be greater than or equal to %s and less than or equal to %s', lowerLimit, upperLimit));
		else if (lowerCheck)
			app.alert(util.printf('The entered value ('+event.value+') must be greater than or equal to %s', lowerLimit));
		else
			app.alert(util.printf('The entered value ('+event.value+') must be less than or equal to %s', upperLimit));
	}
}

/* Compatibility ECMAScript functions */
String.prototype.substr = function (start, length) {
	if (start < 0)
		start = this.length + start;
	if (length === undefined)
		return this.substring(start, this.length);
	return this.substring(start, start + length);
}
Date.prototype.getYear = Date.prototype.getFullYear;
Date.prototype.setYear = Date.prototype.setFullYear;
Date.prototype.toGMTString = Date.prototype.toUTCString;

console.clear = function() { console.println('--- clear console ---\n'); };
console.show = function(){};
console.hide = function(){};

app.plugIns = [];
app.viewerType = 'Reader';
app.language = 'ENU';
app.viewerVersion = NaN;
app.execDialog = function () { return 'cancel'; }
