#!/bin/sh

############################################################################
## BIST

# SET_PLL_FOUT_EN00 (PLL FOUT Disable)
#spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x34\x00\x00\x00" > /dev/null 2>&1

# Golden Data
GOLD_MIDSTATE="\x5f\x4d\x60\xa2\x53\x85\xc4\x07\xc2\xa8\x4e\x0c\x25\x91\x69\xc4\x10\xa4\xa5\x4b\x93\xf7\x17\x08\xf1\xab\xdf\xec\x6e\x8b\x81\xd2"
GOLD_DATA="\xf4\x2a\x1d\x6e\x5b\x30\x70\x7e\x17\x37\x6f\x56"
GOLD_NONCE="\x66\xcb\x34\x26"
GOLD_NONCE_RANGE="\x66\xcb\x34\x20\x66\xcb\x34\x30"
GOLD_TARGET="\x17\x37\x6f\x56\x05\x00"
GOLD_HASH="\x00\x00\x00\x00\x00\x00\x00\x00\x00\x22\x09\x3d\xd4\x38\xed\x47\xfa\x28\xe7\x18\x58\xb8\x22\x0d\x53\xe5\xcd\x83\xb8\xd0\xd4\x42"

############################################################################
# BIST Sequences
echo "####### WRITE_PARM #######"
spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x07\x00"$GOLD_MIDSTATE$GOLD_DATA$GOLD_MIDSTATE$GOLD_MIDSTATE$GOLD_MIDSTATE"\x00\x00" | grep "X | "

echo "####### WRITE_TARGET #######"
spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x09\x00"$GOLD_TARGET"\x00\x00" | grep "X | "

echo "####### WRITE_NONCE #######"
spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x16\x00"$GOLD_NONCE$GOLD_NONCE"\x00\x00" | grep "X | "

echo "####### SET_DISABLE (Applied after RUN_BIST) #######"
spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x10\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfe\x00\x00" > /dev/null 2>&1

echo "####### RUN_BIST #######"
spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x02\x00"$GOLD_HASH$GOLD_HASH$GOLD_HASH$GOLD_HASH"\x00\x00" | grep "X | "

echo "####### READ_BIST #######"
CORE_NUM=`spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x03\x01\x00\x00\x00\x00" | grep "RX | " | awk -F' ' '{print $6}'`
echo "chipid_1: The number of cores passed BIST: ${CORE_NUM}"
CORE_NUM=`spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x03\x02\x00\x00\x00\x00" | grep "RX | " | awk -F' ' '{print $6}'`
echo "chipid_2: The number of cores passed BIST: ${CORE_NUM}"
CORE_NUM=`spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x03\x03\x00\x00\x00\x00" | grep "RX | " | awk -F' ' '{print $6}'`
echo "chipid_3: The number of cores passed BIST: ${CORE_NUM}"

echo "####### READ_ID #######"
CHIP_ID=`spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x00\x01\x00\x00\x00\x00\x00\x00" | grep "RX | " | awk -F' ' '{print $8}'`
echo "chipid_1: chipid = ${CHIP_ID}"
CHIP_ID=`spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x00\x02\x00\x00\x00\x00\x00\x00" | grep "RX | " | awk -F' ' '{print $8}'`
echo "chipid_2: chipid = ${CHIP_ID}"
CHIP_ID=`spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x00\x03\x00\x00\x00\x00\x00\x00" | grep "RX | " | awk -F' ' '{print $8}'`
echo "chipid_3: chipid = ${CHIP_ID}"
