#ifndef __APP_STAT_H_
#define __APP_STAT_H_

static inline uint64_t
rdtscll(void)
{
  unsigned long a, d, c;

  __asm__ __volatile__("rdtsc" : "=a" (a), "=d" (d), "=c" (c) : : );

  return ((uint64_t)d << 32) | (uint64_t)a;
}

struct stat_counter
{
	uint64_t cnt;
	uint64_t sum;
	uint64_t max;
	uint64_t min;
};

static inline void 
InitStatCounter(struct stat_counter *counter)
{
	counter->cnt = 0;
	counter->sum = 0;
	counter->max = 0;
	counter->min = 0;
}
/*----------------------------------------------------------------------------*/
static inline void 
UpdateStatCounter(struct stat_counter *counter, int64_t value)
{
	counter->cnt++;
	counter->sum += value;
	if (value > counter->max)
		counter->max = value;
	if (counter->min == 0 || value < counter->min)
		counter->min = value;
}
/*----------------------------------------------------------------------------*/
static inline uint64_t 
GetAverageStat(struct stat_counter *counter)
{
	return counter->cnt ? (counter->sum / counter->cnt) : 0;
}
/*----------------------------------------------------------------------------*/
static inline int64_t 
TimeDiffUs(struct timeval *t2, struct timeval *t1)
{
	return (t2->tv_sec - t1->tv_sec) * 1000000 + 
			(int64_t)(t2->tv_usec - t1->tv_usec);
}

#endif
