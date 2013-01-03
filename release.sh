#!/bin/sh

# Simtec entropy key release tar builder

# usage: release.sh <version>

set -e

# Determine current location
LOC=$(svn info|grep URL|cut -d\  -f2-)

if [ "${1}x" = "x" ]; then
    echo "Must supply a version number"
    exit 1
fi

VERSION=${1}

if [ -f ekeyd-${VERSION}.tar.gz ]; then
    exit 0
fi

# if SVN credentials are provided to us, use them
if [ "x${SVN_USER}" != "x" ]; then
    SVN_OPTS="--non-interactive --no-auth-cache --username ${SVN_USER} --password ${SVN_PASS}"
else
    SVN_OPTS=""
fi

svn export ${SVN_OPTS} ${LOC} ekeyd-${VERSION}
rm -fr ekeyd-${VERSION}/doc/*.pdf
rm -fr ekeyd-${VERSION}/doc/*.txt
rm -fr ekeyd-${VERSION}/doc/*.dot
rm -fr ekeyd-${VERSION}/doc/*.svg
rm -fr ekeyd-${VERSION}/device/ekey.*
rm -fr ekeyd-${VERSION}/device/firmware
rm -fr ekeyd-${VERSION}/tools
rm -fr ekeyd-${VERSION}/debian
rm -fr ekeyd-${VERSION}/rpm
rm -fr ekeyd-${VERSION}/artwork
rm -fr ekeyd-${VERSION}/bringup
rm -fr ekeyd-${VERSION}/iso
tar cfz ekeyd-${VERSION}.tar.gz ekeyd-${VERSION}
rm -fr ekeyd-${VERSION}
tar tfz ekeyd-${VERSION}.tar.gz
