from __future__ import print_function

import codecs
import inspect
import io
import os
import shutil
import subprocess
import sys
import time
import traceback


def place( frame_record):
    '''
    Useful debugging function - returns representation of source position of
    caller.
    '''
    filename    = frame_record.filename
    line        = frame_record.lineno
    function    = frame_record.function
    ret = os.path.split( filename)[1] + ':' + str( line) + ':' + function + ':'
    if 0:   # lgtm [py/unreachable-statement]
        tid = str( threading.currentThread())
        ret = '[' + tid + '] ' + ret
    return ret


def expand_nv( text, caller):
    '''
    Returns <text> with special handling of {<expression>} items.

    text:
        String containing {<expression>} items.
    caller:
        If an int, the number of frames to step up when looking for file:line
        information or evaluating expressions.

        Otherwise should be a frame record as returned by inspect.stack()[].

    <expression> is evaluated in <caller>'s context using eval(), and expanded
    to <expression> or <expression>=<value>.

    If <expression> ends with '=', this character is removed and we prefix the
    result with <expression>=.

    E.g.:
        x = 45
        y = 'hello'
        expand_nv( 'foo {x} {y=}')
    returns:
        foo 45 y=hello

    <expression> can also use ':' and '!' to control formatting, like
    str.format().
    '''
    if isinstance( caller, int):
        frame_record = inspect.stack()[ caller]
    else:
        frame_record = caller
    frame = frame_record.frame
    try:
        def get_items():
            '''
            Yields (pre, item), where <item> is contents of next {...} or None,
            and <pre> is preceding text.
            '''
            pos = 0
            pre = ''
            while 1:
                if pos == len( text):
                    yield pre, None
                    break
                rest = text[ pos:]
                if rest.startswith( '{{') or rest.startswith( '}}'):
                    pre += rest[0]
                    pos += 2
                elif text[ pos] == '{':
                    close = text.find( '}', pos)
                    if close < 0:
                        raise Exception( 'After "{" at offset %s, cannot find closing "}". text is: %r' % (
                                pos, text))
                    yield pre, text[ pos+1 : close]
                    pre = ''
                    pos = close + 1
                else:
                    pre += text[ pos]
                    pos += 1

        ret = ''
        for pre, item in get_items():
            ret += pre
            nv = False
            if item:
                if item.endswith( '='):
                    nv = True
                    item = item[:-1]
                expression, tail = split_first_of( item, '!:')
                try:
                    value = eval( expression, frame.f_globals, frame.f_locals)
                    value_text = ('{0%s}' % tail).format( value)
                except Exception as e:
                    value_text = '{??Failed to evaluate %r in context %s:%s because: %s??}' % (
                            expression,
                            frame_record.filename,
                            frame_record.lineno,
                            e,
                            )
                if nv:
                    ret += '%s=' % expression
                ret += value_text

        return ret

    finally:
        del frame   # lgtm [py/unnecessary-delete]


class LogPrefixTime:
    def __init__( self, date=False, time_=True, elapsed=False):
        self.date = date
        self.time = time_
        self.elapsed = elapsed
        self.t0 = time.time()
    def __call__( self):
        ret = ''
        if self.date:
            ret += time.strftime( ' %F')
        if self.time:
            ret += time.strftime( ' %T')
        if self.elapsed:
            ret += ' (+%s)' % time_duration( time.time() - self.t0, s_format='%.1f')
        if ret:
            ret = ret.strip() + ': '
        return ret

class LogPrefixFileLine:
    def __call__( self, caller):
        if isinstance( caller, int):
            caller = inspect.stack()[ caller]
        return place( caller) + ' '

class LogPrefixScopes:
    '''
    Internal use only.
    '''
    def __init__( self):
        self.items = []
    def __call__( self):
        ret = ''
        for item in self.items:
            if callable( item):
                item = item()
            ret += item
        return ret


class LogPrefixScope:
    '''
    Can be used to insert scoped prefix to log output.
    '''
    def __init__( self, prefix):
        self.prefix = prefix
    def __enter__( self):
        g_log_prefixe_scopes.items.append( self.prefix)
    def __exit__( self, exc_type, exc_value, traceback):
        global g_log_prefix
        g_log_prefixe_scopes.items.pop()


g_log_delta = 0

class LogDeltaScope:
    '''
    Can be used to temporarily change verbose level of logging.

    E.g to temporarily increase logging:

        with jlib.LogDeltaScope(-1):
            ...
    '''
    def __init__( self, delta):
        self.delta = delta
        global g_log_delta
        g_log_delta += self.delta
    def __enter__( self):
        pass
    def __exit__( self, exc_type, exc_value, traceback):
        global g_log_delta
        g_log_delta -= self.delta

