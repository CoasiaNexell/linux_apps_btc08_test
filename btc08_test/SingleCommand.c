#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "Utils.h"
#include "Btc08.h"

#include "TestVector.h"

#ifdef NX_DTAG
#undef NX_DTAG
#endif
#define NX_DTAG "[SingleCommand]"
#include "NX_DbgMsg.h"


static void singlecommand_command_list()
{
	printf("\n\n");
	printf("====== Single Command =======\n");
	printf("  1. H/W Reset\n");
	printf("  2. Reset and Auto Address \n");
	printf("  3. Reset and TestBist\n");
	printf("  4. Set the chip to the last chip\n");
	printf("    ex > 4 [chipId(1~3)]\n");
	printf("  5. Disable chip\n");
	printf("    ex > 5 [chipId(1~3)] [core index(0~255)]\n");
	printf("-----------------------------\n");
	printf("  q. quit\n");
	printf("=============================\n");
}


static int HWReset( BTC08_HANDLE handle )
{
	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );
	NxDbgMsg( NX_DBG_INFO, "Hardware Reset Done.\n" );
	return 0;
}


static int ResetAutoAddress( BTC08_HANDLE handle )
{
	int numChips;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	//	create BTC08 instance into index 0. ( /dev/spidev0.0 )
	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	numChips = Btc08AutoAddress(handle);
	NxDbgMsg( NX_DBG_INFO, "Number of Chips = %d\n", numChips );
	for (int chipId = 1; chipId <= numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
		NxDbgMsg( NX_DBG_INFO, "ChipId = %d, Number of jobs = %d\n",
					chipId, (res[2]&7) );
	}

	return 0;
}

/* Used to disable the chips */
void TestLastChip( BTC08_HANDLE handle, uint8_t last_chipId )
{
	int numChips;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	NxDbgMsg( NX_DBG_INFO, "=== AUTO ADDRESS ==\n");
	numChips = Btc08AutoAddress(handle);
	NxDbgMsg( NX_DBG_INFO, "%5s Number of Chips = %d\n", "", numChips );
	for (int chipId = 1; chipId <= numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
		NxDbgMsg( NX_DBG_INFO, "%5s ChipId = %d, Number of jobs = %d\n",
					"", chipId, (res[2]&7) );
	}

	NxDbgMsg( NX_DBG_INFO, "=== SET chipId #%d as the LAST CHIP ==\n", last_chipId );
	Btc08SetControl(handle, last_chipId, LAST_CHIP);

	NxDbgMsg( NX_DBG_INFO, "=== AUTO ADDRESS ==\n");
	numChips = Btc08AutoAddress(handle);
	NxDbgMsg( NX_DBG_INFO, "%5s Number of Chips = %d\n", "", numChips );
	for (int chipId = 1; chipId <= numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
		NxDbgMsg( NX_DBG_INFO, "%5s ChipId = %d, Number of jobs = %d\n",
					"", chipId, (res[2]&7) );
	}
}

