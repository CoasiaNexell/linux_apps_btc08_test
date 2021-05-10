#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <byteswap.h>
#include <sched.h>
#include <stdbool.h>

#include "Utils.h"
#include "Btc08.h"

#ifdef NX_DTAG
#undef NX_DTAG
#endif
#define NX_DTAG "[SimpleWork]"
#include "NX_DbgMsg.h"

#define DEBUG	0
#define USE_VECTOR_DATA         1
#define DIST_NONCE_RANGE        1
#define CHECK_LAST_OON          1
#define DETAIL_CALC_HASHRATE    0

static int numCores[MAX_CHIP_NUM] = {0,};
static void DistributionNonce( BTC08_HANDLE handle );
static int handleGN2(BTC08_HANDLE handle, uint8_t chipId, VECTOR_DATA *data);

static void simplework_command_list()
{
	printf("\n\n");
	printf("======= Simple Work =========\n");
	printf("  1. Single Work\n");
	printf("    ex > 1 [last_chipId(1~numChips)]\n");
	printf("  2. Work Loop\n");
	printf("    ex > 2 [numOfJobs(>=4)] [last_chipId(1~numChips)]\n");
	printf("  3. Random Vector Loop\n");
	printf("  4. Work Infinite Loop\n");
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

	NxDbgMsg( NX_DBG_INFO, "=== READ BIST ==\n");
	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		// If it's not BUSY status, read the number of cores in next READ_BIST
		for (int i=0; i<10; i++) {
			ret = Btc08ReadBist(handle, chipId);
			if ( (ret[0] & 1) == 0 )
				break;
			else
				NxDbgMsg( NX_DBG_DEBUG, "%5s ChipId = %d, Status = %s, Number of cores = %d\n", "",
						chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );

			usleep( 300 );
		}
		ret = Btc08ReadBist(handle, chipId);
		handle->numCores[chipId-1] = ret[1];
		NxDbgMsg( NX_DBG_DEBUG, "%5s ChipId = %d, Status = %s, Number of cores = %d\n", "",
					chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );
	}
	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
		NxDbgMsg( NX_DBG_INFO, "%5s ChipId = %d, Number of cores = %d, Number of jobs = %d\n", "",
					chipId, handle->numCores[chipId-1], (res[2]&7) );
	}
}

// PMS value setting sequnce
static void SetPll(BTC08_HANDLE handle, int chipId, int pll_idx)
{
	uint8_t *ret;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	int lock_status;

	NxDbgMsg( NX_DBG_INFO, "=== SET PLL ==\n");

	// seq1. Disable FOUT
	Btc08SetPllFoutEn(handle, chipId, FOUT_EN_DISABLE);

	// seq2. Down reset
	Btc08SetPllResetB(handle, chipId, RESETB_RESET);

	// seq3. Set PLL(change PMS value)
	Btc08SetPllConfig(handle, pll_idx);

	// seq4. wait for 1 ms
	usleep( 1000 );

	// seq5. Enable FOUT
	Btc08SetPllFoutEn(handle, chipId, FOUT_EN_ENABLE);

	// seq6. Check PLL lock
	lock_status = Btc08ReadPll(handle, chipId, res, res_size);
	NxDbgMsg(NX_DBG_ERR, "%5s [chip#%d] pll lock status: %s\n", "", chipId, (lock_status == 1) ? "locked":"unlocked");
}

