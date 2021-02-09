//------------------------------------------------------------------------------
//
//	Copyright (C) 2021 CoAsiaNexell Co. All Rights Reserved
//	CoAsiaNexell Co. Proprietary & Confidential
//
//	NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
//  AND	WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
//  FOR A PARTICULAR PURPOSE.
//
//	Module		: BTC08 Control Module
//	File		: Btc08.c
//	Description	:
//	Author		: SeongO.Park (ray@coasia.com)
//	Export		:
//	History		:
//
//------------------------------------------------------------------------------
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>

#include "Spi.h"
#include "GpioControl.h"
#include "Btc08.h"
#include "Utils.h"


#ifdef NX_DTAG
#undef NX_DTAG
#endif
#define NX_DTAG "[CBtc08]"
#include "NX_DbgMsg.h"



enum BTC08_cmd {
	SPI_CMD_READ_ID			= 0x00,
	SPI_CMD_AUTO_ADDRESS	= 0x01,
	SPI_CMD_RUN_BIST		= 0x02,
	SPI_CMD_READ_BIST		= 0x03,
	SPI_CMD_RESET			= 0x04,
	SPI_CMD_SET_PLL_CONFIG	= 0x05, /* noresponse */
	SPI_CMD_READ_PLL		= 0x06,
	SPI_CMD_WRITE_PARM		= 0x07,
	SPI_CMD_READ_PARM		= 0x08,
	SPI_CMD_WRITE_TARGET	= 0x09,
	SPI_CMD_READ_TARGET		= 0x0A,
	SPI_CMD_RUN_JOB			= 0x0B,
	SPI_CMD_READ_JOB_ID		= 0x0C,
	SPI_CMD_READ_RESULT		= 0x0D,
	SPI_CMD_CLEAR_OON		= 0x0E,
	SPI_CMD_SET_DISABLE		= 0x10,
	SPI_CMD_READ_DISABLE	= 0x11,
	SPI_CMD_SET_CONTROL		= 0x12,	/* no response */
	SPI_CMD_READ_TEMP		= 0x14,
	SPI_CMD_WRITE_NONCE		= 0x16,
	SPI_CMD_READ_HASH		= 0x20,
	SPI_CMD_READ_FEATURE	= 0x32,
	SPI_CMD_READ_REVISION	= 0x33,
	SPI_CMD_SET_PLL_FOUT_EN	= 0x34,
	SPI_CMD_SET_PLL_RESETB 	= 0x35,
};

#define DUMMY_BYTES			0
#define	BCAST_CHIP_ID		0x00
#define CMD_LEN				1
#define	CHIP_ID_LEN			1
#define	TARGET_LEN			6
#define NONCE_LEN			4
#define	HASH_LEN			32
#define MIDSTATE_LEN		32
// MERKLEROOT + TIMESTAMP + DIFFICULTY
#define DATA_LEN			12
#define DISABLE_LEN			32
#define READ_RESULT_LEN 	18

#define ASIC_BOOST_CORE_NUM		4
#define TOTAL_MIDSTATE_LEN		(MIDSTATE_LEN * ASIC_BOOST_CORE_NUM)
#define TOTAL_HASH_LEN			(HASH_LEN * ASIC_BOOST_CORE_NUM)

// midstate + data + midstate + midstate + midstate
#define WRITE_JOB_LEN			((TOTAL_MIDSTATE_LEN + DATA_LEN))


#define OON_IRQ_ENB			(1 << 4)
#define LAST_CHIP_FLAG		(1 << 15)
#define ASIC_BOOST_EN		(1 << 9)


#define HW_RESET_TIME			(50000)		//	50 msec


#define GPIO_RESET_0	127
#define GPIO_IRQ_OON_0	125
#define GPIO_IRQ_GN_0	126

#define GPIO_RESET_1	127
#define GPIO_IRQ_OON_1	125
#define GPIO_IRQ_GN_1	126



