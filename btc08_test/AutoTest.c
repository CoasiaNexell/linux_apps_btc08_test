#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>	//	getopt
#include <stdlib.h>	//	atoi
#include <getopt.h>
#include "TestFunction.h"
#include "Utils.h"
#include "GpioControl.h"
#include "Btc08.h"

#include "SingleCommand.h"
#include "SimpleWork.h"
#include "ScenarioTest.h"
#include "AutoTest.h"

#ifdef NX_DTAG
#undef NX_DTAG
#endif
#define NX_DTAG "[AUTOTEST]"
#include "NX_DbgMsg.h"



static GPIO_HANDLE gstGpioFanSen = NULL;	//	GPIO D13
static GPIO_HANDLE gstGpioPWM0 = NULL;		//	GPIO D1

static void InitDbgGpio()
{
	if( !gstGpioFanSen )
	{
		gstGpioFanSen = CreateGpio( GPIOD13 );
		GpioSetDirection(gstGpioFanSen, GPIO_DIRECTION_OUT);
	}
}

void DbgGpioOn()
{
	if( !gstGpioFanSen )
	{
		InitDbgGpio();
	}
	GpioSetValue(gstGpioFanSen, 1);
}

void DbgGpioOff()
{
	if( !gstGpioFanSen )
	{
		InitDbgGpio();
	}
	GpioSetValue(gstGpioFanSen, 0);
}

static void InitDbgGpio2()
{
	if( !gstGpioPWM0 )
	{
		gstGpioPWM0 = CreateGpio( GPIOD1 );
		GpioSetDirection(gstGpioPWM0, GPIO_DIRECTION_OUT);
	}
}

void DbgGpioOn2()
{
	if( !gstGpioPWM0 )
	{
		InitDbgGpio2();
	}
	GpioSetValue(gstGpioPWM0, 1);
}

void DbgGpioOff2()
{
	if( !gstGpioPWM0 )
	{
		InitDbgGpio2();
	}
	GpioSetValue(gstGpioPWM0, 0);
}



void BistWithDisable( int interval, int repeatCnt )
{
	BTC08_HANDLE handle;
	uint8_t res[32] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	int testFreq[] = {300, 400, 500, 600, 700, 800, 900, 1000};
	//int testFreq[] = {24, 50, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
	int freqSize = sizeof(testFreq) / sizeof(int);

	printf("=====  BIST & Disable Core Test Mode  =====\n");
	printf("  Interval     : %dsec\n", interval );
	printf("  Repeat Cnt   : %d\n",    repeatCnt );
	printf("============================\n");
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
	FILE *fd = fopen("/home/root/disabled_core.log", "w");
	if (!fd) {
		NxDbgMsg(NX_DBG_ERR, "Failed to open disabled_core.log");
		exit(-1);
	}

	Btc08ResetHW( handle, 1 );
	for (int pll_idx = 0; pll_idx < freqSize; pll_idx++)
	{
		int freq = testFreq[pll_idx];
		fprintf(fd, "%d\t", freq);

		for (int core_num=1; core_num <= BTC08_NUM_CORES; core_num++)
		{
			for (int cnt = 0; cnt < repeatCnt; cnt++)
			{
				NxDbgMsg(NX_DBG_INFO, "\n## freq:%d disable_core:%d(%d/%d)\n", freq, (BTC08_NUM_CORES - core_num), (cnt+1), repeatCnt);
				//	TestBist
				TestBist(handle, (BTC08_NUM_CORES - core_num), freq, 0, 200);
				//	ReadDisable
				Btc08ReadDisable(handle, 1, res, res_size);

				//	Output file write
				fprintf(fd, "%d(0x%02x%02x%02x%02x) ", handle->numCores[0], res[28], res[29], res[30], res[31]);

				//	Reset
				Btc08ResetHW( handle, 1 );
				usleep(interval * 1000 * 1000);
			}
			fprintf(fd, "\t");
			fflush(fd);
		}
		fprintf(fd, "\n");
		sync();
	}
	Btc08ResetHW( handle, 1 );
	fclose(fd);
}


