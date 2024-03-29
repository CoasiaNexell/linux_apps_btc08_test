#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "Utils.h"
#include "Btc08.h"
#include "TempCtrl.h"
#include "AutoTest.h"

#include "TestVector.h"
#include <sched.h>

#ifdef NX_DTAG
#undef NX_DTAG
#endif
//#define NX_DTAG "[SingleCommand]"
#define NX_DTAG ""
#include "NX_DbgMsg.h"

/* Two ways not to give job to chip1
 * BR_JOB_CMD_RESET:
 *    Give a job with BR and do CMD_RESET(chipId=1) once for each RUN_JOB
 * CHIPID_JOB:
 *    Give each chip one job except chip1
*/
#define BR_JOB_CMD_RESET     0
#define CHIPID_JOB           0

void TestReadDisable( BTC08_HANDLE handle );
unsigned int gDisableCore = 0xffffffff;

static pthread_t thread_workloop;

static void singlecommand_command_list()
{
	printf("\n\n");
	printf("====== Single Command =======\n");
	printf("  1. H/W Reset\n");
	printf("  2. Reset and Auto Address \n");
	printf("  3. Reset and TestBist\n");
	printf("    ex > 3 [disable_core_num] [freq] [wait_gpio] [delay_ms] (default: 3 0 24 0 200)\n");
	printf("  4. Set the chip to the last chip\n");
	printf("    ex > 4 [chipId]\n");
	printf("  5. Disable core\n");
	printf("    ex > 5 [disable_core_num] [freq] [isfullnonce] [fault_chip_id] [isinfinitemining] (default: 5 0 24 0 0 0)\n");
	printf("  6. Write, Read Target\n");
	printf("  7. Set, Read disable\n");
	printf("  8. Read Revision\n");
	printf("  9. Read Feature\n");
	printf("  10. Read/Write IO Ctrl\n");
	printf("  11. 1 Core Test\n");
	printf("    ex > 11 29 (fixed frequency : 300)\n");
	printf("  12. Read Id\n");
	printf("  13. Read Bist\n");
	printf("  14. Set PLL config\n");
	printf("    ex > 14 400 (default : 300)\n");
	printf("  15. Read PLL config\n");
	printf("  16. Read Job Id\n");
	printf("  17. Read Disable\n");
	printf("  18. Read Debug Cnt\n");
	printf("  19. Set Reset GPIO\n");
	printf("    ex > 19 [0/1] (default:0) \n");
	printf("  20. Read Voltage and Temperature\n");
	printf("-----------------------------\n");
	printf("  q. quit\n");
	printf("=============================\n");
}

static void singlecommand_command_list2()
{
	printf("\n");
	printf("========== Frequency Test Command ==========\n");
	printf("  start           : Infinity Work Loop Start\n");
	printf("  stop            : Work Loop Stop\n");
	printf("  freq [pll_freq] : Set PLL Frequency\n");
	printf("--------------------------------------------\n");
	printf("  q               : quit\n");
	printf("============================================\n");
	printf("cmd > ");
}

static void singlecommand_command_list3()
{
	printf("\n");
	printf("============= Enable Chip Test Command =============\n");
	printf("  start <chipId> <idx_data>   : Start\n");
	printf("  start2 <delay>              : Sequential Start[us]\n");
	printf("  enable <chipId>             : Enable chip\n");
	printf("  disable <chipId>            : Disable chip\n");
	printf("  stop                        : quit\n");
	printf("----------------------------------------------------\n");
	printf("  ex) start 1 4\n");
	printf("  ex) start2 10\n");
	printf("  ex) enable 3\n");
	printf("  ex) disable 1\n");
	printf("====================================================\n");
	printf("cmd > ");
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
	int num_asic_cores = 0;

	if (handle->isAsicBoost)
		num_asic_cores = 4;
	else
		num_asic_cores = 1;

	// Sequence 1. Read Hash
	Btc08ReadHash(handle, chipId, hash, hash_size);
	for (int i=0; i<num_asic_cores; i++)
	{
		result = memcmp(default_golden_hash, &(hash[i*32]), 32);
		if (result == 0)
		{
			NxDbgMsg(NX_DBG_DEBUG, "Matched Hash of Inst_%s!!!\n",
					(i==0) ? "Upper":(((i==1) ? "Lower": ((i==2) ? "Lower_2":"Lower_3"))));
		}
		else
		{
			NxDbgMsg(NX_DBG_ERR, "Failed: Not matched Hash of Inst_%s!!!\n",
					(i==0) ? "Upper":(((i==1) ? "Lower": ((i==2) ? "Lower_2":"Lower_3"))));
			sprintf(title, "[hash] Inst_%s", (i==0) ? "Upper":(((i==1) ? "Lower": ((i==2) ? "Lower_2":"Lower_3"))));
			HexDump2(title, &(hash[i*32]), 32);
			HexDump2("[golden hash]", default_golden_hash, 32);
			ret = -1;
		}
	}

	// Sequence 2. Read Result to read GN and clear GN IRQ
	Btc08ReadResult(handle, chipId, res, res_size);
	for (int i=0; i<num_asic_cores; i++)
	{
		result = memcmp(golden_nonce, res + i*4, 4);
		if (result == 0)
		{
			NxDbgMsg(NX_DBG_DEBUG, "Matched GN [Inst_%s] %10s GN = %02x %02x %02x %02x \n",
				(i==0) ? "Upper":(((i==1) ? "Lower": ((i==2) ? "Lower_2":"Lower_3"))), "",
				*(res + i*4), *(res + i*4 + 1), *(res + i*4 + 2), *(res + i*4 + 3));
		}
		else
		{
			NxDbgMsg(NX_DBG_ERR, "Failed: Not matched GN [Inst_%s] %10s GN = %02x %02x %02x %02x \n",
				(i==0) ? "Upper":(((i==1) ? "Lower": ((i==2) ? "Lower_2":"Lower_3"))), "",
				*(res + i*4), *(res + i*4 + 1), *(res + i*4 + 2), *(res + i*4 + 3));
			ret = -1;
		}
	}
	return ret;
}

static int RunJob(BTC08_HANDLE handle, uint8_t is_full_nonce, uint8_t is_infinite_mining)
{
	int chipId = 0x00, jobId = 0x01;
	uint8_t oon_irq = 0x00, gn_irq = 0x00, oon_job_id = 0x00, gn_job_id = 0x00;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	struct timespec ts_start, ts_oon, ts_gn, ts_diff, ts_last_oon;
	bool	ishashdone = false;
	uint64_t startTime, currTime, deltaTime, prevTime;
	uint64_t totalProcessedHash = 0;
	double totalTime;
	double megaHash;
	int numWorks = 4;
	int gn_cnt = 0, oon_cnt = 0;
	uint8_t start_nonce[4], end_nonce[4];

	handle->isAsicBoost = true;
	if (is_full_nonce) {
		memcpy(start_nonce, start_full_nonce, sizeof(start_nonce)/sizeof(start_nonce[0]));
		memcpy(  end_nonce,   end_full_nonce,    sizeof(end_nonce)/sizeof(end_nonce[0]));
	} else {
		memcpy(start_nonce, start_small_nonce, sizeof(start_nonce)/sizeof(start_nonce[0]));
		memcpy(  end_nonce,   end_small_nonce,    sizeof(end_nonce)/sizeof(end_nonce[0]));
	}

	tstimer_time(&ts_start);
	NxDbgMsg( NX_DBG_INFO, "[RUN JOB] [%ld.%lds]\n", ts_start.tv_sec, ts_start.tv_nsec);

	Btc08WriteParam(handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
#if 0
	DistributionNonce(handle, start_nonce, end_nonce);
	for (int i=0; i<handle->numChips; i++)
	{
		if (1 <= handle->fault_chip_id)
		{
			if (i == 0)
				continue;
		}
		NxDbgMsg(NX_DBG_INFO, "Chip[%d:%d] : %02x%02x%02x%02x ~ %02x%02x%02x%02x\n", i, handle->numCores[i],
			handle->startNonce[i][0], handle->startNonce[i][1], handle->startNonce[i][2], handle->startNonce[i][3],
			handle->endNonce[i][0], handle->endNonce[i][1], handle->endNonce[i][2], handle->endNonce[i][3]);
		Btc08WriteNonce(handle, i+1, handle->startNonce[i], handle->endNonce[i]);
	}
#else
	// temporary changes: Write same nonce range to all of the chips to check if each chip works well.
	for (int i=0; i<handle->numChips; i++)
	{
		Btc08WriteNonce(handle, i+1, start_nonce, end_nonce);
	}
#endif
	for (int i=0; i<numWorks; i++)
	{
		NxDbgMsg(NX_DBG_INFO, "%2s Run Job with jobId#%d\n", "", jobId);
#if BR_JOB_CMD_RESET
		Btc08RunJob(handle, BCAST_CHIP_ID, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), jobId++);
		if (0 != handle->fault_chip_id) {
			Btc08Reset(handle, 1);
		}
#elif CHIPID_JOB
		for (int chipId=1; chipId <= handle->numChips; chipId++)
		{
			if (1 <= handle->fault_chip_id)
			{
				if (chipId == 1)
					continue;
			}

			//NxDbgMsg(NX_DBG_INFO, "%2s Run Job with jobId#%d on chip#%d\n", "", jobId, chipId);
			Btc08RunJob(handle, chipId, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), jobId);
		}
		jobId++;
#else
		Btc08RunJob(handle, BCAST_CHIP_ID, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), jobId++);
