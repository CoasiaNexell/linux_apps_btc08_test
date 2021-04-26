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
//	Module		: BTC08 Pll Control Module
//	File		: PllCtrl.h
//	Description	:
//	Author		:
//	Export		:
//	History		:
//
//------------------------------------------------------------------------------

#ifndef _PLLCTRL_H_
#define _PLLCTRL_H_

#define TEMPERATURE_THREASHOLD	100.
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

#define NUM_PLL_SET (sizeof(pll_sets)/sizeof(struct pll_conf))

int GetPllFreq2Idx(int pll_freq);
int GetPllIdx2Freq(int pll_idx);

#endif // _PLLCTRL_H_

