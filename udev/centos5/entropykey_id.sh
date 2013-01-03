#!/bin/sh
# 
# Helper for entropy key udev rules to reformat values
#
# entropykey_id.sh <ID_SERIAL> <BUSNUM> <DEVNUM>

# convert ID_SERIAL to ID_SERIAL_SHORT
ID_SERIAL_SHORT=$(echo ${1} | tr / . | awk -F_ '{print $NF}' )
echo "ENTROPY_KEY_SERIAL=${ID_SERIAL_SHORT}"

# convert busnum
if [ "x${2}" != "x" ]; then
BUSNUM=$(echo ${2} | awk -F- '{printf("%03d", $1)}')
echo "BUSNUM=${BUSNUM}"
fi

# device number
if [ "x${3}" != "x" ]; then
DEVNUM=$(echo ${3} | awk '{printf("%03d", $1)}')
echo "DEVNUM=${DEVNUM}"
fi