#endif
		ReadId(handle);
	}
	DbgGpioOn();
	startTime = get_current_ms();
	prevTime = startTime;

	while(!ishashdone)
	{
		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_GN))	// Check GN GPIO pin
		{
			Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
			gn_job_id  = res[1];
			gn_irq 	   = res[2] & (1<<0);
			chipId     = res[3];

			if (0 != gn_irq) {		// If GN IRQ is set, then handle GN
				gn_cnt++;
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

		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_OON))	// Check OON
		{
			if (0 == is_infinite_mining)		// Test 4 jobs
			{
				Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
				oon_job_id = res[0];
				oon_irq	  = res[2] & (1<<1);
				chipId    = res[3];

				if (0 != oon_irq)				// If OON IRQ is set, handle OON
				{
					NxDbgMsg(NX_DBG_INFO, "*** OON IRQ on chip#%d for oon_job_id:%d!!! ***\n", chipId, oon_job_id);
					handleOON(handle, BCAST_CHIP_ID);
					oon_cnt++;

					if (is_full_nonce) {
						if (handle->isAsicBoost)
							totalProcessedHash += (0x100000000 * 4);	//	0x100000000 * 4 (asic booster)
						else
							totalProcessedHash += 0x100000000;			//	0x100000000 * 4
					} else {
						if (handle->isAsicBoost)
							totalProcessedHash += (0x11000000 * 4);		//	0x100000000 * 4 (asic booster)
						else
							totalProcessedHash += 0x11000000;			//	0x100000000 * 4
					}

					currTime = get_current_ms();
					totalTime = currTime - startTime;
					megaHash = totalProcessedHash / (1000*1000);

					NxDbgMsg(NX_DBG_INFO, "AVG : %.2f MHash/s,  Hash = %.2f GH, Time = %.2f sec, delta = %lld msec\n",
							megaHash * 1000. / totalTime, megaHash/1000, totalTime/1000. , currTime - prevTime );

					prevTime = currTime;
					if (oon_cnt == numWorks)
						ishashdone = true;
				}			// end of if (0 != oon_irq)
				else		// OON IRQ is not set (cgminer: check OON timeout is expired)
				{
					//NxDbgMsg(NX_DBG_INFO, "%5s === OON IRQ is not set! ===\n", "");
				}
			}
			else		// Test infinite jobs
			{
				NxDbgMsg(NX_DBG_INFO, "*** OON IRQ on chip#%d for oon_job_id:%d!!! ***\n", chipId, oon_job_id);
				handleOON(handle, BCAST_CHIP_ID);
				oon_cnt++;

				if (is_full_nonce) {
					if (handle->isAsicBoost)
						totalProcessedHash += (0x100000000 * 4);	//	0x100000000 * 4 (asic booster)
					else
						totalProcessedHash += 0x100000000;			//	0x100000000 * 4
				} else {
					if (handle->isAsicBoost)
						totalProcessedHash += (0x11000000 * 4);		//	0x100000000 * 4 (asic booster)
					else
						totalProcessedHash += 0x11000000;			//	0x100000000 * 4
				}

				currTime = get_current_ms();
				totalTime = currTime - startTime;
				megaHash = totalProcessedHash / (1000*1000);

				NxDbgMsg(NX_DBG_INFO, "AVG : %.2f MHash/s,  Hash = %.2f GH, Time = %.2f sec, delta = %lld msec\n",
						megaHash * 1000. / totalTime, megaHash/1000, totalTime/1000. , currTime - prevTime );

				prevTime = currTime;
				for (int i=0; i<2; i++)
				{
					NxDbgMsg(NX_DBG_INFO, "%2s Run Job with jobId#%d\n", "", jobId);
#if BR_JOB_CMD_RESET
					Btc08RunJob(handle, BCAST_CHIP_ID, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), jobId++);
					if (0 != handle->fault_chip_id) {
						Btc08Reset(handle, 1);
					}
#elif CHIPID_JOB
					for (int chipId=1; chipId <= handle->numChips; chipId++)
					{
						if (1 <= handle->fault_chip_id)
						{
							if (1 == chipId)
								continue;
						}

						//NxDbgMsg(NX_DBG_INFO, "%2s Run Job with jobId#%d on chip#%d\n", "", jobId, chipId);
						Btc08RunJob(handle, chipId, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), jobId);
					}
					jobId++;
#else
					Btc08RunJob(handle, BCAST_CHIP_ID, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), jobId++);
#endif
					if (jobId == 256)
						jobId = 0;
					ReadId(handle);
				}
			}
		}		// end of if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_OON))
		sched_yield();
	}
	DbgGpioOff();

	if (0 == is_infinite_mining)
	{
		if ((oon_cnt != numWorks) || (gn_cnt != (numWorks * handle->numChips)))
		{
			NxDbgMsg(NX_DBG_INFO, "=== Test Failed!!!\n");
			return -1;
		}
		else
		{
			NxDbgMsg(NX_DBG_INFO, "=== Test Succeed!!!\n");
			return 0;
		}
	} else
		return 0;
}

static void Run1Job(BTC08_HANDLE handle)
{
	int chipId = 0x00, jobId=0x01;
	uint8_t fifo_full = 0x00, oon_irq = 0x00, gn_irq = 0x00, oon_job_id = 0x00, gn_job_id = 0x00;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	struct timespec ts_start, ts_oon, ts_gn, ts_diff, ts_last_oon;
	uint64_t jobcnt = 0;
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
void TestLastChip( BTC08_HANDLE handle, uint8_t fault_chip_id, uint32_t pll_freq )
{
	uint8_t *ret;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	uint8_t disable_cores[32] = {0x00,};
	BOARD_TYPE type = BOARD_TYPE_ASIC;

	handle->fault_chip_id = fault_chip_id;

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	// seq1. AUTO_ADDRESS > READ_ID
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "[AUTO_ADDRESS] NumChips = %d\n", handle->numChips);
	ReadId(handle);

	// seq2. Set PLL
	type = get_board_type(handle);
	if (type == BOARD_TYPE_ASIC)
	{
		NxDbgMsg(NX_DBG_INFO, "[SET_PLL] Set PLL %d\n", pll_freq);
		if (0 > SetPllFreq(handle, pll_freq))
			return;
	}

	// seq4. WRITE_PARAM, WRITE_NONCE, WRITE_TARGET
	NxDbgMsg(NX_DBG_INFO, "[WRITE_PARAM, WRITE_NONCE, WRITE_TARGET]\n");
	Btc08WriteParam (handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	Btc08WriteNonce (handle, BCAST_CHIP_ID, golden_nonce, golden_nonce);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);

	// seq5. SET_DISABLE
	memset(disable_cores, 0x00, 32);
	Btc08SetDisable (handle, BCAST_CHIP_ID, disable_cores);

	// seq6. RUN_BIST > READ_BIST
	NxDbgMsg(NX_DBG_INFO, "[RUN_BIST]\n");
	Btc08RunBist(handle, BCAST_CHIP_ID, default_golden_hash, default_golden_hash,
				default_golden_hash, default_golden_hash);
	ReadBist(handle);

	// seq7. Enable out of nonce interrupt
	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		if (chipId == 1)
		{
			NxDbgMsg(NX_DBG_INFO, "[SET_CONTROL] set last chip(chip#1)\n");
			Btc08SetControl(handle, 1, LAST_CHIP | OON_IRQ_EN | UART_DIVIDER);
			handle->numChips = Btc08AutoAddress(handle);
			NxDbgMsg(NX_DBG_INFO, "[AUTO_ADDRESS] NumChips = %d\n", handle->numChips);
			ReadId(handle);
		}
		else
			Btc08SetControl(handle, chipId, OON_IRQ_EN | UART_DIVIDER);
	}
	//Btc08SetControl(handle, BCAST_CHIP_ID, OON_IRQ_EN | UART_DIVIDER);

	// seq8. Last chip test
	if (1 <= handle->fault_chip_id)
	{
		for (int i = 0; i < (handle->fault_chip_id - 1); i++)
		{
			NxDbgMsg(NX_DBG_INFO, "[SET_CONTROL] set last chip(chip#2)\n");
			Btc08SetControl(handle, 2, (LAST_CHIP | OON_IRQ_EN | UART_DIVIDER));
			handle->numChips = Btc08AutoAddress(handle);
			NxDbgMsg(NX_DBG_INFO, "[AUTO_ADDRESS] NumChips = %d\n", handle->numChips);
			ReadId(handle);
		}
		NxDbgMsg(NX_DBG_INFO, "[PLL_FOUT_EN] PLL_DISABLE chip#1\n");
		Btc08SetPllFoutEn(handle, 1, FOUT_EN_DISABLE);
	}

	// seq6. RUN_BIST > READ_BIST
	NxDbgMsg(NX_DBG_INFO, "[RUN_BIST]\n");
	Btc08RunBist(handle, BCAST_CHIP_ID, default_golden_hash, default_golden_hash,
				default_golden_hash, default_golden_hash);
	ReadBist(handle);

	// seq7. WRITE_PARAM, WRITE_NONCE > RUN_JOB
	RunJob(handle, 0, 0);
}