# Special item that can be inserted into <g_log_prefixes> to enable
# temporary addition of text into log prefixes.
#
g_log_prefixe_scopes = LogPrefixScopes()

# List of items that form prefix for all output from log().
#
g_log_prefixes = []


def log_text( text=None, caller=1, nv=True):
    '''
    Returns log text, prepending all lines with text from g_log_prefixes.

    text:
        The text to output. Each line is prepended with prefix text.
    caller:
        If an int, the number of frames to step up when looking for file:line
        information or evaluating expressions.

        Otherwise should be a frame record as returned by inspect.stack()[].
    nv:
        If true, we expand {...} in <text> using expand_nv().
    '''
    if isinstance( caller, int):
        caller += 1
    prefix = ''
    prefix += g_log_prefixe_scopes()
    for p in g_log_prefixes:
        if callable( p):
            if isinstance( p, LogPrefixFileLine):
                p = p(caller)
            else:
                p = p()
        prefix += p

    if text is None:
        return prefix

    if nv:
        text = expand_nv( text, caller)

    if text.endswith( '\n'):
        text = text[:-1]
    lines = text.split( '\n')

    text = ''
    for line in lines:
        text += prefix + line + '\n'
    return text



s_log_levels_cache = dict()
s_log_levels_items = []

def log_levels_find( caller):
    if not s_log_levels_items:
        return 0

    tb = traceback.extract_stack( None, 1+caller)
    if len(tb) == 0:
        return 0
    filename, line, function, text = tb[0]

    key = function, filename, line,
    delta = s_log_levels_cache.get( key)

    if delta is None:
        # Calculate and populate cache.
        delta = 0
        for item_function, item_filename, item_delta in s_log_levels_items:
            if item_function and not function.startswith( item_function):
                continue
            if item_filename and not filename.startswith( item_filename):
                continue
            delta = item_delta
            break

        s_log_levels_cache[ key] = delta

    return delta


def log_levels_add( delta, filename_prefix, function_prefix):
    '''
    log() calls from locations with filenames starting with <filename_prefix>
    and/or function names starting with <function_prefix> will have <delta>
    added to their level.

    Use -ve delta to increase verbosity from particular filename or function
    prefixes.
    '''
    log( 'adding level: {filename_prefix=!r} {function_prefix=!r}')

    # Sort in reverse order so that long functions and filename specs come
    # first.
    #
    s_log_levels_items.append( (function_prefix, filename_prefix, delta))
    s_log_levels_items.sort( reverse=True)


def log( text, level=0, caller=1, nv=True, out=None):
    '''
    Writes log text, with special handling of {<expression>} items in <text>
    similar to python3's f-strings.

    text:
        The text to output.
    caller:
        How many frames to step up to get caller's context when evaluating
        file:line information and/or expressions. Or frame record as returned
        by inspect.stack()[].
    nv:
        If true, we expand {...} in <text> using expand_nv().
    out:
        Where to send output. If None we use sys.stdout.

    <expression> is evaluated in our caller's context (<n> stack frames up)
    using eval(), and expanded to <expression> or <expression>=<value>.

    If <expression> ends with '=', this character is removed and we prefix the
    result with <expression>=.

    E.g.:
        x = 45
        y = 'hello'
        expand_nv( 'foo {x} {y=}')
    returns:
        foo 45 y=hello

    <expression> can also use ':' and '!' to control formatting, like
    str.format().
    '''
    if out is None:
        out = sys.stdout
    level += g_log_delta
    if isinstance( caller, int):
        caller += 1
    level += log_levels_find( caller)
    if level <= 0:
        text = log_text( text, caller, nv=nv)
        out.write( text)
        out.flush()


def log0( text, caller=1, nv=True, out=None):
    '''
    Most verbose log. Same as log().
    '''
    log( text, level=0, caller=caller+1, nv=nv, out=out)

def log1( text, caller=1, nv=True, out=None):
    log( text, level=1, caller=caller+1, nv=nv, out=out)

def log2( text, caller=1, nv=True, out=None):
    log( text, level=2, caller=caller+1, nv=nv, out=out)

def log3( text, caller=1, nv=True, out=None):
    log( text, level=3, caller=caller+1, nv=nv, out=out)

def log4( text, caller=1, nv=True, out=None):
    log( text, level=4, caller=caller+1, nv=nv, out=out)

def log5( text, caller=1, nv=True, out=None):
    '''
    Least verbose log.
    '''
    log( text, level=5, caller=caller+1, nv=nv, out=out)

def logx( text, caller=1, nv=True, out=None):
    '''
    Does nothing, useful when commenting out a log().
    '''
    pass


