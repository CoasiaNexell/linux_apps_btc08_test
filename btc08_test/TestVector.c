#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <byteswap.h>
#include <stdlib.h>
#include <stdbool.h>

#include "sha2.h"
#include "Utils.h"
#include "TestVector.h"

/* GOLDEN_MIDSTATE */
uint8_t default_golden_midstate[32*4] = {
	0x5f, 0x4d, 0x60, 0xa2, 0x53, 0x85, 0xc4, 0x07,
	0xc2, 0xa8, 0x4e, 0x0c, 0x25, 0x91, 0x69, 0xc4,
	0x10, 0xa4, 0xa5, 0x4b, 0x93, 0xf7, 0x17, 0x08,
	0xf1, 0xab, 0xdf, 0xec, 0x6e, 0x8b, 0x81, 0xd2,

	0x5f, 0x4d, 0x60, 0xa2, 0x53, 0x85, 0xc4, 0x07,
	0xc2, 0xa8, 0x4e, 0x0c, 0x25, 0x91, 0x69, 0xc4,
	0x10, 0xa4, 0xa5, 0x4b, 0x93, 0xf7, 0x17, 0x08,
	0xf1, 0xab, 0xdf, 0xec, 0x6e, 0x8b, 0x81, 0xd2,

	0x5f, 0x4d, 0x60, 0xa2, 0x53, 0x85, 0xc4, 0x07,
	0xc2, 0xa8, 0x4e, 0x0c, 0x25, 0x91, 0x69, 0xc4,
	0x10, 0xa4, 0xa5, 0x4b, 0x93, 0xf7, 0x17, 0x08,
	0xf1, 0xab, 0xdf, 0xec, 0x6e, 0x8b, 0x81, 0xd2,

	0x5f, 0x4d, 0x60, 0xa2, 0x53, 0x85, 0xc4, 0x07,
	0xc2, 0xa8, 0x4e, 0x0c, 0x25, 0x91, 0x69, 0xc4,
	0x10, 0xa4, 0xa5, 0x4b, 0x93, 0xf7, 0x17, 0x08,
	0xf1, 0xab, 0xdf, 0xec, 0x6e, 0x8b, 0x81, 0xd2
};

/* GOLDEN_DATA */
uint8_t default_golden_data[12] = {
	/* Data (MerkleRoot, Time, Target) */
	0xf4, 0x2a, 0x1d, 0x6e, 0x5b, 0x30, 0x70, 0x7e,
	0x17, 0x37, 0x6f, 0x56,
};

/* GOLDEN_NONCE */
uint8_t golden_nonce[4] = {
	0x66, 0xcb, 0x34, 0x26
};

/* GOLDEN_NONCE_RANGE(2G) */
uint8_t golden_nonce_start[4] = {
	0x66, 0xcb, 0x00, 0x00
};

uint8_t golden_nonce_end[4] = {
	0x66, 0xcb, 0xff, 0xff
};

/* GOLDEN_TARGET */
uint8_t default_golden_target[6] = {
	0x17, 0x37, 0x6f, 0x56, 0x05, 0x00
};

/* GOLDEN_HASH */
uint8_t default_golden_hash[32] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x22, 0x09, 0x3d, 0xd4, 0x38, 0xed, 0x47,
	0xfa, 0x28, 0xe7, 0x18, 0x58, 0xb8, 0x22, 0x0d,
	0x53, 0xe5, 0xcd, 0x83, 0xb8, 0xd0, 0xd4, 0x42,
};

/* Enable all cores */
uint8_t golden_enable[32] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};


typedef struct BLOCk_DATA_INFO {
	uint8_t header[80];		//	block header
	uint8_t hash[32];		//	hash result
} BLOCk_DATA_INFO;

uint32_t vmask_001[16];
//
//	Block Header ( 128 Bytes )
//
//	0        1         2         3         4         5         6         7         8         9        10        11        12        
//	12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678
//  |--||------------------------------||------------------------------||--||--||--||==============================================|
//   0                1                                2                  3   4   5                       6
//
//	0 : Version
//	1 : Previous Block Hash
//	2 : Merkle Root
//	3 : Timestamp
//	4 : Bits
//	5 : Nonce
//	6 : Padding ( not use )
//
//
//	Seperate 2 Blocks ( 64 bytes + 64 bytes )
//
//	0        1         2         3         4         5         6         7         8         9        10        11        12        
//	12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678
//  |--------------------------------------------------------------||---------------===============================================|
//                              Block 1                                                        Block 2
//	|--------------------------------------------------------------||----------||--|
//                                 a                                       b     c
//
//	a : Block Header Upper(64) : Version + Previous Block Hash + Merkle Root Upper(28)
//	b : Parameter(12) : Merkle Root Tail(4) + Timestamp(4) + Bits(4)
//	c : Nonce(4)
//
//
//	We find out midstate, parameter and target
//		midstate  (32 bytes) : Block 1 --> sha256 : 32 Bytes
//		parameter (12 bytes) : Merkle Root Tail(4) + TimeStamp(4) + Bits(4)
//		target    ( 6 bytes) : Bits(4) + Search Range(2)
//
//	Our Goal:
//		Nonce, Hash
//

