#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "Utils.h"
#include "Btc08.h"

#include "TestVector.h"
#include <sched.h>

#ifdef NX_DTAG
#undef NX_DTAG
#endif
#define NX_DTAG "[SingleCommand]"
#include "NX_DbgMsg.h"

#define BTC08_NUM_CORES		30

static void singlecommand_command_list()
{
	printf("\n\n");
	printf("====== Single Command =======\n");
	printf("  1. H/W Reset\n");
	printf("  2. Reset and Auto Address \n");
	printf("  3. Reset and TestBist\n");
	printf("  4. Set the chip to the last chip\n");
	printf("    ex > 4 [chipId]\n");
	printf("  5. Disable core\n");
	printf("    ex > 5 [disable_core_num] [freq]\n");
	printf("  6. Write, Read Target\n");
	printf("  7. Set, Read disable\n");
	printf("  8. Read Revision\n");
	printf("  9. Read Feature\n");
	printf("  10. Read/Write IO Ctrl\n");
	printf("  11. 1 Core Test\n");
	printf("    ex > 11 29 (fixed frequency : 300)\n");
	printf("-----------------------------\n");
	printf("  q. quit\n");
	printf("=============================\n");
}

static int handleOON(BTC08_HANDLE handle, uint8_t chipId)
{
	int ret = 0;

#if 1
	Btc08ClearOON(handle, chipId);
#else
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	// Read ID to get FIFO status for chipId#1
	// The FIFO status of all chips are same.
	if (0 == Btc08ReadId (handle, chipId, res, res_size))
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
#endif
	return ret;
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

#if DEBUG
	// Sequence 1. Read Hash
	Btc08ReadHash(handle, chipId, hash, hash_size);
	for (int i=0; i<4; i++)
	{
		result = memcmp(default_golden_hash, &(hash[i*32]), 32);
		if (result == 0)
			NxDbgMsg(NX_DBG_INFO, "Result Hash of Inst_%s!!!\n",
					(i==0) ? "Upper":(((i==1) ? "Lower": ((i==2) ? "Lower_2":"Lower_3"))));
		else
		{
			NxDbgMsg(NX_DBG_ERR, "Failed: Result Hash of Inst_%s!!!\n",
					(i==0) ? "Upper":(((i==1) ? "Lower": ((i==2) ? "Lower_2":"Lower_3"))));
			ret = -1;
		}
	}
	for (int i=0; i<4; i++) {
		sprintf(title, "Inst_%s", (i==0) ? "Upper":(((i==1) ? "Lower": ((i==2) ? "Lower_2":"Lower_3"))));
		HexDump(title, &(hash[i*32]), 32);
	}
#endif

	// Sequence 2. Read Result to read GN and clear GN IRQ
	Btc08ReadResult(handle, chipId, res, res_size);
	for (int i=0; i<4; i++)
	{
		result = memcmp(golden_nonce, res + i*4, 4);
		if (result == 0)
			NxDbgMsg(NX_DBG_INFO, "[Inst_%s] %10s GN = %02x %02x %02x %02x \n",
				(i==0) ? "Upper":(((i==1) ? "Lower": ((i==2) ? "Lower_2":"Lower_3"))), "",
				*(res + i*4), *(res + i*4 + 1), *(res + i*4 + 2), *(res + i*4 + 3));
		else
		{
			NxDbgMsg(NX_DBG_ERR, "Failed: [Inst_%s] %10s GN = %02x %02x %02x %02x \n",
				(i==0) ? "Upper":(((i==1) ? "Lower": ((i==2) ? "Lower_2":"Lower_3"))), "",
				*(res + i*4), *(res + i*4 + 1), *(res + i*4 + 2), *(res + i*4 + 3));
			ret = -1;
		}
	}
	return ret;
}

void ReadBist(BTC08_HANDLE handle)
{
	uint8_t *ret;

	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		// If it's not BUSY status, read the number of cores in next READ_BIST
		for (int i=0; i<10; i++) {
			ret = Btc08ReadBist(handle, chipId);
			if ( (ret[0] & 1) == 0 )
				break;
			else
				NxDbgMsg( NX_DBG_INFO, "[READ_BIST] ChipId = %d, Status = %s, NumCores = %d\n",
						chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );

			usleep( 300 );
		}

		ret = Btc08ReadBist(handle, chipId);

		handle->numCores[chipId] = ret[1];
		NxDbgMsg( NX_DBG_INFO, "[READ_BIST] ChipId = %d, Status = %s, NumCores = %d\n",
					chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );
	}
}

void ReadId(BTC08_HANDLE handle)
{
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
		NxDbgMsg( NX_DBG_INFO, "[READ_ID] ChipId = %d, NumJobs = %d\n",
					chipId, (res[2]&7) );
	}
}

