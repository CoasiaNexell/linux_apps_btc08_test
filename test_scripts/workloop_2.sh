#!/bin/bash

DUMMY_SPI='00 00 '
ZEROS_16BITS='00 00 '
ZEROS_32BITS=$ZEROS_16BITS$ZEROS_16BITS
ZEROS_64BITS=$ZEROS_32BITS$ZEROS_32BITS
ZEROS_128BITS=$ZEROS_64BITS$ZEROS_64BITS
ZEROS_256BITS=$ZEROS_128BITS$ZEROS_128BITS
ZEROS_512BITS=$ZEROS_256BITS$ZEROS_256BITS
ZEROS_1024BITS=$ZEROS_512BITS$ZEROS_512BITS

CMD_RD_ID="00"
CMD_WR_PARM="07"
CMD_WR_TARGET="09"
CMD_WR_NONCE="16"
CMD_RUN_JOB="0B"
CMD_RD_JOBID="0C"
CMD_RD_RESULT="0D"
CMD_CLR_OON="0E"
CMD_RD_HASH="20"
GNPINNUM=126
ONPINNUM=125

STARTNONCE=" 60 00 00 00"
ENDNONCE=" 6f ff ff ff"
#STARTNONCE=" 66 c0 00 00"
#ENDNONCE=" 66 cf ff ff"

GOLD_MIDSTATE=" 5f 4d 60 a2 53 85 c4 07 c2 a8 4e 0c 25 91 69 c4 10 a4 a5 4b 93 f7 17 08 f1 ab df ec 6e 8b 81 d2"
GOLD_DATA=" f4 2a 1d 6e 5b 30 70 7e 17 37 6f 56"
#GOLD_NONCE="66 cb 34 26"
GOLD_TARGET=" 17 37 6f 56 05 00"
GOLD_HASH=" 00 00 00 00 00 00 00 00 00 22 09 3d d4 38 ed 47 fa 28 e7 18 58 b8 22 0d 53 e5 cd 83 b8 d0 d4 42"

CMP_GOLDEN_HASH1=" 00 00 00 00 00 00 00 00 00 00 00 22 09 3D D4 38 ED 47 FA 28 E7 18 58 B8 22 0D 53 E5 CD 83 B8 D0 D4 42"
CMP_GOLDEN_HASHS=$CMP_GOLDEN_HASH1$CMP_GOLDEN_HASH1

RD_HASH=" 80 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01"
############################################################################
## JOB

function spi_txrx() {
	local wr_packet=""
	for byte in $1
	do
		wr_packet=$wr_packet"\x"$byte
	done
	rd_packet=`spidevtest -s 500000 -D /dev/spidev0.0 -v -p $wr_packet | grep "RX | " | sed 's/.....//' | sed 's/__ //g' | sed 's/  |//g'`
	echo "(`date +"%T"`)--> $1" > /dev/console
	echo "(`date +"%T"`)<-- ${rd_packet}" > /dev/console
	echo ${rd_packet}
}

#packet=$(spi_txrx "0c 00 00 00 00 00 00 00")

#echo $packet
#exit

function get_pin_oon {
	local pin=`cat /sys/class/gpio/gpio${ONPINNUM}/value`
	echo "(`date +"%T"`) read oon pin : $pin" > /dev/console
	echo $pin
}

function get_pin_gn {
	local pin=`cat /sys/class/gpio/gpio${GNPINNUM}/value`
	echo "(`date +"%T"`) read gn pin : $pin" > /dev/console
	echo $pin
}

