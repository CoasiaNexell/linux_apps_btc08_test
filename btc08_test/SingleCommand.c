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
	printf("  5. Disable core\n");
	printf("    ex > 5 [chipId(1~3)] [core index(0~255)]\n");
	printf("  6. Write, Read Target\n");
	printf("  7. Set, Read disable\n");
	printf("  8. Read Revision\n");
	printf("  9. Read Feature\n");
	printf("  10. Read/Write IO Ctrl\n");
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

/* WRITE/READ TARGET */
static int TestWRTarget( BTC08_HANDLE handle )
{
	int numChips, debug = 0;
	char title[512];
	uint8_t res[140] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	NxDbgMsg( NX_DBG_INFO, "=== AUTO ADDRESS ==\n");
	numChips = Btc08AutoAddress(handle);

	NxDbgMsg( NX_DBG_INFO, "=== WRITE_TARGET(per Chip) ==\n");
	for (int chipId=1; chipId <= numChips; chipId++)
	{
		if (0 == Btc08WriteTarget(handle, chipId, default_golden_target))
		{
			Btc08ReadTarget(handle, chipId, res, 6);
			if (0 != memcmp(default_golden_target, res, 6))
			{
				NxDbgMsg( NX_DBG_INFO, "=== %5s Failed READ_TARGET(chip#%d) ==\n", "", chipId);
				if (debug) {
					HexDump("write_target:", default_golden_target, 6);
					sprintf(title, "chipId(%d) read_target:", chipId);
					HexDump(title, res, 6);
					return -1;
				}
			}
		}
		else
		{
			NxDbgMsg( NX_DBG_INFO, "=== %5s Failed WRITE_TARGET(per Chip) due to spi err ==\n", "");
			return -1;
		}
	}

	NxDbgMsg( NX_DBG_INFO, "=== WRITE_TARGET(BR) ==\n");
	if (0 == Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target))
	{
		for (int chipId=1; chipId <= numChips; chipId++)
		{
			Btc08ReadTarget(handle, chipId, res, 6);
			if (0 != memcmp(default_golden_target, res, 6))
			{
				NxDbgMsg( NX_DBG_INFO, "=== %5s Failed READ_TARGET(chip#%d) ==\n", "", chipId);
				HexDump("write_target:", default_golden_target, 6);
				sprintf(title, "chipId(%d) read_target:", chipId);
				HexDump(title, res, 6);
				return -1;
			}
		}
	}
	else
	{
		NxDbgMsg( NX_DBG_INFO, "=== %5s Failed WRITE_TARGET(BR) due to spi err ==\n", "");
		return -1;
	}

	NxDbgMsg(NX_DBG_INFO, "=== %5s Succeed READ/WRITE_TARGET ==\n", "");

	return 0;
}

