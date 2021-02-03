//vi set ts=8

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>	// open/read
#include <stdio.h>	// printf
#include <stdlib.h>	// atoi

#define MAX_VOL_TABLES		(34)
#define TEMP_PER_INDEX		(5.)
#define	MIN_TEMP			(-40.)

float gstAdcVoltage[MAX_VOL_TABLES] = 
{
	1716.35, 1690.77, 1659.26, 1621.23, 1576.03, 1523.37, 1463.17, 1395.77,
	1321.78, 1242.20, 1158.51, 1072.43,  985.88,  900.00,  816.87,  737.48,
	662.77,  593.24,  529.26,  470.90,  418.25,  370.86,  328.57,  291.07,
	257.84,  228.08,  201.56,  177.93,  157.10,  138.82,  122.72,  108.60,
	96.26,   85.48,
};


/*
 * 0x000 : 0V
 * 0xFFF : 1.8V
 * (1.8/4096)xADC = voltage
 * 12bit adc => 4096
 */
#define ad2mV(adc)  ((adc*1800.)/4096.)

static float get_mvolt(int ch)
{
	float ret = -1;
	int fd, val = 0;
	char adcpath[128], line[64];

	sprintf(adcpath, "/sys/bus/iio/devices/iio:device0/in_voltage%d_raw", ch);

	fd = open(adcpath, O_RDONLY);

	ret = read(fd, &line, 4);

	val = atoi(line);

	ret = ad2mV((float)val);

	close(fd);

	return ret;
}

static float get_temp( float mV )
{
	int index = -1;
	float temperature;

	for( int i=0 ; i<MAX_VOL_TABLES ; i++ )
	{
		if( mV > gstAdcVoltage[i] )
		{
			index = i;
			break;
		}
	}

	temperature = index * TEMP_PER_INDEX + MIN_TEMP;

	//	Compansation temperature using linear algorithm
	if( index != 0 )
	{
		float gap = TEMP_PER_INDEX * ( (mV - gstAdcVoltage[index]) / (gstAdcVoltage[index-1] - gstAdcVoltage[index]));
		//printf("gap = %.2f\n", gap);
		temperature = temperature - gap;
	}
	return temperature;
}

int main( int argc, char *argv[] )
{
	int ch = 1;
	float mv;
	if( argc > 1 )
		ch = atoi(argv[1]);

	for( int i = 0 ; i<MAX_VOL_TABLES ; i++ )
	{
		printf("%.2f,\n", gstAdcVoltage[i]*10/9);
	}

	while( 1 )
	{
		mv = get_mvolt(ch);
		printf("Channel %d Voltage = %.2f mV, Temperature = %.2f C\n", ch, mv, get_temp(mv) );
		sleep(1);
	}

	return 0;
}
