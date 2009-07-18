#ifndef MEM_SEGMENT_H__
#define MEM_SEGMENT_H__

class MemSegment {
private:
    class MemSegment *next;

public:
    MemSegment(const void *buf, DWORD size) {
        next = NULL;
        data = NULL;
        add(buf, size);
    };

    MemSegment() {
        next = NULL;
        data = NULL;
    }

    bool add(const void *buf, DWORD size);
    void freeAll();

    ~MemSegment() {
        freeAll();
    }

    void *getData(DWORD *sizeOut);
    void *data;
    DWORD dataSize;
};

#endif
