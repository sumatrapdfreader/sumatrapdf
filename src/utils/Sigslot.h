// sigslot.h: Signal/Slot classes
// 
// Written by Sarah Thompson (sarah@telergy.com) 2002.
//
// License: Public domain. You are free to use this code however you like, with the proviso that
// the author takes on no responsibility or liability for any use.
// See full documentation at http://sigslot.sourceforge.net/)
//
// Sumatra notes:
// - we removed non-windows code
// - we only use multi_threaded_global threading policy, support for other
//   policies has been removed
// - we replaced use of stl with our Vec
// - rewrote things so that it compiles both with msvc and moder C++ compilers
//
// TODO: optimize storage. Instead of having a Vec inside each has_slot,
// they could use linked list and only a single pointer to the list.
// allocations would be done from a single VecSegmented and freed item
// would be put on a free list, so that we can reuse them

namespace sigslot {

class lock_block
{
public:
    lock_block()
    {
        static bool isinitialised = false;

        if (!isinitialised)
        {
            InitializeCriticalSection(get_critsec());
            isinitialised = true;
        }
        EnterCriticalSection(get_critsec());
    }

    ~lock_block() { LeaveCriticalSection(get_critsec()); }

private:
    lock_block(const lock_block&) { }

    CRITICAL_SECTION* get_critsec()
    {
        static CRITICAL_SECTION g_critsec;
        return &g_critsec;
    }
};

class has_slots;

class _connection_base0
{
public:
    virtual has_slots* getdest() const = 0;
    virtual void emit() = 0;
    virtual _connection_base0* clone() = 0;
    virtual _connection_base0* duplicate(has_slots* pnewdest) = 0;
    virtual ~_connection_base0() { }
};

template<class arg1_t>
class _connection_base1
{
public:
    virtual has_slots* getdest() const = 0;
    virtual void emit(arg1_t) = 0;
    virtual _connection_base1<arg1_t>* clone() = 0;
    virtual _connection_base1<arg1_t>* duplicate(has_slots* pnewdest) = 0;
    virtual ~_connection_base1() { }
};

template<class arg1_t, class arg2_t>
class _connection_base2
{
public:
    virtual has_slots* getdest() const = 0;
    virtual void emit(arg1_t, arg2_t) = 0;
    virtual _connection_base2<arg1_t, arg2_t>* clone() = 0;
    virtual _connection_base2<arg1_t, arg2_t>* duplicate(has_slots* pnewdest) = 0;
    virtual ~_connection_base2() { }
};

template<class arg1_t, class arg2_t, class arg3_t>
class _connection_base3
{
public:
    virtual has_slots* getdest() const = 0;
    virtual void emit(arg1_t, arg2_t, arg3_t) = 0;
    virtual _connection_base3<arg1_t, arg2_t, arg3_t>* clone() = 0;
    virtual _connection_base3<arg1_t, arg2_t, arg3_t>* duplicate(has_slots* pnewdest) = 0;
    virtual ~_connection_base3() { }
};

template<class arg1_t, class arg2_t, class arg3_t, class arg4_t>
class _connection_base4
{
public:
    virtual has_slots* getdest() const = 0;
    virtual void emit(arg1_t, arg2_t, arg3_t, arg4_t) = 0;
    virtual _connection_base4<arg1_t, arg2_t, arg3_t, arg4_t>* clone() = 0;
    virtual _connection_base4<arg1_t, arg2_t, arg3_t, arg4_t>* duplicate(has_slots* pnewdest) = 0;
    virtual ~_connection_base4() { }
};

template<class arg1_t, class arg2_t, class arg3_t, class arg4_t,
class arg5_t>
class _connection_base5
{
public:
    virtual has_slots* getdest() const = 0;
    virtual void emit(arg1_t, arg2_t, arg3_t, arg4_t, 
        arg5_t) = 0;
    virtual _connection_base5<arg1_t, arg2_t, arg3_t, arg4_t,
        arg5_t>* clone() = 0;
    virtual _connection_base5<arg1_t, arg2_t, arg3_t, arg4_t,
        arg5_t>* duplicate(has_slots* pnewdest) = 0;
    virtual ~_connection_base5() { }
};

template<class arg1_t, class arg2_t, class arg3_t, class arg4_t,
class arg5_t, class arg6_t>
class _connection_base6
{
public:
    virtual has_slots* getdest() const = 0;
    virtual void emit(arg1_t, arg2_t, arg3_t, arg4_t, arg5_t,
        arg6_t) = 0;
    virtual _connection_base6<arg1_t, arg2_t, arg3_t, arg4_t,
        arg5_t, arg6_t>* clone() = 0;
    virtual _connection_base6<arg1_t, arg2_t, arg3_t, arg4_t,
        arg5_t, arg6_t>* duplicate(has_slots* pnewdest) = 0;
    virtual ~_connection_base6() { }
};

template<class arg1_t, class arg2_t, class arg3_t, class arg4_t,
class arg5_t, class arg6_t, class arg7_t>
class _connection_base7
{
public:
    virtual has_slots* getdest() const = 0;
    virtual void emit(arg1_t, arg2_t, arg3_t, arg4_t, arg5_t,
        arg6_t, arg7_t) = 0;
    virtual _connection_base7<arg1_t, arg2_t, arg3_t, arg4_t,
        arg5_t, arg6_t, arg7_t>* clone() = 0;
    virtual _connection_base7<arg1_t, arg2_t, arg3_t, arg4_t,
        arg5_t, arg6_t, arg7_t>* duplicate(has_slots* pnewdest) = 0;
    virtual ~_connection_base7() { }
};

template<class arg1_t, class arg2_t, class arg3_t, class arg4_t,
class arg5_t, class arg6_t, class arg7_t, class arg8_t>
class _connection_base8
{
public:
    virtual has_slots* getdest() const = 0;
    virtual void emit(arg1_t, arg2_t, arg3_t, arg4_t, arg5_t,
        arg6_t, arg7_t, arg8_t) = 0;
    virtual _connection_base8<arg1_t, arg2_t, arg3_t, arg4_t,
        arg5_t, arg6_t, arg7_t, arg8_t>* clone() = 0;
    virtual _connection_base8<arg1_t, arg2_t, arg3_t, arg4_t,
        arg5_t, arg6_t, arg7_t, arg8_t>* duplicate(has_slots* pnewdest) = 0;
    virtual ~_connection_base8() { }
};

class _signal_base
{
public:
    virtual void slot_disconnect(has_slots* pslot) = 0;
    virtual void slot_duplicate(const has_slots* poldslot, has_slots* pnewslot) = 0;
    virtual ~_signal_base() { }
};

class has_slots
{
private:
    // m_senders is a set i.e. no duplicates
    Vec<_signal_base *> m_senders;

public:
    has_slots() { }

