#include <windows.h>
#include <assert.h>
#include "MemSegment.h"

void *MemSegment::getData(DWORD *sizeOut) {
    DWORD totalSize = dataSize;
    MemSegment *curr = next;
    while (curr) {
        totalSize += curr->dataSize;
        curr = curr->next;
    }
    if (0 == dataSize)
        return NULL;
    char *buf = (char*)malloc(totalSize + 1); // +1 for 0 termination
    if (!buf)
        return NULL;
    buf[totalSize] = 0;
    // the chunks are linked in reverse order, so we must reassemble them properly
    char *end = buf + totalSize;
    curr = next;
    while (curr) {
        end -= curr->dataSize;
        memcpy(end, curr->data, curr->dataSize);
        curr = curr->next;
    }
    end -= dataSize;
    memcpy(end, data, dataSize);
    assert(end == buf);
    *sizeOut = totalSize;
    return (void*)buf;
}

bool MemSegment::add(const void *buf, DWORD size) {
    assert(size > 0);
    if (!data) {
        dataSize = size;
        data = malloc(size);
        if (!data)
            return false;
        memcpy(data, buf, size);
    } else {
        MemSegment *ms = new MemSegment(buf, size);
        if (!ms)
            return false;
        if (!ms->data) {
            delete ms;
            return false;
        }
        ms->next = next;
        next = ms;
    }
    return true;
}

void MemSegment::freeAll() {
    free(data);
    data = NULL;
    // clever trick: each segment will delete the next segment
    delete next;
    next = NULL;
}

