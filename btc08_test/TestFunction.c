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
	printf("     ex> 1 0 (default:4, available:0~6) \n");
	printf("  2. Verify vector data with vmask \n");
	printf("     ex> 2 0 (default:4, available:0~6) \n");
	printf("  3. Convert epoch time to human-readable date \n");
	printf("     ex> 3 1614131870 (default:1614131870) \n");
	printf("  4. Calculate target from sdiff (default:512.00)\n");
	printf("     ex> 4 8192.00 \n");
	printf("  5. Calculate current target from nbits\n");
	printf("-----------------------------\n");
	printf("  q. quit\n");
	printf("=============================\n");
}

/* Expected result is that nonce is found in Index0 and Index4.
 * To find a nonce in all vector data, change version information
 *  as rolled version value before testing.
 * ex> Index 6: replace 0x20,0x00,0x00,0x00 --> 0x2F,0xFF,0xE0,0x00 */
static int verify_hash(int idx)
{
	VECTOR_DATA data;
	uint32_t *p_nonce32;
	uint32_t nonce32;
	int ret = 0;

	if (idx >= MAX_NUM_VECTOR)
		idx = 0;

	NxDbgMsg(NX_DBG_INFO, "verify vector(index #%d)\n", idx);

	GetGoldenVector(idx, &data, 0);

	p_nonce32 = (uint32_t *)(data.nonce);
	memcpy(&nonce32, p_nonce32, sizeof(nonce32));
	HexDump("nonce32", &nonce32, 4);

	// remove golden nonce in header to use found nonce
	memset(data.data+76, 0, 4);
	// remove golden hash in header to regenerate hash with the found nonce
	memset(data.hash, 0, 32);

	HexDump("header", data.data, sizeof(data.data));

	// insert the found nonce to header and compare the current_target with regenerated hash
	if (submit_nonce(&data, nonce32))
		NxDbgMsg(NX_DBG_INFO, "=== Succeed to submit nonce! ===");
	else
		NxDbgMsg(NX_DBG_INFO, "=== Failed to submit nonce! ===");
}

/* Expected result is that nonce is found only in Index0, Index3 and Index4.
 * Others have not supported rolled version value.
 *   Index0(20000000), Index1(20400000), Index2(27FFE000), Index3(2000E000),
 *   Index4(20000000), Index5(00000002), Index6(2FFFE000) */
