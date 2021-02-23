#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <byteswap.h>
#include <sched.h>
#include "Utils.h"
#include "Btc08.h"

#include "TestVector.h"

#ifdef NX_DTAG
#undef NX_DTAG
#endif
#define NX_DTAG "[SimpleWork]"
#include "NX_DbgMsg.h"

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

static void RunBist( BTC08_HANDLE handle, int numChips )
{
	uint8_t *ret;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	Btc08WriteParam (handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);

	// Set the golden nonce instead of the nonce range
	Btc08WriteNonce (handle, BCAST_CHIP_ID, golden_nonce, golden_nonce);
	Btc08SetDisable (handle, BCAST_CHIP_ID, golden_enable);
	Btc08RunBist    (handle, default_golden_hash, default_golden_hash, default_golden_hash, default_golden_hash);

	for (int chipId = 1; chipId <= numChips; chipId++)
	{
		// If it's not BUSY status, read the number of cores in next READ_BIST
		for (int i=0; i<10; i++) {
			ret = Btc08ReadBist(handle, chipId);
			if ( (ret[0] & 1) == 0 )
				break;
			else
				NxDbgMsg( NX_DBG_INFO, "ChipId = %d, Status = %s, Number of cores = %d\n",
						chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );

			usleep( 300 );
		}
		ret = Btc08ReadBist(handle, chipId);
		numCores[chipId] = ret[1];
		NxDbgMsg( NX_DBG_INFO, "ChipId = %d, Status = %s, Number of cores = %d\n",
					chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );
	}
	for (int chipId = 1; chipId <= numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
		NxDbgMsg( NX_DBG_INFO, "ChipId = %d, Number of jobs = %d\n",
					chipId, (res[2]&7) );
	}
}

static uint32_t calRealGN(const uint8_t chipId, const uint8_t *in, const uint8_t valid_cnt)
{
	uint32_t *gn = (uint32_t *)in;

	*gn = bswap_32(*gn);

	NxDbgMsg(NX_DBG_DEBUG, "in[0x%08x] swap[0x%08x] cal[0x%08x] \n",
			in, gn, (*gn - valid_cnt * numCores[chipId]));

	return (*gn - valid_cnt * numCores[chipId]);
}

static int handleGN(BTC08_HANDLE handle, uint8_t chipId)
{
	int ret = 0;
	uint8_t hash[128] = {0x00,};
	unsigned int hash_size = sizeof(hash)/sizeof(hash[0]);
	uint8_t gn[18] = {0x00,};
	unsigned int gn_size = sizeof(gn)/sizeof(gn[0]);
	uint8_t lower3, lower2, lower, upper, validCnt;
	int match;

	// Read Hash
	Btc08ReadHash(handle, chipId, hash, hash_size);

	// Read Result to read GN and clear GN IRQ
	Btc08ReadResult(handle, chipId, gn, gn_size);
	validCnt = gn[1];

	for (int i=0; i<16; i+=4)
	{
		uint32_t cal_gn = calRealGN(chipId, &(gn[2+i]), validCnt);
		if (cal_gn == 0x66cb3426)
		{
			NxDbgMsg(NX_DBG_INFO, "Inst_%s found golden nonce 0x%08x \n",
					(i==0) ? "Lower_3":(((i==4) ? "Lower_2": ((i==8) ? "Lower":"Upper"))),
					cal_gn);
			ret = 1;
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
	int numChips;
	int chipId = 0x00, jobId=0x01;
	struct timespec ts_start, ts_oon, ts_gn, ts_diff;
	uint8_t fifo_full = 0x00, oon_irq = 0x00, gn_irq = 0x00;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	uint8_t start_nonce[4] = { 0x60, 0x00, 0x00, 0x00 };
	uint8_t end_nonce[4]   = { 0x6f, 0xff, 0xff, 0xff };

	tstimer_time(&ts_start);
	NxDbgMsg(NX_DBG_INFO, "[%ld.%lds] Start of TestWork!\n", ts_start.tv_sec, ts_start.tv_nsec);

	// Seqeunce 1. Create Handle
	BTC08_HANDLE handle = CreateBtc08(0);
	Btc08ResetHW(handle, 1);
	Btc08ResetHW(handle, 0);

	// Seqeunce 2. Find number of chips : using AutoAddress
	numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "Number of Chips = %d\n", numChips);

	// Seqeunce 3. Find number of cores of individual chips
	RunBist( handle, numChips );

	//
	// RUN_JOB for one work without asicboost  : Please FIXME
	//	

	// Seqeunce 4. Setting Parameters
	Btc08WriteParam(handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);

	// Seqeunce 5. Setting Target Parameters
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);

	// Sequence 6. Setting Nonce Range
	Btc08WriteNonce(handle, BCAST_CHIP_ID, start_nonce, end_nonce);

	// Sequence 7. Run Job
	Btc08RunJob(handle, BCAST_CHIP_ID, ASIC_BOOST_EN, jobId++);


	// Sequence 8. Check Interrupt Signals(GPIO) and Post Processing

	//	FIXME : This loop is basically busy wait type,
	//	If possible, is should be changed using a interrupt.
	while(1)
	{
		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_GN))	// Check GN GPIO pin
		{
			Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
			gn_irq 	  = res[2] & (1<<0);
			chipId    = res[3];

			if (1 != gn_irq) {				// In case of not GN
				NxDbgMsg(NX_DBG_INFO, "H/W interrupt occured, but GN_IRQ value is not set!\n");
				break;						// Go to check OON
			} else {						// In case of GN
				// Check if found GN(0x66cb3426) is correct and submit nonce to pool server and then go loop again
				tstimer_time(&ts_gn);
				NxDbgMsg(NX_DBG_INFO, "[%ld.%lds] GN!!!\n", ts_gn.tv_sec, ts_gn.tv_nsec);

				handleGN(handle, chipId);
				// continue;
			}
		}

		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_OON))	// Check OON
		{
			// TODO: Need to check if it needs to read job id
			Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
			fifo_full = res[2] & (1<<2);
			oon_irq	  = res[2] & (1<<1);

			if (1 != oon_irq) {				// In case of not OON
				NxDbgMsg(NX_DBG_INFO, "OON IRQ is not set!\n");
				// if oon timeout is expired, disable chips
				// if oon timeout is not expired, check oon gpio again
			} else {
				tstimer_time(&ts_oon);
				NxDbgMsg(NX_DBG_INFO, "[%ld.%lds] OON!!!\n", ts_oon.tv_sec, ts_oon.tv_nsec);

				handleOON(handle);
				break;
			}
		}
		sched_yield();
	}

	tstimer_diff(&ts_oon, &ts_start, &ts_diff);

	NxDbgMsg(NX_DBG_INFO, "[%ld.%lds] End of TestWork \n", ts_diff.tv_sec, ts_diff.tv_nsec);

	DestroyBtc08( handle );
}


