#ifndef _AUTOTEST_H_
#define _AUTOTEST_H_

#ifdef __cplusplus
extern "C" {
#endif

void BistWithDisable( int interval, int repeatCnt );
void EnableOneCorePosition( int interval, int repeatCnt );
void AutoTestMining( int interval, int repeatCnt );
void DebugPowerBIST( int freq, int interval );
void MiningWithoutBist( int interval , int freq );
void DbgGpioOn();
void DbgGpioOff();

void DbgGpioOn2();
void DbgGpioOff2();

#ifdef __cplusplus
}
#endif

#endif // _AUTOTEST_H_
