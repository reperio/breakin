#!/bin/sh
killall -9 @DESTDIR@/bin/hpl
killall -9 @DESTDIR@/etc/breakin/ipmi.sh
sync
echo "Breakin was stopped"
