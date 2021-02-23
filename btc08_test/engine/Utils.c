#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
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