def log_levels_add_env( name='JLIB_log_levels'):
    '''
    Added log levels encoded in an environmental variable.
    '''
    t = os.environ.get( name)
    if t:
        for ffll in t.split( ','):
            ffl, delta = ffll.split( '=', 1)
            delta = int( delta)
            ffl = ffl.split( ':')
            if 0:   # lgtm [py/unreachable-statement]
                pass
            elif len( ffl) == 1:
                filename = ffl
                function = None
            elif len( ffl) == 2:
                filename, function = ffl
            else:
                assert 0
            log_levels_add( delta, filename, function)


def strpbrk( text, substrings):
    '''
    Finds first occurrence of any item in <substrings> in <text>.

    Returns (pos, substring) or (len(text), None) if not found.
    '''
    ret_pos = len( text)
    ret_substring = None
    for substring in substrings:
        pos = text.find( substring)
        if pos >= 0 and pos < ret_pos:
            ret_pos = pos
            ret_substring = substring
    return ret_pos, ret_substring


def split_first_of( text, substrings):
    '''
    Returns (pre, post), where <pre> doesn't contain any item in <substrings>
    and <post> is empty or starts with an item in <substrings>.
    '''
    pos, _ = strpbrk( text, substrings)
    return text[ :pos], text[ pos+1:]



log_levels_add_env()


def force_line_buffering():
    '''
    Ensure sys.stdout and sys.stderr are line-buffered. E.g. makes things work
    better if output is piped to a file via 'tee'.

    Returns original out,err streams.
    '''
    stdout0 = sys.stdout
    stderr0 = sys.stderr
    sys.stdout = os.fdopen( os.dup( sys.stdout.fileno()), 'w', 1)
    sys.stderr = os.fdopen( os.dup( sys.stderr.fileno()), 'w', 1)
    return stdout0, stderr0


def exception_info( exception=None, limit=None, out=None, prefix='', oneline=False):
    '''
    General replacement for traceback.* functions that print/return information
    about exceptions. This function provides a simple way of getting the
    functionality provided by these traceback functions:

        traceback.format_exc()
        traceback.format_exception()
        traceback.print_exc()
        traceback.print_exception()

    Returns:
        A string containing description of specified exception and backtrace.

    Inclusion of outer frames:
        We improve upon traceback.* in that we also include stack frames above
        the point at which an exception was caught - frames from the top-level
        <module> or thread creation fn to the try..catch block, which makes
        backtraces much more useful.

        Google 'sys.exc_info backtrace incomplete' for more details.

        We deliberately leave a slightly curious pair of items in the backtrace
        - the point in the try: block that ended up raising an exception, and
        the point in the associated except: block from which we were called.

        For clarity, we insert an empty frame in-between these two items, so
        that one can easily distinguish the two parts of the backtrace.

        So the backtrace looks like this:

            root (e.g. <module> or /usr/lib/python2.7/threading.py:778:__bootstrap():
            ...
            file:line in the except: block where the exception was caught.
            ::(): marker
            file:line in the try: block.
            ...
            file:line where the exception was raised.

        The items after the ::(): marker are the usual items that traceback.*
        shows for an exception.

    Also the backtraces that are generated are more concise than those provided
    by traceback.* - just one line per frame instead of two - and filenames are
    output relative to the current directory if applicatble. And one can easily
    prefix all lines with a specified string, e.g. to indent the text.

    Returns a string containing backtrace and exception information, and sends
    returned string to <out> if specified.

    exception:
        None, or a (type, value, traceback) tuple, e.g. from sys.exc_info(). If
        None, we call sys.exc_info() and use its return value.
    limit:
        None or maximum number of stackframes to output.
    out:
        None or callable taking single <text> parameter or object with a
        'write' member that takes a single <text> parameter.
    prefix:
        Used to prefix all lines of text.
    '''
    if exception is None:
        exception = sys.exc_info()
    etype, value, tb = exception

    if sys.version_info[0] == 2:
        out2 = io.BytesIO()
    else:
        out2 = io.StringIO()
    try:

        frames = []

        # Get frames above point at which exception was caught - frames
        # starting at top-level <module> or thread creation fn, and ending
        # at the point in the catch: block from which we were called.
        #
        # These frames are not included explicitly in sys.exc_info()[2] and are
        # also omitted by traceback.* functions, which makes for incomplete
        # backtraces that miss much useful information.
        #
        for f in reversed(inspect.getouterframes(tb.tb_frame)):
            ff = f[1], f[2], f[3], f[4][0].strip()
            frames.append(ff)

        # It's useful to see boundary between upper and lower frames.
        frames.append( None)

        # Append frames from point in the try: block that caused the exception
        # to be raised, to the point at which the exception was thrown.
        #
        # [One can get similar information using traceback.extract_tb(tb):
        #   for f in traceback.extract_tb(tb):
        #       frames.append(f)
        # ]
        for f in inspect.getinnerframes(tb):
            ff = f[1], f[2], f[3], f[4][0].strip()
            frames.append(ff)

        cwd = os.getcwd() + os.sep
        if oneline:
            if etype and value:
                # The 'exception_text' variable below will usually be assigned
                # something like '<ExceptionType>: <ExceptionValue>', unless
                # there was no explanatory text provided (e.g. "raise Exception()").
                # In this case, str(value) will evaluate to ''.
                exception_text = traceback.format_exception_only(etype, value)[0].strip()
                filename, line, fnname, text = frames[-1]
                if filename.startswith(cwd):
                    filename = filename[len(cwd):]
                if not str(value):
                    # The exception doesn't have any useful explanatory text
                    # (for example, maybe it was raised by an expression like
                    # "assert <expression>" without a subsequent comma).  In
                    # the absence of anything more helpful, print the code that
                    # raised the exception.
                    exception_text += ' (%s)' % text
                line = '%s%s at %s:%s:%s()' % (prefix, exception_text, filename, line, fnname)
                out2.write(line)
        else:
            out2.write( '%sBacktrace:\n' % prefix)
            for frame in frames:
                if frame is None:
                    out2.write( '%s    ^except raise:\n' % prefix)
                    continue
                filename, line, fnname, text = frame
                if filename.startswith( cwd):
                    filename = filename[ len(cwd):]
                if filename.startswith( './'):
                    filename = filename[ 2:]
                out2.write( '%s    %s:%s:%s(): %s\n' % (
                        prefix, filename, line, fnname, text))

            if etype and value:
                out2.write( '%sException:\n' % prefix)
                lines = traceback.format_exception_only( etype, value)
                for line in lines:
                    out2.write( '%s    %s' % ( prefix, line))

        text = out2.getvalue()

        # Write text to <out> if specified.
        out = getattr( out, 'write', out)
        if callable( out):
            out( text)
        return text

    finally:
        # clear things to avoid cycles.
        del exception
        del etype
        del value
        del tb
        del frames