/* SET_DISABLE > READ_DISABLE */
static int TestWRDisable( BTC08_HANDLE handle )
{
	int numChips;
	char title[512];
	uint8_t *ret;
	uint8_t res[32] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	uint8_t enable_all[32] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	uint8_t disable_all[32] = {
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	};
	uint8_t disable_1core[32] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	};
	uint8_t disable_2cores[32] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
	};


	uint8_t *test_data1[3] = { disable_1core, disable_1core, disable_1core };
	uint8_t *test_data2[3] = { disable_2cores, disable_1core, disable_1core };
	uint8_t *test_data3[3] = { disable_2cores, disable_1core, disable_2cores };

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	// AUTO_ADDRESS
	numChips = Btc08AutoAddress(handle);

	Btc08Reset(handle);

	// SET_DISABLE (per Chip)
	for (int chipId=1; chipId <= numChips; chipId++)
	{
		NxDbgMsg( NX_DBG_INFO, "=== SET_DISABLE(chip%d) ==\n", chipId);

		if (0 == Btc08SetDisable(handle, chipId, test_data3[chipId-1]))
		{
			// RUN_BIST
			Btc08WriteParam (handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
			Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
			Btc08WriteNonce (handle, BCAST_CHIP_ID, golden_nonce, golden_nonce);
			Btc08RunBist    (handle, default_golden_hash, default_golden_hash, default_golden_hash, default_golden_hash);

			// READ_BIST
			for (int i=0; i<10; i++) {
				ret = Btc08ReadBist(handle, chipId);
				if ( (ret[0] & 1) == 0 )
					break;
				usleep( 300 );
			}

			ret = Btc08ReadBist(handle, chipId);
			NxDbgMsg( NX_DBG_INFO, "%5s ChipId = %d, Status = %s, Number of cores = %d\n",
						"", chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );

			// READ_DISABLE
			Btc08ReadDisable(handle, chipId, res, 32);
			if (0 != memcmp(test_data3[chipId-1], res, 32))
			{
				NxDbgMsg(NX_DBG_INFO, "=== %5s Failed READ_DISABLE(chip#%d) ==\n", "", chipId);
				HexDump("set_disable:", test_data3[chipId-1], 32);
				sprintf(title, "chipId(%d) read_disable:", chipId);
				HexDump(title, res, 32);
				return -1;
			}
		}
		else {
			NxDbgMsg( NX_DBG_INFO, "=== %5s Failed SET_DISABLE(per Chip) due to spi err ==\n", "");
			return -1;
		}
	}

	// SET_DISABLE (BR)
	NxDbgMsg( NX_DBG_INFO, "=== SET_DISABLE(BR) ==\n");
	if (0 == Btc08SetDisable(handle, BCAST_CHIP_ID, enable_all))
	{
		// RUN_BIST
		Btc08WriteParam (handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
		Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);
		Btc08WriteNonce (handle, BCAST_CHIP_ID, golden_nonce, golden_nonce);
		Btc08RunBist    (handle, default_golden_hash, default_golden_hash, default_golden_hash, default_golden_hash);

		// READ_BIST
		for (int chipId = 1; chipId <= numChips; chipId++)
		{
			for (int i=0; i<10; i++) {
				ret = Btc08ReadBist(handle, chipId);
				if ( (ret[0] & 1) == 0 )
					break;
				usleep( 300 );
			}

			ret = Btc08ReadBist(handle, chipId);
			NxDbgMsg( NX_DBG_INFO, "%5s ChipId = %d, Status = %s, Number of cores = %d\n",
						"", chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );

			// READ_DISABLE
			Btc08ReadDisable(handle, chipId, res, 32);
			if (0 != memcmp(enable_all, res, 32))
			{
				NxDbgMsg(NX_DBG_INFO, "=== Failed READ_DISABLE(chip#%d) ==\n", chipId);
				HexDump("set_disable:", enable_all, 32);
				sprintf(title, "chipId(%d) read_disable:", chipId);
				HexDump(title, res, 32);
				return -1;
			}
		}
	}
	else {
		NxDbgMsg( NX_DBG_INFO, "=== Failed SET_DISABLE(BR) due to spi err ==\n");
		return -1;
	}

	NxDbgMsg(NX_DBG_INFO, "=== Succeed SET/READ_DISABLE ==\n");

	return 0;
}

static int TestReadRevision( BTC08_HANDLE handle )
{
	int numChips;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	uint8_t fixed_rev[4] = {0x20, 0x01, 0x08, 0x00};

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	numChips = Btc08AutoAddress(handle);

	for (int chipId = 1; chipId <= numChips; chipId++)
	{
		if (0 == Btc08ReadRevision(handle, chipId, res, res_size))
		{
			NxDbgMsg(NX_DBG_INFO, "=== Succeed to read revision(year:%02x, month:%02x, day:%02x, index:%02x) ==\n",
						res[0], res[1], res[2], res[3]);

			if (0 != memcmp(fixed_rev, res, 4))
			{
				NxDbgMsg(NX_DBG_INFO, "=== Not matched revision ==\n");
				HexDump("READ_REVISION", res, 4);
				HexDump("fixed_rev", fixed_rev, 4);
				return -1;
			}
		}
		else
		{
			NxDbgMsg(NX_DBG_INFO, "=== Failed READ_REVISION ==\n");
			return -1;
		}
	}
	NxDbgMsg(NX_DBG_INFO, "=== Succeed READ_REVISION ==\n");

	return 0;
}

