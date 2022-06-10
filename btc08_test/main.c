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
	printf("   m [mode]             : mode (1(bist), 2(mining), other(console))\n");
	printf("   f [frequency]        : freqeuncy in MHz (default : 24)\n");
	printf("   c [disable cores]    : disable cores (default : 0)\n");
	printf("   d [bitmask]          : disable core bit mask\n");
	printf("   i [interval]         : interval (default: 1sec)\n");
	printf("   r [repeat]           : repeat (default: 1)\n");
	printf("   l [infinite mining]  : infinite mining (default: 0)\n");
	printf("   n [0 or 1]           : 0(short range), 1(full range) (default : 0)\n");
	printf("   t [delay]            : delay after h/w reset (default : 200ms)\n");
	printf("   L [lastChipid]       : last chip id number\n");
	printf("   D [ASIC data]        : 0(different), 1(same)\n");
	printf("------------------------------------------------------------------\n");
	printf("example 1) auto bist\n");
	printf(" btc08_test -m 3 -i 3 -r 5\n");
	printf("example 2) 1 core poistion\n");
	printf(" btc08_test -m 4 -i 3 -r 3\n");
	printf("example 3) Auto mining test\n");
	printf(" btc08_test -m 5 -i 3 -r 3\n");
	printf("example 4) Mining test without BIST\n");
	printf(" btc08_test -m 7 -l 1\n");
	printf("example 5) Reset And Bist\n");
	printf(" btc08_test -m 1 -t 3000 -c 0 -f 24\n");
	printf("example 6) Different ASIC Data Test\n");
	printf(" btc08_test -m 8 -L 1 -n 1 -c 0 -D 0\n");
	printf("------------------------------------------------------------------\n");
}

int main( int argc, char *argv[] )
{
	static char cmdStr[NX_SHELL_MAX_ARG * NX_SHELL_MAX_STR];
	static char cmd[NX_SHELL_MAX_ARG][NX_SHELL_MAX_STR];
	int cmdCnt;
	int opt;

	int mode = -1;				//	Test Mode : -1 (Interlactive Mode)
	int freqM = 24;				//	frequence in MHz : default 24 MHz
	int disCore = 0;			//	disable core
	int testIndex = 0;			//	Test Item : BIST
	int isFullNonce = 0;		//	Nonce Range for testing : 0(short), 1 (full)
	int repeat_cnt = 1;     	//  Repeat cnt : 1
	int interval = 1;       	//  Interval
	int isInfiniteMining = 0;	// Infinite Mining: 0(default 4 jobs)
	int delay = 200;			//	Set Delay
	int last_chipId = 1;		//	Set Last ChipID
	int is_diff_data = 0;		//	Different ASIC Data Test : 0(same), 1(Different)

	while (-1 != (opt = getopt(argc, argv, "m:f:c:i:n:d:r:l:t:L:D:h")))
	{
		switch (opt)
		{
		case 'm':	mode = atoi(optarg);						break;
		case 'f':	freqM = atoi(optarg);						break;
		case 'c':	disCore = atoi(optarg);						break;
		case 'n':	isFullNonce = atoi(optarg);					break;
		case 'r':	repeat_cnt = atoi(optarg);					break;
		case 'i':	interval = atoi(optarg);					break;
		case 'd':	gDisableCore = strtol(optarg, NULL, 16);	break;
		case 'l':	isInfiniteMining = atoi(optarg);			break;
		case 't':	delay = atoi(optarg);						break;
		case 'L':	last_chipId = atoi(optarg);					break;
		case 'D':	is_diff_data = atoi(optarg);				break;
		case 'h':	print_usage(argv[0]);						return 0;
		default:												break;
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
			printf("  Reset Delay  : %dms\n", delay );
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
			TestBist( handle, disCore, freqM, 0, delay);
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
			printf("========== Mining Test Mode ==========\n");
			printf("  Disable Core    : %d ea\n", disCore);
			printf("  Freqeyncy       : %dMHz\n", freqM );
			printf("  Nonce           : %s\n",  isFullNonce?"Full":"Short");
			printf("  Disable Mask    : 0x%08x\n", gDisableCore);
			printf("  Infinite Mining : %d\n", isInfiniteMining);
			printf("======================================\n");
			TestDisableCore( handle, disCore, freqM, isFullNonce, 0, isInfiniteMining );

			//	Make Reset State
			Btc08ResetHW( handle, 1 );
			break;
		}

		//	BIST & Disable Core Test Mode
		case 3:
		{
			BistWithDisable(interval, repeat_cnt);
			break;
		}

		//	Enable 1 Core Position Test
		case 4:
		{
			EnableOneCorePosition( interval, repeat_cnt );
			break;
		}

		//
		case 5:
		{
			AutoTestMining( interval, repeat_cnt );
			break;
		}

		case 6:
		{
			DebugPowerBIST(freqM, interval);
			break;
		}

		case 7:
		{
			MiningWithoutBist( freqM, isInfiniteMining );
			break;
		}

		// Different ASIC Data Test
		case 8:
		{
			printf("========== Asic Test Mode ==========\n");
			printf("  Last ChipID    : %d\n", last_chipId);
			printf("  Disable Core   : %dea\n", disCore);
			printf("  Disable Mask   : 0x%08x\n", gDisableCore);
			printf("  Freqeyncy      : %dMHz\n", freqM );
			printf("  Nonce          : %s\n",  isFullNonce?"Full":"Short");
			printf("  ASIC Data      : %s\n", is_diff_data?"Same":"Different");
			printf("======================================\n");

			TestAsic(last_chipId, disCore, isFullNonce, is_diff_data, freqM);
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
