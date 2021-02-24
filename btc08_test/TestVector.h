#ifndef __TESTVECTOR_H_
#define __TESTVECTOR_H_
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VECTOR_DATA {
	uint8_t midState[32];
	uint8_t parameter[12];
	uint8_t target[6];
	uint8_t startNonce[4];
	uint8_t endNonce[4];
} VECTOR_DATA;

uint8_t default_golden_midstate[32];
uint8_t default_golden_data[12];
uint8_t golden_nonce[4];
uint8_t golden_nonce_start[4];
uint8_t golden_nonce_end[4];
uint8_t default_golden_target[6];
uint8_t default_golden_hash[32];
uint8_t golden_enable[32];


void GetBistVector( VECTOR_DATA *data );
void GetGoldenVector( int idx, VECTOR_DATA *data );

#ifdef __cplusplus
};
#endif

#endif // __TESTVECTOR_H_