    has_slots(const has_slots& hs)
    {
        lock_block lock;
        for (size_t i = 0; i < hs.m_senders.Count(); i++) {
            _signal_base *s = hs.m_senders.At(i);
            s->slot_duplicate(&hs, this);
            m_senders.Append(s);
        }
    } 

    void signal_connect(_signal_base* sender)
    {
        lock_block lock;
        // ensure set semantics (i.e. no duplicates)
        if (!m_senders.Contains(sender))
            m_senders.Append(sender);
    }

    void signal_disconnect(_signal_base* sender)
    {
        lock_block lock;
        m_senders.Remove(sender);
    }

    // TODO: does it have to be virtual or was it only needed
    /// to accomodate inheriting from mt_policy, which did have
    // virtual functions?
    virtual ~has_slots()
    {
        disconnect_all();
    }

    void disconnect_all()
    {
        lock_block lock;
        for (size_t i = 0; i < m_senders.Count(); i++) {
            _signal_base *s = m_senders.At(i);
            s->slot_disconnect(this);
        }
        m_senders.Reset();
    }
};

class _signal_base0 : public _signal_base
{
public:
    typedef _connection_base0 conn_t;
    Vec<conn_t *> m_connections;

    _signal_base0() { }

    _signal_base0(_signal_base0& s)
        : _signal_base(s)
    {
        lock_block lock;
        for (conn_t** c = s.m_connections.IterStart(); c; c = s.m_connections.IterNext()) {
            (*c)->getdest()->signal_connect(this);
            m_connections.Append((*c)->clone());
        }
    }

    void slot_duplicate(const has_slots* oldtarget, has_slots* newtarget)
    {
        lock_block lock;
        size_t end = m_connections.Count(); // cache because we modify m_connections
        for (size_t i = 0; i < end; i++) {
            conn_t *c = m_connections.At(i);
            if (c->getdest() == oldtarget)
                m_connections.Append(c->duplicate(newtarget));
        }
    }

    ~_signal_base0()
    {
        disconnect_all();
    }

    void disconnect_all()
    {
        lock_block lock;
        for (conn_t** c = m_connections.IterStart(); c; c = m_connections.IterNext()) {
            (*c)->getdest()->signal_disconnect(this);
            delete *c;
        }
        m_connections.Reset();
    }

    void disconnect(has_slots* pclass)
    {
        lock_block lock;
        for (conn_t** c = m_connections.IterStart(); c; c = m_connections.IterNext()) {
            if ((*c)->getdest() == pclass)
            {
                delete *c; // must delete before Remove()
                m_connections.Remove(*c);
                pclass->signal_disconnect(this);
                return;
            }
        }
    }

    void slot_disconnect(has_slots* pslot)
    {
        lock_block lock;
        size_t i = 0;
        while (i < m_connections.Count()) {
            if (m_connections.At(i)->getdest() == pslot)
                m_connections.RemoveAtFast(i);
            else
                i++;
        }
    }
};

template<class arg1_t>
class _signal_base1 : public _signal_base
{
public:
    typedef _connection_base1<arg1_t> conn_t;
    Vec<conn_t *> m_connections;

    _signal_base1() { }

    _signal_base1(_signal_base1<arg1_t>& s)
        : _signal_base(s)
    {
        lock_block lock;
        for (conn_t** c = s.m_connections.IterStart(); c; c = s.m_connections.IterNext()) {
            (*c)->getdest()->signal_connect(this);
            m_connections.Append((*c)->clone());
        }
    }

    void slot_duplicate(const has_slots* oldtarget, has_slots* newtarget)
    {
        lock_block lock;
        size_t end = m_connections.Count(); // cache because we modify m_connections
        for (size_t i = 0; i < end; i++) {
            conn_t *c = m_connections.At(i);
            if (c->getdest() == oldtarget)
                m_connections.Append(c->duplicate(newtarget));
        }
    }

    ~_signal_base1()
    {
        disconnect_all();
    }

    void disconnect_all()
    {
        lock_block lock;
        for (conn_t** c = m_connections.IterStart(); c; c = m_connections.IterNext()) {
            (*c)->getdest()->signal_disconnect(this);
            delete *c;
        }
        m_connections.Reset();
    }

    void disconnect(has_slots* pclass)
    {
        lock_block lock;
        for (conn_t** c = m_connections.IterStart(); c; c = m_connections.IterNext()) {
            if ((*c)->getdest() == pclass)
            {
                delete *c; // must delete before Remove()
                m_connections.Remove(*c);
                pclass->signal_disconnect(this);
                return;
            }
        }
    }

    void slot_disconnect(has_slots* pslot)
    {
        lock_block lock;
        size_t i = 0;
        while (i < m_connections.Count()) {
            if (m_connections.At(i)->getdest() == pslot)
                m_connections.RemoveAtFast(i);
            else
                i++;
        }
    }
};

template<class arg1_t, class arg2_t>
class _signal_base2 : public _signal_base
{
protected:
    typedef _connection_base2<arg1_t, arg2_t> conn_t;
    Vec<conn_t *> m_connections;

public:
    _signal_base2() { }

    _signal_base2(_signal_base2<arg1_t, arg2_t>& s)
        : _signal_base(s)
    {
        lock_block lock;
        for (conn_t** c = s.m_connections.IterStart(); c; c = s.m_connections.IterNext()) {
            (*c)->getdest()->signal_connect(this);
            m_connections.Append((*c)->clone());
        }
    }

