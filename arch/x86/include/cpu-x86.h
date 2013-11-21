#ifndef _CPU_X86_H
#define _CPU_X86_H

typedef struct {
	char manufacturer_string[13];
	int max_basic_input_val;
	int max_ext_input_val;
	int features_ecx, features_edx;
	int ext_features_ecx, ext_features_edx;
	char stepping, model, family, type; 
	char cache_line_size, logical_processors, lapic_id;
	char cpu_brand[49];
} cpuid_t;

#include <cpu-x86_common.h>

#endif
