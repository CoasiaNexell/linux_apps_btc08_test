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
#include "PllCtrl.h"

#ifdef NX_DTAG
#undef NX_DTAG
#endif
#define NX_DTAG "[ScenarioTest]"
#include "NX_DbgMsg.h"

static FILE *bist_log = NULL;
static FILE *mining_log = NULL;
static char bist_logpath[512] = "";
static char mining_logpath[512] = "";
static pthread_mutex_t hServiceLock = PTHREAD_MUTEX_INITIALIZER;

static void simplework_command_list()
{
	printf("\n\n");
	printf("========== Scenario =========\n");
	printf("  1. Start Mining\n");
	printf("  2. Stop Mining\n");
	printf("  3. Bist with the pll freq\n");
	printf("  ex> 3 400 (default:300~1000MHz)\n");
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
	int 			bExitWorkLoop;
	int 			bExitMonitorLoop;

	//	Work Related Param
	uint8_t 		jobId;				//	0 ~ 255

	//	 Temporature Alert
	int 			bTempAlert;
	float 			tempAlertValue;		//	reference temporature value for alert
	float 			currentTemp;

	//	BTC08 Handle
	BTC08_HANDLE 	hBtc08;

	//	Monitoring/Mining Thread ID
	pthread_t		hMonThread;
	pthread_t		hWorkThread;

	//	Nonce Distribution
	uint8_t			startNonce[MAX_CHIPS][4];
	uint8_t			endNonce[MAX_CHIPS][4];

	// pll freq
	int 			pll_freq;
} SERVICE_INFO;

/* Report Hash Result
 * 1. pll frequency
 * 2. temperature
 * 3. hash rate
 */
static void reportHashResult(const int freq, const float temp, const double hashrate)
{
	char result[1024];
	int ret;

	snprintf(result, sizeof(result),
				"\nfrequency  : %d MHz\ntemperature: %.02f C\nhashrate   : %.2f MHash/s\n",
				freq, temp, hashrate);

	NxDbgMsg(NX_DBG_INFO, "[result]\n%s\n", result);

	ret = fwrite(result, strlen(result), 1, mining_log);
	fflush(mining_log);

	if (ret != 1)
		NxDbgMsg(NX_DBG_ERR, "HashResult fwrite error");
}

/* Report Bist Result
 * 1. pll frequency
 * 2. temperature
 * 3. the number of chips
 * 4. the number of cores passed BIST
 */
static void reportBistResult(const int freq, const float temp, BTC08_HANDLE handle)
{
	char result[1024];
	int len;
	size_t ret;

	snprintf(result, sizeof(result),
				"\nfrequency  : %d MHz\ntemperature: %.02f C\ntotal_chips: %d\n",
				freq, temp, handle->numChips);
	len = strlen(result);

	for (int i=1; i<=handle->numChips; i++) {
		snprintf(result + len, sizeof(result), "chip#%d cores: %d\n", i, handle->numCores[i-1]);
		len = strlen(result);
	}

	NxDbgMsg(NX_DBG_INFO, "[result]\n%s\n", result);

	ret = fwrite(result, strlen(result), 1, bist_log);
	fflush(bist_log);

	if (ret != 1)
		NxDbgMsg(NX_DBG_ERR, "BistResult fwrite error");
}

static void reportExitReason(const char *reason, FILE *logfile)
{
	size_t ret;
	NxDbgMsg(NX_DBG_INFO, "[exit reason] %s\n", reason);

	ret = fwrite(reason, strlen(reason), 1, logfile);
	fflush(logfile);

	if (ret != 1)
		NxDbgMsg(NX_DBG_ERR, "ExitReason fwrite error");
}

static void RunBist( BTC08_HANDLE handle )
{
	uint8_t *ret;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	NxDbgMsg(NX_DBG_INFO, "=== RUN BIST ==\n");

	Btc08WriteParam (handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);

	// Set the golden nonce instead of the nonce range
	Btc08WriteNonce (handle, BCAST_CHIP_ID, golden_nonce, golden_nonce);
	Btc08RunBist    (handle, default_golden_hash, default_golden_hash, default_golden_hash, default_golden_hash);

	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		// If it's not BUSY status, read the number of cores in next READ_BIST
		for (int i=0; i<10; i++)
		{
			ret = Btc08ReadBist(handle, chipId);
			if ( (ret[0] & 1) == 0 )
				break;
			else
				NxDbgMsg(NX_DBG_DEBUG, "%5s [chip#%d] status: %s, total cores: %d\n",
						"", chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1]);

			usleep( 300 );
		}

		ret = Btc08ReadBist(handle, chipId);
		handle->numCores[chipId-1] = ret[1];
		NxDbgMsg(NX_DBG_DEBUG, "%5s [chip#%d] status: %s, total cores: %d\n",
					"", chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1]);
	}
}