static int TestReadFeature( BTC08_HANDLE handle )
{
	int numChips;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	// Fixed value: 0xB5B, FPGA/ASIC: 0x00/0x05, 0x00, Hash Depth: 0x88
	uint8_t fixed_feature[4] = {0xB5, 0xB0, 0x00, 0x88};

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	numChips = Btc08AutoAddress(handle);

	for (int chipId = 1; chipId <= numChips; chipId++)
	{
		if (0 == Btc08ReadFeature(handle, chipId, res, res_size))
		{
			NxDbgMsg(NX_DBG_INFO, "=== read feature(0x%02x 0x%02x 0x%02x 0x%02x) ==\n",
						res[0], res[1], res[2], res[3]);

			if (0 != memcmp(fixed_feature, res, 4))
			{
				NxDbgMsg(NX_DBG_INFO, "=== %5s Not matched feature ==\n", "");
				HexDump("READ_FEATURE", res, 4);
				HexDump("fixed_feature", fixed_feature, 4);
				return -1;
			}
		}
		else
		{
			NxDbgMsg(NX_DBG_INFO, "=== %5s Failed READ_FEATURE due to spi err ==\n", "");
			return -1;
		}
	}
	NxDbgMsg(NX_DBG_INFO, "=== %5s Succeed READ_FEATURE ==\n", "");

	return 0;
}

static int TestIOCtrl( BTC08_HANDLE handle )
{
	int numChips;
	uint8_t res[16] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);
	// default: 0x00000000_000e0000_00008000_ffffffff
	// wr     : 0x00000000_0015ffff_ffff0000_00000000
	uint8_t default_ioctrl[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0xff, 0xff, 0xff, 0xff};
	uint8_t wr_ioctrl[16]      = {0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	Btc08ResetHW( handle, 1 );
	Btc08ResetHW( handle, 0 );

	numChips = Btc08AutoAddress(handle);

	// READ_IO_CTRL to read default value
	for (int chipId = 1; chipId <= numChips; chipId++)
	{
		if (0 == Btc08ReadIOCtrl(handle, chipId, res, res_size))
		{
			if (0 != memcmp(default_ioctrl, res, 16))
			{
				NxDbgMsg(NX_DBG_ERR, "=== Test1 Failed: READ_IO_CTRL(chip#%d) ==\n", chipId);
				HexDump("READ_IOCTRL", res, 16);
				HexDump("default_ioctrl", default_ioctrl, 16);
				return -1;
			}
		}
		else
		{
			NxDbgMsg(NX_DBG_ERR, "=== Test1 Failed: READ_IO_CTRL with spi err ==\n");
			return -1;
		}
	}

	// WRITE_IO_CTRL
	if (Btc08WriteIOCtrl(handle, BCAST_CHIP_ID, wr_ioctrl) < 0)
	{
		NxDbgMsg(NX_DBG_ERR, "=== Test Failed: WRITE_IO_CTRL ==\n");
		return -1;
	}

	// READ_IO_CTRL to read the changed value
	for (int chipId = 1; chipId <= numChips; chipId++)
	{
		if (0 == Btc08ReadIOCtrl(handle, chipId, res, res_size))
		{
			if (0 != memcmp(wr_ioctrl, res, 16))
			{
				NxDbgMsg(NX_DBG_ERR, "=== Test2 Failed: READ_IO_CTRL(chip#%d) ==\n", chipId);
				HexDump("READ_IOCTRL", res, 16);
				HexDump("wr_ioctrl", wr_ioctrl, 16);
				return -1;
			}
		}
		else
		{
			NxDbgMsg(NX_DBG_ERR, "=== Test2 Failed: READ_IO_CTRL with spi err ==\n");
			return -1;
		}
	}

	NxDbgMsg(NX_DBG_INFO, "=== Test Succeed: READ/WRITE_IOCTRL ==\n");

	return 0;
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
		//	Disable core
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
		//----------------------------------------------------------------------
		//	Write/Read Target Test
		else if (!strcasecmp(cmd[0], "6") )
		{
			TestWRTarget(handle);
		}
		//----------------------------------------------------------------------
		//	Set Disable core
		else if (!strcasecmp(cmd[0], "7") )
		{
			TestWRDisable(handle);
		}
		//----------------------------------------------------------------------
		//	Read Revision Test
		else if ( !strcasecmp(cmd[0], "8") )
		{
			TestReadRevision(handle);
		}
		//----------------------------------------------------------------------
		//	Read Feature Test
		else if ( !strcasecmp(cmd[0], "9") )
		{
			TestReadFeature(handle);
		}
		//----------------------------------------------------------------------
		//	Read/Write IO Ctrl Test
		else if ( !strcasecmp(cmd[0], "10") )
		{
			TestIOCtrl(handle);
		}
	}

	DestroyBtc08( handle );
}