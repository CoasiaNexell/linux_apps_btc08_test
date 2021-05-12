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


static void singlecommand_command_list()
{
	printf("\n\n");
	printf("====== Single Command =======\n");
	printf("  1. H/W Reset\n");
	printf("  2. Reset and Auto Address \n");
	printf("  3. Reset and TestBist\n");
	printf("  4. Set the chip to the last chip\n");
	printf("    ex > 4 [chipId(1~3)]\n");
	printf("  5. Disable core\n");
	printf("    ex > 5 [chipId(1~3)] [core index(0~255)]\n");
	printf("  6. Write, Read Target\n");
	printf("  7. Set, Read disable\n");
	printf("  8. Read Revision\n");
	printf("  9. Read Feature\n");
	printf("  10. Read/Write IO Ctrl\n");
	printf("-----------------------------\n");
	printf("  q. quit\n");
	printf("=============================\n");
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
			NxDbgMsg(NX_DBG_INFO, "%5s Result Hash of Inst_%s!!!\n", "",
					(i==0) ? "Upper":(((i==1) ? "Lower": ((i==2) ? "Lower_2":"Lower_3"))));
		else
		{
			NxDbgMsg(NX_DBG_ERR, "%5s Failed: Result Hash of Inst_%s!!!\n", "",
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

static void RunJob(BTC08_HANDLE handle)
{
	int chipId = 0x00, jobId=0x01;
	struct timespec ts_start, ts_oon, ts_gn, ts_diff;
	bool	ishashdone = false;
	uint8_t fifo_full = 0x00, oon_irq = 0x00, gn_irq = 0x00, oon_job_id = 0x00, gn_job_id = 0x00;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	uint8_t start_nonce[4] = { 0x66, 0x00, 0x00, 0x00 };
	uint8_t end_nonce[4]   = { 0x67, 0x00, 0x00, 0x00 };

	uint8_t hashdone_allchip[256];
	uint8_t hashdone_chip[256];

	memset(hashdone_allchip, 0xFF, sizeof(hashdone_allchip));
	memset(hashdone_chip,    0x00, sizeof(hashdone_chip));

	tstimer_time(&ts_start);
	NxDbgMsg(NX_DBG_INFO, "=== Start of TestWork! === [%ld.%lds]\n", ts_start.tv_sec, ts_start.tv_nsec);

	handle->isAsicBoost = true;

	NxDbgMsg(NX_DBG_INFO, "[RUN_JOB]\n");
	Btc08WriteParam(handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
	Btc08WriteNonce(handle, BCAST_CHIP_ID, start_nonce, end_nonce);
	for (int i=0; i<4; i++) {
		Btc08RunJob(handle, BCAST_CHIP_ID, (handle->isAsicBoost ? ASIC_BOOST_EN:0x00), i);
		for (int chipId = 1; chipId <= handle->numChips; chipId++)
		{
			Btc08ReadId(handle, chipId, res, res_size);
			NxDbgMsg( NX_DBG_INFO, "[READ_ID] ChipId = %d, NumJobs = %d\n",
						chipId, (res[2]&7) );
		}
	}

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
				//NxDbgMsg(NX_DBG_INFO, "%2s === H/W interrupt occured, but GN_IRQ value is not set!\n", "");
			}
		}

		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_OON))	// Check OON
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
					ishashdone = true;
				}
			}
		}
		//sched_yield();
		usleep(500 * 1000);
		for (int chipId = 1; chipId <= handle->numChips; chipId++)
		{
			Btc08ReadId(handle, chipId, res, res_size);
			NxDbgMsg( NX_DBG_INFO, "[READ_ID] ChipId = %d, NumJobs = %d\n",
						chipId, (res[2]&7) );
		}
		ReadBist(handle);
	}
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
	NxDbgMsg( NX_DBG_INFO, "NumChips = %d\n", handle->numChips );
	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
		NxDbgMsg( NX_DBG_INFO, "ChipId = %d, NumJobs = %d\n",
					chipId, (res[2]&7) );
	}

	return 0;
}