    void slot_duplicate(const has_slots* oldtarget, has_slots* newtarget)
    {
        lock_block lock;
        size_t end = m_connections.Count(); // cache because we modify m_connections
        for (size_t i = 0; i < end; i++) {
            conn_t *c = m_connections.At(i);
            if (c->getdest() == oldtarget)
                m_connections.Append(c->duplicate(newtarget));
        }
    }

    ~_signal_base2()
    {
        disconnect_all();
    }

    void disconnect_all()
    {
        lock_block lock;
        for (conn_t** c = m_connections.IterStart(); c; c = m_connections.IterNext()) {
            (*c)->getdest()->signal_disconnect(this);
            delete *c;
        }
        m_connections.Reset();
    }

    void disconnect(has_slots* pclass)
    {
        lock_block lock;
        for (conn_t** c = m_connections.IterStart(); c; c = m_connections.IterNext()) {
            if ((*c)->getdest() == pclass)
            {
                delete *c; // must delete before Remove()
                m_connections.Remove(*c);
                pclass->signal_disconnect(this);
                return;
            }
        }
    }

    void slot_disconnect(has_slots* pslot)
    {
        lock_block lock;
        size_t i = 0;
        while (i < m_connections.Count()) {
            if (m_connections.At(i)->getdest() == pslot)
                m_connections.RemoveAtFast(i);
            else
                i++;
        }
    }
};

template<class arg1_t, class arg2_t, class arg3_t>
class _signal_base3 : public _signal_base
{
protected:
    typedef _connection_base3<arg1_t, arg2_t, arg3_t> conn_t;
    Vec<conn_t *> m_connections;

public:
    _signal_base3() { }

    _signal_base3(_signal_base3<arg1_t, arg2_t, arg3_t>& s)
        : _signal_base(s)
    {
        lock_block lock;
        for (conn_t** c = s.m_connections.IterStart(); c; c = s.m_connections.IterNext()) {
            (*c)->getdest()->signal_connect(this);
            m_connections.Append((*c)->clone());
        }
    }

    void slot_duplicate(const has_slots* oldtarget, has_slots* newtarget)
    {
        lock_block lock;
        size_t end = m_connections.Count(); // cache because we modify m_connections
        for (size_t i = 0; i < end; i++) {
            conn_t *c = m_connections.At(i);
            if (c->getdest() == oldtarget)
                m_connections.Append(c->duplicate(newtarget));
        }
    }

    ~_signal_base3()
    {
        disconnect_all();
    }

    void disconnect_all()
    {
        lock_block lock;
        for (conn_t** c = m_connections.IterStart(); c; c = m_connections.IterNext()) {
            (*c)->getdest()->signal_disconnect(this);
            delete *c;
        }
        m_connections.Reset();
    }

    void disconnect(has_slots* pclass)
    {
        lock_block lock;
        for (conn_t** c = m_connections.IterStart(); c; c = m_connections.IterNext()) {
            if ((*c)->getdest() == pclass)
            {
                delete *c; // must delete before Remove()
                m_connections.Remove(*c);
                pclass->signal_disconnect(this);
                return;
            }
        }
    }

    void slot_disconnect(has_slots* pslot)
    {
        lock_block lock;
        size_t i = 0;
        while (i < m_connections.Count()) {
            if (m_connections.At(i)->getdest() == pslot)
                m_connections.RemoveAtFast(i);
            else
                i++;
        }
    }
};

template<class arg1_t, class arg2_t, class arg3_t, class arg4_t>
class _signal_base4 : public _signal_base
{
public:
    typedef _connection_base4<arg1_t, arg2_t, arg3_t, arg4_t> conn_t;
    Vec<conn_t *> m_connections;

    _signal_base4() { }

    _signal_base4(_signal_base4<arg1_t, arg2_t, arg3_t, arg4_t>& s)
        : _signal_base(s)
    {
        lock_block lock;
        for (conn_t** c = s.m_connections.IterStart(); c; c = s.m_connections.IterNext()) {
            (*c)->getdest()->signal_connect(this);
            m_connections.Append((*c)->clone());
        }

    }

    void slot_duplicate(const has_slots* oldtarget, has_slots* newtarget)
    {
        lock_block lock;
        size_t end = m_connections.Count(); // cache because we modify m_connections
        for (size_t i = 0; i < end; i++) {
            conn_t *c = m_connections.At(i);
            if (c->getdest() == oldtarget)
                m_connections.Append(c->duplicate(newtarget));
        }
    }

    ~_signal_base4()
    {
        disconnect_all();
    }

    void disconnect_all()
    {
        lock_block lock;
        for (conn_t** c = m_connections.IterStart(); c; c = m_connections.IterNext()) {
            (*c)->getdest()->signal_disconnect(this);
            delete *c;
        }
        m_connections.Reset();
    }

    void disconnect(has_slots* pclass)
    {
        lock_block lock;
        for (conn_t** c = m_connections.IterStart(); c; c = m_connections.IterNext()) {
            if ((*c)->getdest() == pclass)
            {
                delete *c; // must delete before Remove()
                m_connections.Remove(*c);
                pclass->signal_disconnect(this);
                return;
            }
        }
    }

    void slot_disconnect(has_slots* pslot)
    {
        lock_block lock;
        size_t i = 0;
        while (i < m_connections.Count()) {
            if (m_connections.At(i)->getdest() == pslot)
                m_connections.RemoveAtFast(i);
            else
                i++;
        }
    }
};

template<class arg1_t, class arg2_t, class arg3_t, class arg4_t,
class arg5_t>
class _signal_base5 : public _signal_base
{
public:
    typedef _connection_base5<arg1_t, arg2_t, arg3_t, arg4_t, arg5_t> conn_t;
    Vec<conn_t *> m_connections;

    _signal_base5() { }

    _signal_base5(_signal_base5<arg1_t, arg2_t, arg3_t, arg4_t,
        arg5_t>& s)
        : _signal_base(s)
    {
        lock_block lock;
        for (conn_t** c = s.m_connections.IterStart(); c; c = s.m_connections.IterNext()) {
            (*c)->getdest()->signal_connect(this);
            m_connections.Append((*c)->clone());
        }
    }