static int handleGN(BTC08_HANDLE handle, uint8_t chipId, uint8_t *golden_nonce)
{
	int ret = 0, result = 0;
	uint8_t hash[128] = {0x00,};
	unsigned int hash_size = sizeof(hash)/sizeof(hash[0]);
	uint8_t res[18] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	bool lower3, lower2, lower, upper, validCnt;
	uint32_t cal_gn;
	char title[512];

	// Sequence 1. Read Hash
	Btc08ReadHash(handle, chipId, hash, hash_size);
	for (int i=0; i<4; i++)
	{
		result = memcmp(default_golden_hash, &(hash[i*32]), 32);
		if (result == 0)
			NxDbgMsg(NX_DBG_INFO, "%5s Result Hash of Inst_%s!!!\n", "",
					(i==0) ? "Upper":(((i==1) ? "Lower": ((i==2) ? "Lower_2":"Lower_3"))));
		else
		{
			NxDbgMsg(NX_DBG_ERR, "%5s Failed: Result Hash of Inst_%s!!!\n", "",
					(i==0) ? "Upper":(((i==1) ? "Lower": ((i==2) ? "Lower_2":"Lower_3"))));
			ret = -1;
		}
	}
#if DEBUG
	for (int i=0; i<4; i++) {
		sprintf(title, "Inst_%s", (i==0) ? "Upper":(((i==1) ? "Lower": ((i==2) ? "Lower_2":"Lower_3"))));
		HexDump(title, &(hash[i*32]), 32);
	}
#endif

	// Sequence 2. Read Result to read GN and clear GN IRQ
	Btc08ReadResult(handle, chipId, res, res_size);
#if DEBUG
	HexDump("golden nonce:", golden_nonce, 4);
	HexDump("read_result:", res, 18);
	validCnt = res[16];
	lower3   = ((res[17] & (1<<3)) != 0);
	lower2   = ((res[17] & (1<<2)) != 0);
	lower    = ((res[17] & (1<<1)) != 0);
	upper    = ((res[17] & (1<<0)) != 0);

	NxDbgMsg(NX_DBG_INFO, "%5s [%s %s %s %s found golden nonce ]\n", "",
			lower3 ? "Inst_Lower_3,":"", lower2 ? "Inst_Lower_2,":"",
			lower  ?   "Inst_Lower,":"", upper  ?   "Inst_Upper":"");
#endif

	for (int i=0; i<4; i++)
	{
		result = memcmp(golden_nonce, res + i*4, 4);
		if (result == 0)
			NxDbgMsg(NX_DBG_INFO, "%5s [Inst_%s] %10s GN = %02x %02x %02x %02x \n", "",
				(i==0) ? "Upper":(((i==1) ? "Lower": ((i==2) ? "Lower_2":"Lower_3"))), "",
				*(res + i*4), *(res + i*4 + 1), *(res + i*4 + 2), *(res + i*4 + 3));
		else
		{
			NxDbgMsg(NX_DBG_ERR, "%5s Failed: [Inst_%s] %10s GN = %02x %02x %02x %02x \n", "",
				(i==0) ? "Upper":(((i==1) ? "Lower": ((i==2) ? "Lower_2":"Lower_3"))), "",
				*(res + i*4), *(res + i*4 + 1), *(res + i*4 + 2), *(res + i*4 + 3));
			ret = -1;
		}
	}
	return ret;
}

