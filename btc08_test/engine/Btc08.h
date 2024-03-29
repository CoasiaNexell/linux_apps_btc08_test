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
#include <pthread.h>

#include "PllCtrl.h"
#include "TestVector.h"
#include "config.h"
#define BCAST_CHIP_ID		0x00

// RUN_JOB Param Bits
#define DEBUGCNT_ON         (1<<4)
#define ASIC_BOOST_EN		(1<<1)
#define FIX_NONCE_MODE		(1<<0)

// SET_CONTROL Extra Bits
#define LAST_CHIP		    (1<<15)
#define OON_IRQ_EN		    (1<<4)
#define UART_DIVIDER    	(0x0F)

#define MAX_JOB_FIFO_NUM	4
#define MAX_JOB_ID			256
#define MAX_CHIP_NUM		22

#define FOUT_EN_DISABLE     0
#define FOUT_EN_ENABLE      1

#define RESETB_RESET        0
#define RESETB_ON           1

typedef struct tag_BTC08_INFO *BTC08_HANDLE;

#if USE_BTC08_FPGA
#define BTC08_NUM_CORES     1
#else
#define BTC08_NUM_CORES     30
#endif
#define MAX_JOB_FIFO        4
#define JOB_ID_NUM_MASK     (MAX_JOB_FIFO*2-1)	/* total 7 */
#define MAX_JOB_INDEX		7

#define	SPI_MAX_TRANS	(1024)
#define MAX_CHIPS		(32)
#define MAX_CORES		(255)
#define HW_RESET_TIME	(200000)		//	200 msec

struct tag_BTC08_INFO {
	GPIO_HANDLE		hReset;
	GPIO_HANDLE		hGn;
	GPIO_HANDLE		hOon;
	GPIO_HANDLE		hKey0;
	SPI_HANDLE		hSpi;

	bool			isAsicBoost;
	VECTOR_DATA     work[JOB_ID_NUM_MASK];

	uint8_t         last_queued_id;

	uint8_t         fault_chip_id;

	int32_t			numChips;
	int32_t			numCores[MAX_CHIPS];

	uint8_t			startNonce[MAX_CHIPS][4];
	uint8_t			endNonce[MAX_CHIPS][4];

	uint8_t			txBuf[SPI_MAX_TRANS];
	uint8_t			rxBuf[SPI_MAX_TRANS];
};

struct BTC08_INFO {
	BTC08_HANDLE handle;

	uint8_t disable_core_num;
	uint8_t is_full_nonce;
	uint8_t is_diff_range;
	uint8_t chipId;
	uint8_t idx_data;
	int pll_freq;
	int delay;

	uint8_t jobId;
	uint8_t numChips;
	bool chip_enable[MAX_CHIPS];
	bool chip_isdone[MAX_CHIPS];
	VECTOR_DATA chip_data;

	bool isDone;
	uint32_t numGN;
	uint32_t numOON;

	pthread_t pThread;
	pthread_mutex_t lock;
};

typedef enum {
    GPIO_TYPE_OON,
    GPIO_TYPE_GN,
    GPIO_TYPE_RESET
} GPIO_TYPE;

typedef enum {
	BOARD_TYPE_FPGA,
	BOARD_TYPE_ASIC
} BOARD_TYPE;

#define STATUS_LOCKED     1
#define STATUS_UNLOCKED   0

#define GPIO_RESET_0            GPIOD31
#define GPIO_IRQ_OON_0          GPIOD29
#define GPIO_IRQ_GN_0           GPIOD30

#define GPIO_RESET_1             GPIOE4
#define GPIO_IRQ_OON_1           GPIOE2
#define GPIO_IRQ_GN_1            GPIOE3

#define GPIO_HASH0_PLUG         GPIOA24		// ACTIVE_HIGH, High: Hash0 connected, Low: Hash0 removed
#define GPIO_HASH0_BODDET       GPIOA20		// High: Hash0, Low: VTK
#define GPIO_HASH0_PWREN         GPIOA0		// ACTIVE_HIGH, High: FAN ON, Low : FAN OFF

