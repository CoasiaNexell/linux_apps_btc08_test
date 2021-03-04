#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <byteswap.h>
#include <sched.h>
#include <stdbool.h>

#include "Utils.h"
#include "Btc08.h"

#include "TestVector.h"

#ifdef NX_DTAG
#undef NX_DTAG
#endif
#define NX_DTAG "[SimpleWork]"
#include "NX_DbgMsg.h"

#define DEBUG	0

static int numCores[MAX_CHIP_NUM] = {0,};

static void simplework_command_list()
{
	printf("\n\n");
	printf("======= Simple Work =========\n");
	printf("  1. Single Work\n");
	printf("  2. Work Loop\n");
	printf("    ex > 2 [number]\n");
	printf("-----------------------------\n");
	printf("  q. quit\n");
	printf("=============================\n");
}

static void RunBist( BTC08_HANDLE handle)
{
	uint8_t *ret;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	NxDbgMsg( NX_DBG_INFO, "=== RUN BIST ==\n");

	Btc08WriteParam (handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);

	// Set the golden nonce instead of the nonce range
	Btc08WriteNonce (handle, BCAST_CHIP_ID, golden_nonce, golden_nonce);
	Btc08SetDisable (handle, BCAST_CHIP_ID, golden_enable);
	Btc08RunBist    (handle, default_golden_hash, default_golden_hash, default_golden_hash, default_golden_hash);

	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		// If it's not BUSY status, read the number of cores in next READ_BIST
		for (int i=0; i<10; i++) {
			ret = Btc08ReadBist(handle, chipId);
			if ( (ret[0] & 1) == 0 )
				break;
			else
				NxDbgMsg( NX_DBG_INFO, "%5s ChipId = %d, Status = %s, Number of cores = %d\n", "",
						chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );

			usleep( 300 );
		}
		ret = Btc08ReadBist(handle, chipId);
		handle->numCores[chipId-1] = ret[1];
		NxDbgMsg( NX_DBG_INFO, "%5s ChipId = %d, Status = %s, Number of cores = %d\n", "",
					chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );
	}
	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
		NxDbgMsg( NX_DBG_INFO, "%5s ChipId = %d, Number of jobs = %d\n", "",
					chipId, (res[2]&7) );
	}
}

static uint32_t calRealGN(const uint8_t chipId, uint32_t *in, const uint8_t valid_cnt)
{
	uint32_t *gn = in;

	NxDbgMsg(NX_DBG_INFO, "gn[0x%08x] cal[0x%08x] \n",
			*gn, (*gn - valid_cnt * numCores[chipId]));

	return (*gn - valid_cnt * numCores[chipId]);
}

/* 0x12345678 --> 0x56781234*/
static inline void swap16_(void *dest_p, const void *src_p)
{
	uint32_t *dest = dest_p;
	const uint32_t *src = src_p;

	*dest =     ((*src & 0xFF) << 16) |     ((*src & 0xFF00) << 16) |
			((*src & 0xFF0000) >> 16) | ((*src & 0xFF000000) >> 16);
}

