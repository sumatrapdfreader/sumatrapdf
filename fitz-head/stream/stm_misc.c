/*
 * Miscellaneous I/O functions
 */

#include "fitz-base.h"
#include "fitz-stream.h"

int
fz_tell(fz_stream *stm)
{
    if (stm->mode == FZ_SREAD)
        return fz_rtell(stm);
    return fz_wtell(stm);
}

fz_error *
fz_seek(fz_stream *stm, int offset, int whence)
{
    if (stm->mode == FZ_SREAD)
        return fz_rseek(stm, offset, whence);
    return fz_wseek(stm, offset, whence);
}

/*
 * Read a line terminated by LF or CR or CRLF.
 */

fz_error *
fz_readline(fz_stream *stm, char *mem, int n)
{
    fz_error *error;

    char *s = mem;
    int c = EOF;
    while (n > 1)
    {
        c = fz_readbyte(stm);
        if (c == EOF)
            break;
        if (c == '\r') {
            c = fz_peekbyte(stm);
            if (c == '\n')
                c = fz_readbyte(stm);
            break;
        }
        if (c == '\n')
            break;
        *s++ = c;
        n--;
    }
    if (n)
        *s = '\0';

    error = fz_readerror(stm);
    if (error)
        return fz_rethrow(error, "cannot read line");
    return fz_okay;
}

/*
 * Utility function to consume all the contents of an input stream into
 * a freshly allocated buffer; realloced and trimmed to size.
 */

enum { CHUNKSIZE = 1024 * 4 };

fz_error *
fz_readall(fz_buffer **bufp, fz_stream *stm)
{
    fz_error *error;
    fz_buffer *real;
    unsigned char *newbuf;
    unsigned char *buf;
    int len;
    int pos;
    int n;

    len = 0;
    pos = 0;
    buf = nil;

    while (1)
    {
        if (len - pos == 0)
        {
            len += CHUNKSIZE;
            newbuf = fz_realloc(buf, len);
            if (!newbuf)
            {
                fz_free(buf);
                return fz_throw("outofmem: scratch buffer");
            }
            buf = newbuf;
        }

        error = fz_read(&n, stm, buf + pos, len - pos);
        if (error)
        {
            fz_free(buf);
            return fz_rethrow(error, "cannot read data");
        }

        pos += n;

        if (n < CHUNKSIZE)
        {
            if (pos > 0)
            {
                newbuf = fz_realloc(buf, pos);
                if (!newbuf)
                {
                    fz_free(buf);
                    return fz_throw("outofmem: scratch buffer");
                }
            }
            else newbuf = buf;

            real = fz_malloc(sizeof(fz_buffer));
            if (!real)
            {
                fz_free(newbuf);
                return fz_throw("outofmem: buffer struct");
            }

            real->refs = 1;
            real->ownsdata = 1;
            real->bp = buf;
            real->rp = buf;
            real->wp = buf + pos;
            real->ep = buf + pos;
            real->eof = 1;

            *bufp = real;
            return fz_okay;
        }
    }
}

