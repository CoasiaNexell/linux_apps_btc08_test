/*
 * SPI testing utility (using spidev driver)
 *
 * Copyright (c) 2007  MontaVista Software, Inc.
 * Copyright (c) 2007  Anton Vorontsov <avorontsov@ru.mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * Cross-compile with cross-gcc -I/path/to/cross-kernel/include
 */

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <assert.h>
#include <stdbool.h>
#include <linux/gpio.h>
#include "cgminer_test.h"

uint8_t spi_tx[MAX_CMD_LEN+DUMMY_BYTES] = { 0x00, };
uint8_t spi_rx[MAX_CMD_LEN+DUMMY_BYTES] = { 0x00, };

static uint8_t golden_disable[DISABLE_LEN] = { 0x00, };
static uint8_t golden_enable[DISABLE_LEN] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

static const char *device = "/dev/spidev0.0";
static uint32_t mode;
static uint8_t bits = 8;
static uint32_t speed = 500000;
static uint16_t delay;
static uint16_t delaye = 0;
static int verbose;
static int test;
static int test_cnt = 0;
static int cmd_id = -1;
static uint8_t test_id = 0;

static unsigned int pinnum_reset = 0;
static unsigned int pinnum_irq_oon = 0;
static unsigned int pinnum_irq_gn = 0;

static uint8_t num_chips = 0;
static int num_cores = 0;
static int num_jobs = 0;

//uint8_t default_rx[ARRAY_SIZE(default_tx)] = {0, };
char *cmdarg;
char *seqarg;
char *btc08_gpio_pins = NULL;

static void pabort(const char *s)
{
	perror(s);
	abort();
}

/*
 *  Unescape - process hexadecimal escape character
 *      converts shell input "\x23" -> 0x23
 */
static int unescape(char *_dst, char *_src)
{
	int ret = 0;
	char *src = _src;
	char *dst = _dst;
	unsigned int ch;

	while (*src) {
		if (*src == '\\' && *(src+1) == 'x') {
			sscanf(src + 2, "%2x", &ch);
			src += 4;
			*dst++ = (unsigned char)ch;
		} else if (*src == ' ') {
			/*sscanf(src + 1, "%2x", &ch);
			src += 3;
			*dst++ = (unsigned char)ch;*/
			*dst++ = (uint8_t)strtol(src, NULL, 16);
			src += 3;
		}
		else {
			*dst++ = (uint8_t)strtol(src, NULL, 16);
			src += 3;
		}
		ret++;
	}
	return ret;
}


void get_input2hex(uint8_t *out, uint8_t *default_data, int len)
{
	char input[MAX_CMD_LEN] = { 0x00, };
	fgets( input, sizeof(input), stdin );
	//gets_s( input );
	if (input) {
		if (strlen(input) == 0)
			memcpy(out, default_data, len);
		else
			unescape((char *)out, input);
	}
}

static void hex_dump(const void *src, size_t length, size_t line_size, char *prefix)
{
	int i = 0;
	const unsigned char *address = (const unsigned char *)src;
	const unsigned char *line = address;
	unsigned char c;

	printf("%s | ", prefix);
	while (length-- > 0) {
		printf("%02X ", *address++);
		if (!(++i % line_size) || (length == 0 && i % line_size)) {
			if (length == 0) {
				while (i++ % line_size)
					printf("__ ");
			}
			printf(" | ");  /* right close */
//			while (line < address) {
//				c = *line++;
//				printf("%c", (c < 33 || c == 255) ? 0x2E : c);
//			}
			printf("\n");
			if (length > 0)
				printf("%s | ", prefix);
		}
	}
}

static void hex_dump_inv(const void *src, size_t length, size_t line_size, char *prefix)
{
	int i = 0;
	const unsigned char *address = (const unsigned char*)src;
	const unsigned char *line = address;
	unsigned char c;

	printf("%s | ", prefix);
	while (length-- > 0) {
		c = *address++;
		//c ^= 0xff;
		printf("%02X ", c);
		if (!(++i % line_size) || (length == 0 && i % line_size)) {
			if (length == 0) {
				while (i++ % line_size)
					printf("__ ");
			}
			printf(" | ");  /* right close */
//			while (line < address) {
//				c = *line++;
//				printf("%c", (c < 33 || c == 255) ? 0x2E : c);
//			}
			printf("\n"); 
			if (length > 0)
				printf("%s | ", prefix);
		}
	}
}

int spi_transfer(int fd, uint8_t *tx, uint8_t *rx, size_t len)
{
	int ret;
	struct spi_ioc_transfer tr;

	tr.tx_buf = (unsigned long)tx;	// Buffer for transmit data.
	tr.rx_buf = (unsigned long)rx;	// Buffer for receive data.
	tr.len = len;					// Length of receive and transmit buffers in bytes.
	tr.delay_usecs = delay;			// Sets the delay after a transfer before the chip select status is changed and the next transfer is triggered.
	tr.speed_hz = speed;			// Sets the bit-rate of the device
	tr.bits_per_word = bits;		// Sets the device wordsize.
	tr.cs_change = 1;				// If true, device is deselected after transfer ended and before a new transfer is started.
	tr.tx_nbits = 0;				// 	Amount of bits that are used for writing
	tr.rx_nbits = 0;				// Amount of bits that are used for reading.
	tr.pad = 0;

	if (mode & SPI_TX_QUAD)
		tr.tx_nbits = 4;
	else if (mode & SPI_TX_DUAL)
		tr.tx_nbits = 2;
	if (mode & SPI_RX_QUAD)
		tr.rx_nbits = 4;
	else if (mode & SPI_RX_DUAL)
		tr.rx_nbits = 2;
	if (!(mode & SPI_LOOP)) {
		if (mode & (SPI_TX_QUAD | SPI_TX_DUAL))
			tr.rx_buf = 0;
		else if (mode & (SPI_RX_QUAD | SPI_RX_DUAL))
			tr.tx_buf = 0;
	}

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret == -1) {		// ret: length of transmit message in bytes on success / -1 on error
		printf("ret = %d\n", ret);
		pabort("can't send spi message");
	}

	return ret;
}

