#include <windows.h>
#include <assert.h>
#include "MemSegment.h"

char *MemSegment::getData(DWORD *sizeOut)
{
    char *buf = (char *)malloc(totalSize + 1); // +1 for 0 termination
    buf[totalSize] = 0;

    char *end = buf;
    for (size_t i = 0; i < this->size(); i++) {
        DataSegment *seg = (*this)[i];
        memcpy(end, seg->data, seg->len);
        end += seg->len;
    }

    assert(end == buf + totalSize);
    if (sizeOut)
        *sizeOut = totalSize;
    return buf;
}

bool MemSegment::add(const void *buf, DWORD size)
{
    if (totalSize + size + 1 < totalSize)
        return false;

    void *data = malloc(size);
    if (!data)
        return false;

    DataSegment *seg = new DataSegment;
    seg->len = size;
    seg->data = data;
    memcpy(seg->data, buf, size);
    totalSize += size;
    this->push_back(seg);
    return true;
}

void MemSegment::clearFree()
{
    for (size_t i = 0; i < this->size(); i++) {
        DataSegment *seg = (*this)[i];
        free(seg->data);
        delete seg;
    }
    this->clear();
}