static int handleGN(BTC08_HANDLE handle, uint8_t chipId)
{
	int ret = 0;
	uint8_t hash[128] = {0x00,};
	unsigned int hash_size = sizeof(hash)/sizeof(hash[0]);
	uint8_t res[18] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	bool lower3, lower2, lower, upper, validCnt;
	int match;
	uint32_t cal_gn;
	uint32_t *res_32;
	uint32_t swap16[4];
	uint32_t nonce32[4];
	uint32_t *default_gn = (uint32_t *)golden_nonce;	// default gn: 0x66 0xcb 0x34 0x26 ==> 0x2634cb66
	char title[512];

	// Sequence 1. Read Hash
	Btc08ReadHash(handle, chipId, hash, hash_size);

#if DEBUG
	for (int i=0; i<4; i++) {
		sprintf(title, "Inst_%s", (i==0) ? "Lower_3":(((i==1) ? "Lower_2": ((i==2) ? "Lower":"Upper"))));
		HexDump(title, &(hash[i*32]), 32);
	}
#endif
	for (int i=0; i<4; i++)
	{
		ret = memcmp(default_golden_hash, &(hash[i*32]), 32);
		if (ret == 0)
			NxDbgMsg(NX_DBG_INFO, "%5s Hash matched of Inst_%s!!!\n", "",
					(i==0) ? "Lower_3":(((i==1) ? "Lower_2": ((i==2) ? "Lower":"Upper"))));
		else if (ret < 0)
			NxDbgMsg(NX_DBG_INFO, "%5s golden hash is %s found hash of Inst_%s\n", "",
					(i==0) ? "Lower_3":(((i==1) ? "Lower_2": ((i==2) ? "Lower":"Upper"))),
					(ret < 0) ? "less than":"greater than");
	}

	// Sequence 2. Read Result to read GN and clear GN IRQ
	Btc08ReadResult(handle, chipId, res, res_size);
	lower3 = ((res[0] & (1<<3)) != 0);
	lower2 = ((res[0] & (1<<2)) != 0);
	lower  = ((res[0] & (1<<1)) != 0);
	upper  = ((res[0] & (1<<0)) != 0);
	validCnt = res[1];

	NxDbgMsg(NX_DBG_INFO, "%5s [%s %s %s %s found golden nonce ]\n", "",
			lower3 ? "Inst_Lower_3,":"", lower2 ? "Inst_Lower_2,":"",
			lower  ?   "Inst_Lower,":"", upper  ?   "Inst_Upper":"");
	for (int i=0; i<16; i+=4) {
		for (int j=0; j<4; j++) NxDbgMsg(NX_DBG_DEBUG, "res[%d]=0x%02x \n", (2+i+j), res[2+i+j]);
	}

	res_32 = (uint32_t *)&(res[2]);		// res_32[0]: 0xcb662634

	for (int i=0; i<4; i++)
	{
		if (0 != res_32[i])
		{
			/*  res[2]=0x34,  res[3]=0x26,  res[4]=0x66,  res[5]=0xcb  ...
			 * res[14]=0x34, res[15]=0x26, res[16]=0x00, res[17]=0x0f
			 * ==> nonce32[0]=0xcb662634 ... nonce32[3]=0x0f002634         */
			nonce32[i] = res_32[i];
			swap16_(&nonce32[i], &res_32[i]);
			// TODO: Check if it needs to compare the calculated gn with the default/real gn
			// calRealGN(chipId, &(nonce32[i]), validCnt);
			if (nonce32[i] == *default_gn)
			{
				NxDbgMsg(NX_DBG_INFO, "%5s GN Matched!!! %10s Inst_%s found GN 0x%08x \n", "", "",
					(i==0) ? "Lower_3":(((i==1) ? "Lower_2": ((i==2) ? "Lower":"Upper"))),
					nonce32[i]);
			}
			else
			{
				NxDbgMsg(NX_DBG_ERR, "%5s GN not Matched!!! %21s 0x%08x\n", "", "", nonce32[i]);
				ret = -1;
			}
		}
	}

	return ret;
}

static int handleOON(BTC08_HANDLE handle)
{
	int ret = 0;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	// Read ID to get FIFO status for chipId#1
	// The FIFO status of all chips are same.
	if (0 == Btc08ReadId (handle, 1, res, res_size))
	{
		int numJobs, numLeftFifo;

		numJobs = (res[2] & 0x07);		// res[2]: [10:8] Number of jobs in FIFO
		numLeftFifo = MAX_JOB_FIFO_NUM - numJobs;

		// If FIFO is not full, clear OON IRQ and then Run job for a work
		if (0 == numLeftFifo)
		{
			NxDbgMsg(NX_DBG_INFO, "FIFO is full!\n");
			ret = -1;
		}
		else
		{
			Btc08ClearOON(handle, BCAST_CHIP_ID);
			ret = 0;
		}
	}

	return ret;
}