/* Used to disable the chips */
void TestLastChip( BTC08_HANDLE handle, uint8_t last_chipId )
{
	int numChips;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	NxDbgMsg( NX_DBG_INFO, "=== AUTO ADDRESS ==\n");
	numChips = Btc08AutoAddress(handle);
	NxDbgMsg( NX_DBG_INFO, "%5s Number of Chips = %d\n", "", numChips );
	for (int chipId = 1; chipId <= numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
		NxDbgMsg( NX_DBG_INFO, "%5s ChipId = %d, Number of jobs = %d\n",
					"", chipId, (res[2]&7) );
	}

	NxDbgMsg( NX_DBG_INFO, "=== SET chipId #%d as the LAST CHIP ==\n", last_chipId );
	Btc08SetControl(handle, last_chipId, LAST_CHIP);

	NxDbgMsg( NX_DBG_INFO, "=== AUTO ADDRESS ==\n");
	numChips = Btc08AutoAddress(handle);
	NxDbgMsg( NX_DBG_INFO, "%5s Number of Chips = %d\n", "", numChips );
	for (int chipId = 1; chipId <= numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
		NxDbgMsg( NX_DBG_INFO, "%5s ChipId = %d, Number of jobs = %d\n",
					"", chipId, (res[2]&7) );
	}
}

/* Disable core */
void TestDisableCore( BTC08_HANDLE handle, uint8_t chip_id, uint8_t disable_core_num )
{
	uint8_t *ret;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	uint8_t disable_cores[32] = {0x00,};
	int pll_freq = 300;
	uint8_t wrong_golden_data[12] = {
	/* Data (MerkleRoot, Time, Target) */
	0xf4, 0x2a, 0x1d, 0x6e, 0x5b, 0x30, 0x70, 0x7e,
	0x17, 0x37, 0x6f, 0x00,
	};

	memset(disable_cores, 0, 32);

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "== [AUTO_ADDRESS] NumChips = %d\n", handle->numChips);
	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
		NxDbgMsg( NX_DBG_INFO, "[READ_ID] ChipId = %d, NumJobs = %d\n",
					chipId, (res[2]&7) );
	}

	// Software reset (TODO: Check FIFO status if it's empty)
	NxDbgMsg( NX_DBG_INFO, "[RESET]\n");
	Btc08Reset(handle);

	NxDbgMsg( NX_DBG_INFO, "[SET_CONTROL] Set chipId#1 as the LAST CHIP\n");
	// Set last chip
	Btc08SetControl(handle, 1, LAST_CHIP);
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "== [AUTO_ADDRESS] NumChips = %d\n", handle->numChips);

	NxDbgMsg( NX_DBG_INFO, "[SET_DISABLE] Enable all cores ==\n");
	// Enable all cores
	Btc08SetDisable (handle, BCAST_CHIP_ID, golden_enable);
/*
	// Sequence 7. Enable OON IRQ/Set UART divider (tb:32'h0000_8013, sh:0018_001f)
	Btc08SetControl(handle, BCAST_CHIP_ID, (OON_IRQ_EN | UART_DIVIDER));
*/
	NxDbgMsg( NX_DBG_INFO, "=== [SET_PLL] Set PLL ==\n");
	// 6. Set PLL freq
	if (0 > SetPllFreq(handle, pll_freq))
		return;

	NxDbgMsg( NX_DBG_INFO, "[BIST] BIST ==\n");
	// Write param for BIST
	Btc08WriteParam (handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
	Btc08WriteNonce (handle, BCAST_CHIP_ID, golden_nonce, golden_nonce);
	// Run BIST
	Btc08RunBist    (handle, default_golden_hash, default_golden_hash, default_golden_hash, default_golden_hash);
	ReadBist(handle);

	for (int chipId=1; chipId <= handle->numChips; chipId++)
	{
		Btc08ReadDisable(handle, chipId, res, 32);
		//HexDump("[READ_DISABLE]", res, 32);
	}

	memset(disable_cores, 0, 32);
	if( disable_core_num > 0 )
		disable_cores[31] = 1;
	for (int i=1; i<disable_core_num; i++) {
		disable_cores[31-(i/8)] |= (1 << (i % 8));
	}
	//HexDump("[disable_cores]", disable_cores, 32);

	NxDbgMsg( NX_DBG_INFO, "[SET_DISABLE] Disable %d cores\n", disable_core_num);
	// Disable the specific core in the chip
	//Btc08SetDisable (handle, chip_id, golden_enable);
	Btc08SetDisable (handle, chip_id, disable_cores);
	NxDbgMsg( NX_DBG_INFO, "[BIST] BIST\n");
	// Write param for BIST
	Btc08WriteParam (handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
	Btc08WriteNonce (handle, BCAST_CHIP_ID, golden_nonce, golden_nonce);
	// Run BIST
	Btc08RunBist    (handle, default_golden_hash, default_golden_hash, default_golden_hash, default_golden_hash);

	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		// If it's not BUSY status, read the number of cores in next READ_BIST
		for (int i=0; i<10; i++) {
			ret = Btc08ReadBist(handle, chipId);
			if ( (ret[0] & 1) == 0 )
				break;
			else
				NxDbgMsg( NX_DBG_DEBUG, "[READ_BIST] ChipId = %d, Status = %s, NumCores = %d\n",
						chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );

			usleep( 300 );
		}

		ret = Btc08ReadBist(handle, chipId);

		handle->numCores[chipId] = ret[1];
		NxDbgMsg( NX_DBG_INFO, "== [READ_BIST] ChipId = %d, Status = %s, NumCores = %d\n",
					chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );
	}

	for (int chipId=1; chipId <= handle->numChips; chipId++)
	{
		Btc08ReadDisable(handle, chipId, res, 32);
		HexDump("[READ_DISABLE]", res, 32);
	}

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

	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "== [AUTO_ADDRESS] NumChips = %d\n", handle->numChips);
	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
		NxDbgMsg( NX_DBG_INFO, "[READ_ID] ChipId = %d, NumJobs = %d\n",
					chipId, (res[2]&7) );
	}

	// Software reset (TODO: Check FIFO status if it's empty)
	NxDbgMsg( NX_DBG_INFO, "[RESET]\n");
	Btc08Reset(handle);

	NxDbgMsg( NX_DBG_INFO, "[SET_CONTROL] Set chipId#1 as the LAST CHIP\n");
	// Set last chip
	Btc08SetControl(handle, 1, LAST_CHIP);
	handle->numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "== [AUTO_ADDRESS] NumChips = %d\n", handle->numChips);

	NxDbgMsg( NX_DBG_INFO, "[SET_DISABLE] Enable all cores\n");
	// Enable all cores
	Btc08SetDisable (handle, BCAST_CHIP_ID, golden_enable);