static void SetPllConfigByIdx(BTC08_HANDLE handle, int chipId, int pll_idx)
{
	// seq1. Disable FOUT
	Btc08SetPllFoutEn(handle, chipId, FOUT_EN_DISABLE);

	// seq2. Down reset
	Btc08SetPllResetB(handle, chipId, RESETB_RESET);

	// seq3. Set PLL(change PMS value)
	Btc08SetPllConfig(handle, pll_idx);

	// seq4. wait for 1 ms
	usleep(1000);

	// seq5. Enable FOUT
	Btc08SetPllFoutEn(handle, chipId, FOUT_EN_ENABLE);
}

static int ReadPllLockStatus(BTC08_HANDLE handle, int chipId)
{
	int lock_status;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	lock_status = Btc08ReadPll(handle, chipId, res, res_size);
	NxDbgMsg(NX_DBG_DEBUG, "%5s chip#%d is %s\n", "", chipId,
			(lock_status == STATUS_LOCKED)?"locked":"unlocked");

	return lock_status;
}

static void SetPllFreq(BTC08_HANDLE handle, int freq)
{
	int pll_idx = 0;

	pll_idx = GetPllFreq2Idx(freq);
	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		SetPllConfigByIdx(handle, chipId, pll_idx);
		ReadPllLockStatus(handle, chipId);
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

/* Set pll frequency > BIST > Enable OON IRQ & Set UART DIVIDER > Nonce Distribution */
static int InitializeMiningkLoop( SERVICE_INFO *hService )
{
	BTC08_HANDLE handle;

	hService->hBtc08 = CreateBtc08(0);
	if (hService->hBtc08 == NULL)
		return -1;

	handle = hService->hBtc08;

	//	1. Reset
	Btc08ResetHW(handle, 1);
	Btc08ResetHW(handle, 0);

	// 2. Auto Address
	handle->numChips = Btc08AutoAddress(handle);

	// 3. S/W Reset S/W
	Btc08Reset(handle);

	// 4. Set last chip
	Btc08SetControl(handle, 1, LAST_CHIP);
	handle->numChips = Btc08AutoAddress(handle);

	// 5. Enable all cores
	Btc08SetDisable (handle, BCAST_CHIP_ID, golden_enable);

	// 6. Set PLL freq
	SetPllFreq(handle, hService->pll_freq);

	// 7. Find number of cores of individual chips
	RunBist(handle);

	// 8. Enable OON IRQ/Set UART divider
	Btc08SetControl(handle, BCAST_CHIP_ID, (OON_IRQ_EN | UART_DIVIDER));

	// 9. Nonce Distribution
	DistributionNonce(handle);

	hService->jobId = 1;

	return 0;
}

static void DeinitializeWorkLoop( SERVICE_INFO *hService )
{
	//	Stop work loop
	if (hService)
	{
		hService->bExitWorkLoop = 1;
	}
}

static void AddGoldenJob( SERVICE_INFO *hService, VECTOR_DATA *data )
{
	BTC08_HANDLE handle;
	handle = hService->hBtc08;

	// Get golden vector
	GetGoldenVectorWithVMask(4, data, 0);

	Btc08WriteParam(handle, BCAST_CHIP_ID, data->midState, data->parameter);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, data->target);
	DistributionNonce(handle);
	for(int i=0; i<handle->numChips ; i++)
	{
		NxDbgMsg( NX_DBG_INFO, "Chip[%d:%d] : %02x%02x%02x%02x ~ %02x%02x%02x%02x\n",
			i, handle->numCores[i], handle->startNonce[i][0], handle->startNonce[i][1],
			handle->startNonce[i][2], handle->startNonce[i][3], handle->endNonce[i][0],
			handle->endNonce[i][1], handle->endNonce[i][2], handle->endNonce[i][3]);
		Btc08WriteNonce(handle, i+1, handle->startNonce[i], handle->endNonce[i]);
	}

	// Run job
	Btc08RunJob(handle, BCAST_CHIP_ID, ASIC_BOOST_EN, hService->jobId++);
	if(hService->jobId >= (MAX_JOB_ID-1))
		hService->jobId = 1;
}

