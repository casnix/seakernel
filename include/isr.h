#include <config.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
#include <isr-x86.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
#include <isr-x86_64.h>
#endif