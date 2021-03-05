#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <byteswap.h>
#include "Utils.h"
#include "Btc08.h"
#include "NX_Queue.h"
#include "NX_Semaphore.h"

#include "TestVector.h"
#include "TempCtrl.h"

#ifdef NX_DTAG
#undef NX_DTAG
#endif
#define NX_DTAG "[ScenarioTest]"
#include "NX_DbgMsg.h"

static int gst_IsService = 0;
static pthread_mutex_t gst_hCtrlLock = PTHREAD_MUTEX_INITIALIZER;

static void simplework_command_list()
{
	printf("\n\n");
	printf("========== Scenario =========\n");
	printf("  1. Start Service\n");
	printf("  2. Stop Service\n");
	printf("-----------------------------\n");
	printf("  q. quit\n");
	printf("=============================\n");
}



//
//	Description:
//	1. Loops (Thread)
//		a. Main Loop : Console ( start/stop/quit )
//		b. Command Porcessing Loop
//		c. Monitoring Loop
//		d. Mining Loop
//
//	2. Main Loop
//		- 서비스를 control하는 역할을 한다.
//		- 기본적으로 start/stop/quit 만 처리한다.
//		- Start Command의 경우 모든 부차적인 loop를 생성하고 Mining을 하게 한다.
//
//	3. Command Processing Loop
//		- Service 실행 도중에 필요한 command를 처리하는 loop이다.
//		- 실질적인 loop 상에서는 command queue를 check 하여 command가 존재하면 해당 command 를 수행하게 된다.
//
//	4. Monitoring Loop
//		- 전체적으로 system에 영향을 줄 수 있는 값들을 check 하여 필요에 따라서 command 를 발생한다.
//		- 현재 system에 영향을 줄수 있는 요인은 온도가 있고 여기서는 온도만을 check 할 것이다.
//
//	5. Mining Loop
//		- Mining을 하는 loop로 Job 을 생성하는 루프와 실제 Mining을 하는 Loop로 나뉜다.
//


typedef struct SEVICE_INFO {

	int bExitWorkLoop;
	int bExitMonitorLoop;

	//	Work Related Param
	uint64_t totalJobCnt;
	uint8_t jobId;						//	0 ~ 255

	//	 Temporature Alert
	int bTempAlert;
	float tempAlertValue;				//	reference temporature value for alert

	pthread_mutex_t hMutex;
	pthread_t		hWorkThread;		//	Work Thread Handler



	//	BTC08 Handle
	BTC08_HANDLE hBtc08;

	//	Loop Control Parameters
	int bExitJobLoop;
	int bExitMinLoop;
	int bExitMonLoop;
	int bExitCmdLoop;

	//	Queue
	NX_QUEUE		cmdQueue;			//	Command Queue
	NX_QUEUE		workQueue;			//	Work Queue

	NX_SEMAPHORE	hCmdSem;			//	Semaphore for Command
	NX_SEMAPHORE	hWorkSem;			//	Semaphore for work queue

	//	Loop Handle
	pthread_t		hJobThread;			//	Job Creation Loop
	pthread_t		hMinThread;			//	Mining Loop
	pthread_t		hMonThread;			//	Monitoring Loop
	pthread_t		hCmdThread;			//	Command Loop

	//	Nonce Distribution
	uint8_t			startNonce[MAX_CHIPS][4];
	uint8_t			endNonce[MAX_CHIPS][4];
} SERVICE_INFO;


/*
 *
 * 						Work Loop
 * 
 */

//
//	Initialize Work Loop : (Reset + Auto Address + Bist)
//	 a. Reset H/W : H/W Reset
//	 b. Auto Address : find out number of chips.
//	 c. Bist : find out number of cores individual chip.
//

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



////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                              Mining Block                                  //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
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

static void InitializeMiningkLoop( SERVICE_INFO *hService )
{
	uint8_t *ret;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	hService->hBtc08 = CreateBtc08(0);
	BTC08_HANDLE handle = hService->hBtc08;

	//	1. Reset
	Btc08ResetHW(handle, 1);
	Btc08ResetHW(handle, 0);

	// 2. Auto Address
	Btc08AutoAddress(handle);

	// 3. S/W Reset S/W
	Btc08Reset(handle);

	// 4. Set last chip
	Btc08SetControl(handle, 1, LAST_CHIP);
	handle->numChips = Btc08AutoAddress(handle);

	// 5. Enable all cores
	Btc08SetDisable (handle, BCAST_CHIP_ID, golden_enable);

	// 6. Find number of cores of individual chips
	RunBist( handle );

	// 7. Enable OON IRQ/Set UART divider
	Btc08SetControl(handle, BCAST_CHIP_ID, (OON_IRQ_EN | UART_DIVIDER));

	// 8. Nonce Distribution
	DistributionNonce( handle );

	hService->totalJobCnt = 0;
	hService->jobId = 1;
}

