#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <byteswap.h>
#include <stdlib.h>
#include <stdio.h>

#include "Utils.h"
#include "TestVector.h"

#ifdef NX_DTAG
#undef NX_DTAG
#endif
#define NX_DTAG "[TestFunction]"
#include "NX_DbgMsg.h"

/* truediffone == 0x00000000FFFF0000000000000000000000000000000000000000000000000000
 * Generate a 256 bit binary LE target by cutting up diff into 64 bit sized
 * portions or vice versa. */
static const double truediffone = 26959535291011309493156476344723991336010898738574164086137773096960.0;
static const double bits192     = 6277101735386680763835789423207666416102355444464034512896.0;
static const double bits128     = 340282366920938463463374607431768211456.0;
static const double bits64      = 18446744073709551616.0;

static void testfunction_command_list()
{
	printf("\n\n");
	printf("====== Test function =======\n");
	printf("  1. Verify vector data \n");
	printf("  2. Verify vector data with vmask \n");
	printf("  3. Convert epoch time to human-readable date \n");
	printf("     ex> 3 1614131870 (default:1614131870) \n");
	printf("  4. Calculate target from Bit (default:512.00)\n");
	printf("     ex> 4 8192.00 \n");
	printf("  5. Calculate current target from nbits\n");
	printf("-----------------------------\n");
	printf("  q. quit\n");
	printf("=============================\n");
}

static int verify_hash(int idx)
{
	VECTOR_DATA data;
	uint32_t *nonce32_org;
	uint32_t nonce32;
	unsigned char hash_target[32];

	NxDbgMsg(NX_DBG_INFO, "verify vector(index #%d)\n", idx);

	GetGoldenVector(idx, &data, 0);

	// generate current_target from nbits
	memcpy(data.nbits, data.target, 4);
	target_from_nbits(data.nbits, hash_target);
	swab256(data.current_target, hash_target);

	nonce32_org = (uint32_t *)(data.nonce);
	memcpy(&nonce32, nonce32_org, sizeof(nonce32));
	HexDump("nonce32", &nonce32, 4);

	// remove golden nonce in header to use found nonce
	memset(data.data+76, 0, 4);
	// remove golden hash in header to regenerate hash with the found nonce
	memset(data.hash, 0, 32);

	HexDump("header", data.data, sizeof(data.data));

	// insert the found nonce to header and compare the current_target with regenerated hash
	submit_nonce(&data, nonce32);
}

static int verify_hash_with_vmask(int idx)
{
	// TODO

	return 0;
}

static int get_readable_date(time_t epoch)
{
	char buf[80];
	struct tm ts;

    // time format example: "2021-02-24 Wed 10:57:50 KST"
    ts = *localtime(&epoch);
    strftime(buf, sizeof(buf), "%Y-%m-%d %a %H:%M:%S %Z", &ts);
    NxDbgMsg(NX_DBG_INFO, "%s\n", buf);

	return 0;
}

/* Bits(4bytes) in block header --> convert Bits to Target(32bytes) --> Difficulty
 *  nbits(0x1D00FFFF) --> difficulty: 1.000000 <--> target: 0000000000000000000000000000000000000000000000000000FFFF00000000
 *                        difficulty: 8192.00  <--> target: 000000000000000000000000000000000000000000000000F8FF070000000000
 *                        difficulty: 1638.00  <--> target: 0000000000000000000000000000000000000000265882255802280000000000
 *                        difficulty: 512.00   <--> target: 00000000000000000000000000000000000000000000000080FF7F0000000000 */

// converts a little endian 256 bit value to a double
static double le256todouble(const void *target)
{
	uint64_t *data64;
	double dcut64;

	data64 = (uint64_t *)(target + 24);
	dcut64 = le64toh(*data64) * bits192;

	data64 = (uint64_t *)(target + 16);
	dcut64 += le64toh(*data64) * bits128;

	data64 = (uint64_t *)(target + 8);
	dcut64 += le64toh(*data64) * bits64;

	data64 = (uint64_t *)(target);
	dcut64 += le64toh(*data64);

	return dcut64;
}

/* calculate Bits(4bytes) from Target(32bytes) */
static uint32_t nbits_from_target(unsigned char *target)
{
	uint32_t ret = 0;
	int ii= 31;

	while(target[ii--]==0);
	ii++;

	if(target[ii-2] == 0) ii++;

	ret = (ii+1)<<24;
	ret |= target[ii-0]<<16;
	ret |= target[ii-1]<< 8;
	ret |= target[ii-2]<< 0;

	return ret;
}

/* calculate difficulty from current_target(32bytes)
 * maximum_target(diff 1) : 0x00000000FFFF0000000000000000000000000000000000000000000000000000
 * Difficulty = maximum_target / current_target */
static double diff_from_target(void *target)
{
	double d64, dcut64;

	d64 = truediffone;                // maximum_target
	dcut64 = le256todouble(target);   // current_target
	if (!dcut64)
		dcut64 = 1;
	return d64 / dcut64;
}