static BLOCk_DATA_INFO gstGoldenData[7] = {
	// Index 0
	{
		//	#671783 Block Header
		//	Hash 0000000000000000000ac4a7ac61e102ab2035b62f60d18a732f79caab8d3357
		//
		//	Version       : 20000000
		//	Merkle Root   : 222be02d6272f066d498c4f5374283e146f7ccb0a4148d0107a6bc2418174bec
		//	Previous Hash : 000000000000000000056e900ac4220ce2287a082d0ba6b974cfdd3e498f76f2
		//	TimeStamp     : 2021-02-23 04:12 (0x60348090) + alpha() : blockchair's mined on time 
		//	Nonce         : 1353073737
		//	Bits          : 386725091
		{
			//-------------------------------------------
			//	Version
			0x20,0x00,0x00,0x00,
			//	Previous Block Hash
			0x49,0x8f,0x76,0xf2,
			0x74,0xcf,0xdd,0x3e,
			0x2d,0x0b,0xa6,0xb9,
			0xe2,0x28,0x7a,0x08,
			0x0a,0xc4,0x22,0x0c,
			0x00,0x05,0x6e,0x90,
			0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,
			//	Merkle Root
			0x18,0x17,0x4b,0xec,
			0x07,0xa6,0xbc,0x24,
			0xa4,0x14,0x8d,0x01,
			0x46,0xf7,0xcc,0xb0,
			0x37,0x42,0x83,0xe1,
			0xd4,0x98,0xc4,0xf5,
			0x62,0x72,0xf0,0x66,
			0x22,0x2b,0xe0,0x2d,
			//	TimeStamp
			0x60,0x34,0x80,0x90,	//	parameter[12]
			//	Bits
			0x17,0x0C,0xF4,0xE3,
			//-------------------------------------------
			//	Nonce
			0x50,0xA6,0x44,0x49
		},
		//	Hash
		{
			0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,
			0x00,0x0a,0xc4,0xa7,
			0xac,0x61,0xe1,0x02,
			0xab,0x20,0x35,0xb6,
			0x2f,0x60,0xd1,0x8a,
			0x73,0x2f,0x79,0xca,
			0xab,0x8d,0x33,0x57
		}
	},

	// Index 1
	{
		//	#671894 Block
		//
		//	Hash : 00000000000000000007e6336da9efc6d9b98966eed15241cc0ff75f0ecc5638
		//
		//	Version : 20400000
		//	Previous Hash : 00000000000000000006bf973dc198707645fc75bb6141e2e2e28da3aacfeb47
		//	Merckle Root : f9b139565dfda4eb59940d5ee080fdc68fd371d395350f508c024595064d44c4
		//	Time : 2021-02-24 01:08, 6035A6F0
		//	Bits : 386725091
		//	Nonce : 639392792
		//
		{
			//	Version
			0x20,0x40,0x00,0x00,
			//	Previous Hash
			0xaa,0xcf,0xeb,0x47,
			0xe2,0xe2,0x8d,0xa3,
			0xbb,0x61,0x41,0xe2,
			0x76,0x45,0xfc,0x75,
			0x3d,0xc1,0x98,0x70,
			0x00,0x06,0xbf,0x97,
			0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,
			//	Merkle Root
			0x06,0x4d,0x44,0xc4,
			0x8c,0x02,0x45,0x95,
			0x95,0x35,0x0f,0x50,
			0x8f,0xd3,0x71,0xd3,
			0xe0,0x80,0xfd,0xc6,
			0x59,0x94,0x0d,0x5e,
			0x5d,0xfd,0xa4,0xeb,
			0xf9,0xb1,0x39,0x56,
			//	TimeStamp 6035A6F0
			0x60,0x35,0xA6,0xF0,
			//	Bits (386725091)
			0x17,0x0C,0xF4,0xE3,
			//	Nonce (639392792)
			0x26,0x1C,0x5C,0x18,
		},
		{
			0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,
			0x00,0x07,0xe6,0x33,
			0x6d,0xa9,0xef,0xc6,
			0xd9,0xb9,0x89,0x66,
			0xee,0xd1,0x52,0x41,
			0xcc,0x0f,0xf7,0x5f,
			0x0e,0xcc,0x56,0x38,
		}
	},

	// Index 2
	{
		//	#671895 Block
		//	Hash : 0000000000000000000281aec9110b39a14b747ba95aa333e472c9da7dd0f97d
		//
		//	Version : 27ffe000
		//	Previous Hash : 00000000000000000007e6336da9efc6d9b98966eed15241cc0ff75f0ecc5638
		//	Merkle Root : 3a81535e71294d1546c6329e2ae424d9308971323f998914aa7419ba93c32583
		//	TimeStamp : 2021-02-24 01:29 , 6035ABDC
		//	Bits : 386725091
		//	Nonce : 3675314107
		{
			//	Version
			0x27,0xff,0xe0,0x00,
			//	Previous Hash
			0x0e,0xcc,0x56,0x38,
			0xcc,0x0f,0xf7,0x5f,
			0xee,0xd1,0x52,0x41,
			0xd9,0xb9,0x89,0x66,
			0x6d,0xa9,0xef,0xc6,
			0x00,0x07,0xe6,0x33,
			0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,
			//	Merkle Root
			0x93,0xc3,0x25,0x83,
			0xaa,0x74,0x19,0xba,
			0x3f,0x99,0x89,0x14,
			0x30,0x89,0x71,0x32,
			0x2a,0xe4,0x24,0xd9,
			0x46,0xc6,0x32,0x9e,
			0x71,0x29,0x4d,0x15,
			0x3a,0x81,0x53,0x5e,
			//	TimeStamp 6035ABDC
			0x60,0x35,0xAB,0xDC,
			//	Bits (386725091)
			0x17,0x0C,0xF4,0xE3,
			//	Nonce (3675314107)
			0xDB,0x10,0xD7,0xBB,
		},
		{
			//	Hash
			0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0xa5,
			0xa1,0x4b,0x74,0x7b,
			0xa9,0x5a,0xa3,0x33,
			0xe4,0x72,0xc9,0xda,
			0xa9,0x5a,0xa3,0x33,
			0xe4,0x72,0xc9,0xda,
			0x7d,0xd0,0xf9,0x7d,
		}
	},

	// Index 3
	{
		//	#671896 Block
		//
		//	Hash          : 00000000000000000006565cbae6016f8d12e8d97756c8a40e6e09babbeb5906
		//
		//	Version       : 2000e000
		//	Previous Hash : 0000000000000000000281aec9110b39a14b747ba95aa333e472c9da7dd0f97d
		//	Merkle Root   : 4949ad5a4c52006d85db1356f20b14b57c43012abe4b22cfcf830e5ceff62ce4
		//	TimeStamp     : 2021-02-24 01:57
		//	Bits          : 386725091
		//	Nonce         : 289953179
		{
			//	Version
			0x20,0x00,0xe0,0x00,
			//	Previous Hash
			0x7d,0xd0,0xf9,0x7d,
			0xe4,0x72,0xc9,0xda,
			0xa9,0x5a,0xa3,0x33,
			0xa1,0x4b,0x74,0x7b,
			0xc9,0x11,0x0b,0x39,
			0x00,0x02,0x81,0xae,
			0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,
			//	Merkle Root
			0xef,0xf6,0x2c,0xe4,
			0xcf,0x83,0x0e,0x5c,
			0xbe,0x4b,0x22,0xcf,
			0x7c,0x43,0x01,0x2a,
			0xf2,0x0b,0x14,0xb5,
			0x85,0xdb,0x13,0x56,
			0x4c,0x52,0x00,0x6d,
			0x49,0x49,0xad,0x5a,

			//	TimeStamp 6035B26C
			0x60,0x35,0xB2,0x6C,
			//	Bits (386725091)
			0x17,0x0C,0xF4,0xE3,
			//	Nonce (289953179)
			0x11,0x48,0x55,0x9B,
		},
		{
			//	Hash
			0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,
			0x00,0x06,0x56,0x5c,
			0xba,0xe6,0x01,0x6f,
			0x8d,0x12,0xe8,0xd9,
			0x77,0x56,0xc8,0xa4,
			0x0e,0x6e,0x09,0xba,
			0xbb,0xeb,0x59,0x06,
		}
	},

	// Index 4 : Our Golden Data Hash ()

	//
	// Hash                       00000000000000000022093dd438ed47fa28e71858b8220d53e5cd83b8d0d442
	//
	// Version                    20000000
	// Prev_Hash                  00000000000000000026087ab13d78b5062f38bceb8fe2dc14c53e6b25057933
	// Merkle_Root                f42a1d6e97a8bc9f22e5516842b25ac4d53b2442b971b718a07e2c9d3085a5a5
	// Time                       5B30707E (2018-06-25 04:33:00) // 5B30707C
	// Bits                       17376F56
	// Nonce                      66cb3426

	//	Midstate                  5f4d60a25385c407c2a84e0c259169c410a4a54b93f71708f1abdfec6e8b81d2
	//
	//	#529122
	{
		{
			//-------------------------------------------
			//	Version
			0x20,0x00,0x00,0x00,
			//	Previous Block Hash
//            1    2    3    4  
			0x25,0x05,0x79,0x33,//  7
			0x14,0xc5,0x3e,0x6b,//  6
			0xeb,0x8f,0xe2,0xdc,//  5
			0x06,0x2f,0x38,0xbc,//  4
			0xb1,0x3d,0x78,0xb5,//  3
			0x00,0x26,0x08,0x7a,//  2
			0x00,0x00,0x00,0x00,//  1
			0x00,0x00,0x00,0x00,//  0

			//	Merkle Root
//            1    2    3    4  
			0x30,0x85,0xa5,0xa5,//  7
			0xa0,0x7e,0x2c,0x9d,//  6
			0xb9,0x71,0xb7,0x18,//  5
			0xd5,0x3b,0x24,0x42,//  4
			0x42,0xb2,0x5a,0xc4,//  3
			0x22,0xe5,0x51,0x68,//  2
			0x97,0xa8,0xbc,0x9f,//  1
			0xf4,0x2a,0x1d,0x6e,//  0
			//	TimeStamp
			0x5B,0x30,0x70,0x7E,
			//	Bits
			0x17,0x37,0x6F,0x56,
			//	Nonce
			0x66,0xcb,0x34,0x26,
		},
		{
			0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,
			0x00,0x22,0x09,0x3d,
			0xd4,0x38,0xed,0x47,
			0xfa,0x28,0xe7,0x18,
			0x58,0xb8,0x22,0x0d,
			0x53,0xe5,0xcd,0x83,
			0xb8,0xd0,0xd4,0x42,
		}
	},

	// Index 5
	// #286819
	{
		{
			//-------------------------------------------
			//	Version 00000002
			0x00,0x00,0x00,0x02,
			//	Previous Block Hash
			//000000000000000117c80378b8da0e33559b5997f2ad55e2f7d18ec1975b9717
			0x97,0x5b,0x97,0x17,
			0xf7,0xd1,0x8e,0xc1,
			0xf2,0xad,0x55,0xe2,
			0x55,0x9b,0x59,0x97,
			0xb8,0xda,0x0e,0x33,
			0x17,0xc8,0x03,0x78,
			0x00,0x00,0x00,0x01,
			0x00,0x00,0x00,0x00,

			//	Merkle Root
			//871714dcbae6c8193a2bb9b2a69fe1c0440399f38d94b3a0f1b447275a29978a
			0x5a,0x29,0x97,0x8a,
			0xf1,0xb4,0x47,0x27,
			0x8d,0x94,0xb3,0xa0,
			0x44,0x03,0x99,0xf3,
			0xa6,0x9f,0xe1,0xc0,
			0x3a,0x2b,0xb9,0xb2,
			0xba,0xe6,0xc8,0x19,
			0x87,0x17,0x14,0xdc,

			//	TimeStamp  2014-02-20 04:57
			//	53058B1C
			0x53,0x05,0x8b,0x1c,
			//	Bits
			//	19015F53
			0x19,0x01,0x5F,0x53,
			//	Nonce
			//	33087548
			0x33,0x08,0x75,0x48
		},
		//0000000000000000e067a478024addfecdc93628978aa52d91fabd4292982a50
		{
			0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,
			0xe0,0x67,0xa4,0x78,
			0x02,0x4a,0xdd,0xfe,
			0xcd,0xc9,0x36,0x28,
			0x97,0x8a,0xa5,0x2d,
			0x91,0xfa,0xbd,0x42,
			0x92,0x98,0x2a,0x50,
		}
	},

	// Index 6
	{
		{0,},
		{0,}
	}
};

