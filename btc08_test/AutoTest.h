#ifndef _AUTOTEST_H_
#define _AUTOTEST_H_

#ifdef __cplusplus
extern "C" {
#endif

void BistWithDisable( int interval, int repeatCnt );
void EnableOneCorePosition( int interval, int repeatCnt );
void AutoTestMining( int interval, int repeatCnt );
void DebugPowerBIST( int freq, int interval );
void MiningWithoutBist( uint8_t disable_core_num, uint32_t pll_freq,
		uint8_t is_full_nonce, uint8_t fault_chip_id, uint8_t is_infinite_mining );
void DbgGpioOn();
void DbgGpioOff();

void DbgGpioOn2();
void DbgGpioOff2();

#ifdef __cplusplus
}
#endif

#endif // _AUTOTEST_H_
