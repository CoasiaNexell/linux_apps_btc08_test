#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

#include <string.h>
#include <linux/spi/spidev.h>

#include "Spi.h"

// #define NX_DBG_OFF
#ifdef NX_DTAG
#undef NX_DTAG
#endif
#define NX_DTAG "[SPI]"
#include "NX_DbgMsg.h"


struct tag_SPI_INFO{
	int			fd;
	uint32_t	mode;
	uint32_t	speed;
	uint32_t	delay;
	uint32_t	bits;
	char		deviceName[64];
	pthread_mutex_t lock;
};

SPI_HANDLE CreateSpi( const char *device, uint32_t mode, uint32_t speed, uint16_t delay, uint8_t bits )
{
	int fd, ret;
	SPI_HANDLE handle = NULL;

	fd = open(device, O_RDWR);
	if( fd < 0 )
	{
		NxDbgMsg(NX_DBG_ERR, "open failed (%s)\n", device);
		return NULL;
	}

	// spi R/W mode
	ret = ioctl(fd, SPI_IOC_WR_MODE32, &mode);
	if (ret == -1)
	{
		NxDbgMsg(NX_DBG_ERR, "can't set spi mode\n");
		goto ERROR_EXIT;
	}

	ret = ioctl(fd, SPI_IOC_RD_MODE32, &mode);
	if (ret == -1)
	{
		NxDbgMsg(NX_DBG_ERR, "can't get spi mode\n");
		goto ERROR_EXIT;
	}

	// bits per word
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
	{
		NxDbgMsg(NX_DBG_ERR, "can't set bits per word\n");
		goto ERROR_EXIT;
	}

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
	{
		NxDbgMsg(NX_DBG_ERR, "can't get bits per word\n");
		goto ERROR_EXIT;
	}

	// max speed hz
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
	{
		NxDbgMsg(NX_DBG_ERR, "can't set max speed hz\n");
		goto ERROR_EXIT;
	}

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
	{
		NxDbgMsg(NX_DBG_ERR, "can't get max speed hz\n");
		goto ERROR_EXIT;
	}

	handle = (SPI_HANDLE)malloc(sizeof(struct tag_SPI_INFO));

	memset( handle, 0, sizeof(struct tag_SPI_INFO) );

	handle->mode  = mode;
	handle->speed = speed;
	handle->delay = delay;
	handle->bits  = bits;
	handle->fd    = fd;
	strcpy( handle->deviceName, device );

	pthread_mutex_init( &handle->lock, NULL );

	return handle;

ERROR_EXIT:
	if( fd )
	{
		close( fd );
		fd = -1;
	}
	return handle;
}


void DestroySpi( SPI_HANDLE handle )
{
	if( handle )
	{
		pthread_mutex_destroy( &handle->lock );
		if( handle->fd > 0 )
			close(handle->fd);
		free( handle );
	}
}


int SpiTransfer( SPI_HANDLE handle, uint8_t *tx, uint8_t *rx, int32_t txLen, int32_t rxLen )
{
	int ret;
	struct spi_ioc_transfer tr;

	if( !handle )
		return -1;

	pthread_mutex_lock( &handle->lock );

	memset( &tr, 0, sizeof(tr) );
	tr.tx_buf        = (unsigned long)tx,	// Buffer for transmit data.
	tr.rx_buf        = (unsigned long)rx,	// Buffer for receive data.
	tr.len           = txLen + rxLen;		// Length of receive and transmit buffers in bytes.
	tr.delay_usecs   = handle->delay;		// Sets the delay after a transfer before the chip select status is changed and
											// the next transfer is triggered
	tr.speed_hz      = handle->speed;		// Sets the bit-rate of the device
	tr.bits_per_word = handle->bits;		// Sets the device wordsize.
	tr.cs_change     = 1;					// If true, device is deselected after transfer ended and before a new transfer
											// is started.
	tr.tx_nbits      = 0;					// Amount of bits that are used for writing
	tr.rx_nbits      = 0;					// Amount of bits that are used for reading.
	tr.pad           = 0;

	if (handle->mode & SPI_TX_QUAD)
		tr.tx_nbits = 4;
	else if (handle->mode & SPI_TX_DUAL)
		tr.tx_nbits = 2;
	if (handle->mode & SPI_RX_QUAD)
		tr.rx_nbits = 4;
	else if (handle->mode & SPI_RX_DUAL)
		tr.rx_nbits = 2;
	if (!(handle->mode & SPI_LOOP))
	{
		if (handle->mode & (SPI_TX_QUAD | SPI_TX_DUAL))
			tr.rx_buf = 0;
		else if (handle->mode & (SPI_RX_QUAD | SPI_RX_DUAL))
			tr.tx_buf = 0;
	}

	ret = ioctl(handle->fd, SPI_IOC_MESSAGE(1), &tr);
	// ret: length of transmit message in bytes on success / -1 on error
	if (ret == -1)
	{
		NxDbgMsg( NX_DBG_ERR, "can't send spi message\n");
	}

	//	1's complement
	for( int i=0 ; i<txLen+rxLen ; i++ )
	{
		rx[i] = rx[i] ^ 0xFF;
	}

	pthread_mutex_unlock( &handle->lock );

	return ret;
}