struct pll_conf {
	int freq;
	union {
		struct {
			int p        : 6;
			int m        :10;
			int s        : 3;
			int bypass   : 1;
			int div_sel  : 1;
			int afc_enb  : 1;
			int extafc   : 5;
			int feed_en  : 1;
			int fsel     : 1;
			int rsvd     : 3;
		};
		unsigned int val;
	};
};

static struct pll_conf pll_sets[] = {
	{ 300, {6, 600, 2, 0, 1, 0, 0, 0, 0, 0}},
	{ 350, {6, 700, 2, 0, 1, 0, 0, 0, 0, 0}},
	{ 400, {6, 400, 1, 0, 1, 0, 0, 0, 0, 0}},
	{ 450, {6, 450, 1, 0, 1, 0, 0, 0, 0, 0}},
	{ 500, {6, 500, 1, 0, 1, 0, 0, 0, 0, 0}},
	{ 550, {6, 550, 1, 0, 1, 0, 0, 0, 0, 0}},
	{ 600, {6, 600, 1, 0, 1, 0, 0, 0, 0, 0}},
	{ 650, {6, 650, 1, 0, 1, 0, 0, 0, 0, 0}},
	{ 700, {6, 700, 1, 0, 1, 0, 0, 0, 0, 0}},
	{ 750, {6, 750, 1, 0, 1, 0, 0, 0, 0, 0}},
	{ 800, {6, 800, 1, 0, 1, 0, 0, 0, 0, 0}},
	{ 850, {6, 425, 0, 0, 1, 0, 0, 0, 0, 0}},
	{ 900, {6, 450, 0, 0, 1, 0, 0, 0, 0, 0}},
	{ 950, {6, 475, 0, 0, 1, 0, 0, 0, 0, 0}},
	{1000, {6, 500, 0, 0, 1, 0, 0, 0, 0, 0}},
};


#define	SPI_MAX_TRANS	(1024)

struct tag_BTC08_INFO{
	GPIO_HANDLE		hReset;
	GPIO_HANDLE		hGn;
	GPIO_HANDLE		hOon;
	SPI_HANDLE		hSpi;
	uint8_t			txBuf[SPI_MAX_TRANS];
	uint8_t			rxBuf[SPI_MAX_TRANS];
};



BTC08_HANDLE CreateBtc08( int32_t index )
{
	int32_t gpioReset, gpioOon, gpioGn;
	BTC08_HANDLE handle = NULL;
	SPI_HANDLE hSpi = NULL;
	GPIO_HANDLE hReset = NULL;
	GPIO_HANDLE hOon = NULL;
	GPIO_HANDLE hGn = NULL;
	if( index == 0 )
	{
		hSpi = CreateSpi( "/dev/spidev0.0", 0, 500000, 0, 8 );
		gpioReset = GPIO_RESET_0;
		gpioOon   = GPIO_IRQ_OON_0;
		gpioGn    = GPIO_IRQ_GN_0;
	}
	else if( index == 1 )
	{
		hSpi = CreateSpi( "/dev/spidev2.0", 0, 500000, 0, 8 );
		gpioReset = GPIO_RESET_1;
		gpioOon   = GPIO_IRQ_OON_1;
		gpioGn    = GPIO_IRQ_GN_1;
	}
	else
	{
		return NULL;
	}

	//	Create GPIOs
	hReset = CreateGpio(gpioReset);
	hOon   = CreateGpio(gpioOon  );
	hGn    = CreateGpio(gpioGn   );

	if( !hSpi || !hReset || !hOon || !hGn )
	{
		goto ERROR_EXIT;
	}

	GpioSetDirection( hReset, GPIO_DIRECTION_OUT );
	GpioSetDirection( hOon  , GPIO_DIRECTION_IN  );
	GpioSetDirection( hGn   , GPIO_DIRECTION_IN  );

	handle = (BTC08_HANDLE)malloc( sizeof(struct tag_BTC08_INFO) );
	memset( handle, 0, sizeof(struct tag_BTC08_INFO) );

	handle->hSpi   = hSpi;
	handle->hReset = hReset;
	handle->hOon   = hOon;
	handle->hGn    = hGn;

	return handle;

ERROR_EXIT:
	if( hSpi )		DestroySpi ( hSpi   );
	if( hReset )	DestroyGpio( hReset );
	if( hOon   )	DestroyGpio( hOon   );
	if( hGn    )	DestroyGpio( hGn    );
	return NULL;
}

