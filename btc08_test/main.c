#include <stdio.h>
#include <strings.h>
#include "TestFunction.h"
#include "Utils.h"

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

int main( int argc, char *argv[] )
{
	static char cmdStr[NX_SHELL_MAX_ARG * NX_SHELL_MAX_STR];
	static char cmd[NX_SHELL_MAX_ARG][NX_SHELL_MAX_STR];
	int cmdCnt;
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
