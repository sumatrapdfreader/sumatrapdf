#ifndef MEM_SEGMENT_H__
#define MEM_SEGMENT_H__

#include "vstrlist.h"

typedef struct {
    DWORD len;
    void *data;
} DataSegment;

class MemSegment : public vector<DataSegment *> {
private:
    DWORD totalSize;

public:
    MemSegment() : totalSize(0) { }
    ~MemSegment() { clearFree(); }

    bool add(const void *buf, DWORD size);
    char *getData(DWORD *sizeOut=NULL);
    void clearFree();
};

#endif