def number_sep( s):
    '''
    Simple number formatter, adds commas in-between thousands. <s> can
    be a number or a string. Returns a string.
    '''
    if not isinstance( s, str):
        s = str( s)
    c = s.find( '.')
    if c==-1:   c = len(s)
    end = s.find('e')
    if end == -1:   end = s.find('E')
    if end == -1:   end = len(s)
    ret = ''
    for i in range( end):
        ret += s[i]
        if i<c-1 and (c-i-1)%3==0:
            ret += ','
        elif i>c and i<end-1 and (i-c)%3==0:
            ret += ','
    ret += s[end:]
    return ret

assert number_sep(1)=='1'
assert number_sep(12)=='12'
assert number_sep(123)=='123'
assert number_sep(1234)=='1,234'
assert number_sep(12345)=='12,345'
assert number_sep(123456)=='123,456'
assert number_sep(1234567)=='1,234,567'


class Stream:
    '''
    Base layering abstraction for streams - abstraction for things like
    sys.stdout to allow prefixing of all output, e.g. with a timestamp.
    '''
    def __init__( self, stream):
        self.stream = stream
    def write( self, text):
        self.stream.write( text)

class StreamPrefix:
    '''
    Prefixes output with a prefix, which can be a string or a callable that
    takes no parameters and return a string.
    '''
    def __init__( self, stream, prefix):
        self.stream = stream
        self.at_start = True
        if callable(prefix):
            self.prefix = prefix
        else:
            self.prefix = lambda : prefix

    def write( self, text):
        if self.at_start:
            text = self.prefix() + text
            self.at_start = False
        append_newline = False
        if text.endswith( '\n'):
            text = text[:-1]
            self.at_start = True
            append_newline = True
        text = text.replace( '\n', '\n%s' % self.prefix())
        if append_newline:
            text += '\n'
        self.stream.write( text)

    def flush( self):
        self.stream.flush()


def debug( text):
    if callable(text):
        text = text()
    print( text)

debug_periodic_t0 = [0]
def debug_periodic( text, override=0):
    interval = 10
    t = time.time()
    if t - debug_periodic_t0[0] > interval or override:
        debug_periodic_t0[0] = t
        debug(text)


