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
//	Module		: SPI Module
//	File		: spi.h
//	Description	:
//	Author		: SeongO.Park (ray@coasia.com)
//	Export		:
//	History		:
//
//------------------------------------------------------------------------------

#ifndef _SPI_H_
#define _SPI_H_

#include <stdint.h>

#define TX_RX_MAX_SPEED      (2 * 1000 * 1000)     //  1 MHz
#define TX_MAX_SPEED        (20 * 1000 * 1000)     // 4 MHz

typedef struct tag_SPI_INFO *SPI_HANDLE;

SPI_HANDLE CreateSpi( const char *device, uint32_t mode, uint32_t speed, uint16_t delay, uint8_t bits );
void DestroySpi( SPI_HANDLE handle );
int SpiTransfer( SPI_HANDLE handle, uint8_t *tx, uint8_t *rx, int32_t txLen, int32_t rxLen  );

#endif // _SPI_H_