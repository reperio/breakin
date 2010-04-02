#!/bin/ash

trymount() {
	DEV_NAME=$1
	#source /tmp/vol_id.tmp # might use these variables some day
	mount /dev/${DEV_NAME} /var/snapshot
	if [ $? != 0 ]
	then
		echo "Mount error for found USB device."
		return 1
	fi
	echo "USB snapshot device found at ${DEV_NAME}."
	
	echo "==== INFO ===" > /var/snapshot/breakin.dmp
	echo "Breakin version: 2.2" >> /var/snapshot/breakin.dmp
	uname -a >> /var/snapshot/breakin.dmp
	echo "==== breakin.dat ====" >> /var/snapshot/breakin.dmp
	cat /var/run/breakin.dat >> /var/snapshot/breakin.dmp
	echo "==== breakin.log ====" >> /var/snapshot/breakin.dmp
	cat /var/log/breakin.log >> /var/snapshot/breakin.dmp
	echo "==== syslog ====" >> /var/snapshot/breakin.dmp
	cat /var/log/messages >> /var/snapshot/breakin.dmp
	echo "==== cmdline ====" >> /var/snapshot/breakin.dmp
	cat /proc/cmdline >> /var/snapshot/breakin.dmp
	echo "==== cpuinfo ====" >> /var/snapshot/breakin.dmp
	cat /proc/cpuinfo >> /var/snapshot/breakin.dmp
	echo "==== partitions ====" >> /var/snapshot/breakin.dmp
	cat /proc/partitions >> /var/snapshot/breakin.dmp
	echo "==== PCI ====" >> /var/snapshot/breakin.dmp
	lspci -vv >> /var/snapshot/breakin.dmp
	echo "==== dmi ====" >> /var/snapshot/breakin.dmp
	/usr/bin/dmidecode >> /var/snapshot/breakin.dmp

	sync

	umount /var/snapshot

	echo "Data written to 'breakin.dmp', safe to remove USB device."
	exit 0
}

for i in `ls -1d /sys/block/sd[a-z] /sys/block/hd[a-z] 2>/dev/null`
do
	REMOVABLE=$(< ${i}/removable)
	if [ "${REMOVABLE}" != 1 ]
	then
		ls -l ${i}/device | grep usb > /dev/null
		if [ $? != 0 ]
		then
			continue # skip non-removable, non-usb devices
		fi
	fi
	#DEV_NAME=`basename ${i}`
	#vol_id -x /dev/${DEV_NAME} > /tmp/vol_id.tmp
	#if [ $? != 0 ] # no file system found
	#then
		for j in `ls -1d ${i}/?d?[0-9] 2>/dev/null`
		do
			DEV_NAME=`basename ${j}`
			#vol_id -x /dev/${DEV_NAME} > /tmp/vol_id.tmp
			#if [ $? = 0 ]
			#then
				trymount $DEV_NAME
			#fi
		done
	#else # found file system
	#	trymount $DEV_NAME
	#fi
done

echo "No USB device found."
exit 1