void DestroyBtc08( BTC08_HANDLE handle )
{
	if( handle )
	{
		if( handle->hSpi   )		DestroySpi (handle->hSpi  );
		if( handle->hReset )		DestroyGpio(handle->hReset);
		if( handle->hOon   )		DestroyGpio(handle->hOon  );
		if( handle->hGn    )		DestroyGpio(handle->hGn   );

		free( handle );
	}
}

int Btc08ResetHW (BTC08_HANDLE handle, int32_t enable)
{
	if( !handle )
		return -1;

	if (enable)
	{
		GpioSetValue( handle->hReset, 0 );
		usleep( HW_RESET_TIME );
	}
	else
	{
		GpioSetValue( handle->hReset, 1 );
		usleep( 50000 );	//	wait 50msec : FIXME: TODO:
	}

	return 0;
}

int	Btc08ReadId (BTC08_HANDLE handle, uint8_t chipId)
{
	uint8_t *rx;
	size_t txLen = 2;

	if( !handle )
		return -1;

	if( chipId == BCAST_CHIP_ID )
	{
		return 0;
	}

	handle->txBuf[0] = SPI_CMD_READ_ID;
	handle->txBuf[1] = chipId;

	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, 4+DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return 0;
	}

	rx = handle->rxBuf + txLen;
	// rx[0]: [31:24] Reserved
	// rx[1]: [22:16] Number of SPI bytes previously transmitted
	// rx[2]: [10:8] Number of jobs in FIFO
	// rx[3]: [7:0] Chip ID

	//NxDbgMsg(NX_DBG_ERR, "%s() NumJobs in FIFO = %d\n", __FUNCTION__, (rx[2] & 7) );
	return (int)( rx[3] );
}


int Btc08AutoAddress (BTC08_HANDLE handle)
{
	uint8_t *rx;
	size_t txLen = 2+32;
	handle->txBuf[0] = SPI_CMD_AUTO_ADDRESS;
	handle->txBuf[1] = 0x00;				//	Chip ID : Broadcast

	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, 2+DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return 0;
	}

	rx = handle->rxBuf + txLen;
	// rx[0]
	// rx[1] : number of chips
	return rx[1];
}



int Btc08RunBist(BTC08_HANDLE handle, uint8_t *hash, uint8_t *hash2, uint8_t *hash3, uint8_t *hash4)
{
	int txLen = 0;
	handle->txBuf[0] = SPI_CMD_RUN_BIST;
	handle->txBuf[1] = 0x00;

	txLen += 2;
	memcpy(handle->txBuf+txLen, hash , HASH_LEN);	txLen += HASH_LEN;
	memcpy(handle->txBuf+txLen, hash2, HASH_LEN);	txLen += HASH_LEN;
	memcpy(handle->txBuf+txLen, hash3, HASH_LEN);	txLen += HASH_LEN;
	memcpy(handle->txBuf+txLen, hash4, HASH_LEN);	txLen += HASH_LEN;

	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	return 0;
}


uint8_t* Btc08ReadBist    (BTC08_HANDLE handle, uint8_t chipId)
{
	uint8_t *rx;
	int txLen = 2;
	handle->txBuf[0] = SPI_CMD_READ_BIST;
	handle->txBuf[1] = chipId;


	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, 2 + DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return NULL;
	}

	rx = handle->rxBuf + txLen;
	// ret[0]: [8] : Busy status
	// ret[1]: [7:0] Number of cores

	//NxDbgMsg( NX_DBG_INFO, " ChipId = %d, Status = %s, Number of cores = %d\n",
	//						chipId, (rx[0]&1) ? "Busy":"Idle", rx[1]);

	return rx;
}