/*
 * Process one work(nonce range: 2G) without asicboost
 *  Sequence :
 *   1. Create Handle
 *   2. Find number of chips : using AutoAddress
 *   3. Find number of cores of individual chips
 *   4. Setting Parameters
 *      a. midstate parameter
 *      b. default golden data : well known ( MerkleRoot's tail, Time, Target Difficulty(nBits) )
 *   5. Setting Target Parameters
 *      Target Parameter : Target Difficulty(4bytes) + Compare Shift Value(2bytes)
 *   6. Setting Nonce Range
 *      a. start nonce
 *      b. end nonce
 *   7. Run Job
 *   8. Check Interrupt Signals(GPIO) and Post Processing
 *      Post processing : actually don't need to current work.
 */

static void TestWork()
{
	int chipId = 0x00, jobId=0x01;
	struct timespec ts_start, ts_oon, ts_gn, ts_diff;
	uint8_t fifo_full = 0x00, oon_irq = 0x00, gn_irq = 0x00;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	uint8_t start_nonce[4] = { 0x60, 0x00, 0x00, 0x00 };
	uint8_t end_nonce[4]   = { 0x6f, 0xff, 0xff, 0xff };

	tstimer_time(&ts_start);
	NxDbgMsg(NX_DBG_INFO, "=== Start of TestWork! === [%ld.%lds]\n", ts_start.tv_sec, ts_start.tv_nsec);

	// Seqeunce 1. Create Handle
	BTC08_HANDLE handle = CreateBtc08(0);
	Btc08ResetHW(handle, 1);
	Btc08ResetHW(handle, 0);

	// Seqeunce 2. Find number of chips : using AutoAddress
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_DEBUG, "%5s Number of Chips = %d\n", "", handle->numChips);
	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
		NxDbgMsg( NX_DBG_DEBUG, "%5s ChipId = %d, Number of jobs = %d\n",
					"", chipId, (res[2]&7) );
	}

	// Sequence 3. Reset S/W
	Btc08Reset(handle);

	// Sequence 4. Set last chip
	Btc08SetControl(handle, 1, LAST_CHIP);
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "%5s Number of Chips = %d\n", "", handle->numChips);
	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
		NxDbgMsg( NX_DBG_DEBUG, "%5s ChipId = %d, Number of jobs = %d\n",
					"", chipId, (res[2]&7) );
	}

	// Sequence 5. Enable all cores
	Btc08SetDisable (handle, BCAST_CHIP_ID, golden_enable);

	// Seqeunce 6. Run BIST
	RunBist( handle );

	//
	// RUN_JOB for one work without asicboost  : Please FIXME
	//	

	// Sequence 7. Enable OON IRQ/Set UART divider (tb:32'h0000_8013, sh:0018_001f)
	Btc08SetControl(handle, BCAST_CHIP_ID, (OON_IRQ_EN | UART_DIVIDER));

	// Seqeunce 8. Setting Parameters
	Btc08WriteParam(handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);

	// Seqeunce 9. Setting Target Parameters
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);

	// Sequence 10. Setting Nonce Range
	Btc08WriteNonce(handle, BCAST_CHIP_ID, start_nonce, end_nonce);

	NxDbgMsg( NX_DBG_INFO, "=== RUN JOB ==\n");

	// Sequence 11. Run Job
	Btc08RunJob(handle, BCAST_CHIP_ID, ASIC_BOOST_EN, jobId++);
	Btc08RunJob(handle, BCAST_CHIP_ID, ASIC_BOOST_EN, jobId++);
	Btc08RunJob(handle, BCAST_CHIP_ID, ASIC_BOOST_EN, jobId++);
	Btc08RunJob(handle, BCAST_CHIP_ID, ASIC_BOOST_EN, jobId++);
	// Sequence 12. Check Interrupt Signals(GPIO) and Post Processing

	//	FIXME : This loop is basically busy wait type,
	//	If possible, is should be changed using a interrupt.
	while(1)
	{
		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_GN))	// Check GN GPIO pin
		{
			Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
			gn_irq 	  = res[2] & (1<<0);
			chipId    = res[3];

			if (0 != gn_irq) {				// In case of GN
				// Check if found GN(0x66cb3426) is correct and submit nonce to pool server and then go loop again
				tstimer_time(&ts_gn);
				NxDbgMsg(NX_DBG_INFO, "%5s === GN IRQ on chip#%d!!! === [%ld.%lds]\n", "", chipId, ts_gn.tv_sec, ts_gn.tv_nsec);

				handleGN(handle, chipId);
			} else {						// In case of not GN
				NxDbgMsg(NX_DBG_INFO, "%5s === H/W interrupt occured, but GN_IRQ value is not set!\n", "");
			}
		}

		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_OON))	// Check OON
		{
			// TODO: Need to check if it needs to read job id
			Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
			oon_irq	  = res[2] & (1<<1);
			chipId    = res[3];

			if (0 != oon_irq) {		// In case of OON
				tstimer_time(&ts_oon);
				NxDbgMsg(NX_DBG_INFO, "%5s === OON IRQ on chip#%d!!! === [%ld.%lds]\n", "", chipId, ts_oon.tv_sec, ts_oon.tv_nsec);

				handleOON(handle);
				break;
			} else {				// In case of not OON
				NxDbgMsg(NX_DBG_INFO, "%5s === OON IRQ is not set!\n", "");
				// if oon timeTestWorkLoop_JobDist
			}
		}
		sched_yield();
	}

	tstimer_diff(&ts_oon, &ts_start, &ts_diff);

	NxDbgMsg(NX_DBG_INFO, "=== End of TestWork === [%ld.%lds]\n", ts_diff.tv_sec, ts_diff.tv_nsec);

	DestroyBtc08( handle );
}