def time_duration( seconds, verbose=False, s_format='%i'):
    '''
    Returns string expressing an interval.

    seconds:
        The duration in seconds
    verbose:
        If true, return like '4 days 1 hour 2 mins 23 secs', otherwise as
        '4d3h2m23s'.
    s_format:
        If specified, use as printf-style format string for seconds.
    '''
    x = abs(seconds)
    ret = ''
    i = 0
    for div, text in [
            ( 60, 'sec'),
            ( 60, 'min'),
            ( 24, 'hour'),
            ( None, 'day'),
            ]:
        force = ( x == 0 and i == 0)
        if div:
            remainder = x % div
            x = int( x/div)
        else:
            remainder = x
        if not verbose:
            text = text[0]
        if remainder or force:
            if verbose and remainder > 1:
                # plural.
                text += 's'
            if verbose:
                text = ' %s ' % text
            if i == 0:
                remainder = s_format % remainder
            ret = '%s%s%s' % ( remainder, text, ret)
        i += 1
    ret = ret.strip()
    if ret == '':
        ret = '0s'
    if seconds < 0:
        ret = '-%s' % ret
    return ret

assert time_duration( 303333) == '3d12h15m33s'
assert time_duration( 303333.33, s_format='%.1f') == '3d12h15m33.3s'
assert time_duration( 303333, verbose=True) == '3 days 12 hours 15 mins 33 secs'
assert time_duration( 303333.33, verbose=True, s_format='%.1f') == '3 days 12 hours 15 mins 33.3 secs'

assert time_duration( 0) == '0s'
assert time_duration( 0, verbose=True) == '0 sec'


def date_time( t=None):
    if t is None:
        t = time.time()
    return time.strftime( "%F-%T", time.gmtime( t))

def stream_prefix_time( stream):
    '''
    Returns StreamPrefix that prefixes lines with time and elapsed time.
    '''
    t_start = time.time()
    def prefix_time():
        return '%s (+%s): ' % (
                time.strftime( '%T'),
                time_duration( time.time() - t_start, s_format='0.1f'),
                )
    return StreamPrefix( stream, prefix_time)

def stdout_prefix_time():
    '''
    Changes sys.stdout to prefix time and elapsed time; returns original
    sys.stdout.
    '''
    ret = sys.stdout
    sys.stdout = stream_prefix_time( sys.stdout)
    return ret


def make_out_callable( out):
    '''
    Returns a stream-like object with a .write() method that writes to <out>.
    out:
        Where output is sent.
        If None, output is lost.
        Otherwise if an integer, we do: os.write( out, text)
        Otherwise if callable, we do: out( text)
        Otherwise we assume <out> is python stream or similar, and do: out.write(text)
    '''
    class Ret:
        def write( self, text):
            pass
        def flush( self):
            pass
    ret = Ret()
    if out == log:
        # A hack to avoid expanding '{...}' in text, if caller
        # does: jlib.system(..., out=jlib.log, ...).
        out = lambda text: log(text, nv=False)
    if out is None:
        ret.write = lambda text: None
    elif isinstance( out, int):
        ret.write = lambda text: os.write( out, text)
    elif callable( out):
        ret.write = out
    else:
        ret.write = lambda text: out.write( text)
    return ret


def system_raw(
        command,
        out=None,
        shell=True,
        encoding='latin_1',
        errors='strict',
        buffer_len=-1,
        executable=None,
        ):
    '''
    Runs command, writing output to <out> which can be an int fd, a python
    stream or a Stream object.

    Args:
        command:
            The command to run.
        out:
            Where output is sent.
            If None, child process inherits this process's stdout and stderr.
            If subprocess.DEVNULL, child process's output is lost.
            Otherwise we repeatedly read child process's output via a pipe and
            write to <out>:
                If <out> is an integer, we do: os.write( out, text)
                Otherwise if <out> is callable, we do: out( text)
                Otherwise we assume <out> is python stream or similar, and do:
                out.write(text)
        shell:
            Whether to run command inside a shell (see subprocess.Popen).
        encoding:
            Sepecify the encoding used to translate the command's output
            to characters.

            Note that if <encoding> is None and we are being run by python3,
            <out> will be passed bytes, not a string.

            Note that latin_1 will never raise a UnicodeDecodeError.
        errors:
            How to handle encoding errors; see docs for codecs module for
            details.
        buffer_len:
            The number of bytes we attempt to read at a time. If -1 we read
            output one line at a time.

    Returns:
        subprocess's <returncode>, i.e. -N means killed by signal N, otherwise
        the exit value (e.g. 12 if command terminated with exit(12)).
    '''
    stdin = None
    if out in (None, subprocess.DEVNULL):
        stdout = out
        stderr = out
    else:
        stdout = subprocess.PIPE
        stderr = subprocess.STDOUT
    child = subprocess.Popen(
            command,
            shell=shell,
            stdin=stdin,
            stdout=stdout,
            stderr=stderr,
            close_fds=True,
            executable=executable,
            #encoding=encoding - only python-3.6+.
            )

    child_out = child.stdout
    if encoding:
        child_out = codecs.getreader( encoding)( child_out, errors)

    if stdout == subprocess.PIPE:
        out = make_out_callable( out)
        if buffer_len == -1:
            for line in child_out:
                out.write( line)
        else:
            while 1:
                text = child_out.read( buffer_len)
                if not text:
                    break
                out.write( text)
    #decode( lambda : os.read( child_out.fileno(), 100), outfn, encoding)

    return child.wait()