/*
 *
 *	GetGoldenParameter
 *	Parameter : midstate
 *  
 * 
 * 
 */

void GetGoldenParameter( uint8_t midstate[32], uint8_t param[12], uint8_t target[6] )
{
	memcpy(midstate, default_golden_midstate, 32);
	memcpy(param, default_golden_data, 12);
	memcpy(target, default_golden_target, 6);
}

void GetGoldenNonce( uint8_t startNonce[4], uint8_t endNonce[4] )
{
	memcpy(startNonce, golden_nonce_start, 4);
	memcpy(endNonce, golden_nonce_end, 4);
}

static void DumpData( const char *name, void *data, int size )
{
	uint8_t *buf = (uint8_t *)data;
	printf("%s(%d) : ", name, size);
	for( int i=0 ; i < size ; i++ )
	{
		printf("%02x", buf[i]);
	}
	printf("\n");
}

static void DumpGoldenVector( BLOCk_DATA_INFO *golden )
{
	if( golden )
	{
		DumpData("Block Header", golden->header, sizeof(golden->header) );
		DumpData("Hash        ", golden->hash, sizeof(golden->hash) );
	}
}

static void DumpVectorData( VECTOR_DATA *data )
{
	if( data )
	{
		DumpData("MidState    ", data->midState + 32*0, 32);
		DumpData("MidState1   ", data->midState + 32*1, 32);
		DumpData("MidState2   ", data->midState + 32*2, 32);
		DumpData("MidState3   ", data->midState + 32*3, 32);
		DumpData("Parameter   ", data->parameter, sizeof(data->parameter));
		DumpData("Target      ", data->target, sizeof(data->target));
		//DumpData("Start Nonce ", data->startNonce, sizeof(data->startNonce));
		//DumpData("End Nonce   ", data->endNonce, sizeof(data->endNonce));
		//DumpData("Nonce       ", data->nonce, sizeof(data->nonce));
	}
}

