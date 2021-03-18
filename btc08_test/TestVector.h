#ifndef __TESTVECTOR_H_
#define __TESTVECTOR_H_
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VECTOR_DATA {
	uint8_t midState[32*4];
	uint8_t parameter[12];
	uint8_t target[6];
	uint8_t startNonce[4];
	uint8_t endNonce[4];

	//	result
	uint8_t nonce[4];
	uint8_t hash[32];
} VECTOR_DATA;

//	midstate length : 32, 32*4 : for asic booster
uint8_t default_golden_midstate[32*4];
uint8_t default_golden_data[12];
uint8_t golden_nonce[4];
uint8_t golden_nonce_start[4];
uint8_t golden_nonce_end[4];
uint8_t default_golden_target[6];
uint8_t default_golden_hash[32];
uint8_t golden_enable[32];

#define MAX_NUM_VECTOR		(6)

void GetBistVector( VECTOR_DATA *data );
void GetGoldenVector( int idx, VECTOR_DATA *data, int enMidRandom );
void GetGoldenVectorWithVMask( int idx, VECTOR_DATA *data, int enMidRandom );

#ifdef __cplusplus
};
#endif

#endif // __TESTVECTOR_H_