############################################################################
# ret = {header1 mid1 hash1 header2 mid2 hash2 header3 mid3 hash3 header4 mid4 hash4}
####### ret = {vmask1 vmask2 vmask3 vmask4 header mid1 mid2 mid3 mid4 hash1 hash2 hash3 hash4}
function gen_data {
	vmask1=`printf "%04x%04x" $RANDOM $RANDOM`
	vmask2=`printf "%04x%04x" $RANDOM $RANDOM`
	vmask3=`printf "%04x%04x" $RANDOM $RANDOM`
	vmask4=`printf "%04x%04x" $RANDOM $RANDOM`
	seed=`printf "%04x%04x" $RANDOM $RANDOM`
	prevhash=`echo -n $seed | xxd -r -p | openssl dgst -sha256 | awk '{print $2}'`
	seed=`printf "%04x%04x" $RANDOM $RANDOM`
	merklehash=`echo -n $seed | xxd -r -p | openssl dgst -sha256 | awk '{print $2}'`

	timebit=`printf "%04x%04x" $RANDOM $RANDOM`
	target=`printf "%04x%04x" $RANDOM $RANDOM`
	nonce=`printf "%04x%04x" $RANDOM $RANDOM`

	echo '==================='      > /dev/console
	echo "vmask1     = $vmask1"     > /dev/console
	echo "vmask2     = $vmask2"     > /dev/console
	echo "vmask3     = $vmask3"     > /dev/console
	echo "vmask4     = $vmask4"     > /dev/console
	echo "prevhash   = $prevhash"   > /dev/console
	echo "merklehash = $merklehash" > /dev/console
	echo "timebit    = $timebit"    > /dev/console
	echo "target     = $target"     > /dev/console
	echo "nonce      = $nonce"      > /dev/console
	echo '-------------------'      > /dev/console

	vmask1_le=`./be2le.sh $vmask1`
	vmask2_le=`./be2le.sh $vmask2`
	vmask3_le=`./be2le.sh $vmask3`
	vmask4_le=`./be2le.sh $vmask4`
	prevhash_le=`./be2le.sh $prevhash`
	merklehash_le=`./be2le.sh $merklehash`
	timebit_le=`./be2le.sh $timebit`
	target_le=`./be2le.sh $target`
	nonce_le=`./be2le.sh $nonce`

	header1="$vmask1_le$prevhash_le$merklehash_le$timebit_le$target_le$nonce_le"
	header2="$vmask2_le$prevhash_le$merklehash_le$timebit_le$target_le$nonce_le"
	header3="$vmask3_le$prevhash_le$merklehash_le$timebit_le$target_le$nonce_le"
	header4="$vmask4_le$prevhash_le$merklehash_le$timebit_le$target_le$nonce_le"
	header1_64=`echo ${header1:0:128}`
	header2_64=`echo ${header2:0:128}`
	header3_64=`echo ${header3:0:128}`
	header4_64=`echo ${header4:0:128}`

	midstate1=`python3 midstate.py $header1 | grep midstate | awk '{print $3}'`
	midstate2=`python3 midstate.py $header2 | grep midstate | awk '{print $3}'`
	midstate3=`python3 midstate.py $header3 | grep midstate | awk '{print $3}'`
	midstate4=`python3 midstate.py $header4 | grep midstate | awk '{print $3}'`

	#echo 'midstate1 = '$midstate1 > /dev/console
	#echo 'midstate2 = '$midstate2 > /dev/console
	#echo 'midstate3 = '$midstate3 > /dev/console
	#echo 'midstate4 = '$midstate4 > /dev/console

	hash1=`echo -n $header1 | xxd -r -p | openssl dgst -sha256 -binary | openssl dgst -sha256 | awk '{print $2}'`
	hash2=`echo -n $header2 | xxd -r -p | openssl dgst -sha256 -binary | openssl dgst -sha256 | awk '{print $2}'`
	hash3=`echo -n $header3 | xxd -r -p | openssl dgst -sha256 -binary | openssl dgst -sha256 | awk '{print $2}'`
	hash4=`echo -n $header4 | xxd -r -p | openssl dgst -sha256 -binary | openssl dgst -sha256 | awk '{print $2}'`

	ret="$header1 $midstate1 $header2 $midstate2 $header3 $midstate3 $header4 $midstate4 $hash1 $hash2 $hash3 $hash4"
	echo ${ret}
}

############################################################################
#header[128:135] = merkle_left
#header[136:143] = time
#header[144:151] = nbits
#header[152:159] = nonce

