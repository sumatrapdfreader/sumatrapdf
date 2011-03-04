#ifndef MEM_SEGMENT_H__
#define MEM_SEGMENT_H__

// TODO: rename to MemChunked.h

#include "Vec.h"

class MemChunked
{
    struct Chunk
    {
        void *  data;
        DWORD   size;
    };

    Vec<Chunk> chunks;

    void FreeChunks();

public:
    MemChunked() { }
    ~MemChunked() { FreeChunks(); }

    DWORD   TotalSize() const;
    bool    AddChunk(const void *buf, DWORD size);
    char *  GetData(DWORD *sizeOut=NULL) const;
};

#endif
