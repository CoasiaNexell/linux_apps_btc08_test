#include <stdio.h>
#include <strings.h>
#include "TestFunction.h"
#include "Utils.h"
#include "GpioControl.h"

#include "SingleCommand.h"
#include "SimpleWork.h"
#include "ScenarioTest.h"

// #define NX_DBG_OFF
#ifdef NX_DTAG
#undef NX_DTAG
#endif
#define NX_DTAG "[MAIN]"
#include "NX_DbgMsg.h"

#define GPIO_HASH0_PLUG         (24)		// High: Hash0 connected, Low: Hash0 removed
#define GPIO_HASH0_BODDET       (20)		// High: Hash0, Low: VTK
#define GPIO_HASH0_PWREN         (0)		// High: FAN ON, Low : FAN OFF

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
	int plug_status = 0;
	int board_type = 0;

	GPIO_HANDLE hPlug  = NULL;
	GPIO_HANDLE hBdDET = NULL;
	GPIO_HANDLE hPwrEn = NULL;

	hPlug  = CreateGpio(GPIO_HASH0_PLUG);
	hBdDET = CreateGpio(GPIO_HASH0_BODDET);
	hPwrEn = CreateGpio(GPIO_HASH0_PWREN);

	GpioSetDirection(hPlug,  GPIO_DIRECTION_IN);
	GpioSetDirection(hBdDET, GPIO_DIRECTION_IN);
	GpioSetDirection(hPwrEn, GPIO_DIRECTION_OUT);

	plug_status = GpioGetValue(hPlug);
	board_type  = GpioGetValue(hBdDET);
	GpioSetValue(hPwrEn, 1);

	printf("Hash0: connection status(%s), board_type(%s)\n",
			(plug_status == 1) ? "Connected":"Removed",
			(board_type  == 1) ? "Hash":"VTK");

	if( hPlug  )	DestroyGpio( hPlug );
	if( hBdDET )	DestroyGpio( hBdDET );
	if( hPwrEn )	DestroyGpio( hPwrEn );
}

int main( int argc, char *argv[] )
{
	static char cmdStr[NX_SHELL_MAX_ARG * NX_SHELL_MAX_STR];
	static char cmd[NX_SHELL_MAX_ARG][NX_SHELL_MAX_STR];
	int cmdCnt;

	setup_hashboard_gpio();

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
