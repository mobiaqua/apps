#!/bin/sh

[ -z "${OE_BASE}" ] && echo "Script require OE_BASE and DISTRO configured by OE environment." && exit 1

curdir=$PWD

echo
echo "Preparing mplayer..."
echo

if [ ! -e mplayer/Makefile ]; then
	cd ${OE_BASE}/build-${DISTRO} && \
		source env.source && \
		${OE_BASE}/bb/bin/bitbake mplayer -cclean && \
		${OE_BASE}/bb/bin/bitbake mplayer -cconfigure && {
			cd ${curdir}
			echo
			echo "Copying mplayer sources..."
			echo
			cp -R ${OE_BASE}/build-${DISTRO}/tmp/work/*-linux-gnueabi/mplayer-*/trunk/ mplayer/
			cp build.sh clean.sh _env.sh mplayer/
			chmod +x mplayer/*.sh
			echo ;echo "--- Setup done ---"; echo
		}
else
	echo
	echo "Nothing to do..."
	echo
fi