static void RunJob(BTC08_HANDLE handle)
{
	int chipId = 0x00, jobId=0x01;
	uint8_t fifo_full = 0x00, oon_irq = 0x00, gn_irq = 0x00, oon_job_id = 0x00, gn_job_id = 0x00;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	uint8_t oon_jobid, gn_jobid;
	struct timespec ts_start, ts_oon, ts_gn, ts_diff, ts_last_oon;
	uint64_t jobcnt = 0;
	uint64_t hashes_done;
	bool	ishashdone = false;
	uint64_t startTime, currTime, deltaTime, prevTime;
	uint64_t totalProcessedHash = 0;
	double totalTime;
	double megaHash;
	uint8_t hashdone_allchip[256];
	uint8_t hashdone_chip[256];
	uint8_t start_nonce[4] = { 0x66, 0x00, 0x00, 0x00 };
	uint8_t end_nonce[4]   = { 0x77, 0x00, 0x00, 0x00 };
	int numWorks = 4;

	//uint8_t start_nonce[4] = { 0x00, 0x00, 0x00, 0x00 };
	//uint8_t end_nonce[4]   = { 0xff, 0xff, 0xff, 0xff };

	memset(hashdone_allchip, 0xFF, sizeof(hashdone_allchip));
	memset(hashdone_chip,    0x00, sizeof(hashdone_chip));

	handle->isAsicBoost = true;

	tstimer_time(&ts_start);
	NxDbgMsg( NX_DBG_INFO, "[RUN JOB] [%ld.%lds]\n", ts_start.tv_sec, ts_start.tv_nsec);
	Btc08WriteParam(handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
	Btc08WriteNonce(handle, BCAST_CHIP_ID, start_nonce, end_nonce);
	for (int i=0; i<4; i++) {
		NxDbgMsg(NX_DBG_INFO, "%2s Run Job with jobId#%d\n", "", jobId);
		Btc08RunJob(handle, BCAST_CHIP_ID, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), jobId++);
		jobcnt++;
		ReadId(handle);
	}

	startTime = get_current_ms();
	prevTime = startTime;

	while(!ishashdone)
	{
		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_GN))	// Check GN GPIO pin
		{
			//for (int i=1; i<=handle->numChips; i++)
			{
				//Btc08ReadJobId(handle, i, res, res_size);
				Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
				gn_job_id  = res[1];
				gn_irq 	   = res[2] & (1<<0);
				chipId     = res[3];

				if (0 != gn_irq) {		// If GN IRQ is set, then handle GN
					tstimer_time(&ts_gn);
					tstimer_diff(&ts_gn, &ts_start, &ts_diff);
					NxDbgMsg(NX_DBG_INFO, "=== GN IRQ on chip#%d for jobId#%d!!! [%ld.%lds] ==> [%ld.%lds]\n",
								chipId, gn_job_id, ts_gn.tv_sec, ts_gn.tv_nsec, ts_diff.tv_sec, ts_diff.tv_nsec);
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
		{
			Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
			oon_job_id = res[0];
			oon_irq	  = res[2] & (1<<1);
			chipId    = res[3];

			if (0 != oon_irq)				// If OON IRQ is set, handle OON
			{
				NxDbgMsg(NX_DBG_INFO, "*** OON IRQ on chip#%d for oon_job_id:%d!!! ***\n", chipId, oon_job_id);
				handleOON(handle, BCAST_CHIP_ID);

				//totalProcessedHash += (0x100000000 * 4);	//	0x100000000 * 4 (asic booster)
				totalProcessedHash += (0x11000000 * 4);	//	0x100000000 * 4 (asic booster)

				currTime = get_current_ms();
				totalTime = currTime - startTime;

				megaHash = totalProcessedHash / (1000*1000);
				NxDbgMsg(NX_DBG_INFO, "AVG : %.2f MHash/s,  Hash = %.2f GH, Time = %.2f sec, delta = %lld msec\n",
						megaHash * 1000. / totalTime, megaHash/1000, totalTime/1000. , currTime - prevTime );

				prevTime = currTime;

				if (oon_job_id == numWorks)
					ishashdone = true;
				/*Btc08RunJob(handle, BCAST_CHIP_ID, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), jobId++);
				if (jobId == 256)
					jobId = 0;*/
			} 
			else		// OON IRQ is not set (cgminer: check OON timeout is expired)
			{
				//NxDbgMsg(NX_DBG_INFO, "%5s === OON IRQ is not set! ===\n", "");
				// if oon timeout is expired, disable chips
				// if oon timeout is not expired, check oon gpio again
			}
		}
		sched_yield();
	}

	if (oon_job_id != numWorks)
		NxDbgMsg(NX_DBG_INFO, "=== Test Failed!!!");
	else
		NxDbgMsg(NX_DBG_INFO, "=== Test Succeed!!!");
}

static void Run1Job(BTC08_HANDLE handle)
{
	int chipId = 0x00, jobId=0x01;
	uint8_t fifo_full = 0x00, oon_irq = 0x00, gn_irq = 0x00, oon_job_id = 0x00, gn_job_id = 0x00;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	uint8_t oon_jobid, gn_jobid;
	struct timespec ts_start, ts_oon, ts_gn, ts_diff, ts_last_oon;
	uint64_t jobcnt = 0;
	uint64_t hashes_done;
	bool	ishashdone = false;
	uint64_t startTime, currTime, deltaTime, prevTime;
	uint64_t totalProcessedHash = 0;
	double totalTime;
	double megaHash;
	uint8_t start_nonce[4] = { 0x66, 0x00, 0x00, 0x00 };
	uint8_t end_nonce[4]   = { 0x77, 0x00, 0x00, 0x00 };
	int numWorks = 1;

	handle->isAsicBoost = true;

	tstimer_time(&ts_start);
	NxDbgMsg( NX_DBG_INFO, "[RUN JOB] [%ld.%lds]\n", ts_start.tv_sec, ts_start.tv_nsec);

	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
	Btc08WriteNonce(handle, 1, start_nonce, end_nonce);
	//Btc08WriteParam(handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	NxDbgMsg(NX_DBG_INFO, "%2s Run Job with jobId#%d\n", "", jobId);
	Btc08RunJob(handle, BCAST_CHIP_ID, ASIC_BOOST_EN, jobId++);
	jobcnt++;

	startTime = get_current_ms();
	prevTime = startTime;

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
					NxDbgMsg(NX_DBG_INFO, "=== GN IRQ on chip#%d for jobId#%d!!! [%ld.%lds]\n",
								chipId, gn_job_id, ts_gn.tv_sec, ts_gn.tv_nsec);
					tstimer_diff(&ts_gn, &ts_start, &ts_diff);
					NxDbgMsg(NX_DBG_INFO, "[%ld.%lds]\n", ts_diff.tv_sec, ts_diff.tv_nsec);
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
		{
			Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
			oon_job_id = res[0];
			oon_irq	  = res[2] & (1<<1);
			chipId    = res[3];

			if (0 != oon_irq)				// If OON IRQ is set, handle OON
			{
				NxDbgMsg(NX_DBG_INFO, "*** OON IRQ on chip#%d for oon_job_id:%d!!! ***\n", chipId, oon_job_id);
				handleOON(handle, BCAST_CHIP_ID);

				//totalProcessedHash += (0x100000000 * 4);	//	0x100000000 * 4 (asic booster)
				totalProcessedHash += (0x11000000 * 4);	//	0x100000000 * 4 (asic booster)

				currTime = get_current_ms();
				totalTime = currTime - startTime;

				megaHash = totalProcessedHash / (1000*1000);
				NxDbgMsg(NX_DBG_INFO, "AVG : %.2f MHash/s,  Hash = %.2f GH, Time = %.2f sec, delta = %lld msec\n",
						megaHash * 1000. / totalTime, megaHash/1000, totalTime/1000. , currTime - prevTime );

				prevTime = currTime;

				if (oon_job_id == numWorks)
					ishashdone = true;
				/*Btc08RunJob(handle, BCAST_CHIP_ID, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), jobId++);
				if (jobId == 256)
					jobId = 0;*/
			}
			else		// OON IRQ is not set (cgminer: check OON timeout is expired)
			{
				//NxDbgMsg(NX_DBG_INFO, "%5s === OON IRQ is not set! ===\n", "");
				// if oon timeout is expired, disable chips
				// if oon timeout is not expired, check oon gpio again
			}
		}

		sched_yield();
	}

	if (oon_job_id != numWorks)
		NxDbgMsg(NX_DBG_INFO, "=== Test Failed!!!");
	else
		NxDbgMsg(NX_DBG_INFO, "=== Test Succeed!!!");
}

