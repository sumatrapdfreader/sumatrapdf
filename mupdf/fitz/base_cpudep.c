/*
run-time cpu feature detection code
mm, alphabet soup...

Glenn Kennard <d98gk@efd.lth.se>
*/

#include "fitz.h"

/* global run-time constant */
unsigned fz_cpuflags = 0;

#ifndef HAVE_CPUDEP

void fz_accelerate(void)
{
}

void fz_cpudetect(void)
{
}

#else

#ifndef WIN32
#include <signal.h> /* signal/sigaction */
#include <setjmp.h> /* sigsetjmp/siglongjmp */
#endif

/*
#ifdef WIN32
#define sigjmp_buf jmp_buf
#define sigsetjmp(a,b) setjmp(a)
#define siglongjmp longjmp
#endif
*/

typedef struct {
	void (*test)(void);
	const unsigned flag;
	const char *name;
} featuretest;


#if defined(ARCH_X86) || defined(ARCH_X86_64)

#ifdef __GNUC__
static void mmx(void)
{ __asm__ ("pand %mm0, %mm0\n\t"); }

static void m3dnow(void)
{ __asm__ ("pavgusb %mm0, %mm0\n\t"); }

static void mmxext(void) /* aka  Extended 3DNow! */
{ __asm__ ("pmaxsw %mm0, %mm0\n\t"); }

static void sse(void)
{ __asm__ ("andps %xmm0, %xmm0\n\t"); }

static void sse2(void)
{ __asm__ ("andpd %xmm0, %xmm0\n\t"); }

/*
static void sse3(void)
{ __asm__ ("haddps %%xmm0, %%xmm0\n\t" : : : "%xmm0"); }
*/
#else
static void mmx(void)
{ __asm pand mm0, mm0; }

static void m3dnow(void)
{ __asm pavgusb mm0, mm0; }

static void mmxext(void) /* aka  Extended 3DNow! */
{ __asm pmaxsw mm0, mm0; }

static void sse(void)
{ __asm andps xmm0, xmm0; }

static void sse2(void)
{ __asm andpd xmm0, xmm0; }
#endif


#ifdef ARCH_X86_64
static void amd64(void)
#ifdef __GNUC__
{ __asm__ ("and %rax, %rax\n\t"); }
#else
{ __asm and rax, rax; }
#endif
#endif


static const featuretest features[] = {
	{ mmx, HAVE_MMX, "mmx" },
	{ m3dnow, HAVE_3DNOW, "3dnow" },
	{ mmxext, HAVE_MMXEXT, "mmxext" },
	{ sse, HAVE_SSE, "sse" },
	{ sse2, HAVE_SSE2, "sse2" },
	/*	{ sse3, HAVE_SSE3, "sse3" }, */
#ifdef ARCH_X86_64
	{ amd64, HAVE_AMD64, "amd64" }
#endif
};

#endif


#if defined(ARCH_SPARC)
static void vis(void)
/*
Stupidly Sun assembler decides to mark anything using VIS instructions in the
ELF header, which causes link errors if using the following (which also
requires passing -xarch=v8plusa|v9a passed to the assembler so it accepts the
instruction in the first place, v9a for 64 bit binaries):

{ __asm__ ("fand %f8, %f8, %f8\n\t"); }

so instead we just emit the opcode directly, bypassing that check.
*/
{ __asm__ (".word 0x91B20E08"); }

/* static void vis2(void)
{ __asm__ ("edge8n %%l0, %%l0, %%l0\n\t" : : : "%l0"); } */

static const featuretest features[] = {
	{ vis, HAVE_VIS, "vis" }
};

#endif


#if defined(ARCH_PPC)

static void altivec(void)
{ __asm__ ("vand v0, v0, v0\n\t"); }


static const featuretest features[] = {
	{ altivec, HAVE_ALTIVEC, "altivec" },
};

#endif

static int
enabled(char *env, const char *ext)
{
	int len;
	char *s;
	if (!env)
		return 1;
	len = strlen(ext);
	s = env;
	while ((s = strstr(s, ext)))
	{
		int atstart = s == env || *(s-1) == ',' || *(s-1) == ' ';
		s += len;
		if (atstart && (*s == ' ' || *s == ',' || *s == '\0')) {
			return 1;
		}
	}
	return 0;
}

static void
dumpflags(void)
{
	unsigned f = fz_cpuflags;
	int i, n;

	fputs("detected cpu features:", stdout);
	n = 0;
	for (i = 0; i < sizeof(features) / sizeof(featuretest); i++)
	{
		if (f & features[i].flag)
		{
			fputc(' ', stdout);
			fputs(features[i].name, stdout);
			n ++;
		}
	}
	if (!n)
		fputs(" none", stdout);
	fputc('\n', stdout);
}

#ifndef WIN32

static sigjmp_buf jmpbuf;
static volatile sig_atomic_t canjump;

static void
sigillhandler(int sig)
{
	if (!canjump) {
		signal(sig, SIG_DFL);
		raise(sig);
	}

	canjump = 0;
	siglongjmp(jmpbuf, 1);
}

void fz_cpudetect(void)
{
	static int hasrun = 0;

	unsigned flags = 0;
	int i;
	void (*oldhandler)(int) = NULL;
	void (*tmphandler)(int);
	char *env;

	if (hasrun)
		return;
	hasrun = 1;

	env = getenv("CPUACCEL");

	for (i = 0; i < sizeof(features) / sizeof(featuretest); i++)
	{
		canjump = 0;

		tmphandler = signal(SIGILL, sigillhandler);
		if (!oldhandler)
			oldhandler = tmphandler;

		if (sigsetjmp(jmpbuf, 1))
		{
			/* test failed - disable feature */
			flags &= ~features[i].flag;
			continue;
		}

		canjump = 1;

		features[i].test();

#if defined(ARCH_X86) || defined(ARCH_X86_64)
		/* reset mmx/x87 pipeline state */
		if (features[i].flag & (HAVE_MMX | HAVE_3DNOW | HAVE_MMXEXT)) {
			__asm__ __volatile__ ("emms\n\t");
		}
#endif

		/* if we got here the test succeeded */
		if (enabled(env, features[i].name))
			flags |= features[i].flag;
		else
			flags &= ~features[i].flag;
	}

	/* restore previous signal handler */
	signal(SIGILL, oldhandler);

	fz_cpuflags = flags;

#if defined(ARCH_X86) || defined(ARCH_X86_64)
	__asm__ __volatile__ ("emms\n\t");
#endif

	dumpflags();
}

/*
static __attribute__((constructor, used)) void fzcpudetect(void)
{
fz_cpudetect();
}
*/


#else /* WIN32 */

void fz_cpudetect(void)
{
	static int hasrun = 0;

	unsigned flags = 0;
	int i;
	char *env;

	if (hasrun)
		return;
	hasrun = 1;

	env = getenv("CPUACCEL");

	for (i = 0; i < sizeof(features) / sizeof(featuretest); i++)
	{
		__try
		{
			features[i].test();
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			/* test failed - disable feature */
			flags &= ~features[i].flag;
			continue;
		}

#if defined(ARCH_X86) || defined(ARCH_X86_64)
		if (features[i].flag & (HAVE_MMX | HAVE_3DNOW | HAVE_MMXEXT)) {
			/* reset mmx/x87 pipeline state */
			__asm emms;
		}
#endif

		/* if we got here the test succeeded */
		if (enabled(env, features[i].name))
			flags |= features[i].flag;
		else
			flags &= ~features[i].flag;
	}

	fz_cpuflags = flags;

	dumpflags();
}


#endif

#endif