/* Disable core
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
int TestDisableCore( BTC08_HANDLE handle, uint8_t disable_core_num,
		uint32_t pll_freq, uint8_t is_full_nonce, uint8_t fault_chip_id, uint8_t is_infinite_mining )
{
	uint8_t *ret;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	uint8_t disable_cores[32] = {0x00,};
	BOARD_TYPE type = BOARD_TYPE_ASIC;

	// Disable cores
	if( gDisableCore != 0xffffffff )
	{
		//	Use user disable makse
		memset(disable_cores, 0xff, 32);
		disable_cores[28] = (gDisableCore >> 24) & 0xFF;
		disable_cores[29] = (gDisableCore >> 16) & 0xFF;
		disable_cores[30] = (gDisableCore >>  8) & 0xFF;
		disable_cores[31] = (gDisableCore >>  0) & 0xFF;
	}
	else
	{
		if( disable_core_num > 0 )
		{
			memset(disable_cores, 0xff, 32);
			disable_cores[31] &= ~(1);
			for (int i=1; i<(BTC08_NUM_CORES-disable_core_num); i++) {
				disable_cores[31-(i/8)] &= ~(1 << (i % 8));
			}
		}
		else
		{
			memset(disable_cores, 0x00, 32);
		}
	}

	HexDump("[disable_cores]", disable_cores, 32);

	handle->fault_chip_id = fault_chip_id;

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	DbgGpioOff();

	// seq1. AUTO_ADDRESS > READ_ID
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "[AUTO_ADDRESS] NumChips = %d\n", handle->numChips);
	ReadId(handle);

	// seq2. Set PLL
	type = get_board_type(handle);
	if (type == BOARD_TYPE_ASIC)
	{
		NxDbgMsg(NX_DBG_INFO, "[SET_PLL] Set PLL %d\n", pll_freq);
		if (0 > SetPllFreq(handle, pll_freq))
			return -3;
	}

	// seq4. WRITE_PARAM, WRITE_NONCE, WRITE_TARGET
	NxDbgMsg(NX_DBG_INFO, "[WRITE_PARAM, WRITE_NONCE, WRITE_TARGET]\n");
	Btc08WriteParam (handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	Btc08WriteNonce (handle, BCAST_CHIP_ID, golden_nonce, golden_nonce);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);

	// seq5. SET_DISABLE
	if( gDisableCore != 0xffffffff )
		NxDbgMsg( NX_DBG_INFO, "[SET_DISABLE] Disable cores : 0x%08x\n", gDisableCore);
	else
		NxDbgMsg( NX_DBG_INFO, "[SET_DISABLE] Disable %d cores\n", disable_core_num);
	for (int chipId=1; chipId <= handle->numChips; chipId++) {
		Btc08SetDisable (handle, chipId, disable_cores);
	}

	// seq6. RUN_BIST > READ_BIST
	NxDbgMsg(NX_DBG_INFO, "[RUN_BIST]\n");
	DbgGpioOn();
	Btc08RunBist(handle, BCAST_CHIP_ID, default_golden_hash, default_golden_hash,
				default_golden_hash, default_golden_hash);
	DbgGpioOff();
	ReadBist(handle);
	Btc08SetControl(handle, BCAST_CHIP_ID, (OON_IRQ_EN | UART_DIVIDER));

	// seq7. Last chip test
	if (1 <= handle->fault_chip_id)
	{
		for (int i = 0; i < (handle->fault_chip_id - 1); i++)
		{
			NxDbgMsg(NX_DBG_INFO, "[SET_CONTROL] set last chip(chip#2)\n");
			Btc08SetControl(handle, 2, (LAST_CHIP | OON_IRQ_EN | UART_DIVIDER));
			handle->numChips = Btc08AutoAddress(handle);
			NxDbgMsg(NX_DBG_INFO, "[AUTO_ADDRESS] NumChips = %d\n", handle->numChips);
			ReadId(handle);
		}
		NxDbgMsg(NX_DBG_INFO, "[PLL_FOUT_EN] PLL_DISABLE chip#1\n");
		Btc08SetPllFoutEn(handle, 1, FOUT_EN_DISABLE);
	}

	Btc08SetControl(handle, 1, (LAST_CHIP | OON_IRQ_EN | UART_DIVIDER));

	for( int i=0 ; i<handle->numChips ; i++ )
	{
		if( handle->numCores[i] != (BTC08_NUM_CORES - disable_core_num) )
		{
			NxDbgMsg(NX_DBG_ERR, "BIST failed!!! handle->numCores[%d] = %d, expeded core = %d \n",
				i, handle->numCores[i], (BTC08_NUM_CORES - disable_core_num));
			return -2;
		}
	}

	// seq8. WRITE_PARAM, WRITE_NONCE > RUN_JOB
	DbgGpioOn();
	return RunJob(handle, is_full_nonce, is_infinite_mining);
}

int TestMiningWithoutBist( BTC08_HANDLE handle, uint8_t disable_core_num,
		uint32_t pll_freq, uint8_t is_full_nonce, uint8_t fault_chip_id, uint8_t is_infinite_mining )
{
	uint8_t *ret;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	uint8_t disable_cores[32] = {0x00,};
	BOARD_TYPE type = BOARD_TYPE_ASIC;

	// Disable cores
	if( gDisableCore != 0xffffffff )
	{
		//	Use user disable makse
		memset(disable_cores, 0xff, 32);
		disable_cores[28] = (gDisableCore >> 24) & 0xFF;
		disable_cores[29] = (gDisableCore >> 16) & 0xFF;
		disable_cores[30] = (gDisableCore >>  8) & 0xFF;
		disable_cores[31] = (gDisableCore >>  0) & 0xFF;
	}
	else
	{
		if( disable_core_num > 0 )
		{
			memset(disable_cores, 0xff, 32);
			disable_cores[31] &= ~(1);
			for (int i=1; i<(BTC08_NUM_CORES-disable_core_num); i++) {
				disable_cores[31-(i/8)] &= ~(1 << (i % 8));
			}
		}
		else
		{
			memset(disable_cores, 0x00, 32);
		}
	}

	HexDump("[disable_cores]", disable_cores, 32);

	handle->fault_chip_id = fault_chip_id;

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );
	DbgGpioOff();

	// seq1. AUTO_ADDRESS > READ_ID
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "[AUTO_ADDRESS] NumChips = %d\n", handle->numChips);
	ReadId(handle);

	// seq2. Set PLL
	type = get_board_type(handle);
	if (type == BOARD_TYPE_ASIC)
	{
		NxDbgMsg(NX_DBG_INFO, "[SET_PLL] Set PLL %d\n", pll_freq);
		if (0 > SetPllFreq(handle, pll_freq))
			return -3;
	}

	// seq3. SET_DISABLE
	if( gDisableCore != 0xffffffff )
		NxDbgMsg( NX_DBG_INFO, "[SET_DISABLE] Disable cores : 0x%08x\n", gDisableCore);
	else
		NxDbgMsg( NX_DBG_INFO, "[SET_DISABLE] Disable %d cores\n", disable_core_num);
	for (int chipId=1; chipId <= handle->numChips; chipId++) {
		Btc08SetDisable (handle, chipId, disable_cores);
	}

	// seq4. No BIST & Set OON_IRQ_EN and UART_DIVIDER
	DbgGpioOn();
	for (int chipId = 1; chipId <= handle->numChips; chipId++) {
		Btc08WriteCoreCfg(handle, chipId, (BTC08_NUM_CORES - disable_core_num));
	}
	Btc08SetControl(handle, BCAST_CHIP_ID, (OON_IRQ_EN | UART_DIVIDER));		// SoC example: 32'h0000_0011

	if (1 <= handle->fault_chip_id)
	{
		for (int i = 0; i < (handle->fault_chip_id - 1); i++)
		{
			NxDbgMsg(NX_DBG_INFO, "[SET_CONTROL] set last chip(chip#2)\n");
			Btc08SetControl(handle, 2, (LAST_CHIP | OON_IRQ_EN | UART_DIVIDER));
			handle->numChips = Btc08AutoAddress(handle);
			NxDbgMsg(NX_DBG_INFO, "[AUTO_ADDRESS] NumChips = %d\n", handle->numChips);
			ReadId(handle);
		}
		NxDbgMsg(NX_DBG_INFO, "[PLL_FOUT_EN] PLL_DISABLE chip#1\n");
		Btc08SetPllFoutEn(handle, 1, FOUT_EN_DISABLE);
	}

	Btc08SetControl(handle, 1, (LAST_CHIP | OON_IRQ_EN | UART_DIVIDER));

	// seq3. SET_DISABLE
	if( gDisableCore != 0xffffffff )
		NxDbgMsg( NX_DBG_INFO, "[SET_DISABLE] Disable cores : 0x%08x\n", gDisableCore);
	else
		NxDbgMsg( NX_DBG_INFO, "[SET_DISABLE] Disable %d cores\n", disable_core_num);
	for (int chipId=1; chipId <= handle->numChips; chipId++) {
		Btc08SetDisable (handle, chipId, disable_cores);
	}

	// seq5. WRITE_PARAM, WRITE_NONCE > RUN_JOB
	DbgGpioOff();
	return RunJob(handle, is_full_nonce, is_infinite_mining);
}

#ifdef __cplusplus
extern "C" {
#endif
void DbgGpioOn();
void DbgGpioOff();
#ifdef __cplusplus
};
#endif


void TestBist( BTC08_HANDLE handle, uint8_t disable_core_num, int pll_freq, int wait_gpio, int delay )
{
	uint8_t *ret;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	BOARD_TYPE type = BOARD_TYPE_ASIC;
	uint8_t disable_cores[32] = {0x00,};

	if (wait_gpio)
	{
		NxDbgMsg( NX_DBG_INFO, "Wait until KEY0 is pressed\n");
		while(true)
		{
			if (0 == GpioGetValue(handle->hKey0))
			{
				NxDbgMsg( NX_DBG_INFO, "KEY0 is pressed!!!\n");
				break;
			}
			else
				continue;
		}
	}

	// Disable cores
	if( gDisableCore != 0xffffffff )
	{
		//	Use user disable makse
		memset(disable_cores, 0xff, 32);
		disable_cores[28] = (gDisableCore >> 24) & 0xFF;
		disable_cores[29] = (gDisableCore >> 16) & 0xFF;
		disable_cores[30] = (gDisableCore >>  8) & 0xFF;
		disable_cores[31] = (gDisableCore >>  0) & 0xFF;
	}
	else
	{
		if( disable_core_num > 0 )
		{
			memset(disable_cores, 0xff, 32);
			disable_cores[31] &= ~(1);
			for (int i=1; i<(BTC08_NUM_CORES-disable_core_num); i++) {
				disable_cores[31-(i/8)] &= ~(1 << (i % 8));
			}
		}
		else
		{
			memset(disable_cores, 0x00, 32);
		}
	}

	HexDump("[disable_cores]", disable_cores, 32);

	GpioSetValue( handle->hReset, 0 );
	usleep( HW_RESET_TIME );	//	wait 200ms
	GpioSetValue( handle->hReset, 1 );
	NxDbgMsg( NX_DBG_INFO, "H/W Reset Fisish\n");

	if((delay*1000) < HW_RESET_TIME)
		usleep( HW_RESET_TIME );
	else
		usleep( delay*1000 );

	// AUTO_ADDRESS > READ_ID
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "[AUTO_ADDRESS] NumChips = %d\n", handle->numChips);
	ReadId(handle);

	// Software reset (TODO: Check FIFO status if it's empty)
	NxDbgMsg( NX_DBG_INFO, "[RESET]\n");
	Btc08Reset(handle, BCAST_CHIP_ID);

	if( gDisableCore != 0xffffffff )
		NxDbgMsg( NX_DBG_INFO, "[SET_DISABLE] Disable cores : 0x%08x\n", gDisableCore);
	else
		NxDbgMsg( NX_DBG_INFO, "[SET_DISABLE] Disable %d cores\n", disable_core_num);

	// Enable all cores
	Btc08SetDisable (handle, BCAST_CHIP_ID, disable_cores);

	// seq2. Set PLL
	type = get_board_type(handle);
	if (type == BOARD_TYPE_ASIC)
	{
		NxDbgMsg(NX_DBG_INFO, "[SET_PLL] Set PLL %d\n", pll_freq);
		if (0 > SetPllFreq(handle, pll_freq))
			return;
	}

	NxDbgMsg( NX_DBG_INFO, "[BIST] Run BIST\n");
	// Write param for BIST
	Btc08WriteParam (handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
	Btc08WriteNonce (handle, BCAST_CHIP_ID, golden_nonce, golden_nonce);
	// Run BIST
	DbgGpioOn();
	Btc08RunBist    (handle, BCAST_CHIP_ID, default_golden_hash, default_golden_hash, default_golden_hash, default_golden_hash);
	DbgGpioOff();
	ReadBist(handle);

	TestReadDisable(handle);
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
			NxDbgMsg(NX_DBG_ERR, "[WRITE_TARGET] Failed on chip#%d due to spi err\n", chipId);
			return -1;
		}
	}

	NxDbgMsg(NX_DBG_INFO, "[WRITE_TARGET(BR)]\n");
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
	ReadId(handle);
	Btc08Reset(handle, BCAST_CHIP_ID);

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

			// SET_DISABLE
			Btc08ReadDisable(handle, chipId, res, 32);
			if (0 != memcmp(test_data1[chipId-1], res, 32))
			{
				NxDbgMsg(NX_DBG_ERR, "== Failed READ_DISABLE on chip%d\n", chipId);
				HexDump("set_disable:", test_data1[chipId-1], 32);
				sprintf(title, "chipId(%d) read_disable:", chipId);
				HexDump(title, res, 32);
				return -1;
			}
		}
		else {
			NxDbgMsg(NX_DBG_ERR, "Failed SET_DISABLE(per Chip) due to spi err\n");
			return -1;
		}
	}
	// READ_BIST
	ReadBist(handle);

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
	// READ_BIST
	ReadBist(handle);

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
	NxDbgMsg( NX_DBG_INFO, "[AUTO_ADDRESS] NumChips = %d\n", handle->numChips );
	ReadId(handle);

	Btc08ReadRevision(handle, BCAST_CHIP_ID, res, res_size);
	NxDbgMsg(NX_DBG_INFO, "[READ_REVISION(BR)] (year:%02x, month:%02x, day:%02x, index:%02x)\n",
						res[0], res[1], res[2], res[3]);

	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		if (0 == Btc08ReadRevision(handle, chipId, res, res_size))
		{
			NxDbgMsg(NX_DBG_INFO, "[READ_REVISION(chip%d)] (year:%02x, month:%02x, day:%02x, index:%02x)\n",
						chipId, res[0], res[1], res[2], res[3]);

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
	uint8_t fpga_feature[4] = {0xB5, 0xB0, 0x00, 0x88};
	uint8_t asic_feature[4] = {0xB5, 0xB5, 0x00, 0x88};

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "[AUTO_ADDRESS] NumChips = %d\n", handle->numChips);
	ReadId(handle);

	Btc08ReadFeature(handle, BCAST_CHIP_ID, res, res_size);
	NxDbgMsg(NX_DBG_INFO, "[READ_FEATURE(BR)] (0x%02x 0x%02x 0x%02x 0x%02x) ==\n",
			res[0], res[1], res[2], res[3]);
	if (0 == memcmp(fpga_feature, res, 4))
		NxDbgMsg(NX_DBG_INFO, "== Succeed READ_FEATURE: FPGA\n");
	else if (0 == memcmp(asic_feature, res, 4))
		NxDbgMsg(NX_DBG_INFO, "== Succeed READ_FEATURE: ASIC\n");
	else
		HexDump("Failed READ_FEATURE", res, 4);

	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		if (0 == Btc08ReadFeature(handle, chipId, res, res_size))
		{
			NxDbgMsg(NX_DBG_INFO, "[READ_FEATURE(chip%d)] (0x%02x 0x%02x 0x%02x 0x%02x) ==\n",
						chipId, res[0], res[1], res[2], res[3]);

			if (0 == memcmp(fpga_feature, res, 4))
				NxDbgMsg(NX_DBG_INFO, "== Succeed READ_FEATURE: FPGA\n");
			else if (0 == memcmp(asic_feature, res, 4))
				NxDbgMsg(NX_DBG_INFO, "== Succeed READ_FEATURE: ASIC\n");
			else
			{
				HexDump("Failed READ_FEATURE ", res, 4);
				return -1;
			}
		}
		else
		{
			NxDbgMsg(NX_DBG_ERR, "Failed READ_FEATURE due to spi err ==\n");
			return -1;
		}
	}

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
				NxDbgMsg(NX_DBG_ERR, "[READ_IO_CTRL(chip#%d)] Test1 Failed\n", chipId);
				HexDump("READ_IOCTRL", res, 16);
				HexDump("default_ioctrl", default_ioctrl, 16);
				return -1;
			}
		}
		else
		{
			NxDbgMsg(NX_DBG_ERR, "[READ_IO_CTRL(chip#%d)] Test1 Failed due to spi err ==\n", chipId);
			return -1;
		}
	}

	// WRITE_IO_CTRL
	if (Btc08WriteIOCtrl(handle, BCAST_CHIP_ID, wr_ioctrl) < 0)
	{
		NxDbgMsg(NX_DBG_ERR, "[WRITE_IO_CTRL(BR)] Failed\n");
		return -1;
	}

	// READ_IO_CTRL to read the changed value
	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		if (0 == Btc08ReadIOCtrl(handle, chipId, res, res_size))
		{
			if (0 != memcmp(wr_ioctrl, res, 16))
			{
				NxDbgMsg(NX_DBG_ERR, "[READ_IO_CTRL(chip#%d)] Test2 Failed\n", chipId);
				HexDump("READ_IOCTRL", res, 16);
				HexDump("wr_ioctrl", wr_ioctrl, 16);
				return -1;
			}
		}
		else
		{
			NxDbgMsg(NX_DBG_ERR, "[READ_IO_CTRL(chip#%d)] Test2 Failed due to spi err ==\n", chipId);
			return -1;
		}
	}

	NxDbgMsg(NX_DBG_INFO, "=== Test Succeed: READ/WRITE_IOCTRL ==\n");

	return 0;
}

/* tb.1core.vinc
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
void TestXCore( BTC08_HANDLE handle, uint8_t disable_core_num )
{
	uint8_t *ret;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	uint8_t disable_cores[32] = {0x00,};
	uint8_t last_chip_id = 1;
	BOARD_TYPE type = BOARD_TYPE_ASIC;

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
	type = get_board_type(handle);
	if (type == BOARD_TYPE_ASIC)
	{
		NxDbgMsg(NX_DBG_INFO, "[SET_PLL] Set PLL 300\n");
		if (0 > SetPllFreq(handle, 300))
			return;
	}

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
	if( disable_core_num > 0 )
	{
		memset(disable_cores, 0xff, 32);
		disable_cores[31] &= ~(1);
		for (int i=1; i<(BTC08_NUM_CORES-disable_core_num); i++) {
			disable_cores[31-(i/8)] &= ~(1 << (i % 8));
		}
	}
	else
	{
		memset(disable_cores, 0x00, 32);
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

void TestReadId( BTC08_HANDLE handle )
{
	ReadId(handle);
}

void TestReadPll( BTC08_HANDLE handle )
{
	BOARD_TYPE type = get_board_type(handle);
	if (type == BOARD_TYPE_FPGA)
	{
		NxDbgMsg(NX_DBG_WARN, "Do not set pll on FPGA\n");
		return;
	}

	NxDbgMsg(NX_DBG_INFO, "NumChips:%d\n", handle->numChips);
	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		ReadPllLockStatus(handle, chipId);
	}
}

void TestReadBist( BTC08_HANDLE handle )
{
	ReadBist(handle);
}

void TestSetPll( BTC08_HANDLE handle, int pll_freq )
{
	BOARD_TYPE type;

	// AUTO_ADDRESS > READ_ID
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "[AUTO_ADDRESS] NumChips = %d\n", handle->numChips);
	ReadId(handle);

	type = get_board_type(handle);
	if (type == BOARD_TYPE_ASIC)
		SetPllFreq(handle, pll_freq);
	else
		NxDbgMsg(NX_DBG_WARN, "Do not set pll on FPGA\n");
}

void DumpReadJobId(uint8_t chipId, uint8_t res[4])
{
	uint8_t oon_job_id, gn_job_id;
	uint8_t gn_irq, oon_irq, fifo_full;
	uint8_t chip_id;

	oon_job_id = res[0];
	gn_job_id  = res[1];
	gn_irq 	   = res[2] & (1<<0);
	oon_irq	   = res[2] & (1<<1);
	fifo_full  = res[2] & (1<<2);
	chip_id     = res[3];

	NxDbgMsg(NX_DBG_INFO,
		"[READ_JOB_ID(%d)] oon_job_id(%d) gn_job_id(%d) chipId(%d) %s %s %s\n",
		chipId, oon_job_id, gn_job_id, chip_id,
		(0 != fifo_full) ? "FIFO full":"FIFO not full",
		(0 != oon_irq) ? "OON IRQ":"",
		(0 != gn_irq) ? "GN IRQ":"");
}

void TestReadJobId( BTC08_HANDLE handle )
{
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
	DumpReadJobId(BCAST_CHIP_ID, res);

	for (int i=1; i<=handle->numChips; i++)
	{
		Btc08ReadJobId(handle, i, res, res_size);
		DumpReadJobId(i, res);
	}
}


void TestReadDisable( BTC08_HANDLE handle )
{
	uint8_t res[32] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	for (int chipId=1; chipId <= handle->numChips; chipId++)
	{
		Btc08ReadDisable(handle, chipId, res, res_size);
		HexDump("read_disable", res, res_size);
	}
}

void TestReadDebugCnt( BTC08_HANDLE handle )
{
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	for (int chipId=1; chipId <= handle->numChips; chipId++)
	{
		Btc08ReadDebugCnt(handle, chipId, res, res_size);
	}
}

static int TestResetGpio( BTC08_HANDLE handle, uint8_t val )
{
	if ((val != 0) && (val != 1)) {
		NxDbgMsg(NX_DBG_ERR, "Enter 0/1\n");
	}

	Btc08ResetHW(handle, val);

	NxDbgMsg(NX_DBG_INFO, "Set Reset GPIO (%d)\n", Btc08GpioGetValue(handle, GPIO_TYPE_RESET));
	return 0;
}

static int TestReadVolTemp( BTC08_HANDLE handle )
{
	int adcCh = 1;
	float mv;
	float currentTemp;

	if (NULL == handle)
	{
		NxDbgMsg(NX_DBG_ERR, "handle is NULL\n");
		return -1;
	}

#if USE_BTC08_FPGA
	adcCh = 0;
#else
	if ((plug_status_0 == 1) && (plug_status_1 != 1)) {
		adcCh = 0;
	} else if ((plug_status_0 != 1) && (plug_status_1 == 1)) {
		adcCh = 1;
	}
#endif

	mv = get_mvolt(adcCh);
	currentTemp = get_temp(mv);
	NxDbgMsg(NX_DBG_INFO, "Hash%d: voltage(%.2f mV), temperature(%.3f C)\n", adcCh, mv, currentTemp);

	return 0;
}

/**
 * @brief Different ASIC Data Test
 * @param last_chipId Set last chip ID
 * @param disable_core_num Set disable core number
 * @param is_full_nonce 0(short range), 1(full range)
 * @param is_diff_data 0(different), 1(same)
 * @param pll_freq Set PLL frequency
 */