static void DeinitializeWorkLoop( SERVICE_INFO *hService )
{
	//	Ternoff H/W
	if( hService && hService->hBtc08 )
	{
		//	Set Reset
		Btc08ResetHW(hService->hBtc08, 1);
		DestroyBtc08(hService->hBtc08);
	}
}


static void AddGoldenJob( SERVICE_INFO *hService )
{
	//	Write midstate and data(merkleroot, time, target) & target & start and end nonce
	Btc08WriteParam(hService->hBtc08, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	Btc08WriteTarget(hService->hBtc08, BCAST_CHIP_ID, default_golden_target);
	Btc08WriteNonce(hService->hBtc08, BCAST_CHIP_ID, golden_nonce_start, golden_nonce_end);
	Btc08RunJob(hService->hBtc08, BCAST_CHIP_ID, ASIC_BOOST_EN, hService->jobId);
	if( hService->jobId >= (MAX_JOB_ID-1) )
		hService->jobId = 1;
	else
		hService->jobId ++;
	hService->totalJobCnt ++;
}

static uint32_t calRealGN(uint8_t chipId, uint8_t *in, uint32_t valid_cnt, uint32_t numCores)
{
	uint32_t *gn = (uint32_t *)in;

	*gn = bswap_32(*gn);

	NxDbgMsg(NX_DBG_DEBUG, "in[0x%08x] swap[0x%08x] cal[0x%08x] \n",
			in, gn, (*gn - valid_cnt * numCores));

	return (*gn - valid_cnt * numCores);
}

static int handleGN(SERVICE_INFO *hService, uint8_t chipId)
{
	int ret = 0;
	uint8_t hash[128] = {0x00,};
	unsigned int hash_size = sizeof(hash)/sizeof(hash[0]);
	uint8_t gn[18] = {0x00,};
	unsigned int gn_size = sizeof(gn)/sizeof(gn[0]);
	uint8_t lower3, lower2, lower, upper, validCnt;
	int match;

	// Read Hash
	Btc08ReadHash(hService->hBtc08, chipId, hash, hash_size);

	// Read Result to read GN and clear GN IRQ
	Btc08ReadResult(hService->hBtc08, chipId, gn, gn_size);
	validCnt = gn[1];

	for (int i=0; i<16; i+=4)
	{
		uint32_t cal_gn = calRealGN(chipId, &(gn[2+i]), validCnt, hService->hBtc08->numCores[chipId]);
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

static int handleOON(SERVICE_INFO *hService)
{
	int ret = 0;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	// Read ID to get FIFO status for chipId#1
	// The FIFO status of all chips are same.
	if (0 == Btc08ReadId (hService->hBtc08, 1, res, res_size))
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
			Btc08ClearOON(hService->hBtc08, BCAST_CHIP_ID);
			ret = 0;
		}
	}

	return ret;
}

static void *WorkLoop( void *arg )
{
	SERVICE_INFO *hService = (SERVICE_INFO*)arg;
	BTC08_HANDLE handle;
	uint32_t jobCnt = 0;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = 4;

	InitializeMiningkLoop( hService );

	handle = hService->hBtc08;

	AddGoldenJob(hService);

	// Run jobs with asicboost
	while( !hService->bExitWorkLoop )
	{
		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_GN))	// Check GN GPIO pin
		{
			uint8_t gn_irq = 0x00, chipId;
			Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
			gn_irq 	  = res[2] & (1<<0);
			chipId    = res[3];

			// If GN IRQ is not set, then go to check OON
			if (1 != gn_irq)
			{
				NxDbgMsg(NX_DBG_ERR, "H/W GN interrupt occured but GN_IRQ is not set!\n");
			}
			// If GN IRQ is set, then handle GN
			else
			{
				// Check if found GN(0x66cb3426) is correct and submit nonce to pool server and then go loop again
				handleGN(hService, chipId);
				continue;
			}
		}

		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_OON))	// Check OON
		{
			uint8_t oon_irq, fifo_full;
			// TODO: Need to check if it needs to read job id
			Btc08ReadJobId(handle, BCAST_CHIP_ID, res, res_size);
			fifo_full = res[2] & (1<<2);
			oon_irq	  = res[2] & (1<<1);

			if (1 != oon_irq) {				// OON IRQ is not set (cgminer: check OON timeout is expired)
				NxDbgMsg(NX_DBG_WARN, "H/W OON interrupt occured but OON IRQ is not set!\n");
				// if oon timeout is expired, disable chips
				// if oon timeout is not expired, check oon gpio again
			} else {						// If OON IRQ is set, handle OON
				handleOON(hService);
				AddGoldenJob(hService);
			}
		}
		sched_yield();
	}

	return (void*)0xDeadFace;
}