/* Disable core */
void TestDisableCore( BTC08_HANDLE handle, uint8_t chip_id, uint8_t disable_core_idx )
{
	int numChips;
	uint8_t *ret;
	uint8_t res[4] = {0x00,};
	int numCores[MAX_CHIP_NUM] = {0, };
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	uint8_t disable_cores[32] = {0x00,};

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	NxDbgMsg( NX_DBG_INFO, "=== AUTO ADDRESS ==\n");
	numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "%5s Number of Chips = %d\n", "", numChips);

	NxDbgMsg( NX_DBG_INFO, "=== RUN BIST with all cores enabled ==\n");
	// Enable all cores
	Btc08SetDisable (handle, BCAST_CHIP_ID, golden_enable);

	// Write param for BIST
	Btc08WriteParam (handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
	Btc08WriteNonce (handle, BCAST_CHIP_ID, golden_nonce, golden_nonce);

	// Run BIST
	Btc08RunBist    (handle, default_golden_hash, default_golden_hash, default_golden_hash, default_golden_hash);

	for (int chipId = 1; chipId <= numChips; chipId++)
	{
		// If it's not BUSY status, read the number of cores in next READ_BIST
		for (int i=0; i<10; i++) {
			ret = Btc08ReadBist(handle, chipId);
			if ( (ret[0] & 1) == 0 )
				break;
			else
				NxDbgMsg( NX_DBG_INFO, "%5s ChipId = %d, Status = %s, Number of cores = %d\n",
						"", chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );

			usleep( 300 );
		}

		ret = Btc08ReadBist(handle, chipId);

		numCores[chipId] = ret[1];
		NxDbgMsg( NX_DBG_INFO, "%5s ChipId = %d, Status = %s, Number of cores = %d\n",
					"", chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );
	}

	for (int chipId = 1; chipId <= numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
		NxDbgMsg( NX_DBG_INFO, "%5s ChipId = %d, Number of jobs = %d\n",
					"", chipId, (res[2]&7) );
	}

	NxDbgMsg( NX_DBG_INFO, "=== RUN BIST with the %dth core disabled ==\n", (disable_core_idx+1));
	// Mark the core to disable
	for (int i=0; i<32; i++) {
		disable_cores[31-(disable_core_idx)/8] = (1 << (disable_core_idx % 8));
		NxDbgMsg( NX_DBG_DEBUG, "==> disable_cores[%d] : %02x\n", i, disable_cores[i]);
	}

	// Disable the specific core in the chip
	Btc08SetDisable (handle, chip_id, disable_cores);

	// Write param for BIST
	Btc08WriteParam (handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
	Btc08WriteNonce (handle, BCAST_CHIP_ID, golden_nonce, golden_nonce);

	// Run BIST
	Btc08RunBist    (handle, default_golden_hash, default_golden_hash, default_golden_hash, default_golden_hash);

	for (int chipId = 1; chipId <= numChips; chipId++)
	{
		// If it's not BUSY status, read the number of cores in next READ_BIST
		for (int i=0; i<10; i++) {
			ret = Btc08ReadBist(handle, chipId);
			if ( (ret[0] & 1) == 0 )
				break;
			else
				NxDbgMsg( NX_DBG_INFO, "%5s ChipId = %d, Status = %s, Number of cores = %d\n",
						"", chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );

			usleep( 300 );
		}

		ret = Btc08ReadBist(handle, chipId);

		numCores[chipId] = ret[1];
		NxDbgMsg( NX_DBG_INFO, "%5s ChipId = %d, Status = %s, Number of cores = %d\n",
					"", chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );
	}

	for (int chipId = 1; chipId <= numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
		NxDbgMsg( NX_DBG_INFO, "%5s ChipId = %d, Number of jobs = %d\n",
					"", chipId, (res[2]&7) );
	}
}

void TestBist( BTC08_HANDLE handle )
{
	int numChips;
	uint8_t *ret;
	uint8_t res[4] = {0x00,};
	int numCores[MAX_CHIP_NUM] = {0, };
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	//	create BTC08 instance into index 0. ( /dev/spidev0.0 )

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	NxDbgMsg( NX_DBG_INFO, "=== AUTO ADDRESS ==\n");
	numChips = Btc08AutoAddress(handle);
	NxDbgMsg(NX_DBG_INFO, "%5s Number of Chips = %d\n", "", numChips);
	for (int chipId = 1; chipId <= numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
		NxDbgMsg( NX_DBG_INFO, "%5s ChipId = %d, Number of jobs = %d\n",
					"", chipId, (res[2]&7) );
	}

	// Software reset (TODO: Check FIFO status if it's empty)
	Btc08Reset(handle);

	NxDbgMsg( NX_DBG_INFO, "=== SET chipId#1 as the LAST CHIP & RUN BIST ==\n");
	// Set last chip
	Btc08SetControl(handle, 1, LAST_CHIP);
	numChips = Btc08AutoAddress(handle);

	// Enable all cores
	Btc08SetDisable (handle, BCAST_CHIP_ID, golden_enable);

	// Write param for BIST
	Btc08WriteParam (handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
	Btc08WriteNonce (handle, BCAST_CHIP_ID, golden_nonce, golden_nonce);

	// Run BIST
	Btc08RunBist    (handle, default_golden_hash, default_golden_hash, default_golden_hash, default_golden_hash);

	for (int chipId = 1; chipId <= numChips; chipId++)
	{
		// If it's not BUSY status, read the number of cores in next READ_BIST
		for (int i=0; i<10; i++) {
			ret = Btc08ReadBist(handle, chipId);
			if ( (ret[0] & 1) == 0 )
				break;
			else
				NxDbgMsg( NX_DBG_INFO, "%5s ChipId = %d, Status = %s, Number of cores = %d\n",
						"", chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );

			usleep( 300 );
		}

		ret = Btc08ReadBist(handle, chipId);

		numCores[chipId] = ret[1];
		NxDbgMsg( NX_DBG_INFO, "%5s ChipId = %d, Status = %s, Number of cores = %d\n",
					"", chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );
	}

	for (int chipId = 1; chipId <= numChips; chipId++)
	{
		Btc08ReadId(handle, chipId, res, res_size);
		NxDbgMsg( NX_DBG_INFO, "%5s ChipId = %d, Number of jobs = %d\n",
					"", chipId, (res[2]&7) );
	}
}


void SingleCommandLoop(void)
{
	static char cmdStr[NX_SHELL_MAX_ARG * NX_SHELL_MAX_STR];
	static char cmd[NX_SHELL_MAX_ARG][NX_SHELL_MAX_STR];
	int cmdCnt;

	//	create BTC08 instance into index 0. ( /dev/spidev0.0 )
	BTC08_HANDLE handle = CreateBtc08(0);

	for( ;; )
	{
		singlecommand_command_list();
		printf( "command > " );
		fgets( cmdStr, NX_SHELL_MAX_ARG*NX_SHELL_MAX_STR - 1, stdin );
		cmdCnt = Shell_GetArgument( cmdStr, cmd );

		//----------------------------------------------------------------------
		if( !strcasecmp(cmd[0], "q") )
		{
			break;
		}
		//----------------------------------------------------------------------
		//	Reset H/W
		else if( !strcasecmp(cmd[0], "1") )
		{
			HWReset( handle );
		}
		//----------------------------------------------------------------------
		//	Reset And Auto Address
		else if( !strcasecmp(cmd[0], "2") )
		{
			ResetAutoAddress( handle );
		}
		//----------------------------------------------------------------------
		//	Reset And BistTest
		else if( !strcasecmp(cmd[0], "3") )
		{
			TestBist( handle );
		}
		//----------------------------------------------------------------------
		//	Set the chip as the last chip
		else if (!strcasecmp(cmd[0], "4") )
		{
			int chipId = 1;
			if( cmdCnt > 1 )
			{
				chipId = strtol(cmd[1], 0, 10);
			}
			printf("chipId = %d\n", chipId);
			TestLastChip( handle, chipId );
		}
		//----------------------------------------------------------------------
		//	Disable chip
		else if (!strcasecmp(cmd[0], "5") )
		{
			uint8_t chip_id = 1;
			uint8_t core_idx = 0;
			if( cmdCnt > 2 )
			{
				chip_id  = strtol(cmd[1], 0, 10);
				core_idx = strtol(cmd[2], 0, 10);
			}
			printf("chip_id = %d, core_idx = %d\n", chip_id, core_idx);
			TestDisableCore( handle, chip_id, core_idx );
		}
	}

	DestroyBtc08( handle );
}
