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
//	Module		: GPIO Control Module
//	File		: GpioControl.c
//	Description	:
//	Author		: SeongO.Park (ray@coasia.com)
//	Export		:
//	History		:
//
//------------------------------------------------------------------------------

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include "GpioControl.h"

#ifdef NX_DTAG
#undef NX_DTAG
#endif
#define NX_DTAG "[GpioControl]"
#include "NX_DbgMsg.h"


struct tag_GPIO_INFO {
	int32_t			iPort;
	int32_t			iDirection;
	int32_t			iEdge;
	int32_t			bInit;
	int32_t			bReset;
	pthread_mutex_t	hLock;
};


GPIO_HANDLE CreateGpio( int32_t iGpio )
{
	char buf[64];		//	for gpio export
	GPIO_HANDLE handle;
	int fd, len;
	

	if( iGpio <= GPIO_ERROR || iGpio >= GPIO_MAX )
	{
		NxDbgMsg(NX_DBG_ERR, "Fail, Check GPIO Number. ( %d )\n", iGpio );
		goto ERROR_EXIT;
	}

	snprintf( buf, sizeof(buf), "/sys/class/gpio/gpio%d", iGpio );

	if( !access(buf, F_OK) )
	{
		//	if already exporrted 
	}
	else
	{
		fd = open("/sys/class/gpio/export", O_WRONLY);

		if( 0 > fd )
		{
			NxDbgMsg(NX_DBG_ERR, "Fail, Open GPIO.\n" );
			goto ERROR_EXIT;
		}

		len = snprintf( buf, sizeof(buf), "%d", iGpio );

		if( 0 > write(fd, buf, len) )
		{
			NxDbgMsg(NX_DBG_ERR, "Fail, Write GPIO.\n" );
			close( fd );
			goto ERROR_EXIT;
		}

		close( fd );
	}

	handle = (GPIO_HANDLE)malloc( sizeof(struct tag_GPIO_INFO) );

	memset( handle, 0, sizeof(struct tag_GPIO_INFO) );
	handle->iPort		= iGpio;
	handle->iDirection	= GPIO_DIRECTION_IN;

	pthread_mutex_init( &handle->hLock, NULL );

	return handle;
ERROR_EXIT:
	return NULL;
}


void DestroyGpio( GPIO_HANDLE handle )
{
	int fd, len;
	char buf[64];

	if( !handle )
	{
		return;
	}

	//	unexport handle
	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	if( 0 > fd )
	{
		NxDbgMsg(NX_DBG_ERR, "Fail, Open GPIO.\n" );
		return;
	}
	len = snprintf( buf, sizeof(buf), "%d", handle->iPort );
	if( 0 > write(fd, buf, len) )
	{
		close( fd );
		NxDbgMsg(NX_DBG_ERR, "Fail, Write GPIO.\n" );
		return;
	}
	close( fd );

	pthread_mutex_destroy( &handle->hLock );

	free( handle );
 }


int32_t GpioSetDirection( GPIO_HANDLE handle, int32_t iDirection )
{
	int fd, len;
	char buf[64];

	if( !handle )
	{
		return -1;
	}

	if( iDirection < GPIO_DIRECTION_IN || iDirection > GPIO_DIRECTION_OUT )
	{
		NxDbgMsg(NX_DBG_ERR, "Fail, Check Direction. ( %d )\n", iDirection );
		return -1;
	}

	snprintf( buf, sizeof(buf), "/sys/class/gpio/gpio%d/direction", handle->iPort );

	fd = open( buf, O_WRONLY );
	if( 0 > fd )
	{
		NxDbgMsg(NX_DBG_ERR, "Fail, Open GPIO.\n" );
		return -1;
	}

	len = snprintf(buf, sizeof(buf), "%s", (iDirection == GPIO_DIRECTION_IN) ? "in" : "out");

	if( 0 > write( fd, buf, len ) )
	{
		NxDbgMsg(NX_DBG_ERR, "Fail, Write GPIO.\n" );
		close(fd);
		return -1;
	}
	close( fd );

	handle->iDirection = iDirection;
	return 0;
}


int32_t GpioSetValue( GPIO_HANDLE handle, int32_t iValue )
{
	int fd, len;
	char buf[64];

	if( !handle )
	{
		return -1;
	}

	if( iValue < 0 || iValue > 1 )
	{
		NxDbgMsg(NX_DBG_ERR, "Fail, Check Value. ( %d )\n", iValue );

		return -1;
	}

	snprintf( buf, sizeof(buf), "/sys/class/gpio/gpio%d/value", handle->iPort );

	fd = open(buf, O_RDWR);
	if( 0 > fd )
	{
		NxDbgMsg(NX_DBG_ERR, "Fail, Open GPIO.\n" );

		return -1;
	}

	len = snprintf(buf, sizeof(buf), "%d", iValue);

	if( 0 > write(fd, buf, len) )
	{
		close(fd);

		NxDbgMsg(NX_DBG_ERR, "Fail, Write GPIO.\n" );

		return -1;
	}

	close( fd );
	return 0;
}


