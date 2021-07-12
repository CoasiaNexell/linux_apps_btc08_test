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

// #define NX_DBG_OFF
#ifdef NX_DTAG
#undef NX_DTAG
#endif
#define NX_DTAG "[MAIN]"
#include "NX_DbgMsg.h"

static void l1_command_liist()
{
	printf("\n\n");
	printf("===== Main Command Loop =====\n");
	printf("  1. Single Command\n");
	printf("  2. Simple Work\n");
	printf("  3. Scenario Test\n");
	printf("  4. Function Test\n");
	printf("-----------------------------\n");
	printf("  q. quit\n");
	printf("=============================\n");
}

void setup_hashboard_gpio()
{
	GPIO_HANDLE hPlug0  = NULL;
	GPIO_HANDLE hBdDET0 = NULL;
	GPIO_HANDLE hPwrEn0 = NULL;
	GPIO_HANDLE hReset0 = NULL;
	GPIO_HANDLE hPlug1  = NULL;
	GPIO_HANDLE hBdDET1 = NULL;
	GPIO_HANDLE hPwrEn1 = NULL;
	GPIO_HANDLE hReset1 = NULL;

	// Export gpio
	hPlug0  = CreateGpio(GPIO_HASH0_PLUG);
	hBdDET0 = CreateGpio(GPIO_HASH0_BODDET);
	hPwrEn0 = CreateGpio(GPIO_HASH0_PWREN);

	hPlug1  = CreateGpio(GPIO_HASH1_PLUG);
	hBdDET1 = CreateGpio(GPIO_HASH1_BODDET);
	hPwrEn1 = CreateGpio(GPIO_HASH1_PWREN);

	hReset0  = CreateGpio(GPIO_RESET_0);
	hReset1  = CreateGpio(GPIO_RESET_1);

	// Set direction
	GpioSetDirection(hPlug0,  GPIO_DIRECTION_IN);
	GpioSetDirection(hBdDET0, GPIO_DIRECTION_IN);
	GpioSetDirection(hPwrEn0, GPIO_DIRECTION_OUT);

	GpioSetDirection(hPlug1,  GPIO_DIRECTION_IN);
	GpioSetDirection(hBdDET1, GPIO_DIRECTION_IN);
	GpioSetDirection(hPwrEn1, GPIO_DIRECTION_OUT);

	GpioSetDirection(hReset0, GPIO_DIRECTION_OUT);
	GpioSetDirection(hReset1, GPIO_DIRECTION_OUT);

	// Read plug_status and board_type, FAN ON
	plug_status_0 = GpioGetValue(hPlug0);
	board_type_0  = GpioGetValue(hBdDET0);
	GpioSetValue(hPwrEn0, 1);

	plug_status_1 = GpioGetValue(hPlug1);
	board_type_1  = GpioGetValue(hBdDET1);
	GpioSetValue(hPwrEn1, 1);

	GpioSetValue(hReset0, 0);
	GpioSetValue(hReset1, 0);

	printf("Hash0: connection status(%s), board_type(%s), Reset GPIO(%d)\n",
			(plug_status_0 == 1) ? "Connected":"Removed",
			(board_type_0  == 1) ? "Hash":"VTK",
			GpioGetValue(hReset0));
	printf("Hash1: connection status(%s), board_type(%s), Reset GPIO(%d)\n",
			(plug_status_1 == 1) ? "Connected":"Removed",
			(board_type_1  == 1) ? "Hash":"VTK",
			GpioGetValue(hReset1));

	// Unexport gpio
	if( hPlug0  )	DestroyGpio( hPlug0 );
	if( hBdDET0 )	DestroyGpio( hBdDET0 );
	if( hPwrEn0 )	DestroyGpio( hPwrEn0 );
	if( hReset0 )	DestroyGpio( hReset0 );
	if( hPlug1  )	DestroyGpio( hPlug1 );
	if( hBdDET1 )	DestroyGpio( hBdDET1 );
	if( hPwrEn1 )	DestroyGpio( hPwrEn1 );
	if( hReset1 )	DestroyGpio( hReset1 );
}