int Btc08Reset       (BTC08_HANDLE handle)
{
	handle->txBuf[0] = SPI_CMD_RESET;
	handle->txBuf[1] = 0x00;

	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, 2, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	return 0;
}


int Btc08SetPllConfig(BTC08_HANDLE handle, uint8_t idx)
{
	handle->txBuf[0] = SPI_CMD_SET_PLL_CONFIG;
	handle->txBuf[1] = 0x00;
	handle->txBuf[2] = (uint8_t)(pll_sets[idx].val>>24)&0xff;
	handle->txBuf[3] = (uint8_t)(pll_sets[idx].val>>24)&0xff;
	handle->txBuf[4] = (uint8_t)(pll_sets[idx].val>>24)&0xff;
	handle->txBuf[5] = (uint8_t)(pll_sets[idx].val>>24)&0xff;

	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, 2+4, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	return 0;
}


int Btc08ReadPll(BTC08_HANDLE handle)
{
	uint8_t *rx;
	size_t txLen = 2;
	handle->txBuf[0] = SPI_CMD_READ_PLL;
	handle->txBuf[1] = 0x00;

	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, 4+DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}


	rx = handle->rxBuf + txLen;

	//	1 : locked
	//	0 : unlocked
	return (rx[1]&(1<<7)) ? 1 : 0;
}


int Btc08WriteParam  (BTC08_HANDLE handle, uint8_t chipId, uint8_t *midState, uint8_t *data )
{
	int txLen = 0;
	handle->txBuf[0] = SPI_CMD_WRITE_PARM;
	handle->txBuf[1] = chipId;

	txLen += 2;
	if( midState )
	{
		memcpy( handle->txBuf+txLen, midState, MIDSTATE_LEN );
		txLen += MIDSTATE_LEN;
	}

	if( data )
	{
		memcpy( handle->txBuf+txLen, data, DATA_LEN );
		txLen += DATA_LEN;
	}

	//	for ASIC Bootster
	if( midState )
	{
		memcpy( handle->txBuf+txLen, midState, MIDSTATE_LEN );	txLen += MIDSTATE_LEN;
		memcpy( handle->txBuf+txLen, midState, MIDSTATE_LEN );	txLen += MIDSTATE_LEN;
		memcpy( handle->txBuf+txLen, midState, MIDSTATE_LEN );	txLen += MIDSTATE_LEN;
	}

	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	return 0;
}


int Btc08ReadParam   (BTC08_HANDLE handle, uint8_t chipId)
{
	uint8_t *rx;
	size_t txLen = 2;
	handle->txBuf[0] = SPI_CMD_READ_PARM;
	handle->txBuf[1] = chipId;


	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, 4+DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	rx = handle->rxBuf + txLen;
	//	1 : locked
	//	0 : unlocked
	return (rx[1]&(1<<7)) ? 1 : 0;
}


int Btc08WriteTarget (BTC08_HANDLE handle, uint8_t chipId, uint8_t *target )
{
	size_t txLen = 2;
	handle->txBuf[0] = SPI_CMD_WRITE_TARGET;
	handle->txBuf[1] = chipId;

	memcpy( handle->txBuf+txLen, target, TARGET_LEN );
	txLen += TARGET_LEN;

	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	return 0;
}


int Btc08ReadTarget  (BTC08_HANDLE handle, uint8_t chipId )
{
	uint8_t *rx;
	size_t txLen = 2;
	handle->txBuf[0] = SPI_CMD_READ_TARGET;
	handle->txBuf[1] = chipId;


	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, TARGET_LEN+DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	//
	//	TODO: FIXME: Need processing read data
	//
	rx = handle->rxBuf + txLen;
	return 0;
}


int Btc08RunJob      (BTC08_HANDLE handle, uint8_t chipId, uint8_t jobId )
{
	handle->txBuf[0] = SPI_CMD_RUN_JOB;
	handle->txBuf[1] = chipId;
	//handle->txBuf[2] = ASIC_BOOST_EN;
	handle->txBuf[2] = 1<<1;
	handle->txBuf[3] = jobId;

	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, 2+2, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	return 0;
}