    void slot_duplicate(const has_slots* oldtarget, has_slots* newtarget)
    {
        lock_block lock;
        size_t end = m_connections.Count(); // cache because we modify m_connections
        for (size_t i = 0; i < end; i++) {
            conn_t *c = m_connections.At(i);
            if (c->getdest() == oldtarget)
                m_connections.Append(c->duplicate(newtarget));
        }
    }

    ~_signal_base5()
    {
        disconnect_all();
    }

    void disconnect_all()
    {
        lock_block lock;
        for (conn_t** c = m_connections.IterStart(); c; c = m_connections.IterNext()) {
            (*c)->getdest()->signal_disconnect(this);
            delete *c;
        }
        m_connections.Reset();
    }

    void disconnect(has_slots* pclass)
    {
        lock_block lock;
        for (conn_t** c = m_connections.IterStart(); c; c = m_connections.IterNext()) {
            if ((*c)->getdest() == pclass)
            {
                delete *c; // must delete before Remove()
                m_connections.Remove(*c);
                pclass->signal_disconnect(this);
                return;
            }
        }
    }

    void slot_disconnect(has_slots* pslot)
    {
        lock_block lock;
        size_t i = 0;
        while (i < m_connections.Count()) {
            if (m_connections.At(i)->getdest() == pslot)
                m_connections.RemoveAtFast(i);
            else
                i++;
        }
    }
};

template<class arg1_t, class arg2_t, class arg3_t, class arg4_t, class arg5_t, class arg6_t>
class _signal_base6 : public _signal_base
{
public:
    typedef _connection_base6<arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t> conn_t;
    Vec<conn_t *> m_connections;

    _signal_base6() { }

    _signal_base6(_signal_base6<arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t>& s)
        : _signal_base(s)
    {
        lock_block lock;
        for (conn_t** c = s.m_connections.IterStart(); c; c = s.m_connections.IterNext()) {
            (*c)->getdest()->signal_connect(this);
            m_connections.Append((*c)->clone());
        }
    }

    void slot_duplicate(const has_slots* oldtarget, has_slots* newtarget)
    {
        lock_block lock;
        size_t end = m_connections.Count(); // cache because we modify m_connections
        for (size_t i = 0; i < end; i++) {
            conn_t *c = m_connections.At(i);
            if (c->getdest() == oldtarget)
                m_connections.Append(c->duplicate(newtarget));
        }
    }

    ~_signal_base6()
    {
        disconnect_all();
    }

    void disconnect_all()
    {
        lock_block lock;
        for (conn_t** c = m_connections.IterStart(); c; c = m_connections.IterNext()) {
            (*c)->getdest()->signal_disconnect(this);
            delete *c;
        }
        m_connections.Reset();
    }

    void disconnect(has_slots* pclass)
    {
        lock_block lock;
        for (conn_t** c = m_connections.IterStart(); c; c = m_connections.IterNext()) {
            if ((*c)->getdest() == pclass)
            {
                delete *c; // must delete before Remove()
                m_connections.Remove(*c);
                pclass->signal_disconnect(this);
                return;
            }
        }
    }

    void slot_disconnect(has_slots* pslot)
    {
        lock_block lock;
        size_t i = 0;
        while (i < m_connections.Count()) {
            if (m_connections.At(i)->getdest() == pslot)
                m_connections.RemoveAtFast(i);
            else
                i++;
        }
    }
};

template<class arg1_t, class arg2_t, class arg3_t, class arg4_t, class arg5_t, class arg6_t, class arg7_t>
class _signal_base7 : public _signal_base
{
public:
    typedef _connection_base7<arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t, arg7_t> conn_t;
    Vec<conn_t *> m_connections;

    _signal_base7() {}

    _signal_base7(_signal_base7<arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t, arg7_t>& s)
        : _signal_base(s)
    {
        lock_block lock;
        for (conn_t** c = s.m_connections.IterStart(); c; c = s.m_connections.IterNext()) {
            (*c)->getdest()->signal_connect(this);
            m_connections.Append((*c)->clone());
        }
    }

    void slot_duplicate(const has_slots* oldtarget, has_slots* newtarget)
    {
        lock_block lock;
        size_t end = m_connections.Count(); // cache because we modify m_connections
        for (size_t i = 0; i < end; i++) {
            conn_t *c = m_connections.At(i);
            if (c->getdest() == oldtarget)
                m_connections.Append(c->duplicate(newtarget));
        }
    }

    ~_signal_base7()
    {
        disconnect_all();
    }

    void disconnect_all()
    {
        lock_block lock;
        for (conn_t** c = m_connections.IterStart(); c; c = m_connections.IterNext()) {
            (*c)->getdest()->signal_disconnect(this);
            delete *c;
        }
        m_connections.Reset();
    }

    void disconnect(has_slots* pclass)
    {
        lock_block lock;
        for (conn_t** c = m_connections.IterStart(); c; c = m_connections.IterNext()) {
            if ((*c)->getdest() == pclass)
            {
                delete *c; // must delete before Remove()
                m_connections.Remove(*c);
                pclass->signal_disconnect(this);
                return;
            }
        }
    }

    void slot_disconnect(has_slots* pslot)
    {
        lock_block lock;
        size_t i = 0;
        while (i < m_connections.Count()) {
            if (m_connections.At(i)->getdest() == pslot)
                m_connections.RemoveAtFast(i);
            else
                i++;
        }
    }
};

template<class arg1_t, class arg2_t, class arg3_t, class arg4_t, class arg5_t, class arg6_t, class arg7_t, class arg8_t>
class _signal_base8 : public _signal_base
{
public:
    typedef _connection_base8<arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t, arg7_t, arg8_t> conn_t;
    Vec<conn_t *> m_connections;

    _signal_base8() { }

    _signal_base8(_signal_base8<arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t, arg7_t, arg8_t>& s)
        : _signal_base(s)
    {
        lock_block lock;
        for (conn_t** c = s.m_connections.IterStart(); c; c = s.m_connections.IterNext()) {
            (*c)->getdest()->signal_connect(this);
            m_connections.Append((*c)->clone());
        }
    }