static uint8_t *btc08_spi_transfer(int fd, uint8_t cmd, uint8_t chip_id,
		uint8_t *data, uint8_t data_len, uint8_t resp_len)
{
	int ret;
	int tx_len = ALIGN((CMD_LEN + CHIP_ID_LEN + data_len + resp_len), 4);

	memset(spi_tx, 0x00, MAX_CMD_LEN+DUMMY_BYTES);

	spi_tx[0] = cmd;
	spi_tx[1] = chip_id;

	if (data != NULL)
		memcpy(spi_tx + 2, data, data_len);

	ret = spi_transfer(fd, spi_tx, spi_rx, tx_len);
	for(int i=0; i<tx_len; i++) spi_rx[i] ^= 0xff;
	
	// TODO: For test
	/*memcpy(spi_rx, spi_tx, (2+data_len));
	if (cmd == SPI_CMD_AUTO_ADDRESS || cmd == SPI_CMD_READ_BIST) {
		spi_rx[3] = 02;
		spi_rx[3] = 03;
		spi_rx[4] = 04;
		spi_rx[32] = 32;
		spi_rx[33] = 33;
		spi_rx[34] = 4;
		spi_rx[35] = 5;
		spi_rx[36] = 36;
		spi_rx[37] = 37;
		spi_rx[38] = 38;
		spi_rx[39] = 39;
	}
	if (cmd == SPI_CMD_READ_ID)
	{
		spi_rx[2] = 2;
		spi_rx[3] = 3;
		spi_rx[4] = 10;
		spi_rx[5] = 3;
	}
	if (cmd == SPI_CMD_READ_JOB_ID)
	{
		spi_rx[2] = 2;
		spi_rx[3] = 3;
		spi_rx[4] = 1;
		spi_rx[5] = 6;
	}*/

	if (verbose)
	{
		//printf(" resp = ret[%d] = 0x%02X(%d)\n", 2+data_len);
		hex_dump	(spi_tx, tx_len, 32, "TX");
		hex_dump_inv(spi_rx, tx_len, 32, "RX");
	}

	return (ret == -1) ? NULL : (spi_rx + 2 + data_len);
}


/* GPIO_EXPORT */
int gpio_export(unsigned int gpio)
{
	int fd, len;
	char buf[MAX_BUF];
 
	fd = open(GPIO_SYSFS GPIO_EXPORT, O_WRONLY);
	if (fd < 0) {
		printf("Failed to export GPIO (%d)\n", gpio);
		return fd;
	}
 
	len = snprintf(buf, sizeof(buf), "%d", gpio);
	write(fd, buf, len);
	close(fd);
 
	return 0;
}

/* GPIO_DIRECTION */
int gpio_set_direction(unsigned int gpio, unsigned int out_flag)
{
	int fd, len;
	char buf[MAX_BUF];
 
	len = snprintf(buf, sizeof(buf), GPIO_SYSFS "/gpio%d" GPIO_DIR, gpio);

	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		printf("Failed to configure GPIO (%d)\n", gpio);
		return fd;
	}
 
	if (out_flag)
		write(fd, GPIO_DIR_OUT, 4);
	else
		write(fd, GPIO_DIR_IN, 3);
 
	close(fd);
	return 0;
}

/* GPIO_VALUE */
int gpio_set_value(unsigned int gpio, unsigned int value)
{
	int fd, len;
	char buf[MAX_BUF];
 
 	len = snprintf(buf, sizeof(buf), GPIO_SYSFS "/gpio%d" GPIO_VALUE, gpio);
 
	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		printf("Failed to open GPIO (%d) to set value (%d)\n", gpio, value);
		return fd;
	}
 
	if (value)
		write(fd, "1", 2);
	else
		write(fd, "0", 2);
 
 	close(fd);
	return 0;
}

/* GPIO_VALUE */
int gpio_get_value(unsigned int gpio, unsigned int *value)
{
	int fd, len;
	char buf[MAX_BUF];
	char val;
 
	printf("1\n");
 	len = snprintf(buf, sizeof(buf), GPIO_SYSFS "/gpio%d" GPIO_VALUE, gpio);
 
 	printf("2\n");
	fd = open(buf, O_RDONLY);
	if (fd < 0) {
		printf(" <== Failed to open GPIO (%d)\n", gpio);
		return fd;
	}
  	printf("3\n");
	lseek(fd, 0, SEEK_SET);
 	printf("4\n");
	if (read(fd, &val, 1) < 0) {
		printf(" <== Failed to read GPIO (%d)", gpio);
		return -1;
	}
	else {
		printf(" <== val=%c", val);
		*value = atoi( &val );
		printf(" <== val=%c value=%d", val, *value);
	}
 
 	close(fd);
	return 0;
}

static int cmd_read_id (int fd)
{
	uint8_t chip_id = 0;

	FUNC_IN();

	printf("Enter chip id\n");
	scanf("%x", &chip_id);

	cmd_READ_ID(fd, chip_id);

	return 0;
}

static int cmd_auto_address (int fd)
{
	FUNC_IN();

	cmd_AUTO_ADDRESS(fd);

	return 0;
}

static int cmd_run_bist (int fd)
{
	char hash_type[MAX_CMD_LEN] = { 0x00, };
	uint8_t hash_upper[HASH_LEN] = { 0x00, };
	uint8_t hash_lower[HASH_LEN] = { 0x00, };
	uint8_t hash_lower2[HASH_LEN] = { 0x00, };
	uint8_t hash_lower3[HASH_LEN] = { 0x00, };

	FUNC_IN();

	for (int i=0; i<ASIC_BOOST_CORE_NUM; i++)
	{
		sprintf(hash_type , "HASH_%s", 
				(i==0) ? "UPPER":(i==1) ? "LOWER":(i==2) ? "LOWER2":(i==3) ? "LOWER3":"INVALID");
		printf("Enter hash %s (Input Enter to set default golden hash)\n", hash_type);

		if (i == 0) {
			get_input2hex(hash_upper, default_golden_hash, HASH_LEN);
			if (verbose)	hex_dump(hash_upper, HASH_LEN, 32, "HASH_UPPER");
		}
		if (i == 1) {
			get_input2hex(hash_lower, default_golden_hash, HASH_LEN);
			if (verbose)	hex_dump(hash_lower, HASH_LEN, 32, "HASH_LOWER");
		}
		if (i == 2) {
			get_input2hex(hash_lower2, default_golden_hash, HASH_LEN);
			if (verbose)	hex_dump(hash_lower2, HASH_LEN, 32, "HASH_LOWER2");
		}
		if (i == 3) {
			get_input2hex(hash_lower3, default_golden_hash, HASH_LEN);
			if (verbose)	hex_dump(hash_lower3, HASH_LEN, 32, "HASH_LOWER3");
		}
	}

	cmd_RUN_BIST(fd, hash_upper, hash_lower, hash_lower2, hash_lower3);

	return 0;
}

static int cmd_read_bist (int fd)
{
	uint8_t chip_id = 0;

	FUNC_IN();

	printf("Enter chip id\n");
	scanf("%x", &chip_id);

	cmd_READ_BIST(fd, chip_id);

	return 0;
}

static int cmd_reset (int fd)
{
	FUNC_IN();

	cmd_RESET(fd);

	return 0;
}