int Btc08ReadJobId   (BTC08_HANDLE handle, uint8_t chipId )
{
	uint8_t *rx;
	size_t txLen = 2;
	int fifo_full, oon_irq, gn_irq;
	handle->txBuf[0] = SPI_CMD_READ_JOB_ID;
	handle->txBuf[1] = chipId;

	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, 4+DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	//
	//	TODO: FIXME: Need more processing
	//

	rx = handle->rxBuf + txLen;
	// rx[0]: [31:24] OON Job ID
	// rx[1]: [23:16] GN Job ID
	// rx[2]: [10:8]  10/9/8 : Result FIFO Full/OON IRQ/GN IRQ Flag
	// rx[3]: [7:0]   Chip ID
	fifo_full = ((rx[2] & (1<<2)) != 0);
	oon_irq = ((rx[2] & (1<<1)) != 0);
	gn_irq = ((rx[2] & (1<<0)) != 0);

	NxDbgMsg(NX_DBG_INFO, " <== OON Job ID(%d), GN Job ID(%d)\n", rx[0], rx[1]);
	NxDbgMsg(NX_DBG_INFO, " <== Flag FIFO Full(%d), OON IRQ(%d), GN IRQ(%d)\n", fifo_full, oon_irq, gn_irq);
	NxDbgMsg(NX_DBG_INFO, " <== Chip ID(%d)\n", rx[3]);
	return 0;
}

int Btc08ReadResult  (BTC08_HANDLE handle, uint8_t chipId )
{
	uint8_t *rx;
	size_t txLen=2;
	int lower, lower2, lower3, upper;
	handle->txBuf[0] = SPI_CMD_READ_RESULT;
	handle->txBuf[1] = chipId;

	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, READ_RESULT_LEN+DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	rx = handle->rxBuf + txLen;
	// rX[0]: [139:136] Inst_Lower_3/Inst_Lower_2/Inst_Lower/Upper found golden nonce
	// rX[1]: [135:128] read ValidCnt in Core
	// rX[2-5]: [127:96]  read_golden_nonce of Inst_Lower
	// rX[6-9]: [95:64]  read_golden_nonce of Inst_Lower
	// rX[10-13]: [63:32]  read_golden_nonce of Inst_Lower
	// rX[14-17]: [31:0]  read_golden_nonce of Inst_Upper
	lower3 = ((rx[0] & 8) != 0);
	lower2 = ((rx[0] & 4) != 0);
	lower  = ((rx[0] & 2) != 0);
	upper  = ((rx[0] & 1) != 0);
	NxDbgMsg(NX_DBG_INFO, " <== GN found in Inst_%s\n",
			lower3 ? "Lower_3": (lower2 ? "Lower_2" : (lower ? "Lower": (upper ? "Upper":""))));

	return 0;
}


int Btc08ClearOON    (BTC08_HANDLE handle, uint8_t chipId )
{
	handle->txBuf[0] = SPI_CMD_CLEAR_OON;
	handle->txBuf[1] = chipId;

	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, 2, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	return 0;
}


int Btc08SetDisable  (BTC08_HANDLE handle, uint8_t chipId, uint8_t *disable )
{
	size_t txLen = 2;
	handle->txBuf[0] = SPI_CMD_SET_DISABLE;
	handle->txBuf[1] = chipId;

	memcpy( handle->txBuf+txLen, disable, DISABLE_LEN );
	txLen += DISABLE_LEN;

	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	return 0;
}


int Btc08ReadDisable (BTC08_HANDLE handle, uint8_t chipId )
{
	uint8_t *rx;
	size_t txLen = 2;
	handle->txBuf[0] = SPI_CMD_READ_DISABLE;
	handle->txBuf[1] = chipId;

	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, DISABLE_LEN + DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	rx = handle->rxBuf + txLen;
	return 0;
}