/* Process 4 works with asicboost at first.
 * If OON IRQ occurs, clear OON and then pass the additional work to job fifo.
 * If GN IRQ occurs, read GN and then clear GN IRQ.
 */
static void TestWorkLoop(int numWorks)
{
	uint8_t chipId = 0x00, jobId = 0x01, jobcnt = 0x01;
	uint8_t fifo_full = 0x00, oon_irq = 0x00, gn_irq = 0x00;
	uint8_t res[4] = {0x00,};
	uint8_t oon_jobid, gn_jobid;
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	uint8_t start_nonce[4] = { 0x60, 0x00, 0x00, 0x00 };
	uint8_t end_nonce[4]   = { 0x6f, 0xff, 0xff, 0xff };
	struct timespec ts_start, ts_oon, ts_gn, ts_diff;

	tstimer_time(&ts_start);
	NxDbgMsg(NX_DBG_INFO, "=== Start workloop === [%ld.%lds]\n", ts_start.tv_sec, ts_start.tv_nsec);

	// Seqeunce 1. Create Handle
	BTC08_HANDLE handle = CreateBtc08(0);

	Btc08ResetHW(handle, 1);
	Btc08ResetHW(handle, 0);

	// Seqeunce 2. Find number of chips : using AutoAddress
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "(Before last chip) Number of Chips = %d\n", handle->numChips);
	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
#if DEBUG
		NxDbgMsg( NX_DBG_DEBUG, "ChipId = %d, Number of jobs = %d\n",
					chipId, (res[2]&7) );
#endif
	}

	// Sequence 3. Reset S/W
	Btc08Reset(handle);

	// Sequence 4. Set last chip
	Btc08SetControl(handle, 1, LAST_CHIP);
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "(After last chip) Number of Chips = %d\n", handle->numChips);
	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
#if DEBUG
		NxDbgMsg( NX_DBG_DEBUG, "ChipId = %d, Number of jobs = %d\n",
					chipId, (res[2]&7) );
