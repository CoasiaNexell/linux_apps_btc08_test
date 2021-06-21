#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
#include <byteswap.h>

#include "Utils.h"

#ifdef NX_DTAG
#undef NX_DTAG
#endif
#define NX_DTAG "[Utils]"
#include "NX_DbgMsg.h"

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
	//printf("time diff: %3ld.%03lds\n", res->tv_sec, res->tv_nsec);
}

// get current time in ms unit
uint64_t get_current_ms()
{
	uint64_t msec;
	struct timespec ts;
	tstimer_time(&ts);
	msec = (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
	return msec;
}

double calc_hashrate(bool isAsicBoost, uint64_t jobcnt, struct timespec *ts_diff)
{
	uint64_t processed_hashes;
	double hashrate;

	if (isAsicBoost)
		processed_hashes = (uint64_t)jobcnt * 4 * 0x100000000ull;
	else
		processed_hashes = (uint64_t)jobcnt * 0x100000000ull;

	hashrate = (double)processed_hashes / (double)ts_diff->tv_sec / (1000 * 1000);

	printf("HashRate = %.1f mhash/sec (Works = %llu, Hashes = %"PRIu64" mhash, Total Time = %lds)\n",
			hashrate, jobcnt, processed_hashes/(1000 * 1000), ts_diff->tv_sec);

	return hashrate;
}

void _cg_memcpy(void *dest, const void *src, unsigned int n, const char *file, const char *func, const int line)
{
	if (n < 1 || n > (1ul << 31)) {
		printf("ERR: Asked to memcpy %u bytes from %s %s():%d\n",
		       n, file, func, line);
		return;
	}
	if (!dest) {
		printf("ERR: Asked to memcpy %u bytes to NULL from %s %s():%d\n",
		       n, file, func, line);
		return;
	}
	if (!src) {
		printf("ERR: Asked to memcpy %u bytes from NULL from %s %s():%d\n",
		       n, file, func, line);
		return;
	}
	memcpy(dest, src, n);
}

void HexDump( const char *name, const void *data, int32_t size )
{
	int32_t i=0, offset = 0;
	char tmp[32];
	static char lineBuf[1024];
	const uint8_t *_data = (const uint8_t*)data;

	if( name )
		NxDbgMsg(NX_DBG_INFO, "%s (%d): \n", name, size);

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
		NxDbgMsg(NX_DBG_INFO, "%s", lineBuf );
		offset += 16;
	}
}

/* Adequate size s==len*2 + 1 must be alloced to use this variant */
void __bin2hex(char *s, const unsigned char *p, size_t len)
{
	int i;
	static const char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

	for (i = 0; i < (int)len; i++) {
		*s++ = hex[p[i] >> 4];
		*s++ = hex[p[i] & 0xF];
	}
	*s++ = '\0';
}

/* Returns a malloced array string of a binary value of arbitrary length. The
 * array is rounded up to a 4 byte size to appease architectures that need
 * aligned array  sizes */
char *bin2hex(const unsigned char *p, size_t len)
{
	ssize_t slen;
	char *s;

	slen = len * 2 + 1;
	if (slen % 4)
		slen += 4 - (slen % 4);
	s = calloc(slen, 1);
	__bin2hex(s, p, len);

	return s;
}

/* 0x12345678 --> 0x56781234*/
void swap16_(void *dest_p, const void *src_p)
{
	uint32_t *dest = dest_p;
	const uint32_t *src = src_p;

	*dest =     ((*src & 0xFF) << 16) |     ((*src & 0xFF00) << 16) |
			((*src & 0xFF0000) >> 16) | ((*src & 0xFF000000) >> 16);
}

uint32_t swab32(uint32_t v)
{
	return bswap_32(v);
}

void swab256(void *dest_p, const void *src_p)
{
	uint32_t *dest = dest_p;
	const uint32_t *src = src_p;

	dest[0] = swab32(src[7]);
	dest[1] = swab32(src[6]);
	dest[2] = swab32(src[5]);
	dest[3] = swab32(src[4]);
	dest[4] = swab32(src[3]);
	dest[5] = swab32(src[2]);
	dest[6] = swab32(src[1]);
	dest[7] = swab32(src[0]);
}

void flip64(void *dest_p, const void *src_p)
{
	uint32_t *dest = dest_p;
	const uint32_t *src = src_p;
	int i;

	for (i = 0; i < 16; i++)
		dest[i] = bswap_32(src[i]);
}

void flip80(void *dest_p, const void *src_p)
{
	uint32_t *dest = dest_p;
	const uint32_t *src = src_p;
	int i;

	for (i = 0; i < 20; i++)
		dest[i] = swab32(src[i]);
}