static bool set_vmask(int *vmask_003)
{
	int mask, tmpMask = 0, cnt = 0, i, rem;
	const char *version_mask = "1fffe000";

	mask = strtol(version_mask, NULL, 16);
	if (!mask)
		return false;

	vmask_003[0] = mask;

	while (mask % 16 == 0) {
		cnt++;
		mask /= 16;
	}

	if ((rem = mask % 16))
		tmpMask = rem;
	else if ((rem = mask % 8))
		tmpMask = rem;
	else if ((rem = mask % 4))
		tmpMask = rem;
	else if ((rem = mask % 2))
		tmpMask = rem;

	for (i = 0; i < cnt; i++)
		tmpMask *= 16;

	vmask_003[2] = tmpMask;
	vmask_003[1] = vmask_003[0] - tmpMask;

	return true;
}

static void get_vmask(char *bbversion, int *vmask_003, uint32_t *vmask_001)
{
	char defaultStr[9]= "00000000";
	int bversion, num_bits, i, j;
	uint8_t buffer[4] = {};
	uint32_t uiMagicNum;
	char *tmpstr;
	uint32_t *p1;

	p1 = (uint32_t *)buffer;
	bversion = strtol(bbversion, NULL, 16);

	for (i = 0; i < 4; i++) {
		uiMagicNum = bversion | vmask_003[i];
		*p1 = bswap_32(uiMagicNum);

		switch(i) {
			case 0:
				vmask_001[8] = *p1;
				break;
			case 1:
				vmask_001[4] = *p1;
				break;
			case 2:
				vmask_001[2] = *p1;
				break;
			case 3:
				vmask_001[0] = *p1;
				break;
			default:
				break;
		}
	}

	for (i = 0; i < 16; i++) {
		if ((i!= 2) && (i!=4) && (i!=8)) {
			vmask_001[i] = vmask_001[0];
		}
		else {
			printf("vmask_001[%d]=%02x\n", i, vmask_001[i]);
		}
	}
}

