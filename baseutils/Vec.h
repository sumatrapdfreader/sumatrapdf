#ifndef Vec_h
#define Vec_h

/* Simple but also optimized for small sizes vector/array class that can
store pointer types or POD types
(http://stackoverflow.com/questions/146452/what-are-pod-types-in-c). */
template <typename T>
class Vec {
private:
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

        if (newCap >= INT_MAX / EL_SIZE) abort();
        if (this->els != this->buf) {
            this->els = (T*)realloc(this->els, newCap * EL_SIZE);
        }
        else {
            this->els = (T*)malloc(newCap * EL_SIZE);
            if (this->els)
                memcpy(this->els, this->buf, this->len * EL_SIZE);
        }
        if (!this->els) abort();
        this->cap = newCap;
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

    void FreeEls() {
        if (els != buf)
            free(els);
    }

public:
    Vec(size_t initCap=0) {
        len = 0;
        cap = INTERNAL_BUF_CAP;
        els = buf;
        EnsureCap(initCap);
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

    void InsertAt(size_t idx, const T& el) {
        MakeSpaceAt(idx, 1)[0] = el;
    }

    void Append(const T& el) {
        InsertAt(len, el);
    }

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

    T& Pop() {
        assert(len > 0);
        if (len > 0)
            len--;
        return At(len);
    }

    T& Last() {
        assert(len > 0);
        return At(len - 1);
    }

    int Find(T el) {
        for (size_t i = 0; i < len; i++)
            if (els[i] == el)
                return (int)i;
        return -1;
    }

    void Remove(T el) {
        int i = Find(el);
        if (i > -1)
            RemoveAt(i);
    }

    // for convenient iteration over all elements
    T* First() { 
        return els;
    }

    T* Sentinel() {
        return els + len;
    }
};

#endif