void TestAsic(uint8_t last_chipId, uint8_t disable_core_num, uint8_t is_full_nonce, uint8_t is_diff_data, int pll_freq)
{
	int chipId = 0x00, jobId=0x01;
	struct timespec ts_start, ts_oon, ts_gn, ts_diff;
	uint8_t fifo_full = 0x00, oon_irq = 0x00, gn_irq = 0x00, oon_job_id = 0x00, gn_job_id =0x00;
	uint8_t res[4] = {0x00,};
	uint8_t disable_cores[32] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	bool	ishashdone = false;
	uint8_t hashdone_allchip[256];
	uint8_t hashdone_chip[256];
	BTC08_HANDLE handle;
	BOARD_TYPE type = BOARD_TYPE_ASIC;
	VECTOR_DATA data;

	memset(hashdone_allchip, 0xFF, sizeof(hashdone_allchip));
	memset(hashdone_chip,    0x00, sizeof(hashdone_chip));

	tstimer_time(&ts_start);
	NxDbgMsg(NX_DBG_INFO, "=== Start of TestWork! === [%ld.%lds]\n", ts_start.tv_sec, ts_start.tv_nsec);

	// Seqeunce 1. Create Handle
#if USE_BTC08_FPGA
	handle = CreateBtc08(0);
#else
	if ((plug_status_0 == 1) && (plug_status_1 != 1))
		handle = CreateBtc08(0);
	else if ((plug_status_0 != 1) && (plug_status_1 == 1))
		handle = CreateBtc08(1);
#endif
	if(!handle)
	{
		NxDbgMsg( NX_DBG_INFO, "Can not Create BTC08 Handler\n");
		return;
	}

	// Sequence 2. Reset HW
	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	// Sequence 3. Set ASIC Boost
	handle->isAsicBoost = true;

	// Seqeunce 4. Find number of chips : using AutoAddress
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_DEBUG, "%5s NumChips = %d\n", "", handle->numChips);
	ReadId(handle);

	// Sequence 5. Reset SW
	Btc08Reset(handle, BCAST_CHIP_ID);

	// Sequence 6. Set last chip
	if(last_chipId > handle->numChips)
	{
		Btc08SetControl(handle, handle->numChips, LAST_CHIP | OON_IRQ_EN | UART_DIVIDER);
	}
	else
	{
		Btc08SetControl(handle, last_chipId, LAST_CHIP | OON_IRQ_EN | UART_DIVIDER);
	}
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "(After last chip) NumChips = %d\n", handle->numChips);
	ReadId(handle);

	// Sequence 7. Set Enable Cores
	if( gDisableCore != 0xffffffff )
	{
		//	Use user disable makse
		memset(disable_cores, 0xff, 32);
		disable_cores[28] = (gDisableCore >> 24) & 0xFF;
		disable_cores[29] = (gDisableCore >> 16) & 0xFF;
		disable_cores[30] = (gDisableCore >>  8) & 0xFF;
		disable_cores[31] = (gDisableCore >>  0) & 0xFF;
	}
	else
	{
		if( disable_core_num > 0 )
		{
			memset(disable_cores, 0xff, 32);
			disable_cores[31] &= ~(1);
			for (int i=1; i<(BTC08_NUM_CORES-disable_core_num); i++) {
				disable_cores[31-(i/8)] &= ~(1 << (i % 8));
			}
		}
		else
		{
			memset(disable_cores, 0x00, 32);
		}
	}

	if( gDisableCore != 0xffffffff )
		NxDbgMsg( NX_DBG_INFO, "[SET_DISABLE] Disable cores : 0x%08x\n", gDisableCore);
	else
		NxDbgMsg( NX_DBG_INFO, "[SET_DISABLE] Disable %d cores\n", disable_core_num);
	for (int chipId=1; chipId <= handle->numChips; chipId++) {
		Btc08SetDisable (handle, chipId, disable_cores);
	}

	// Sequence 8. Set PLL Freq.
	type = get_board_type(handle);
	if (type == BOARD_TYPE_ASIC)
	{
		NxDbgMsg(NX_DBG_INFO, "[SET_PLL] Set PLL %d\n", pll_freq);
		if (0 > SetPllFreq(handle, pll_freq))
			return;
	}

	// Seqeunce 9. Run Bist Test
	RunBist( handle );

	// Sequence 10. Enable OON IRQ/Set UART divider
	for (int chipId = last_chipId + 1; chipId <= handle->numChips; chipId++)
	{
		Btc08SetControl(handle, chipId, (OON_IRQ_EN | UART_DIVIDER));
	}

	// Sequence 11. Get Golden Vector Data
	GetGoldenVector(4, &data, 0);

	// Seqeunce 12. Setting Parameters
	if(!is_diff_data)
	{
		data.midState[32*1] += 1;
		data.midState[32*2] += 2;
		data.midState[32*3] += 3;
	}
	Btc08WriteParam(handle, BCAST_CHIP_ID, data.midState, data.parameter);

	// Seqeunce 13. Setting Target Parameters
	Btc08WriteTarget(handle, BCAST_CHIP_ID, data.target);

	// Sequence 14. Setting Nonce Range
	if(is_full_nonce)
	{
		DistributionNonce(handle, start_full_nonce, end_full_nonce);
	}
	else
	{
		DistributionNonce(handle, start_small_nonce, end_small_nonce);
	}

	for( int i=0; i<handle->numChips ; i++ )
	{
		NxDbgMsg(NX_DBG_INFO, "Chip[%d:%d] : %02x%02x%02x%02x ~ %02x%02x%02x%02x\n", i, handle->numCores[i],
			handle->startNonce[i][0], handle->startNonce[i][1], handle->startNonce[i][2], handle->startNonce[i][3],
			handle->endNonce[i][0], handle->endNonce[i][1], handle->endNonce[i][2], handle->endNonce[i][3]);
		Btc08WriteNonce(handle, i+1, handle->startNonce[i], handle->endNonce[i]);
	}

	NxDbgMsg( NX_DBG_INFO, "===== RUN JOB =====\n");

	// Sequence 15. Run Job
	Btc08RunJob(handle, BCAST_CHIP_ID, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), jobId++);

	// Sequence 16. Check Interrupt Signals(GPIO) and Post Processing
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
					printf("OON2\n");
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