void print_usage( char *appname )
{
	printf("\n------------------------------------------------------------------\n");
	printf("usage : %s [options]\n", appname);
	printf(" options : \n");
	printf("   m [mode]          : mode( 1(bist), 2(mining), other(console)\n");
	printf("   f [frequency]     : freqeuncy in MHz (default : 24)\n");
	printf("   c [disable cores] : disable cores (default : 0)\n");
	printf("   d [bitmask]       : disable core bit mask\n");
	printf("   i [interval]      : interval (default: 1sec)\n");
	printf("   r [repeat]        : repeat (default: 1)\n");
	printf("   n [0 or 1]        : 0(short range), 1(full range) (default : 0)\n");
	printf("------------------------------------------------------------------\n");
	printf("example1) auto bist\n");
	printf(" btc08_test -m 3 -i 5 -r 5\n");
	printf("example2) auto bist with one core in different locations\n");
	printf(" btc08_test -m 4 -i 5 -r 5\n");
	printf("------------------------------------------------------------------\n");
}

int main( int argc, char *argv[] )
{
	static char cmdStr[NX_SHELL_MAX_ARG * NX_SHELL_MAX_STR];
	static char cmd[NX_SHELL_MAX_ARG][NX_SHELL_MAX_STR];
	int cmdCnt;
	int opt;

	int mode = -1;			//	Test Mode : -1 (Interlactive Mode)
	int freqM = 24;			//	frequence in MHz : default 24 MHz
	int disCore = 0;		//	disable core
	int testIndex = 0;		//	Test Item : BIST
	int isFullNonce = 0;	//	Nonce Range for testing : 0(short), 1 (full)
	int repeat_cnt = 1;     //  Repeat cnt : 1
	int interval = 1;       //  Interval

	while (-1 != (opt = getopt(argc, argv, "m:f:c:i:n:d:r:h")))
	{
		switch (opt)
		{
		case 'm':	mode = atoi(optarg);			break;
		case 'f':	freqM = atoi(optarg);			break;
		case 'c':	disCore = atoi(optarg);			break;
		case 'n':	isFullNonce = atoi(optarg);		break;
		case 'r':	repeat_cnt = atoi(optarg);		break;
		case 'i':	interval = atoi(optarg);		break;
		case 'd':	gDisableCore = strtol(optarg, NULL, 16); break;
		case 'h':	print_usage(argv[0]);			return 0;
		default:	break;
		}
	}

#ifndef USE_BTC08_FPGA
	setup_hashboard_gpio();
	if ((plug_status_0 != 1) && (plug_status_1 != 1))
	{
		printf("Not connected!!!\n");
		return -1;
	}
#endif

	switch ( mode )
	{
		//	BIST Test Mode
		case 1:
		{
			BTC08_HANDLE handle;
			printf("=====  BIST Test Mode  =====\n");
			printf("  Disable Core : %d ea\n", disCore);
			printf("  Freqeyncy    : %dMHz\n", freqM );
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
			TestBist( handle, disCore, freqM, 0);
			//	Make Reset State
			Btc08ResetHW( handle, 1 );
			break;
		}

		//	mining test
		case 2:
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
			printf("===== Mining Test Mode =====\n");
			printf("  Disable Core : %d ea\n", disCore);
			printf("  Freqeyncy    : %dMHz\n", freqM );
			printf("  Nonce        : %s\n",  isFullNonce?"Full":"Short");
			printf("  Disable Mask : 0x%08x\n", gDisableCore);
			printf("============================\n");
			TestDisableCore( handle, disCore, freqM, isFullNonce, 0 );

			//	Make Reset State
			Btc08ResetHW( handle, 1 );
			break;
		}

		//	BIST & Disable Cores Test Mode
		case 3:
		{
			BTC08_HANDLE handle;
			uint8_t res[32] = {0x00,};
			unsigned int res_size = sizeof(res)/sizeof(res[0]);

			printf("=====  BIST & Disable Core Test Mode  =====\n");
			printf("  Interval     : %dsec\n", interval );
			printf("  Repeat Cnt   : %d\n",    repeat_cnt );
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
				return -1;
			}

			Btc08ResetHW( handle, 1 );
			for (int pll_idx = 0; pll_idx < NUM_PLL_SET; pll_idx++)
			{
				int freq = GetPllIdx2Freq(pll_idx);
				fprintf(fd, "%d\t", freq);

				for (int core_num=1; core_num <= BTC08_NUM_CORES; core_num++)
				{
					for (int cnt = 0; cnt < repeat_cnt; cnt++)
					{
						NxDbgMsg(NX_DBG_INFO, "freq:%d disable_core:%d(%d/%d)\n",
							freq, (BTC08_NUM_CORES - core_num), (cnt+1), repeat_cnt);
						TestBist(handle, (BTC08_NUM_CORES - core_num), freq, 0);
						Btc08ReadDisable(handle, 1, res, res_size);
						//HexDump2("read_disable", res+28, 4);
						fprintf(fd, "%d(0x%02x%02x%02x%02x) ",
							handle->numCores[0], res[28], res[29], res[30], res[31]);

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
			break;
		}

		//	BIST & Disable 1 Core Test Mode
		case 4:
		{
			BTC08_HANDLE handle;
			uint8_t res[32] = {0x00,};
			unsigned int res_size = sizeof(res)/sizeof(res[0]);

			printf("=====  BIST & Disable 1 Core Test Mode  =====\n");
			printf("  Interval     : %dsec\n", interval );
			printf("  Repeat Cnt   : %d\n",    repeat_cnt );
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
			FILE *fd = fopen("/home/root/disable_1_core.log", "w");
			if (!fd) {
				NxDbgMsg(NX_DBG_ERR, "Failed to open disable_1_core.log");
				return -1;
			}

			Btc08ResetHW( handle, 1 );
			for (int pll_idx = 0; pll_idx < NUM_PLL_SET; pll_idx++)
			{
				int freq = GetPllIdx2Freq(pll_idx);
				fprintf(fd, "%d\t", freq);

				NxDbgMsg(NX_DBG_ERR, "1\n");
				for (int core_idx=0; core_idx < BTC08_NUM_CORES; core_idx++)
				{
					gDisableCore = ~(1 << (core_idx % 8));
					for (int cnt = 0; cnt < repeat_cnt; cnt++)
					{
						NxDbgMsg(NX_DBG_INFO, "freq:%d Disable Mask : 0x%08x 0x%08x(%d/%d)\n",
							freq, (1 << (core_idx % 8)), gDisableCore, (cnt+1), repeat_cnt);
						//	Make Reset State
						TestBist( handle, disCore, freqM, 0);
						// TODO:
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
			break;
		}

		default :
			for( ;; )
			{
				l1_command_liist();
				printf( "cmd > " );
				fgets( cmdStr, NX_SHELL_MAX_ARG*NX_SHELL_MAX_STR - 1, stdin );
				cmdCnt = Shell_GetArgument( cmdStr, cmd );

				//----------------------------------------------------------------------
				if( !strcasecmp(cmd[0], "q") )
				{
					printf("bye bye ~~\n");
					break;
				}
				//----------------------------------------------------------------------
				// Single Command
				else if( !strcasecmp(cmd[0], "1") )
				{
					SingleCommandLoop();
				}
				//----------------------------------------------------------------------
				// Simple Work
				else if( !strcasecmp(cmd[0], "2") )
				{
					SimpleWorkLoop();
				}
				//----------------------------------------------------------------------
				// Scenario Test
				else if( !strcasecmp(cmd[0], "3") )
				{
					ScenarioTestLoop();
				}
				//----------------------------------------------------------------------
				// Function Test
				else if( !strcasecmp(cmd[0], "4") )
				{
					FuntionTestLoop();
				}
			}
		break;
	}

	return 0;
}