static int handleGN(BTC08_HANDLE handle, uint8_t chipId, uint8_t *found_nonce, uint8_t *micro_job_id, VECTOR_DATA *data)
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

/* Work Loop
 * Initialize Work Loop : (Reset + Auto Address + Bist)
 *	 a. Reset H/W : H/W Reset
 *	 b. Auto Address : find out number of chips.
 *	 c. Bist : find out number of cores individual chip.
 */
static void *WorkLoop( void *arg )
{
	SERVICE_INFO *hService = (SERVICE_INFO*)arg;
	BTC08_HANDLE handle;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = 4;
	uint8_t gn_irq = 0x0, gn_job_id = 0x0, chipId = 0x0;
	uint64_t startTime, currTime, deltaTime, prevTime;
	uint64_t totalProcessedHash = 0;
	double totalTime;
	double megaHash;
	uint8_t micro_job_id;
	uint32_t found_nonce[4];
	uint8_t job_id;
	VECTOR_DATA data;
	char reason[1024] = {0,};

	int status = InitializeMiningkLoop( hService );
	if (status < 0) {
		snprintf(reason, sizeof(reason), "test failed due to btc08 handle creation failure\n");
		goto failure;
	}

	handle = hService->hBtc08;
	if (handle->numChips < 1 || handle->numChips > MAX_CHIP_NUM) {
		snprintf(reason, sizeof(reason), "test failed due to spi err. wrong number of chips:%d\n", handle->numChips);
		goto failure;
	}

	for (int chipId = 1; chipId <= handle->numChips; chipId++) {
		Btc08ReadId(handle, chipId, res, res_size);
		if (res[3] != chipId) {
			memset(reason, 0, sizeof(char)*512);
			snprintf(reason, sizeof(reason), "test failed due to spi err. wrong chipId:(%d != %d)\n", res[3], chipId);
			goto failure;
		}
	}

	for (int i = 0; i < MAX_JOB_FIFO_NUM; i++) {
		AddGoldenJob(hService, &data);
	}

	startTime = get_current_ms();
	prevTime = startTime;

	while( !hService->bExitWorkLoop )
	{
		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_GN))	// Check GN GPIO pin
		{
			for (int i=1; i<=handle->numChips; i++)
			{
				Btc08ReadJobId(handle, i, res, res_size);
				gn_job_id  = res[1];
				gn_irq 	   = res[2] & (1<<0);
				chipId     = res[3];

				NxDbgMsg(NX_DBG_INFO, " <== OON jobId(%d) GN jobid(%d) ChipId(%d) %s %s %s\n",
							res[0], res[1], res[3],
							(0 != (res[2] & (1<<2))) ? "FIFO is full":"",
							(0 != (res[2] & (1<<1))) ? "OON IRQ":"",
							(0 != gn_irq) ? "GN IRQ":"");

				if (0 != gn_irq)		// If GN IRQ is set, then handle GN
				{
					if (0 == handleGN(handle, chipId, (uint8_t *)found_nonce, &micro_job_id, &data))
					{
						for (int i=0; i<4; i++)
						{
							if((micro_job_id & (1<<i)) != 0)
							{
								memcpy(&data, &(data.vmask_001[(1<<i)]), 4);
								if (!submit_nonce(&data, found_nonce[i])) {
									NxDbgMsg(NX_DBG_ERR, "%5s Failed: invalid nonce 0x%08x\n", "", found_nonce[i]);
									hService->bExitWorkLoop = 1;
									break;
								} else {
									NxDbgMsg(NX_DBG_INFO, "%5s Succeed: valid nonce 0x%08x\n", "", found_nonce[i]);
								}
							}
						}
					}
				} else {			// If GN IRQ is not set, then go to check OON
					NxDbgMsg(NX_DBG_INFO, "%5s === H/W GN occured but GN_IRQ value is not set!!!\n", "");
				}
			}
		}

		if (0 == Btc08GpioGetValue(handle, GPIO_TYPE_OON))	// Check OON
		{
			NxDbgMsg(NX_DBG_INFO, "=== OON IRQ!!! ===\n");
			Btc08ClearOON(handle, BCAST_CHIP_ID);

			totalProcessedHash += 0x800000000;	//	0x100000000 * 2(works) * 4(asic booster)

			currTime = get_current_ms();
			totalTime = currTime - startTime;

			megaHash = totalProcessedHash / (1000*1000);
			NxDbgMsg(NX_DBG_INFO, "AVG : %.2f MHash/s, Hash = %.2f GH, Time = %.2f sec, delta = %lld msec\n",
					megaHash * 1000. / totalTime, megaHash/1000, totalTime/1000. , currTime - prevTime );

			reportHashResult(hService->pll_freq, hService->currentTemp, (megaHash*1000./totalTime));

			prevTime = currTime;

			// Add new 2 works
			for (int i=0; i<2; i++) {
				AddGoldenJob(hService, &data);
			}
		}
		sched_yield();
	}

	if (hService && hService->hBtc08) {
		Btc08ResetHW(hService->hBtc08, 1);
	}

	return (void*)0xDeadFace;

