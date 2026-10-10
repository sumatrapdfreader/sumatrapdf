/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// ng: what CrashHandler_posix.cpp needs from Mach: the stack of every thread
// and the images those stacks run through. Frames are written as
// "module + offset" and each module with its Mach-O UUID, which is all
// cmd/ng-crashes.ts needs to symbolicate them with atos and the build's .dSYM.

#include "base/Base.h"

#include "base/CrashHandler.h"

#include <dlfcn.h>
#include <mach/mach.h>
#include <mach-o/loader.h>
#include <pthread.h>
#include <signal.h>
#include <sys/sysctl.h>
#include <sys/ucontext.h>

#if defined(__arm64__)
using ThreadState = arm_thread_state64_t;
constexpr thread_state_flavor_t kThreadFlavor = ARM_THREAD_STATE64;
constexpr mach_msg_type_number_t kThreadStateCount = ARM_THREAD_STATE64_COUNT;
#define kArchName "arm64"
#else
using ThreadState = x86_thread_state64_t;
constexpr thread_state_flavor_t kThreadFlavor = x86_THREAD_STATE64;
constexpr mach_msg_type_number_t kThreadStateCount = x86_THREAD_STATE64_COUNT;
#define kArchName "x86_64"
#endif

constexpr int kMaxFrames = 96;
constexpr int kMaxModules = 64;
constexpr int kMaxThreadName = 64;
constexpr int kUuidLen = 16;

struct Regs {
    uintptr_t pc;
    uintptr_t sp;
    uintptr_t fp;
    uintptr_t lr; // 0 on x86_64
};

struct Module {
    const mach_header_64* hdr;
    const char* path;
};

static Module gModules[kMaxModules];
static int gModuleCount = 0;

static Regs RegsFromState(const ThreadState& s) {
    Regs r{};
#if defined(__arm64__)
    r.pc = (uintptr_t)arm_thread_state64_get_pc(s);
    r.sp = (uintptr_t)arm_thread_state64_get_sp(s);
    r.fp = (uintptr_t)arm_thread_state64_get_fp(s);
    r.lr = (uintptr_t)arm_thread_state64_get_lr(s);
#else
    r.pc = (uintptr_t)s.__rip;
    r.sp = (uintptr_t)s.__rsp;
    r.fp = (uintptr_t)s.__rbp;
#endif
    return r;
}

// a wild frame pointer must end the walk, not fault inside the handler
static bool ReadMem(uintptr_t addr, void* dst, size_t n) {
    vm_size_t got = 0;
    kern_return_t kr = vm_read_overwrite(mach_task_self(), (vm_address_t)addr, n, (vm_address_t)dst, &got);
    return kr == KERN_SUCCESS && got == n;
}

// frame records are {caller's fp, return address}, linked through fp
static int WalkStack(const Regs& r, uintptr_t* frames, int maxFrames) {
    int n = 0;
    frames[n++] = r.pc;
    if (r.pc == 0) {
        // call through a null pointer: no frame was set up, the caller is
        // in lr (arm64) or on top of the stack (x86_64)
        uintptr_t caller = r.lr;
        if (!caller) {
            ReadMem(r.sp, &caller, sizeof(caller));
        }
        if (caller) {
            frames[n++] = caller;
        }
    }
    uintptr_t fp = r.fp;
    while (fp && n < maxFrames) {
        uintptr_t rec[2];
        if (!ReadMem(fp, rec, sizeof(rec)) || rec[1] == 0) {
            break;
        }
        frames[n++] = rec[1];
        if (rec[0] <= fp) {
            break;
        }
        fp = rec[0];
    }
    return n;
}

static void AppendHex(str::Builder& b, u64 v, int minDigits = 1) {
    char buf[20];
    int n = 0;
    do {
        buf[n++] = "0123456789abcdef"[v & 0xf];
        v >>= 4;
    } while (v != 0 || n < minDigits);
    b.Append(StrL("0x"));
    while (n > 0) {
        b.AppendChar(buf[--n]);
    }
}

