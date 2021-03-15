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
static void DistributionNonce( BTC08_HANDLE handle );
static int handleGN2(BTC08_HANDLE handle, uint8_t chipId, VECTOR_DATA *data);

static void simplework_command_list()
{
	printf("\n\n");
	printf("======= Simple Work =========\n");
	printf("  1. Single Work\n");
	printf("  2. Work Loop\n");
	printf("    ex > 2 [numOfJobs(>=4)]\n");
	printf("  3. Random Vector Loop\n");
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

/* 0x12345678 --> 0x56781234*/
static inline void swap16_(void *dest_p, const void *src_p)
{
	uint32_t *dest = dest_p;
	const uint32_t *src = src_p;

	*dest =     ((*src & 0xFF) << 16) |     ((*src & 0xFF00) << 16) |
			((*src & 0xFF0000) >> 16) | ((*src & 0xFF000000) >> 16);
}

static int handleGN(BTC08_HANDLE handle, uint8_t chipId, uint8_t *golden_nonce)
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
	uint32_t *default_gn = (uint32_t *)golden_nonce;
	char title[512];

	// Sequence 1. Read Hash
	Btc08ReadHash(handle, chipId, hash, hash_size);
	for (int i=0; i<4; i++)
	{
		ret = memcmp(default_golden_hash, &(hash[i*32]), 32);
		if (ret == 0)
			NxDbgMsg(NX_DBG_INFO, "%5s Hash matched of Inst_%s!!!\n", "",
					(i==0) ? "Lower_3":(((i==1) ? "Lower_2": ((i==2) ? "Lower":"Upper"))));
	}
#if DEBUG
//#if 1
	for (int i=0; i<4; i++) {
		sprintf(title, "Inst_%s", (i==0) ? "Lower_3":(((i==1) ? "Lower_2": ((i==2) ? "Lower":"Upper"))));
		HexDump(title, &(hash[i*32]), 32);
	}

	HexDump("golden nonce:", golden_nonce, 4);
#endif

	// Sequence 2. Read Result to read GN and clear GN IRQ
	Btc08ReadResult(handle, chipId, res, res_size);
#if DEBUG
	HexDump("read_result:", res, 18);
#endif
	validCnt = res[16];
	lower3   = ((res[17] & (1<<3)) != 0);
	lower2   = ((res[17] & (1<<2)) != 0);
	lower    = ((res[17] & (1<<1)) != 0);
	upper    = ((res[17] & (1<<0)) != 0);

	NxDbgMsg(NX_DBG_INFO, "%5s [%s %s %s %s found golden nonce ]\n", "",
			lower3 ? "Inst_Lower_3,":"", lower2 ? "Inst_Lower_2,":"",
			lower  ?   "Inst_Lower,":"", upper  ?   "Inst_Upper":"");

	res_32 = (uint32_t *)&(res[0]);
	for (int i=0; i<4; i++)
	{
		if (0 != res_32[i])
		{
			nonce32[i] = res_32[i];
			//swap16_(&nonce32[i], &res_32[i]);
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

static int handleOON(BTC08_HANDLE handle, uint8_t chipId)
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
			Btc08ClearOON(handle, chipId);
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
	uint8_t fifo_full = 0x00, oon_irq = 0x00, gn_irq = 0x00, oon_job_id = 0x00;
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

	handle->isAsicBoost = true;

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
	Btc08RunJob(handle, BCAST_CHIP_ID, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), jobId++);
	Btc08RunJob(handle, BCAST_CHIP_ID, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), jobId++);
	Btc08RunJob(handle, BCAST_CHIP_ID, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), jobId++);
	Btc08RunJob(handle, BCAST_CHIP_ID, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), jobId++);
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
				NxDbgMsg(NX_DBG_INFO, "%2s === GN IRQ on chip#%d for jobId#%d!!! === [%ld.%lds]\n",
						"", chipId, ts_gn.tv_sec, ts_gn.tv_nsec);

				handleGN(handle, chipId, golden_nonce);
			} else {						// In case of not GN
				NxDbgMsg(NX_DBG_INFO, "%2s === H/W interrupt occured, but GN_IRQ value is not set!\n", "");
			}
		}

		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_OON))	// Check OON
		{
			// TODO: Need to check if it needs to read job id
			Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
			oon_job_id = res[0];
			oon_irq	  = res[2] & (1<<1);
			chipId    = res[3];

			if (0 != oon_irq) {		// In case of OON
				tstimer_time(&ts_oon);
				NxDbgMsg(NX_DBG_INFO, "%2s === OON IRQ on chip#%d for jobId#d!!! === [%ld.%lds]\n",
						"", chipId, oon_job_id, ts_oon.tv_sec, ts_oon.tv_nsec);

				handleOON(handle, BCAST_CHIP_ID);
				break;
			} else {				// In case of not OON
				NxDbgMsg(NX_DBG_INFO, "%2s === OON IRQ is not set!\n", "");
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
	#define DIST_NONCE_RANGE 		1
	#define DETAIL_CALC_HASHRATE	1

	uint8_t chipId = 0x00, jobId = 0x01;
	uint8_t fifo_full = 0x00, oon_irq = 0x00, gn_irq = 0x00, oon_job_id = 0x00, gn_job_id = 0x00;
	uint8_t res[4] = {0x00,};
	uint8_t oon_jobid, gn_jobid;
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	struct timespec ts_start, ts_oon, ts_gn, ts_diff, ts_last_oon;
	uint64_t jobcnt = 0;
	uint64_t hashes_done;
	bool	ishashdone = false;
	uint8_t hashdone_allchip[256];
	uint8_t hashdone_chip[256];
	uint64_t hashrate;
	VECTOR_DATA data;

	memset(hashdone_allchip, 0xFF, sizeof(hashdone_allchip));
	memset(hashdone_chip,    0x00, sizeof(hashdone_chip));

	tstimer_time(&ts_start);
	NxDbgMsg(NX_DBG_INFO, "=== Start workloop === [%ld.%lds]\n", ts_start.tv_sec, ts_start.tv_nsec);

	// Seqeunce 1. Create Handle
	BTC08_HANDLE handle = CreateBtc08(0);

	Btc08ResetHW(handle, 1);
	Btc08ResetHW(handle, 0);

	handle->isAsicBoost = true;

	// Seqeunce 2. Find number of chips : using AutoAddress
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "(Before last chip) Number of Chips = %d\n", handle->numChips);
	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
		NxDbgMsg( NX_DBG_DEBUG, "ChipId = %d, Number of jobs = %d\n",
					chipId, (res[2]&7) );
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
		NxDbgMsg( NX_DBG_DEBUG, "ChipId = %d, Number of jobs = %d\n",
					chipId, (res[2]&7) );
	}

	// Sequence 5. Enable all cores
	Btc08SetDisable (handle, BCAST_CHIP_ID, golden_enable);

	// Seqeunce 6. Find number of cores of individual chips
	RunBist( handle );

	// Sequence 7. Enable OON IRQ/Set UART divider
	Btc08SetControl(handle, BCAST_CHIP_ID, (OON_IRQ_EN | UART_DIVIDER));

	// Sequence 8. Setting parameters, target, nonce range
	GetGoldenVector(4, &data, 0);
	Btc08WriteParam(handle, BCAST_CHIP_ID, data.midState, data.parameter);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, data.target);
