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
//	File		: Btc08.h
//	Description	:
//	Author		: SeongO.Park (ray@coasia.com)
//	Export		:
//	History		:
//
//------------------------------------------------------------------------------

#ifndef _BTC08_H_
#define _BTC08_H_

#include <stdint.h>
#include <GpioControl.h>
#include <Spi.h>
#include <stdbool.h>

#define BCAST_CHIP_ID		0x00

// RUN_JOB Param Bits
#define DEBUGCNT_ON         (1<<4)
#define ASIC_BOOST_EN		(1<<1)
#define FIX_NONCE_MODE		(1<<0)

// SET_CONTROL Extra Bits
#define LAST_CHIP		    (1<<15)
#define OON_IRQ_EN		    (1<<4)
#define UART_DIVIDER    	(0x03)

#define MAX_JOB_FIFO_NUM	4
#define MAX_JOB_ID			256
#define MAX_CHIP_NUM		22

typedef struct tag_BTC08_INFO *BTC08_HANDLE;

#define	SPI_MAX_TRANS	(1024)
#define MAX_CHIPS		(32)
#define MAX_CORES		(255)

struct tag_BTC08_INFO{
	GPIO_HANDLE		hReset;
	GPIO_HANDLE		hGn;
	GPIO_HANDLE		hOon;
	SPI_HANDLE		hSpi;

	bool			isAsicBoost;

	int32_t			numChips;
	int32_t			numCores[MAX_CHIPS];

	uint8_t			startNonce[MAX_CHIPS][4];
	uint8_t			endNonce[MAX_CHIPS][4];

	uint8_t			txBuf[SPI_MAX_TRANS];
	uint8_t			rxBuf[SPI_MAX_TRANS];
};

typedef enum {
    GPIO_TYPE_OON,
    GPIO_TYPE_GN,
    GPIO_TYPE_RESET
} GPIO_TYPE;

//
//	pre-defined : SPI index & GN/OON/RESET pins
//		0 : spi port 0 & Related GPIO
//		1 : spi port 1 & Related GPIO
//
BTC08_HANDLE CreateBtc08( int32_t index );
void DestroyBtc08( BTC08_HANDLE handle );

int Btc08ResetHW     (BTC08_HANDLE handle, int32_t enable);
int Btc08GpioGetValue (BTC08_HANDLE handle, GPIO_TYPE type);
int	Btc08ReadId      (BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size);
int Btc08AutoAddress (BTC08_HANDLE handle);
int Btc08RunBist     (BTC08_HANDLE handle, uint8_t *hash, uint8_t *hash2, uint8_t *hash3, uint8_t *hash4);
uint8_t * Btc08ReadBist    (BTC08_HANDLE handle, uint8_t chipId);
int Btc08Reset       (BTC08_HANDLE handle);			//	S/W Reset
int Btc08SetPllConfig(BTC08_HANDLE handle, uint8_t idx);
int Btc08ReadPll     (BTC08_HANDLE handle);
int Btc08WriteParam  (BTC08_HANDLE handle, uint8_t chipId, uint8_t *midState, uint8_t *param);
int Btc08ReadParam   (BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size);
int Btc08WriteTarget (BTC08_HANDLE handle, uint8_t chipId, uint8_t *target);
int Btc08ReadTarget  (BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size );
int Btc08RunJob      (BTC08_HANDLE handle, uint8_t chipId, uint8_t option, uint8_t jobId);
int Btc08ReadJobId   (BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size);
int Btc08ReadResult  (BTC08_HANDLE handle, uint8_t chipId, uint8_t* gn, uint8_t gn_size);
int Btc08ClearOON    (BTC08_HANDLE handle, uint8_t chipId);
int Btc08SetDisable  (BTC08_HANDLE handle, uint8_t chipId, uint8_t *disable);
int Btc08ReadDisable (BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size);
int Btc08SetControl  (BTC08_HANDLE handle, uint8_t chipId, uint32_t param);
int Btc08ReadTemp    (BTC08_HANDLE handle, uint8_t chipId);
int Btc08WriteNonce  (BTC08_HANDLE handle, uint8_t chipId, uint8_t *startNonce, uint8_t *endNonce);
int Btc08ReadHash    (BTC08_HANDLE handle, uint8_t chipId, uint8_t* hash, uint8_t hash_size);
int Btc08ReadDebugCnt(BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size);
int Btc08ReadIOCtrl(BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size);
int Btc08ReadFeature (BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size);
int Btc08ReadRevision(BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size);
int Btc08SetPllFoutEn(BTC08_HANDLE handle, uint8_t chipId, uint8_t fout);
int Btc08SetPllResetB(BTC08_HANDLE handle, uint8_t chipId, uint8_t reset);

#endif // _BTC08_H_