static void *TestWorkLoop_freq(void *arg)
{
	struct BTC08_INFO *btc08_info = ((struct BTC08_INFO*)arg);
	uint8_t chipId = 0x00, jobId = 0x00;
	uint8_t fifo_full = 0x00, oon_irq = 0x00, gn_irq = 0x00, oon_job_id = 0x00, gn_job_id = 0x00;
	uint8_t res[4] = {0x00,};
	uint8_t disable_cores[32] = {0x00,};
	uint8_t oon_jobid, gn_jobid;
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	struct timespec ts_start, ts_end, ts_diff;
	uint64_t jobcnt = 0;
	uint64_t hashes_done;
	uint64_t hashrate;
	uint64_t allCores = 0;
	BTC08_HANDLE handle = btc08_info->handle;
	BOARD_TYPE type = BOARD_TYPE_ASIC;
	VECTOR_DATA data;

	NxDbgMsg(NX_DBG_INFO, "===== Start Frequency Test Mode =====\n");
	NxDbgMsg(NX_DBG_INFO, "  Disable Core   : %dea\n", btc08_info->disable_core_num);
	NxDbgMsg(NX_DBG_INFO, "  Disable Mask   : 0x%08x\n", gDisableCore);
	NxDbgMsg(NX_DBG_INFO, "  Freqeyncy      : %dMHz\n",  btc08_info->pll_freq);
	NxDbgMsg(NX_DBG_INFO, "  Nonce          : %s\n",  btc08_info->is_full_nonce?"Full":"Small");
	NxDbgMsg(NX_DBG_INFO, "  Range          : %s\n",  btc08_info->is_diff_range?"Same":"Distribution");
	NxDbgMsg(NX_DBG_INFO, "=====================================\n");

	// Sequence 1. Reset HW
	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );
	DbgGpioOff();

	// Sequence 2. Set ASIC Boost
	handle->isAsicBoost = true;

	// Seqeunce 3. Find number of chips : using AutoAddress
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_DEBUG, "[Init] NumChips = %d\n", handle->numChips);
	ReadId(handle);

	// Sequence 4. Reset SW
	Btc08Reset(handle, BCAST_CHIP_ID);

	// Sequence 5. Set last chip
	Btc08SetControl(handle, 1, LAST_CHIP | OON_IRQ_EN | UART_DIVIDER);
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "NumChips     : %d\n", handle->numChips);
	ReadId(handle);

	// Sequence 6. Set Enable Cores
	if( gDisableCore != 0xffffffff )
	{
		//	Use user disable makse
		memset(disable_cores, 0xff, 32);
		disable_cores[28] = (gDisableCore >> 24) & 0xFF;
		disable_cores[29] = (gDisableCore >> 16) & 0xFF;
		disable_cores[30] = (gDisableCore >>  8) & 0xFF;
		disable_cores[31] = (gDisableCore >>  0) & 0xFF;
	}
	else
	{
		if( btc08_info->disable_core_num > 0 )
		{
			memset(disable_cores, 0xff, 32);
			disable_cores[31] &= ~(1);
			for (int i=1; i<(BTC08_NUM_CORES-btc08_info->disable_core_num); i++) {
				disable_cores[31-(i/8)] &= ~(1 << (i % 8));
			}
		}
		else
		{
			memset(disable_cores, 0x00, 32);
		}
	}

	if( gDisableCore != 0xffffffff )
	{
		NxDbgMsg( NX_DBG_DEBUG, "[SET_DISABLE] Disable cores : 0x%08x\n", gDisableCore);
	}
	else
	{
		NxDbgMsg( NX_DBG_DEBUG, "[SET_DISABLE] Disable %d cores\n", btc08_info->disable_core_num);
	}

	for (int chipId=1; chipId <= handle->numChips; chipId++)
	{
		Btc08SetDisable (handle, chipId, disable_cores);
	}

	// Sequence 7. Set PLL Freq.
	type = get_board_type(handle);
	if (type == BOARD_TYPE_ASIC)
	{
		NxDbgMsg(NX_DBG_DEBUG, "[SET_PLL] Set PLL %d\n", btc08_info->pll_freq);
		if (0 > SetPllFreq(handle, btc08_info->pll_freq))
		{
			goto ERROR_BIST;
		}
	}

	// Seqeunce 8. Run Bist Test
	DbgGpioOn();
	RunBist( handle );
	DbgGpioOff();

	for(int i = 0; i < handle->numChips; i++)
	{
		allCores += handle->numCores[i];
	}
	NxDbgMsg(NX_DBG_INFO, "Active Cores : [%d / %d]\n", allCores, handle->numChips * (BTC08_NUM_CORES - btc08_info->disable_core_num));
	if(allCores != handle->numChips * (BTC08_NUM_CORES - btc08_info->disable_core_num))
	{
		btc08_info->isDone = true;
		goto ERROR_BIST;
	}
	NxDbgMsg(NX_DBG_INFO, "=====================================\n");

	// Sequence 9. Enable OON IRQ/Set UART divider
	for(int chipId = 2; chipId <= handle->numChips; chipId++)
	{
		Btc08SetControl(handle, chipId, (OON_IRQ_EN | UART_DIVIDER));
	}

	// Sequence 10. Get Golden Vector Data
	GetGoldenVector(4, &data, 0);

	// Seqeunce 11. Setting Parameters
	Btc08WriteParam(handle, BCAST_CHIP_ID, data.midState, data.parameter);

	// Seqeunce 12. Setting Target Parameters
	Btc08WriteTarget(handle, BCAST_CHIP_ID, data.target);

	// Sequence 13. Setting Nonce Range
	if(btc08_info->is_diff_range)
	{
		if(btc08_info->is_full_nonce)
		{
			for( int i=0; i<handle->numChips ; i++ )
			{
				NxDbgMsg(NX_DBG_INFO, "Chip[%d:%d] : %02x%02x%02x%02x ~ %02x%02x%02x%02x\n", i + 1, handle->numCores[i],
					start_full_nonce[0], start_full_nonce[1], start_full_nonce[2], start_full_nonce[3],
					end_full_nonce[0],end_full_nonce[1], end_full_nonce[2], end_full_nonce[3]);
				Btc08WriteNonce(handle, i + 1, start_full_nonce, end_full_nonce);
			}
		}
		else
		{
			for( int i=0; i<handle->numChips ; i++ )
			{
				NxDbgMsg(NX_DBG_INFO, "Chip[%d:%d] : %02x%02x%02x%02x ~ %02x%02x%02x%02x\n", i + 1, handle->numCores[i],
					start_small_nonce[0], start_small_nonce[1], start_small_nonce[2], start_small_nonce[3],
					end_small_nonce[0],end_small_nonce[1], end_small_nonce[2], end_small_nonce[3]);
				Btc08WriteNonce(handle, i + 1, start_full_nonce, end_small_nonce);
			}
		}
	}
	else
	{
		if(btc08_info->is_full_nonce)
		{
			DistributionNonce(handle, start_full_nonce, end_full_nonce);
		}
		else
		{
			DistributionNonce(handle, start_small_nonce, end_small_nonce);
		}
		for( int i=0; i<handle->numChips ; i++ )
		{
			NxDbgMsg(NX_DBG_INFO, "Chip[%d:%d] : %02x%02x%02x%02x ~ %02x%02x%02x%02x\n", i + 1, handle->numCores[i],
				handle->startNonce[i][0], handle->startNonce[i][1], handle->startNonce[i][2], handle->startNonce[i][3],
				handle->endNonce[i][0], handle->endNonce[i][1], handle->endNonce[i][2], handle->endNonce[i][3]);
			Btc08WriteNonce(handle, i+1, handle->startNonce[i], handle->endNonce[i]);
		}
	}

	// Sequence 14. Run Job
	for (int i = 0; i < MAX_JOB_FIFO_NUM; i++)
	{
		NxDbgMsg(NX_DBG_INFO, "=== Run Job with jobId#%d\n", jobId);
		Btc08RunJob(handle, BCAST_CHIP_ID, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), jobId++);
	}
	DbgGpioOn();
	tstimer_time(&ts_start);
	while(!btc08_info->isDone)
	{
		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_GN))	// Check GN GPIO pin
		{
			for (int i=1; i<=handle->numChips; i++)
			{
				pthread_mutex_lock(&btc08_info->lock);
				Btc08ReadJobId(handle, i, res, res_size);
				gn_job_id  = res[1];
				gn_irq 	   = res[2] & (1<<0);
				chipId     = res[3];

				if (0 != gn_irq) // If GN IRQ is set, then handle GN
				{
				// NxDbgMsg(NX_DBG_INFO, "Active Cores : [%3d / %3d]\n", allCores, handle->numChips * (BTC08_NUM_CORES - btc08_info->disable_core_num));
					NxDbgMsg(NX_DBG_INFO, "GN : chip#%d for jobId#%d\t[%3d / %3d]\n", chipId, gn_job_id, allCores, handle->numChips * (BTC08_NUM_CORES - btc08_info->disable_core_num));
					if (handleGN(handle, chipId, golden_nonce) < 0)
					{
						NxDbgMsg( NX_DBG_ERR, "=== GN Read fail! ==\n");
						btc08_info->isDone = true;
						pthread_mutex_unlock(&btc08_info->lock);
						singlecommand_command_list2();
						goto ERROR_RUN;
					}
				}
				else // If GN IRQ is not set, then go to check OON
				{
					NxDbgMsg(NX_DBG_DEBUG, "%5s === H/W GN occured but GN_IRQ value is not set!!!\n", "");
				}
				pthread_mutex_unlock(&btc08_info->lock);
			}
		}
		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_OON))	// Check OON
		{
			Btc08ReadJobId(handle,  handle->numChips, res, res_size);
			oon_job_id = res[0];
			oon_irq	  = res[2] & (1<<1);
			chipId    = res[3];

			if (0 != oon_irq)	// In case of OON
			{
				if (chipId == handle->numChips)		// OON occures on chip#1 > chip#2 > chip#3
				{
					pthread_mutex_lock(&btc08_info->lock);
					// NxDbgMsg(NX_DBG_INFO, "Active Cores : [%3d / %3d]\n", allCores, handle->numChips * (BTC08_NUM_CORES - btc08_info->disable_core_num));
					NxDbgMsg(NX_DBG_DEBUG, "    OON : chip#%d for oon_jobId#%d\n", chipId, oon_job_id);
					int ret = handleOON(handle, BCAST_CHIP_ID);
					if (ret == 0)
					{
						NxDbgMsg(NX_DBG_INFO, "Run : jobId#%d\n", jobId);
						Btc08RunJob(handle, BCAST_CHIP_ID, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), jobId++);
						jobcnt++;
						if (jobId >= MAX_JOB_ID)
						{
							jobId = 0;
						}
					}
					else
					{
						NxDbgMsg(NX_DBG_INFO, "    oon_job_id(%d), jobcnt(%d)\n", oon_job_id, jobcnt);
					}

					if (0 == GpioGetValue(handle->hKey0))
					{
						NxDbgMsg( NX_DBG_INFO, "KEY0 is pressed!\n");
						pthread_mutex_unlock(&btc08_info->lock);
						singlecommand_command_list2();
						goto ERROR_RUN;
					}
					pthread_mutex_unlock(&btc08_info->lock);
				}
			}
		}
		sched_yield();
	}
