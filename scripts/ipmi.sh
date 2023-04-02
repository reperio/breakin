#!/bin/sh

DAEMON=$1

if [ ! -e /dev/ipmi0 ]
then
	exit
fi

get_ipmi() {
	ipmitool sdr type Temperature 2>/dev/null | cut -d"|" -f1,5 | sed s/Temperature// > /tmp/ipmi_temp.log.tmp
	mv /tmp/ipmi_temp.log.tmp /tmp/ipmi_temp.log

	ipmitool sdr type Fan 2>/dev/null | cut -d"|" -f1,5 | sed s/Fan// > /tmp/ipmi_fan.log.tmp
	mv /tmp/ipmi_fan.log.tmp /tmp/ipmi_fan.log
}

if [ "${DAEMON}" != "" ]
then
	while [ 1 ]
	do
		get_ipmi
		sleep ${DAEMON}
	done
else
	get_ipmi
fi
