#include <sea/config.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
#include <asm/bitops-x86_common.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
#include <asm/bitops-x86_common.h>
#endif
