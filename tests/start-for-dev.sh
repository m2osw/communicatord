#!/bin/sh -e
#
# Start the communicatord locally for a developer

# Make sure that the Debug binary exists
BINARY_DIR=`cd ../../BUILD/Debug/contrib/communicatord && pwd`
COMMUNICATORD=${BINARY_DIR}/daemon/communicatord
if test ! -x "${COMMUNICATORD}"
then
	echo "error: could not find the communicatord binary."
	exit 1
fi

if test "$1" = "-d"
then
	GDB="gdb --args"
else
	GDB=""
fi

TMP_DIR=${BINARY_DIR}/tmp
mkdir -p ${TMP_DIR}

if test ! -f ${TMP_DIR}/priv.key \
     -o ! -f ${TMP_DIR}/cert.crt
then
	openssl req -newkey rsa:2048 \
		-nodes -keyout ${TMP_DIR}/priv.key \
		-x509 -days 3650 -out ${TMP_DIR}/cert.crt \
		-subj "/C=US/ST=California/L=Sacramento/O=Snap/OU=Website/CN=snap.website"
fi

${GDB} ${COMMUNICATORD} \
	--my-address 192.168.2.1:4042 \
	--unix-listen ${TMP_DIR}/communicatord.sock \
	--remote-listen 192.168.2.1:4042 \
	--secure-listen admin:password1@192.168.3.1:4043 \
	--certificate ${TMP_DIR}/cert.crt \
	--private-key ${TMP_DIR}/priv.key \
	--services ${BINARY_DIR}/../../dist/share/communicatord/services