int Btc08SetControl  (BTC08_HANDLE handle, uint8_t chipId, uint32_t param )
{
	handle->txBuf[0] = SPI_CMD_SET_CONTROL;
	handle->txBuf[1] = chipId;
	handle->txBuf[2] = (uint8_t)((param>>24)&0xff);
	handle->txBuf[3] = (uint8_t)((param>>16)&0xff);
	handle->txBuf[4] = (uint8_t)((param>> 8)&0xff);
	handle->txBuf[5] = (uint8_t)((param>> 0)&0xff);

	if (handle->txBuf[4] & LAST_CHIP_FLAG)
	{
		NxDbgMsg(NX_DBG_INFO, " ==> Set a last chip (chip_id %d)\n", chipId);
	}
	if (handle->txBuf[5] & OON_IRQ_ENB)
	{
		NxDbgMsg(NX_DBG_INFO, " ==> Set OON IRQ Enable\n");
	}

	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, 2+4, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}
	return 0;
}


int Btc08ReadTemp    (BTC08_HANDLE handle, uint8_t chipId )
{
	handle->txBuf[0] = SPI_CMD_READ_TEMP;
	handle->txBuf[1] = chipId;

	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, 2, 2+DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}
	return 0;
}


int Btc08WriteNonce  (BTC08_HANDLE handle, uint8_t chipId, uint8_t *startNonce, uint8_t *endNonce )
{
	size_t txLen = 2;
	handle->txBuf[0] = SPI_CMD_WRITE_NONCE;
	handle->txBuf[1] = chipId;

	memcpy(handle->txBuf + txLen, startNonce, NONCE_LEN);	txLen += NONCE_LEN;
	memcpy(handle->txBuf + txLen, endNonce  , NONCE_LEN);	txLen += NONCE_LEN;

	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}
	return 0;
}


int Btc08ReadHash    (BTC08_HANDLE handle, uint8_t chipId )
{
	uint8_t *rx;
	size_t txLen = 2;
	handle->txBuf[0] = SPI_CMD_READ_HASH;
	handle->txBuf[1] = chipId;

	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, 128+DUMMY_BYTES) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	rx = handle->rxBuf + txLen;

	return 0;
}


int Btc08ReadFeature (BTC08_HANDLE handle, uint8_t chipId )
{
	uint8_t *rx;
	size_t txLen = 2;
	handle->txBuf[0] = SPI_CMD_READ_FEATURE;
	handle->txBuf[1] = chipId;

	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, 4+DUMMY_BYTES) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	rx = handle->rxBuf + txLen;
	// ret[0-1] [31:20] Fixed value: 0xB5B
	// ret[1]   [19:16] FPGA/ASIC : 0x05/0x0
	// ret[2]   [15:8] 0x00
	// ret[3]   [7:0] Hash depth : 0x86
	int is_FPGA = ((rx[1]&8) == 0x05) ? 1:0;
	NxDbgMsg(NX_DBG_INFO, " <== is_FPGA? (%s), hash_depth(%02X)\n", is_FPGA?"FPGA":"ASIC", rx[3]);
	return 0;
}


int Btc08ReadRevision(BTC08_HANDLE handle, uint8_t chipId )
{
	uint8_t *rx;
	size_t txLen = 2;
	handle->txBuf[0] = SPI_CMD_READ_REVISION;
	handle->txBuf[1] = chipId;

	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, 4+DUMMY_BYTES) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	rx = handle->rxBuf + txLen;

	// Read revision date info (year, month, day, index)
	return 0;
}


int Btc08SetPllFoutEn(BTC08_HANDLE handle, uint8_t chipId, uint8_t fout)
{
	handle->txBuf[0] = SPI_CMD_SET_PLL_FOUT_EN;
	handle->txBuf[1] = chipId;
	handle->txBuf[2] = 0;
	handle->txBuf[3] = (fout&1);

	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, 2+2, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}
	return 0;
}


int Btc08SetPllResetB(BTC08_HANDLE handle, uint8_t chipId, uint8_t reset)
{
	handle->txBuf[0] = SPI_CMD_SET_PLL_RESETB;
	handle->txBuf[1] = chipId;
	handle->txBuf[2] = 0;
	handle->txBuf[3] = (reset&1);

	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, 2+2, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}
	return 0;
}