static int cmd_set_pll_config (int fd)
{
	char str[MAX_CMD_LEN] = { 0x00, };
	uint8_t chip_id = 0;
	uint8_t pll_idx = 0;

	FUNC_IN();

	printf("Enter chip id\n");
	scanf("%x", &chip_id);
	getc(stdin);

	printf("Enter pll index\n");
	scanf("%x", &pll_idx);

	cmd_SET_PLL_CONFIG(fd, chip_id, pll_idx);

	return 0;
}

static int cmd_set_pll_fout_en (int fd)
{
	char str[MAX_CMD_LEN] = { 0x00, };
	uint8_t chip_id = 0;
	int fout_en;

	FUNC_IN();

	printf("Enter chip id\n");
	scanf("%x", &chip_id);
	getc(stdin);

	printf("Enter pll fout en (Default: 0 - 0: disable, 1: enable)\n");
	scanf("%x", &fout_en);

	if (verbose)	printf("chip_id=%02x, fout_en=0x%02x\n", chip_id, fout_en);

	cmd_SET_PLL_FOUT_EN(fd, chip_id, (uint8_t)fout_en);

	return 0;
}

static int cmd_set_pll_resetb (int fd)
{
	char str[MAX_CMD_LEN] = { 0x00, };
	uint8_t chip_id = 0;
	int reset;

	FUNC_IN();

	printf("Enter chip id\n");
	scanf("%x", &chip_id);
	getc(stdin);

	printf("Enter pll reset (0: reset, 1: on)\n");
	scanf("%x", &reset);

	if (verbose)	printf("chip_id=%02x, reset=0x%02x\n", chip_id, reset);

	cmd_SET_PLL_RESETB(fd, chip_id, reset);

	return 0;
}

static int cmd_read_pll (int fd)
{
	uint8_t chip_id = 0;

	FUNC_IN();

	printf("Enter chip id\n");
	scanf("%x", &chip_id);

	cmd_READ_PLL(fd, chip_id);

	return 0;
}

static int cmd_write_param (int fd)
{
	uint8_t chip_id = 0;
	uint8_t midstate[MIDSTATE_LEN] = { 0x00, };
	uint8_t data[DATA_LEN] = { 0x00, };

	FUNC_IN();

	printf("Enter chip id\n");
	scanf("%x", &chip_id);
	getc(stdin);

	printf("\nEnter midstate (Input Enter to set default midstate)\n");
	get_input2hex(midstate, default_golden_midstate, MIDSTATE_LEN);
	if (verbose)	hex_dump(midstate, MIDSTATE_LEN, 32, "MIDSTATE");

	printf("\nEnter data (Input Enter to set default data)\n");
	get_input2hex(data, default_golden_data, DATA_LEN);
	if (verbose)	hex_dump(data, DATA_LEN, 32, "DATA");

	cmd_WRITE_PARM(fd, chip_id, midstate, data);

	return 0;
}

static int cmd_read_parm (int fd)
{
	uint8_t chip_id = 0;

	FUNC_IN();

	printf("Enter chip id\n");
	scanf("%x", &chip_id);

	cmd_READ_PARM(fd, chip_id);

	return 0;
}

int cmd_write_target (int fd)
{
	uint8_t chip_id = 0;
	uint8_t target[TARGET_LEN] = { 0x00, };

	FUNC_IN();

	printf("Enter chip_id\n");
	scanf("%x", &chip_id);
	getc(stdin);

	printf("Enter target (Input Enter to set default golden target)\n");
	get_input2hex(target, default_golden_target, TARGET_LEN);
	if (verbose)	hex_dump(target, TARGET_LEN, 32, "TARGET");

	cmd_WRITE_TARGET(fd, chip_id, target);

	return 0;
}

static int cmd_read_target (int fd)
{
	uint8_t chip_id = 0;

	FUNC_IN();

	printf("Enter chip id\n");
	scanf("%x", &chip_id);

	cmd_READ_TARGET(fd, chip_id);

	return 0;
}

static int cmd_run_job (int fd)
{
	uint8_t chip_id = 0;
	uint8_t job_id = 0;

	FUNC_IN();

	printf("Enter chip id\n");
	scanf("%x", &chip_id);
	getc(stdin);

	printf("Enter job id\n");
	scanf("%x", &job_id);

	cmd_RUN_JOB(fd, chip_id, job_id);

	return 0;
}

static int cmd_read_job_id (int fd)
{
	uint8_t chip_id = 0;

	FUNC_IN();

	printf("Enter chip id\n");
	scanf("%x", &chip_id);

	cmd_READ_JOB_ID(fd, chip_id);

	return 0;
}

static int cmd_read_result (int fd)
{
	uint8_t chip_id = 0;

	FUNC_IN();

	printf("Enter chip id\n");
	scanf("%x", &chip_id);

	cmd_READ_RESULT(fd, chip_id);

	return 0;
}

static int cmd_clear_oon (int fd)
{
	uint8_t chip_id = 0;

	FUNC_IN();

	printf("Enter chip id\n");
	scanf("%x", &chip_id);

	cmd_CLEAR_OON(fd, chip_id);

	return 0;
}

static int cmd_set_disable (int fd)
{
	uint8_t chip_id = 0;
	uint8_t is_disable = 0;
	uint8_t disable[DISABLE_LEN] = { 0x00, };

	FUNC_IN();

	printf("Enter chip id\n");
	scanf("%x", &chip_id);
	getc(stdin);

	printf("Enter disable (Default:disable - 1: Enable, 0: Disable)\n");
	scanf("%d", &is_disable);

	if (is_disable == 0)
		memcpy(disable, golden_disable, DISABLE_LEN);
	else
		memcpy(disable, golden_enable, DISABLE_LEN);

	if (verbose)	hex_dump(disable, DISABLE_LEN, 32, "DISABLE");

	cmd_SET_DISABLE(fd, chip_id, disable);

	return 0;
}

static int cmd_read_disable (int fd)
{
	uint8_t chip_id = 0;

	FUNC_IN();

	printf("Enter chip id\n");
	scanf("%x", &chip_id);

	cmd_READ_DISABLE(fd, chip_id);

	return 0;
}

static int cmd_set_control (int fd)
{
	uint8_t chip_id = 0;
	uint32_t udiv = 0;

	FUNC_IN();

	printf("Enter chip id\n");
	scanf("%x", &chip_id);
	getc(stdin);

	printf("Enter control register values\n");
	scanf("%d", &udiv);

	cmd_SET_CONTROL(fd, chip_id, udiv);

	return 0;
}

static int cmd_read_temp (int fd)
{
	uint8_t chip_id = 0;

	FUNC_IN();

	printf("Enter chip id\n");
	scanf("%x", &chip_id);

	cmd_READ_TEMP(fd, chip_id);

	return 0;
}

