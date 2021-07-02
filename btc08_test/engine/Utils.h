#ifndef _UTILS_H_
#define _UTILS_H_

#include <time.h>
#include <sys/time.h>
#include <stdint.h>
#include <stdbool.h>

#define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))
#define cg_memcpy(dest, src, n) _cg_memcpy(dest, src, n, __FILE__, __func__, __LINE__)

void tvtime(struct timeval *tv);
int timeval_to_ms(struct timeval *tv);
void tvtimer_diff(struct timeval *end, struct timeval *start, struct timeval *res);

void tstimer_time(struct timespec *ts);
int tstimer_to_ms(struct timespec *ts);
void tstimer_diff(struct timespec *end, struct timespec *start, struct timespec *res);
uint64_t get_current_ms();
double calc_hashrate(bool isAsicBoost, uint64_t jobcnt, struct timespec *ts_diff);

void HexDump( const char *name, const void *data, int32_t size );
void HexDump2( const char *name, const void *data, int32_t size );
char *bin2hex(const unsigned char *p, size_t len);

void swap16_(void *dest_p, const void *src_p);
uint32_t swab32(uint32_t v);
void swab256(void *dest_p, const void *src_p);

void flip64(void *dest_p, const void *src_p);
void flip80(void *dest_p, const void *src_p);

//------------------------------------------------------------------------------
// Shell utils
//------------------------------------------------------------------------------
#define			NX_SHELL_MAX_ARG		32
#define			NX_SHELL_MAX_STR		64
#define			NX_SHELL_MAX_FUNCTION	64

char Shell_ToUpper( char c );
char* Shell_StrChr( char *string,  uint32_t c );
uint32_t Shell_StrLen( const char *string );
char* Shell_StrCpy( char *strDestination, const char *strSource );
char* Shell_StrCat( char *strDestination, const char *strSource );
int Shell_StrCmp( const char *string1, const char *string2 );
int32_t Shell_IntAtoInt( const char *string );
uint32_t Shell_HexAtoInt( const char *string );
uint32_t Shell_BinAtoInt( const char *string );
char* Shell_RemoveNull( char *string );
int Shell_GetArgument( char *pSrc, char arg[][NX_SHELL_MAX_STR] );

#endif // _UTILS_H_