    void slot_duplicate(const has_slots* oldtarget, has_slots* newtarget)
    {
        lock_block lock;
        size_t end = m_connections.Count(); // cache because we modify m_connections
        for (size_t i = 0; i < end; i++) {
            conn_t *c = m_connections.At(i);
            if (c->getdest() == oldtarget)
                m_connections.Append(c->duplicate(newtarget));
        }
    }

    ~_signal_base8()
    {
        disconnect_all();
    }

    void disconnect_all()
    {
        lock_block lock;
        for (conn_t** c = m_connections.IterStart(); c; c = m_connections.IterNext()) {
            (*c)->getdest()->signal_disconnect(this);
            delete *c;
        }
        m_connections.Reset();
    }

    void disconnect(has_slots* pclass)
    {
        lock_block lock;
        for (conn_t** c = m_connections.IterStart(); c; c = m_connections.IterNext()) {
            if ((*c)->getdest() == pclass)
            {
                delete *c; // must delete before Remove()
                m_connections.Remove(*c);
                pclass->signal_disconnect(this);
                return;
            }
        }
    }

    void slot_disconnect(has_slots* pslot)
    {
        lock_block lock;
        size_t i = 0;
        while (i < m_connections.Count()) {
            if (m_connections.At(i)->getdest() == pslot)
                m_connections.RemoveAtFast(i);
            else
                i++;
        }
    }
};

template<class dest_type>
class _connection0 : public _connection_base0
{
public:
    _connection0()
    {
        m_pobject = NULL;
        m_pmemfun = NULL;
    }

    _connection0(dest_type* pobject, void (dest_type::*pmemfun)())
    {
        m_pobject = pobject;
        m_pmemfun = pmemfun;
    }

    virtual _connection_base0* clone()
    {
        return new _connection0<dest_type>(*this);
    }

    virtual _connection_base0* duplicate(has_slots* pnewdest)
    {
        return new _connection0<dest_type>((dest_type *)pnewdest, m_pmemfun);
    }

    virtual void emit()
    {
        (m_pobject->*m_pmemfun)();
    }

    virtual has_slots* getdest() const
    {
        return m_pobject;
    }

private:
    dest_type* m_pobject;
    void (dest_type::* m_pmemfun)();
};

template<class dest_type, class arg1_t>
class _connection1 : public _connection_base1<arg1_t>
{
public:
    _connection1()
    {
        m_pobject = NULL;
        m_pmemfun = NULL;
    }

    _connection1(dest_type* pobject, void (dest_type::*pmemfun)(arg1_t))
    {
        m_pobject = pobject;
        m_pmemfun = pmemfun;
    }

    virtual _connection_base1<arg1_t>* clone()
    {
        return new _connection1<dest_type, arg1_t>(*this);
    }

    virtual _connection_base1<arg1_t>* duplicate(has_slots* pnewdest)
    {
        return new _connection1<dest_type, arg1_t>((dest_type *)pnewdest, m_pmemfun);
    }

    virtual void emit(arg1_t a1)
    {
        (m_pobject->*m_pmemfun)(a1);
    }

    virtual has_slots* getdest() const
    {
        return m_pobject;
    }

private:
    dest_type* m_pobject;
    void (dest_type::* m_pmemfun)(arg1_t);
};

template<class dest_type, class arg1_t, class arg2_t>
class _connection2 : public _connection_base2<arg1_t, arg2_t>
{
public:
    _connection2()
    {
        m_pobject = NULL;
        m_pmemfun = NULL;
    }

    _connection2(dest_type* pobject, void (dest_type::*pmemfun)(arg1_t,
        arg2_t))
    {
        m_pobject = pobject;
        m_pmemfun = pmemfun;
    }

    virtual _connection_base2<arg1_t, arg2_t>* clone()
    {
        return new _connection2<dest_type, arg1_t, arg2_t>(*this);
    }

    virtual _connection_base2<arg1_t, arg2_t>* duplicate(has_slots* pnewdest)
    {
        return new _connection2<dest_type, arg1_t, arg2_t>((dest_type *)pnewdest, m_pmemfun);
    }

    virtual void emit(arg1_t a1, arg2_t a2)
    {
        (m_pobject->*m_pmemfun)(a1, a2);
    }

    virtual has_slots* getdest() const
    {
        return m_pobject;
    }

private:
    dest_type* m_pobject;
    void (dest_type::* m_pmemfun)(arg1_t, arg2_t);
};

template<class dest_type, class arg1_t, class arg2_t, class arg3_t>
class _connection3 : public _connection_base3<arg1_t, arg2_t, arg3_t>
{
public:
    _connection3()
    {
        m_pobject = NULL;
        m_pmemfun = NULL;
    }

    _connection3(dest_type* pobject, void (dest_type::*pmemfun)(arg1_t,
        arg2_t, arg3_t))
    {
        m_pobject = pobject;
        m_pmemfun = pmemfun;
    }

    virtual _connection_base3<arg1_t, arg2_t, arg3_t>* clone()
    {
        return new _connection3<dest_type, arg1_t, arg2_t, arg3_t>(*this);
    }

    virtual _connection_base3<arg1_t, arg2_t, arg3_t>* duplicate(has_slots* pnewdest)
    {
        return new _connection3<dest_type, arg1_t, arg2_t, arg3_t>((dest_type *)pnewdest, m_pmemfun);
    }

    virtual void emit(arg1_t a1, arg2_t a2, arg3_t a3)
    {
        (m_pobject->*m_pmemfun)(a1, a2, a3);
    }

    virtual has_slots* getdest() const
    {
        return m_pobject;
    }

private:
    dest_type* m_pobject;
    void (dest_type::* m_pmemfun)(arg1_t, arg2_t, arg3_t);
};

template<class dest_type, class arg1_t, class arg2_t, class arg3_t, class arg4_t>
class _connection4 : public _connection_base4<arg1_t, arg2_t, arg3_t, arg4_t>
{
public:
    _connection4()
    {
        m_pobject = NULL;
        m_pmemfun = NULL;
    }

