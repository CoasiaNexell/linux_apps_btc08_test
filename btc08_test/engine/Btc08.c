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
#include <byteswap.h>

#include "Btc08.h"
#include "Utils.h"


#ifdef NX_DTAG
#undef NX_DTAG
#endif
#define NX_DTAG "[Btc08]"
#include "NX_DbgMsg.h"

#define DEBUG_RESULT	0

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
	SPI_CMD_DEBUG           = 0x15,
	SPI_CMD_WRITE_NONCE		= 0x16,
	SPI_CMD_READ_DEBUGCNT	= 0x19,
	SPI_CMD_READ_HASH		= 0x20,
	SPI_CMD_WRITE_IO_CTRL   = 0x30,
	SPI_CMD_READ_IO_CTRL    = 0x31,
	SPI_CMD_READ_FEATURE	= 0x32,
	SPI_CMD_READ_REVISION	= 0x33,
	SPI_CMD_SET_PLL_FOUT_EN	= 0x34,
	SPI_CMD_SET_PLL_RESETB 	= 0x35,
	SPI_CMD_SET_TMODE       = 0x38,
};

#define DUMMY_BYTES         2
#define BCAST_CHIP_ID       0x00
#define CMD_LEN             1
#define CHIP_ID_LEN         1
#define PARM_LEN            140
#define TARGET_LEN          6
#define NONCE_LEN           4
#define HASH_LEN            32
#define MIDSTATE_LEN        32
// MERKLEROOT + TIMESTAMP + DIFFICULTY
#define DATA_LEN            12
#define DISABLE_LEN         32
#define READ_RESULT_LEN     18

#define TMODE_SEL_LEN       2
#define IOCTRL_LEN          16
#define ASIC_BOOST_CORE_NUM		4
#define TOTAL_MIDSTATE_LEN		(MIDSTATE_LEN * ASIC_BOOST_CORE_NUM)
#define TOTAL_HASH_LEN			(HASH_LEN * ASIC_BOOST_CORE_NUM)

// midstate + data + midstate + midstate + midstate
#define WRITE_JOB_LEN			((TOTAL_MIDSTATE_LEN + DATA_LEN))


#define OON_IRQ_ENB			(1 << 4)
#define LAST_CHIP_FLAG		(1 << 15)

#define HW_RESET_TIME			(200000)		//	50 msec

#define GPIO_RESET_0	127
#define GPIO_IRQ_OON_0	125
#define GPIO_IRQ_GN_0	126

#define GPIO_RESET_1	127
#define GPIO_IRQ_OON_1	125
#define GPIO_IRQ_GN_1	126

static void _WriteDummy(BTC08_HANDLE handle, int txlen)
{
	handle->txBuf[txlen+0] = 0;
	handle->txBuf[txlen+1] = 0;
	handle->txBuf[txlen+2] = 0;
	handle->txBuf[txlen+3] = 0;
}

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
		hSpi = CreateSpi( "/dev/spidev0.0", 0, TX_RX_MAX_SPEED, 0, 8 );
		gpioReset = GPIO_RESET_0;
		gpioOon   = GPIO_IRQ_OON_0;
		gpioGn    = GPIO_IRQ_GN_0;
	}
	else if( index == 1 )
	{
		hSpi = CreateSpi( "/dev/spidev2.0", 0, TX_RX_MAX_SPEED, 0, 8 );
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
		usleep( HW_RESET_TIME );	//	wait 50msec : FIXME: TODO:
	}

	return 0;
}

int Btc08GpioGetValue (BTC08_HANDLE handle, GPIO_TYPE type)
{
	int ret = -1;

	switch (type)
	{
		case GPIO_TYPE_OON:
			ret = GpioGetValue( handle->hOon );
			break;
		case GPIO_TYPE_GN:
			ret = GpioGetValue( handle->hGn );
			break;
		case GPIO_TYPE_RESET:
			ret = GpioGetValue( handle->hReset );
			break;
		default:
			break;
	}

	return ret;
}

