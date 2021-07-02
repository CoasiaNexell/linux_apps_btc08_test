#ifndef _SINGLECOMMAND_H_
#define _SINGLECOMMAND_H_

void SingleCommandLoop(void);

void TestDisableCore( BTC08_HANDLE handle, uint8_t disable_core_num, uint32_t pll_freq, uint8_t is_full_nonce, uint8_t fault_chip_id );
void TestBist( BTC08_HANDLE handle, uint8_t disable_core_num, int pll_freq, int wait_gpio );

extern unsigned int gDisableCore;

#endif // _SINGLECOMMAND_H_