void EnableOneCorePosition( int interval, int repeatCnt )
{
	BTC08_HANDLE handle;
	int isFullNonce = 0;
	int testFreq[] = {24, 50, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
	int freqSize = sizeof(testFreq) / sizeof(int);
	unsigned int disableCoreMask = 0xffffffff;
	int result;

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
	FILE *fd = fopen("/home/root/one_core_position.log", "w");
	if (!fd) {
		NxDbgMsg(NX_DBG_ERR, "Failed to open one_core_position.log");
		exit(-1);
	}

	for (int pll_idx = 0; pll_idx < freqSize; pll_idx++)
	{
		int freq = testFreq[pll_idx];
		fprintf(fd, "%d\t", freq);
		for (int core_num=1; core_num <= BTC08_NUM_CORES; core_num++)
		{
			int successCnt = 0;
			int bistCnt = 0;
			gDisableCore = disableCoreMask & (~(1<<(core_num-1)));
			for (int cnt = 0; cnt < repeatCnt; cnt++)
			{
				NxDbgMsg(NX_DBG_INFO, "\n## Mining Test (%d/%d) : freq (%d), coreMask(0x%08x)\n", cnt, repeatCnt, freq, gDisableCore);
				result = TestDisableCore( handle, 29, freq, isFullNonce, 0, 0 );
				if( 0 == result ){
					successCnt++;
					bistCnt++;
				}
				else if( -1 == result ){
					bistCnt++;
				}
				else{
					//	bist error & pll lock error
				}
				Btc08ResetHW( handle, 1 );
				usleep(interval * 1000 * 1000);
			}
			fprintf(fd, "%d/%d %d/%d\t", bistCnt, repeatCnt, successCnt, repeatCnt);
			fflush(fd);
		}
		fprintf(fd, "\n");
		sync();
	}
	fclose(fd);

	//	Make Reset State
	Btc08ResetHW( handle, 1 );
}



void AutoTestMining( int interval, int repeatCnt )
{
	BTC08_HANDLE handle;
	int isFullNonce = 0;
	int testFreq[] = {24, 50, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
	int freqSize = sizeof(testFreq) / sizeof(int);
	int result;

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
	FILE *fd = fopen("/home/root/auto_test_mining.log", "w");
	if (!fd) {
		NxDbgMsg(NX_DBG_ERR, "Failed to open auto_test_mining.log");
		exit(-1);
	}

	for (int pll_idx = 0; pll_idx < freqSize; pll_idx++)
	{
		int freq = testFreq[pll_idx];
		fprintf(fd, "%d\t", freq);
		for (int core_num=1; core_num <= BTC08_NUM_CORES; core_num++)
		{
			int successCnt = 0;
			for (int cnt = 0; cnt < repeatCnt; cnt++)
			{
				NxDbgMsg(NX_DBG_INFO, "\n\n## Mining Test (%d/%d) : freq (%d)\n", cnt, repeatCnt, freq, gDisableCore);
				result = TestDisableCore( handle, BTC08_NUM_CORES - core_num, freq, isFullNonce, 0, 0 );
				if( 0 == result ){
					successCnt++;
				}
				Btc08ResetHW( handle, 1 );
				usleep(interval * 1000 * 1000);
			}
			fprintf(fd, "%d/%d\t", successCnt, repeatCnt);
			fflush(fd);
		}
		fprintf(fd, "\n");
		sync();
	}
	fclose(fd);

	//	Make Reset State
	Btc08ResetHW( handle, 1 );
}


void DebugPowerBIST( int freq, int interval )
{
	BTC08_HANDLE handle;
	printf("=====  Debug Power  =====\n");
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

	Btc08ResetHW( handle, 1 );
	while(1)
	{
		DbgGpioOff();
		TestBist(handle, 0, freq, 0, 200);
		usleep(interval * 1000 * 1000);
	}
	Btc08ResetHW( handle, 1 );
}

void MiningWithoutBist( uint8_t disable_core_num, uint32_t pll_freq,
		uint8_t is_full_nonce, uint8_t fault_chip_id, uint8_t is_infinite_mining )
{
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

#if 0
	while(1)
	{
		DbgGpioOff();
		TestMiningWithoutBist( handle, 0, freq, isFullNonce, 0 );
		Btc08ResetHW( handle, 1 );
		usleep(interval * 1000 * 1000);
	}
#else
	TestMiningWithoutBist( handle, disable_core_num, pll_freq, is_full_nonce,
			fault_chip_id, is_infinite_mining );

#endif

	//	Make Reset State
	Btc08ResetHW( handle, 1 );
}