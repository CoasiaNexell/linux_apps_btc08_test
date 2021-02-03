//------------------------------------------------------------------------------
//
//	Copyright (C) 2016 Nexell Co. All Rights Reserved
//	Nexell Co. Proprietary & Confidential
//
//	NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
//  AND	WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
//  FOR A PARTICULAR PURPOSE.
//
//	Module		:
//	File		:
//	Description	:
//	Author		:
//	Export		:
//	History		:
//
//------------------------------------------------------------------------------

#ifndef __NX_DBGMSG_H__
#define __NX_DBGMSG_H__

#include <stdint.h>

#define NX_DBG_VBS			2	// ANDROID_LOG_VERBOSE
#define NX_DBG_DEBUG		3	// ANDROID_LOG_DEBUG
#define	NX_DBG_INFO			4	// ANDROID_LOG_INFO
#define	NX_DBG_WARN			5	// ANDROID_LOG_WARN
#define	NX_DBG_ERR			6	// ANDROID_LOG_ERROR
#define NX_DBG_DISABLE		9

#ifndef NX_DTAG
#define	NX_DTAG		"[]"
#endif

// #ifdef	NX_DBG_ON
// #undef	NX_DBG_ON
// #define NX_DBG_ON	1
// #endif

#ifndef NX_DBG_OFF
#ifndef ANDROID

#include <stdio.h>
#define DBG_PRINT			printf
#define NxTrace(...)		do {										\
								DBG_PRINT(NX_DTAG);						\
								DBG_PRINT(" ");							\
								DBG_PRINT(__VA_ARGS__);					\
							} while(0)

#define NxDbgMsg(A, ...)	do {										\
								if( gNxFilterDebugLevel <= A ) {		\
									DBG_PRINT(NX_DTAG);					\
									DBG_PRINT(" ");						\
									DBG_PRINT(__VA_ARGS__);				\
								}										\
							} while(0)

#else

#include <android/log.h>
#define DBG_PRINT			__android_log_print
#define NxTrace(...)		DBG_PRINT(ANDROID_LOG_VERBOSE, NX_DTAG, __VA_ARGS__);

#define NxDbgMsg(A, ...)	do {										\
								if( gNxFilterDebugLevel <= A ) {		\
									DBG_PRINT(A, NX_DTAG, __VA_ARGS__);	\
								}										\
							} while(0)

#endif
#else

#define NxTrace(...)
#define NxDbgMsg(A, ...)
#endif

extern uint32_t gNxFilterDebugLevel;

void	NxChgFilterDebugLevel( uint32_t level );

#endif	// __NX_DBGMSG_H__