static int handleGN3(BTC08_HANDLE handle, uint8_t chipId, uint8_t *found_nonce, uint8_t *micro_job_id, VECTOR_DATA *data)
{
	int ret = 0, result = 0;
	uint8_t hash[128] = {0x00,};
	unsigned int hash_size = sizeof(hash)/sizeof(hash[0]);
	uint8_t zero_hash[128] = {0x00,};
	uint8_t res[18] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	char buf[512];

	memset(zero_hash, 0, sizeof(zero_hash));

	// Sequence 1. Read Hash
	ret = Btc08ReadHash(handle, chipId, hash, hash_size);
	if (ret == 0) {
		for (int i=0; i<4; i++)
		{
			result = memcmp(zero_hash, &(hash[i*32]), 32);
			if (result != 0)
			{
				sprintf(buf, "Hash of Inst_%s",
						(i==0) ? "Upper":(((i==1) ? "Lower": ((i==2) ? "Lower_2":"Lower_3"))));
				HexDump(buf, &(hash[i*32]), 32);
			}
		}
	} else
		return -1;

	// Sequence 2. Read Result to read GN and clear GN IRQ
	ret = Btc08ReadResult(handle, chipId, res, res_size);
	if (ret == 0)
	{
		*micro_job_id = res[17];
		for (int i=0; i<4; i++)
		{
			memcpy(&(found_nonce[i*4]), &(res[i*4]), 4);
			if((*micro_job_id & (1<<i)) != 0) {
				sprintf(buf, "Nonce of Inst_%s",
						(i==0) ? "Upper":(((i==1) ? "Lower": ((i==2) ? "Lower_2":"Lower_3"))));
				HexDump(buf, &(res[i*4]), 4);
			}
		}
	} else
		return -1;

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
 * Process one work(nonce range: 4G) with asicboost
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
 * Test result
 *  3 chips, core 2/1/2 : HashRate = 1010.6 mhash/sec (Works = 1, Hashes = 17179 mhash, Total Time = 17s)
 *  3 chips, core 1/1/1 : HashRate = 592.4 mhash/sec  (Works = 1, Hashes = 17179 mhash, Total Time = 29s)
 */
static void TestWork(uint8_t last_chipId)
{
	int chipId = 0x00, jobId=0x01;
	struct timespec ts_start, ts_oon, ts_gn, ts_diff;
	uint8_t fifo_full = 0x00, oon_irq = 0x00, gn_irq = 0x00, oon_job_id = 0x00, gn_job_id =0x00;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	bool	ishashdone = false;
	uint8_t hashdone_allchip[256];
	uint8_t hashdone_chip[256];
	VECTOR_DATA data;

	memset(hashdone_allchip, 0xFF, sizeof(hashdone_allchip));
	memset(hashdone_chip,    0x00, sizeof(hashdone_chip));

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
	Btc08SetControl(handle, last_chipId, LAST_CHIP);
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

	// For Hashboard
	//for (int chipId = 1; chipId <= handle->numChips; chipId++)
	//	SetPll(handle, chipId, 4);

	// Seqeunce 6. Run BIST
	RunBist( handle );

	handle->isAsicBoost = true;

	// Sequence 7. Enable OON IRQ/Set UART divider (tb:32'h0000_8013, sh:0018_001f)
	Btc08SetControl(handle, BCAST_CHIP_ID, (OON_IRQ_EN | UART_DIVIDER));

#if USE_VECTOR_DATA
	GetGoldenVector(4, &data, 0);

	// Seqeunce 8. Setting Parameters
	Btc08WriteParam(handle, BCAST_CHIP_ID, data.midState, data.parameter);

	// Seqeunce 9. Setting Target Parameters
	Btc08WriteTarget(handle, BCAST_CHIP_ID, data.target);
#else
	// Seqeunce 8. Setting Parameters
	Btc08WriteParam(handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);

	// Seqeunce 9. Setting Target Parameters
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
#endif

#if DIST_NONCE_RANGE
	// Sequence 10. Setting Nonce Range
	DistributionNonce(handle);		//	Distribution Nonce
	for( int i=0; i<handle->numChips ; i++ )
	{
		NxDbgMsg( NX_DBG_INFO, "Chip[%d:%d] : %02x%02x%02x%02x ~ %02x%02x%02x%02x\n", i, handle->numCores[i],
			handle->startNonce[i][0], handle->startNonce[i][1], handle->startNonce[i][2], handle->startNonce[i][3],
			handle->endNonce[i][0], handle->endNonce[i][1], handle->endNonce[i][2], handle->endNonce[i][3] );

		Btc08WriteNonce(handle, i+1, handle->startNonce[i], handle->endNonce[i]);
	}
#else
	// Sequence 10. Setting Nonce Range (To use 1 sec for 1 work, set nonce range '0x07ff_ffff')
	//uint8_t start_nonce[4] = { 0x5e, 0xcb, 0x34, 0x27 };
	//uint8_t end_nonce[4]   = { 0x66, 0xcb, 0x34, 0x26 };
	uint8_t start_nonce[4] = { 0x00, 0x00, 0x00, 0x00 };
	uint8_t end_nonce[4]   = { 0xff, 0xff, 0xff, 0xff };
	Btc08WriteNonce(handle, BCAST_CHIP_ID, start_nonce, end_nonce);
#endif

	NxDbgMsg( NX_DBG_INFO, "=== RUN JOB ==\n");

	// Sequence 11. Run Job
	Btc08RunJob(handle, BCAST_CHIP_ID, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), jobId++);

	// Sequence 12. Check Interrupt Signals(GPIO) and Post Processing
	//	FIXME : This loop is basically busy wait type,
	//	If possible, is should be changed using a interrupt.
	while(!ishashdone)
	{
		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_GN))	// Check GN GPIO pin
		{
			Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
			gn_job_id  = res[1];
			gn_irq 	  = res[2] & (1<<0);
			chipId    = res[3];

			if (0 != gn_irq) {				// In case of GN
				// Check if found GN(0x66cb3426) is correct and submit nonce to pool server and then go loop again
				tstimer_time(&ts_gn);
				NxDbgMsg(NX_DBG_INFO, "%2s === GN IRQ on chip#%d for jobId#%d!!! === [%ld.%lds]\n",
						"", chipId, gn_job_id, ts_gn.tv_sec, ts_gn.tv_nsec);

				if (handleGN(handle, chipId, golden_nonce) < 0)
				{
					NxDbgMsg( NX_DBG_ERR, "=== GN Read fail!!! ==\n");
					break;
				}
			} else {						// In case of not GN
				NxDbgMsg(NX_DBG_INFO, "%2s === H/W interrupt occured, but GN_IRQ value is not set!\n", "");
			}
		}

		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_OON))	// Check OON
		{
#if CHECK_LAST_OON
			Btc08ReadJobId(handle,  handle->numChips, res, res_size);
			oon_job_id = res[0];
			oon_irq	  = res[2] & (1<<1);
			chipId    = res[3];

			if (0 != oon_irq) {		// In case of OON
				if (chipId == handle->numChips)		// OON occures on chip#1 > chip#2 > chip#3
				{
					tstimer_time(&ts_oon);
					NxDbgMsg(NX_DBG_INFO, "%2s === OON IRQ on chip#%d for oon_jobId#%d!!! === [%ld.%lds]\n",
							"", chipId, oon_job_id, ts_oon.tv_sec, ts_oon.tv_nsec);

					int ret = handleOON(handle, BCAST_CHIP_ID);
					ishashdone = true;
				}
			}
#else
			for (int i=1; i<=handle->numChips; i++)
			{
				Btc08ReadJobId(handle, i, res, res_size);
				oon_job_id = res[0];
				oon_irq	  = res[2] & (1<<1);
				chipId    = res[3];

				if (0 != oon_irq) {		// In case of OON
					tstimer_time(&ts_oon);
					NxDbgMsg(NX_DBG_INFO, "%2s === OON IRQ on chip#%d for oon_jobId#%d!!! === [%ld.%lds]\n",
							"", chipId, oon_job_id, ts_oon.tv_sec, ts_oon.tv_nsec);

					if (oon_job_id == (jobId-1))
						hashdone_chip[chipId-1] = 0xFF;

					int ret = handleOON(handle, i);
					if ((ret == 0) && (oon_job_id == (jobId-1)))
					{
						int result = memcmp(hashdone_chip, hashdone_allchip, sizeof(uint8_t) * handle->numChips);
						if (result == 0)
						{
							ishashdone = true;
							break;
						}
					}
				} else {				// In case of not OON
					//NxDbgMsg(NX_DBG_INFO, "%2s === OON IRQ is not set!\n", "");
					// if oon timeTestWorkLoop_JobDist
				}
			}
#endif
		}
		sched_yield();
	}

	tstimer_diff(&ts_oon, &ts_start, &ts_diff);

	NxDbgMsg(NX_DBG_INFO, "=== End of TestWork === [%ld.%lds]\n", ts_diff.tv_sec, ts_diff.tv_nsec);

	// The expected hashrate = 600 mhash/sec [MinerCoreClk(50MHz) * NumOfCores(3cores) * AsicBoost(4sub-cores)]
	calc_hashrate(handle->isAsicBoost, (jobId-1), &ts_diff);

	DestroyBtc08( handle );
}

