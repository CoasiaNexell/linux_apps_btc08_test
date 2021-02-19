#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "Utils.h"

/* Timeval */
// convert timespec to timeval
void timespec_to_val(struct timeval *val, const struct timespec *spec)
{
	val->tv_sec = spec->tv_sec;
	val->tv_usec = spec->tv_nsec / 1000;
}

// get the current time with timeval format
void tvtime(struct timeval *tv)
{
	struct timespec ts;

	tstimer_time(&ts);
	timespec_to_val(tv, &ts);
}

// convert timeval info in us to ms
int timeval_to_ms(struct timeval *tv)
{
	return tv->tv_sec * 1000 + tv->tv_usec / 1000;
}

// get time(based on timeval) difference
void tvtimer_diff(struct timeval *end, struct timeval *start, struct timeval *res)
{
	res->tv_sec = end->tv_sec - start->tv_sec;
	res->tv_usec = end->tv_usec - start->tv_usec;
	if (res->tv_usec < 0) {
		res->tv_usec += 1000000;
		res->tv_sec--;
	}
	//printf("time diff: %3ld.%03lds\n", res->tv_sec, res->tv_usec);
}

/* Timespec */
// convert timespec info in ns to ms
int timespec_to_ms(struct timespec *ts)
{
	return ts->tv_sec * 1000 + ts->tv_nsec / 1000000;
}

// retrieve the time of the specified clock(CLOCK_MONOTONIC)
void tstimer_time(struct timespec *ts)
{
	clock_gettime(CLOCK_MONOTONIC, ts);
	//printf("%ld.%lds\n", ts->tv_sec, ts->tv_nsec);
}

// convert timespec in nanoseconds to miliseconds
int tstimer_to_ms(struct timespec *ts)
{
	return timespec_to_ms(ts);
}

/* get time(based on timespec) difference
 * struct timespec ts_start, ts_end, ts_diff;
 * tstimer_time(&ts_start);
 * tstimer_time(&ts_end);
 * tstimer_diff(&ts_end, &ts_start, &ts_diff);
*/
void tstimer_diff(struct timespec *end, struct timespec *start, struct timespec *res)
{
	res->tv_sec = end->tv_sec - start->tv_sec;
	res->tv_nsec = end->tv_nsec - start->tv_nsec;
	if (res->tv_nsec < 0) {
		res->tv_nsec += 1000000000;
		res->tv_sec--;
	}
	printf("time diff: %3ld.%03lds\n", res->tv_sec, res->tv_nsec);
}

// get current time in ms unit
int get_current_ms()
{
	struct timespec ts;
	tstimer_time(&ts);
	printf("%ld.%lds\n", ts.tv_sec, ts.tv_nsec);

	return tstimer_to_ms(&ts);
}

void HexDump( const char *name, const void *data, int32_t size )
{
	int32_t i=0, offset = 0;
	char tmp[32];
	static char lineBuf[1024];
	const uint8_t *_data = (const uint8_t*)data;

	if( name )
		printf("%s (%d): \n", name, size);

	while( offset < size )
	{
		sprintf( lineBuf, "%08lx :  ", (unsigned long)offset );
		for( i=0 ; i<16 ; ++i )
		{
			if( i == 8 )
			{
				strcat( lineBuf, " " );
			}
			if( offset+i >= size )
			{
				strcat( lineBuf, "   " );
			}
			else
			{
				sprintf(tmp, "%02x ", _data[offset+i]);
				strcat( lineBuf, tmp );
			}
		}
		strcat( lineBuf, "   " );

		//     Add ACSII A~Z, & Number & String
		for( i=0 ; i<16 ; ++i )
		{
			if( offset+i >= size )
			{
				break;
			}
			else{
				if( isprint(_data[offset+i]) )
				{
					sprintf(tmp, "%c", _data[offset+i]);
					strcat(lineBuf, tmp);
				}
				else
				{
					strcat( lineBuf, "." );
				}
			}
		}
		strcat(lineBuf, "\n");
		printf( "%s", lineBuf );
		offset += 16;
	}
}

