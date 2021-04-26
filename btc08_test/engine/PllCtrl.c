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