static int cmd_write_nonce (int fd)
{
	uint8_t chip_id = 0;
	uint8_t start_nonce[NONCE_LEN] = { 0x00, };
	uint8_t end_nonce[NONCE_LEN] = { 0x00, };

	FUNC_IN();

	printf("Enter start nonce (Input Enter to set default start nonce)\n");
	get_input2hex(start_nonce, default_golden_start_nonce, NONCE_LEN);
	if (verbose)	hex_dump(start_nonce, NONCE_LEN, 32, "START_NONCE");

	printf("Enter end nonce (Input Enter to set default end nonce)\n");
	get_input2hex(end_nonce, default_golden_end_nonce, NONCE_LEN);
	if (verbose)	hex_dump(end_nonce, NONCE_LEN, 32, "END_NONCE");

	cmd_WRITE_NONCE(fd, chip_id, start_nonce, end_nonce);

	return 0;
}

static int cmd_read_hash (int fd)
{
	uint8_t chip_id = 0;

	FUNC_IN();

	printf("Enter chip id\n");
	scanf("%x", &chip_id);

	cmd_READ_HASH(fd, chip_id);

	return 0;
}

static int cmd_read_feature (int fd)
{
	uint8_t chip_id = 0;

	FUNC_IN();

	printf("Enter chip id\n");
	scanf("%x", &chip_id);

	cmd_READ_FEATURE(fd, chip_id);

	return 0;
}

static int cmd_read_revision (int fd)
{
	uint8_t chip_id = 0;

	FUNC_IN();

	printf("Enter chip id\n");
	scanf("%x", &chip_id);

	cmd_READ_REVISION(fd, chip_id);

	return 0;
}

static int seq_gpio (int fd)
{
	FUNC_IN();

	if (btc08_gpio_pins == NULL) {
		pinnum_reset = _6818_FPGA_GPIO_RESET;
		pinnum_irq_oon = _6818_FPGA_GPIO_IRQ_OON;
		pinnum_irq_gn = _6818_FPGA_GPIO_IRQ_GN;
	}

	printf("Init GPIO: RESET(%d), IRQ_GN(%d), IRQ_OON(%d)\n",
			pinnum_reset, pinnum_irq_oon, pinnum_irq_gn);

	if (gpio_export (pinnum_reset) 	< 0 ||
		gpio_export (pinnum_irq_oon) < 0 ||
		gpio_export (pinnum_irq_gn) < 0)
		return -1;

	if (gpio_set_direction (pinnum_reset, 	1) < 0 ||		// out
		gpio_set_direction (pinnum_irq_oon, 0) < 0 ||		// in
		gpio_set_direction (pinnum_irq_gn,	0) < 0)			// in
		return -1;

	if (gpio_set_value (pinnum_reset, 1) < 0)
		return -1;
	else
		return 0;
}

static uint8_t set_pin(int pin, int val)
{
	// 96+31 = 127
	uint32_t ret = 0;
	int fd;
	char pinpath[64];
	sprintf(pinpath, "/sys/class/gpio/gpio%d/value", pin);
	fd = open(pinpath, O_WRONLY);
	if(fd==0) return -1;

	if(val==0) ret = '0';
	else ret = '1';
	lseek(fd, 0, SEEK_SET);
	write(fd, &ret, 1);

	close(fd);

	//applog(LOG_DEBUG, "%s: pin %d, val 0x%02x", __FUNCTION__, pin, (uint8_t)ret);

	return (uint8_t)ret;
}

static int seq_reset_autoaddr (int fd)
{
	FUNC_IN();

	gpio_set_value (pinnum_reset, 0);
	sleep (0.1);
	gpio_set_value (pinnum_reset, 1);

	cmd_AUTO_ADDRESS(fd);
	cmd_AUTO_ADDRESS(fd);

	// TODO: Enable OON IRQ, UDIV 3 (0x00 0x13 0x00 0x1F)
	//cmd_SET_CONTROL(fd, BCAST_CHIP_ID, (DEFAULT_UDIV | OON_IRQ_ENB));
	//num_chips = cmd_AUTO_ADDRESS(fd);

	return 0;
}

static int seq_bist (int fd)
{
	FUNC_IN();

	int num_chips = cmd_AUTO_ADDRESS(fd);

	cmd_WRITE_PARM (fd, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);
	
	cmd_WRITE_TARGET (fd, BCAST_CHIP_ID, default_golden_target);

	cmd_WRITE_NONCE (fd, BCAST_CHIP_ID, default_golden_start_nonce, default_golden_start_nonce);

	cmd_SET_DISABLE (fd, BCAST_CHIP_ID, golden_enable);

	cmd_RUN_BIST (fd, default_golden_hash, default_golden_hash, default_golden_hash, default_golden_hash);

	for (int chip_id = 1; chip_id <= num_chips; chip_id++) {
		cmd_READ_BIST (fd, chip_id);
	}

	return 0;
}

static int seq_workloop (int fd)
{
	FUNC_IN();

	// Check if FIFO is empty
	for (int chip_id = 1; chip_id < num_chips; chip_id++) {
		num_jobs += cmd_READ_ID (fd, chip_id);
	}

	unsigned int gn_value;
	int value = gpio_get_value(_6818_FPGA_GPIO_IRQ_GN, &gn_value);
	printf("value = %d gn_value = %d\n", value, gn_value);

	//scanwork();
			
	// (4) Run Job
	cmd_WRITE_PARM (fd, BCAST_CHIP_ID, default_golden_midstate, default_golden_data);

	cmd_WRITE_TARGET (fd, BCAST_CHIP_ID, default_golden_target);

	cmd_WRITE_NONCE (fd, BCAST_CHIP_ID, default_golden_start_nonce, default_golden_start_nonce);

	for (int job_id=0; job_id<ASIC_BOOST_CORE_NUM; job_id++) {
		cmd_RUN_JOB (fd, BCAST_CHIP_ID, job_id);
	}

	return 0;
}