#endif
	}

	// Sequence 5. Enable all cores
	Btc08SetDisable (handle, BCAST_CHIP_ID, golden_enable);

	// Seqeunce 6. Find number of cores of individual chips
	RunBist( handle );

	// Sequence 7. Enable OON IRQ/Set UART divider
	Btc08SetControl(handle, BCAST_CHIP_ID, (OON_IRQ_EN | UART_DIVIDER));

	// Sequence 8. Setting parameters, target, nonce range
	Btc08WriteParam(handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
	Btc08WriteNonce(handle, BCAST_CHIP_ID, start_nonce, end_nonce);

	tstimer_time(&ts_start);
	NxDbgMsg( NX_DBG_INFO, "=== RUN JOB === [%ld.%lds]\n", ts_start.tv_sec, ts_start.tv_nsec);

	// Sequence 9. Run job
	for (int i = 0; i < MAX_JOB_FIFO_NUM; i++)
	{
		NxDbgMsg(NX_DBG_INFO, "%2s Run Job with jobId = %d\n", "", jobId);
		Btc08RunJob(handle, BCAST_CHIP_ID, ASIC_BOOST_EN, jobId++);
		jobcnt++;
	}

	while(1)
	{
		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_GN))	// Check GN GPIO pin
		{
			Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
			gn_irq 	  = res[2] & (1<<0);
			chipId    = res[3];

			if (0 != gn_irq) {		// If GN IRQ is set, then handle GN
				// Check if found GN(0x66cb3426) is correct and submit nonce to pool server and then go loop again
				tstimer_time(&ts_gn);
				NxDbgMsg(NX_DBG_INFO, "%5s === GN IRQ on chip#%d!!! === [%ld.%lds]\n", "", chipId, ts_gn.tv_sec, ts_gn.tv_nsec);

				handleGN(handle, chipId);
			} else {				// If GN IRQ is not set, then go to check OON
				NxDbgMsg(NX_DBG_INFO, "%5s === H/W GN occured but GN_IRQ value is not set!\n", "");
			}
		}

		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_OON))	// Check OON
		{
			// TODO: Need to check if it needs to read job id
			Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
			oon_irq	  = res[2] & (1<<1);
			chipId    = res[3];

			if (0 != oon_irq) {				// If OON IRQ is set, handle OON
				tstimer_time(&ts_oon);
				NxDbgMsg(NX_DBG_INFO, "%5s === OON IRQ on chip#%d!!! === [%ld.%lds]\n", "", chipId, ts_oon.tv_sec, ts_oon.tv_nsec);

				handleOON(handle);

				if (numWorks >= jobcnt)
				{
					NxDbgMsg(NX_DBG_INFO, "%2s Run Job with jobId = %d\n", "", jobId);
					Btc08RunJob(handle, BCAST_CHIP_ID, ASIC_BOOST_EN, jobId++);
					jobcnt++;
					if (jobId > MAX_JOB_ID)		// [7:0] job id ==> 8bits (max 256)
						jobId = 1;
				}
				else {
					break;
				}
			} else {						// OON IRQ is not set (cgminer: check OON timeout is expired)
				NxDbgMsg(NX_DBG_INFO, "%5s === OON IRQ is not set! ===\n", "");
				// if oon timeout is expired, disable chips
				// if oon timeout is not expired, check oon gpio again
			}
		}

		sched_yield();
	}

	tstimer_diff(&ts_oon, &ts_start, &ts_diff);
	NxDbgMsg(NX_DBG_INFO, "Total works = %d [%ld.%lds]\n", (jobcnt-1), ts_diff.tv_sec, ts_diff.tv_nsec);

	DestroyBtc08( handle );
}

/* Process 4 works with asicboost at first.
 * If OON IRQ occurs, clear OON and then pass the additional work to job fifo.
 * If GN IRQ occurs, read GN and then clear GN IRQ.
 */
//	Job Distribution (nonce)