/* Process the same works with asicboost. version mask is not supported.
 * If OON IRQ occurs, check READ_JOB_ID, clear OON and then pass the additional work to job fifo.
 * If GN IRQ occurs, read GN and then clear GN IRQ.
 * Test Results
 * 3 chips, core 2/1/2: HashRate = 1010.6 mhash/sec (Works = 4, Hashes = 68719 mhash, Total Time = 68s)
 * 3 chips, core 1/1/1: HashRate = 602.8 mhash/sec  (Works = 4, Hashes = 68719 mhash, Total Time = 114s)
 */
static void TestWorkLoop(int numWorks, uint8_t last_chipId)
{
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
	Btc08SetControl(handle, last_chipId, LAST_CHIP);
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
	DistributionNonce(handle);
	for( int i=0; i<handle->numChips ; i++ )
	{
		NxDbgMsg( NX_DBG_INFO, "Chip[%d:%d] : %02x%02x%02x%02x ~ %02x%02x%02x%02x\n", i, handle->numCores[i],
			handle->startNonce[i][0], handle->startNonce[i][1], handle->startNonce[i][2], handle->startNonce[i][3],
			handle->endNonce[i][0], handle->endNonce[i][1], handle->endNonce[i][2], handle->endNonce[i][3] );
		Btc08WriteNonce(handle, i+1, handle->startNonce[i], handle->endNonce[i]);
	}

	tstimer_time(&ts_start);
	NxDbgMsg( NX_DBG_INFO, "=== RUN JOB === [%ld.%lds]\n", ts_start.tv_sec, ts_start.tv_nsec);

	// Sequence 9. Run job
#if CHECK_LAST_OON
#else
	for (int i = 0; i < MAX_JOB_FIFO_NUM; i++)
#endif
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
					if (handleGN(handle, chipId, golden_nonce) < 0)
					{
						NxDbgMsg( NX_DBG_ERR, "=== GN Read fail!!! ==\n");
						ishashdone = true;
						break;
					}
				} else {				// If GN IRQ is not set, then go to check OON
					//NxDbgMsg(NX_DBG_INFO, "%5s === H/W GN occured but GN_IRQ value is not set!!!\n", "");
				}
			}
		}

		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_OON))	// Check OON