#if 0
	NxDbgMsg( NX_DBG_INFO, "[SET_PLL] Set PLL\n");
	// 6. Set PLL freq
	if (0 > SetPllFreq(handle, pll_freq))
		return;
#endif

	NxDbgMsg( NX_DBG_INFO, "[BIST] Run BIST\n");
	// Write param for BIST
	Btc08WriteParam (handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
	Btc08WriteNonce (handle, BCAST_CHIP_ID, golden_nonce, golden_nonce);
	// Run BIST
	Btc08RunBist    (handle, default_golden_hash, default_golden_hash, default_golden_hash, default_golden_hash);

	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		// If it's not BUSY status, read the number of cores in next READ_BIST
		for (int i=0; i<10; i++) {
			ret = Btc08ReadBist(handle, chipId);
			if ( (ret[0] & 1) == 0 )
				break;
			else
				NxDbgMsg( NX_DBG_DEBUG, "[READ_BIST] ChipId = %d, Status = %s, NumCores = %d\n",
						chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );

			usleep( 300 );
		}

		ret = Btc08ReadBist(handle, chipId);

		handle->numCores[chipId] = ret[1];
		NxDbgMsg( NX_DBG_INFO, "== [READ_BIST] ChipId = %d, Status = %s, NumCores = %d\n",
					chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );
	}
}

/* WRITE/READ TARGET */
static int TestWRTarget( BTC08_HANDLE handle )
{
	int numChips, debug = 0;
	char title[512];
	uint8_t res[140] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	NxDbgMsg( NX_DBG_INFO, "=== AUTO ADDRESS ==\n");
	numChips = Btc08AutoAddress(handle);

	NxDbgMsg( NX_DBG_INFO, "=== WRITE_TARGET(per Chip) ==\n");
	for (int chipId=1; chipId <= numChips; chipId++)
	{
		if (0 == Btc08WriteTarget(handle, chipId, default_golden_target))
		{
			Btc08ReadTarget(handle, chipId, res, 6);
			if (0 != memcmp(default_golden_target, res, 6))
			{
				NxDbgMsg(NX_DBG_ERR, "=== Failed READ_TARGET(chip#%d) ==\n", chipId);
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
			NxDbgMsg(NX_DBG_ERR, "=== Failed WRITE_TARGET(per Chip) due to spi err ==\n");
			return -1;
		}
	}

	NxDbgMsg(NX_DBG_INFO, "=== WRITE_TARGET(BR) ==\n");
	if (0 == Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target))
	{
		for (int chipId=1; chipId <= numChips; chipId++)
		{
			Btc08ReadTarget(handle, chipId, res, 6);
			if (0 != memcmp(default_golden_target, res, 6))
			{
				NxDbgMsg(NX_DBG_ERR, "=== Failed READ_TARGET(chip#%d) ==\n", chipId);
				HexDump("write_target:", default_golden_target, 6);
				sprintf(title, "chipId(%d) read_target:", chipId);
				HexDump(title, res, 6);
				return -1;
			}
		}
	}
	else
	{
		NxDbgMsg(NX_DBG_ERR, "=== Failed WRITE_TARGET(BR) due to spi err ==\n");
		return -1;
	}

	NxDbgMsg(NX_DBG_INFO, "=== Succeed READ/WRITE_TARGET ==\n");

	return 0;
}

