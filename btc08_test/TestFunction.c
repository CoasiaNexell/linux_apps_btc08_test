#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <byteswap.h>

#include "TempCtrl.h"
#include "Btc08.h"
#include "Utils.h"

#ifdef NX_DTAG
#undef NX_DTAG
#endif
#define NX_DTAG "[TestFunction]"
#include "NX_DbgMsg.h"

#define BCAST_CHIP_ID		0x00
#define ASIC_BOOST_EN		(0x02)
#define FIX_NONCE_MODE		(0x01)

#define MAX_JOB_FIFO_NUM	4
#define MAX_JOB_ID			256
#define MAX_CHIP_NUM		22

pthread_mutex_t shutdown_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool shutdown = true;
static int numCores[MAX_CHIP_NUM] = {0,};

/* GOLDEN_MIDSTATE */
static uint8_t default_golden_midstate[32] = {
	0x5f, 0x4d, 0x60, 0xa2, 0x53, 0x85, 0xc4, 0x07,
	0xc2, 0xa8, 0x4e, 0x0c, 0x25, 0x91, 0x69, 0xc4,
	0x10, 0xa4, 0xa5, 0x4b, 0x93, 0xf7, 0x17, 0x08,
	0xf1, 0xab, 0xdf, 0xec, 0x6e, 0x8b, 0x81, 0xd2,
};

/* GOLDEN_DATA */
static uint8_t default_golden_data[12] = {
	/* Data (MerkleRoot, Time, Target) */
	0xf4, 0x2a, 0x1d, 0x6e, 0x5b, 0x30, 0x70, 0x7e,
	0x17, 0x37, 0x6f, 0x56,
};

/* GOLDEN_NONCE */
static uint8_t golden_nonce[4] = {
	0x66, 0xcb, 0x34, 0x26
};

/* GOLDEN_NONCE_RANGE(2G) */
static uint8_t golden_nonce_start[4] = {
	0x66, 0xcb, 0x00, 0x00
};

static uint8_t golden_nonce_end[4] = {
	0x66, 0xcb, 0xff, 0xff
};

/* GOLDEN_TARGET */
static uint8_t default_golden_target[6] = {
	0x17, 0x37, 0x6f, 0x56, 0x05, 0x00
};

/* GOLDEN_HASH */
static uint8_t default_golden_hash[32] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x22, 0x09, 0x3d, 0xd4, 0x38, 0xed, 0x47,
	0xfa, 0x28, 0xe7, 0x18, 0x58, 0xb8, 0x22, 0x0d,
	0x53, 0xe5, 0xcd, 0x83, 0xb8, 0xd0, 0xd4, 0x42,
};

/* Enable all cores */
static uint8_t golden_enable[32] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

void TestBist()
{
	int numChips;
	uint8_t *ret;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	//	create BTC08 instance into index 0. ( /dev/spidev0.0 )
	BTC08_HANDLE handle = CreateBtc08(0);
	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	numChips = Btc08AutoAddress(handle);

	NxDbgMsg(NX_DBG_INFO, "Number of Chips = %d\n", numChips);

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

	DestroyBtc08( handle );
}

int ResetAutoAddress()
{
	int numChips;
	//	create BTC08 instance into index 0. ( /dev/spidev0.0 )
	BTC08_HANDLE handle = CreateBtc08(0);
	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	numChips = Btc08AutoAddress(handle);
	NxDbgMsg( NX_DBG_INFO, "Number of Chips = %d\n", numChips );

	DestroyBtc08( handle );
	return 0;
}

int handleOON(BTC08_HANDLE handle)
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

uint32_t calRealGN(const uint8_t chipId, const uint8_t *in, const uint8_t valid_cnt)
{
	uint32_t *gn = (uint32_t *)in;

	*gn = bswap_32(*gn);

	NxDbgMsg(NX_DBG_DEBUG, "in[0x%08x] swap[0x%08x] cal[0x%08x] \n",
			in, gn, (*gn - valid_cnt * numCores[chipId]));

	return (*gn - valid_cnt * numCores[chipId]);
}