static int HWReset( BTC08_HANDLE handle )
{
	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );
	NxDbgMsg( NX_DBG_INFO, "Hardware Reset Done.\n" );
	return 0;
}

static int ResetAutoAddress( BTC08_HANDLE handle )
{
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	//	create BTC08 instance into index 0. ( /dev/spidev0.0 )
	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg( NX_DBG_INFO, "[AUTO_ADDRESS] NumChips = %d\n", handle->numChips );
	ReadId(handle);

	return 0;
}

/* Used to disable the chips */
void TestLastChip( BTC08_HANDLE handle, uint8_t last_chipId )
{
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg( NX_DBG_INFO, "[AUTO ADDRESS] NumChips = %d\n", handle->numChips );
	ReadId(handle);

	NxDbgMsg( NX_DBG_INFO, "[SET_CONTROL] SET chipId #%d as the LAST CHIP\n", last_chipId );
	Btc08SetControl(handle, last_chipId, LAST_CHIP);

	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg( NX_DBG_INFO, "[AUTO ADDRESS] NumChips = %d\n", handle->numChips );
	ReadId(handle);
}

/*
 * seq1. AUTO_ADDRESS > READ_ID
 * seq2. Set PLL
 * seq3. SET_CONTROL (OON_ENB|UART_DIVIDER) ex. 32'h00000018
 * seq4. WRITE_PARAM, WRITE_NONCE, WRITE_TARGET
 * seq5. SET_DISABLE
 * seq6. RUN_BIST > READ_BIST
 * seq7. WRITE_PARAM, WRITE_NONCE > RUN_JOB
 * seq8. loop {
 * 		    wait until GN_IRQ==1
 * 			    if (gn_chip_id != 0) { CLEAR_OON(BR) }
 * 			    if (gn_flag)		 { READ_HASH > READ_RESULT > READ_ID }
 *       }
*/
/* Disable core */
void TestDisableCore( BTC08_HANDLE handle, uint8_t disable_core_num, uint32_t pll_freq )
{
	uint8_t *ret;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	uint8_t disable_cores[32] = {0x00,};
	uint8_t last_chip_id = 1;

	// Disable cores
	memset(disable_cores, 0xff, 32);
	if( disable_core_num > 0 )
		disable_cores[31] &= ~(1);
	for (int i=1; i<(BTC08_NUM_CORES-disable_core_num); i++) {
		disable_cores[31-(i/8)] &= ~(1 << (i % 8));
	}
	HexDump("[disable_cores]", disable_cores, 32);

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	// seq1. AUTO_ADDRESS > READ_ID
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "[AUTO_ADDRESS] NumChips = %d\n", handle->numChips);
	ReadId(handle);

	NxDbgMsg( NX_DBG_INFO, "[RESET]\n");
	Btc08Reset(handle);