static Str BaseName(const char* path) {
    const char* slash = strrchr(path, '/');
    return Str(slash ? slash + 1 : path);
}

static void RememberModule(const Dl_info& info) {
    auto hdr = (const mach_header_64*)info.dli_fbase;
    for (int i = 0; i < gModuleCount; i++) {
        if (gModules[i].hdr == hdr) {
            return;
        }
    }
    if (gModuleCount < kMaxModules) {
        gModules[gModuleCount++] = {hdr, info.dli_fname};
    }
}

// "3 0x0000000104a1b2c4 SumatraPDF + 0x1b2c4 (CrashMe() + 12)"
static void AppendFrame(str::Builder& b, int idx, uintptr_t addr) {
    b.Append(fmt("%d ", idx));
    AppendHex(b, addr, 16);
    Dl_info info{};
    if (addr == 0 || dladdr((void*)addr, &info) == 0 || !info.dli_fbase) {
        b.Append(StrL(" ???\n"));
        return;
    }
    RememberModule(info);
    b.AppendChar(' ');
    b.Append(BaseName(info.dli_fname));
    b.Append(StrL(" + "));
    AppendHex(b, addr - (uintptr_t)info.dli_fbase);
    if (info.dli_sname) {
        b.Append(StrL(" ("));
        b.Append(Str(info.dli_sname));
        b.Append(fmt(" + %d)", (int)(addr - (uintptr_t)info.dli_saddr)));
    }
    b.AppendChar('\n');
}

static void AppendStack(str::Builder& b, const Regs& r) {
    uintptr_t frames[kMaxFrames];
    int n = WalkStack(r, frames, kMaxFrames);
    for (int i = 0; i < n; i++) {
        AppendFrame(b, i, frames[i]);
    }
}

static void AppendThreadHeader(str::Builder& b, thread_t thread, bool isCrashed) {
    b.Append(fmt("\n--- thread %d", (int)thread));
    char name[kMaxThreadName] = {};
    pthread_t pt = pthread_from_mach_thread_np(thread);
    if (pt && pthread_getname_np(pt, name, sizeof(name)) == 0 && name[0]) {
        b.Append(StrL(" '"));
        b.Append(Str(name));
        b.AppendChar('\'');
    }
    b.Append(isCrashed ? StrL(" (crashed) ---\n") : StrL(" ---\n"));
}

static void AppendOtherThreads(str::Builder& b, thread_t self) {
    thread_act_array_t threads = nullptr;
    mach_msg_type_number_t nThreads = 0;
    if (task_threads(mach_task_self(), &threads, &nThreads) != KERN_SUCCESS) {
        return;
    }
    for (mach_msg_type_number_t i = 0; i < nThreads; i++) {
        thread_t t = threads[i];
        if (t != self && thread_suspend(t) == KERN_SUCCESS) {
            ThreadState state{};
            mach_msg_type_number_t count = kThreadStateCount;
            if (thread_get_state(t, kThreadFlavor, (thread_state_t)&state, &count) == KERN_SUCCESS) {
                AppendThreadHeader(b, t, false);
                AppendStack(b, RegsFromState(state));
            }
            // a debug report is not the end of the process
            thread_resume(t);
        }
        mach_port_deallocate(mach_task_self(), t);
    }
    vm_deallocate(mach_task_self(), (vm_address_t)threads, nThreads * sizeof(thread_t));
}

static bool FindUuid(const mach_header_64* hdr, u8* uuidOut) {
    if (hdr->magic != MH_MAGIC_64) {
        return false;
    }
    auto cmd = (const load_command*)(hdr + 1);
    for (u32 i = 0; i < hdr->ncmds; i++) {
        if (cmd->cmd == LC_UUID) {
            memcpy(uuidOut, ((const uuid_command*)cmd)->uuid, kUuidLen);
            return true;
        }
        cmd = (const load_command*)((const char*)cmd + cmd->cmdsize);
    }
    return false;
}

