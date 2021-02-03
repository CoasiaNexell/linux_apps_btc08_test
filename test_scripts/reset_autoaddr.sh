#!/bin/sh

############################################################################
## Variable for the maximum SPI transfer speed (Default: 500 KHz)
FREQ=$1
if (( $# > 0 )); then
	FREQ=$1
else
	FREQ=500000
fi

echo "Maximum SPI transfer speed: $FREQ"

############################################################################
## Set RESET GPIO Low and High
echo 0 > /sys/class/gpio/gpio127/value
sleep 0.1
echo 1 > /sys/class/gpio/gpio127/value

############################################################################
# AUTO_ADDRESS
CHIPCNT=`spidevtest -s 50000 -D /dev/spidev0.0 -v -p "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" | grep "RX | " | sed '1d' | awk -F' ' '{print $6}'`
echo "The number of chips: ${CHIPCNT}"

############################################################################
# SET_CONTROL (Set INT enable - [4]:OON IRQ enable of extra 16 bits)
echo "Set OON IRQ enable"
spidevtest -s $FREQ -D /dev/spidev0.0 -v -p "\x12\x00\x00\x18\x00\x1f" > /dev/null 2>&1

############################################################################
# AUTO ADDRESS
CHIPCNT=`spidevtest -s $FREQ -D /dev/spidev0.0 -v -p "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" | grep "RX | " | sed '1d' | awk -F' ' '{print $6}'`
echo "The number of chips: ${CHIPCNT}"

############################################################################
# READ_DISABLE (Read a disable status of each core from return 256 bits)
#corebits1=`spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x11\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" | grep "RX | " | awk -F'|' '{print $2}' | sed '2d' | sed 's/.......//' | sed 's/..$//'`

############################################################################
# READ_BIST
#corecnt=`spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x03\x01\x00\x00" | grep "RX | " | awk -F' ' '{print $6}'`

############################################################################
# READ_PLL
spidevtest -s $FREQ -D /dev/spidev0.0 -v -p "\x06\x01\x00\x00\x00\x00\x00\x00" | grep "RX | " | awk -F' ' '{print $5$6$7$8}'
spidevtest -s $FREQ -D /dev/spidev0.0 -v -p "\x06\x02\x00\x00\x00\x00\x00\x00" | grep "RX | " | awk -F' ' '{print $5$6$7$8}'
spidevtest -s $FREQ -D /dev/spidev0.0 -v -p "\x06\x03\x00\x00\x00\x00\x00\x00" | grep "RX | " | awk -F' ' '{print $5$6$7$8}'
