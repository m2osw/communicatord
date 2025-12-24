#!/bin/sh -e
#
# Start the communicatord locally for a developer

# Make sure that the Debug binary exists
BINARY_DIR=`cd ../../BUILD/Debug/contrib/communicator && pwd`
COMMUNICATORD=${BINARY_DIR}/daemon/communicatord
if test ! -x "${COMMUNICATORD}"
then
	echo "error: could not find the communicatord binary, was it compiled?"
	exit 1
fi
TMP_DIR=${BINARY_DIR}/tmp

MY_ADDRESS="192.168.2.1"
REMOTE_LISTEN="192.168.2.1"
SECURE_LISTEN="192.168.3.1"
GDB=""
while test -n "$1"
do
	case "$1" in
	"--debug"|"-d")
		shift
		GDB='gdb -ex "catch throw" -ex run --args'
		;;

	"--help"|"-h")
		echo "Usage: $0 [-opts]"
		echo "where -opts are:"
		echo "   --my-address | -a          define this computer's address"
		echo "   --secure-listen | -s       define the secure-listen IP address (public network)"
		echo "   --remote-listen | -r       define the remote-listen IP address (private network)"
		echo
		echo "the communicator daemon will always have a Unix socket created in:"
		echo "     ${TMP_DIR}"
		exit 1
		;;

	"--my-address"|"-a")
		shift
		MY_ADDRESS="${1}"
		shift
		;;

	"--remote-listen"|"-r")
		shift
		REMOTE_LISTEN="${1}"
		shift
		;;

	"--secure-listen"|"-s")
		shift
		SECURE_LISTEN="${1}"
		shift
		;;

	*)
		echo "error: unrecognized option $1"
		exit 1
		;;

	esac
done

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
	--my-address ${MY_ADDRESS}:4042 \
	--unix-listen ${TMP_DIR}/communicatord.sock \
	--remote-listen ${REMOTE_LISTEN}:4042 \
	--secure-listen admin:password1@${SECURE_LISTEN}:4043 \
	--certificate ${TMP_DIR}/cert.crt \
	--private-key ${TMP_DIR}/priv.key \
	--services ${BINARY_DIR}/../../dist/share/communicator/services