/*
 *
 * 						Monitor Loop
 * 
 */
void *MonitorLoop( void *arg )
{
	int adcCh = 1;
	float temperature;
	SERVICE_INFO *hService = (SERVICE_INFO*)arg;

	while( !hService->bExitMonitorLoop )
	{
		temperature = get_temp(get_mvolt(adcCh));
		if( temperature > hService->tempAlertValue )
		{
			NxDbgMsg( NX_DBG_INFO, "Temper Alert : %.3f > %.3f\n", temperature, hService->tempAlertValue );
		}
		usleep(500);
	}
	return (void*)0xDeadFace;
}


static SERVICE_INFO *StartService( void )
{
	SERVICE_INFO *hService;

	if( gst_IsService )
		return NULL;

	hService = (SERVICE_INFO *)malloc(sizeof(SERVICE_INFO));

	if( !hService )
		return NULL;

	pthread_mutex_lock( &gst_hCtrlLock );

	memset( hService, 0, sizeof(SERVICE_INFO) );
	hService->bExitWorkLoop = 1;
	hService->bExitMonitorLoop = 1;

	//	Monitor Loop
	hService->bExitMonitorLoop = 0;
	if( pthread_create( &hService->hWorkThread, NULL, MonitorLoop, hService ) )
	{
		hService->bExitMonitorLoop = 0;
		goto ERROR_EXIT;
	}

	//	Work Loop
	hService->bExitWorkLoop = 0;
	if( pthread_create( &hService->hMonThread, NULL, WorkLoop, hService ) )
	{
		hService->bExitWorkLoop = 1;
		goto ERROR_EXIT;
	}

	pthread_mutex_unlock( &gst_hCtrlLock );
	return hService;

ERROR_EXIT:
	if( !hService->bExitWorkLoop )
	{
		hService->bExitWorkLoop = 1;
		pthread_join( hService->hWorkThread, NULL );
	}

	if( !hService->bExitMonitorLoop )
	{
		hService->bExitMonitorLoop = 1;
		pthread_join( hService->hMonThread, NULL );
	}

	free( hService );
	pthread_mutex_unlock( &gst_hCtrlLock );
	return NULL;
}

static void StopService( SERVICE_INFO *hService )
{
	pthread_mutex_lock( &gst_hCtrlLock );
	if( hService )
	{
		if( !hService->bExitWorkLoop )
		{
			hService->bExitWorkLoop = 1;
			pthread_join( hService->hWorkThread, NULL );
		}
		if( !hService->bExitMonitorLoop )
		{
			hService->bExitMonitorLoop = 1;
			pthread_join( hService->hMonThread, NULL );
		}
		free( hService );
	}
	pthread_mutex_unlock( &gst_hCtrlLock );
}


void ScenarioTestLoop(void)
{
	static char cmdStr[NX_SHELL_MAX_ARG * NX_SHELL_MAX_STR];
	static char cmd[NX_SHELL_MAX_ARG][NX_SHELL_MAX_STR];
	int cmdCnt;
	SERVICE_INFO *hService = NULL;
	for( ;; )
	{
		simplework_command_list();
		printf( "scenario> " );
		fgets( cmdStr, NX_SHELL_MAX_ARG*NX_SHELL_MAX_STR - 1, stdin );
		cmdCnt = Shell_GetArgument( cmdStr, cmd );

		//----------------------------------------------------------------------
		if( !strcasecmp(cmd[0], "q") )
		{
			break;
		}
		else if( !strcasecmp(cmd[0], "1") )
		{
			if( NULL == hService )
			{
				hService = StartService();
			}
			break;
		}
		else if( !strcasecmp(cmd[0], "2") )
		{
			if( hService )
			{
				StopService(hService);
				hService = NULL;
			}
			break;
		}
	}
}