failure:
	if (hService)
		hService->bExitWorkLoop = 1;

	reportExitReason(reason, mining_log);

	if (hService && hService->hBtc08)
		DestroyBtc08(hService->hBtc08);

	return NULL;
}

/* Monitor Loop */
void *MonitorLoop( void *arg )
{
	int adcCh = 1;
	float mv;
	char reason[1024];
	SERVICE_INFO *hService = (SERVICE_INFO*)arg;

	if (NULL == hService)
		return (void*)0xDeadFace;

	while( !hService->bExitMonitorLoop )
	{
		mv = get_mvolt(adcCh);
		hService->currentTemp = get_temp(mv);
		NxDbgMsg(NX_DBG_INFO, "voltage: %.2f, temperature: %.3f C\n", mv, hService->currentTemp);
		if( hService->currentTemp > hService->tempAlertValue )
		{
			DeinitializeWorkLoop(hService);
			snprintf(reason, sizeof(reason), "test stopped due to high temperature: %.3f > %.2f C\n",
						hService->currentTemp, hService->tempAlertValue);
			reportExitReason(reason, mining_log);
			break;
		}
		usleep(1000 * 1000);
	}

	return (void*)0xDeadFace;
}

static SERVICE_INFO *StartService( float max_temp, int freq )
{
	SERVICE_INFO *hService;

	hService = (SERVICE_INFO *)malloc(sizeof(SERVICE_INFO));
	if( !hService )
		return NULL;

	pthread_mutex_lock( &hServiceLock );

	memset( hService, 0, sizeof(SERVICE_INFO) );

	hService->pll_freq = freq;
	hService->tempAlertValue = max_temp;
	hService->bExitWorkLoop = 0;
	hService->bExitMonitorLoop = 0;

	//	Monitor Loop
	if( pthread_create( &hService->hMonThread, NULL, MonitorLoop, hService ) )
	{
		hService->bExitMonitorLoop = 1;
		NxDbgMsg(NX_DBG_ERR, "failed to create MonitorLoop thread\n");
		goto ERROR_EXIT;
	}

	//	Work Loop
	if( pthread_create( &hService->hWorkThread, NULL, WorkLoop, hService ) )
	{
		hService->bExitWorkLoop = 1;
		NxDbgMsg(NX_DBG_ERR, "failed to create WorkLoop thread\n");
		goto ERROR_EXIT;
	}

	pthread_mutex_unlock( &hServiceLock );
	return hService;

ERROR_EXIT:
	if( !hService->bExitWorkLoop )
	{
		hService->bExitWorkLoop = 1;
	}

	if( !hService->bExitMonitorLoop )
	{
		hService->bExitMonitorLoop = 1;
	}

	free( hService );
	pthread_mutex_unlock( &hServiceLock );
	return hService;
}

static void StopService( SERVICE_INFO *hService )
{
	pthread_mutex_lock( &hServiceLock );

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
		NxDbgMsg(NX_DBG_INFO, "hService is freed");
	}
	pthread_mutex_unlock( &hServiceLock );
}