static const struct {
	const char *cmd;
	int (*func) (int fd);
	const char *desc;
} cmd_table[] = {
	{ "read_id",        	cmd_read_id,
	  "Read chip id and FIFO status" },
	{ "auto_address",		cmd_auto_address,
	  "Auto addressing to all connected chips" },
	{ "run_bist",			cmd_run_bist,
	  "Run BIST" },
	{ "read_bist",			cmd_read_bist,
	  "Read BIST status and the number of cores that have passed BIST" },
	{ "reset",				cmd_reset,
	  "Software Reset" },
	{ "set_pll_config",		cmd_set_pll_config,
	  "Set PLL config" },
	{ "set_pll_fout_en",	cmd_set_pll_fout_en,
	  "Set PLL FOUT" },
	{ "set_pll_resetb",		cmd_set_pll_resetb,
	  "Set PLL Set reset/on" },
	{ "read_pll",			cmd_read_pll,
	  "Read PLL" },
	{ "write_parm",			cmd_write_param,
	  "Set Midstate and Data(MerkleRoot, Time, Target)" },
	{ "read_parm",			cmd_read_parm,
	  "Read Midstate and Data(MerkleRoot, Time, Target)" },
	{ "write_target",		cmd_write_target,
	  "Write the target value to be compared with the hash result" },
	{ "read_target",		cmd_read_target,
	  "Read target and select" },
	{ "run_job",			cmd_run_job,
	  "Run job" },
	{ "read_job_id",		cmd_read_job_id,
	  "Read job status" },
	{ "read_result",			cmd_read_result,
	  "Read result(GN and OUT Interrupt)" },
	{ "clear_oon",			cmd_clear_oon,
	  "Clear out of nonce" },
	{ "set_disable",		cmd_set_disable,
	  "Set the status of each core as enable/disable" },
	{ "read_disable",		cmd_read_disable,
	  "Read disable status of each core" },
	{ "set_control",		cmd_set_control,
	  "Set control register" },
	{ "read_temp",			cmd_read_temp,
	  "Read temperature" },
	{ "write_nonce",		cmd_write_nonce,
	  "Set start and end number of nonce" },
	{ "read_hash",			cmd_read_hash,
	  "Read hash" },
	{ "read_feature",		cmd_read_feature,
	  "Read fixed feature info" },
	{ "read_revision",		cmd_read_revision,
	  "Read fixed revision info" },
	{  NULL, },
};

static const struct {
	const char *seq;
	int (*func) (int fd);
	const char *desc;
} seq_table[] = {
	{ "gpio",        		seq_gpio,
	  "Export and set direction of RESET, IRQ_GN, IRQ_OON GPIOs > set RESET as High" },
	{ "reset_autoaddr",		seq_reset_autoaddr,
	  "Reset RESET GPIO > AUTO_ADDRESS > SET_CONTROL > AUTO_ADDRESS" },
	{ "bist",        		seq_bist,
	  "WRITE_PARAM > WRITE_TARGET > WRITE_NONCE > SET_DISABLE > RUN_BIST > READ_BIST" },
	/*{ "workloop",        	seq_workloop,
	  "WRITE_PARAM > WRITE_TARGET > WRITE_NONCE > RUN_JOB" },*/
	{  NULL, },
};

static void print_usage(const char *prog)
{
	printf("Usage: %s [-Dsdbvgct]\n", prog);
	puts(" [SPI Configurations] \n"
		 "  -D --device   		device to use (default /dev/spidev0.0)\n"
	     "  -s --speed    		max speed (Hz) (default 5000000)\n"
	     "  -d --delay    		delay (usec) (default 0 usec)\n"
	     "  -b --bpw      		bits per word \n"
	     "  -v --verbose  		Verbose (show tx buffer)\n"
	     "  -p            		Send data (e.g. \"1234\\xde\\xad\")\n"
		 " [GPIO Configurations] \n"
		 "  -G --gpio     		Change gpio pin num RESET:IRQ_GN:IRQ_OON (default 66:112:65)\n"
		 " [SPI Commands] \n"
		 "  -c  --cmd [cmd]		Run each command");
	for (int i = 0; cmd_table[i].cmd; i++) {
		const char *cmd = cmd_table[i].cmd;
		const char *desc = cmd_table[i].desc;

		printf("%-12s%-20s%-10s\n", " ", cmd? cmd: "",
				desc? desc: "");
	}
	puts("  -t  --test [test]		Run test");
	for (int i = 0; seq_table[i].seq; i++) {
		const char *seq = seq_table[i].seq;
		const char *desc = seq_table[i].desc;

		printf("%-12s%-20s%-10s\n", " ", seq? seq: "",
				desc? desc: "");
	}
	exit(1);
}

static void parse_opts(int argc, char *argv[])
{
	while (1) {
		static const struct option lopts[] = {
			{ "device",  1, 0, 'D' },
			{ "speed",   1, 0, 's' },
			{ "delay",   1, 0, 'd' },
			{ "bpw",     1, 0, 'b' },
			{ "verbose", 0, 0, 'v' },
			{ "gpio",    1, 0, 'g' },
			{ "cmd",     1, 0, 'c' },
			{ "test",    1, 0, 't' },
			{ NULL, 0, 0, 0 },
		};
		int c;

		c = getopt_long(argc, argv, "D:s:d:b:g:c:t:v", lopts, NULL);

		if (c == -1)
			break;

		switch (c) {
		case 'D':
			device = optarg;
			break;
		case 's':
			speed = atoi(optarg);
			break;
		case 'd':
			delay = atoi(optarg);
			break;
		case 'b':
			bits = atoi(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		case 'g':
			btc08_gpio_pins = optarg;
			sscanf(btc08_gpio_pins, "%d:%d:%d",
					&pinnum_reset, &pinnum_irq_oon, &pinnum_irq_gn);
			break;
		case 't':
			seqarg = optarg;
			break;
		case 'c':
			cmdarg = optarg;
			break;
		default:
			print_usage(argv[0]);
			break;
		}
	}
}



int init_spi_config(int fd)
{
	int ret = 0;

	// spi R/W mode
	ret = ioctl(fd, SPI_IOC_WR_MODE32, &mode);
	if (ret == -1)
		perror("can't set spi mode");

	ret = ioctl(fd, SPI_IOC_RD_MODE32, &mode);
	if (ret == -1)
		perror("can't get spi mode");

	// bits per word
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		perror("can't set bits per word");

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		perror("can't get bits per word");

	// max speed hz
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		perror("can't set max speed hz");

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		perror("can't get max speed hz");

	if (verbose) {
		printf("spi mode:  0x%x\n", mode);
		printf("bits per word: %d\n", bits);
		printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);
	}

	return ret;
}

static int cmd_READ_ID (int fd, uint8_t chip_id)
{
	uint8_t *ret;
	int num_jobs = 0;

	FUNC_IN();

	if (chip_id == BCAST_CHIP_ID) {
		printf(" <== SPI_CMD_READ_ID: Invalid Chip ID(BCAST is not allowed)\n");
		return num_jobs;
	}

	printf("#######################");
	ret = btc08_spi_transfer(fd, SPI_CMD_READ_ID, chip_id, NULL, 0, (4 + DUMMY_BYTES));
	if (ret == NULL) {
		printf(" <== Error on READ_ID\n");
	} else {
		// ret[0]: [31:24] Reserved
		// ret[1]: [22:16] Number of SPI bytes previously transmitted
		// ret[2]: [10:8] Number of jobs in FIFO
		// ret[3]: [7:0] Chip ID
		/*if (ret[3] != chip_id) {
			printf(" <== Error on READ_ID for chip_id %d (%d)\n", chip_id, ret[3]);
		} else {*/
			num_jobs = (ret[2] & 7);
			printf(" <== Chip id is %d, Number of jobs (%d)\n", ret[3], num_jobs);
		//}
	}

	return num_jobs;
}