#if 0
	NxDbgMsg( NX_DBG_INFO, "[SET_CONTROL] Set chipId#1 as the LAST CHIP\n");
	// Set last chip
	Btc08SetControl(handle, last_chip_id, LAST_CHIP);
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "[AUTO_ADDRESS] NumChips = %d\n", handle->numChips);
	ReadId(handle);
#endif
	NxDbgMsg( NX_DBG_INFO, "[SET_DISABLE] Enable all cores ==\n");
	// Enable all cores
	Btc08SetDisable (handle, BCAST_CHIP_ID, golden_enable);

	NxDbgMsg( NX_DBG_INFO, "[SET_PLL] Set PLL %d\n", pll_freq);
	// seq2. Set PLL
	if (0 > SetPllFreq(handle, pll_freq))
		return;

	// seq4. WRITE_PARAM, WRITE_NONCE, WRITE_TARGET
	NxDbgMsg( NX_DBG_INFO, "[WRITE_PARAM, WRITE_NONCE, WRITE_TARGET]\n");
	Btc08WriteParam (handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	Btc08WriteNonce (handle, BCAST_CHIP_ID, golden_nonce, golden_nonce);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
	// seq5. SET_DISABLE
	NxDbgMsg( NX_DBG_INFO, "[SET_DISABLE] Disable %d cores\n", disable_core_num);
	for (int i=0; i<handle->numChips; i++) {
		Btc08SetDisable (handle, i, disable_cores);
	}

	// seq6. RUN_BIST > READ_BIST
	NxDbgMsg( NX_DBG_INFO, "[RUN_BIST]\n");
	Btc08RunBist    (handle, BCAST_CHIP_ID, default_golden_hash, default_golden_hash, default_golden_hash, default_golden_hash);
	ReadBist(handle);

	// seq3. SET_CONTROL (OON_ENB|UART_DIVIDER) 32'h0000_0018
	NxDbgMsg( NX_DBG_INFO, "[SET_CONTROL] 0x%02x\n", (OON_IRQ_EN | UART_DIVIDER));
	Btc08SetControl(handle, BCAST_CHIP_ID, (OON_IRQ_EN | UART_DIVIDER));

	// Run Job
	RunJob(handle);
}