int	Btc08ReadId (BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size)
{
	uint8_t *rx;
	size_t txLen = 2;

	if( !handle )
		return -1;

	if( chipId == BCAST_CHIP_ID )
	{
		NxDbgMsg(NX_DBG_ERR, "[%s] failed, wrong chip id\n", __FUNCTION__);
		return -1;
	}

	handle->txBuf[0] = SPI_CMD_READ_ID;
	handle->txBuf[1] = chipId;

	_WriteDummy(handle, txLen);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, 4+DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	rx = handle->rxBuf + txLen;
	// rx[0]: [31:24] Reserved
	// rx[1]: [22:16] Number of SPI bytes previously transmitted
	// rx[2]: [10:8] Number of jobs in FIFO
	// rx[3]: [7:0] Chip ID

	memcpy(res, rx, res_size);

#if DEBUG_RESULT
	NxDbgMsg(NX_DBG_ERR, "%s() NumJobs in FIFO = %d\n", __FUNCTION__, (rx[2] & 7));
#endif

	return 0;
}


int Btc08AutoAddress (BTC08_HANDLE handle)
{
	uint8_t *rx;
	size_t txLen = 2+32;
	handle->txBuf[0] = SPI_CMD_AUTO_ADDRESS;
	handle->txBuf[1] = 0x00;				//	Chip ID : Broadcast

	_WriteDummy(handle, txLen);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, 2+DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	rx = handle->rxBuf + txLen;
	// rx[0] : 0x01
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

	_WriteDummy(handle, txLen);
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

	_WriteDummy(handle, txLen);
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

	_WriteDummy(handle, 2);
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
	size_t txLen = 6;
	handle->txBuf[0] = SPI_CMD_SET_PLL_CONFIG;
	handle->txBuf[1] = 0x00;
	handle->txBuf[2] = (uint8_t)(pll_sets[idx].val>>24)&0xff;
	handle->txBuf[3] = (uint8_t)(pll_sets[idx].val>>16)&0xff;
	handle->txBuf[4] = (uint8_t)(pll_sets[idx].val>> 8)&0xff;
	handle->txBuf[5] = (uint8_t)(pll_sets[idx].val>> 0)&0xff;

	NxDbgMsg(NX_DBG_DEBUG, "Btc08SetPllConfig() 0x%02x 0x%02x 0x%02x 0x%02x\n",
	 		handle->txBuf[2], handle->txBuf[3], handle->txBuf[4], handle->txBuf[5]);
	_WriteDummy(handle, 6);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	return 0;
}

int Btc08SetPllConfigByPass(BTC08_HANDLE handle, uint8_t idx)
{
	size_t txLen = 6;
	struct pll_conf temp = pll_sets[idx];
	temp.bypass = 1;
	temp.div_sel = 0;

	handle->txBuf[0] = SPI_CMD_SET_PLL_CONFIG;
	handle->txBuf[1] = 0x00;
	handle->txBuf[2] = (uint8_t)(temp.val>>24)&0xff;
	handle->txBuf[3] = (uint8_t)(temp.val>>16)&0xff;
	handle->txBuf[4] = (uint8_t)(temp.val>> 8)&0xff;
	handle->txBuf[5] = (uint8_t)(temp.val>> 0)&0xff;

	NxDbgMsg(NX_DBG_DEBUG, "Btc08SetPllConfigByPass() 0x%02x 0x%02x 0x%02x 0x%02x\n",
	 		handle->txBuf[2], handle->txBuf[3], handle->txBuf[4], handle->txBuf[5]);
	_WriteDummy(handle, 6);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	return 0;
}

int Btc08ReadPll(BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size)
{
	uint8_t *rx;
	size_t txLen = 2;
	handle->txBuf[0] = SPI_CMD_READ_PLL;
	handle->txBuf[1] = chipId;

	_WriteDummy(handle, txLen);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, 4+DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	rx = handle->rxBuf + txLen;
	// rx[0]    [31:30] Reserved
	//             [29] FEED_OUT
	//          [28:24] AFC_CODE
	// rx[1]       [23] LOCK (1: locked, 0: unlocked)
	//             [22] RESETB
	//             [21] FOUTEN
	//             [20] DIV_SEL
	//             [19] BYPASS
	//          [18:16] S
	// rx[2~3]   [15:6] M
	//            [5:0] P
	memcpy(res, rx, res_size);

	return (rx[1]&(1<<7));
}