function convert_to_hw_parm {
	datas=$1
	#echo "parm input : $datas" > /dev/console
	mid1=`echo $datas | awk '{print $2}'`
	mid2=`echo $datas | awk '{print $4}'`
	mid3=`echo $datas | awk '{print $6}'`
	mid4=`echo $datas | awk '{print $8}'`
	#echo "mid1 : $mid1" > /dev/console

	MIDSTATE1_=`./be2le.sh $mid1 -4`
	MIDSTATE2_=`./be2le.sh $mid2 -4`
	MIDSTATE3_=`./be2le.sh $mid3 -4`
	MIDSTATE4_=`./be2le.sh $mid4 -4`
	MIDSTATE1=`./be2le.sh $MIDSTATE1_ -f bb | tr '[a-f]' '[A-F]'`
	MIDSTATE2=`./be2le.sh $MIDSTATE2_ -f bb | tr '[a-f]' '[A-F]'`
	MIDSTATE3=`./be2le.sh $MIDSTATE3_ -f bb | tr '[a-f]' '[A-F]'`
	MIDSTATE4=`./be2le.sh $MIDSTATE4_ -f bb | tr '[a-f]' '[A-F]'`

	header1=`echo $datas | awk '{print $1}'`
	#echo "header1 : $header1" > /dev/console
	merkle_left_=`echo ${header1:128:8}`
	MERKLE_LEFT=`./be2le.sh $merkle_left_ -f bb`
	#echo "merkle_left : $MERKLE_LEFT" > /dev/console
	time_=`echo ${header1:136:8}`
	TIME=`./be2le.sh $time_ -f bb`
	nbits_=`echo ${header1:144:8}`
	NBITS=`./be2le.sh $nbits_ -f bb`

	echo "$MIDSTATE1$MERKLE_LEFT$TIME$NBITS$MIDSTATE2$MIDSTATE3$MIDSTATE4"
}

FULL_TARGET='FF FF FF FF 06 50 '

function convert_to_hw_nonce {
	datas=$1
	header1=`echo $datas | awk '{print $1}'`
	nonce_=`echo ${header1:152:8}`
	NONCE=`./be2le.sh $nonce_ -f bb | tr '[a-f]' '[A-F]'`
	echo ${NONCE}
}

function convert_to_hw_hashs {
	datas=$1
	HASH1=`echo $datas | awk '{print $9}'`
	HASH2=`echo $datas | awk '{print $10}'`
	HASH3=`echo $datas | awk '{print $11}'`
	HASH4=`echo $datas | awk '{print $12}'`
	HASHS=`./be2le.sh  $HASH1 -f bb | tr '[a-f]' '[A-F]'`
	HASHS+=`./be2le.sh $HASH2 -f bb | tr '[a-f]' '[A-F]'`
	HASHS+=`./be2le.sh $HASH3 -f bb | tr '[a-f]' '[A-F]'`
	HASHS+=`./be2le.sh $HASH4 -f bb | tr '[a-f]' '[A-F]'`
	echo ${HASHS}
}

############################################################################
# test
#ret=$(gen_data)
#echo "=========================================================="
#echo "=$ret"
#echo "----------------------------------------------------------"
#HWPARMS=$(convert_to_hw_parm "${ret}")
#NONCE=$(convert_to_hw_nonce "$ret")
#HASHS=$(convert_to_hw_hashs "$ret")
#echo ":$HWPARMS"
#echo ":$NONCE"
#echo ":$HASHS"
#echo "=========================================================="
#
#exit

############################################################################
if [ "$(get_pin_oon)" != "0" ]; then
	echo "OON pin error1"
fi
if [ $(get_pin_gn) != "1" ]; then
	echo "GN pin error1"
fi