void TestBist( BTC08_HANDLE handle, int pll_freq )
{
	uint8_t *ret;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	//	create BTC08 instance into index 0. ( /dev/spidev0.0 )

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	// AUTO_ADDRESS > READ_ID
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "[AUTO_ADDRESS] NumChips = %d\n", handle->numChips);
	ReadId(handle);

	// Software reset (TODO: Check FIFO status if it's empty)
	NxDbgMsg( NX_DBG_INFO, "[RESET]\n");
	Btc08Reset(handle);

	NxDbgMsg( NX_DBG_INFO, "[SET_CONTROL] Set chipId#1 as the LAST CHIP\n");
	// Set last chip
	Btc08SetControl(handle, 1, LAST_CHIP);
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "[AUTO_ADDRESS] NumChips = %d\n", handle->numChips);

	NxDbgMsg( NX_DBG_INFO, "[SET_DISABLE] Enable all cores\n");
	// Enable all cores
	Btc08SetDisable (handle, BCAST_CHIP_ID, golden_enable);

	NxDbgMsg( NX_DBG_INFO, "[SET_PLL] Set PLL %d\n", pll_freq);
	// 6. Set PLL freq
	if (0 > SetPllFreq(handle, pll_freq))
		return;

	NxDbgMsg( NX_DBG_INFO, "[BIST] Run BIST\n");
	// Write param for BIST
	Btc08WriteParam (handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
	Btc08WriteNonce (handle, BCAST_CHIP_ID, golden_nonce, golden_nonce);
	// Run BIST
	Btc08RunBist    (handle, BCAST_CHIP_ID, default_golden_hash, default_golden_hash, default_golden_hash, default_golden_hash);
	ReadBist(handle);
}

/* WRITE/READ TARGET */
static int TestWRTarget( BTC08_HANDLE handle )
{
	int debug = 0;
	char title[512];
	uint8_t res[140] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "[AUTO_ADDRESS] NumChips = %d\n", handle->numChips);
	ReadId(handle);

	for (int chipId=1; chipId <= handle->numChips; chipId++)
	{
		NxDbgMsg( NX_DBG_INFO, "[WRITE_TARGET] Chip%d\n", chipId);
		if (0 == Btc08WriteTarget(handle, chipId, default_golden_target))
		{
			Btc08ReadTarget(handle, chipId, res, 6);
			if (0 != memcmp(default_golden_target, res, 6))
			{
				NxDbgMsg(NX_DBG_ERR, "[READ_TARGET] Failed on chip#%d\n", chipId);
				if (debug) {
					HexDump("write_target:", default_golden_target, 6);
					sprintf(title, "chipId(%d) read_target:", chipId);
					HexDump(title, res, 6);
					return -1;
				}
			}
		}
		else
		{
			NxDbgMsg(NX_DBG_ERR, "[WRITE_TARGET] Failed on chip#%d due to spi err ==\n", chipId);
			return -1;
		}
	}

	NxDbgMsg(NX_DBG_INFO, "[WRITE_TARGET(BR)] ==\n");
	if (0 == Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target))
	{
		for (int chipId=1; chipId <= handle->numChips; chipId++)
		{
			Btc08ReadTarget(handle, chipId, res, 6);
			if (0 != memcmp(default_golden_target, res, 6))
			{
				NxDbgMsg(NX_DBG_ERR, "[READ_TARGET] Failed on chip#%d\n", chipId);
				HexDump("write_target:", default_golden_target, 6);
				sprintf(title, "chipId(%d) read_target:", chipId);
				HexDump(title, res, 6);
				return -1;
			}
		}
	}
	else
	{
		NxDbgMsg(NX_DBG_ERR, "[WRITE_TARGET] Failed due to spi err ==\n");
		return -1;
	}

	NxDbgMsg(NX_DBG_INFO, "Succeed READ/WRITE_TARGET\n");

	return 0;
}