ERROR_RUN:
	DbgGpioOff();
	tstimer_time(&ts_end);
	tstimer_diff(&ts_end, &ts_start, &ts_diff);

	// The expected hashrate = 600 mhash/sec [MinerCoreClk(50MHz) * NumOfCores(3cores) * AsicBoost(4sub-cores)]
	calc_hashrate(handle->isAsicBoost, jobcnt, &ts_diff);
ERROR_BIST:
	DestroyBtc08( handle );
}

static void TestWork_Change_freq(struct BTC08_INFO *btc08_info, int freq)
{
	BOARD_TYPE type = BOARD_TYPE_ASIC;
	DbgGpioOff();
	type = get_board_type(btc08_info->handle);
	if (type == BOARD_TYPE_ASIC)
	{
		NxDbgMsg(NX_DBG_INFO, "[SET_PLL] Set PLL %d\n", freq);
		pthread_mutex_lock(&btc08_info->lock);
		if (0 > SetPllFreq(btc08_info->handle, freq))
		{
			pthread_mutex_unlock(&btc08_info->lock);
			return;
		}
		pthread_mutex_unlock(&btc08_info->lock);
	}
	DbgGpioOn();
}

/**
 * @brief Initialize Target Board
 * @date 2022.08.11
 * @author dnjsdlf5400@coasia.com
 * @param btc08_info Data structure to store
 * @return int 0(Success), -1(Error:Freqency), -2(Error:Active Cores)
 */
static int InitBTC08_INFO(struct BTC08_INFO *btc08_info)
{
	BTC08_HANDLE handle = btc08_info->handle;
	uint8_t disable_cores[32] = {0x00,};
	uint16_t allCores = 0;
	BOARD_TYPE type = BOARD_TYPE_ASIC;

	NxDbgMsg(NX_DBG_INFO, "\n========= Init Board Status =========\n");

	// Step 1. Reset HW
	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );
	DbgGpioOff();

	// Step 2. Set ASIC Boost
	handle->isAsicBoost = true;

	// Step 3. Find All chips
	handle->numChips = Btc08AutoAddress(handle);
	btc08_info->numChips = handle->numChips;
	NxDbgMsg(NX_DBG_DEBUG, "[Init] NumChips = %d\n", handle->numChips);
	ReadId(handle);

	// Step 4. Reset SW
	Btc08Reset(handle, BCAST_CHIP_ID);

	// Step 5. Set last chip
	Btc08SetControl(handle, 1, (LAST_CHIP | OON_IRQ_EN | UART_DIVIDER));
	handle->numChips = Btc08AutoAddress(handle);
	btc08_info->numChips = handle->numChips;
	NxDbgMsg(NX_DBG_INFO, "NumChips     : %d\n", handle->numChips);
	ReadId(handle);

	// Step 6. Set Enable Cores
	if( gDisableCore != 0xffffffff )
	{
		//	Use user disable makse
		memset(disable_cores, 0xff, 32);
		disable_cores[28] = (gDisableCore >> 24) & 0xFF;
		disable_cores[29] = (gDisableCore >> 16) & 0xFF;
		disable_cores[30] = (gDisableCore >>  8) & 0xFF;
		disable_cores[31] = (gDisableCore >>  0) & 0xFF;
	}
	else
	{
		if( btc08_info->disable_core_num > 0 )
		{
			memset(disable_cores, 0xff, 32);
			disable_cores[31] &= ~(1);
			for (int i=1; i<(BTC08_NUM_CORES-btc08_info->disable_core_num); i++) {
				disable_cores[31-(i/8)] &= ~(1 << (i % 8));
			}
		}
		else
		{
			memset(disable_cores, 0x00, 32);
		}
	}

	for (int i = 0; i < handle->numChips; i++)
	{
		Btc08SetDisable (handle, i + 1, disable_cores);
	}

	// Step 7. Set PLL Freq.
	type = get_board_type(handle);
	if (type == BOARD_TYPE_ASIC)
	{
		if (0 > SetPllFreq(handle, btc08_info->pll_freq))
		{
			NxDbgMsg(NX_DBG_INFO, "Error : Can not change pll frequency\n");
			return -1;
		}
	}
	NxDbgMsg(NX_DBG_INFO, "PLL          : %d Mhz\n", btc08_info->pll_freq);

	// Step 8. Run Bist
	DbgGpioOn();
	RunBist(handle);
	DbgGpioOff();

	// Step 9. Check All Cores
	for(int i = 0; i < handle->numChips; i++)
	{
		allCores += (uint16_t)handle->numCores[i];
	}
	NxDbgMsg(NX_DBG_INFO, "Active Cores : [%3d/%3d]\n", allCores, handle->numChips * BTC08_NUM_CORES);
	if(allCores != handle->numChips * BTC08_NUM_CORES)
	{
		return -2;
	}

	// Step 10. Enable OON IRQ/Set UART divider
	for(int i = 1; i < handle->numChips; i++)
	{
		Btc08SetControl(handle, i + 1, (OON_IRQ_EN | UART_DIVIDER));
	}

	// Step 11. Get Golden Vector Data
	GetGoldenVector(btc08_info->idx_data, &btc08_info->chip_data, 0);

	// Step 12. Setting Parameters
	Btc08WriteParam(handle, BCAST_CHIP_ID, btc08_info->chip_data.midState, btc08_info->chip_data.parameter);

	// Step 13. Setting Target parameters
	Btc08WriteTarget(handle, BCAST_CHIP_ID, btc08_info->chip_data.target);

	// Step 14. Setting Nonce Range
	if(btc08_info->is_full_nonce)
	{
		NxDbgMsg(NX_DBG_INFO, "Nonce Range  : %02x%02x%02x%02x ~ %02x%02x%02x%02x\n",
			start_full_nonce[0], start_full_nonce[1], start_full_nonce[2], start_full_nonce[3],
			end_full_nonce[0],end_full_nonce[1], end_full_nonce[2], end_full_nonce[3]);
		Btc08WriteNonce(handle, BCAST_CHIP_ID, start_full_nonce, end_full_nonce);
	}
	else
	{
		NxDbgMsg(NX_DBG_INFO, "Nonce Range  : %02x%02x%02x%02x ~ %02x%02x%02x%02x\n",
			start_small_nonce[0], start_small_nonce[1], start_small_nonce[2], start_small_nonce[3],
			end_small_nonce[0],end_small_nonce[1], end_small_nonce[2], end_small_nonce[3]);
		Btc08WriteNonce(handle, BCAST_CHIP_ID, start_small_nonce, end_small_nonce);
	}

	// Step 15. Initiaize BTC08_INFO data (chip_*[]...)
	for(int i = 0; i < MAX_CHIPS; i++)
	{
		btc08_info->chip_enable[i] = false;
		btc08_info->chip_isdone[i] = true;
	}

	btc08_info->jobId = 0;
	btc08_info->numGN = 0;
	btc08_info->numOON = 0;

	NxDbgMsg(NX_DBG_INFO, "====================================\n\n");

	return 0;
}

/**
 * @brief Set the BTC08_INFO and runjob
 * @date 2022.08.11
 * @author dnjsdlf5400@coasia.com
 * @param btc08_info Data structure to store
 * @return int 0(Success), -1(Error:Wrong chipId)
 */
static int SetBTC08_INFO(struct BTC08_INFO *btc08_info)
{
	BTC08_HANDLE handle = btc08_info->handle;

	// Step 1. Check chipId
	if(btc08_info->chip_enable[btc08_info->chipId])
	{
		return -1;
	}

	// Step 2. Set BTC08_INFO data (chip_*[]...)
	btc08_info->chip_enable[btc08_info->chipId] = true;
	btc08_info->chip_isdone[btc08_info->chipId] = false;

	// Step 8. Run Job (Only N chip)
	NxDbgMsg(NX_DBG_INFO, "  Chip#%d --> jobId#%d, JobId#%d\n", btc08_info->chipId, btc08_info->jobId, btc08_info->jobId + 1);
	for(int i = 0; i < 2; i++)
	{
		Btc08RunJob(handle, btc08_info->chipId, (handle->isAsicBoost ? ASIC_BOOST_EN : 0x00), btc08_info->jobId++);
	}

	return 0;
}