static void calc_midstate(VECTOR_DATA *header, int idx, uint32_t *vmask_001,
	uint8_t *midstate, uint8_t *midstate1, uint8_t *midstate2, uint8_t *midstate3)
{
	unsigned char data[64];
	uint32_t *data32 = (uint32_t *)data;
	sha256_ctx ctx;

	memset(header->data, 0, sizeof(header->data));
	memcpy(header->data, gstGoldenData[idx].header, 80/*except padding*/);

	if (vmask_001)
	{
		/* This would only be set if the driver requested a vmask and
		 * the pool has a valid version mask. */
		// vmask_001[2] = 0x00E0_0020
		memcpy(header->data, &(vmask_001[2]), 4);
		flip64(data32, header->data);
		sha256_init(&ctx);
		sha256_update(&ctx, data, 64);
		cg_memcpy(midstate1, ctx.h, 32);

		// vmask_001[4] = 0x0000_FF3F
		memcpy(header->data, &(vmask_001[4]), 4);
		flip64(data32, header->data);
		sha256_init(&ctx);
		sha256_update(&ctx, data, 64);
		cg_memcpy(midstate2, ctx.h, 32);

		// vmask_001[8] = 0x00E0_FF3F
		memcpy(header->data, &(vmask_001[8]), 4);
		flip64(data32, header->data);
		sha256_init(&ctx);
		sha256_update(&ctx, data, 64);
		cg_memcpy(midstate3, ctx.h, 32);

		// vmask_001[0] = 0x0000_0020
		memcpy(header->data, &(vmask_001[0]), 4);
	}

	flip64(data32, header->data);
	sha256_init(&ctx);
	sha256_update(&ctx, data, 64);
	cg_memcpy(midstate, ctx.h, 32);
}