/* SET_DISABLE > READ_DISABLE */
static int TestWRDisable( BTC08_HANDLE handle )
{
	char title[512];
	uint8_t *ret;
	uint8_t res[32] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	uint8_t enable_all[32] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	uint8_t disable_all[32] = {
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	};
	uint8_t disable_1core[32] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	};
	uint8_t disable_2cores[32] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
	};


	uint8_t *test_data1[3] = { disable_1core, disable_1core, disable_1core };
	uint8_t *test_data2[3] = { disable_2cores, disable_1core, disable_1core };
	uint8_t *test_data3[3] = { disable_2cores, disable_1core, disable_2cores };

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	// AUTO_ADDRESS
	handle->numChips = Btc08AutoAddress(handle);

	Btc08Reset(handle);

	for (int chipId=1; chipId <= handle->numChips; chipId++)
	{
		Btc08ReadDisable(handle, chipId, res, 32);
		HexDump("read_disable", res, 32);
	}
	
	// SET_DISABLE (per Chip)
	for (int chipId=1; chipId <= handle->numChips; chipId++)
	{
		NxDbgMsg( NX_DBG_INFO, "=== SET_DISABLE(chip%d) ==\n", chipId);

		if (0 == Btc08SetDisable(handle, chipId, test_data1[chipId-1]))
		{
			// RUN_BIST
			Btc08WriteParam (handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
			Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
			Btc08WriteNonce (handle, BCAST_CHIP_ID, golden_nonce, golden_nonce);
			Btc08RunBist    (handle, BCAST_CHIP_ID, default_golden_hash, default_golden_hash, default_golden_hash, default_golden_hash);

			// READ_BIST
			ReadBist(handle);

			// SET_DISABLE
			Btc08ReadDisable(handle, chipId, res, 32);
			if (0 != memcmp(test_data1[chipId-1], res, 32))
			{
				NxDbgMsg(NX_DBG_ERR, "=== Failed READ_DISABLE(chip#%d) ==\n", chipId);
				HexDump("set_disable:", test_data1[chipId-1], 32);
				sprintf(title, "chipId(%d) read_disable:", chipId);
				HexDump(title, res, 32);
				return -1;
			}
		}
		else {
			NxDbgMsg(NX_DBG_ERR, "=== Failed SET_DISABLE(per Chip) due to spi err ==\n");
			return -1;
		}
	}

	// SET_DISABLE (BR)
	NxDbgMsg( NX_DBG_INFO, "=== SET_DISABLE(BR) ==\n");
	if (0 == Btc08SetDisable(handle, BCAST_CHIP_ID, enable_all))
	{
		// RUN_BIST
		Btc08WriteParam (handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
		Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
		Btc08WriteNonce (handle, BCAST_CHIP_ID, golden_nonce, golden_nonce);
		Btc08RunBist    (handle, BCAST_CHIP_ID, default_golden_hash, default_golden_hash, default_golden_hash, default_golden_hash);

		// READ_BIST
		for (int chipId = 1; chipId <= handle->numChips; chipId++)
		{
			// READ_BIST
			ReadBist(handle);

			// SET_DISABLE
			Btc08ReadDisable(handle, chipId, res, 32);
			if (0 != memcmp(enable_all, res, 32))
			{
				NxDbgMsg(NX_DBG_ERR, "=== Failed READ_DISABLE(chip#%d) ==\n", chipId);
				HexDump("set_disable:", enable_all, 32);
				sprintf(title, "chipId(%d) read_disable:", chipId);
				HexDump(title, res, 32);
				return -1;
			}
		}
	}
	else {
		NxDbgMsg(NX_DBG_ERR, "=== Failed SET_DISABLE(BR) due to spi err ==\n");
		return -1;
	}

	NxDbgMsg(NX_DBG_INFO, "=== Succeed SET/READ_DISABLE ==\n");

	return 0;
}

static int TestReadRevision( BTC08_HANDLE handle )
{
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	uint8_t fixed_rev[4] = {0x20, 0x01, 0x08, 0x00};

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	handle->numChips = Btc08AutoAddress(handle);
	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		if (0 == Btc08ReadRevision(handle, chipId, res, res_size))
		{
			NxDbgMsg(NX_DBG_INFO, "=== Succeed to read revision(year:%02x, month:%02x, day:%02x, index:%02x) ==\n",
						res[0], res[1], res[2], res[3]);

			if (0 != memcmp(fixed_rev, res, 4))
			{
				NxDbgMsg(NX_DBG_ERR, "=== Not matched revision ==\n");
				HexDump("READ_REVISION", res, 4);
				HexDump("fixed_rev", fixed_rev, 4);
				return -1;
			}
		}
		else
		{
			NxDbgMsg(NX_DBG_ERR, "=== Failed READ_REVISION ==\n");
			return -1;
		}
	}
	NxDbgMsg(NX_DBG_INFO, "=== Succeed READ_REVISION ==\n");

	return 0;
}

static int TestReadFeature( BTC08_HANDLE handle )
{
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	// Fixed value: 0xB5B, FPGA/ASIC: 0x00/0x05, 0x00, Hash Depth: 0x88
	uint8_t fixed_feature[4] = {0xB5, 0xB0, 0x00, 0x88};

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	handle->numChips = Btc08AutoAddress(handle);

	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		if (0 == Btc08ReadFeature(handle, chipId, res, res_size))
		{
			NxDbgMsg(NX_DBG_INFO, "=== read feature(0x%02x 0x%02x 0x%02x 0x%02x) ==\n",
						res[0], res[1], res[2], res[3]);

			if (0 != memcmp(fixed_feature, res, 4))
			{
				NxDbgMsg(NX_DBG_ERR, "=== Not matched feature ==\n");
				HexDump("READ_FEATURE", res, 4);
				HexDump("fixed_feature", fixed_feature, 4);
				return -1;
			}
		}
		else
		{
			NxDbgMsg(NX_DBG_ERR, "=== Failed READ_FEATURE due to spi err ==\n");
			return -1;
		}
	}
	NxDbgMsg(NX_DBG_INFO, "=== Succeed READ_FEATURE ==\n");

	return 0;
}