#if CHECK_LAST_OON
		{
			Btc08ReadJobId(handle,  handle->numChips, res, res_size);
			oon_job_id = res[0];
			oon_irq	  = res[2] & (1<<1);
			chipId    = res[3];

			if (0 != oon_irq) {		// In case of OON
				if (chipId == handle->numChips)		// OON occures on chip#1 > chip#2 > chip#3
				{
					tstimer_time(&ts_oon);
					NxDbgMsg(NX_DBG_INFO, "%2s === OON IRQ on chip#%d for oon_jobId#%d!!! === [%ld.%lds]\n",
							"", chipId, oon_job_id, ts_oon.tv_sec, ts_oon.tv_nsec);

					int ret = handleOON(handle, BCAST_CHIP_ID);
					if (ret == 0 && numWorks > jobcnt)
					{
						NxDbgMsg(NX_DBG_INFO, "%2s Run Job with jobId#%d\n", "", jobId);
						Btc08RunJob(handle, BCAST_CHIP_ID, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), jobId++);
						jobcnt++;
						if (jobId > MAX_JOB_ID)
							jobId = 1;
					}
					else
					{
						NxDbgMsg(NX_DBG_INFO, "%2s ==== oon_job_id(%d), numWorks(%d), jobcnt(%d)\n", "", oon_job_id, numWorks, jobcnt);
						if (oon_job_id == numWorks)
						{
							tstimer_time(&ts_last_oon);
							tstimer_diff(&ts_last_oon, &ts_start, &ts_diff);
							ishashdone = true;
							break;
						}
					}
				}
			}
		}