//
//						Find Target for BTC08
//
//	Bits = 170CF4E3
//	    00000000_00000000_000CF4E3_00000000_00000000_00000000_00000000_00000000
//
//	Select 0:
//      |------ 6 ------|
//               |------ 5 ------|
//                        |------ 4 ------|
//                                 |------ 3 ------|
//                                          |------ 2 ------|
//                                                   |------ 1 ------|
//                                                            |------ 0 ------|
//
//
//	Bits = 170CF4E3
//	    00000000_00000000_000CF4E3_00000000_00000000_00000000_00000000_00000000
//  Select 0:             |------ 4 ------|
//	Select 1:
//	                      |---4--| <-------
//	                        |---3---|
//	                          |---2---|
//	                            |---1---|
//	                               |---0--|
//
void GetGoldenVector( int idx, VECTOR_DATA *data, int enMidRandom )
{
	uint8_t select0, select1, shift = 0;
	int32_t offset = 0;
	uint8_t midstate[32];
	int golenMidstate = 0;

	//	calculate midstate
	{
		sha256_ctx ctx;
		uint8_t temp[64];
		uint32_t *data32 = (uint32_t*)temp;

		flip64(data32, gstGoldenData[idx].header);
		sha256_init(&ctx);
		sha256_update(&ctx, temp, 64);
		memcpy(midstate, ctx.h, 32);
	}

	//	Make random error except selected index
	if( enMidRandom )
	{
		golenMidstate = random() % 4;
		memset( data, 0, sizeof(VECTOR_DATA) );
		switch( golenMidstate )
		{
			case 0:
				memcpy(data->midState, midstate, 32);

				memcpy(data->midState + 32 * 1, midstate, 30);
				memcpy(data->midState + 32 * 2, midstate, 30);
				memcpy(data->midState + 32 * 3, midstate, 30);
				break;
			case 1:
				memcpy(data->midState, midstate, 30);
				memcpy(data->midState + 32 * 1, midstate, 32);
				memcpy(data->midState + 32 * 2, midstate, 30);
				memcpy(data->midState + 32 * 3, midstate, 30);
				break;
			case 2:
				memcpy(data->midState, midstate, 30);
				memcpy(data->midState + 32 * 1, midstate, 30);
				memcpy(data->midState + 32 * 2, midstate, 32);
				memcpy(data->midState + 32 * 3, midstate, 30);
				break;
			case 3:
				memcpy(data->midState, midstate, 30);
				memcpy(data->midState + 32 * 1, midstate, 30);
				memcpy(data->midState + 32 * 2, midstate, 30);
				memcpy(data->midState + 32 * 3, midstate, 32);
				break;
		}
	}
	//	default
	else
	{
		memcpy(data->midState, midstate, 32);
		memcpy(data->midState + 32 * 1, midstate, 32);
		memcpy(data->midState + 32 * 2, midstate, 32);
		memcpy(data->midState + 32 * 3, midstate, 32);
	}

	//	parameter
	offset = 64;	//	Jump to Mekle Root Tail

	//	Mekle Root Tail + Time Stamp + Difficulty
	memcpy( data->parameter, gstGoldenData[idx].header + offset, 12 );

	//	target : nBits + 

//	[43:12] : Target (32 bit)
//	[11:0] : Select (12 bit)
// 		[11:8] : Select0   // 256 bits를 64 bits 단위로 7개로 구분
//		[7:4]  : Select1   // 64 bits를 32 bits 단위로 5개로 구분
//		[3:0]  : Shift

	offset = 64 + 8;
	memcpy( data->target, gstGoldenData[idx].header + offset, 4 );
	select0 = (data->target[0] / 4) - 1;
	select1 = (data->target[0] % 4) + 1;
	data->target[4] = select0;
	data->target[5] = select1<<4 | (shift&0xF);

	offset = 64 + 12;
	//	Fill Hash
	memcpy( data->hash, gstGoldenData[idx].hash, sizeof(gstGoldenData[idx].hash) );
	//	Fill Golden Nonce
	memcpy( data->nonce, gstGoldenData[idx].header + offset, 4 );

	//	make full range nonce
	data->startNonce[0] = 0x00;
	data->startNonce[1] = 0x00;
	data->startNonce[2] = 0x00;
	data->startNonce[3] = 0x00;

	data->endNonce[0] = 0xff;
	data->endNonce[1] = 0xff;
	data->endNonce[2] = 0xff;
	data->endNonce[3] = 0xff;

	printf("=======================================\n");
	printf("Input Vector (%d):\n", idx);
	DumpGoldenVector(&gstGoldenData[idx]);
	printf("=======================================\n");
	printf("Input Prameter : Golden Midstate = %d\n", golenMidstate);
	DumpVectorData(data);
	printf("=======================================\n");
}

static inline uint32_t swab32(uint32_t v)
{
	return bswap_32(v);
}

static inline void flip80(void *dest_p, const void *src_p)
{
	uint32_t *dest = dest_p;
	const uint32_t *src = src_p;
	int i;

	for (i = 0; i < 20; i++)
		dest[i] = swab32(src[i]);
}

static inline void swab256(void *dest_p, const void *src_p)
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

