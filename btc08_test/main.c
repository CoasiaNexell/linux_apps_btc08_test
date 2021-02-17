#include <stdio.h>
#include "TestFunction.h"

// #define NX_DBG_OFF
#ifdef NX_DTAG
#undef NX_DTAG
#endif
#define NX_DTAG "[MAIN]"
#include "NX_DbgMsg.h"

void print_command_list()
{
	printf("============================\n");
	printf("  1. Auto Address \n");
	printf("  2. Test Bist \n");
	printf("  3. Test Work \n");
	printf("  6. Start Temperature Monitor \n");
	printf("  7. Stop Temperature Monitor \n");
	printf("============================\n");
}


int main( int argc, char *argv[] )
{
	int test_id;
	for( ;; )
	{
		print_command_list();
		printf( "> " );
		scanf("%d", &test_id);
		if (test_id > 9) {
			print_command_list();
			break;
		}

		switch( test_id )
		{
			case 1:
				ResetAutoAddress();
				break;
			case 2:
				TestBist();
				break;
			case 3:
				TestWork();
				break;
			case 6:
				StartMonTempThread();
				break;
			case 7:
				ShutdownMonTempThread();
				break;
			default:
				printf("Unknown command = %d\n", test_id);
				break;
		}
	}

	return 0;
}