static uint8_t cmd_AUTO_ADDRESS (int fd)
{
	uint8_t dummy[32] = {0x00,};
	uint8_t *ret;
	uint8_t	num_chips = 0;

	FUNC_IN();

	ret = btc08_spi_transfer(fd, SPI_CMD_AUTO_ADDRESS, 0x00, dummy, sizeof(dummy), (2 + DUMMY_BYTES));
	if (ret == NULL) {
		printf(" <== Error on AUTO_ADDRESS\n");
	} else {
		num_chips = ret[1];
		printf(" <== Number of chips: %d\n", ret[1]);
	}

	return num_chips;
}

static void cmd_RUN_BIST (int fd, uint8_t *golden_hash_upper, uint8_t *golden_hash_lower,
						uint8_t *golden_hash_lower2, uint8_t *golden_hash_lower3)
{
	FUNC_IN();

	uint8_t golden_hash[TOTAL_HASH_LEN] = { 0x00, };
	int len = 0;

	if (golden_hash_upper) {
		memcpy(golden_hash, golden_hash_upper, HASH_LEN);
		len += HASH_LEN;
	}
	if (golden_hash_lower) {
		memcpy(golden_hash+len, golden_hash_lower, HASH_LEN);
		len += HASH_LEN;
	}
	if (golden_hash_lower2) {
		memcpy(golden_hash+len, golden_hash_lower2, HASH_LEN);
		len += HASH_LEN;
	}
	if (golden_hash_lower3) {
		memcpy(golden_hash+len, golden_hash_lower3, HASH_LEN);
		len += HASH_LEN;
	}

	if (len != TOTAL_HASH_LEN)
		printf(" <== SPI_CMD_RUN_BIST: Need to check golden_hash(%d, %d)\n", len, TOTAL_HASH_LEN);

	btc08_spi_transfer(fd, SPI_CMD_RUN_BIST, 0x00, golden_hash, len, DUMMY_BYTES);
}

static uint8_t cmd_READ_BIST (int fd, uint8_t chip_id)
{
	uint8_t *ret;
	uint8_t	num_cores = 0;

	FUNC_IN();

	if (chip_id == BCAST_CHIP_ID) {
		printf(" <== SPI_CMD_READ_BIST: Invalid Chip ID(BCAST is not allowed)\n");
		return num_cores;
	}

	ret = btc08_spi_transfer(fd, SPI_CMD_READ_BIST, chip_id, NULL, 0, (2 + DUMMY_BYTES));
	if (ret == NULL) {
		printf(" <== Error on READ_BIST\n");
		return num_cores;
	} else {
		// ret[0]: [8] : Busy status
		// ret[1]: [7:0] Number of cores
		num_cores = ret[1];
		printf(" <== SPI_CMD_READ_BIST: Status(%s), Number of cores(%d)\n", (ret[0]&1) ? "Busy":"Idle", ret[1]);
	}
	
	return num_cores;
}

static void cmd_RESET (int fd)
{
	FUNC_IN();

	btc08_spi_transfer(fd, SPI_CMD_RESET, 0x00, NULL, 0, DUMMY_BYTES);
}

static void cmd_SET_PLL_CONFIG (int fd, uint8_t chip_id, int pll_idx)
{
	uint8_t pll_buf[4];

	FUNC_IN();

	pll_buf[0] = (uint8_t)(pll_sets[pll_idx].val>>24)&0xff;
	pll_buf[1] = (uint8_t)(pll_sets[pll_idx].val>>16)&0xff;
	pll_buf[2] = (uint8_t)(pll_sets[pll_idx].val>> 8)&0xff;
	pll_buf[3] = (uint8_t)(pll_sets[pll_idx].val>> 0)&0xff;

	btc08_spi_transfer(fd, SPI_CMD_SET_PLL_CONFIG, chip_id, pll_buf, sizeof(pll_buf), DUMMY_BYTES);
}

static void cmd_READ_PLL (int fd, uint8_t chip_id)
{
	uint8_t *ret;

	FUNC_IN();

	if (chip_id == BCAST_CHIP_ID) {
		printf(" <== SPI_CMD_READ_PLL: Invalid Chip ID(BCAST is not allowed)\n");
		return;
	}

	ret = btc08_spi_transfer(fd, SPI_CMD_READ_PLL, chip_id, NULL, 0, (4+DUMMY_BYTES));
	if (ret == NULL) {
		printf(" <== Error on READ_PLL");
	} else {
		printf(" <== PLL [23]: %s", (ret[1]&(1<<7)) ? "locked":"");
	}
}

static void cmd_WRITE_PARM (int fd, uint8_t chip_id, uint8_t *midstate, uint8_t *data)
{
	uint8_t param[WRITE_JOB_LEN] = { 0x00, };
	int len = 0;

	FUNC_IN();

	if (midstate) {
		memcpy(param, midstate, MIDSTATE_LEN);
		len += MIDSTATE_LEN;
	}

	if (data) {
		memcpy(param+len, data, DATA_LEN);
		len += DATA_LEN;
	}

	if (midstate) {
		memcpy(param+len, midstate, MIDSTATE_LEN);
		len += MIDSTATE_LEN;

		memcpy(param+len, midstate, MIDSTATE_LEN);
		len += MIDSTATE_LEN;

		memcpy(param+len, midstate, MIDSTATE_LEN);
		len += MIDSTATE_LEN;
	}

	if (len != WRITE_JOB_LEN) {
		printf(" <== Need to check params(%d, %d) for WRITE_PARAM\n", len, WRITE_JOB_LEN);
	}

	btc08_spi_transfer(fd, SPI_CMD_WRITE_PARM, chip_id, param, len, DUMMY_BYTES);
}

static void cmd_READ_PARM (int fd, uint8_t chip_id)
{
	uint8_t *ret;

	FUNC_IN();

	if (chip_id == BCAST_CHIP_ID) {
		printf(" <== cmd_READ_PARM: Invalid Chip ID(BCAST is not allowed)\n");
		return;
	}

	ret = btc08_spi_transfer(fd, SPI_CMD_READ_PARM, chip_id, NULL, 0, (WRITE_JOB_LEN + DUMMY_BYTES));
	if (ret == NULL) {
		printf(" <== Error on READ_PARM");
	} else {
		// Read Rx data
		// ret[0-31] [1119:864] : MidState3 (256 bit)
		//           [863:608] : MidState2 (256 bit)
		//           [607:352] : MidState1  (256 bit)
		//           [351:256]  : Data (96 bit - MerkleRoot[31:0], Time[31:0], Target[31:0])
		//           [255:0] : MidState (256 bit)
	}
}