    _connection4(dest_type* pobject, void (dest_type::*pmemfun)(arg1_t, arg2_t, arg3_t, arg4_t))
    {
        m_pobject = pobject;
        m_pmemfun = pmemfun;
    }

    virtual _connection_base4<arg1_t, arg2_t, arg3_t, arg4_t>* clone()
    {
        return new _connection4<dest_type, arg1_t, arg2_t, arg3_t, arg4_t>(*this);
    }

    virtual _connection_base4<arg1_t, arg2_t, arg3_t, arg4_t>* duplicate(has_slots* pnewdest)
    {
        return new _connection4<dest_type, arg1_t, arg2_t, arg3_t, arg4_t>((dest_type *)pnewdest, m_pmemfun);
    }

    virtual void emit(arg1_t a1, arg2_t a2, arg3_t a3, arg4_t a4)
    {
        (m_pobject->*m_pmemfun)(a1, a2, a3, a4);
    }

    virtual has_slots* getdest() const
    {
        return m_pobject;
    }

private:
    dest_type* m_pobject;
    void (dest_type::* m_pmemfun)(arg1_t, arg2_t, arg3_t, arg4_t);
};

template<class dest_type, class arg1_t, class arg2_t, class arg3_t, class arg4_t, class arg5_t>
class _connection5 : public _connection_base5<arg1_t, arg2_t, arg3_t, arg4_t, arg5_t>
{
public:
    _connection5()
    {
        m_pobject = NULL;
        m_pmemfun = NULL;
    }

    _connection5(dest_type* pobject, void (dest_type::*pmemfun)(arg1_t, arg2_t, arg3_t, arg4_t, arg5_t))
    {
        m_pobject = pobject;
        m_pmemfun = pmemfun;
    }

    virtual _connection_base5<arg1_t, arg2_t, arg3_t, arg4_t,  arg5_t>* clone()
    {
        return new _connection5<dest_type, arg1_t, arg2_t, arg3_t, arg4_t, arg5_t>(*this);
    }

    virtual _connection_base5<arg1_t, arg2_t, arg3_t, arg4_t, arg5_t>* duplicate(has_slots* pnewdest)
    {
        return new _connection5<dest_type, arg1_t, arg2_t, arg3_t, arg4_t, arg5_t>((dest_type *)pnewdest, m_pmemfun);
    }

    virtual void emit(arg1_t a1, arg2_t a2, arg3_t a3, arg4_t a4, arg5_t a5)
    {
        (m_pobject->*m_pmemfun)(a1, a2, a3, a4, a5);
    }

    virtual has_slots* getdest() const
    {
        return m_pobject;
    }

private:
    dest_type* m_pobject;
    void (dest_type::* m_pmemfun)(arg1_t, arg2_t, arg3_t, arg4_t, arg5_t);
};

template<class dest_type, class arg1_t, class arg2_t, class arg3_t, class arg4_t, class arg5_t, class arg6_t>
class _connection6 : public _connection_base6<arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t>
{
public:
    _connection6()
    {
        m_pobject = NULL;
        m_pmemfun = NULL;
    }

    _connection6(dest_type* pobject, void (dest_type::*pmemfun)(arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t))
    {
        m_pobject = pobject;
        m_pmemfun = pmemfun;
    }

    virtual _connection_base6<arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t>* clone()
    {
        return new _connection6<dest_type, arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t>(*this);
    }

    virtual _connection_base6<arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t>* duplicate(has_slots* pnewdest)
    {
        return new _connection6<dest_type, arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t>((dest_type *)pnewdest, m_pmemfun);
    }

    virtual void emit(arg1_t a1, arg2_t a2, arg3_t a3, arg4_t a4, arg5_t a5, arg6_t a6)
    {
        (m_pobject->*m_pmemfun)(a1, a2, a3, a4, a5, a6);
    }

    virtual has_slots* getdest() const
    {
        return m_pobject;
    }

private:
    dest_type* m_pobject;
    void (dest_type::* m_pmemfun)(arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t);
};

template<class dest_type, class arg1_t, class arg2_t, class arg3_t, class arg4_t, class arg5_t, class arg6_t, class arg7_t>
class _connection7 : public _connection_base7<arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t, arg7_t>
{
public:
    _connection7()
    {
        m_pobject = NULL;
        m_pmemfun = NULL;
    }

    _connection7(dest_type* pobject, void (dest_type::*pmemfun)(arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t, arg7_t))
    {
        m_pobject = pobject;
        m_pmemfun = pmemfun;
    }

    virtual _connection_base7<arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t, arg7_t>* clone()
    {
        return new _connection7<dest_type, arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t, arg7_t>(*this);
    }

    virtual _connection_base7<arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t, arg7_t>* duplicate(has_slots* pnewdest)
    {
        return new _connection7<dest_type, arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t, arg7_t>((dest_type *)pnewdest, m_pmemfun);
    }

    virtual void emit(arg1_t a1, arg2_t a2, arg3_t a3, arg4_t a4, arg5_t a5, arg6_t a6, arg7_t a7)
    {
        (m_pobject->*m_pmemfun)(a1, a2, a3, a4, a5, a6, a7);
    }

    virtual has_slots* getdest() const
    {
        return m_pobject;
    }

private:
    dest_type* m_pobject;
    void (dest_type::* m_pmemfun)(arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t, arg7_t);
};

template<class dest_type, class arg1_t, class arg2_t, class arg3_t, class arg4_t, class arg5_t, class arg6_t, class arg7_t, class arg8_t>
class _connection8 : public _connection_base8<arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t, arg7_t, arg8_t>
{
public:
    _connection8()
    {
        m_pobject = NULL;
        m_pmemfun = NULL;
    }

    _connection8(dest_type* pobject, void (dest_type::*pmemfun)(arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t, arg7_t, arg8_t))
    {
        m_pobject = pobject;
        m_pmemfun = pmemfun;
    }

    virtual _connection_base8<arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t, arg7_t, arg8_t>* clone()
    {
        return new _connection8<dest_type, arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t, arg7_t, arg8_t>(*this);
    }