/**
 * @brief pthread function
 * @date 2022.08.11
 * @author dnjsdlf5400@coasia.com
 * @param arg struct BTC08_INFO
 */
static void *TestWorkLoop_EnableChip(void *arg)
{
	struct BTC08_INFO *btc08_info = ((struct BTC08_INFO*)arg);
	BTC08_HANDLE handle = btc08_info->handle;
	struct timespec ts_start, ts_end, ts_diff;

	uint8_t chipId = 0x00;
	uint8_t oon_irq = 0x00, gn_irq = 0x00, oon_job_id = 0x00, gn_job_id = 0x00;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	// Step 1. Initialize BTC08_INFO
	if(InitBTC08_INFO(btc08_info) != 0)
	{
		NxDbgMsg(NX_DBG_INFO, "Error : Can not initialize BTC08_INFO\n");
		goto ERROR_RUN;
	}

	// Step 2. Set BTC08_INFO
	if(btc08_info->delay >= 0)
	{
		pthread_mutex_lock(&btc08_info->lock);
		for(int i = 0; i < btc08_info->numChips; i++)
		{
			DbgGpioOn();
			btc08_info->chipId = i + 1;
			if(SetBTC08_INFO(btc08_info) != 0)
			{
				NxDbgMsg(NX_DBG_INFO, "Error : chip#%d is already used\n", btc08_info->chipId);
				pthread_mutex_unlock(&btc08_info->lock);
				DbgGpioOff();
				goto ERROR_RUN;
			}
			DbgGpioOff();
			if(btc08_info->delay > 0)
			{
				usleep(btc08_info->delay);
			}
		}
		pthread_mutex_unlock(&btc08_info->lock);
	}
	else
	{
		pthread_mutex_lock(&btc08_info->lock);
		DbgGpioOn();
		if(SetBTC08_INFO(btc08_info) != 0)
		{
			NxDbgMsg(NX_DBG_INFO, "Error : chip#%d is already used\n", btc08_info->chipId);
			pthread_mutex_unlock(&btc08_info->lock);
			DbgGpioOff();
			goto ERROR_RUN;
		}
		DbgGpioOff();
		pthread_mutex_unlock(&btc08_info->lock);
	}

	// Step 3. Check GN or OON Interrupt
	DbgGpioOn();
	tstimer_time(&ts_start);
	while(!btc08_info->isDone)
	{
		if(0 == Btc08GpioGetValue(handle, GPIO_TYPE_GN)) // Check GN GPIO pin
		{
			for(int i = 0; i < handle->numChips; i++) // Check all of active chips
			{
				if(btc08_info->chip_enable[i + 1]) // if enable chip
				{
					pthread_mutex_lock(&btc08_info->lock);
					Btc08ReadJobId(handle, i + 1, res, res_size);
					gn_job_id  = res[1];
					gn_irq 	   = res[2] & (1<<0);
					chipId     = res[3];

					if (0 != gn_irq) // if gn_irq is occurs
					{
						if(!btc08_info->chip_isdone[i + 1])
						{
							btc08_info->chip_isdone[i + 1] = true;

							NxDbgMsg(NX_DBG_INFO, "GN : chip#%d for jobId#%d\n", i + 1, gn_job_id);
							btc08_info->numGN++;

							if (handleGN(handle, i + 1, golden_nonce) < 0)
							{
								NxDbgMsg(NX_DBG_INFO, "=== GN Read fail [chip#%d for jobId#%d]\n", i + 1, gn_job_id);
								btc08_info->isDone = true;
								pthread_mutex_unlock(&btc08_info->lock);
								singlecommand_command_list3();
								goto ERROR_RUN;
							}
						}
					}
					else // If GN IRQ is not set, then go to check OON
					{
						NxDbgMsg(NX_DBG_DEBUG, "=== H/W GN occured but GN_IRQ value is not set\n");
					}
					pthread_mutex_unlock(&btc08_info->lock);
				}
			}
		}
		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_OON))	// Check OON GPIO pin
		{
			for (int i = 0; i < handle->numChips; i++)
			{
				if(btc08_info->chip_enable[i + 1]) // if enable chip
				{
					pthread_mutex_lock(&btc08_info->lock);
					Btc08ReadJobId(handle,  i + 1, res, res_size);
					oon_job_id = res[0];
					oon_irq	  = res[2] & (1<<1);
					chipId    = res[3];

					if (0 != oon_irq) // if oon_irq is occurs
					{
						if(!btc08_info->chip_isdone[i + 1]) // if gn_irq is not occurs
						{
							NxDbgMsg(NX_DBG_INFO, "OON : chip#%d for jobId#%d\n", i + 1, oon_job_id);
							btc08_info->numOON++;
						}

						if(handleOON(handle, i + 1) == 0)
						{
							btc08_info->chip_isdone[i + 1] = false;
							NxDbgMsg(NX_DBG_INFO, "  Run jobId#%d --> chip#%d\n", btc08_info->jobId, i + 1);
							Btc08RunJob(handle, i + 1, (handle->isAsicBoost ? ASIC_BOOST_EN : 0x00), btc08_info->jobId++);
							btc08_info->jobId = btc08_info->jobId >= MAX_JOB_ID ? 0 : btc08_info->jobId;
						}
					}
					pthread_mutex_unlock(&btc08_info->lock);
				}
			}
		}
		sched_yield();
	}

ERROR_RUN:
	DbgGpioOff();
	DestroyBtc08(handle);
	tstimer_time(&ts_end);
	tstimer_diff(&ts_end, &ts_start, &ts_diff);
	NxDbgMsg(NX_DBG_INFO, "\n=========================\n");
	NxDbgMsg(NX_DBG_INFO, "GN    : %d\n", btc08_info->numGN);
	NxDbgMsg(NX_DBG_INFO, "OON   : %d\n", btc08_info->numOON);
	NxDbgMsg(NX_DBG_INFO, "Total : %d\n", btc08_info->numGN + btc08_info->numOON);
	NxDbgMsg(NX_DBG_INFO, "-------------------------\n");
	calc_hashrate(handle->isAsicBoost, btc08_info->numGN + btc08_info->numOON, &ts_diff);
	NxDbgMsg(NX_DBG_INFO, "=========================\n\n");
}

void SingleCommandLoop(void)
{
	static char cmdStr[NX_SHELL_MAX_ARG * NX_SHELL_MAX_STR];
	static char cmd[NX_SHELL_MAX_ARG][NX_SHELL_MAX_STR];
	int cmdCnt;
	BTC08_HANDLE handle;

#if USE_BTC08_FPGA
	handle = CreateBtc08(0);
#else
	//	create BTC08 instance into index 0/1. ( /dev/spidev0.0 or /dev/spidev2.0 )
	if ((plug_status_0 == 1) && (plug_status_1 != 1)) {
		handle = CreateBtc08(0);
	} else if ((plug_status_0 != 1) && (plug_status_1 == 1)) {
		handle = CreateBtc08(1);
	}
#endif

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
			uint8_t disable_core_num = 0;
			int pll_freq = 24;
			int wait_gpio = 0;
			int delay = 200;
			if( cmdCnt > 1 )
			{
				disable_core_num = strtol(cmd[1], 0, 10);
			}
			if( cmdCnt > 2 )
			{
				pll_freq = strtol(cmd[2], 0, 10);
			}
			if( cmdCnt > 3 )
			{
				wait_gpio = strtol(cmd[3], 0, 10);
			}
			if( cmdCnt > 4 )
			{
				delay = strtol(cmd[4], 0, 10);
			}
			printf("disable_core_num = %d, pll_freq = %d, wait_gpio = %d\n",
					disable_core_num, pll_freq, wait_gpio);
			TestBist( handle, disable_core_num, pll_freq, wait_gpio, delay );
		}
		//----------------------------------------------------------------------
		//	Set the chip as the last chip
		else if (!strcasecmp(cmd[0], "4") )
		{
			int fault_chip_id = 0;
			int pll_freq = 24;
			if( cmdCnt > 1 )
			{
				fault_chip_id = strtol(cmd[1], 0, 10);
			}
			if ( cmdCnt > 2 )
			{
				pll_freq = strtol(cmd[2], 0, 10);
			}
			printf("fault_chip_id = %d\n", fault_chip_id);
			TestLastChip( handle, fault_chip_id, pll_freq );
		}
		//----------------------------------------------------------------------
		//	Disable core
		else if (!strcasecmp(cmd[0], "5") )
		{
			uint8_t disable_core_num = 0;
			uint32_t pll_freq = 24;
			uint8_t is_full_nonce = 0;
			int fault_chip_id = 0;
			uint8_t is_infinite_mining = 0;
			if( cmdCnt > 1 )
			{
				disable_core_num = strtol(cmd[1], 0, 10);
			}
			if ( cmdCnt > 2 )
			{
				pll_freq = strtol(cmd[2], 0, 10);
			}
			if ( cmdCnt > 3 )
			{
				is_full_nonce = strtol(cmd[3], 0, 10);
			}
			if( cmdCnt > 4 )
			{
				fault_chip_id = strtol(cmd[4], 0, 10);
			}
			if( cmdCnt > 5 )
			{
				is_infinite_mining = strtol(cmd[5], 0, 10);
			}
			printf("disable_core_num = %d, pll_freq = %d MHz is_full_nonce = %d, fault_chip_id = %d, is_infinite_mining = %d\n",
					disable_core_num, pll_freq, is_full_nonce, fault_chip_id, is_infinite_mining);
			TestDisableCore( handle, disable_core_num, pll_freq, is_full_nonce, fault_chip_id, is_infinite_mining );
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
			TestXCore(handle, disable_core_num);
		}
		//----------------------------------------------------------------------
		//	Read ID Test
		else if ( !strcasecmp(cmd[0], "12") )
		{
			TestReadId(handle);
		}
		//----------------------------------------------------------------------
		//	Read Bist Test
		else if ( !strcasecmp(cmd[0], "13") )
		{
			TestReadBist(handle);
		}
		//----------------------------------------------------------------------
		//	Set Pll Test
		else if ( !strcasecmp(cmd[0], "14") )
		{
			uint32_t pll_freq = 24;
			if ( cmdCnt > 1 )
			{
				pll_freq = strtol(cmd[1], 0, 10);
			}
			printf("pll_freq = %d\n", pll_freq);
			TestSetPll(handle, pll_freq);
		}
		//----------------------------------------------------------------------
		//	Read Pll Test
		else if ( !strcasecmp(cmd[0], "15") )
		{
			TestReadPll(handle);
		}
		//----------------------------------------------------------------------
		//	Read Job Id Test
		else if ( !strcasecmp(cmd[0], "16") )
		{
			TestReadJobId(handle);
		}
		//----------------------------------------------------------------------
		//	Read Disable Test
		else if ( !strcasecmp(cmd[0], "17") )
		{
			TestReadDisable(handle);
		}
		//----------------------------------------------------------------------
		//	Read Debug Cnt Test
		else if ( !strcasecmp(cmd[0], "18") )
		{
			TestReadDebugCnt(handle);
		}
		//----------------------------------------------------------------------
		//	Set Reset GPIO val Test
		else if( !strcasecmp(cmd[0], "19") )
		{
			int val = 0;
			if ( cmdCnt > 1 )
				val = strtol(cmd[1], 0, 10);
			printf("val = %d\n", val);
			TestResetGpio( handle, !val );
		}
		// Read voltage and temperature
		else if( !strcasecmp(cmd[0], "20") )
		{
			TestReadVolTemp( handle );
		}
	}

	DestroyBtc08( handle );
}

