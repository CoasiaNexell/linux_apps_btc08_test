#include <stdio.h>
#include <strings.h>
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

int main( int argc, char *argv[] )
{
	static char cmdStr[NX_SHELL_MAX_ARG * NX_SHELL_MAX_STR];
	static char cmd[NX_SHELL_MAX_ARG][NX_SHELL_MAX_STR];
	int cmdCnt;

#ifndef USE_BTC08_FPGA
	setup_hashboard_gpio();
	if ((plug_status_0 != 1) && (plug_status_1 != 1))
	{
		printf("Not connected!!!\n");
		return;
	}
#endif

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

	return 0;
}