static void ChipSortingBIST(int pll_freq)
{
	uint8_t *ret;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	int adcCh = 1;
	int freq = 0;
	float temperature;
	int active_chips = 0;
	char reason[1024] = {0,};

	BTC08_HANDLE handle = CreateBtc08(0);

	Btc08ResetHW(handle, 1);
	Btc08ResetHW(handle, 0);

	// seq1. read the number of chips
	handle->numChips = Btc08AutoAddress(handle);
	if (handle->numChips < 1 || handle->numChips > MAX_CHIP_NUM) {
		snprintf(reason, sizeof(reason), "test failed due to spi err. wrong number of chips:%d\n", handle->numChips);
		goto failure;
	}

	for (int chipId = 1; chipId <= handle->numChips; chipId++) {
		Btc08ReadId(handle, chipId, res, res_size);
		if (res[3] != chipId) {
			memset(reason, 0, sizeof(char)*512);
			snprintf(reason, sizeof(reason), "test failed due to spi err. wrong chipId:(%d != %d)\n", res[3], chipId);
			goto failure;
		}
	}

	Btc08SetDisable (handle, BCAST_CHIP_ID, golden_enable);

	if (pll_freq != 0)
	{
		freq = pll_freq;

		// seq2. set(change) pll to each chip
		SetPllFreq(handle, freq);

		// seq3. read the temperature
		temperature = get_temp(get_mvolt(adcCh));
		if (temperature > TEMPERATURE_THREASHOLD) {
			memset(reason, 0, sizeof(char)*512);
			snprintf(reason, sizeof(reason), "test stopped due to high temperature: %.3f\n", temperature);
			goto failure;
		}

		// seq4. run BIST
		RunBist(handle);

		// seq6. report result (pll freq, temperature, number of cores)
		reportBistResult(freq, temperature, handle);
	}
	else
	{
		for (int pll_idx = 0; pll_idx < NUM_PLL_SET; pll_idx++)
		{
			freq = GetPllIdx2Freq(pll_idx);

			// seq2. set(change) pll to each chip
			SetPllFreq(handle, freq);

			// seq3. read the temperature
			temperature = get_temp(get_mvolt(adcCh));
			if (temperature > TEMPERATURE_THREASHOLD) {
				memset(reason, 0, sizeof(char)*512);
				snprintf(reason, sizeof(reason), "test stopped due to high temperature: %.3f\n", temperature);
				goto failure;
			}

			// seq4. run BIST
			RunBist(handle);

			// seq6. report result (pll freq, temperature, number of cores)
			reportBistResult(freq, temperature, handle);
		}
	}

	// seq7. report with the reason and exit
	reportExitReason("\ntest done\n", bist_log);
	DestroyBtc08(handle);
	return;

failure:
	reportExitReason(reason, bist_log);
	DestroyBtc08(handle);
	return;
}

static int init_log_file(int type)
{
	struct tm *timenow;

	time_t now = time(NULL);
	timenow = gmtime(&now);

	if (type == 0)	// bist
	{
		strftime(bist_logpath, sizeof(bist_logpath),
			"/home/root/bist_%Y-%m-%d_%H:%M:%S.log", timenow);
		bist_log = fopen(bist_logpath, "w");
		if(!bist_log) {
			NxDbgMsg(NX_DBG_ERR, "failed to open %s", bist_logpath);
			return -1;
		}
	}
	else			// mining
	{
		strftime(mining_logpath, sizeof(mining_logpath),
			"/home/root/mining_%Y-%m-%d_%H:%M:%S.log", timenow);
		mining_log = fopen(mining_logpath, "w");
		if(!mining_log) {
			NxDbgMsg(NX_DBG_ERR, "failed to open %s", mining_logpath);
			return -1;
		}
	}

	return 0;
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
			// prepare mining log file
			init_log_file(1);
			if( NULL == hService )
			{
				float max_temp = 100.0;
				int   pll_freq = 300;
				if( cmdCnt > 1 ) {
					max_temp = strtol(cmd[1], 0, 10);
					if (max_temp > 100.00) {
						max_temp = 100.00;
					}
				}
				if (cmdCnt > 2) {
					pll_freq = strtol(cmd[2], 0, 10);
					if (pll_freq > 1000) {
						pll_freq = 1000;
					}
				}
				printf("max_temp = %.3f pll_freq = %d\n", max_temp, pll_freq);
				hService = StartService(max_temp, pll_freq);
			}
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
		else if( !strcasecmp(cmd[0], "3") )
		{
			int pll_freq = 0;
			if (cmdCnt > 1) {
				pll_freq = strtol(cmd[1], 0, 10);
				if (pll_freq > 1000) {
					pll_freq = 1000;
				}
			}
			printf("pll_freq = %d\n", pll_freq);
			// prepare bist log file
			init_log_file(0);
			ChipSortingBIST(pll_freq);
			fclose(bist_log);
		}
	}

	if (NULL != mining_log) {
		fclose(mining_log);
	}
	if (NULL != bist_log) {
		fclose(bist_log);
	}
}