/* Process 4 works with asicboost at first.
 * If OON IRQ occurs, clear OON and then pass the additional work to job fifo.
 * If GN IRQ occurs, read GN and then clear GN IRQ.
 */
static void TestWorkLoop(int numWorks)
{
	int numChips;
	uint8_t chipId = 0x00, jobId = 0x01, jobcnt = 0x01;
	uint8_t fifo_full = 0x00, oon_irq = 0x00, gn_irq = 0x00;
	uint8_t res[4] = {0x00,};
	uint8_t oon_jobid, gn_jobid;
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	uint8_t start_nonce[4] = { 0x60, 0x00, 0x00, 0x00 };
	uint8_t end_nonce[4]   = { 0x6f, 0xff, 0xff, 0xff };

	struct timespec ts_start, ts_oon, ts_gn;

	tstimer_time(&ts_start);
	NxDbgMsg(NX_DBG_INFO, "[%ld.%lds] Start workloop!\n", ts_start.tv_sec, ts_start.tv_nsec);

	BTC08_HANDLE handle = CreateBtc08(0);

	Btc08ResetHW(handle, 1);
	Btc08ResetHW(handle, 0);

	// Auto Address
	numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "Number of Chips = %d\n", numChips);

	// RunBist
	RunBist( handle, numChips );

	// Write midstate and data(merkleroot, time, target) & target & start and end nonce
	Btc08WriteParam(handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
	Btc08WriteNonce(handle, BCAST_CHIP_ID, start_nonce, end_nonce);

	// Run jobs with asicboost
	for (int i = 0; i < MAX_JOB_FIFO_NUM; i++)
	{
		NxDbgMsg(NX_DBG_INFO, "JobId = %d\n", jobId);
		Btc08RunJob(handle, BCAST_CHIP_ID, ASIC_BOOST_EN, jobId++);
		jobcnt++;
	}

	while(1)
	{
		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_GN))	// Check GN GPIO pin
		{
			Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
			oon_jobid = res[0];
			gn_jobid  = res[1];
			gn_irq 	  = res[2] & (1<<0);
			chipId    = res[3];

			NxDbgMsg(NX_DBG_INFO, "oon_jobId=%d, gn_jobid = %d, res[2] = 0x%02x, chipId=%d\n",
					oon_jobid, gn_jobid, gn_irq, chipId );

			if (1 != gn_irq) {		// If GN IRQ is not set, then go to check OON
				NxDbgMsg(NX_DBG_INFO, "H/W GN occured but GN_IRQ value is not set!\n");
				break;
			} else {				// If GN IRQ is set, then handle GN
				// Check if found GN(0x66cb3426) is correct and submit nonce to pool server and then go loop again
				tstimer_time(&ts_gn);
				NxDbgMsg(NX_DBG_INFO, "[%ld.%lds] GN!!!\n", ts_gn.tv_sec, ts_gn.tv_nsec);

				handleGN(handle, chipId);
				//continue;
				Btc08RunJob(handle, BCAST_CHIP_ID, ASIC_BOOST_EN, jobId++);
				jobcnt++;
				if (jobId > MAX_JOB_ID)		// [7:0] job id ==> 8bits (max 256)
					jobId = 1;
			}
		}

		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_OON))	// Check OON
		{
			// TODO: Need to check if it needs to read job id
			Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
			fifo_full = res[2] & (1<<2);
			oon_irq	  = res[2] & (1<<1);

			if (1 != oon_irq) {				// OON IRQ is not set (cgminer: check OON timeout is expired)
				NxDbgMsg(NX_DBG_INFO, "OON IRQ is not set!\n");
				// if oon timeout is expired, disable chips
				// if oon timeout is not expired, check oon gpio again
			} else {						// If OON IRQ is set, handle OON
				tstimer_time(&ts_oon);
				NxDbgMsg(NX_DBG_INFO, "[%ld.%lds] OON!!!\n", ts_oon.tv_sec, ts_oon.tv_nsec);

				handleOON(handle);

				if (numWorks >= jobcnt)
				{
					NxDbgMsg(NX_DBG_INFO, "JobId = %d\n", jobId);
					Btc08RunJob(handle, BCAST_CHIP_ID, ASIC_BOOST_EN, jobId++);
					jobcnt++;
					if (jobId > MAX_JOB_ID)		// [7:0] job id ==> 8bits (max 256)
						jobId = 1;
				}
			}
		}

		sched_yield();
	}

	NxDbgMsg(NX_DBG_INFO, "Total works = %d\n", jobcnt);

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
	}
}