static void cmd_WRITE_TARGET (int fd, uint8_t chip_id, uint8_t *target)
{
	FUNC_IN();

	// TODO: Need to check difficulty : 0x89 0x6c 0x05 0x10
	btc08_spi_transfer(fd, SPI_CMD_WRITE_TARGET, chip_id, target, TARGET_LEN, DUMMY_BYTES);
}

static void cmd_READ_TARGET (int fd, uint8_t chip_id)
{
	FUNC_IN();

	uint8_t *ret;

	if (chip_id == BCAST_CHIP_ID) {
		printf(" <== SPI_CMD_READ_TARGET: Invalid Chip ID(BCAST is not allowed)\n");
		return;
	}

	ret = btc08_spi_transfer(fd, SPI_CMD_READ_TARGET, chip_id, NULL, 0, (6 + DUMMY_BYTES));
	if (ret == NULL) {
		printf(" <== Error on READ_TARGET\n");
	} else {
		// ret[0-1]: [43:32]  Select (12bit)
		// ret[2-5]: [31:0]   Target (32bit)
		// Read Select/Target
		printf(" <== Select(12bit) ");
	}
}

static void cmd_RUN_JOB (int fd, uint8_t chip_id, uint8_t job_id)
{
	FUNC_IN();
	uint8_t id[2];
	id[0] = (0x00 | ASIC_BOOST_EN);
	id[1] = job_id;

	//uint8_t id[2] = { (0x00 | ASIC_BOOST_EN), job_id };

	// TODO: Need to check difficulty : 0x89 0x6c 0x05 0x10
	btc08_spi_transfer(fd, SPI_CMD_RUN_JOB, chip_id, id, 2, DUMMY_BYTES);
}

static void cmd_READ_JOB_ID (int fd, uint8_t chip_id)
{
	FUNC_IN();

	uint8_t *ret;

	ret = btc08_spi_transfer(fd, SPI_CMD_READ_JOB_ID, chip_id, NULL, 0, (4 + DUMMY_BYTES));
	if (ret == NULL) {
		printf(" <== Error on READ_JOB_ID\n");
	} else {
		// ret[0]: [31:24] OON Job ID
		// ret[1]: [23:16] GN Job ID
		// ret[2]: [10:8]  10/9/8 : Result FIFO Full/OON IRQ/GN IRQ Flag
		// ret[3]: [7:0]   Chip ID
		bool fifo_full = ((ret[2] & (1<<2)) != 0);
		bool oon_irq = ((ret[2] & (1<<1)) != 0);
		bool gn_irq = ((ret[2] & (1<<0)) != 0);

		printf(" <== OON Job ID(%d), GN Job ID(%d)\n", ret[0], ret[1]);
		printf(" <== Flag FIFO Full(%d), OON IRQ(%d), GN IRQ(%d)\n", fifo_full, oon_irq, gn_irq);
		printf(" <== Chip ID(%d)\n", ret[3]);
	}
}

static void cmd_READ_RESULT (int fd, uint8_t chip_id)
{
	FUNC_IN();

	uint8_t *ret;

	if (chip_id == BCAST_CHIP_ID) {
		printf(" <== SPI_CMD_READ_RESULT: Invalid Chip ID(BCAST is not allowed)\n");
		return;
	}

	ret = btc08_spi_transfer(fd, SPI_CMD_READ_RESULT, chip_id, NULL, 0, (READ_RESULT_LEN + DUMMY_BYTES));
	if (ret == NULL) {
		printf(" <== Error on READ_RESULT!!!\n");
	} else {
		// ret[0]: [139:136] Inst_Lower_3/Inst_Lower_2/Inst_Lower/Upper found golden nonce
		// ret[1]: [135:128] read ValidCnt in Core
		// ret[2-5]: [127:96]  read_golden_nonce of Inst_Lower
		// ret[6-9]: [95:64]  read_golden_nonce of Inst_Lower
		// ret[10-13]: [63:32]  read_golden_nonce of Inst_Lower
		// ret[14-17]: [31:0]  read_golden_nonce of Inst_Upper
		bool lower3 = ((ret[0] & 8) != 0);
		bool lower2 = ((ret[0] & 4) != 0);
		bool lower = ((ret[0] & 2) != 0);
		bool upper = ((ret[0] & 1) != 0);
		printf(" <== GN found in Inst_%s\n",
				lower3 ? "Lower_3": (lower2 ? "Lower_2" : (lower ? "Lower": (upper ? "Upper":""))));
	}
}

static void cmd_CLEAR_OON (int fd, uint8_t chip_id)
{
	FUNC_IN();

	btc08_spi_transfer(fd, SPI_CMD_CLEAR_OON, chip_id, NULL, 0, DUMMY_BYTES);
}

static void cmd_SET_DISABLE (int fd, uint8_t chip_id, uint8_t* disable)
{
	FUNC_IN();

	btc08_spi_transfer(fd, SPI_CMD_SET_DISABLE, chip_id, disable, DISABLE_LEN, DUMMY_BYTES);
}

static void cmd_READ_DISABLE (int fd, uint8_t chip_id)
{
	FUNC_IN();

	uint8_t *ret;

	if (chip_id == BCAST_CHIP_ID) {
		printf(" <== SPI_CMD_READ_DISABLE: Invalid Chip ID(BCAST is not allowed)\n");
		return;
	}

	ret = btc08_spi_transfer(fd, SPI_CMD_READ_DISABLE, chip_id, NULL, 0, (32 + DUMMY_BYTES));
	if (ret == NULL) {
		printf(" <== SPI_CMD_READ_DISABLE: Error on READ_DISABLE!!!\n");
	}
}

static void cmd_SET_CONTROL (int fd, uint8_t chip_id, uint32_t parm)
{
	uint8_t sbuf[4];

	FUNC_IN();

	sbuf[0] = (uint8_t)(parm>>24)&0xff;
	sbuf[1] = (uint8_t)(parm>>16)&0xff;
	sbuf[2] = (uint8_t)(parm>> 8)&0xff;
	sbuf[3] = (uint8_t)(parm>> 0)&0xff;

	printf("parm = 0x%02x, sbuf[0]=0x%02x, sbuf[1]=0x%02x, sbuf[2]=0x%02x, sbuf[3]=0x%02x\n",
			parm, sbuf[0], sbuf[1], sbuf[2], sbuf[3]);

	// reset_autoaddr.sh : extra data(x00\x18\x00\x1f)
	if (sbuf[2] & LAST_CHIP_FLAG)
		printf(" ==> Set a last chip (chip_id %d)\n", chip_id);
	if (sbuf[3] & OON_IRQ_ENB)
		printf(" ==> Set OON IRQ Enable\n");

	printf(" ==> Uart divider : %d\n", (sbuf[3] & 0x07));

	btc08_spi_transfer(fd, SPI_CMD_SET_CONTROL, chip_id, sbuf, sizeof(sbuf), 0);
}