/* Set param(1120bits) - MidState3(126bits) + MidState2 + MidState1 + Data(96bits) + MidState */
int Btc08WriteParam  (BTC08_HANDLE handle, uint8_t chipId, uint8_t *midState, uint8_t *data)
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
		memcpy( handle->txBuf+txLen, midState + MIDSTATE_LEN*1, MIDSTATE_LEN );	txLen += MIDSTATE_LEN;
		memcpy( handle->txBuf+txLen, midState + MIDSTATE_LEN*2, MIDSTATE_LEN );	txLen += MIDSTATE_LEN;
		memcpy( handle->txBuf+txLen, midState + MIDSTATE_LEN*3, MIDSTATE_LEN );	txLen += MIDSTATE_LEN;
	}

	_WriteDummy(handle, txLen);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	return 0;
}

int Btc08ReadParam   (BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size)
{
	uint8_t *rx;
	size_t txLen = 0;

	if (BCAST_CHIP_ID == chipId)
	{
		NxDbgMsg(NX_DBG_ERR, "[%s] failed, wrong chip id\n", __FUNCTION__);
		return -1;
	}

	handle->txBuf[0] = SPI_CMD_READ_PARM;
	handle->txBuf[1] = chipId;

	txLen += 2;

	_WriteDummy(handle, txLen);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, PARM_LEN+DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	rx = handle->rxBuf + txLen;
	//	1 : locked
	//	0 : unlocked

	memcpy(res, rx, res_size);

	return (rx[1]&(1<<7)) ? 1 : 0;
}

/* Set target to compare with hash result */
int Btc08WriteTarget (BTC08_HANDLE handle, uint8_t chipId, uint8_t *target)
{
	size_t txLen = 0;
	handle->txBuf[0] = SPI_CMD_WRITE_TARGET;
	handle->txBuf[1] = chipId;

	txLen += 2;
	memcpy( handle->txBuf+txLen, target, TARGET_LEN );
	txLen += TARGET_LEN;

	_WriteDummy(handle, txLen);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	return 0;
}

int Btc08ReadTarget  (BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size )
{
	uint8_t *rx;
	size_t txLen = 2;

	if (BCAST_CHIP_ID == chipId)
	{
		NxDbgMsg(NX_DBG_ERR, "[%s] failed, wrong chip id\n", __FUNCTION__);
		return -1;
	}

	handle->txBuf[0] = SPI_CMD_READ_TARGET;
	handle->txBuf[1] = chipId;

	_WriteDummy(handle, txLen);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, TARGET_LEN+DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	rx = handle->rxBuf + txLen;
	// rx[0-3]: [47:16] Target (32bits)
	// rx[  4]: [15:12] 4'h0
	//           [11:8] Select0
	// rx[  5]:   [7:0] Select1
	NxDbgMsg(NX_DBG_DEBUG, " <== Target(%02x %02x %02x %02x), Select0(%02x), Select1(%02x)\n",
				rx[0], rx[1], rx[2], rx[3],
				(rx[4]&15), rx[5]);

	memcpy(res, rx, res_size);

	return 0;
}


int Btc08RunJob      (BTC08_HANDLE handle, uint8_t chipId, uint8_t option, uint8_t jobId )
{
	size_t txLen = 4;
	handle->txBuf[0] = SPI_CMD_RUN_JOB;
	handle->txBuf[1] = chipId;
	handle->txBuf[2] = option;
	handle->txBuf[3] = jobId;

	_WriteDummy(handle, 4);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	return 0;
}