/* SET_DISABLE > READ_DISABLE */
static int TestWRDisable( BTC08_HANDLE handle )
{
	int numChips;
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
	numChips = Btc08AutoAddress(handle);

	Btc08Reset(handle);

	for (int chipId=1; chipId <= numChips; chipId++)
	{
		Btc08ReadDisable(handle, chipId, res, 32);
		HexDump("read_disable", res, 32);
	}
	
	// SET_DISABLE (per Chip)
	for (int chipId=1; chipId <= numChips; chipId++)
	{
		NxDbgMsg( NX_DBG_INFO, "=== SET_DISABLE(chip%d) ==\n", chipId);

		if (0 == Btc08SetDisable(handle, chipId, test_data1[chipId-1]))
		{
			// RUN_BIST
			Btc08WriteParam (handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
			Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
			Btc08WriteNonce (handle, BCAST_CHIP_ID, golden_nonce, golden_nonce);
			Btc08RunBist    (handle, default_golden_hash, default_golden_hash, default_golden_hash, default_golden_hash);

			// READ_BIST
			for (int i=0; i<10; i++) {
				ret = Btc08ReadBist(handle, chipId);
				if ( (ret[0] & 1) == 0 )
					break;
				usleep( 300 );
			}

			ret = Btc08ReadBist(handle, chipId);
			NxDbgMsg( NX_DBG_INFO, "%5s ChipId = %d, Status = %s, Number of cores = %d\n",
						"", chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );

			// READ_DISABLE
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
		Btc08RunBist    (handle, default_golden_hash, default_golden_hash, default_golden_hash, default_golden_hash);

		// READ_BIST
		for (int chipId = 1; chipId <= numChips; chipId++)
		{
			for (int i=0; i<10; i++) {
				ret = Btc08ReadBist(handle, chipId);
				if ( (ret[0] & 1) == 0 )
					break;
				usleep( 300 );
			}

			ret = Btc08ReadBist(handle, chipId);
			NxDbgMsg( NX_DBG_INFO, "%5s ChipId = %d, Status = %s, Number of cores = %d\n",
						"", chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );

			// READ_DISABLE
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
	int numChips;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	uint8_t fixed_rev[4] = {0x20, 0x01, 0x08, 0x00};

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	numChips = Btc08AutoAddress(handle);

	for (int chipId = 1; chipId <= numChips; chipId++)
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
	int numChips;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	// Fixed value: 0xB5B, FPGA/ASIC: 0x00/0x05, 0x00, Hash Depth: 0x88
	uint8_t fixed_feature[4] = {0xB5, 0xB0, 0x00, 0x88};

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	numChips = Btc08AutoAddress(handle);

	for (int chipId = 1; chipId <= numChips; chipId++)
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
	int numChips;
	uint8_t res[16] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	// default: 0x00000000_000e0000_00008000_ffffffff
	// wr     : 0x00000000_0015ffff_ffff0000_00000000
	uint8_t default_ioctrl[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0xff, 0xff, 0xff, 0xff};
	uint8_t wr_ioctrl[16]      = {0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	numChips = Btc08AutoAddress(handle);

	// READ_IO_CTRL to read default value
	for (int chipId = 1; chipId <= numChips; chipId++)
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
	for (int chipId = 1; chipId <= numChips; chipId++)
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
			uint8_t chip_id = 1;
			uint8_t core_idx = 0;
			if( cmdCnt > 2 )
			{
				chip_id  = strtol(cmd[1], 0, 10);
				core_idx = strtol(cmd[2], 0, 10);
			}
			printf("chip_id = %d, core_idx = %d\n", chip_id, core_idx);
			TestDisableCore( handle, chip_id, core_idx );
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
	}

	DestroyBtc08( handle );
}