//------------------------------------------------------------------------------
// Shell utils
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Shell_ToUpper
//------------------------------------------------------------------------------
char Shell_ToUpper( char c )
{
//	return (char)toupper(c);	
	return (char)((c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c);
}
//------------------------------------------------------------------------------
// Shell_StrChr
//------------------------------------------------------------------------------
char* Shell_StrChr( char	*string,  uint32_t c )
{
	return strchr(string, (int)c);
}
//------------------------------------------------------------------------------
// Shell_StrLen
//------------------------------------------------------------------------------
uint32_t Shell_StrLen( const char *string )
{
	return (uint32_t)strlen(string);
}
//------------------------------------------------------------------------------
// Shell_StrLen
//------------------------------------------------------------------------------
char* Shell_StrCpy( char *strDestination, const char *strSource )
{
	return strcpy(strDestination, strSource);	
}
//------------------------------------------------------------------------------
// Shell_StrLen
//------------------------------------------------------------------------------
char* Shell_StrCat( char *strDestination, const char *strSource )
{
	return strcat(strDestination, strSource);
}
//------------------------------------------------------------------------------
// Shell_StrCmp
//------------------------------------------------------------------------------
int Shell_StrCmp( const char *string1, const char *string2 )
{
	return strcmp(string1, string2);
}
//------------------------------------------------------------------------------
// Shell_IntAtoInt
//------------------------------------------------------------------------------
int32_t Shell_IntAtoInt( const char *string )
{
	return atoi(string);
#if 0
	char 	ch;
	uint32_t 	result = 0;
	
	while( (ch = *string++) != 0 )
	{
		if( ch >= '0' && ch <= '9' )
		{
			result = result * 10 + (ch - '0');
		}
	}
	return result;
#endif
}
//------------------------------------------------------------------------------
// Shell_HexAtoInt
//------------------------------------------------------------------------------
uint32_t Shell_HexAtoInt( const char *string )
{
/*	
	uint32_t HexNum;
	
	sscanf(string, "%x", &HexNum);
	return HexNum;
*/
	char 	ch;
	uint32_t 	result = 0;
	
	while( (ch = *string++) != 0 )
	{
		if( ch >= '0' && ch <= '9' )
		{
			result = result * 16 + (ch - '0');
		}
		else if( ch >= 'a' && ch <= 'f' )
		{
			result = result * 16 + (ch - 'a') + 10;	
		}
		else if( ch >= 'A' && ch <= 'F' )
		{
			result = result * 16 + (ch - 'A') + 10;	
		}
	}

	return result;
}
//------------------------------------------------------------------------------
// Shell_BinAtoInt
//------------------------------------------------------------------------------
uint32_t Shell_BinAtoInt( const char *string )
{
	uint32_t	 BinNum		= 0;
	uint32_t  BitPos		= 0;
	uint32_t  StrLen;
	const char *pString;

	StrLen	= Shell_StrLen(string);
	pString = string + StrLen - 1;

	while(1)
	{
		if(StrLen == 0) break;
		
		if(*pString == '0')			BinNum |= (uint32_t)(0<<BitPos);
		else if(*pString == '1')	BinNum |= (uint32_t)(1<<BitPos);
		else
		{
			//NX_ASSERT( CFALSE && "NEVER GET HERE" );
		}						

		BitPos++;
		StrLen--;
		pString--;
	}
	return BinNum;	
}
//------------------------------------------------------------------------------
// Shell_RemoveNull
//------------------------------------------------------------------------------
char* Shell_RemoveNull( char *string )
{
	while(*string++ == ' ');
		string--;
	return string;	
}
//------------------------------------------------------------------------------
// Shell_GetArgument
//------------------------------------------------------------------------------
int Shell_GetArgument( char *pSrc, char arg[][NX_SHELL_MAX_STR] )
{
	int	i, j;

	// Reset all arguments
	for( i=0 ; i<NX_SHELL_MAX_ARG ; i++ ) 
	{
		arg[i][0] = 0;
	}

	for( i=0 ; i<NX_SHELL_MAX_ARG ; i++ )
	{
		// Remove space char
		while( *pSrc == ' ' ) 	pSrc++;

		// check end of string.
		if( (*pSrc == 0) || (*pSrc == '\n') )  		break;

		j=0;

		while( (*pSrc != ' ') && (*pSrc != 0) && (*pSrc != '\n') )
		{
			arg[i][j] = *pSrc++;
//			if( arg[i][j] >= 'a' && arg[i][j] <= 'z' ) 		// to upper char
//				arg[i][j] += ('A' - 'a');
			
			j++;
			if( j > (NX_SHELL_MAX_STR-1) ) 	j = NX_SHELL_MAX_STR-1;
		}
	  	
		arg[i][j] = 0;
	}

	return i;
}