bool fulltest(const unsigned char *hash, const unsigned char *target)
{
	uint32_t *hash32 = (uint32_t *)hash;
	uint32_t *target32 = (uint32_t *)target;
	bool rc = true;
	int i;

	for (i = 28 / 4; i >= 0; i--) {
		uint32_t h32tmp = le32toh(hash32[i]);
		uint32_t t32tmp = le32toh(target32[i]);

		if (h32tmp > t32tmp) {
			rc = false;
			break;
		}
		if (h32tmp < t32tmp) {
			rc = true;
			break;
		}
	}
#ifdef DEBUG
	unsigned char hash_swap[32], target_swap[32];
	char *hash_str, *target_str;

	swab256(hash_swap, hash);
	swab256(target_swap, target);
	hash_str = bin2hex(hash_swap, 32);
	target_str = bin2hex(target_swap, 32);

	printf("Proof: %s\nTarget: %s\nTrgVal? %s\n",
		hash_str,
		target_str,
		rc ? "YES (hash <= target)" :
				"no (false positive; hash > target)");

	free(hash_str);
	free(target_str);
#endif
	return rc;
}

/* To be used once the work has been tested to be meet diff1 and has had its
 * nonce adjusted. Returns true if the work target is met. */
bool submit_tested_work(VECTOR_DATA *data)
{
	if (!fulltest(data->hash, data->target)) {
		printf("Failed. Share above target!\n");
		return false;
	}
	return true;
}

static void regen_hash(VECTOR_DATA *data)
{
	uint32_t *data32 = (uint32_t *)(data->data);
#ifdef DEBUG
	printf("%s data32:0x%08x\n", __FUNCTION__, *data32);
#endif
	unsigned char swap[80];
	uint32_t *swap32 = (uint32_t *)swap;
	unsigned char hash1[32];

	flip80(swap32, data32);
	sha256(swap, 80, hash1);
	sha256(hash1, 32, (unsigned char *)(data->hash));

#ifdef DEBUG
	DumpData("data->hash", data->hash, sizeof(data->hash));
#endif
}

// Fills in the work nonce and builds the output data in data->hash
static void rebuild_nonce(VECTOR_DATA *data, uint32_t nonce)
{
	uint32_t *work_nonce = (uint32_t *)(data->nonce);
	*work_nonce = htole32(nonce);
#ifdef DEBUG
	printf("%s work_nonce:0x%08x\n", __FUNCTION__, *work_nonce);
#endif

	regen_hash(data);
}

bool test_nonce(VECTOR_DATA *data, uint32_t nonce)
{
	uint32_t *hash_32 = (uint32_t *)(data->hash + 28);
	rebuild_nonce(data, nonce);
	return (*hash_32 == 0);
}

bool submit_nonce(VECTOR_DATA *data, uint32_t nonce)
{
	if (test_nonce(data, nonce)) {
		submit_tested_work(data);
	}
	else {
		return false;
	}

	return true;
}

void GetGoldenVectorWithVMask( int idx, VECTOR_DATA *data, int enMidRandom )
{
	uint8_t select0, select1, shift = 0;
	int32_t offset = 0;
	uint8_t midstate[32], midstate1[32], midstate2[32], midstate3[32];

	//	calculate midstate with version rolling
	int vmask_003[4];
	char *bbversion;

	memset(data, 0, sizeof(VECTOR_DATA));

	set_vmask(vmask_003);
	bbversion = bin2hex(gstGoldenData[idx].header, 4);

	get_vmask(bbversion, vmask_003, vmask_001);
	calc_midstate (data, idx, vmask_001,
					midstate, midstate1, midstate2, midstate3);

	memcpy(data->midState, midstate, 32);
	memcpy(data->midState + 32 * 1, midstate1, 32);
	memcpy(data->midState + 32 * 2, midstate2, 32);
	memcpy(data->midState + 32 * 3, midstate3, 32);

	//	parameter
	offset = 64;	//	Jump to Mekle Root Tail

	// 96bits for Merkle Root, Time Stamp, Difficulty
	memcpy( data->parameter, gstGoldenData[idx].header + offset, 12 );

	//	[47:16] : Target (32 bit)
	//	[11:0] : Select (12 bit)
	// 		[11:8] : Select0   // 256 bits를 64 bits 단위로 7개로 구분
	//		[7:4]  : Select1   // 64 bits를 32 bits 단위로 5개로 구분
	//		[3:0]  : Shift
	offset = 64 + 8;
	// Need to calc target from nbit
	memcpy( data->target, gstGoldenData[idx].header + offset, 4 );
	select0 = (data->target[0] / 4) - 1;
	select1 = (data->target[0] % 4) + 1;
	data->target[4] = select0;
	data->target[5] = select1<<4 | (shift&0xF);

	//	make full range nonce
	data->startNonce[0] = 0x00;
	data->startNonce[1] = 0x00;
	data->startNonce[2] = 0x00;
	data->startNonce[3] = 0x00;

	data->endNonce[0] = 0xff;
	data->endNonce[1] = 0xff;
	data->endNonce[2] = 0xff;
	data->endNonce[3] = 0xff;

	printf("=======================================\n");
	printf("Input Vector (%d):\n", idx);
	DumpGoldenVector(&gstGoldenData[idx]);
	printf("=======================================\n");
	DumpVectorData(data);
	printf("=======================================\n");
}