    virtual _connection_base8<arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t, arg7_t, arg8_t>* duplicate(has_slots* pnewdest)
    {
        return new _connection8<dest_type, arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t, arg7_t, arg8_t>((dest_type *)pnewdest, m_pmemfun);
    }

    virtual void emit(arg1_t a1, arg2_t a2, arg3_t a3, arg4_t a4, arg5_t a5, arg6_t a6, arg7_t a7, arg8_t a8)
    {
        (m_pobject->*m_pmemfun)(a1, a2, a3, a4, a5, a6, a7, a8);
    }

    virtual has_slots* getdest() const
    {
        return m_pobject;
    }

private:
    dest_type* m_pobject;
    void (dest_type::* m_pmemfun)(arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t, arg7_t, arg8_t);
};

class signal0 : public _signal_base0
{
public:
    signal0() { }

    signal0(signal0& s)
        : _signal_base0(s)
    { }

    template<class desttype>
    void connect(desttype* pclass, void (desttype::*pmemfun)())
    {
        lock_block lock;
        typedef _connection0<desttype> conn_t;
        conn_t* conn = new conn_t(pclass, pmemfun);
        this->m_connections.Append(conn);
        pclass->signal_connect(this);
    }

    void emit()
    {
        lock_block lock;
        for (size_t i = 0; i < this->m_connections.Count(); i++) {
            this->m_connections.At(i)->emit();
        }
    }

    void operator()()
    {
        emit();
    }
};

template<class arg1_t>
class signal1 : public _signal_base1<arg1_t>
{
public:
    signal1() { }

    signal1(signal1<arg1_t>& s)
        : _signal_base1<arg1_t>(s)
    { }

    template<class desttype>
    void connect(desttype* pclass, void (desttype::*pmemfun)(arg1_t))
    {
        typedef _connection1<desttype, arg1_t> conn_t;
        lock_block lock;
        conn_t* conn = new conn_t(pclass, pmemfun);
        this->m_connections.Append(conn);
        pclass->signal_connect(this);
    }

    void emit(arg1_t a1)
    {
        lock_block lock;
        for (size_t i = 0; i < this->m_connections.Count(); i++) {
            this->m_connections.At(i)->emit(a1);
        }
    }

    void operator()(arg1_t a1)
    {
        emit(a1);
    }
};

template<class arg1_t, class arg2_t>
class signal2 : public _signal_base2<arg1_t, arg2_t>
{
public:
    signal2() { }

    signal2(signal2<arg1_t, arg2_t>& s)
        : _signal_base2<arg1_t, arg2_t>(s)
    { }

    template<class desttype>
    void connect(desttype* pclass, void (desttype::*pmemfun)(arg1_t, arg2_t))
    {
        typedef _connection2<desttype, arg1_t, arg2_t> conn_t;
        lock_block lock;
        conn_t* conn = new conn_t(pclass, pmemfun);
        this->m_connections.Append(conn);
        pclass->signal_connect(this);
    }

    void emit(arg1_t a1, arg2_t a2)
    {
        lock_block lock;
        for (size_t i = 0; i < this->m_connections.Count(); i++) {
            this->m_connections.At(i)->emit(a1, a2);
        }
    }

    void operator()(arg1_t a1, arg2_t a2)
    {
        emit(a1, a2);
    }
};

template<class arg1_t, class arg2_t, class arg3_t>
class signal3 : public _signal_base3<arg1_t, arg2_t, arg3_t>
{
public:
    signal3() { }

    signal3(signal3<arg1_t, arg2_t, arg3_t>& s)
        : _signal_base3<arg1_t, arg2_t, arg3_t>(s)
    { }

    template<class desttype>
    void connect(desttype* pclass, void (desttype::*pmemfun)(arg1_t, arg2_t, arg3_t))
    {
        typedef _connection3<desttype, arg1_t, arg2_t, arg3_t> conn_t;
        lock_block lock;
        conn_t* conn = new conn_t(pclass, pmemfun);
        this->m_connections.Append(conn);
        pclass->signal_connect(this);
    }

    void emit(arg1_t a1, arg2_t a2, arg3_t a3)
    {
        lock_block lock;
        for (size_t i = 0; i < this->m_connections.Count(); i++) {
            this->m_connections.At(i)->emit(a1, a2, a3);
        }
    }

