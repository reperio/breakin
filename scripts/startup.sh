#!/bin/ash

ARGS=""

if [ "${_update_interval}" != "" ]
then
	ARGS="${ARGS} --update_interval ${_update_interval}"
fi

if [ "${_update_url}" != "" ]
then
	ARGS="${ARGS} --update_url ${_update_url}"
fi

if [ "${_sshpasswd}" != "" ]
then
	ARGS="${ARGS} --ssh"
fi

if [ "${_breakin_disable}" != "" ]
then
	ARGS="${ARGS} --disable_test ${_breakin_disable}"
fi

if [ "${_console}" != "" ]
then
        PORT=`echo ${CONSOLE} | cut -d"," -f1`
        BAUD=`echo ${CONSOLE} | cut -d"," -f2`

	if [ "${PORT}" = "ttyS0" ]
	then
		ARGS="${ARGS} --serialdev=/dev/${PORT} --baud=${BAUD}"
	fi
fi

echo "Getting IPMI values"
/etc/breakin/ipmi.sh

echo "Starting IPMI server process"
/etc/breakin/ipmi.sh 30 &

echo "Flushing the mcelog for bogus MCE events"
/usr/bin/mcelog --dmi >/dev/null 2>&1

exec /usr/local/bin/breakin ${ARGS}