void SingleCommandLoop_freq(uint8_t disCore, uint8_t isFullNonce, uint8_t isDiffRange, int32_t freqM)
{
	static char cmdStr[NX_SHELL_MAX_ARG * NX_SHELL_MAX_STR];
	static char cmd[NX_SHELL_MAX_ARG][NX_SHELL_MAX_STR];
	int cmdCnt;
	int ret;

	struct BTC08_INFO *btc08_info = NULL;

	singlecommand_command_list2();
	printf("start\n");
	goto FIRST_START;

	for( ;; )
	{
		fgets( cmdStr, NX_SHELL_MAX_ARG*NX_SHELL_MAX_STR - 1, stdin );
		cmdCnt = Shell_GetArgument( cmdStr, cmd );

		//----------------------------------------------------------------------
		if( !strcasecmp(cmd[0], "q") )
		{
			if( btc08_info != NULL )
			{
				pthread_mutex_lock(&btc08_info->lock);
				btc08_info->isDone = true;
				pthread_mutex_unlock(&btc08_info->lock);
				pthread_join(btc08_info->pThread, (void**)&ret);
				free(btc08_info);
				btc08_info = NULL;
			}
			break;
		}
		else if( !strcasecmp(cmd[0], "start") )
		{
FIRST_START:
			if( btc08_info == NULL )
			{
				btc08_info = (struct BTC08_INFO *)calloc( sizeof(struct BTC08_INFO), 1 );
				if( btc08_info )
				{
#if USE_BTC08_FPGA
					btc08_info->handle = CreateBtc08(0);
#else
					if ((plug_status_0 == 1) && (plug_status_1 != 1))
						btc08_info->handle = CreateBtc08(0);
					else if ((plug_status_0 != 1) && (plug_status_1 == 1))
						btc08_info->handle = CreateBtc08(1);
#endif
					pthread_mutex_init(&btc08_info->lock, NULL);

					btc08_info->isDone = false;

					btc08_info->disable_core_num = disCore;
					btc08_info->is_full_nonce = isFullNonce;
					btc08_info->pll_freq = freqM;
					btc08_info->is_diff_range = isDiffRange;
					if(pthread_create(&btc08_info->pThread, NULL, TestWorkLoop_freq, (void*)btc08_info) != 0)
					{
						printf("hthread error\n");
					}
				}
			}
			else
			{
				printf("already started!!\n");
			}
			continue;
		}
		else if( !strcasecmp(cmd[0], "stop") )
		{
			if( btc08_info != NULL )
			{
				pthread_mutex_lock(&btc08_info->lock);
				btc08_info->isDone = true;
				pthread_mutex_unlock(&btc08_info->lock);
				pthread_join(btc08_info->pThread, (void**)&ret);
				free(btc08_info);
				btc08_info = NULL;
			}
			else
			{
				printf("Not runing, start first!!\n");
			}
			singlecommand_command_list2();
			continue;
		}
		else if( !strcasecmp(cmd[0], "freq") )
		{
			uint32_t freq = atoi(cmd[1]);
			if( btc08_info && !btc08_info->isDone )
			{
				TestWork_Change_freq(btc08_info, freq);
			}
			continue;
		}
		else if(!strcasecmp(cmd[0], "help"))
		{
			singlecommand_command_list2();
			continue;
		}
		else{
			printf("unknown command : %s \n", cmd[0]);
			singlecommand_command_list2();
		}
	}
}

void SingleCommandLoop_EnableChip(int freqM)
{
	static char cmdStr[NX_SHELL_MAX_ARG * NX_SHELL_MAX_STR];
	static char cmd[NX_SHELL_MAX_ARG][NX_SHELL_MAX_STR];
	int cmdCnt;
	int ret;

	struct BTC08_INFO *btc08_info = NULL;
	uint8_t chipId = 0;
	uint8_t idx_data = 255;
	int delay;

	singlecommand_command_list3();

	for( ;; )
	{
		fgets( cmdStr, NX_SHELL_MAX_ARG*NX_SHELL_MAX_STR - 1, stdin );
		cmdCnt = Shell_GetArgument( cmdStr, cmd );

		//----------------------------------------------------------------------
		if( !strcasecmp(cmd[0], "start") )
		{
			// Step 1. Check Input Data (chipId, idx_data)
			chipId = (uint8_t)atoi(cmd[1]);
			idx_data = (uint8_t)atoi(cmd[2]);

			if(chipId < 1 || chipId >= MAX_CHIPS)
			{
				NxDbgMsg(NX_DBG_INFO, "Error : Input wrong chipId\n");
				singlecommand_command_list3();
				continue;
			}
			if(idx_data < 0 || idx_data >= MAX_JOB_INDEX)
			{
				NxDbgMsg(NX_DBG_INFO, "Error : Input wrong idx_data[0~6]\n");
				singlecommand_command_list3();
				continue;
			}
			if(btc08_info)
			{
				NxDbgMsg(NX_DBG_INFO, "Error : already started\n");
				singlecommand_command_list3();
				continue;
			}

			// Step 2. Create BTC08_INFO
			btc08_info = (struct BTC08_INFO *)calloc( sizeof(struct BTC08_INFO), 1 );

			if(!btc08_info)
			{
				NxDbgMsg(NX_DBG_INFO, "Error : Can not create BTC08_INFO\n");
				singlecommand_command_list3();
				continue;
			}

#if USE_BTC08_FPGA
			btc08_info->handle = CreateBtc08(0);
#else
			if ((plug_status_0 == 1) && (plug_status_1 != 1))
				btc08_info->handle = CreateBtc08(0);
			else if ((plug_status_0 != 1) && (plug_status_1 == 1))
				btc08_info->handle = CreateBtc08(1);
#endif

			btc08_info->chipId = chipId; 
			btc08_info->disable_core_num = 0;
			btc08_info->is_full_nonce = 0;
			btc08_info->pll_freq = freqM;
			btc08_info->idx_data = idx_data;
			btc08_info->delay = -1;
			btc08_info->isDone = false;

			// Step 3. Mutex init
			pthread_mutex_init(&btc08_info->lock, NULL);

			// Step 4. Create thread
			if(pthread_create(&btc08_info->pThread, NULL, TestWorkLoop_EnableChip, (void*)btc08_info))
			{
				printf("Error : Can not create pthread\n");
				free(btc08_info);
				singlecommand_command_list3();
			}
		}
		else if( !strcasecmp(cmd[0], "start2") )
		{
			// Step 1. Check Input Data (delay)
			delay = atoi(cmd[1]);

			if(delay < 0)
			{
				NxDbgMsg(NX_DBG_INFO, "Error : Input wrong chipId\n");
				singlecommand_command_list3();
				continue;
			}

			// Step 2. Create BTC08_INFO
			btc08_info = (struct BTC08_INFO *)calloc( sizeof(struct BTC08_INFO), 1 );

			if(!btc08_info)
			{
				NxDbgMsg(NX_DBG_INFO, "Error : Can not create BTC08_INFO\n");
				singlecommand_command_list3();
				continue;
			}

#if USE_BTC08_FPGA
			btc08_info->handle = CreateBtc08(0);
#else
			if ((plug_status_0 == 1) && (plug_status_1 != 1))
				btc08_info->handle = CreateBtc08(0);
			else if ((plug_status_0 != 1) && (plug_status_1 == 1))
				btc08_info->handle = CreateBtc08(1);
#endif

			btc08_info->pll_freq = freqM;
			btc08_info->disable_core_num = 0;
			btc08_info->is_full_nonce = 1;
			btc08_info->delay = delay;
			btc08_info->idx_data = 4;
			btc08_info->isDone = false;

			// Step 3. Mutex init
			pthread_mutex_init(&btc08_info->lock, NULL);

			// Step 4. Create thread
			if(pthread_create(&btc08_info->pThread, NULL, TestWorkLoop_EnableChip, (void*)btc08_info))
			{
				printf("Error : Can not create pthread\n");
				free(btc08_info);
				singlecommand_command_list3();
			}
		}
		else if( !strcasecmp(cmd[0], "stop") )
		{
			if(btc08_info)
			{
				// Step 1. Set "isDone" to 1
				pthread_mutex_lock(&btc08_info->lock);
				btc08_info->isDone = true;
				pthread_mutex_unlock(&btc08_info->lock);

				// Step 2. Wait thread finish
				pthread_join(btc08_info->pThread, (void**)&ret);

				// Step 3. free BTC08_INFO
				free(btc08_info);
				btc08_info = NULL;
			}
			break;
		}
		else if( !strcasecmp(cmd[0], "enable") )
		{
			// Step 1. Check Input Data (chipId)
			chipId = (uint8_t)atoi(cmd[1]);

			if(chipId < 1 || chipId >= MAX_CHIPS)
			{
				NxDbgMsg(NX_DBG_INFO, "Error : Input wrong chipId\n");
				singlecommand_command_list3();
				continue;
			}
			if(!btc08_info)
			{
				NxDbgMsg(NX_DBG_INFO, "Error : Not started\n");
				singlecommand_command_list3();
				continue;
			}

			// Step 2. Set BTC08_INFO
			btc08_info->chipId = chipId;
			if(SetBTC08_INFO(btc08_info) != 0)
			{
				NxDbgMsg(NX_DBG_INFO, "Error : chip#%d is already used\n", btc08_info->chipId);
			}
		}
		else if( !strcasecmp(cmd[0], "disable") )
		{
			// Step 1. Check Input Data (chipId)
			chipId = (uint8_t)atoi(cmd[1]);
			bool isAllDisable = true;

			if(chipId < 1 || chipId >= MAX_CHIPS)
			{
				NxDbgMsg(NX_DBG_INFO, "Error : Input wrong chipId\n");
				continue;
			}
			if(!btc08_info)
			{
				NxDbgMsg(NX_DBG_INFO, "Error : Not started\n");
				continue;
			}

			// Step 2. Disable Selected chip
			pthread_mutex_lock(&btc08_info->lock);
			if(btc08_info->chip_enable[chipId])
			{
				btc08_info->chip_enable[chipId] = false;
				btc08_info->chip_isdone[chipId] = true;
				NxDbgMsg(NX_DBG_INFO, "    Disable chip#%d for jobId#%d\n", chipId, btc08_info->jobId++);
			}
			else
			{
				NxDbgMsg(NX_DBG_INFO, "Error : chip#%d is not used\n", chipId);
			}
			pthread_mutex_unlock(&btc08_info->lock);

			for(int i = 0; i < MAX_CHIPS; i++)
			{
				if(btc08_info->chip_enable[i])
				{
					isAllDisable = false;
					break;
				}
			}
			if(isAllDisable)
			{
				singlecommand_command_list3();
			}
		}
		else if(!strcasecmp(cmd[0], "help"))
		{
			singlecommand_command_list3();
		}
		else{
			printf("unknown command : %s \n", cmd[0]);
			if(btc08_info->isDone)
			{
				singlecommand_command_list3();
			}
		}
	}
}
