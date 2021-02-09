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

typedef struct tag_BTC08_INFO *BTC08_HANDLE;

//
//	pre-defined : SPI index & GN/OON/RESET pins
//		0 : spi port 0 & Related GPIO
//		1 : spi port 1 & Related GPIO
//
BTC08_HANDLE CreateBtc08( int32_t index );
void DestroyBtc08( BTC08_HANDLE handle );

int Btc08ResetHW     (BTC08_HANDLE handle, int32_t enable);
int	Btc08ReadId      (BTC08_HANDLE handle, uint8_t chipId);
int Btc08AutoAddress (BTC08_HANDLE handle);
int Btc08RunBist     (BTC08_HANDLE handle, uint8_t *hash, uint8_t *hash2, uint8_t *hash3, uint8_t *hash4);
uint8_t * Btc08ReadBist    (BTC08_HANDLE handle, uint8_t chipId);
int Btc08Reset       (BTC08_HANDLE handle);			//	S/W Reset
int Btc08SetPllConfig(BTC08_HANDLE handle, uint8_t idx);
int Btc08ReadPll     (BTC08_HANDLE handle);
int Btc08WriteParam  (BTC08_HANDLE handle, uint8_t chipId, uint8_t *midState, uint8_t *param );
int Btc08ReadParam   (BTC08_HANDLE handle, uint8_t chipId);
int Btc08WriteTarget (BTC08_HANDLE handle, uint8_t chipId, uint8_t *target );
int Btc08ReadTarget  (BTC08_HANDLE handle, uint8_t chipId );
int Btc08RunJob      (BTC08_HANDLE handle, uint8_t chipId, uint8_t jobId );
int Btc08ReadJobId   (BTC08_HANDLE handle, uint8_t chipId );
int Btc08ReadResult  (BTC08_HANDLE handle, uint8_t chipId );
int Btc08ClearOON    (BTC08_HANDLE handle, uint8_t chipId );
int Btc08SetDisable  (BTC08_HANDLE handle, uint8_t chipId, uint8_t *disable );
int Btc08ReadDisable (BTC08_HANDLE handle, uint8_t chipId );
int Btc08SetControl  (BTC08_HANDLE handle, uint8_t chipId, uint32_t param );
int Btc08ReadTemp    (BTC08_HANDLE handle, uint8_t chipId );
int Btc08WriteNonce  (BTC08_HANDLE handle, uint8_t chipId, uint8_t *startNonce, uint8_t *endNonce );
int Btc08ReadHash    (BTC08_HANDLE handle, uint8_t chipId );
int Btc08ReadFeature (BTC08_HANDLE handle, uint8_t chipId );
int Btc08ReadRevision(BTC08_HANDLE handle, uint8_t chipId );
int Btc08SetPllFoutEn(BTC08_HANDLE handle, uint8_t chipId, uint8_t fout);
int Btc08SetPllResetB(BTC08_HANDLE handle, uint8_t chipId, uint8_t reset);

#endif // _BTC08_H_