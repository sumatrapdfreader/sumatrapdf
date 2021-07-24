#include "../../zutil.h"

#if defined(__linux__)
#  include <sys/auxv.h>
#  ifdef ARM_ASM_HWCAP
#    include <asm/hwcap.h>
#  endif
#elif defined(__FreeBSD__) && defined(__aarch64__)
#  include <machine/armreg.h>
#  ifndef ID_AA64ISAR0_CRC32_VAL
#    define ID_AA64ISAR0_CRC32_VAL ID_AA64ISAR0_CRC32
#  endif
#elif defined(__APPLE__)
#  include <sys/sysctl.h>
#elif defined(_WIN32)
#  include <winapifamily.h>
#endif

static int arm_has_crc32() {
#if defined(__linux__) && defined(ARM_AUXV_HAS_CRC32)
#  ifdef HWCAP_CRC32
    return (getauxval(AT_HWCAP) & HWCAP_CRC32) != 0 ? 1 : 0;
#  else
    return (getauxval(AT_HWCAP2) & HWCAP2_CRC32) != 0 ? 1 : 0;
#  endif
#elif defined(__FreeBSD__) && defined(__aarch64__)
    return getenv("QEMU_EMULATING") == NULL
      && ID_AA64ISAR0_CRC32_VAL(READ_SPECIALREG(id_aa64isar0_el1)) >= ID_AA64ISAR0_CRC32_BASE;
#elif defined(__APPLE__)
    int hascrc32;
    size_t size = sizeof(hascrc32);
    return sysctlbyname("hw.optional.armv8_crc32", &hascrc32, &size, NULL, 0) == 0
      && hascrc32 == 1;
#elif defined(ARM_NOCHECK_ACLE)
    return 1;
#else
    return 0;
#endif
}

/* AArch64 has neon. */
#if !defined(__aarch64__) && !defined(_M_ARM64)
static inline int arm_has_neon() {
#if defined(__linux__) && defined(ARM_AUXV_HAS_NEON)
#  ifdef HWCAP_ARM_NEON
    return (getauxval(AT_HWCAP) & HWCAP_ARM_NEON) != 0 ? 1 : 0;
#  else
    return (getauxval(AT_HWCAP) & HWCAP_NEON) != 0 ? 1 : 0;
#  endif
#elif defined(__APPLE__)
    int hasneon;
    size_t size = sizeof(hasneon);
    return sysctlbyname("hw.optional.neon", &hasneon, &size, NULL, 0) == 0
      && hasneon == 1;
#elif defined(_M_ARM) && defined(WINAPI_FAMILY_PARTITION)
#  if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_PHONE_APP)
    return 1; /* Always supported */
#  endif
#endif

#if defined(ARM_NOCHECK_NEON)
    return 1;
#else
    return 0;
#endif
}
#endif

Z_INTERNAL int arm_cpu_has_neon;
Z_INTERNAL int arm_cpu_has_crc32;

void Z_INTERNAL arm_check_features(void) {
#if defined(__aarch64__) || defined(_M_ARM64)
    arm_cpu_has_neon = 1; /* always available */
#else
    arm_cpu_has_neon = arm_has_neon();
#endif
    arm_cpu_has_crc32 = arm_has_crc32();
}