if __name__ == '__main__':

    if os.getenv( 'jtest_py_system_raw_test') == '1':
        out = io.StringIO()
        system_raw(
                'jtest_py_system_raw_test=2 python jlib.py',
                sys.stdout,
                encoding='utf-8',
                #'latin_1',
                errors='replace',
                )
        print( repr( out.getvalue()))

    elif os.getenv( 'jtest_py_system_raw_test') == '2':
        for i in range(256):
            sys.stdout.write( chr(i))


def system(
        command,
        verbose=None,
        raise_errors=True,
        out=sys.stdout,
        prefix=None,
        rusage=False,
        shell=True,
        encoding=None,
        errors='replace',
        buffer_len=-1,
        executable=None,
        ):
    '''
    Runs a command like os.system() or subprocess.*, but with more flexibility.

    We give control over where the command's output is sent, whether to return
    the output and/or exit code, and whether to raise an exception if the
    command fails.

    We also support the use of /usr/bin/time to gather rusage information.

        command:
            The command to run.
        verbose:
            If true, we include information about the command that was run, and
            its result.

            If callable or something with a .write() method, information is
            sent to <verbose> itself. Otherwise it is sent to <out>.
        raise_errors:
            If true, we raise an exception if the command fails, otherwise we
            return the failing error code or zero.
        out:
            Where output is sent.
            If None, child process inherits this process's stdout and stderr.
            If subprocess.DEVNULL, child process's output is lost.
            Otherwise we repeatedly read child process's output via a pipe and
            write to <out>:
                If <out> is 'return' we store the output and include it in our
                return value or exception.
                Otherwise if <out> is an integer, we do: os.write( out, text)
                Otherwise if <out> is callable, we do: out( text)
                Otherwise we assume <out> is python stream or similar, and do:
                out.write(text)
        prefix:
            If not None, should be prefix string or callable used to prefix
            all output. [This is for convenience to avoid the need to do
            out=StreamPrefix(...).]
        rusage:
            If true, we run via /usr/bin/time and return rusage string
            containing information on execution. <raise_errors> and
            out='return' are ignored.
        shell:
            Passed to underlying subprocess.Popen() call.
        encoding:
            Sepecify the encoding used to translate the command's output
            to characters. Defaults to utf-8.
        errors:
            How to handle encoding errors; see docs for codecs module
            for details. Defaults to 'replace' so we never raise a
            UnicodeDecodeError.
        buffer_len:
            The number of bytes we attempt to read at a time. If -1 we read
            output one line at a time.

    Returns:
        If <rusage> is true, we return the rusage text.

        Else if raise_errors is true:
            If the command failed, we raise an exception.
            Else if <out> is 'return' we return the text output from the command.
            Else we return None

        Else if <out> is 'return', we return (e, text) where <e> is the
        command's exit code and <text> is the output from the command.

        Else we return <e>, the command's exit code.
    '''
    if encoding is None:
        if sys.version_info[0] == 2:
            # python-2 doesn't seem to implement 'replace' properly.
            encoding = None
            errors = None
        else:
            encoding = 'utf-8'
            errors = 'replace'

    out_original = out
    if out == 'return':
        # Store the output ourselves so we can return it.
        out = io.StringIO()

    out_raw = out in (None, subprocess.DEVNULL)

    if verbose:
        if callable( verbose) or getattr( verbose, 'write', None):
            pass
        elif out_raw:
            raise Exception( 'No out stream available for verbose')
        else:
            verbose = out
        verbose = make_out_callable( verbose)
        verbose.write('running: %s\n' % command)

    if prefix:
        if out_raw:
            raise Exception( 'No out stream available for prefix')
        out = StreamPrefix( make_out_callable( out), prefix)

    if rusage:
        command2 = ''
        command2 += '/usr/bin/time -o ubt-out -f "D=%D E=%D F=%F I=%I K=%K M=%M O=%O P=%P R=%r S=%S U=%U W=%W X=%X Z=%Z c=%c e=%e k=%k p=%p r=%r s=%s t=%t w=%w x=%x C=%C"'
        command2 += ' '
        command2 += command
        e = system_raw(
                command2,
                out,
                shell,
                encoding,
                errors,
                buffer_len=buffer_len,
                executable=executable,
                )
        if e:
            raise Exception('/usr/bin/time failed')
        with open('ubt-out') as f:
            rusage_text = f.read()
        #print 'have read rusage output: %r' % rusage_text
        if rusage_text.startswith( 'Command '):
            # Annoyingly, /usr/bin/time appears to write 'Command
            # exited with ...' or 'Command terminated by ...' to the
            # output file before the rusage info if command doesn't
            # exit 0.
            nl = rusage_text.find('\n')
            rusage_text = rusage_text[ nl+1:]
        return rusage_text
    else:
        e = system_raw(
                command,
                out,
                shell,
                encoding,
                errors,
                buffer_len=buffer_len,
                executable=executable,
                )

        if verbose:
            verbose.write('[returned e=%s]\n' % e)

        if raise_errors:
            if e:
                raise Exception( 'command failed: %s' % command)
            elif out_original == 'return':
                return out.getvalue()
            else:
                return

        if out_original == 'return':
            return e, out.getvalue()
        else:
            return e