int Btc08ReadJobId   (BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size)
{
	uint8_t *rx;
	size_t txLen = 2;
	int fifo_full, oon_irq, gn_irq;
	handle->txBuf[0] = SPI_CMD_READ_JOB_ID;
	handle->txBuf[1] = chipId;

	_WriteDummy(handle, txLen);
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

	memcpy(res, rx, res_size);

	// rx[0]: [31:24] OON Job ID
	// rx[1]: [23:16] GN Job ID
	// rx[2]: [10:8]  10/9/8 : Result FIFO Full/OON IRQ/GN IRQ Flag
	// rx[3]: [7:0]   Chip ID
	fifo_full = rx[2] & (1<<2);
	oon_irq = rx[2] & (1<<1);
	gn_irq = rx[2] & (1<<0);

#if (DEBUG_RESULT)
	NxDbgMsg(NX_DBG_INFO, " <== OON jobId(%d) GN jobid(%d) ChipId(%d) %s %s %s\n",
				rx[0], rx[1], rx[3],
				(0 != fifo_full) ? "FIFO is full":"",
				(0 != oon_irq) ? "OON IRQ":"",
				(0 != gn_irq) ? "GN IRQ":"");
#endif

	return 0;
}

int Btc08ReadResult  (BTC08_HANDLE handle, uint8_t chipId, uint8_t* gn, uint8_t gn_size)
{
	uint8_t *rx;
	size_t txLen=2;
	uint8_t lower = 0x00, lower2 = 0x00, lower3 = 0x00, upper = 0x00, validCnt = 0x00;
	handle->txBuf[0] = SPI_CMD_READ_RESULT;
	handle->txBuf[1] = chipId;

	_WriteDummy(handle, txLen);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, READ_RESULT_LEN+DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	rx = handle->rxBuf + txLen;

	memcpy(gn, rx, gn_size);
	// rx[  0-3]: [143:112] read_golden_nonce of Inst_Upper(when no golden nonce, just 0)
	// rx[  4-7]:  [111:80] read_golden_nonce of Inst_Lower(when no golden nonce, just 0)
	// rx[ 8-11]:   [79:48] read_golden_nonce of Inst_Lower2(when no golden nonce, just 0)
	// rx[12-15]:   [47:16] read_golden_nonce of Inst_Lower3(when no golden nonce, just 0)
	// rx[   16]:    [15:8] read ValidCnt in Core(use this value for calculating real golden nonce)
	// rx[   17]:     [7:4] Reserved
	//                  [3] Inst_Lower_3 found golden nonce
	//                  [2] Inst_Lower_2 found golden nonce
	//                  [1] Inst_Lower found golden nonce
	//                  [0] Inst_Upper found golden nonce
	validCnt = rx[16];
	lower3   = rx[17] & (1<<3);
	lower2   = rx[17] & (1<<2);
	lower    = rx[17] & (1<<1);
	upper    = rx[17] & (1<<0);

#if DEBUG_RESULT
	if (0 != upper) {
		NxDbgMsg(NX_DBG_INFO, " <== Inst_Upper found golden nonce %02x %02x %02x %02x \n", rx[0], rx[1], rx[2], rx[3]);
	}
	if (0 != lower) {
		NxDbgMsg(NX_DBG_INFO, " <== Inst_Lower found golden nonce %02x %02x %02x %02x \n", rx[4], rx[5], rx[6], rx[7]);
	}
	if (0 != lower2) {
		NxDbgMsg(NX_DBG_INFO, " <== Inst_Lower_2 found golden nonce %02x %02x %02x %02x \n", rx[8], rx[9], rx[10], rx[11]);
	}
	if (0 != lower3) {
		NxDbgMsg(NX_DBG_INFO, " <== Inst_Lower_3 found golden nonce %02x %02x %02x %02x \n", rx[12], rx[13], rx[14], rx[15]);
	}
#endif

	return 0;
}


int Btc08ClearOON    (BTC08_HANDLE handle, uint8_t chipId )
{
	handle->txBuf[0] = SPI_CMD_CLEAR_OON;
	handle->txBuf[1] = chipId;

	_WriteDummy(handle, 2);
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

	_WriteDummy(handle, txLen);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	return 0;
}