int32_t GpioGetValue( GPIO_HANDLE handle )
{
	int fd;
	char buf[64];

	if( !handle )
	{
		return -1;
	}

	snprintf( buf, sizeof(buf), "/sys/class/gpio/gpio%d/value", handle->iPort );

	fd = open(buf, O_RDWR);
	if( 0 > fd )
	{
		NxDbgMsg(NX_DBG_ERR, "Fail, Open GPIO.\n" );
		return -1;
	}

	if( 0 > read(fd, buf, sizeof(buf)) )
	{
		close(fd);
		NxDbgMsg(NX_DBG_ERR, "Fail, Read GPIO.\n" );
		return -1;
	}

	close(fd);

	return atoi(buf);
}


int32_t GpioSetEdge( GPIO_HANDLE handle, int32_t iEdge )
{
	int fd, len;
	char buf[64];

	if( !handle )
	{
		return -1;
	}

	if( iEdge < GPIO_EDGE_NONE || iEdge > GPIO_EDGE_BOTH ) {
		NxDbgMsg(NX_DBG_ERR, "Fail, Check Edge.\n" );
		return -1;
	}

	snprintf( buf, sizeof(buf), "/sys/class/gpio/gpio%d/edge", handle->iPort );

	fd = open( buf, O_RDWR );
	if( 0 > fd )
	{
		NxDbgMsg(NX_DBG_ERR, "Fail, Open GPIO.\n" );
		return -1;
	}

	if( iEdge == GPIO_EDGE_FALLING )		len = snprintf( buf, sizeof(buf), "falling" );
	else if( iEdge == GPIO_EDGE_RIGING )	len = snprintf( buf, sizeof(buf), "rising" );
	else if( iEdge == GPIO_EDGE_BOTH )		len = snprintf( buf, sizeof(buf), "both" );
	else									len = snprintf( buf, sizeof(buf), "none" );

	if( 0 > write(fd, buf, len) )
	{
		close(fd);
		NxDbgMsg(NX_DBG_ERR, "Fail, Write GPIO.\n" );
		return -1;
	}

	close(fd);

	handle->iEdge = iEdge;
	return 0;
}


int32_t GpioWiatInterrupt( GPIO_HANDLE handle )
{
	int32_t fd = 0;
	int32_t hPoll = 0;
	struct pollfd   pollEvent;
	int32_t bRun = 1;
	char buf[64];

	if( !handle )
	{
		return -1;
	}

	pthread_mutex_lock( &handle->hLock );
	handle->bReset = 0;
	pthread_mutex_unlock( &handle->hLock );

	if( handle->iDirection == GPIO_DIRECTION_OUT ) {
		NxDbgMsg(NX_DBG_ERR, "Fail, Check Direction.\n" );
		return -1;
	}

	if( handle->iEdge == GPIO_EDGE_NONE ) {
		NxDbgMsg(NX_DBG_ERR, "Fail, Check Edge.\n");
		return -1;
	}

	snprintf( buf, sizeof(buf), "/sys/class/gpio/gpio%d/value", handle->iPort );

	if( 0 > (fd = open(buf, O_RDWR)) )
	{
		NxDbgMsg(NX_DBG_ERR, "Fail, Open GPIO.\n" );
		return -1;
	}

	// Dummy Read.
	memset( buf, 0x00, sizeof(buf) );
	if( 0 > read(fd, buf, sizeof(buf)) ) {
		close(fd);
		NxDbgMsg(NX_DBG_ERR, "Fail, Read GPIO.\n" );
		return -1;
	}

	memset( &pollEvent, 0x00, sizeof(pollEvent) );
	pollEvent.fd	= fd;
	pollEvent.events= POLLPRI;

	while( bRun )
	{
		hPoll = poll( (struct pollfd*)&pollEvent, 1, 100 );

		if( hPoll < 0 )
		{
			close(fd);

			NxDbgMsg(NX_DBG_ERR, "Fail, poll().\n" );
			return -1;
		}
		else if( hPoll > 0 )
		{
			if( pollEvent.revents & POLLPRI )
			{
				close(fd);

				NxDbgMsg(NX_DBG_INFO, "%s()--\n", __FUNCTION__ );
				return 0;
			}
		}

		pthread_mutex_lock( &handle->hLock );
		if( handle->bReset ) bRun = 0;
		pthread_mutex_unlock( &handle->hLock );
	}

	close(fd);
	return -1;
}


int32_t GpioResetInterrupt( GPIO_HANDLE handle )
{
	if( !handle )
	{
		return -1;
	}
	pthread_mutex_lock( &handle->hLock );
	handle->bReset = 1;
	pthread_mutex_unlock( &handle->hLock );
	return 0;
}
