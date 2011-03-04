#ifndef MemChunked_h
#define MemChunked_h

#include "Vec.h"

class MemChunked
{
    struct Chunk
    {
        void *  data;
        DWORD   size;
    };

    Vec<Chunk>  chunks;

    void        FreeChunks();
    DWORD       TotalSize();

public:
    MemChunked() { }
    ~MemChunked() { FreeChunks(); }

    bool    AddChunk(const void *buf, DWORD size);
    char *  GetData(DWORD *sizeOut=NULL);
};

#endif