    void operator()(arg1_t a1, arg2_t a2, arg3_t a3)
    {
        emit(a1, a2, a3);
    }
};

template<class arg1_t, class arg2_t, class arg3_t, class arg4_t>
class signal4 : public _signal_base4<arg1_t, arg2_t, arg3_t,
    arg4_t>
{
public:
    signal4() { }

    signal4(signal4<arg1_t, arg2_t, arg3_t, arg4_t>& s)
        : _signal_base4<arg1_t, arg2_t, arg3_t, arg4_t>(s)
    { }

    template<class desttype>
    void connect(desttype* pclass, void (desttype::*pmemfun)(arg1_t, arg2_t, arg3_t, arg4_t))
    {
        typedef _connection4<desttype, arg1_t, arg2_t, arg3_t, arg4_t> conn_t;
        lock_block lock;
        conn_t* conn = new conn_t(pclass, pmemfun);
        this->m_connections.Append(conn);
        pclass->signal_connect(this);
    }

    void emit(arg1_t a1, arg2_t a2, arg3_t a3, arg4_t a4)
    {
        lock_block lock;
        for (size_t i = 0; i < this->m_connections.Count(); i++) {
            this->m_connections.At(i)->emit(a1, a2, a3, a4);
        }
    }

    void operator()(arg1_t a1, arg2_t a2, arg3_t a3, arg4_t a4)
    {
        emit(a1, a2, a3, a4);
    }
};

template<class arg1_t, class arg2_t, class arg3_t, class arg4_t,
class arg5_t>
class signal5 : public _signal_base5<arg1_t, arg2_t, arg3_t,
    arg4_t, arg5_t>
{
public:
    signal5() { }

    signal5(signal5<arg1_t, arg2_t, arg3_t, arg4_t,
        arg5_t>& s)
        : _signal_base5<arg1_t, arg2_t, arg3_t, arg4_t,
        arg5_t>(s)
    { }

    template<class desttype>
        void connect(desttype* pclass, void (desttype::*pmemfun)(arg1_t,
        arg2_t, arg3_t, arg4_t, arg5_t))
    {
        typedef _connection5<desttype, arg1_t, arg2_t, arg3_t, arg4_t, arg5_t> conn_t;
        lock_block lock;
        conn_t* conn = new conn_t(pclass, pmemfun);
        this->m_connections.Append(conn);
        pclass->signal_connect(this);
    }

    void emit(arg1_t a1, arg2_t a2, arg3_t a3, arg4_t a4,
        arg5_t a5)
    {
        lock_block lock;
        for (size_t i = 0; i < this->m_connections.Count(); i++) {
            this->m_connections.At(i)->emit(a1, a2, a3, a4, a5);
        }
    }

    void operator()(arg1_t a1, arg2_t a2, arg3_t a3, arg4_t a4,
        arg5_t a5)
    {
        emit(a1, a2, a3, a4, a5);
    }
};

template<class arg1_t, class arg2_t, class arg3_t, class arg4_t,
class arg5_t, class arg6_t>
class signal6 : public _signal_base6<arg1_t, arg2_t, arg3_t,
    arg4_t, arg5_t, arg6_t>
{
public:
    signal6() { }

    signal6(const signal6<arg1_t, arg2_t, arg3_t, arg4_t,
        arg5_t, arg6_t>& s)
        : _signal_base6<arg1_t, arg2_t, arg3_t, arg4_t,
        arg5_t, arg6_t>(s)
    { }

    template<class desttype>
        void connect(desttype* pclass, void (desttype::*pmemfun)(arg1_t,
        arg2_t, arg3_t, arg4_t, arg5_t, arg6_t))
    {
        typedef _connection6<desttype, arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t> conn_t;
        lock_block lock;
        conn_t* conn = new conn_t(pclass, pmemfun);
        this->m_connections.Append(conn);
        pclass->signal_connect(this);
    }

    void emit(arg1_t a1, arg2_t a2, arg3_t a3, arg4_t a4,
        arg5_t a5, arg6_t a6)
    {
        lock_block lock;
        for (size_t i = 0; i < this->m_connections.Count(); i++) {
            this->m_connections.At(i)->emit(a1, a2, a3, a4, a5, a6);
        }
    }

    void operator()(arg1_t a1, arg2_t a2, arg3_t a3, arg4_t a4,
        arg5_t a5, arg6_t a6)
    {
        emit(a1, a2, a3, a4, a5, a6);
    }
};

template<class arg1_t, class arg2_t, class arg3_t, class arg4_t,
class arg5_t, class arg6_t, class arg7_t>
class signal7 : public _signal_base7<arg1_t, arg2_t, arg3_t,
    arg4_t, arg5_t, arg6_t, arg7_t>
{
public:
    signal7() { }

    signal7(signal7<arg1_t, arg2_t, arg3_t, arg4_t,
        arg5_t, arg6_t, arg7_t>& s)
        : _signal_base7<arg1_t, arg2_t, arg3_t, arg4_t,
        arg5_t, arg6_t, arg7_t>(s)
    { }

    template<class desttype>
        void connect(desttype* pclass, void (desttype::*pmemfun)(arg1_t,
        arg2_t, arg3_t, arg4_t, arg5_t, arg6_t, 
        arg7_t))
    {
        typedef _connection7<desttype, arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t, arg7_t> conn_t;
        lock_block lock;
        conn_t* conn = new conn_t(pclass, pmemfun);
        this->m_connections.Append(conn);
        pclass->signal_connect(this);
    }

    void emit(arg1_t a1, arg2_t a2, arg3_t a3, arg4_t a4, arg5_t a5, arg6_t a6, arg7_t a7)
    {
        lock_block lock;
        for (size_t i = 0; i < this->m_connections.Count(); i++) {
            this->m_connections.At(i)->emit(a1, a2, a3, a4, a5, a6, a7);
        }
    }

    void operator()(arg1_t a1, arg2_t a2, arg3_t a3, arg4_t a4, arg5_t a5, arg6_t a6, arg7_t a7)
    {
        emit(a1, a2, a3, a4, a5, a6, a7);
    }
};

template<class arg1_t, class arg2_t, class arg3_t, class arg4_t,
class arg5_t, class arg6_t, class arg7_t, class arg8_t>
class signal8 : public _signal_base8<arg1_t, arg2_t, arg3_t,
    arg4_t, arg5_t, arg6_t, arg7_t, arg8_t>
{
public:
    signal8() { }

    signal8(signal8<arg1_t, arg2_t, arg3_t, arg4_t,
        arg5_t, arg6_t, arg7_t, arg8_t>& s)
        : _signal_base8<arg1_t, arg2_t, arg3_t, arg4_t,
        arg5_t, arg6_t, arg7_t, arg8_t>(s)
    { }

    template<class desttype>
        void connect(desttype* pclass, void (desttype::*pmemfun)(arg1_t,
        arg2_t, arg3_t, arg4_t, arg5_t, arg6_t, 
        arg7_t, arg8_t))
    {
        typedef _connection8<desttype, arg1_t, arg2_t, arg3_t, arg4_t, arg5_t, arg6_t, arg7_t, arg8_t> conn_t;
        lock_block lock;
        conn_t* conn = new conn_t(pclass, pmemfun);
        this->m_connections.Append(conn);
        pclass->signal_connect(this);
    }

    void emit(arg1_t a1, arg2_t a2, arg3_t a3, arg4_t a4,
        arg5_t a5, arg6_t a6, arg7_t a7, arg8_t a8)
    {
        lock_block lock;
        for (size_t i = 0; i < this->m_connections.Count(); i++) {
            this->m_connections.At(i)->emit(a1, a2, a3, a4, a5, a6, a7, a8);
        }
    }

    void operator()(arg1_t a1, arg2_t a2, arg3_t a3, arg4_t a4,
        arg5_t a5, arg6_t a6, arg7_t a7, arg8_t a8)
    {
        emit(a1, a2, a3, a4, a5, a6, a7, a8);
    }
};

}; // namespace sigslot