// calculate target(32bytes) from difficulty
// ex. diff 512.000000 --> target 00000000000000000000000000000000000000000000000080ff7f0000000000
void target_from_diff(double diff)
{
	unsigned char target[32];
	uint64_t *data64, h64;
	double d64, dcut64;
	uint32_t nbits;
	double diff2;
	char *htarget;

	if (diff == 0.0) {
		/* This shouldn't happen but best we check to prevent a crash */
		NxDbgMsg(NX_DBG_ERR, "Diff zero passed to set_target");
		diff = 1.0;
	}

	d64 = truediffone;
	d64 /= diff;

	dcut64 = d64 / bits192;
	h64 = dcut64;
	data64 = (uint64_t *)(target + 24);
	*data64 = htole64(h64);
	dcut64 = h64;
	dcut64 *= bits192;
	d64 -= dcut64;

	dcut64 = d64 / bits128;
	h64 = dcut64;
	data64 = (uint64_t *)(target + 16);
	*data64 = htole64(h64);
	dcut64 = h64;
	dcut64 *= bits128;
	d64 -= dcut64;

	dcut64 = d64 / bits64;
	h64 = dcut64;
	data64 = (uint64_t *)(target + 8);
	*data64 = htole64(h64);
	dcut64 = h64;
	dcut64 *= bits64;
	d64 -= dcut64;

	h64 = d64;
	data64 = (uint64_t *)(target);
	*data64 = htole64(h64);

	// print target
	htarget = bin2hex(target, 32);
	NxDbgMsg(NX_DBG_INFO, "target %s from diff %lf\n", htarget, diff);
	free(htarget);

	diff2 = diff_from_target(target);
	NxDbgMsg(NX_DBG_INFO, "diff2: %lf\n", diff2);

	// calc nbits from target
	nbits = nbits_from_target(target);
	NxDbgMsg(NX_DBG_INFO, "nbits: %04x\n", nbits);
}

void FuntionTestLoop(void)
{
	static char cmdStr[NX_SHELL_MAX_ARG * NX_SHELL_MAX_STR];
	static char cmd[NX_SHELL_MAX_ARG][NX_SHELL_MAX_STR];
	int cmdCnt;

	for( ;; )
	{
		testfunction_command_list();
		printf( "function > " );
		fgets( cmdStr, NX_SHELL_MAX_ARG*NX_SHELL_MAX_STR - 1, stdin );
		cmdCnt = Shell_GetArgument( cmdStr, cmd );

		//----------------------------------------------------------------------
		if( !strcasecmp(cmd[0], "q") )
		{
			break;
		}
		//----------------------------------------------------------------------
		// Verify nonce
		else if( !strcasecmp(cmd[0], "1") )
		{
			int idx = 0;
			if( cmdCnt > 1 )
			{
				idx = strtol(cmd[1], 0, 10);
			}
			NxDbgMsg(NX_DBG_INFO, "idx: %d\n", idx);
			verify_hash(idx);
		}
		//----------------------------------------------------------------------
		// Verify nonce with vmask
		else if( !strcasecmp(cmd[0], "2") )
		{
			int idx = 4;
			if( cmdCnt > 1 )
			{
				idx = strtol(cmd[1], 0, 10);
			}
			NxDbgMsg(NX_DBG_INFO, "idx: %d\n", idx);
			verify_hash_with_vmask(idx);
		}
		//----------------------------------------------------------------------
		// Convert epoch time to human-readable date
		else if( !strcasecmp(cmd[0], "3") )
		{
			time_t epoch = 1614131870;
			if( cmdCnt > 1 )
			{
				epoch = strtol(cmd[1], 0, 10);
			}
			get_readable_date(epoch);
		}
		//----------------------------------------------------------------------
		// Calculate target from diff
		else if( !strcasecmp(cmd[0], "4") )
		{
			// ex> diff: 8192.00, 1638.00, 512.00, etc
			char *s_diff = "512.00";
			double diff = 512.00;
			if( cmdCnt > 1 )
			{
				diff = strtod(cmd[1], &s_diff);
			}
			NxDbgMsg(NX_DBG_INFO, "diff:%lf\n", diff);
			target_from_diff(diff);
		}
		//----------------------------------------------------------------------
		// Calculate current target from nbits
		else if( !strcasecmp(cmd[0], "5") )
		{
			// genesis nbits: 0x1D00FFFF
			uint8_t nbits[4] = {0x1D, 0x00, 0xFF, 0xFF};
			uint8_t target[32] = {0x00,};
			unsigned char *htarget;  // 00000000ffff0000000000000000000000000000000000000000000000000000

			target_from_nbits(nbits, target);

			htarget = bin2hex(target, 32);
			NxDbgMsg(NX_DBG_INFO, "nbits:0x%8x ==> htarget:%s\n", (uint32_t *)nbits, htarget);
			free(htarget);
		}
	}
}