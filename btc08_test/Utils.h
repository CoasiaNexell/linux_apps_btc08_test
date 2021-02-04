#ifndef _UTILS_H_
#define _UTILS_H_

#define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))

void HexDump( const char *name, const void *data, int32_t size );

#endif // _UTILS_H_