#else
#if DETAIL_CALC_HASHRATE
		{
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

#if DETAIL_CALC_HASHRATE
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
						NxDbgMsg(NX_DBG_INFO, "%2s oon_job_id(%d) numWorks(%d), jobcnt(%d)\n", "", oon_job_id, numWorks, jobcnt);
#if DETAIL_CALC_HASHRATE
						if (oon_job_id == numWorks)
						{
							int result = memcmp(hashdone_chip, hashdone_allchip, sizeof(uint8_t) * handle->numChips);
							if (result == 0)
							{
								tstimer_time(&ts_last_oon);
								tstimer_diff(&ts_last_oon, &ts_start, &ts_diff);
								ishashdone = true;
								break;
							}
						}
#else
						tstimer_time(&ts_last_oon);
						tstimer_diff(&ts_last_oon, &ts_start, &ts_diff);
						ishashdone = true;
						break;
#endif
					}
				}
				else {						// OON IRQ is not set (cgminer: check OON timeout is expired)
					//NxDbgMsg(NX_DBG_INFO, "%5s === OON IRQ is not set! ===\n", "");
					// if oon timeout is expired, disable chips
					// if oon timeout is not expired, check oon gpio again
				}
#if DETAIL_CALC_HASHRATE
			}
		}
#endif
#endif
		sched_yield();
	}

	// The expected hashrate = 600 mhash/sec [MinerCoreClk(50MHz) * NumOfCores(3cores) * AsicBoost(4sub-cores)]
	calc_hashrate(handle->isAsicBoost, jobcnt, &ts_diff);

	if (oon_job_id != numWorks)
		NxDbgMsg(NX_DBG_INFO, "=== Test Failed!!!");
	else
		NxDbgMsg(NX_DBG_INFO, "=== Test Succeed!!!");

	DestroyBtc08( handle );
}

static void dump_work(char* title, struct VECTOR_DATA *work)
{
	char *header, *prev_blockhash, *merkle_root, *timestamp, *nbits;
	char *midstate, *midstate1, *midstate2, *midstate3, *target;

	header         = bin2hex(work->data,         128);
	prev_blockhash = bin2hex(work->data+4,        32);
	merkle_root    = bin2hex(work->data+4+32,     32);
	timestamp      = bin2hex(work->data+4+32+32,   4);
	nbits          = bin2hex(work->data+4+32+32+4, 4);

	midstate       = bin2hex(work->midState,      32);
	midstate1      = bin2hex(work->midState+32,     32);
	midstate2      = bin2hex(work->midState+32+32,     32);
	midstate3      = bin2hex(work->midState+32+32+32,     32);
	target         = bin2hex(work->target,        32);

	NxDbgMsg(NX_DBG_INFO,  "================== %s ==================\n", title);
	NxDbgMsg(NX_DBG_INFO, "header        : %s\n", header);
	NxDbgMsg(NX_DBG_INFO, "prev_blockhash: %s\n", prev_blockhash);
	NxDbgMsg(NX_DBG_INFO, "merkle_root   : %s\n", merkle_root);
	NxDbgMsg(NX_DBG_INFO, "timestamp     : %s\n", timestamp);
	NxDbgMsg(NX_DBG_INFO, "nbits         : %s\n", nbits);

	NxDbgMsg(NX_DBG_INFO, "midstate      : %s\n", midstate);
	NxDbgMsg(NX_DBG_INFO, "midstate1     : %s\n", midstate1);
	NxDbgMsg(NX_DBG_INFO, "midstate2     : %s\n", midstate2);
	NxDbgMsg(NX_DBG_INFO, "midstate3     : %s\n", midstate3);
	NxDbgMsg(NX_DBG_INFO, "target        : %s\n", target);

	free(header);
	free(prev_blockhash);
	free(merkle_root);
	free(timestamp);
	free(nbits);
	free(midstate);
	free(midstate1);
	free(midstate2);
	free(midstate3);
	free(target);
	NxDbgMsg(NX_DBG_INFO, "=======================================================================\n");
}

static void dump_work_list(BTC08_HANDLE handle)
{
	char s[1024];

	for (int i=0; i < JOB_ID_NUM_MASK; i++)
	{
		snprintf(s, sizeof(s), "[WORK LIST] handle->work[%d] index:%d\n",
				i, handle->work[i].job_id);
		NxDbgMsg(NX_DBG_INFO, "%s", s);
	}
}

