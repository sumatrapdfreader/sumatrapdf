// for a lack of a better place, simple tests to make sure sigslot compiles
class SigSlotSender {
public:
    sigslot::signal0 Sig0;
    sigslot::signal1<int> Sig1;
    sigslot::signal2<int, void*> Sig2;
    sigslot::signal3<char, int, bool> Sig3;
    sigslot::signal4<int *, int, char, char *> Sig4;
#if 0
    sigslot::signal5<int> Sig5;
    sigslot::signal6<int> Sig6;
    sigslot::signal7<int> Sig7;
    sigslot::signal8<int> Sig8;
#endif
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
};

static void SigSlotTest()
{
    SigSlotSender s;
    SigSlotReceiver r;

    s.Sig0.emit();
    s.Sig1.emit(1);
    s.Sig2.emit(-4, NULL);
    s.Sig3.emit('a', 9633, true);
    s.Sig4.emit(NULL, 3, 'z', NULL);

    s.Sig0.connect(&r, &SigSlotReceiver::Slot0);
    s.Sig1.connect(&r, &SigSlotReceiver::Slot1);
    s.Sig2.connect(&r, &SigSlotReceiver::Slot2);
    s.Sig3.connect(&r, &SigSlotReceiver::Slot3);
    s.Sig4.connect(&r, &SigSlotReceiver::Slot4);

    s.Sig0.emit();
    s.Sig1.emit(1);
    s.Sig2.emit(-4, NULL);
    s.Sig3.emit('a', 9633, true);
    s.Sig4.emit(NULL, 3, 'z', NULL);
    s.Sig0.emit();
    s.Sig1.emit(1);
    s.Sig2.emit(-4, NULL);
    s.Sig3.emit('a', 9633, true);
    s.Sig4.emit(NULL, 3, 'z', NULL);

    assert(2 == r.s0c);
    assert(2 == r.s1c);
    assert(2 == r.s2c);
    assert(2 == r.s3c);
    assert(2 == r.s4c);

    s.Sig0.disconnect(&r);
    s.Sig1.disconnect(&r);
    s.Sig2.disconnect(&r);
    s.Sig3.disconnect(&r);
    s.Sig4.disconnect(&r);

    s.Sig0.emit();
    s.Sig1.emit(1);
    s.Sig2.emit(-4, NULL);
    s.Sig3.emit('a', 9633, true);
    s.Sig4.emit(NULL, 3, 'z', NULL);

    assert(2 == r.s0c);
    assert(2 == r.s1c);
    assert(2 == r.s2c);
    assert(2 == r.s3c);
    assert(2 == r.s4c);
}