static void DistributionNonce( BTC08_HANDLE handle )
{
	int ii;
	uint32_t totalCores = 0;
	uint32_t noncePerCore;
	for( int i=0 ; i<handle->numChips ; i++ )
	{
		totalCores += handle->numCores[i];
	}
	NxDbgMsg( NX_DBG_INFO, "Total Cores = %d\n", totalCores );

	noncePerCore = 0xffffffff / totalCores;
	handle->startNonce[0] = 0;
	for( ii=0 ; ii<handle->numChips-1 ; ii++ ) {
		handle->endNonce[ii] = handle->startNonce[ii] + (noncePerCore*handle->numCores[ii]);
		handle->startNonce[ii+1] = handle->endNonce[ii]+1;
	}
	handle->endNonce[ii]=0xffffffff;

}
static void TestWorkLoop_JobDist(int numWorks)
{
	int ii;
	uint8_t chipId = 0x00, jobId = 0x01, jobcnt = 0x01;
	uint8_t fifo_full = 0x00, oon_irq = 0x00, gn_irq = 0x00;
	uint8_t res[4] = {0x00,};
	uint8_t oon_jobid, gn_jobid;
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	uint8_t start_nonce[4] = { 0x50, 0x00, 0x00, 0x00 };
	uint8_t end_nonce[4]   = { 0x6f, 0xff, 0xff, 0xff };
	struct timespec ts_start, ts_oon, ts_gn, ts_diff;

	VECTOR_DATA data;

	tstimer_time(&ts_start);
	NxDbgMsg(NX_DBG_INFO, "=== Start workloop === [%ld.%lds]\n", ts_start.tv_sec, ts_start.tv_nsec);

	// Seqeunce 1. Create Handle
	BTC08_HANDLE handle = CreateBtc08(0);

	Btc08ResetHW(handle, 1);
	Btc08ResetHW(handle, 0);

	// Seqeunce 2. Find number of chips : using AutoAddress
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "(Before last chip) Number of Chips = %d\n", handle->numChips);
	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
#if DEBUG
		NxDbgMsg( NX_DBG_DEBUG, "ChipId = %d, Number of jobs = %d\n",
					chipId, (res[2]&7) );
#endif
	}

	// Sequence 3. Reset S/W
	Btc08Reset(handle);

	// Sequence 4. Set last chip
	Btc08SetControl(handle, 1, LAST_CHIP);
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "(After last chip) Number of Chips = %d\n", handle->numChips);
	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
#if DEBUG
		NxDbgMsg( NX_DBG_DEBUG, "ChipId = %d, Number of jobs = %d\n",
					chipId, (res[2]&7) );