int Btc08ReadDisable (BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size)
{
	uint8_t *rx;
	size_t txLen = 2;

	if (BCAST_CHIP_ID == chipId)
	{
		NxDbgMsg(NX_DBG_ERR, "[%s] failed, wrong chip id\n", __FUNCTION__);
		return -1;
	}

	handle->txBuf[0] = SPI_CMD_READ_DISABLE;
	handle->txBuf[1] = chipId;

	_WriteDummy(handle, txLen);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, DISABLE_LEN + DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	rx = handle->rxBuf + txLen;
	memcpy(res, rx, res_size);

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

#if DEBUG_RESULT
	if (handle->txBuf[4] & LAST_CHIP_FLAG)
	{
		NxDbgMsg(NX_DBG_INFO, " ==> Set a last chip (chip_id %d)\n", chipId);
	}
	if (handle->txBuf[5] & OON_IRQ_ENB)
	{
		NxDbgMsg(NX_DBG_INFO, " ==> Set OON IRQ Enable\n");
	}
#endif

	_WriteDummy(handle, 6);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, 2+4, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}
	return 0;
}

/* WRITE_NONCE to set the range of nonce range */
int Btc08WriteNonce  (BTC08_HANDLE handle, uint8_t chipId, uint8_t *startNonce, uint8_t *endNonce )
{
	size_t txLen = 2;
	handle->txBuf[0] = SPI_CMD_WRITE_NONCE;
	handle->txBuf[1] = chipId;

	memcpy(handle->txBuf + txLen, startNonce, NONCE_LEN);	txLen += NONCE_LEN;
	memcpy(handle->txBuf + txLen, endNonce  , NONCE_LEN);	txLen += NONCE_LEN;

	_WriteDummy(handle, txLen);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}
	return 0;
}


int Btc08ReadHash    (BTC08_HANDLE handle, uint8_t chipId, uint8_t* hash, uint8_t hash_size)
{
	uint8_t *rx;
	size_t txLen = 2;
	handle->txBuf[0] = SPI_CMD_READ_HASH;
	handle->txBuf[1] = chipId;

	_WriteDummy(handle, txLen);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, TOTAL_HASH_LEN+DUMMY_BYTES) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	rx = handle->rxBuf + txLen;
	// rx  [0~31] [1023:768] Result Hash of Inst_Upper (when no golden nonce, just 0)
	// rx [32~63]  [767:512] Result Hash of Inst_Lower (when no golden nonce, just 0)
	// rx [64~95]  [511:256] Result Hash of Inst_Lower_2 (when no golden nonce, just 0)
	// rx[96~127]    [255:0] Result Hash of Inst_Lower_3 (when no golden nonce, just 0)

	memcpy(hash, rx, hash_size);

	return 0;
}

int Btc08ReadDebugCnt(BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size)
{
	uint8_t *rx;
	size_t txLen = 2;

	if (BCAST_CHIP_ID == chipId)
	{
		NxDbgMsg(NX_DBG_ERR, "[%s] failed, wrong chip id\n", __FUNCTION__);
		return -1;
	}

	handle->txBuf[0] = SPI_CMD_READ_DEBUGCNT;
	handle->txBuf[1] = chipId;

	_WriteDummy(handle, txLen);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, 4+DUMMY_BYTES) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	rx = handle->rxBuf + txLen;
	// rx[0-3] [31:0] DebugCnt

	uint32_t debugcnt;
	uint32_t *p1 = (uint32_t *)&(rx[0]);
	debugcnt = *p1;
	debugcnt = bswap_32(debugcnt);
	NxDbgMsg(NX_DBG_ERR, " rx[0]=0x%02x, rx[1]=0x%02x, rx[2]=0x%02x, rx[3]=0x%02x debugcnt:%u(0x%08x)<==\n",
				rx[0], rx[1], rx[2], rx[3], debugcnt, debugcnt);

	memcpy(res, rx, res_size);

	return 0;
}