test_cnt=0
hash_oks=0
nonce_oks=0
hash_errors=0
nonce_errors=0
while ((1))
do
	header=$(gen_data)

	HWPARMS=$(convert_to_hw_parm "${header}")
	NONCES=$(convert_to_hw_nonce "${header}")
	HASHS=$(convert_to_hw_hashs "${header}")

	ret=$(spi_txrx "$CMD_WR_PARM 00 $HWPARMS 00 00")
	ret=$(spi_txrx "$CMD_WR_TARGET 00 $FULL_TARGET 00 00")
	ret=$(spi_txrx "$CMD_WR_NONCE 00 $NONCES $NONCES 00 00")

	ret=$(spi_txrx "$CMD_RUN_JOB 00 03 01 00 00")

	# read job id
	ret=$(spi_txrx "0C 00 $ZEROS_32BITS$DUMMY_SPI")

	chipid_str=`echo $ret | awk '{print $6}'`
	flags_str=`echo $ret | awk '{print $5}'`
	chipid=$((chipid_str))
	flags=$((flags_str))
	echo "flags = $flags, chipid = $chipid"

	chip1_oon=0
	chip2_oon=0
	chip3_oon=0

	while (( 1 )); do
		if ((flags & 2)); then
			ret=$(spi_txrx "0e $chipid_str$DUMMY_SPI")
			if [ $chipid_str == "01" ]; then
				chip1_oon=1
			fi
			if [ $chipid_str == "02" ]; then
				chip2_oon=1
			fi
			if [ $chipid_str == "03" ]; then
				chip3_oon=1
			fi
		fi

		# read job id
		ret=$(spi_txrx "0C 00 $ZEROS_32BITS$DUMMY_SPI")

		chipid_str=`echo $ret | awk '{print $6}'`
		flags_str=`echo $ret | awk '{print $5}'`
		chipid=$((chipid_str))
		flags=$((flags_str))
		echo "flags = $flags, chipid = $chipid"

		if ((flags & 1)); then
			ret=$(spi_txrx "20 $chipid_str $ZEROS_1024BITS$DUMMY_SPI")
			hw_hashs=`echo $ret | sed 's/......//' | sed 's/......$//'`

			if [ "$HASHS" != "$hw_hashs" ]; then
				echo '!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!error!!!!!!!!!!!!!!!!!!!!!!!!!!!!'
				echo "cal hash = $HASHS"
				echo "HW  hash = $hw_hashs"
				hash_errors=$((hash_errors+1))
			else
				echo 'cal hash == hw hash : ok~'
				hash_oks=$((hash_oks+1))
			fi

			# read result from chip_id 3
			ret=$(spi_txrx "0D $chipid_str $ZEROS_128BITS$ZEROS_16BITS$DUMMY_SPI")
			hw_nonces=`echo $ret | sed 's/......//' | sed 's/............$//'`

			if [ "$NONCES $NONCES $NONCES $NONCES" != "$hw_nonces" ]; then
				echo '!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!error!!!!!!!!!!!!!!!!!!!!!!!!!!!!'
				echo "cal nonce = $NONCES $NONCES $NONCES $NONCES"
				echo "HW  nonce = $hw_nonces"
				nonce_errors=$((nonce_errors+1))
			else
				echo 'cal nonce == hw nonce : ok~'
				nonce_oks=$((nonce_oks+1))
			fi
		fi

		if [ $chip1_oon == 1 ] && [ $chip2_oon == 1 ] && [ $chip3_oon == 1 ] ; then
			echo "---- all chip oon ----"
			break
		fi

		sleep 1
		# read job id
		ret=$(spi_txrx "0C 00 $ZEROS_32BITS$DUMMY_SPI")

		chipid_str=`echo $ret | awk '{print $6}'`
		flags_str=`echo $ret | awk '{print $5}'`
		chipid=$((chipid_str))
		flags=$((flags_str))
		echo "flags = $flags, chipid = $chipid"

	done
	test_cnt=$((test_cnt+1))
	echo "test_cnt = $test_cnt"
	echo "hash oks = $hash_oks"
	echo "nonce oks = $nonce_oks"
	echo "hash errors = $hash_errors"
	echo "nonce errors = $nonce_errors"
	echo '==================='      > /dev/console
done

exit 
