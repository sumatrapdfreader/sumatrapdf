
#include <windows.h>
#include "lcms2_plugin.h"

static cmsContext ctx;
static cmsHPROFILE prof_cmyk, prof_rgb;
static volatile int rc = 0;


static
void* MyMtxCreate(cmsContext id)
{
   return (void*) CreateMutex( NULL, FALSE, NULL);   
}

static
void MyMtxDestroy(cmsContext id, void* mtx)
{
    CloseHandle((HANDLE) mtx);
}

static
cmsBool MyMtxLock(cmsContext id, void* mtx)
{
    WaitForSingleObject((HANDLE) mtx, INFINITE);
    return TRUE;
}

static
void MyMtxUnlock(cmsContext id, void* mtx)
{
    ReleaseMutex((HANDLE) mtx);
}


static cmsPluginMutex MutexPluginSample = {
                           
     { cmsPluginMagicNumber, 2060, cmsPluginMutexSig, NULL}, 

     MyMtxCreate,  MyMtxDestroy,  MyMtxLock,  MyMtxUnlock                       
};


static DWORD WINAPI one_thread(LPVOID lpParameter)
{
    int i, j;
    cmsUInt8Number rgb[3*1000];
    cmsUInt8Number cmyk[4*1000];

    Sleep(rand() % 500 );
    cmsHTRANSFORM xform = cmsCreateTransformTHR(ctx, prof_rgb, TYPE_RGB_8, prof_cmyk, TYPE_CMYK_8, 0, 0);

    for (i=0; i < 100000; i++) {

        for (j=0; j < 1000; j++) 
        {
            rgb[j * 3    ] = 189;
            rgb[j * 3 + 1] = 100;
            rgb[j * 3 + 2] = 75;
        }
        cmsDoTransform(xform, rgb, cmyk, 1000);
        for (j=0; j < 1000; j++) 
        {
            if (cmyk[j * 4 ] != 37 ||
                cmyk[j * 4 + 1 ] != 188 ||
                cmyk[j * 4 + 2 ] != 195 ||
                cmyk[j * 4 + 3 ] != 7) 
            {
                OutputDebugString(L"ERROR\n"); 
                rc = 1;
            }

        }

    }
        
    cmsDeleteTransform(xform);

    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow)
{
    int i;
    cmsContext ctx;

    OutputDebugString(L"Test in progress...\n"); 

    ctx = cmsCreateContext(NULL, 0);

    prof_cmyk = cmsOpenProfileFromFileTHR(ctx, "USWebCoatedSWOP.icc", "r");
    prof_rgb = cmsOpenProfileFromFileTHR(ctx, "AdobeRGB1998.icc","r");
   

#define NWORKERS 10

    HANDLE workers[NWORKERS];


    for (int i=0; i<NWORKERS; ++i)
    {
        DWORD threadid;

        workers[i] = CreateThread(NULL,0,one_thread,NULL,0,&threadid);
    }

    WaitForMultipleObjects(NWORKERS,workers,TRUE,INFINITE);

    for ( i=0;i<NWORKERS;++i)
        CloseHandle(workers[i]);


    cmsCloseProfile(prof_rgb);
    cmsCloseProfile(prof_cmyk);
    cmsDeleteContext(ctx);

    OutputDebugString(L"Test Done\n"); 

    return rc;
}