static int TestIOCtrl( BTC08_HANDLE handle )
{
	uint8_t res[16] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	// default: 0x00000000_000e0000_00008000_ffffffff
	// wr     : 0x00000000_0015ffff_ffff0000_00000000
	uint8_t default_ioctrl[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0xff, 0xff, 0xff, 0xff};
	uint8_t wr_ioctrl[16]      = {0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	handle->numChips = Btc08AutoAddress(handle);

	// READ_IO_CTRL to read default value
	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		if (0 == Btc08ReadIOCtrl(handle, chipId, res, res_size))
		{
			if (0 != memcmp(default_ioctrl, res, 16))
			{
				NxDbgMsg(NX_DBG_ERR, "=== Test1 Failed: READ_IO_CTRL(chip#%d) ==\n", chipId);
				HexDump("READ_IOCTRL", res, 16);
				HexDump("default_ioctrl", default_ioctrl, 16);
				return -1;
			}
		}
		else
		{
			NxDbgMsg(NX_DBG_ERR, "=== Test1 Failed: READ_IO_CTRL with spi err ==\n");
			return -1;
		}
	}

	// WRITE_IO_CTRL
	if (Btc08WriteIOCtrl(handle, BCAST_CHIP_ID, wr_ioctrl) < 0)
	{
		NxDbgMsg(NX_DBG_ERR, "=== Test Failed: WRITE_IO_CTRL ==\n");
		return -1;
	}

	// READ_IO_CTRL to read the changed value
	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		if (0 == Btc08ReadIOCtrl(handle, chipId, res, res_size))
		{
			if (0 != memcmp(wr_ioctrl, res, 16))
			{
				NxDbgMsg(NX_DBG_ERR, "=== Test2 Failed: READ_IO_CTRL(chip#%d) ==\n", chipId);
				HexDump("READ_IOCTRL", res, 16);
				HexDump("wr_ioctrl", wr_ioctrl, 16);
				return -1;
			}
		}
		else
		{
			NxDbgMsg(NX_DBG_ERR, "=== Test2 Failed: READ_IO_CTRL with spi err ==\n");
			return -1;
		}
	}

	NxDbgMsg(NX_DBG_INFO, "=== Test Succeed: READ/WRITE_IOCTRL ==\n");

	return 0;
}

// tb.1core.vinc
void Test1Core( BTC08_HANDLE handle, uint8_t disable_core_num )
{
	uint8_t *ret;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	uint8_t disable_cores[32] = {0x00,};
	uint8_t last_chip_id = 1;

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	//----------------------------------------------------------------------
	//  CMD_READ_REVISION  ==> Check chipId!
	//----------------------------------------------------------------------
	handle->numChips = 1;

	Btc08ReadRevision(handle, BCAST_CHIP_ID, res, res_size);
	NxDbgMsg(NX_DBG_INFO, "[READ_REVISION(BR)] (year:%02x, month:%02x, day:%02x, index:%02x)\n",
			res[0], res[1], res[2], res[3]);
	//----------------------------------------------------------------------
	//  CMD_READ_FEATURE  ==> Check chipId!
	//----------------------------------------------------------------------
	Btc08ReadFeature(handle, BCAST_CHIP_ID, res, res_size);
	NxDbgMsg(NX_DBG_INFO, "[READ_FEATURE] (0x%02x 0x%02x 0x%02x 0x%02x)\n",
			res[0], res[1], res[2], res[3]);

	//----------------------------------------------------------------------
	//  Auto Address
	//----------------------------------------------------------------------
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "[AUTO_ADDRESS] NumChips = %d\n", handle->numChips);
	ReadId(handle);

	//----------------------------------------------------------------------
	//  PLL
	//----------------------------------------------------------------------
	NxDbgMsg(NX_DBG_INFO, "pll_freq:100\n");
	if (0 > SetPllFreq(handle, 300))
		return;

	Btc08SetControl(handle, BCAST_CHIP_ID, 24/*0x0000_0018*/);

	//----------------------------------------------------------------------
	//  WRITE PARAM for BIST
	//----------------------------------------------------------------------
	NxDbgMsg( NX_DBG_INFO, "[BIST] BIST\n");
	for (int chipId=1; chipId <= handle->numChips; chipId++)
	{
		// Write param for BIST
		Btc08WriteParam (handle, chipId, default_golden_midstate, default_golden_data);
		Btc08WriteNonce (handle, chipId, golden_nonce, golden_nonce);
		Btc08WriteTarget(handle, chipId, default_golden_target);
	}

	//----------------------------------------------------------------------
	//  RUN BIST
	//----------------------------------------------------------------------
	// Disable cores
	memset(disable_cores, 0xff, 32);
	if( disable_core_num > 0 )
		disable_cores[31] &= ~(1);
	for (int i=1; i<(30-disable_core_num); i++) {
		disable_cores[31-(i/8)] &= ~(1 << (i % 8));
	}
	HexDump("[disable_cores]", disable_cores, 32);
	NxDbgMsg( NX_DBG_INFO, "[SET_DISABLE] Disable %d cores\n", disable_core_num);
	// Disable the specific core in the chip
	for (int chipId=1; chipId <= handle->numChips; chipId++)
	{
		Btc08SetDisable(handle, chipId, disable_cores);
	}
	// Check chipId!
	for (int chipId=1; chipId <= handle->numChips; chipId++)
	{
		Btc08RunBist(handle, chipId, default_golden_hash, default_golden_hash, default_golden_hash, default_golden_hash);
	}
	ReadBist(handle);

	//----------------------------------------------------------------------
	//  Enable Out of Nonce interrupt
	//----------------------------------------------------------------------
	Btc08SetControl(handle, BCAST_CHIP_ID, 17/*0x0000_0011*/);		// CHECK!!

	//----------------------------------------------------------------------
	//  Run JOB
	//----------------------------------------------------------------------
	Run1Job(handle);
}