#if DIST_NONCE_RANGE
	DistributionNonce(handle);		//	Distribution Nonce
	for( int i=0; i<handle->numChips ; i++ )
	{
		NxDbgMsg( NX_DBG_INFO, "Chip[%d:%d] : %02x%02x%02x%02x ~ %02x%02x%02x%02x\n", i, handle->numCores[i], 
			handle->startNonce[i][0], handle->startNonce[i][1], handle->startNonce[i][2], handle->startNonce[i][3],
			handle->endNonce[i][0], handle->endNonce[i][1], handle->endNonce[i][2], handle->endNonce[i][3] );
		Btc08WriteNonce(handle, i+1, handle->startNonce[i], handle->endNonce[i]);
	}
#else
	//uint8_t start_nonce[4] = { 0x60, 0x00, 0x00, 0x00 };
	//uint8_t end_nonce[4]   = { 0x6f, 0xff, 0xff, 0xff };
	uint8_t start_nonce[4] = { 0x00, 0x00, 0x00, 0x00 };
	uint8_t end_nonce[4]   = { 0xff, 0xff, 0xff, 0xff };

	Btc08WriteNonce(handle, BCAST_CHIP_ID, start_nonce, end_nonce);
#endif

	tstimer_time(&ts_start);
	NxDbgMsg( NX_DBG_INFO, "=== RUN JOB === [%ld.%lds]\n", ts_start.tv_sec, ts_start.tv_nsec);

	// Sequence 9. Run job
	for (int i = 0; i < MAX_JOB_FIFO_NUM; i++)
	{
		NxDbgMsg(NX_DBG_INFO, "%2s Run Job with jobId#%d\n", "", jobId);
		Btc08RunJob(handle, BCAST_CHIP_ID, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), jobId++);
		jobcnt++;
	}

	while(!ishashdone)
	{
		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_GN))	// Check GN GPIO pin
		{
			for (int i=1; i<=handle->numChips; i++)
			{
				Btc08ReadJobId(handle, i, res, res_size);
				gn_job_id  = res[1];
				gn_irq 	   = res[2] & (1<<0);
				chipId     = res[3];

				if (0 != gn_irq) {		// If GN IRQ is set, then handle GN
					tstimer_time(&ts_gn);
					NxDbgMsg(NX_DBG_INFO, "%5s === GN IRQ on chip#%d for jobId#%d!!! === [%ld.%lds]\n",
								"", chipId, gn_job_id, ts_gn.tv_sec, ts_gn.tv_nsec);
					handleGN(handle, i, data.nonce);
				} else {				// If GN IRQ is not set, then go to check OON
					//NxDbgMsg(NX_DBG_INFO, "%5s === H/W GN occured but GN_IRQ value is not set!!!\n", "");
				}
			}
		}

		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_OON))	// Check OON
		{
#ifdef DETAIL_CALC_HASHRATE
			for (int i=1; i<=handle->numChips; i++)
			{
				Btc08ReadJobId(handle, i, res, res_size);
#else
				Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
#endif
				oon_job_id = res[0];
				oon_irq	   = res[2] & (1<<1);
				chipId     = res[3];

				if (0 != oon_irq) {				// If OON IRQ is set, handle OON
					tstimer_time(&ts_oon);
					NxDbgMsg(NX_DBG_INFO, "%5s === OON IRQ on chip#%d for jobId#%d!!! === [%ld.%lds]\n",
								"", chipId, oon_job_id, ts_oon.tv_sec, ts_oon.tv_nsec);

#ifdef DETAIL_CALC_HASHRATE
					if (oon_job_id == numWorks)
						hashdone_chip[chipId-1] = 0xFF;
					int ret = handleOON(handle, i);
#else
					int ret = handleOON(handle, BCAST_CHIP_ID);
#endif
					if (ret == 0 && numWorks > jobcnt)
					{
						NxDbgMsg(NX_DBG_INFO, "%2s Run Job with jobId#%d\n", "", jobId);
						Btc08RunJob(handle, BCAST_CHIP_ID, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), jobId++);
						jobcnt++;
						if (jobId > MAX_JOB_ID)
							jobId = 1;
					}
					else {
#ifdef DETAIL_CALC_HASHRATE
						if (oon_job_id == numWorks)
						{
#endif
							int result = memcmp(hashdone_chip, hashdone_allchip, sizeof(uint8_t) * handle->numChips);
							if (result == 0)
							{
								tstimer_time(&ts_last_oon);
								tstimer_diff(&ts_last_oon, &ts_start, &ts_diff);
								ishashdone = true;
								break;
							}
#ifdef DETAIL_CALC_HASHRATE
						}
#endif
					}
				} else {						// OON IRQ is not set (cgminer: check OON timeout is expired)
					//NxDbgMsg(NX_DBG_INFO, "%5s === OON IRQ is not set! ===\n", "");
					// if oon timeout is expired, disable chips
					// if oon timeout is not expired, check oon gpio again
				}
			}
		}
		sched_yield();
	}

	// The expected hashrate = 600 mhash/sec [MinerCoreClk(50MHz) * NumOfCores(3cores) * AsicBoost(4sub-cores)]
	calc_hashrate(handle->isAsicBoost, jobcnt, &ts_diff);

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
	uint32_t startNonce, endNonce;

	for( int i=0 ; i<handle->numChips ; i++ )
	{
		totalCores += handle->numCores[i];
	}
	NxDbgMsg( NX_DBG_INFO, "Total Cores = %d\n", totalCores );

	noncePerCore = 0xffffffff / totalCores;
	startNonce = 0;
	for( ii=0 ; ii<handle->numChips-1 ; ii++ ) {
		endNonce = startNonce + (noncePerCore*handle->numCores[ii]);
		{
			handle->startNonce[ii][0] = (startNonce>>24) & 0xff;
			handle->startNonce[ii][1] = (startNonce>>16) & 0xff;
			handle->startNonce[ii][2] = (startNonce>>8 ) & 0xff;
			handle->startNonce[ii][3] = (startNonce>>0 ) & 0xff;
			handle->endNonce[ii][0]   = (endNonce>>24) & 0xff;
			handle->endNonce[ii][1]   = (endNonce>>16) & 0xff;
			handle->endNonce[ii][2]   = (endNonce>>8 ) & 0xff;
			handle->endNonce[ii][3]   = (endNonce>>0 ) & 0xff;
		}
		startNonce = endNonce + 1;
	}
	handle->startNonce[ii][0] = (startNonce>>24) & 0xff;
	handle->startNonce[ii][1] = (startNonce>>16) & 0xff;
	handle->startNonce[ii][2] = (startNonce>>8 ) & 0xff;
	handle->startNonce[ii][3] = (startNonce>>0 ) & 0xff;
	handle->endNonce[ii][0] = 0xff;
	handle->endNonce[ii][1] = 0xff;
	handle->endNonce[ii][2] = 0xff;
	handle->endNonce[ii][3] = 0xff;
}