int Btc08Debug(BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size)
{
	uint8_t *rx;
	size_t txLen = 2;

	if (BCAST_CHIP_ID == chipId)
	{
		NxDbgMsg(NX_DBG_ERR, "[%s] failed, wrong chip id\n", __FUNCTION__);
		return -1;
	}

	handle->txBuf[0] = SPI_CMD_DEBUG;
	handle->txBuf[1] = chipId;

	_WriteDummy(handle, txLen);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, 4+DUMMY_BYTES) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	rx = handle->rxBuf + txLen;
	// rx[0-3] [31:0] DebugCnt

	uint32_t debugcnt;
	uint32_t *p1 = (uint32_t *)&(rx[0]);
	debugcnt = *p1;
	debugcnt = bswap_32(debugcnt);
	NxDbgMsg(NX_DBG_ERR, " rx[0]=0x%02x, rx[1]=0x%02x, rx[2]=0x%02x, rx[3]=0x%02x debugcnt:%u(0x%08x)<==\n",
				rx[0], rx[1], rx[2], rx[3], debugcnt, debugcnt);

	memcpy(res, rx, res_size);

	return 0;
}

int Btc08WriteIOCtrl(BTC08_HANDLE handle, uint8_t chipId, uint8_t *ioctrl)
{
	size_t txLen = 2;
	handle->txBuf[0] = SPI_CMD_WRITE_IO_CTRL;
	handle->txBuf[1] = chipId;

	memcpy(handle->txBuf + txLen, ioctrl, IOCTRL_LEN);	txLen += IOCTRL_LEN;

	_WriteDummy(handle, txLen);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}
	return 0;
}

int Btc08ReadIOCtrl(BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size)
{
	uint8_t *rx;
	size_t txLen = 2;

	if (BCAST_CHIP_ID == chipId)
	{
		NxDbgMsg(NX_DBG_ERR, "[%s] failed, wrong chip id\n", __FUNCTION__);
		return -1;
	}

	handle->txBuf[0] = SPI_CMD_READ_IO_CTRL;
	handle->txBuf[1] = chipId;

	_WriteDummy(handle, txLen);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, IOCTRL_LEN+DUMMY_BYTES) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	rx = handle->rxBuf + txLen;
	// ret[0~2]  [127:104] Reserved
	// ret[3~5]   [103:84] IS(0: CMOS input, 1: Schmitt Trigger Input)
	//             [83:80] EXTCLK Pad Control (PS:[3], PE:[2], DS1[1], DS0[0])
	// ret[6-7]    [79:64] SR(slew rate control)
	// ret[8-9]    [63:48] PS(weak resistor control)
	// ret[10-11]  [47:32] PE(Pull up/down enable)
	// ret[12-13]  [31:16] DS1(drive strength)
	// ret[14-15]   [15:0] DS0(drive strength)

	memcpy(res, rx, res_size);

	return 0;
}

int Btc08ReadFeature (BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size)
{
	uint8_t *rx;
	size_t txLen = 2;

	if (BCAST_CHIP_ID == chipId)
	{
		NxDbgMsg(NX_DBG_ERR, "[%s] failed, wrong chip id\n", __FUNCTION__);
		return -1;
	}

	handle->txBuf[0] = SPI_CMD_READ_FEATURE;
	handle->txBuf[1] = chipId;

	_WriteDummy(handle, txLen);
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
	NxDbgMsg(NX_DBG_DEBUG, " <== type(%s), hash_depth(%02X)\n",
				((rx[1]&8) == 0x00) ? "FPGA" : ((rx[1]&8) == 0x05) ? "ASIC":"Unkown", rx[3]);

	memcpy(res, rx, res_size);
	return 0;
}


int Btc08ReadRevision(BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size)
{
	uint8_t *rx;
	size_t txLen = 2;

	if (BCAST_CHIP_ID == chipId)
	{
		NxDbgMsg(NX_DBG_ERR, "[%s] failed, wrong chip id\n", __FUNCTION__);
		return -1;
	}

	handle->txBuf[0] = SPI_CMD_READ_REVISION;
	handle->txBuf[1] = chipId;

	_WriteDummy(handle, txLen);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, 4+DUMMY_BYTES) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	rx = handle->rxBuf + txLen;
	// Read revision date info (year, month, day, index)
	// rx[0]: year
	// rx[1]: month
	// rx[2]: day
	// rx[3]: index
	memcpy(res, rx, res_size);

	return 0;
}

