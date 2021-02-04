#include <stdint.h>
#include <unistd.h>

#include "Btc08.h"

#ifdef NX_DTAG
#undef NX_DTAG
#endif
#define NX_DTAG "[TestFunction]"
#include "NX_DbgMsg.h"


#define	BCAST_CHIP_ID		0x00

/* GOLD_MIDSTATE */
static uint8_t default_golden_midstate[32] = {
	0x5f, 0x4d, 0x60, 0xa2, 0x53, 0x85, 0xc4, 0x07,
	0xc2, 0xa8, 0x4e, 0x0c, 0x25, 0x91, 0x69, 0xc4,
	0x10, 0xa4, 0xa5, 0x4b, 0x93, 0xf7, 0x17, 0x08,
	0xf1, 0xab, 0xdf, 0xec, 0x6e, 0x8b, 0x81, 0xd2,
};

/* GOLD_DATA */
static uint8_t default_golden_data[12] = {
	/* Data (MerkleRoot, Time, Target) */
	0xf4, 0x2a, 0x1d, 0x6e, 0x5b, 0x30, 0x70, 0x7e,
	0x17, 0x37, 0x6f, 0x56,
};

/* GOLD_NONCE_FOR_BIST */
static uint8_t golden_nonce_for_bist[4] = {
	0x66, 0xcb, 0x34, 0x26
};

/* GOLD_TARGET */
static uint8_t default_golden_target[6] = {
	0x17, 0x37, 0x6f, 0x56, 0x05, 0x00
};

/* GOLD_HASH */
static uint8_t default_golden_hash[32] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x22, 0x09, 0x3d, 0xd4, 0x38, 0xed, 0x47,
	0xfa, 0x28, 0xe7, 0x18, 0x58, 0xb8, 0x22, 0x0d,
	0x53, 0xe5, 0xcd, 0x83, 0xb8, 0xd0, 0xd4, 0x42,
};

/* Enable all cores */
static uint8_t golden_enable[32] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

void TestBist()
{
	int numChips;
	uint8_t *ret;

	//	create BTC08 instance into index 0. ( /dev/spidev0.0 )
	BTC08_HANDLE handle = CreateBtc08(0);
	Btc08ResetHW( handle );

	numChips = Btc08AutoAddress(handle);

	NxDbgMsg(NX_DBG_INFO, "Number of Chips = %d\n", numChips);

	Btc08WriteParam (handle, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);

	Btc08WriteTarget(handle, BCAST_CHIP_ID, default_golden_target);

	Btc08WriteNonce (handle, BCAST_CHIP_ID, golden_nonce_for_bist, golden_nonce_for_bist);

	Btc08SetDisable (handle, BCAST_CHIP_ID, golden_enable);

	Btc08RunBist    (handle, default_golden_hash, default_golden_hash, default_golden_hash, default_golden_hash);

	for (int chipId = 1; chipId <= numChips; chipId++)
	{
		// If it's not BUSY status, read the number of cores in next READ_BIST
		for (int i=0; i<10; i++) {
			ret = Btc08ReadBist(handle, chipId);
			if ( (ret[0] & 1) == 0 )
				break;
			else
				NxDbgMsg( NX_DBG_INFO, "ChipId = %d, Status = %s, Number of cores = %d\n",
						chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );

			usleep( 300 );
		}

		ret = Btc08ReadBist(handle, chipId);
		NxDbgMsg( NX_DBG_INFO, "ChipId = %d, Status = %s, Number of cores = %d\n",
					chipId, (ret[0]&1) ? "BUSY":"IDLE", ret[1] );
	}

	for (int chipId = 1; chipId <= numChips; chipId++)
	{
		NxDbgMsg( NX_DBG_INFO, "ChipId = %d, ReadId = %d\n",
					chipId, Btc08ReadId(handle, chipId) );
	}

	DestroyBtc08( handle );
}

int ResetAutoAddress()
{
	int numChips;
	//	create BTC08 instance into index 0. ( /dev/spidev0.0 )
	BTC08_HANDLE handle = CreateBtc08(0);
	Btc08ResetHW( handle );

	numChips = Btc08AutoAddress(handle);
	NxDbgMsg( NX_DBG_INFO, "Number of Chips = %d\n", numChips );

	DestroyBtc08( handle );
	return 0;
}