static int verify_hash_with_vmask(int idx)
{
	VECTOR_DATA data;
	uint32_t *p_nonce32;
	uint32_t nonce32;
	char core[512];
	int ret = 0;

	if (idx >= MAX_NUM_VECTOR)
		idx = 0;

	NxDbgMsg(NX_DBG_INFO, "verify vector with vmask(index #%d)\n", idx);

	GetGoldenVectorWithVMask(idx, &data, 0);

	p_nonce32 = (uint32_t *)(data.nonce);
	memcpy(&nonce32, p_nonce32, sizeof(nonce32));
	HexDump("nonce32", &nonce32, 4);

	// remove golden nonce in header to use found nonce
	memset(data.data+76, 0, 4);
	// remove golden hash in header to regenerate hash with the found nonce
	memset(data.hash, 0, 32);

	for (int i=0; i<4; i++)
	{
		memcpy(&data, &(data.vmask_001[(1<<i)]), 4);

		sprintf(core, "header for Inst_%s", (i==0) ?
			"Upper":(((i==1) ? "Lower": ((i==2) ? "Lower_2":"Lower_3"))));
		HexDump(core, data.data, sizeof(data.data));

		if (submit_nonce(&data, nonce32))
			ret = 1;
	}

	if (0 < ret)
		NxDbgMsg(NX_DBG_INFO, "=== Succeed to submit nonce! ===");
	else
		NxDbgMsg(NX_DBG_INFO, "=== Failed to submit nonce! ===");

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

void calc_btc08_target(uint8_t *dest_target, uint32_t nbits)
{
	uint32_t *nbits_ptr = (uint32_t *)dest_target;
	*nbits_ptr = bswap_32(nbits);
	HexDump("dest_target", dest_target, 4);

	uint8_t select0, select1, shift = 0;
	select0 = (dest_target[0] / 4) - 1;
	select1 = (dest_target[0] % 4) + 1;
	dest_target[4] = select0;
	dest_target[5] = select1<<4 | (shift&0xF);

	HexDump("select", dest_target+4, 2);
}

/* calculate Bits(4bytes) from Target(32bytes) */
/* Bits(4bytes) in block header --> convert Bits to Target(32bytes) --> Difficulty
 *  difficulty: 1.000000 <--> target: 0000000000000000000000000000000000000000000000000000FFFF00000000
 *  difficulty: 8192.00  <--> target: 000000000000000000000000000000000000000000000000F8FF070000000000
 *  difficulty: 1638.00  <--> target: 0000000000000000000000000000000000000000265882255802280000000000
 *  difficulty: 512.00   <--> target: 00000000000000000000000000000000000000000000000080FF7F0000000000 */
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

uint32_t get_diff(double diff)
{
	uint32_t n_bits;
	int shift = 29;
	double f = (double) 0x0000ffff / diff;
	while (f < (double) 0x00008000) {
		shift--;
		f *= 256.0;
	}
	while (f >= (double) 0x00800000) {
		shift++;
		f /= 256.0;
	}
	n_bits = (int) f + (shift << 24);
	return n_bits;
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

void gen_real_target(const unsigned char *target)
{
	uint32_t *target32 = (uint32_t *)target;
	unsigned char target_swap[32];
	char *target_str;

	for (int i = 28 / 4; i >= 0; i--) {
		uint32_t t32tmp = le32toh(target32[i]);
	}

	swab256(target_swap, target);
	target_str = bin2hex(target_swap, 32);
	NxDbgMsg(NX_DBG_INFO, "Target: %s\n", target_str);
}

/* calculate target(32bytes) from sdiff
 * ex. diff 512.000000 --> target 00000000000000000000000000000000000000000000000080ff7f0000000000 */
void target_from_diff(unsigned char *dest_target, double diff)
{
	unsigned char target[32];
	uint64_t *data64, h64;
	double d64, dcut64;
	uint32_t nbits;
	double diff2;
	char *htarget;
	uint8_t btc08_target[6];

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
	NxDbgMsg(NX_DBG_INFO, "Generated target %s from sdiff %lf\n", htarget, diff);
	free(htarget);

	memcpy(dest_target, target, 32);

	gen_real_target(target);

	diff2 = diff_from_target(target);
	NxDbgMsg(NX_DBG_INFO, "%3s difficulty from target: %lf\n", "", diff2);

	// calc nbits from target
	nbits = nbits_from_target(target);
	NxDbgMsg(NX_DBG_INFO, "%3s nbits from target: %04x\n", "", nbits);

	calc_btc08_target(btc08_target, nbits);
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
			char *s_sdiff = "512.00";
			double sdiff = 512.00;
			unsigned char target[32];
			if( cmdCnt > 1 )
			{
				sdiff = strtod(cmd[1], &s_sdiff);
			}
			NxDbgMsg(NX_DBG_INFO, "sdiff:%lf\n", sdiff);
			target_from_diff(target, sdiff);	// set_target
		}
		//----------------------------------------------------------------------
		// Calculate current target from nbits
		else if( !strcasecmp(cmd[0], "5") )
		{
			// genesis nbits: 0x1D00FFFF
			// htarget: 00000000ffff0000000000000000000000000000000000000000000000000000
			uint8_t nbits[4] = {0x17, 0x0B, 0xEF, 0x93};
			uint32_t *nbits_32 = (uint32_t *)nbits;
			uint8_t target[32] = {0x00,};
			unsigned char *htarget;

			target_from_nbits(nbits, target);
			htarget = bin2hex(target, 32);
			HexDump("nbits", nbits, 4);
			NxDbgMsg(NX_DBG_INFO, "htarget:%s\n", htarget);
			free(htarget);
		}
	}
}