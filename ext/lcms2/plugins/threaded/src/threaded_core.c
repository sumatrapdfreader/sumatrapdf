//---------------------------------------------------------------------------------
//
//  Little Color Management System, multithread extensions
//  Copyright (c) 1998-2022 Marti Maria Saguer, all rights reserved
//
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//---------------------------------------------------------------------------------

#include "threaded_internal.h"

// This is the threading support. Unfortunately, it has to be platform-dependent because 
// windows does not support pthreads. 
#ifdef CMS_IS_WINDOWS_

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>

// To pass parameter to the thread
typedef struct
{
    _cmsTransform2Fn worker;
    _cmsWorkSlice* param;

} thread_adaptor_param;


// This is an adaptor to the native thread on windows
static
DWORD WINAPI thread_adaptor(LPVOID p)
{
    thread_adaptor_param* ap = (thread_adaptor_param*)p;
    _cmsWorkSlice* s = ap->param;

    ap->worker(s->CMMcargo, s->InputBuffer, s->OutputBuffer, 
               s->PixelsPerLine, s->LineCount, s->Stride);
    _cmsFree(0, p);
    return 0;
}

// This function creates a thread and executes it. The thread calls the worker function
// with the given parameters.
cmsHANDLE _cmsThrCreateWorker(cmsContext ContextID, _cmsTransform2Fn worker, _cmsWorkSlice* param)
{
    DWORD ThreadID;
    thread_adaptor_param* p;
    HANDLE handle;

    p = (thread_adaptor_param*)_cmsMalloc(0, sizeof(thread_adaptor_param));
    if (p == NULL) return NULL;

    p->worker = worker;
    p->param = param;

    handle  = CreateThread(NULL, 0, thread_adaptor, (LPVOID) p, 0, &ThreadID);
    if (handle == NULL)
    {
        cmsSignalError(ContextID, cmsERROR_UNDEFINED, "Cannot create thread");
    }

    return (cmsHANDLE)handle;
}

// Waits until given thread is ended
void _cmsThrJoinWorker(cmsContext ContextID, cmsHANDLE hWorker)
{
    if (WaitForSingleObject((HANDLE)hWorker, INFINITE) != WAIT_OBJECT_0)
    {
        cmsSignalError(ContextID, cmsERROR_UNDEFINED, "Cannot join thread");
    }
}

// Returns the ideal number of threads the system can run
cmsInt32Number _cmsThrIdealThreadCount(void)
{
    SYSTEM_INFO sysinfo;

    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors; //Returns the number of processors in the system.
}

#else

// Rest of the wide world
#include <pthread.h>
#include <unistd.h>

// To pass parameter to the thread
typedef struct
{
    _cmsTransform2Fn worker;
    _cmsWorkSlice* param;

} thread_adaptor_param;


// This is the native thread on pthread
static
void* thread_adaptor(void* p)
{
    thread_adaptor_param* ap = (thread_adaptor_param*)p;
    _cmsWorkSlice* s = ap->param;

    ap->worker(s->CMMcargo, s->InputBuffer, s->OutputBuffer,
               s->PixelsPerLine, s->LineCount, s->Stride);
    _cmsFree(0, p);

    return NULL;
}

// This function creates a thread and executes it. The thread calls the worker function
// with the given parameters.
cmsHANDLE _cmsThrCreateWorker(cmsContext ContextID, _cmsTransform2Fn worker, _cmsWorkSlice* param)
{
    pthread_t threadId;
    thread_adaptor_param* p;

    p = (thread_adaptor_param*)_cmsMalloc(0, sizeof(thread_adaptor_param));
    if (p == NULL) return NULL;

    p->worker = worker;
    p->param = param;

    int err = pthread_create(&threadId, NULL, thread_adaptor, p);
    if (err != 0)
    {
        cmsSignalError(ContextID, cmsERROR_UNDEFINED, "Cannot create thread [pthread error %d]", err);
        return NULL;
    }
    else
        return (cmsHANDLE) threadId;
}

// Waits until given thread is ended
void _cmsThrJoinWorker(cmsContext ContextID, cmsHANDLE hWorker)
{
    int err = pthread_join((pthread_t)hWorker, NULL);
    if (err != 0)
    {
        cmsSignalError(ContextID, cmsERROR_UNDEFINED, "Cannot join thread [pthread error %d]", err); 
    }
}

cmsInt32Number _cmsThrIdealThreadCount(void)
{    
    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (cores == -1L)
        return 1;
    else
        return (cmsInt32Number)cores;
}

#endif