static bool set_work(BTC08_HANDLE handle, VECTOR_DATA *data)
{
	bool retval = false;
	int job_id = handle->last_queued_id + 1;

	Btc08WriteParam(handle, BCAST_CHIP_ID, data->midState, data->parameter);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, data->target);
	DistributionNonce(handle);
	for( int i=0; i<handle->numChips ; i++ )
	{
		NxDbgMsg( NX_DBG_INFO, "Chip[%d:%d] : %02x%02x%02x%02x ~ %02x%02x%02x%02x\n", i, handle->numCores[i],
			handle->startNonce[i][0], handle->startNonce[i][1], handle->startNonce[i][2], handle->startNonce[i][3],
			handle->endNonce[i][0], handle->endNonce[i][1], handle->endNonce[i][2], handle->endNonce[i][3] );
		Btc08WriteNonce(handle, i+1, handle->startNonce[i], handle->endNonce[i]);
	}
	NxDbgMsg(NX_DBG_INFO, "%2s Run Job with jobId#%d\n", "", job_id);
	if (-1 != Btc08RunJob(handle, BCAST_CHIP_ID, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), job_id))
	{
		memset(&(handle->work[handle->last_queued_id]), 0,    sizeof(struct VECTOR_DATA));
		memcpy(&(handle->work[handle->last_queued_id]), data, sizeof(struct VECTOR_DATA));
		handle->work[handle->last_queued_id].job_id = job_id;
#if DEBUG
			char s[2048];
			snprintf(s, sizeof(s), "[NEW WORK] handle->work[%d] local_job_id:%d, work_job_id:%d\n",
					handle->last_queued_id, job_id, handle->work[handle->last_queued_id].job_id);
			dump_work(s, &(handle->work[handle->last_queued_id]));
			dump_work_list(handle);
#endif
		handle->last_queued_id++;
		if (handle->last_queued_id >= JOB_ID_NUM_MASK)		// range: 0 ~ JOB_ID_NUM_MASK (0~7)
			handle->last_queued_id = 0;
	}

	return retval;
}

/* Process the works with asicboost and version mask. (4 works > oon > 2 works > oon > 2 works > ...)
 * If OON IRQ occurs, clear OON and then pass the additional work to job fifo.
 * If GN IRQ occurs, read GN and then clear GN IRQ.
 * Test Results (Ignore 1~4th hashresults)
 * 3 chips, core 2/1/2: HashRate = 1010.6 mhash/sec (Works = 4, Hashes = 68719 mhash, Total Time = 68s)
 * 3 chips, core 1/1/1: HashRate = 602.8 mhash/sec  (Works = 4, Hashes = 68719 mhash, Total Time = 114s)
 */
