#ifndef _SINGLECOMMAND_H_
#define _SINGLECOMMAND_H_

void SingleCommandLoop(void);

//
//	return :
//		0 : success
//		-1 : failed mining
//		-2 : failed bist
//
int TestDisableCore( BTC08_HANDLE handle, uint8_t disable_core_num,
        uint32_t pll_freq, uint8_t is_full_nonce, uint8_t fault_chip_id, uint8_t is_infinite_mining );
void TestBist( BTC08_HANDLE handle, uint8_t disable_core_num, int pll_freq, int wait_gpio );
int TestMiningWithoutBist( BTC08_HANDLE handle, uint8_t disable_core_num,
		uint32_t pll_freq, uint8_t is_full_nonce, uint8_t fault_chip_id, uint8_t is_infinite_mining );
extern unsigned int gDisableCore;

#endif // _SINGLECOMMAND_H_