def get_gitfiles( directory, submodules=False):
    '''
    Returns list of all files known to git in <directory>; <directory> must be
    somewhere within a git checkout.

    Returned names are all relative to <directory>.

    If <directory>.git exists we use git-ls-files and write list of files to
    <directory>/jtest-git-files.

    Otherwise we require that <directory>/jtest-git-files already exists.
    '''
    if os.path.isdir( '%s/.git' % directory):
        command = 'cd ' + directory + ' && git ls-files'
        if submodules:
            command += ' --recurse-submodules'
        command += ' > jtest-git-files'
        system( command, verbose=sys.stdout)

    with open( '%s/jtest-git-files' % directory, 'r') as f:
        text = f.read()
    ret = text.strip().split( '\n')
    return ret

def get_git_id_raw( directory):
    if not os.path.isdir( '%s/.git' % directory):
        return
    text = system(
            f'cd {directory} && (PAGER= git show --pretty=oneline|head -n 1 && git diff)',
            out='return',
            )
    return text

def get_git_id( directory, allow_none=False):
    '''
    Returns text where first line is '<git-sha> <commit summary>' and remaining
    lines contain output from 'git diff' in <directory>.

    directory:
        Root of git checkout.
    allow_none:
        If true, we return None if <directory> is not a git checkout and
        jtest-git-id file does not exist.
    '''
    filename = f'{directory}/jtest-git-id'
    text = get_git_id_raw( directory)
    if text:
        with open( filename, 'w') as f:
            f.write( text)
    elif os.path.isfile( filename):
        with open( filename) as f:
            text = f.read()
    else:
        if not allow_none:
            raise Exception( f'Not in git checkout, and no file called: {filename}.')
        text = None
    return text

class Args:
    '''
    Iterates over argv items.
    '''
    def __init__( self, argv):
        self.items = iter( argv)
    def next( self):
        if sys.version_info[0] == 3:
            return next( self.items)
        else:
            return self.items.next()
    def next_or_none( self):
        try:
            return self.next()
        except StopIteration:
            return None

def update_file( text, filename):
    '''
    Writes <text> to <filename>. Does nothing if contents of <filename> are
    already <text>.
    '''
    try:
        with open( filename) as f:
            text0 = f.read()
    except OSError:
        text0 = None
    if text == text0:
        log( 'Unchanged: ' + filename)
    else:
        log( 'Updating:  ' + filename)
        # Write to temp file and rename, to ensure we are atomic.
        filename_temp = f'{filename}-jlib-temp'
        with open( filename_temp, 'w') as f:
            f.write( text)
        os.rename( filename_temp, filename)


def mtime( filename, default=0):
    '''
    Returns mtime of file, or <default> if error - e.g. doesn't exist.
    '''
    try:
        return os.path.getmtime( filename)
    except OSError:
        return default

def get_filenames( paths):
    '''
    Yields each file in <paths>, walking any directories.
    '''
    if isinstance( paths, str):
        paths = (paths,)
    for name in paths:
        if os.path.isdir( name):
            for dirpath, dirnames, filenames in os.walk( name):
                for filename in filenames:
                    path = os.path.join( dirpath, filename)
                    yield path
        else:
            yield name

def remove( path):
    '''
    Removes file or directory, without raising exception if it doesn't exist.

    We assert-fail if the path still exists when we return, in case of
    permission problems etc.
    '''
    try:
        os.remove( path)
    except Exception:
        pass
    shutil.rmtree( path, ignore_errors=1)
    assert not os.path.exists( path)