#define GPIO_HASH1_PLUG         GPIOA11		// ACTIVE_HIGH, High: Hash1 connected, Low: Hash1 removed
#define GPIO_HASH1_BODDET        GPIOA9		// High: Hash1, Low: VTK
#define GPIO_HASH1_PWREN        GPIOA16		// ACTIVE_HIGH, High: FAN ON, Low : FAN OFF

#define GPIO_KEY_0               GPIOA3

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
int Btc08RunBist     (BTC08_HANDLE handle, uint8_t chipId, uint8_t *hash, uint8_t *hash2, uint8_t *hash3, uint8_t *hash4);
uint8_t * Btc08ReadBist    (BTC08_HANDLE handle, uint8_t chipId);
int Btc08Reset       (BTC08_HANDLE handle, uint8_t chipId);			//	S/W Reset
int Btc08SetPllConfig(BTC08_HANDLE handle, uint8_t chipId, uint8_t idx);
int Btc08SetPllConfigByPass(BTC08_HANDLE handle, uint8_t idx);
int Btc08ReadPll     (BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size);
int Btc08WriteParam  (BTC08_HANDLE handle, uint8_t chipId, uint8_t *midState, uint8_t *param);
int Btc08ReadParam   (BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size);
int Btc08WriteTarget (BTC08_HANDLE handle, uint8_t chipId, uint8_t *target);
int Btc08ReadTarget  (BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size );
int Btc08RunJob      (BTC08_HANDLE handle, uint8_t chipId, uint8_t option, uint8_t jobId);
int Btc08ReadJobId   (BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size);
int Btc08ReadResult  (BTC08_HANDLE handle, uint8_t chipId, uint8_t* gn, uint8_t gn_size);
int Btc08ClearOON    (BTC08_HANDLE handle, uint8_t chipId);
int Btc08SetDisable  (BTC08_HANDLE handle, uint8_t chipId, uint8_t *disable);
int Btc08WriteCoreCfg(BTC08_HANDLE handle, uint8_t chipId, uint8_t coreCnt );
int Btc08ReadDisable (BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size);
int Btc08SetControl  (BTC08_HANDLE handle, uint8_t chipId, uint32_t param);
int Btc08WriteNonce  (BTC08_HANDLE handle, uint8_t chipId, uint8_t *startNonce, uint8_t *endNonce);
int Btc08ReadHash    (BTC08_HANDLE handle, uint8_t chipId, uint8_t* hash, uint8_t hash_size);
int Btc08ReadDebugCnt(BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size);
int Btc08WriteIOCtrl (BTC08_HANDLE handle, uint8_t chipId, uint8_t *ioctrl);
int Btc08ReadIOCtrl  (BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size);
int Btc08ReadFeature (BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size);
int Btc08ReadRevision(BTC08_HANDLE handle, uint8_t chipId, uint8_t* res, uint8_t res_size);
int Btc08SetPllFoutEn(BTC08_HANDLE handle, uint8_t chipId, uint8_t fout);
int Btc08SetPllResetB(BTC08_HANDLE handle, uint8_t chipId, uint8_t reset);
int Btc08SetTmode    (BTC08_HANDLE handle, uint8_t chipId, uint8_t *tmode_sel);

// PLL Seq APIs
void SetPllConfigByIdx(BTC08_HANDLE handle, int chipId, int pll_idx);
int  ReadPllLockStatus(BTC08_HANDLE handle, int chipId);
int  SetPllFreq       (BTC08_HANDLE handle, int freq);

void ReadId(BTC08_HANDLE handle);
void ReadBist(BTC08_HANDLE handle);
void RunBist(BTC08_HANDLE handle);
BOARD_TYPE get_board_type(BTC08_HANDLE handle);
void DistributionNonce(BTC08_HANDLE handle, uint8_t start_nonce[4], uint8_t end_nonce[4]);

int plug_status_0, board_type_0;
int plug_status_1, board_type_1;
#endif // _BTC08_H_