int Btc08SetPllFoutEn(BTC08_HANDLE handle, uint8_t chipId, uint8_t fout)
{
	size_t txLen = 4;

	handle->txBuf[0] = SPI_CMD_SET_PLL_FOUT_EN;
	handle->txBuf[1] = chipId;
	handle->txBuf[2] = 0;
	handle->txBuf[3] = (fout&1);

	_WriteDummy(handle, 4);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}
	return 0;
}

int Btc08SetPllResetB(BTC08_HANDLE handle, uint8_t chipId, uint8_t reset)
{
	size_t txLen = 4;

	handle->txBuf[0] = SPI_CMD_SET_PLL_RESETB;
	handle->txBuf[1] = chipId;
	handle->txBuf[2] = 0;
	handle->txBuf[3] = (reset&1);

	_WriteDummy(handle, 4);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}
	return 0;
}

int Btc08SetTmode (BTC08_HANDLE handle, uint8_t chipId, uint8_t *tmode_sel)
{
	size_t txLen = 0;
	handle->txBuf[0] = SPI_CMD_SET_TMODE;
	handle->txBuf[1] = chipId;

	txLen += 2;
	memcpy( handle->txBuf+txLen, tmode_sel, TMODE_SEL_LEN );
	txLen += TMODE_SEL_LEN;

	_WriteDummy(handle, txLen);
	if( 0 > SpiTransfer( handle->hSpi, handle->txBuf, handle->rxBuf, txLen, DUMMY_BYTES ) )
	{
		// SPI Error
		NxDbgMsg(NX_DBG_ERR, "[%s] spi transfer error!!!\n", __FUNCTION__);
		return -1;
	}

	return 0;
}

void SetPllConfigByIdx(BTC08_HANDLE handle, int chipId, int pll_idx)
{
	// seq1. Disable FOUT
	Btc08SetPllFoutEn(handle, chipId, FOUT_EN_DISABLE);

	// seq3. Set PLL(change PMS value)
	Btc08SetPllConfig(handle, pll_idx);

	// seq2. Down reset
	Btc08SetPllResetB(handle, chipId, RESETB_RESET);

	// seq4. Up reset
	Btc08SetPllResetB(handle, chipId, RESETB_ON);

	// seq4. wait for 1 ms
	usleep(1000);

	// seq5. Enable FOUT
	Btc08SetPllFoutEn(handle, chipId, FOUT_EN_ENABLE);
}

int ReadPllLockStatus(BTC08_HANDLE handle, int chipId)
{
	int lock_status;
	uint8_t res[4] = {0x00,};
	unsigned int res_size = sizeof(res)/sizeof(res[0]);

	lock_status = Btc08ReadPll(handle, chipId, res, 4);
	NxDbgMsg(NX_DBG_DEBUG, "ReadPllLockStatus: res[0]:0x%02x, res[1]:0x%02x, res[2]:0x%02x, res[3]:0x%02x\n",
			res[0], res[1], res[2], res[3]);
	DumpPllValue(res);
	NxDbgMsg(NX_DBG_INFO, "chip#%d is %s\n", chipId,
			(lock_status == STATUS_LOCKED)?"locked":"unlocked");

	return lock_status;
}

int SetPllFreq(BTC08_HANDLE handle, int freq)
{
	int pll_idx = 0;

	pll_idx = GetPllFreq2Idx(freq);
	if (pll_idx < 0) {
		NxDbgMsg(NX_DBG_ERR, "Failed to set the correct PLL Freq!!!\n");
		return -1;
	} else {
		NxDbgMsg(NX_DBG_INFO, "pll_idx:%d\n", pll_idx);
	}

	for (int chipId = 1; chipId <= handle->numChips; chipId++)
	{
		SetPllConfigByIdx(handle, chipId, pll_idx);
		ReadPllLockStatus(handle, chipId);
	}

	return 0;
}
