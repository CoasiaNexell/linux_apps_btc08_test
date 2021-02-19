#ifndef _UTILS_H_
#define _UTILS_H_

#include <time.h>
#include <sys/time.h>

#define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))

void tvtime(struct timeval *tv);
int timeval_to_ms(struct timeval *tv);
void tvtimer_diff(struct timeval *end, struct timeval *start, struct timeval *res);

void tstimer_time(struct timespec *ts);
int tstimer_to_ms(struct timespec *ts);
void tstimer_diff(struct timespec *end, struct timespec *start, struct timespec *res);
int get_current_ms();

void HexDump( const char *name, const void *data, int32_t size );

#endif // _UTILS_H_