int handleGN(BTC08_HANDLE handle, uint8_t chipId)
{
	int ret = 0;
	uint8_t hash[128] = {0x00,};
	unsigned int hash_size = sizeof(hash)/sizeof(hash[0]);
	uint8_t gn[18] = {0x00,};
	unsigned int gn_size = sizeof(gn)/sizeof(gn[0]);
	uint8_t lower3, lower2, lower, upper, validCnt;
	bool match;

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

/* Process one work(nonce range: 2G) without asicboost */
void TestWork()
{
	int numChips;
	int chipId = 0x00, jobId=0x01;
	struct timespec ts_start, ts_oon, ts_gn, ts_diff;
	uint8_t fifo_full = 0x00, oon_irq = 0x00, gn_irq = 0x00;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	tstimer_time(&ts_start);
	NxDbgMsg(NX_DBG_INFO, "[%ld.%lds] Start of TestWork!\n", ts_start.tv_sec, ts_start.tv_nsec);

	BTC08_HANDLE handle = CreateBtc08(0);

	Btc08ResetHW(handle, 1);
	Btc08ResetHW(handle, 0);

	// Auto Address
	numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "Number of Chips = %d\n", numChips);

	// RUN_JOB for one work without asicboost
	Btc08WriteParam(handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
	Btc08WriteNonce(handle, BCAST_CHIP_ID, golden_nonce_start, golden_nonce_end);
	Btc08RunJob(handle, BCAST_CHIP_ID, 0x00, jobId);

	do {
		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_GN))	// Check GN GPIO pin
		{
			Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
			gn_irq 	  = res[2] & (1<<0);
			chipId    = res[3];

			if (1 != gn_irq) {				// In case of not GN
				NxDbgMsg(NX_DBG_INFO, "OON IRQ is not set!\n");
				break;						// Go to check OON
			} else {						// In case of GN
				// Check if found GN(0x66cb3426) is correct and submit nonce to pool server and then go loop again
				tstimer_time(&ts_gn);
				NxDbgMsg(NX_DBG_INFO, "[%ld.%lds] GN!!!\n", ts_gn.tv_sec, ts_gn.tv_nsec);

				handleGN(handle, chipId);
				continue;
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
	} while( !shutdown );

	tstimer_diff(&ts_oon, &ts_start, &ts_diff);

	NxDbgMsg(NX_DBG_INFO, "[%ld.%lds] End of TestWork \n", ts_diff.tv_sec, ts_diff.tv_nsec);

	DestroyBtc08( handle );
}

/* Process 4 works with asicboost at first.
 * If OON IRQ occurs, clear OON and then pass the additional work to job fifo.
 * If GN IRQ occurs, read GN and then clear GN IRQ.
 */
void TestWorkLoop(int numWorks)
{
	int numChips;
	uint8_t chipId = 0x00, jobId = 0x01, jobcnt = 0x01;
	uint8_t fifo_full = 0x00, oon_irq = 0x00, gn_irq = 0x00;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	struct timespec ts_start, ts_oon, ts_gn;

	tstimer_time(&ts_start);
	NxDbgMsg(NX_DBG_INFO, "[%ld.%lds] Start workloop!\n", ts_start.tv_sec, ts_start.tv_nsec);

	BTC08_HANDLE handle = CreateBtc08(0);

	Btc08ResetHW(handle, 1);
	Btc08ResetHW(handle, 0);

	// Auto Address
	numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "Number of Chips = %d\n", numChips);

	// Write midstate and data(merkleroot, time, target) & target & start and end nonce
	Btc08WriteParam(handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
	Btc08WriteNonce(handle, BCAST_CHIP_ID, golden_nonce_start, golden_nonce_end);

	// Run jobs with asicboost
	for (int workId = 1; workId <= MAX_JOB_FIFO_NUM; workId++)
	{
		Btc08RunJob(handle, BCAST_CHIP_ID, ASIC_BOOST_EN, workId);
		jobcnt++;
	}

	do {
		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_GN))	// Check GN GPIO pin
		{
			Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
			gn_irq 	  = res[2] & (1<<0);
			chipId    = res[3];

			if (1 != gn_irq) {		// If GN IRQ is not set, then go to check OON
				NxDbgMsg(NX_DBG_INFO, "OON IRQ is not set!\n");
				break;
			} else {				// If GN IRQ is set, then handle GN
				// Check if found GN(0x66cb3426) is correct and submit nonce to pool server and then go loop again
				tstimer_time(&ts_gn);
				NxDbgMsg(NX_DBG_INFO, "[%ld.%lds] GN!!!\n", ts_gn.tv_sec, ts_gn.tv_nsec);

				handleGN(handle, chipId);
				continue;
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
					Btc08RunJob(handle, BCAST_CHIP_ID, ASIC_BOOST_EN, jobId);

					jobcnt++;
					jobId++;
					if (jobId > MAX_JOB_ID)		// [7:0] job id ==> 8bits (max 256)
						jobId = 1;
				}
				break;
			}
		}
	} while( !shutdown );

	NxDbgMsg(NX_DBG_INFO, "Total works = %d\n", jobcnt);

	DestroyBtc08( handle );
}

void ResetHW( int32_t enable )
{
	BTC08_HANDLE handle = CreateBtc08(0);

	Btc08ResetHW( handle, enable );

	DestroyBtc08( handle );
}

void *mon_temp_thread( void *arg )
{
	int ch = 1;
	float mv;
	float temperature;

	while( !shutdown )
	{
		mv = get_mvolt(ch);
		temperature = get_temp(mv);

        NxDbgMsg( NX_DBG_INFO, "Channel %d Voltage = %.2f mV, Temperature = %.2f C\n", ch, mv, temperature );
		if ( temperature >= 100. ) {
			ResetHW( 1 );
		}

		sleep(1);
	}

	NxDbgMsg( NX_DBG_INFO, "Stopped temperature monitoring\n" );

    return NULL;
}

void StartMonTempThread()
{
	int ret;
	pthread_t mon_temp_thr;

	if ( !shutdown )
		return;

	shutdown = false;
	ret = pthread_create( &mon_temp_thr, NULL, mon_temp_thread, NULL );
	if (ret) {
		NxDbgMsg( NX_DBG_ERR, "Failed to start temperature monitor thread: %s\n", strerror(ret) );
	}

	pthread_detach( mon_temp_thr );
}

void ShutdownMonTempThread()
{
	if ( !shutdown )
		shutdown = true;
}