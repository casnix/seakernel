#ifndef __SEA_CPU_TIME_H
#define __SEA_CPU_TIME_H

typedef long clock_t;

struct tm
{
	int	tm_sec;
	int	tm_min;
	int	tm_hour;
	int	tm_mday;
	int	tm_mon;
	int	tm_year;
	int	tm_wday;
	int	tm_yday;
	int	tm_isdst;
};

struct timeval {
	int tv_sec;
	int tv_usec;
};

struct tms {
	clock_t tms_utime;
	clock_t tms_stime;
	clock_t tms_cutime;
	clock_t tms_cstime;
};

unsigned long long arch_time_get_epoch();
unsigned long long time_get_epoch();
void arch_time_get(struct tm *now);
void time_get(struct tm *);

#endif

