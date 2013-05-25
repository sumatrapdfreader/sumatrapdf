/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "Sigslot.h"

// must be last due to assert() over-write
#include "UtAssert.h"

// for a lack of a better place, simple tests to make sure sigslot compiles
class SigSlotSender {
public:
    sigslot::signal0 Sig0;
    sigslot::signal1<int> Sig1;
    sigslot::signal2<int, void*> Sig2;
    sigslot::signal3<char, int, bool> Sig3;
    sigslot::signal4<int *, int, char, char *> Sig4;
    sigslot::signal5<size_t, int*, void*, unsigned, long long> Sig5;
    sigslot::signal6<int, int, int, int, int, int> Sig6;
    sigslot::signal7<int, int, int, int, int, int, int> Sig7;
    sigslot::signal8<int, int, int, int, int, int, int, int> Sig8;
};

class SigSlotReceiver : public sigslot::has_slots {
public:
    int s0c, s1c, s2c, s3c, s4c, s5c, s6c, s7c, s8c;
    SigSlotReceiver() {
        s0c = s1c = s2c = s3c = s4c = s5c = s6c = s7c = s8c = 0;
    }
    void Slot0() { s0c++; }
    void Slot1(int a1) { s1c++; }
    void Slot2(int a1, void *a2) { s2c++; }
    void Slot3(char a1, int a2, bool a3) { s3c++; }
    void Slot4(int *a1, int a2, char a3, char *a4) { s4c++; }
    void Slot5(size_t a1, int* a2, void* a3, unsigned a4, long long a5) { s5c++; }
    void Slot6(int a1, int a2, int a3, int a4, int a5, int a6) { s6c++; }
    void Slot7(int a1, int a2, int a3, int a4, int a5, int a6,  int a7) { s7c++; }
    void Slot8(int a1, int a2, int a3, int a4, int a5, int a6,  int a7, int a8) { s8c++; }
};

static void SigSlotTestEmit(SigSlotSender& s)
{
    s.Sig0.emit();
    s.Sig1.emit(1);
    s.Sig2.emit(-4, NULL);
    s.Sig3.emit('a', 9633, true);
    s.Sig4.emit(NULL, 3, 'z', NULL);
    s.Sig5.emit(3, NULL, NULL, 3, 8);
    s.Sig6.emit(1,2,3,4,5,6);
    s.Sig7.emit(1,2,3,4,5,6,7);
    s.Sig8.emit(1,2,3,4,5,6,7,8);
}

static void SigSlotTestAssertCounts(SigSlotReceiver& r, int count)
{
    utassert(count == r.s0c);
    utassert(count == r.s1c);
    utassert(count == r.s2c);
    utassert(count == r.s3c);
    utassert(count == r.s4c);
    utassert(count == r.s5c);
    utassert(count == r.s6c);
    utassert(count == r.s7c);
    utassert(count == r.s8c);
}

void SigSlotTest()
{
    SigSlotSender s;
    SigSlotReceiver r;
    SigSlotReceiver r2;

    SigSlotTestAssertCounts(r, 0);
    SigSlotTestEmit(s);
    SigSlotTestAssertCounts(r, 0);

    s.Sig0.disconnect(&r);
    s.Sig0.disconnect(NULL);
    s.Sig1.disconnect(&r);
    s.Sig1.disconnect(NULL);
    s.Sig2.disconnect(&r);
    s.Sig2.disconnect(NULL);
    s.Sig3.disconnect(&r);
    s.Sig3.disconnect(NULL);
    s.Sig4.disconnect(&r);
    s.Sig4.disconnect(NULL);
    s.Sig5.disconnect(&r);
    s.Sig5.disconnect(NULL);
    s.Sig6.disconnect(&r);
    s.Sig6.disconnect(NULL);
    s.Sig7.disconnect(&r);
    s.Sig7.disconnect(NULL);
    s.Sig8.disconnect(&r);
    s.Sig8.disconnect(NULL);

    s.Sig0.connect(&r, &SigSlotReceiver::Slot0);
    s.Sig1.connect(&r, &SigSlotReceiver::Slot1);
    s.Sig2.connect(&r, &SigSlotReceiver::Slot2);
    s.Sig3.connect(&r, &SigSlotReceiver::Slot3);
    s.Sig4.connect(&r, &SigSlotReceiver::Slot4);
    s.Sig5.connect(&r, &SigSlotReceiver::Slot5);
    s.Sig6.connect(&r, &SigSlotReceiver::Slot6);
    s.Sig7.connect(&r, &SigSlotReceiver::Slot7);
    s.Sig8.connect(&r, &SigSlotReceiver::Slot8);

    SigSlotTestEmit(s);
    SigSlotTestAssertCounts(r, 1);

    SigSlotTestEmit(s);
    SigSlotTestAssertCounts(r, 2);

    s.Sig0.disconnect(&r2);
    s.Sig0.disconnect(&r);
    s.Sig0.disconnect(&r);

    s.Sig1.disconnect(&r2);
    s.Sig1.disconnect(&r);
    s.Sig1.disconnect(&r);

    s.Sig2.disconnect(&r2);
    s.Sig2.disconnect(&r);
    s.Sig2.disconnect(&r);

    s.Sig3.disconnect(&r2);
    s.Sig3.disconnect(&r);
    s.Sig3.disconnect(&r);

    s.Sig4.disconnect(&r2);
    s.Sig4.disconnect(&r);
    s.Sig4.disconnect(&r);

    s.Sig5.disconnect(&r2);
    s.Sig5.disconnect(&r);
    s.Sig5.disconnect(&r);

    s.Sig6.disconnect(&r2);
    s.Sig6.disconnect(&r);
    s.Sig6.disconnect(&r);

    s.Sig7.disconnect(&r2);
    s.Sig7.disconnect(&r);
    s.Sig7.disconnect(&r);

    s.Sig8.disconnect(&r2);
    s.Sig8.disconnect(&r);
    s.Sig8.disconnect(&r);

    SigSlotTestEmit(s);
    SigSlotTestAssertCounts(r, 2);
}