void SingleCommandLoop(void)
{
	static char cmdStr[NX_SHELL_MAX_ARG * NX_SHELL_MAX_STR];
	static char cmd[NX_SHELL_MAX_ARG][NX_SHELL_MAX_STR];
	int cmdCnt;

	//	create BTC08 instance into index 0. ( /dev/spidev0.0 )
	BTC08_HANDLE handle = CreateBtc08(0);

	for( ;; )
	{
		singlecommand_command_list();
		printf( "command > " );
		fgets( cmdStr, NX_SHELL_MAX_ARG*NX_SHELL_MAX_STR - 1, stdin );
		cmdCnt = Shell_GetArgument( cmdStr, cmd );

		//----------------------------------------------------------------------
		if( !strcasecmp(cmd[0], "q") )
		{
			break;
		}
		//----------------------------------------------------------------------
		//	Reset H/W
		else if( !strcasecmp(cmd[0], "1") )
		{
			HWReset( handle );
		}
		//----------------------------------------------------------------------
		//	Reset And Auto Address
		else if( !strcasecmp(cmd[0], "2") )
		{
			ResetAutoAddress( handle );
		}
		//----------------------------------------------------------------------
		//	Reset And BistTest
		else if( !strcasecmp(cmd[0], "3") )
		{
			int pll_freq = 300;
			if( cmdCnt > 1 )
			{
				pll_freq = strtol(cmd[1], 0, 10);
			}
			printf("pll_freq = %d\n", pll_freq);
			TestBist( handle, pll_freq );
		}
		//----------------------------------------------------------------------
		//	Set the chip as the last chip
		else if (!strcasecmp(cmd[0], "4") )
		{
			int chipId = 1;
			if( cmdCnt > 1 )
			{
				chipId = strtol(cmd[1], 0, 10);
			}
			printf("chipId = %d\n", chipId);
			TestLastChip( handle, chipId );
		}
		//----------------------------------------------------------------------
		//	Disable core
		else if (!strcasecmp(cmd[0], "5") )
		{
			uint8_t disable_core_num = 0;
			uint32_t pll_freq = 300;
			if( cmdCnt > 1 )
			{
				disable_core_num = strtol(cmd[1], 0, 10);
			}
			if ( cmdCnt > 2 )
			{
				disable_core_num = strtol(cmd[1], 0, 10);
				pll_freq = strtol(cmd[2], 0, 10);
			}
			printf("disable_core_num = %d, pll_freq = %d\n",  disable_core_num, pll_freq);
			TestDisableCore( handle, disable_core_num, pll_freq );
		}
		//----------------------------------------------------------------------
		//	Write/Read Target Test
		else if (!strcasecmp(cmd[0], "6") )
		{
			TestWRTarget(handle);
		}
		//----------------------------------------------------------------------
		//	Set Disable core
		else if (!strcasecmp(cmd[0], "7") )
		{
			TestWRDisable(handle);
		}
		//----------------------------------------------------------------------
		//	Read Revision Test
		else if ( !strcasecmp(cmd[0], "8") )
		{
			TestReadRevision(handle);
		}
		//----------------------------------------------------------------------
		//	Read Feature Test
		else if ( !strcasecmp(cmd[0], "9") )
		{
			TestReadFeature(handle);
		}
		//----------------------------------------------------------------------
		//	Read/Write IO Ctrl Test
		else if ( !strcasecmp(cmd[0], "10") )
		{
			TestIOCtrl(handle);
		}
		//----------------------------------------------------------------------
		//	1 Core Test
		else if ( !strcasecmp(cmd[0], "11") )
		{
			uint8_t disable_core_num = 0;
			if( cmdCnt > 1 )
			{
				disable_core_num = strtol(cmd[1], 0, 10);
			}
			Test1Core(handle, disable_core_num);
		}
	}

	DestroyBtc08( handle );
}
