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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>	// open/read
#include <stdio.h>	// printf
#include <stdlib.h>	// atoi

#include "PllCtrl.h"

#ifdef NX_DTAG
#undef NX_DTAG
#endif
#define NX_DTAG "[PllCtrl]"
#include "NX_DbgMsg.h"

/* in: pll freq, out: pll table index */
int GetPllFreq2Idx(int pll_freq)
{
	int ret;
	struct pll_conf *plls;

	if(pll_freq < pll_sets[0].freq)
		return -1;
	if(pll_freq > pll_sets[NUM_PLL_SET-1].freq) {
		NxDbgMsg(NX_DBG_WARN, "set to Max Frequency setting (%d)", pll_sets[NUM_PLL_SET-1].freq);
		return (NUM_PLL_SET-1);
	}

	ret = 0;
	for(plls = &pll_sets[0]; plls; plls++) {
		if(pll_freq <= plls->freq) break;
		ret++;
	}
	return ret;
}

/* in: pll table index, out: pll freq */
int GetPllIdx2Freq(int pll_idx)
{
    if (pll_idx < 0 || pll_idx > NUM_PLL_SET)
        return 0;
    else
        return pll_sets[pll_idx].freq;
}

void DumpPllValue(uint8_t val[4])
{
// [31:29] : reserved
// [28]    : FSEL : 0
// [27]    : FEED_EN : 0
// [26:22] : EXTAFC : 5'h0
// [21]    : AFC_ENB : 0
// [20]    : DIV_SEL : 1
// [19]    : BYPASS : 0
// [18:16] : S : 3'h1, 3’b001
// [15:6]  : M : 10'h21C, 10’b10_0001_1100
// [5:0]   : P : 6'h06,6’b00_0110
	uint32_t P, M, S;
	uint32_t BYPASS;
	uint32_t DIV_SEL;
	uint32_t AFC_ENB;
	uint32_t EXTAFC;
	uint32_t FEED_EN;
	uint32_t F_SEL;
	uint32_t Reseved;

	P       =  val[3] & 0x3f;
	M       = (val[2] << 2) | ((val[3] * 0xC0)>>6);
	S       =  val[1] & 0x7;
	BYPASS  = (val[1] & 0x08) >> 3;
	DIV_SEL = (val[1] & 0x10) >> 4;
	AFC_ENB = (val[1] & 0x20) >> 5;
	EXTAFC  = ((val[0] & 0x7)<<2) | (val[0] & 0xC0)>>6;
	FEED_EN = (val[0] & 0x8)>>3;
	F_SEL   = (val[0] & 0x10)>>4;

	NxDbgMsg(NX_DBG_DEBUG, "================== 0x%02x 0x%02x 0x%02x 0x%02x\n", val[0], val[1], val[2], val[3]);
	NxDbgMsg(NX_DBG_DEBUG, "P       = %x(%d) \n", P, P );
	NxDbgMsg(NX_DBG_DEBUG, "M       = %x(%d) \n", M, M );
	NxDbgMsg(NX_DBG_DEBUG, "S       = %x(%d) \n", S, S );
	NxDbgMsg(NX_DBG_DEBUG, "BYPASS  = %x \n", BYPASS  );
	NxDbgMsg(NX_DBG_DEBUG, "DIV_SEL = %x \n", DIV_SEL );
	NxDbgMsg(NX_DBG_DEBUG, "AFC_ENB = %x \n", AFC_ENB );
	NxDbgMsg(NX_DBG_DEBUG, "EXTAFC  = %x \n", EXTAFC  );
	NxDbgMsg(NX_DBG_DEBUG, "FEED_EN = %x \n", FEED_EN );
	NxDbgMsg(NX_DBG_DEBUG, "F_SEL   = %x \n", F_SEL   );
	NxDbgMsg(NX_DBG_DEBUG, "==================\n");
}