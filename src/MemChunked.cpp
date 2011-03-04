#include <windows.h>
#include <assert.h>
#include "MemChunked.h"

DWORD MemChunked::TotalSize()
{
    DWORD size = 0;
    for (Chunk *c = chunks.First(); c < chunks.Sentinel(); c++) {
        size += c->size;
    }
    return size;
}

char *MemChunked::GetData(DWORD *sizeOut)
{
    DWORD totalSize = TotalSize();
    char *buf = (char *)malloc(totalSize + 1); // +1 for 0 termination
    buf[totalSize] = 0;

    char *p = buf;
    for (Chunk *c = chunks.First(); c < chunks.Sentinel(); c++) {
        memcpy(p, c->data, c->size);
        p += c->size;
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
    for (Chunk *c = chunks.First(); c < chunks.Sentinel(); c++) {
        free(c->data);
    }
}

