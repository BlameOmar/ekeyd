#!/bin/sh
#
# Entropy key udev helper, ensures ekeyd is updated with new devices
#
# Copyright 2009-2011 Simtec Electronics
#
# For licence terms refer to the COPYING file.

BINPATH=/usr/sbin

# This function waits for the USB dev nodes to be made before
# attempting to allow the ULUSBD to attach, this might resolve
# a race condition where udev runs us before the nodes are
# available.
wait_for_usb () {
  COUNTER=0
  while ! test -e /dev/bus/usb/${BUSNUM}/${DEVNUM}; do
    sleep 1
    COUNTER=$(( ${COUNTER} + 1 ))
    test ${COUNTER} -ge 10 && exit 1
  done
  $BINPATH/ekey-ulusbd -b${BUSNUM} -d${DEVNUM} -P/var/run/ekey-ulusbd-${ENTROPY_KEY_SERIAL}.pid -p/var/run/entropykeys/${ENTROPY_KEY_SERIAL} -D
  sleep 1
  $BINPATH/ekeydctl ${ACTION} /var/run/entropykeys/${ENTROPY_KEY_SERIAL}
  exit 0
}

if test "x$SUBSYSTEM" = "xtty"; then
  # Update ekeyd with device operation
  $BINPATH/ekeydctl ${ACTION} /dev/entropykey/${ENTROPY_KEY_SERIAL}
else
  if test "x$ACTION" = "xadd"; then
    # start userland USB connection daemon
    if test "x${BUSNUM}" = "x" -o "x${DEVNUM}" = "x"; then
      exit 0
    fi
    if test -r "/var/run/ekey-ulusbd-${ENTROPY_KEY_SERIAL}.pid"; then
      kill $(cat "/var/run/ekey-ulusbd-${ENTROPY_KEY_SERIAL}.pid") || true
    fi
    mkdir -p /var/run/entropykeys
    wait_for_usb &
    exit 0
  fi
  # Update ekeyd with device operation
  $BINPATH/ekeydctl ${ACTION} /var/run/entropykeys/${ENTROPY_KEY_SERIAL}
  if test "x$ACTION" = "xremove"; then
    rm "/var/run/ekey-ulusbd-${ENTROPYKEY_KEY_SERIAL}.pid"
    rm "/var/run/entropykeys/${ENTROPYKEY_KEY_SERIAL}"
  fi
fi

exit 0

