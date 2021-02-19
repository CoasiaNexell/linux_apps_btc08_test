#!/bin/sh

#set -e

############################################################################
## BIST

# fout en 0
#spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x34\x00\x00\x00" > /dev/null 2>&1

GOLD_MIDSTATE="\x5f\x4d\x60\xa2\x53\x85\xc4\x07\xc2\xa8\x4e\x0c\x25\x91\x69\xc4\x10\xa4\xa5\x4b\x93\xf7\x17\x08\xf1\xab\xdf\xec\x6e\x8b\x81\xd2"
GOLD_DATA="\xf4\x2a\x1d\x6e\x5b\x30\x70\x7e\x17\x37\x6f\x56"
GOLD_NONCE="\x66\xcb\x34\x26"
GOLD_NONCE_RANGE="\x66\xcb\x00\x00\x66\xcb\xff\xff"
GOLD_TARGET="\x17\x37\x6f\x56\x05\x00"
GOLD_HASH="\x00\x00\x00\x00\x00\x00\x00\x00\x00\x22\x09\x3d\xd4\x38\xed\x47\xfa\x28\xe7\x18\x58\xb8\x22\x0d\x53\xe5\xcd\x83\xb8\xd0\xd4\x42"

#parameter GOLD_MIDSTATE	= 256'h5f4d60a2_5385c407_c2a84e0c_259169c4_10a4a54b_93f71708_f1abdfec_6e8b81d2;
#parameter GOLD_DATA	=  96'hf42a1d6e_5b30707e_17376f56; // merkle_root_timestamp_difficulty
#parameter GOLD_NONCE	=  64'h66cb3426_66cb3426;
#parameter DIFFICULTY	= 32'h17376f56;
#parameter GOLD_HASH	= 256'h00000000_00000000_0022093d_d438ed47_fa28e718_58b8220d_53e5cd83_b8d0d442;

#parameter GOLD_TARGET	= 32'h17376f56;

echo "####### WRITE_PARM #######"
spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x07\x00"$GOLD_MIDSTATE$GOLD_DATA$GOLD_MIDSTATE"\x00\x00"

echo "####### WRITE_TARGET #######"
spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x09\x00"$GOLD_TARGET"\x00\x00"

echo "####### WRITE_NONCE #######"
#spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x16\x00\x00\x00\x00\x00\xff\xff\xff\xff\x00\x00"
spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x16\x00"$GOLD_NONCE_RANGE"\x00\x00"
#spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x16\x00\x99\x62\xe3\x01\x99\x62\xe3\x01\x00\x00" > /dev/null 2>&1

#spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x07\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" > /dev/null 2>&1
#spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x09\x00\xff\xff\xff\xff\x06\x40\x00\x00" > /dev/null 2>&1
#spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x16\x00\xff\xff\xff\x0b\xff\xff\xff\x0b\x00\x00" > /dev/null 2>&1

echo "####### RUN_JOB #######"
# run job without asicboost
#spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x0b\x00\x00\x01\x01\x01"
# run job with asicboost
spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x0b\x00\x00\x21\x01\x01"

#sleep 0.5

# read FSM
#spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x15\x01\x00\x01\x01\x01\x01\x01\x01\x01"

exit

############################################################################
echo "####### READ_JOB_ID #######"
spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x0c\x00\x00\x00\x00\x00\x00\x00"

############################################################################
# read result from chip_id 3
echo "####### READ_RESULT #######"
spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x0d\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"

############################################################################
echo "####### CLEAR_OUT_OF_NONCE #######"
spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x0e\x00\x00\x00"

echo "####### READ_BIST #######"
spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x03\x01\x00\x00"
spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x03\x02\x00\x00"
spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x03\x03\x00\x00"
spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x03\x01\x00\x00"
spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x03\x02\x00\x00"
spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x03\x03\x00\x00"
exit 
readbist=`spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x03\x01\x00\x00" | grep "RX | " | awk -F' ' '{print $5$6}'`
bistflag=`spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x03\x01\x00\x00" | grep "RX | " | awk -F' ' '{print $5}'`
corecnt=`spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x03\x01\x00\x00" | grep "RX | " | awk -F' ' '{print $6}'`
echo "after bist : result = 0x$readbist, flag = "$(((0x${bistflag}&1)^1)), core num = $((0x$corecnt^255))
exit

corebits1=`spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x11\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" | grep "RX | " | awk -F'|' '{print $2}' | sed '2d' | sed 's/.......//' | sed 's/..$//'`
corebits2=`spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x11\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" | grep "RX | " | awk -F'|' '{print $2}' | sed '1d' | sed 's/.//' | sed 's/ __//g' | sed 's/........$//'`

corecnt_=$((0x${corecnt}))
echo "bits = ${corebits1} ${corebits2} : cores =  ${corecnt_}(0x${corecnt})"

if [ "${corecnt}" = "00" ]; then
	exit 1
fi

# fout en 1
#spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x34\x00\x00\x01" > /dev/null 2>&1
