#include <windows.h>
#include <assert.h>
#include "MemSegment.h"

DWORD MemChunked::TotalSize() const
{
    DWORD size = 0;
    for (size_t i=0; i<chunks.Count(); i++)
    {
        Chunk c = chunks.At(i);
        size += c.size;
    }
    return size;
}

char *MemChunked::GetData(DWORD *sizeOut) const
{
    DWORD totalSize = TotalSize();
    char *buf = (char *)malloc(totalSize + 1); // +1 for 0 termination
    buf[totalSize] = 0;

    char *p = buf;
    for (size_t i = 0; i < chunks.Count(); i++) {
        Chunk c = chunks.At(i);
        memcpy(p, c.data, c.size);
        p += c.size;
    }

    assert(p == buf + totalSize);
    if (sizeOut)
        *sizeOut = totalSize;
    return buf;
}

bool MemChunked::AddChunk(const void *buf, DWORD size)
{
    if (TotalSize() + size + 1 < TotalSize())
        return false;

    void *data = malloc(size);
    if (!data)
        return false;
    memcpy(data, buf, size);
    
    Chunk c = { data, size };
    chunks.Append(c);
    return true;
}

void MemChunked::FreeChunks()
{
    for (size_t i = 0; i < chunks.Count(); i++) {
        Chunk c = chunks.At(i);
        free(c.data);
    }
}