static void cmd_READ_TEMP (int fd, uint8_t chip_id)
{
	FUNC_IN();

	uint8_t *ret;

	if (chip_id == BCAST_CHIP_ID) {
		printf(" <== SPI_CMD_READ_TEMP: Invalid Chip ID(BCAST is not allowed)\n");
		return;
	}

	ret = btc08_spi_transfer(fd, SPI_CMD_READ_TEMP, chip_id, NULL, 0, (2 + DUMMY_BYTES));
	if (ret == NULL) {
		printf(" <== SPI_CMD_READ_TEMP: Error on READ_TEMP!!!\n");
	} else {
		// Read temp
	}
}

static void cmd_WRITE_NONCE (int fd, uint8_t chip_id, uint8_t *start_nonce, uint8_t *end_nonce)
{
	FUNC_IN();

	uint8_t nonce_range[NONCE_LEN*2] = {0x00,};

	memcpy(nonce_range, start_nonce, NONCE_LEN);
	memcpy(nonce_range+NONCE_LEN, end_nonce, NONCE_LEN);

	btc08_spi_transfer(fd, SPI_CMD_WRITE_NONCE, chip_id, nonce_range, sizeof(nonce_range), DUMMY_BYTES);
}

static void cmd_READ_HASH (int fd, uint8_t chip_id)
{
	FUNC_IN();

	uint8_t *ret;

	if (chip_id == BCAST_CHIP_ID) {
		printf(" <== SPI_CMD_READ_HASH: Invalid Chip ID(BCAST is not allowed)\n");
		return;
	}

	ret = btc08_spi_transfer(fd, SPI_CMD_READ_HASH, chip_id, NULL, 0, (128 + DUMMY_BYTES));
	if (ret == NULL) {
		printf(" <== Error on READ_HASH\n");
	} else {
		// Read hash
	}
}

static void cmd_READ_FEATURE (int fd, uint8_t chip_id)
{
	FUNC_IN();

	uint8_t *ret;

	if (chip_id == BCAST_CHIP_ID) {
		printf(" <== SPI_CMD_READ_FEATURE: Invalid Chip ID(BCAST is not allowed)\n");
		return;
	}

	ret = btc08_spi_transfer(fd, SPI_CMD_READ_FEATURE, chip_id, NULL, 0, (4 + DUMMY_BYTES));
	if (ret == NULL) {
		printf(" <== Error on READ_HASH\n");
	} else {
		// ret[0-1] [31:20] Fixed value: 0xB5B
		// ret[1]   [19:16] FPGA/ASIC : 0x05/0x0
		// ret[2]   [15:8] 0x00
		// ret[3]   [7:0] Hash depth : 0x86
		int is_FPGA = ((ret[1]&8) == 0x05) ? true:false;
		printf(" <== is_FPGA? (%s), hash_depth(%02X)\n", is_FPGA?"FPGA":"ASIC", ret[3]);
	}
}

static void cmd_READ_REVISION (int fd, uint8_t chip_id)
{
	FUNC_IN();

	uint8_t *ret;

	if (chip_id == BCAST_CHIP_ID) {
		printf(" <== SPI_CMD_READ_REVISION: Invalid Chip ID(BCAST is not allowed)\n");
		return;
	}

	ret = btc08_spi_transfer(fd, SPI_CMD_READ_REVISION, chip_id, NULL, 0, (4 + DUMMY_BYTES));
	if (ret == NULL) {
		printf(" <== Error on READ_REVISION\n");
	} else {
		// Read revision date info (year, month, day, index)		
	}
}

static void cmd_SET_PLL_FOUT_EN (int fd, uint8_t chip_id, uint8_t fout_en)
{
	FUNC_IN();

	uint8_t tx_buf[2];

	// [15:1]: dummy
	// [0]: FOUT_EN (0 : disable, 1: enable) TODO: Get parameter
	tx_buf[0] = 0;
	tx_buf[1] = (fout_en&1);

	btc08_spi_transfer(fd, SPI_CMD_SET_PLL_FOUT_EN, chip_id, tx_buf, sizeof(tx_buf), DUMMY_BYTES);
}

static void cmd_SET_PLL_RESETB (int fd, uint8_t chip_id, uint8_t reset)
{
	FUNC_IN();

	uint8_t resetb[2];

	// [15:1]: dummy
	// [0]: RESETB (0 : reset, 1: on)
	resetb[0] = 0;
	resetb[1] = (reset&1);

	btc08_spi_transfer(fd, SPI_CMD_SET_PLL_RESETB, chip_id, resetb, sizeof(resetb), DUMMY_BYTES);
}

int btc08_spitest_cmds(char *argv, int fd)
{
	int i, result;

	for (i = 0; cmd_table[i].cmd; i++) {
		if (strcmp(cmd_table[i].cmd, argv) == 0 &&
				cmd_table[i].func) {
			result = cmd_table[i].func(fd);
			if (result < 0)
				printf(" <== Error '%s': %s\n", argv, strerror(-result));
			return result;
		}
	}

	printf("Error '%s': Unknown command\n", argv);
	return -1;
}

int btc08_spitest_seqs(char *argv, int fd)
{
	int i, result;

	for (i = 0; seq_table[i].seq; i++) {
		if (strcmp(seq_table[i].seq, argv) == 0 &&
				seq_table[i].func) {
			result = seq_table[i].func(fd);
			if (result < 0)
				printf(" <== Error '%s': %s\n", argv, strerror(-result));
			return result;
		}
	}

	printf("Error '%s': Unknown seqs\n", argv);
	return -1;
}

int main(int argc, char *argv[])
{
	int fd;
	int ret = 0;

	parse_opts(argc, argv);

	// set spi configuration
	fd = open(device, O_RDWR);
	if (fd < 0)
		printf("can't open device %s\n", device);

	pinnum_reset = _6818_FPGA_GPIO_RESET;
	pinnum_irq_oon = _6818_FPGA_GPIO_IRQ_OON;
	pinnum_irq_gn = _6818_FPGA_GPIO_IRQ_GN;

	ret = init_spi_config(fd);
	if (ret < 0)
		pabort("Failed to set spi config");

	if (cmdarg) {
		btc08_spitest_cmds(cmdarg, fd);
	}

	if (seqarg) {
		btc08_spitest_seqs(seqarg, fd);
	}

	close(fd);

	return ret;
}
