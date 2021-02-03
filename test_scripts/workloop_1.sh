#!/bin/bash

CMD_RD_ID="00"
CMD_WR_PARM="07"
CMD_WR_TARGET="09"
CMD_WR_NONCE="16"
CMD_RUN_JOB="0B"
CMD_RD_JOBID="0C"
CMD_RD_RESULT="0D"
CMD_CLR_OON="0E"
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

if [ "$(get_pin_oon)" != "0" ]; then
	echo "OON pin error1"
fi
if [ $(get_pin_gn) != "1" ]; then
	echo "GN pin error1"
fi

ret=$(spi_txrx "$CMD_WR_PARM 00$GOLD_MIDSTATE$GOLD_DATA$GOLD_MIDSTATE 00 00")
ret=$(spi_txrx "$CMD_WR_TARGET 00$GOLD_TARGET 00 00")
ret=$(spi_txrx "$CMD_WR_NONCE 00$STARTNONCE$ENDNONCE 00 00")
ret=$(spi_txrx "$CMD_RUN_JOB 00 02 01 00 00")
ret=$(spi_txrx "$CMD_RUN_JOB 00 02 02 00 00")
ret=$(spi_txrx "$CMD_RUN_JOB 00 02 03 00 00")
ret=$(spi_txrx "$CMD_RUN_JOB 00 02 04 00 00")

if [ "$(get_pin_oon)" != "1" ]; then
	echo "OON pin error2"
fi

jobidx=1
jobcnt=1
while ((1))
do
	if [ "$(get_pin_oon)" = "0" ]; then
		echo "`date +"%T"`: pin OON LOW"
		ret=$(spi_txrx "$CMD_RD_ID 01 00 00 00 00 00 00")
		JOB_CNT=`echo $ret | awk '{print $5}'`
		valid_job=$((JOB_CNT))
		left_fifo=$((4-valid_job))
		if (( $left_fifo == 0 )); then
			echo "`date +"%T"`: read_id = ${ret}!!!, fifo full !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
		else
			chipid="00"
			echo "wr job(${jobidx}) ${jobcnt} to ${chipid} at `date +"%T"`"
			ret=$(spi_txrx "$CMD_CLR_OON 00 00 00")
			ret=$(spi_txrx "$CMD_RUN_JOB ${chipid} 02 `printf "%02x" $jobcnt` 00")
			jobcnt=$(( jobcnt+1 ))
			jobidx=$(( jobidx+1 ))
			if [ $jobcnt -gt 16 ]; then 
				jobcnt=1 
			fi
		fi
	fi

	if [ "$(get_pin_gn)" = "0" ]; then
		ret=$(spi_txrx "$CMD_RD_JOBID 00 00 00 00 00 00 00")
		READJOBID_VAL=`echo $ret | awk '{print $3" "$4" "$5" "$6}'`
		oonjobid=`echo $READJOBID_VAL | awk '{print $1}'`
		gnjobid=`echo $READJOBID_VAL | awk '{print $2}'`
		IFLAGS=`echo $READJOBID_VAL | awk '{print $3}'`
		chipid=`echo $READJOBID_VAL | awk '{print $4}'`
		echo "`date +"%T"` pin GN LOW"
		gnflag=$((IFLAGS&1))
		if (( $gnflag != 1 )); then
			echo "`date +"%T"`: flags = $IFLAGS!!!, it's not GN $READJOBID_VAL!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
		else
			echo "`date +"%T"` GN ($READJOBID_VAL): jobid = $gnjobid, chipid = $chipid"
			HASHS=`spidevtest -s 500000 -D /dev/spidev0.0 -v -p "\x20\x${chipid}\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" | grep "RX | " | awk -F'|'  '{print $2}' | tr '\n' ' ' | sed 's/    / /g' | sed 's/ __//g'`
			ret=$(spi_txrx "$CMD_RD_RESULT ${chipid} 00 00 00 00 00 00 00 00 00 00 00 00")
			NONCE=`echo $ret | awk '{print $3$4$5$6" "$7$8$9$10}'`
			echo "`date +"%T"` hash at chip${chipid} : ${HASHS}, nonce: ${NONCE}, jobid = ${gnjobid}"
		fi
	fi
done

exit