// "SumatraPDF 0x0000000104a00000 4C4C44F5-5555-3144-A1B2-... /path/to/SumatraPDF"
static void AppendModules(str::Builder& b) {
    b.Append(StrL("\n--- modules ---\n"));
    for (int i = 0; i < gModuleCount; i++) {
        const Module& m = gModules[i];
        b.Append(BaseName(m.path));
        b.AppendChar(' ');
        AppendHex(b, (uintptr_t)m.hdr, 16);
        b.AppendChar(' ');
        u8 uuid[kUuidLen];
        if (FindUuid(m.hdr, uuid)) {
            for (int j = 0; j < kUuidLen; j++) {
                if (j == 4 || j == 6 || j == 8 || j == 10) {
                    b.AppendChar('-');
                }
                b.AppendChar("0123456789ABCDEF"[uuid[j] >> 4]);
                b.AppendChar("0123456789ABCDEF"[uuid[j] & 0xf]);
            }
        } else {
            b.Append(StrL("(no uuid)"));
        }
        b.AppendChar(' ');
        b.Append(Str(m.path));
        b.AppendChar('\n');
    }
}

static Str SignalName(int sig) {
    switch (sig) {
        case SIGSEGV:
            return StrL("SIGSEGV");
        case SIGBUS:
            return StrL("SIGBUS");
        case SIGILL:
            return StrL("SIGILL");
        case SIGFPE:
            return StrL("SIGFPE");
        case SIGABRT:
            return StrL("SIGABRT");
        case SIGTRAP:
            return StrL("SIGTRAP");
    }
    return StrL("signal");
}

// "Signal: SIGSEGV (11) code 2 addr 0x0"
void MacAppendSignalInfo(str::Builder& b, int sig, void* sigInfo) {
    auto si = (siginfo_t*)sigInfo;
    b.Append(fmt("Signal: %s (%d)", SignalName(sig), sig));
    if (si) {
        b.Append(fmt(" code %d addr ", si->si_code));
        AppendHex(b, (uintptr_t)si->si_addr);
    }
    b.AppendChar('\n');
}

// Stacks of all threads, the calling one first. uctx is the signal handler's
// ucontext_t, so that the stack is the fault's and not the handler's; null
// for a debug report, which walks from the caller.
NO_INLINE void MacAppendStacks(str::Builder& b, void* uctx) {
    gModuleCount = 0;
    Regs regs{};
    if (uctx) {
        regs = RegsFromState(((ucontext_t*)uctx)->uc_mcontext->__ss);
    } else {
        regs.pc = (uintptr_t)__builtin_return_address(0);
        // our frame record holds the caller's fp, which pairs with that pc
        regs.fp = *(uintptr_t*)__builtin_frame_address(0);
    }
    thread_t self = mach_thread_self();
    AppendThreadHeader(b, self, uctx != nullptr);
    AppendStack(b, regs);
    AppendOtherThreads(b, self);
    mach_port_deallocate(mach_task_self(), self);
    AppendModules(b);
}

static TempStr SysctlStrTemp(const char* name) {
    char buf[256] = {};
    size_t size = sizeof(buf) - 1;
    if (sysctlbyname(name, buf, &size, nullptr, 0) != 0) {
        return {};
    }
    return str::DupTemp(Str(buf));
}

TempStr MacSysInfoTemp() {
    str::Builder b(GetTempArena());
    b.Append(fmt("Os: macOS %s (%s)\n", SysctlStrTemp("kern.osproductversion"), SysctlStrTemp("kern.osversion")));
    b.Append(fmt("Arch: %s\n", StrL(kArchName)));
    b.Append(fmt("Machine: %s\n", SysctlStrTemp("hw.model")));
    b.Append(fmt("Cpu: %s\n", SysctlStrTemp("machdep.cpu.brand_string")));
    u64 memSize = 0;
    size_t size = sizeof(memSize);
    if (sysctlbyname("hw.memsize", &memSize, &size, nullptr, 0) == 0) {
        b.Append(fmt("Memory: %d MB\n", (int)(memSize / (1024 * 1024))));
    }
    return ToStr(b);
}