void GetBistVector( VECTOR_DATA *data )
{
	memcpy(data->midState+32*0, default_golden_midstate, 32);
	memcpy(data->midState+32*1, default_golden_midstate, 32);
	memcpy(data->midState+32*2, default_golden_midstate, 32);
	memcpy(data->midState+32*3, default_golden_midstate, 32);
	memcpy(data->parameter, default_golden_data, sizeof(data->parameter));
	memcpy(data->target, default_golden_target, sizeof(data->target));
	memcpy(data->startNonce, golden_nonce_start, sizeof(data->startNonce));
	memcpy(data->endNonce, golden_nonce_end, sizeof(data->endNonce));
	DumpVectorData(data);
}

//------------------------------------------------------------------------------
//
//                               Test Source
//
//------------------------------------------------------------------------------



void TestMidstate()
{
	sha256_ctx ctx;
	//	Input
	uint8_t input[64] = {
		// Version
		0x20,0x00,0x00,0x00,
		// Previous Has
		0x79,0x4d,0xdf,0x60,
		0x9e,0xe2,0x29,0x81,
		0xe1,0xef,0x18,0xd3,
		0xff,0xc9,0xa3,0x42,
		0x40,0x3d,0x02,0x0f,
		0x00,0x02,0x7f,0x14,
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,
		// Merkle Root
		0x72,0x3d,0x04,0x43,
		0x83,0x1a,0x01,0xd5,
		0xb6,0x67,0x05,0x2d,
		0xa9,0xe7,0xe1,0x5c,
		0xe1,0xe6,0x72,0xfe,
		0xf0,0x71,0x8b,0xb7,
		0x01,0x45,0xf6,0x40
	};

	uint8_t midstate[32];
	uint8_t data[64];
	uint32_t *data32 = (uint32_t*)data;

	flip64(data32, input);
	sha256_init(&ctx);
	sha256_update(&ctx, data, 64);
	memcpy(midstate, ctx.h, 32);

	char *strMidstate = bin2hex(midstate, sizeof(midstate));

	printf("expected midstate : be0c32ef0c855e9c6f1fc64137d4f053053ceb7b52d13f5d090d54da52066e07\n");
	printf("calc midstate : %s\n", strMidstate);

	free(strMidstate);

//	be0c32ef0c855e9c6f1fc64137d4f053053ceb7b52d13f5d090d54da52066e07

}

// #529122
//
// Hash                       00000000000000000022093dd438ed47fa28e71858b8220d53e5cd83b8d0d442
//
// Version                    20000000
// Prev_Hash                  00000000000000000026087ab13d78b5062f38bceb8fe2dc14c53e6b25057933
// Merkle_Root                f42a1d6e97a8bc9f22e5516842b25ac4d53b2442b971b718a07e2c9d3085a5a5
// Time                       5B30707E (2018-06-25 04:34:14)
// Bits                       17376F56
// Nonce                      66cb3426

//	Midstate                  5f4d60a25385c407c2a84e0c259169c410a4a54b93f71708f1abdfec6e8b81d2

void TestMidstateGolden()
{
	sha256_ctx ctx;
	//	Input
	uint8_t input[64] = {
		// Version
		0x20,0x00,0x00,0x00,
		// Previous Has
		// 4    3    2    1    
		0x25,0x05,0x79,0x33,
		0x14,0xc5,0x3e,0x6b,
		0xeb,0x8f,0xe2,0xdc,
		0x06,0x2f,0x38,0xbc,
		0xb1,0x3d,0x78,0xb5,
		0x00,0x26,0x08,0x7a,
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,
		// Merkle Root
		// 4    3    2    1    
		0x30,0x85,0xa5,0xa5,
		0xa0,0x7e,0x2c,0x9d,
		0xb9,0x71,0xb7,0x18,
		0xd5,0x3b,0x24,0x42,
		0x42,0xb2,0x5a,0xc4,
		0x22,0xe5,0x51,0x68,
		0x97,0xa8,0xbc,0x9f,
	};

	uint8_t midstate[32];
	uint8_t data[64];
	uint32_t *data32 = (uint32_t*)data;

	flip64(data32, input);
	sha256_init(&ctx);
	sha256_update(&ctx, data, 64);
	memcpy(midstate, ctx.h, 32);

	char *strMidstate = bin2hex(midstate, sizeof(midstate));

	printf("expected midstate : 5f4d60a25385c407c2a84e0c259169c410a4a54b93f71708f1abdfec6e8b81d2\n");
	printf("calc midstate : %s\n", strMidstate);

	free(strMidstate);

}
