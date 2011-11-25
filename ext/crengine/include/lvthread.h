/** \file lvptrvec.h
    \brief pointer vector template

    Multyplatform threads support.

    CoolReader Engine

    (c) Vadim Lopatin, 2000-2006
    This source code is distributed under the terms of
    GNU General Public License.

    See LICENSE file for details.

*/

#ifndef __LVTHREAD_H_INCLUDED__
#define __LVTHREAD_H_INCLUDED__

#include <stdlib.h>

#if (CR_USE_THREADS==1)

#if defined(_LINUX)
#include <pthread.h>

class LVThread {
private:
    pthread_t _thread;
    bool _valid;
    bool _stopped;
    static void * start_routine(void * param)
    {
        LVThread * thread = (LVThread*)param;
        thread->run();
        thread->_stopped = true;
        return NULL;
    }
protected:
    virtual void run()
    {
    }
public:
    virtual void start()
    {
        _valid = (pthread_create(&_thread, NULL, &start_routine, this) == 0);
    }
    bool stopped()
    {
        return _stopped;
    }
    void join()
    {
        if ( _valid ) {
            void * res;
            pthread_join( _thread, &res );
            _valid = false;
        }
    }
    LVThread()
    : _valid(false), _stopped(false)
    {
    }
    virtual ~LVThread()
    {
        if ( _valid ) {
            pthread_detach( _thread );
        }
    }
};

class LVMutex {
private:
    pthread_mutex_t _mutex;
    bool _valid;
public:
    LVMutex()
    {
        _valid = ( pthread_mutex_init(&_mutex, NULL) !=0 );
    }
    ~LVMutex()
    {
        if ( _valid )
            pthread_mutex_destroy( &_mutex );
    }
    bool lock()
    {
        if ( _valid )
            return (pthread_mutex_lock( &_mutex )==0);
        return false;
    }
    bool trylock()
    {
        if ( _valid )
            return (pthread_mutex_trylock( &_mutex )==0);
        return false;
    }
    void unlock()
    {
        if ( _valid )
            pthread_mutex_unlock( &_mutex );
    }
};

#elif defined(_WIN32)

class LVThread {
    private:
        HANDLE _thread;
        bool _valid;
        bool _stopped;
        DWORD _id;

        static DWORD WINAPI start_routine(
                LPVOID param
                                      )
        {
            LVThread * thread = (LVThread*)param;
            thread->run();
            thread->_stopped = true;
            return 0;
        }
    protected:
        virtual void run()
        {
        }
    public:
        virtual void start()
        {
            ResumeThread( _thread );
        }
        bool stopped()
        {
            return _stopped;
        }
        void join()
        {
            if ( _valid ) {
                WaitForSingleObject( _thread, INFINITE );
                _valid = false;
            }
        }
        LVThread()
        : _stopped(false)
        {
            _thread = CreateThread(NULL, 0, start_routine, this, CREATE_SUSPENDED, &_id );
            _valid = _thread != NULL;
        }
        virtual ~LVThread()
        {
            if ( _valid ) {
                CloseHandle( _thread );
            }
        }
};

class LVMutex {
    private:
        HANDLE _mutex;
        bool _valid;
    public:
        LVMutex()
        {
            _mutex = CreateMutex( NULL, FALSE, NULL );
            _valid = (_mutex != NULL);
        }
        ~LVMutex()
        {
            if ( _valid )
                CloseHandle( _mutex );
        }
        bool lock()
        {
            if ( _valid ) {
                return WaitForSingleObject( _mutex, INFINITE ) == WAIT_OBJECT_0;
            }
            return false;
        }
        bool trylock()
        {
            if ( _valid ){
                return WaitForSingleObject( _mutex, 0 ) == WAIT_OBJECT_0;
            }
            return false;
        }
        void unlock()
        {
            if ( _valid )
                ReleaseMutex( _mutex );
        }
};


#endif

#else // CR_USE_THREADS

class LVThread {
    protected:
        virtual void run()
        {
        }
    public:
        virtual void start()
        {
            // fake: simulate execution here
            run();
        }
        bool stopped()
        {
            return true;
        }
        void join()
        {
        }
        LVThread()
        {
        }
        virtual ~LVThread()
        {
        }
};


class LVMutex {
    public:
        LVMutex()
        {
        }
        ~LVMutex()
        {
        }
        bool lock()
        {
            return true;
        }
        bool trylock()
        {
            return true;
        }
        void unlock()
        {
        }
};

#endif

class LVLock {
    private:
        LVMutex &_mutex;
        bool _locked;
    public:
        LVLock( LVMutex &mutex )
        : _mutex(mutex)
        {
            _locked = _mutex.lock();
        }
        ~LVLock()
        {
            if ( _locked )
                _mutex.unlock();
        }
};


#endif