static void FindNextTimeStamp( uint8_t *param )
{
	uint32_t epochTime;

	epochTime = (param[4]<<24) | 
				(param[5]<<16) |
				(param[6]<< 8) |
				(param[7]<< 0);

	epochTime += 1;

	param[4] = (epochTime >> 24) & 0xff;
	param[5] = (epochTime >> 16) & 0xff;
	param[6] = (epochTime >>  8) & 0xff;
	param[7] = (epochTime >>  0) & 0xff;
}

static int handleGN2(BTC08_HANDLE handle, uint8_t chipId, VECTOR_DATA *data)
{
	int ret = 0;
	uint8_t hash[128] = {0x00,};
	uint8_t res[18] = {0x00,};
	bool bHash = false;
	bool bNonce = false;

	// Sequence 1. Read Hash
	Btc08ReadHash(handle, chipId, hash, sizeof(hash));
	for (int i=0 ; i<4 ; i++)
	{
		ret = memcmp(data->hash, &(hash[i*32]), 32);
		if (ret == 0)
		{
			NxDbgMsg(NX_DBG_INFO, "%5s [%d] : Hash matched of Inst_%s!!!\n", "", i,
					(i==0) ? "Lower_3":(((i==1) ? "Lower_2": ((i==2) ? "Lower":"Upper"))));
			bHash = true;
		}
	}

	// Sequence 2. Read Result to read GN and clear GN IRQ
	Btc08ReadResult(handle, chipId, res, sizeof(res));
	for (int i=0 ; i<4 ; i++)
	{
		ret = memcmp( data->nonce, res + i*4, 4 );
		if( ret == 0 )
		{
			NxDbgMsg( NX_DBG_INFO, "%5s [%d] : nonce = %02x %02x %02x %02x\n", "", i,
				*(res + i*4), *(res + i*4 + 1), *(res + i*4 + 2), *(res + i*4 + 3) );
			bNonce = true;
		}
	}

	return (bHash&&bNonce) ? 0 : -1;
}