#endif
	}

	// Sequence 5. Enable all cores
	Btc08SetDisable (handle, BCAST_CHIP_ID, golden_enable);

	// Seqeunce 6. Find number of cores of individual chips
	RunBist( handle );
	DistributionNonce(handle);		//	Distribution Nonce

	// Sequence 7. Enable OON IRQ/Set UART divider
	Btc08SetControl(handle, BCAST_CHIP_ID, (OON_IRQ_EN | UART_DIVIDER));

	GetGoldenVector(0, &data);

	// Sequence 8. Setting parameters, target, nonce range
	Btc08WriteParam(handle, BCAST_CHIP_ID, data.midState, data.parameter);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, data.target);

	for( int i=0; i<handle->numChips ; i++ )
	{
		NxDbgMsg( NX_DBG_INFO, "Chip[%d:%d] : 0x%08x ~ 0x%08x\n", i, handle->numCores[i], handle->startNonce[i], handle->endNonce[i] );
		start_nonce[0] = (handle->startNonce[i]>>24) & 0xff;
		start_nonce[1] = (handle->startNonce[i]>>16) & 0xff;
		start_nonce[2] = (handle->startNonce[i]>>8 ) & 0xff;
		start_nonce[3] = (handle->startNonce[i]>>0 ) & 0xff;
		end_nonce[0]   = (handle->endNonce[i]>>24) & 0xff;
		end_nonce[1]   = (handle->endNonce[i]>>16) & 0xff;
		end_nonce[2]   = (handle->endNonce[i]>>8 ) & 0xff;
		end_nonce[3]   = (handle->endNonce[i]>>0 ) & 0xff;
		// HexDump("Start Nonce", start_nonce, sizeof(start_nonce));
		// HexDump("End Nonce", end_nonce, sizeof(end_nonce));
		Btc08WriteNonce(handle, i+1, start_nonce, end_nonce);
	}

	tstimer_time(&ts_start);
	NxDbgMsg( NX_DBG_INFO, "=== RUN JOB === [%ld.%lds]\n", ts_start.tv_sec, ts_start.tv_nsec);

	// Sequence 9. Run job
	for (int i = 0; i < MAX_JOB_FIFO_NUM; i++)
	{
		NxDbgMsg(NX_DBG_INFO, "%2s Run Job with jobId = %d\n", "", jobId);
		Btc08RunJob(handle, BCAST_CHIP_ID, ASIC_BOOST_EN, jobId++);
	// fifo status
		jobcnt++;
	}

	while(1)
	{
		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_GN))	// Check GN GPIO pin
		{
			Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
			gn_irq 	  = res[2] & (1<<0);
			chipId    = res[3];

			if (0 != gn_irq) {		// If GN IRQ is set, then handle GN
				// Check if found GN(0x66cb3426) is correct and submit nonce to pool server and then go loop again
				tstimer_time(&ts_gn);
				NxDbgMsg(NX_DBG_INFO, "%5s === GN IRQ on chip#%d!!! === [%ld.%lds]\n", "", chipId, ts_gn.tv_sec, ts_gn.tv_nsec);

				handleGN(handle, chipId);
			} else {				// If GN IRQ is not set, then go to check OON
				NxDbgMsg(NX_DBG_INFO, "%5s === H/W GN occured but GN_IRQ value is not set!\n", "");
			}
		}

		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_OON))	// Check OON
		{
			// TODO: Need to check if it needs to read job id
			Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
			oon_irq	  = res[2] & (1<<1);
			chipId    = res[3];

			if (0 != oon_irq) {				// If OON IRQ is set, handle OON
				tstimer_time(&ts_oon);
				NxDbgMsg(NX_DBG_INFO, "%5s === OON IRQ on chip#%d!!! === [%ld.%lds]\n", "", chipId, ts_oon.tv_sec, ts_oon.tv_nsec);

				handleOON(handle);

				if (numWorks >= jobcnt)
				{
					NxDbgMsg(NX_DBG_INFO, "%2s Run Job with jobId = %d\n", "", jobId);
					Btc08RunJob(handle, BCAST_CHIP_ID, ASIC_BOOST_EN, jobId++);
					jobcnt++;
					if (jobId > MAX_JOB_ID)		// [7:0] job id ==> 8bits (max 256)
						jobId = 1;
				}
				else {
					break;
				}
			} else {						// OON IRQ is not set (cgminer: check OON timeout is expired)
				NxDbgMsg(NX_DBG_INFO, "%5s === OON IRQ is not set! ===\n", "");
				// if oon timeout is expired, disable chips
				// if oon timeout is not expired, check oon gpio again
			}
		}

		sched_yield();
	}

	tstimer_diff(&ts_oon, &ts_start, &ts_diff);
	NxDbgMsg(NX_DBG_INFO, "Total works = %d [%ld.%lds]\n", (jobcnt-1), ts_diff.tv_sec, ts_diff.tv_nsec);

	DestroyBtc08( handle );
}

void SimpleWorkLoop(void)
{
	static char cmdStr[NX_SHELL_MAX_ARG * NX_SHELL_MAX_STR];
	static char cmd[NX_SHELL_MAX_ARG][NX_SHELL_MAX_STR];
	int cmdCnt;
	for( ;; )
	{
		simplework_command_list();
		printf( "work > " );
		fgets( cmdStr, NX_SHELL_MAX_ARG*NX_SHELL_MAX_STR - 1, stdin );
		cmdCnt = Shell_GetArgument( cmdStr, cmd );

		//----------------------------------------------------------------------
		if( !strcasecmp(cmd[0], "q") )
		{
			break;
		}
		//----------------------------------------------------------------------
		// Single Work
		else if( !strcasecmp(cmd[0], "1") )
		{
			TestWork();
		}
		//----------------------------------------------------------------------
		else if( !strcasecmp(cmd[0], "2") )
		{
			int numWork = 10;
			if( cmdCnt > 1 )
			{
				numWork = strtol(cmd[1], 0, 10);
			}
			printf("numWork = %d\n", numWork);
			TestWorkLoop( numWork );
		}
		//----------------------------------------------------------------------
		else if( !strcasecmp(cmd[0], "3") )
		{
			TestWorkLoop_JobDist( 50000 );
		}
	}
}