# Things for figuring out whether files need updating, using mtimes.
#
def newest( names):
    '''
    Returns mtime of newest file in <filenames>. Returns 0 if no file exists.
    '''
    assert isinstance( names, (list, tuple))
    assert names
    ret_t = 0
    ret_name = None
    for filename in get_filenames( names):
        t = mtime( filename)
        if t > ret_t:
            ret_t = t
            ret_name = filename
    return ret_t, ret_name

def oldest( names):
    '''
    Returns mtime of oldest file in <filenames> or 0 if no file exists.
    '''
    assert isinstance( names, (list, tuple))
    assert names
    ret_t = None
    ret_name = None
    for filename in get_filenames( names):
        t = mtime( filename)
        if ret_t is None or t < ret_t:
            ret_t = t
            ret_name = filename
    if ret_t is None:
        ret_t = 0
    return ret_t, ret_name

def update_needed( infiles, outfiles):
    '''
    If any file in <infiles> is newer than any file in <outfiles>, returns
    string description. Otherwise returns None.
    '''
    in_tmax, in_tmax_name = newest( infiles)
    out_tmin, out_tmin_name = oldest( outfiles)
    if in_tmax > out_tmin:
        text = f'{in_tmax_name} is newer than {out_tmin_name}'
        return text

def ensure_parent_dir( path):
    parent = os.path.dirname( path)
    if parent:
        os.makedirs( parent, exist_ok=True)

def build(
        infiles,
        outfiles,
        command,
        force_rebuild=False,
        out=None,
        all_reasons=False,
        verbose=True,
        executable=None,
        ):
    '''
    Ensures that <outfiles> are up to date using enhanced makefile-like
    determinism of dependencies.

    Rebuilds <outfiles> by running <command> if we determine that any of them
    are out of date.

    infiles:
        Names of files that are read by <command>. Can be a single filename. If
        an item is a directory, we expand to all filenames in the directory's
        tree.
    outfiles:
        Names of files that are written by <command>. Can also be a single
        filename.
    command:
        Command to run.
    force_rebuild:
        If true, we always re-run the command.
    out:
        A callable, passed to jlib.system(). If None, we use jlib.log() with
        our caller's stack record.
    all_reasons:
        If true we check all ways for a build being needed, even if we already
        know a build is needed; this only affects the diagnostic that we
        output.
    verbose:
        Passed to jlib.system().

    Returns:
        true if we have run the command, otherwise None.

    We compare mtimes of <infiles> and <outfiles>, and we also detect changes
    to the command itself.

    If any of infiles are newer than any of outfiles, or <command> is
    different to contents of commandfile '<outfile[0]>.cmd, then truncates
    commandfile and runs <command>. If <command> succeeds we writes <command>
    to commandfile.
    '''
    if isinstance( infiles, str):
        infiles = (infiles,)
    if isinstance( outfiles, str):
        outfiles = (outfiles,)

    if not out:
        out_frame_record = inspect.stack()[1]
        out = lambda text: log( text, caller=out_frame_record, nv=False)

    command_filename = f'{outfiles[0]}.cmd'

    reasons = []

    if not reasons or all_reasons:
        if force_rebuild:
            reasons.append( 'force_rebuild was specified')

    if not reasons or all_reasons:
        try:
            with open( command_filename) as f:
                command0 = f.read()
        except Exception:
            command0 = None
        if command != command0:
           reasons.append( 'command has changed')

    if not reasons or all_reasons:
        reason = update_needed( infiles, outfiles)
        if reason:
            reasons.append( reason)

    if not reasons:
        out( 'Already up to date: ' + ' '.join(outfiles))
        return

    if out:
        out( 'Rebuilding because %s: %s' % (
                ', and '.join( reasons),
                ' '.join(outfiles),
                ))

    # Empty <command_filename) while we run the command so that if command
    # fails but still creates target(s), then next time we will know target(s)
    # are not up to date.
    #
    ensure_parent_dir( command_filename)
    with open( command_filename, 'w') as f:
        pass

    system( command, out=out, verbose=verbose, executable=executable)

    with open( command_filename, 'w') as f:
        f.write( command)

    return True


def link_l_flags( sos):
    dirs = set()
    names = []
    if isinstance( sos, str):
        sos = [sos]
    for so in sos:
        if not so:
            continue
        dir_ = os.path.dirname( so)
        name = os.path.basename( so)
        assert name.startswith( 'lib')
        assert name.endswith ( '.so')
        name = name[3:-3]
        dirs.add( dir_)
        names.append( name)
    ret = ''
    # Important to use sorted() here, otherwise ordering from set() is
    # arbitrary causing occasional spurious rebuilds.
    for dir_ in sorted(dirs):
        ret += f' -L {dir_}'
    for name in names:
        ret += f' -l {name}'
    return ret