static void TestInfiniteWorkLoop()
{
	uint8_t chipId = 0x00;
	uint8_t fifo_full = 0x00, oon_irq = 0x00, gn_irq = 0x00, oon_job_id = 0x00, gn_job_id = 0x00;
	uint8_t res[4] = {0x00,};
	uint8_t oon_jobid, gn_jobid;
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	bool isErr = false;
	VECTOR_DATA data;
	uint64_t startTime, currTime, deltaTime, prevTime;
	uint64_t totalProcessedHash = 0;
	double totalTime;
	double megaHash;
	uint8_t micro_job_id;
	uint32_t found_nonce[4];
	int index = 0;

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

	for (int chipId = 1; chipId <= handle->numChips; chipId++)
		SetPll(handle, chipId, 4);

	// Seqeunce 6. Find number of cores of individual chips
	RunBist( handle );

	// Sequence 7. Enable OON IRQ/Set UART divider
	Btc08SetControl(handle, BCAST_CHIP_ID, (OON_IRQ_EN | UART_DIVIDER));

	// Sequence 8. Setting parameters, target, nonce range, run job
	for (int i = 0; i < MAX_JOB_FIFO_NUM; i++)
	{
		GetGoldenVectorWithVMask(index++, &data, 0);
		set_work(handle, &data);
		if (index >= MAX_NUM_VECTOR)
			index = 0;
	}

	startTime = get_current_ms();
	prevTime = startTime;

	while(!isErr)
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
					if (0 == handleGN3(handle, chipId, (uint8_t *)found_nonce, &micro_job_id, &data))
					{
						struct VECTOR_DATA work;
						memcpy(&work, &(handle->work[gn_job_id - 1]), sizeof(struct VECTOR_DATA));
						memset(work.data+76, 0, 4);
						memset(work.hash, 0, 32);
						HexDump("work", &work, sizeof(struct VECTOR_DATA));

						for (int i=0; i<4; i++)
						{
							if((micro_job_id & (1<<i)) != 0)
							{
								memcpy(work.data, &(work.vmask_001[(1<<i)]), 4);
#if DEBUG
								dump_work("[GN]", &work);
								dump_work_list(handle);
#endif
								if (!submit_nonce(&work, found_nonce[i])) {
									NxDbgMsg(NX_DBG_ERR, "%5s Failed: invalid nonce 0x%08x\n", "", found_nonce[i]);
									isErr = true;
									break;
								} else {
									NxDbgMsg(NX_DBG_INFO, "%5s Succeed: valid nonce 0x%08x\n", "", found_nonce[i]);
								}
							}
						}
					}
				} else {				// If GN IRQ is not set, then go to check OON
					//NxDbgMsg(NX_DBG_INFO, "%5s === H/W GN occured but GN_IRQ value is not set!!!\n", "");
				}
			}
		}

		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_OON))	// Check OON
		{
			Btc08ClearOON(handle, BCAST_CHIP_ID);

			totalProcessedHash += 0x800000000;	//	0x100000000 * 2(works) * 4(asic booster)

			currTime = get_current_ms();
			totalTime = currTime - startTime;

			megaHash = totalProcessedHash / (1000*1000);
			NxDbgMsg(NX_DBG_INFO, "AVG : %.2f MHash/s, Hash = %.2f GH, Time = %.2f sec, delta = %lld msec\n",
					megaHash * 1000. / totalTime, megaHash/1000, totalTime/1000. , currTime - prevTime );

			prevTime = currTime;

			for (int i = 0; i < 2; i++)
			{
				GetGoldenVectorWithVMask(index, &data, 0);
				set_work(handle, &data);
				if (index >= MAX_NUM_VECTOR)
					index = 0;
			}
		}
		sched_yield();
	}

	if (isErr)
		NxDbgMsg(NX_DBG_INFO, "=== Test Failed!!!");
	else
		NxDbgMsg(NX_DBG_INFO, "=== Test Succeed!!!");

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
					(i==0) ? "Upper":(((i==1) ? "Lower": ((i==2) ? "Lower_2":"Lower_3"))));
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

/* Useful for finding timestamps */
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
			gn_jobid  = res[1];
			gn_irq 	  = res[2] & (1<<0);
			chipId    = res[3];

			if (0 != gn_irq) {		// If GN IRQ is set, then handle GN
				// Check if found GN(0x66cb3426) is correct and submit nonce to pool server and then go loop again
				NxDbgMsg(NX_DBG_INFO, "%5s === GN IRQ on chip#%d gn_jobid:%d!!! ===\n", "", chipId, gn_jobid);

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
			uint8_t last_chipId = 1;
			if( cmdCnt > 1 )
			{
				last_chipId = strtol(cmd[1], 0, 10);
			}
			TestWork(last_chipId);
		}
		//----------------------------------------------------------------------
		else if( !strcasecmp(cmd[0], "2") )
		{
			int numWork = 4;
			uint8_t last_chipId = 1;
			if( cmdCnt > 1 )
			{
				numWork = strtol(cmd[1], 0, 10);
			}
			if (cmdCnt > 2)
			{
				last_chipId = strtol(cmd[2], 0, 10);
			}
			printf("numWork = %d last_chipId = %d\n", numWork, last_chipId);
			TestWorkLoop( numWork, last_chipId );
		}
		//----------------------------------------------------------------------
		else if( !strcasecmp(cmd[0], "3") )
		{
			TestWorkLoop_RandomVector();
		}
		else if( !strcasecmp(cmd[0], "4") )
		{
			TestInfiniteWorkLoop();
		}
	}
}