static void TestWorkLoop_RandomVector()
{
	int ii;
	uint8_t chipId = 0x00, jobId = 0x01;
	uint8_t fifo_full = 0x00, oon_irq = 0x00, gn_irq = 0x00;
	uint8_t res[4] = {0x00,};
	uint8_t oon_jobid, gn_jobid;
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	uint8_t start_nonce[4];
	uint8_t end_nonce[4];
	uint64_t startTime, currTime, deltaTime, prevTime;
	double totalTime;
	double megaHash;

	uint64_t totalProcessedHash = 0;
	bool bGN = false;
	VECTOR_DATA data;
	uint32_t nonce = 0;
	int index;

	srand( (uint32_t)get_current_ms() );

	NxDbgMsg(NX_DBG_INFO, "=== Start workloop ===\n");

	// Seqeunce 1. Create Handle
	BTC08_HANDLE handle = CreateBtc08(0);

	Btc08ResetHW(handle, 1);
	Btc08ResetHW(handle, 0);

	handle->isAsicBoost = true;

	// Seqeunce 2. Find number of chips : using AutoAddress
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "(Before last chip) Number of Chips = %d\n", handle->numChips);
	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
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
	}

	// Sequence 5. Enable all cores
	Btc08SetDisable (handle, BCAST_CHIP_ID, golden_enable);

	// Seqeunce 6. Find number of cores of individual chips
	RunBist( handle );
	DistributionNonce(handle);		//	Distribution Nonce

	// Sequence 7. Enable OON IRQ/Set UART divider
	Btc08SetControl(handle, BCAST_CHIP_ID, (OON_IRQ_EN | UART_DIVIDER));

	index = rand() % MAX_NUM_VECTOR;
	GetGoldenVector(index, &data, 1);

	// Sequence 8. Setting parameters, target, nonce range
	Btc08WriteTarget(handle, BCAST_CHIP_ID, data.target);

	for( int i=0; i<handle->numChips ; i++ )
	{
		NxDbgMsg( NX_DBG_INFO, "Chip[%d:%d] : %02x%02x%02x%02x ~ %02x%02x%02x%02x\n", i, handle->numCores[i], 
			handle->startNonce[i][0], handle->startNonce[i][1], handle->startNonce[i][2], handle->startNonce[i][3],
			handle->endNonce[i][0], handle->endNonce[i][1], handle->endNonce[i][2], handle->endNonce[i][3] );
		Btc08WriteNonce(handle, i+1, handle->startNonce[i], handle->endNonce[i]);
	}

	NxDbgMsg( NX_DBG_INFO, "=== RUN JOB ===\n");

	startTime = get_current_ms();
	prevTime = startTime;

	// Sequence 9. Run job
	for (int i = 0; i < MAX_JOB_FIFO_NUM; i++)
	{
		NxDbgMsg(NX_DBG_INFO, "%2s Run Job with jobId = %d\n", "", jobId);
		Btc08WriteParam(handle, BCAST_CHIP_ID, data.midState, data.parameter);
		FindNextTimeStamp( data.parameter );
		Btc08RunJob(handle, BCAST_CHIP_ID, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), jobId++);
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
				NxDbgMsg(NX_DBG_INFO, "%5s === GN IRQ on chip#%d!!! ===\n", "", chipId);

				if( 0 != handleGN2(handle, chipId, &data) )
				{
					NxDbgMsg(NX_DBG_ERR, "%5s  === Miss matching hash or nonce ===\n", "");
				}
				nonce = ( data.nonce[0] << 24 ) |
						( data.nonce[1] << 16 ) |
						( data.nonce[2] <<  8 ) |
						( data.nonce[3] <<  0 );
				bGN = true;
			} else {				// If GN IRQ is not set, then go to check OON
				NxDbgMsg(NX_DBG_DEBUG, "%5s === H/W GN occured but GN_IRQ value is not set!\n", "");
			}
		}

		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_OON))	// Check OON
		{
			// TODO: Need to check if it needs to read job id
			Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
			oon_irq	  = res[2] & (1<<1);
			chipId    = res[3];

			if (0 != oon_irq)				// If OON IRQ is set, handle OON
			{
				NxDbgMsg(NX_DBG_INFO, "%5s === OON IRQ on chip#%d!!! ===\n", "", chipId);

				handleOON(handle, BCAST_CHIP_ID);

				if( bGN )
				{
					bGN = false;
					//	Get New Vector
					index = rand() % MAX_NUM_VECTOR;
					GetGoldenVector(index, &data, 1);
					Btc08WriteTarget(handle, BCAST_CHIP_ID, data.target);
				}
				totalProcessedHash += 0x400000000;	//	0x100000000 * 4 (asic booster)

				currTime = get_current_ms();
				totalTime = currTime - startTime;

				megaHash = totalProcessedHash / (1000*1000);
				NxDbgMsg(NX_DBG_INFO, "AVG : %.2f MHash/s,  Hash = %.2f GH, Time = %.2f sec, delta = %lld msec\n",
						megaHash * 1000. / totalTime, megaHash/1000, totalTime/1000. , currTime - prevTime );

				prevTime = currTime;

				NxDbgMsg(NX_DBG_INFO, "%2s Run Job with jobId = %d\n", "", jobId);
				Btc08WriteParam(handle, BCAST_CHIP_ID, data.midState, data.parameter);
				Btc08RunJob(handle, BCAST_CHIP_ID, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), jobId++);
				FindNextTimeStamp( data.parameter );
				if (jobId >= MAX_JOB_ID)		// [7:0] job id ==> 8bits (max 256)
					jobId = 1;
			} 
			else		// OON IRQ is not set (cgminer: check OON timeout is expired)
			{
				NxDbgMsg(NX_DBG_INFO, "%5s === OON IRQ is not set! ===\n", "");
				// if oon timeout is expired, disable chips
				// if oon timeout is not expired, check oon gpio again
			}
		}

		sched_yield();
	}

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
			int numWork = 4;
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
			TestWorkLoop_RandomVector();
		}
	}
}
