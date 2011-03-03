#ifndef VEC_H__
#define VEC_H__

template <typename T>
class Vec {
    static const size_t INTERNAL_BUF_CAP = 16;
    static const size_t EL_SIZE = sizeof(T);
    size_t  len;
    size_t  cap;
    T *     els;
    T       buf[INTERNAL_BUF_CAP];

    void EnsureCap(size_t needed) {
        if (this->cap >= needed)
            return;
        size_t newCap = this->cap * 2;
        if (this->cap > 1024)
            newCap = this->cap * 3 / 2;

        if (needed > newCap)
            newCap = needed;

        T * newEls = (T*)malloc(newCap * EL_SIZE);
        if (len > 0)
            memcpy(newEls, els, len * EL_SIZE);
        FreeEls();
        els = newEls;
        cap = newCap;
    }

public:
    Vec(size_t initCap=0) {
        len = 0;
        cap = INTERNAL_BUF_CAP;
        els = buf;
        EnsureCap(initCap);
    }

    void FreeEls() {
        if (els != buf)
            free(els);
    }

    ~Vec() {
        FreeEls();
    }

    void Clear() {
        len = 0;
        cap = INTERNAL_BUF_CAP;
        FreeEls();
        els = buf;
    }

    T& operator[](size_t idx) const {
        return els[idx];
    }

    T& At(size_t idx) const {
        return els[idx];
    }

    size_t Count() const {
        return len;
    }

    T* MakeSpaceAt(size_t idx, size_t count=1) {
        EnsureCap(len + count);
        T* res = &(els[idx]);
        int tomove = len - idx;
        if (tomove > 0) {
            T* src = els + idx;
            T* dst = els + idx + count;
            memmove(dst, src, tomove * EL_SIZE);
        }
        len += count;
        return res;
    }

    void InsertAt(size_t idx, const T& el) {
        MakeSpaceAt(idx, 1)[0] = el;
    }

    void Append(const T& el) {
        InsertAt(len, el);
    }

#if 0
    int Find(T el) {
        for (size_t i=0; i<len; i++) {
            if (el == els[i])
                return i;
        }
        return -1;
    }
#endif

    void RemoveAt(size_t idx, size_t count=1) {
        int tomove = len - idx - count;
        if (tomove > 0) {
            T *dst = els + idx;
            T *src = els + idx + count;
            memmove(dst, src, tomove * EL_SIZE);
        }
        len -= count;
    }

    void Push(T el) {
        Append(el);
    }

    void Pop() {
        if (len > 0)
            --len;
    }
};

#endif
